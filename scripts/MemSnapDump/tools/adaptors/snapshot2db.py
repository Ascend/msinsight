"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

         http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

import os
import sys
import argparse
import json
import math
import shutil
import sqlite3
import time
from pathlib import Path
from simulate import SimulateHooker, SimulateDeviceSnapshot, AllocatorHooker
from base import DeviceSnapshot, TraceEntry, Block, BlockState
from tools.adaptors.database import SnapshotDb, block2record, event2record
from util.file_util import check_dir_valid, load_pickle_to_dict
from util.logger import get_logger, set_global_log_file
from util.timer import timer

dump_logger = get_logger("DatabaseDump")
DEFAULT_EVENTS_PER_SLICE = 500_000
MANIFEST_SCHEMA_VERSION = 1
ARTIFACT_DIR_SUFFIX = ".msinsight"
MANIFEST_FILE_NAME = "manifest.json"
_SQLITE_LOCK_RETRIES = 5
_SQLITE_LOCK_RETRY_SECONDS = 0.2


def _is_sqlite_lock_error(error: BaseException) -> bool:
    message = str(error).lower()
    return isinstance(error, sqlite3.OperationalError) and (
        "database is locked" in message or "database schema is locked" in message
    )


def _quoted_block_table_name(device: int) -> str:
    device_id = int(device)
    if device_id < 0:
        raise ValueError(f"Invalid device id for block table update: {device}")
    table_name = SnapshotDb.get_block_table_name_by_device(device_id)
    if table_name != f"{SnapshotDb.BLOCK_TABLE_NAME}_{device_id}":
        raise ValueError(f"Unexpected block table name: {table_name}")
    return '"' + table_name + '"'


def _backfill_block_sql(device: int) -> str:
    return "".join(("UPDATE ", _quoted_block_table_name(device), ' SET "id" = ?, "allocEventId" = ? WHERE "id" = ?'))


def _compact_block_id_sql(device: int) -> str:
    return "".join(("UPDATE ", _quoted_block_table_name(device), ' SET "id" = ? WHERE "id" = ?'))


def _apply_block_table_updates(db_path: str, sql: str, updates) -> None:
    last_error = None
    for attempt in range(_SQLITE_LOCK_RETRIES):
        conn = None
        try:
            conn = sqlite3.connect(db_path, timeout=30)
            conn.execute("PRAGMA busy_timeout = 30000")
            try:
                conn.execute("PRAGMA journal_mode = WAL")
            except sqlite3.OperationalError:
                pass
            conn.executemany(sql, updates)
            conn.commit()
            return
        except sqlite3.OperationalError as error:
            last_error = error
            if not _is_sqlite_lock_error(error) or attempt == _SQLITE_LOCK_RETRIES - 1:
                raise
            time.sleep(_SQLITE_LOCK_RETRY_SECONDS * (attempt + 1))
        finally:
            if conn is not None:
                conn.close()
    if last_error is not None:
        raise last_error


class SnapshotDbHandler:
    def __init__(self, db_path: str, devices: list[int], insert_cache_size: int = 1000):
        self._closed = False
        self.db_path = db_path
        self.db = None
        self._device_event_cache = dict()
        self._device_block_cache = dict()
        self._insert_cache_size = insert_cache_size
        try:
            self.db = SnapshotDb(db_path)
            for device in devices:
                self._device_block_cache[device] = []
                self._device_event_cache[device] = []
                self.db.create_trace_entry_table(device)
                self.db.create_block_table(device)
        except Exception:
            self.close(commit=False)
            raise

    def insert_event(self, event_record: dict, device: int = 0):
        if device not in self._device_event_cache:
            self._device_event_cache[device] = []
        self._device_event_cache[device].append(event_record)
        if len(self._device_event_cache[device]) >= self._insert_cache_size:
            self._do_insert_events(device)

    def insert_block(self, block_record: dict, device: int = 0):
        if device not in self._device_block_cache:
            self._device_block_cache[device] = []
        self._device_block_cache[device].append(block_record)
        if len(self._device_block_cache[device]) >= self._insert_cache_size:
            self._do_insert_blocks(device)

    def flush(self, device: int = 0):
        if self._device_event_cache.get(device, None):
            self._do_insert_events(device)
        if self._device_block_cache.get(device, None):
            self._do_insert_blocks(device)

    def _do_insert_events(self, device: int = 0):
        if device not in self._device_event_cache:
            self._device_event_cache[device] = []
            return
        self.db.get_trace_entry_table(device).insert_records(self.db.conn, self._device_event_cache[device])
        self.db.conn.commit()
        self._device_event_cache[device].clear()

    def _do_insert_blocks(self, device: int = 0):
        if device not in self._device_block_cache:
            self._device_block_cache[device] = []
        self.db.get_block_table(device).insert_records(self.db.conn, self._device_block_cache[device])
        self.db.conn.commit()
        self._device_block_cache[device].clear()

    def close(self, *, commit: bool = True):
        if getattr(self, "_closed", True):
            return
        db = getattr(self, "db", None)
        if db is not None:
            if commit:
                db.conn.commit()
            else:
                db.conn.rollback()
            db.conn.close()
        self._closed = True

    def __del__(self):
        self.close()


class DumpEventHooker(SimulateHooker, AllocatorHooker):
    def __init__(
        self,
        db_path: str,
        devices: list[int],
        truncated_blocks,
        dump_cache_size: int = 1000,
    ):
        self.db_path = db_path
        self.truncated_blocks = truncated_blocks
        self.db_handler = SnapshotDbHandler(db_path, devices, insert_cache_size=dump_cache_size)

    def post_undo_event(self, already_undo_event: TraceEntry, current_snapshot: DeviceSnapshot) -> bool:
        return True

    def pre_undo_event(self, wait4undo_event: TraceEntry, current_snapshot: DeviceSnapshot) -> bool:
        # 每个事件回放前dump一次event
        self.db_handler.insert_event(
            event2record(
                event=wait4undo_event,
                allocated=current_snapshot.total_allocated,
                active=current_snapshot.total_activated,
                reserved=current_snapshot.total_reserved,
            ),
            current_snapshot.device,
        )
        return True

    def post_replay_free_block(self, released_block: Block, current_snapshot: DeviceSnapshot):
        self.truncated_blocks.resolve(released_block)
        self.db_handler.insert_block(block2record(released_block), current_snapshot.device)

    def dump_left_boundary(self, current_snapshot: DeviceSnapshot):
        """写入分片左边界状态；活跃块继续由模拟器持有，后续遇到申请事件时回填。"""
        device = current_snapshot.device
        for seg in current_snapshot.segments:
            for block in seg.blocks:
                if block.state == BlockState.INACTIVE:
                    continue
                block_id = self.truncated_blocks.register(block, self.db_path, device)
                self.db_handler.insert_block(
                    block2record(block, block_id=block_id, alloc_event_id=-1),
                    device,
                )
            mock_segment_alloc_event = TraceEntry(
                idx=None,
                action='segment_map' if seg.is_expandable else 'segment_alloc',
                addr=seg.address,
                frames=seg.frames,
                _raw_frames=seg._raw_frames,
                size=seg.total_size,
                stream=seg.stream,
            )
            self.db_handler.insert_event(
                event2record(
                    event=mock_segment_alloc_event,
                    allocated=current_snapshot.total_allocated,
                    active=current_snapshot.total_activated,
                    reserved=current_snapshot.total_reserved,
                ),
                device,
            )

    def flush(self, device: int = 0):
        self.db_handler.flush(device)

    def close(self, *, commit: bool = True):
        self.db_handler.close(commit=commit)


class TruncatedBlockRegistry:
    """记录跨分片块的占位记录，并在倒序回放遇到真实申请事件后回填。"""

    def __init__(self):
        self._next_tracking_id = 0
        self._next_block_id = -1
        self._block_ids = {}
        self._pending = {}
        self._backfills = {}

    def register(self, block: Block, db_path: str, device: int) -> int:
        tracking_id = getattr(block, "_slice_tracking_id", None)
        if tracking_id is None:
            tracking_id = self._next_tracking_id
            self._next_tracking_id += 1
            block._slice_tracking_id = tracking_id
        block_id = self._block_ids.get(tracking_id)
        if block_id is None:
            block_id = self._next_block_id
            self._next_block_id -= 1
            self._block_ids[tracking_id] = block_id
        self._pending.setdefault(tracking_id, []).append((db_path, device, block_id))
        return block_id

    def published_block_id(self, block: Block):
        tracking_id = getattr(block, "_slice_tracking_id", None)
        if tracking_id is None:
            return None
        return self._block_ids.get(tracking_id)

    def resolve(self, block: Block):
        tracking_id = getattr(block, "_slice_tracking_id", None)
        if tracking_id is None or block.alloc_event_idx is None:
            return
        pending_records = self._pending.pop(tracking_id, [])
        self._block_ids[tracking_id] = block.alloc_event_idx
        for db_path, device, block_id in pending_records:
            self._backfills.setdefault((db_path, device), []).append(
                (block.alloc_event_idx, block.alloc_event_idx, block_id)
            )

    def flush_backfills(self):
        for (db_path, device), updates in self._backfills.items():
            _apply_block_table_updates(db_path, _backfill_block_sql(device), updates)
        self._backfills.clear()

    def compact_unresolved_block_ids(self):
        """将最终无法回填申请事件的块编号为连续负数。分两步改主键，避免目标号仍被占用。"""
        if not self._pending:
            return
        temp_id = self._next_block_id
        temp_ids = {}
        phase1 = {}
        for tracking_id, pending_records in self._pending.items():
            old_block_id = self._block_ids[tracking_id]
            temp_id -= 1
            temp_ids[tracking_id] = temp_id
            if old_block_id != temp_id:
                for db_path, device, _ in pending_records:
                    phase1.setdefault((db_path, device), []).append((temp_id, old_block_id))
        for (db_path, device), updates in phase1.items():
            _apply_block_table_updates(db_path, _compact_block_id_sql(device), updates)
        self._next_block_id = temp_id

        phase2 = {}
        compacted_pending = {}
        for index, (tracking_id, pending_records) in enumerate(self._pending.items(), start=1):
            new_block_id = -index
            old_block_id = temp_ids[tracking_id]
            compacted_records = []
            for db_path, device, _ in pending_records:
                compacted_records.append((db_path, device, new_block_id))
                if old_block_id != new_block_id:
                    phase2.setdefault((db_path, device), []).append((new_block_id, old_block_id))
            self._block_ids[tracking_id] = new_block_id
            compacted_pending[tracking_id] = compacted_records
        for (db_path, device), updates in phase2.items():
            _apply_block_table_updates(db_path, _compact_block_id_sql(device), updates)
        self._pending = compacted_pending


class SnapshotSliceManifest:
    def __init__(
        self,
        output_dir: Path,
        pickle_file: str,
        cache_hash: str,
        device_traces: list,
        devices: list[int],
        events_per_slice: int,
    ):
        self.output_dir = output_dir
        self.path = output_dir / MANIFEST_FILE_NAME
        self.data = {
            "schemaVersion": MANIFEST_SCHEMA_VERSION,
            "status": "building",
            "sourceFile": str(Path(pickle_file).resolve()),
            "cacheHash": cache_hash,
            "eventsPerSlice": events_per_slice,
            "devices": {},
        }
        for device in devices:
            event_count = len(device_traces[device])
            slice_count = math.ceil(event_count / events_per_slice)
            self.data["devices"][str(device)] = {
                "eventCount": event_count,
                "sliceCount": slice_count,
                "readySlices": [],
                "slices": [
                    {
                        "index": index,
                        "startEventId": index * events_per_slice,
                        "endEventId": min((index + 1) * events_per_slice, event_count) - 1,
                        "file": f"device_{device}/slice_{index:05d}.db",
                        "ready": False,
                    }
                    for index in range(slice_count)
                ],
            }
        self.save()

    def mark_ready(self, device: int, slice_index: int):
        device_data = self.data["devices"][str(device)]
        device_data["slices"][slice_index]["ready"] = True
        if slice_index not in device_data["readySlices"]:
            device_data["readySlices"].append(slice_index)
            device_data["readySlices"].sort()
        self.save()

    def mark_complete(self):
        self.data["status"] = "complete"
        self.save()

    def save(self):
        temporary_path = self.path.with_suffix(".json.tmp")
        with temporary_path.open("w", encoding="utf-8") as file:
            json.dump(self.data, file, ensure_ascii=False, indent=2)
            file.flush()
            os.fsync(file.fileno())
        for retry in range(10):
            try:
                os.replace(temporary_path, self.path)
                return
            except PermissionError:
                if retry == 9:
                    raise
                # Windows 下服务端可能正短暂读取 manifest，等待后重试原子替换。
                time.sleep(0.02)


def _prepare_output_directory(output_dir: Path):
    output_dir.mkdir(parents=True, exist_ok=True)
    for stale_device_dir in output_dir.glob("device_*"):
        if stale_device_dir.is_dir():
            shutil.rmtree(stale_device_dir)
    (output_dir / MANIFEST_FILE_NAME).unlink(missing_ok=True)
    (output_dir / f"{MANIFEST_FILE_NAME}.tmp").unlink(missing_ok=True)


def _create_slice_hooker(
    output_dir: Path,
    device: int,
    slice_index: int,
    truncated_blocks: TruncatedBlockRegistry,
):
    device_dir = output_dir / f"device_{device}"
    device_dir.mkdir(parents=True, exist_ok=True)
    final_path = device_dir / f"slice_{slice_index:05d}.db"
    final_path.unlink(missing_ok=True)
    hooker = DumpEventHooker(str(final_path), [device], truncated_blocks)
    # SQLite writes directly to the final path so later slices can backfill it while parsing continues.
    return hooker, final_path


def dump(
    pickle_file: str,
    output_dir: str | Path,
    device=None,
    events_per_slice: int = DEFAULT_EVENTS_PER_SLICE,
    cache_hash: str = "",
) -> bool:
    try:
        data = load_pickle_to_dict(Path(pickle_file))
    except Exception as e:
        dump_logger.error("Failed to load pickle file: %s", e)
        return False
    device_traces = data.get("device_traces", [])
    # 当指定device为空时dump所有记录了跟踪事件的device，否则仅dump指定device
    need_dump_devices = [dev for dev in range(len(device_traces)) if device_traces[dev]]
    dump_logger.info("Recognized have trace events devices %s.", need_dump_devices)
    if device is not None and device not in need_dump_devices:
        dump_logger.error("Specified device %s is not found or has no trace events in the snapshot.", device)
        return False
    if device is not None:
        need_dump_devices = [device]
    dump_logger.info("Recognized need to dump devices %s.", need_dump_devices)
    if not need_dump_devices:
        dump_logger.error("No device with trace events was found in the snapshot.")
        return False
    if events_per_slice <= 0:
        dump_logger.error("Events per slice must be greater than 0.")
        return False
    artifact_dir = Path(output_dir)
    _prepare_output_directory(artifact_dir)
    manifest = SnapshotSliceManifest(
        artifact_dir,
        pickle_file,
        cache_hash,
        device_traces,
        need_dump_devices,
        events_per_slice,
    )
    total_events = sum(len(device_traces[dev]) for dev in need_dump_devices)
    processed_events = 0
    last_progress = 0

    def report_progress():
        nonlocal processed_events, last_progress
        processed_events += 1
        progress = 100 if total_events == 0 else processed_events * 100 // total_events
        while last_progress < progress:
            last_progress += 1
            dump_logger.info(
                "%s%% of entries have been processed, %s entries remain.",
                last_progress,
                total_events - processed_events,
            )

    try:
        for dev in need_dump_devices:
            dump_logger.info("Start to dump the snapshot to database for device %s.", dev)
            snapshot = SimulateDeviceSnapshot(data, dev, _raw_frames=True)
            truncated_blocks = TruncatedBlockRegistry()
            event_count = len(snapshot.device_snapshot.trace_entries)
            slice_count = math.ceil(event_count / events_per_slice)
            for slice_index in range(slice_count - 1, -1, -1):
                slice_start = slice_index * events_per_slice
                hooker, final_path = _create_slice_hooker(artifact_dir, dev, slice_index, truncated_blocks)
                hooker_id = snapshot.register_hooker(hooker)
                allocator_hooker_id = snapshot.register_allocator_hooker(hooker)
                try:
                    if not snapshot.replay_until(slice_start, report_progress):
                        dump_logger.error(
                            "Failed to dump slice %s for device %s.",
                            slice_index,
                            dev,
                        )
                        return False
                    hooker.dump_left_boundary(snapshot.device_snapshot)
                    hooker.flush(dev)
                    hooker.close()
                finally:
                    snapshot.unregister_hooker(hooker_id)
                    snapshot.unregister_allocator_hooker(allocator_hooker_id)
                    hooker.close(commit=False)
                manifest.mark_ready(dev, slice_index)
                dump_logger.info(
                    "Snapshot slice ready: device=%s, slice=%s, path=%s",
                    dev,
                    slice_index,
                    final_path,
                )
            # 收尾：已解析的跨窗块主键写回申请事件号；未解析占位块再收成连续负 ID。
            try:
                truncated_blocks.flush_backfills()
                truncated_blocks.compact_unresolved_block_ids()
            except sqlite3.Error as error:
                dump_logger.exception(
                    "Finalization backfill failed for device %s after retries: %s.",
                    dev,
                    error,
                )
                return False
            dump_logger.info("Finished dump the snapshot to database for device %s.", dev)
        manifest.mark_complete()
        dump_logger.info("Successfully dump the snapshot to database for devices %s.", need_dump_devices)
        return True
    except Exception as error:
        dump_logger.exception("Failed to dump snapshot slices: %s", error)
        return False


def get_args():
    parser = argparse.ArgumentParser(
        description="This script is used to parse and convert snapshot data into a database "
        "format that is more convenient for visualization."
    )
    arg_snapshot = parser.add_argument("snapshot_file", type=str, help="Memory snapshot file path.")
    arg_dump_dir = parser.add_argument(
        "--dump_dir",
        "-o",
        required=False,
        type=str,
        default='',
        help="Specify the artifact directory used to store the manifest, slice databases, and logs. "
        "If not provided, '<snapshot file>.msinsight' is used.",
    )
    arg_log_file = parser.add_argument(
        "--log",
        "-l",
        required=False,
        type=str,
        default='',
        help="Specify the log file path. If provided, all logs will be written to this file.",
    )
    parser.add_argument(
        "--device",
        "-d",
        required=False,
        type=lambda x: int(x) if int(x) >= 0 else parser.error("The device id must be at least 0"),
        help="Specify the device id to dump. If not provided, we will dump the data of all devices.",
    )
    parser.add_argument(
        "--events_per_slice",
        type=lambda x: int(x) if int(x) > 0 else parser.error("Events per slice must be greater than 0"),
        default=DEFAULT_EVENTS_PER_SLICE,
        help=f"Maximum number of trace events stored in one database. Default: {DEFAULT_EVENTS_PER_SLICE}.",
    )
    parser.add_argument(
        "--cache_hash",
        type=str,
        default="",
        help="Salted source hash used by the server to validate parsed artifacts.",
    )
    args = parser.parse_args()
    snapshot_path = Path(args.snapshot_file)
    # 校验snapshot path
    if not snapshot_path.is_file() or not os.access(args.snapshot_file, os.R_OK):
        raise argparse.ArgumentError(
            arg_snapshot, "The specified snapshot file does not exist, or is not a file, or is not readable."
        )
    # 校验dump目标路径
    if not args.dump_dir:
        args.dump_dir = snapshot_path.parent / f"{snapshot_path.name}{ARTIFACT_DIR_SUFFIX}"
    dump_dir = Path(args.dump_dir)
    try:
        dump_dir.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        raise argparse.ArgumentError(arg_dump_dir, str(error)) from error
    if not check_dir_valid(dump_dir):
        raise argparse.ArgumentError(
            arg_dump_dir, "The specified directory does not exist, or is not a directory, or is not writable"
        )
    args.dump_dir = dump_dir
    if not args.log:
        args.log = str(dump_dir / "parse.log")
    try:
        set_global_log_file(args.log)
    except OSError as e:
        raise argparse.ArgumentError(arg_log_file, str(e))
    return args


class ExistCode:
    SUCCESS = 0
    FAILED = -1


@timer(name="Dump snapshot to database.", logger=dump_logger)
def main():
    try:
        args = get_args()
    except argparse.ArgumentError as e:
        dump_logger.error("Failed to parse arguments: %s", e)
        sys.exit(ExistCode.FAILED)
    if not dump(
        args.snapshot_file,
        args.dump_dir,
        args.device,
        args.events_per_slice,
        args.cache_hash,
    ):
        dump_logger.error("Failed to dump the snapshot to database.")
        sys.exit(ExistCode.FAILED)
    sys.exit(ExistCode.SUCCESS)


if __name__ == '__main__':
    main()
