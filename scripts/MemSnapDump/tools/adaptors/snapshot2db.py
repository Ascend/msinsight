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
import tempfile
from pathlib import Path
from simulate import SimulateHooker, SimulateDeviceSnapshot, AllocatorHooker
from base import DeviceSnapshot, TraceEntry, Block, BlockState
from tools.adaptors.database import SnapshotDb, block2record, event2record
from util.file_util import check_dir_valid, load_pickle_to_dict
from util.logger import get_logger, set_global_log_file
from util.timer import timer

dump_logger = get_logger("DatabaseDump")


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
    def __init__(self, db_path: str, devices: list[int], dump_cache_size: int = 1000):
        self.db_handler = SnapshotDbHandler(db_path, devices, insert_cache_size=dump_cache_size)

    def post_undo_event(self, already_undo_event: TraceEntry, current_snapshot: DeviceSnapshot) -> bool:
        # 回放完毕，dump剩余Segment及block数据, 注意应该先插入blocks
        if not current_snapshot.trace_entries:
            for seg in current_snapshot.segments:
                for block in seg.blocks:
                    if block.state != BlockState.INACTIVE:
                        self.db_handler.insert_block(block2record(block), current_snapshot.device)
                # segment不插入block表，而是以模拟事件插入事件表，便于后续重建segment
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
                    current_snapshot.device,
                )
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
        self.db_handler.insert_block(block2record(released_block), current_snapshot.device)

    def flush(self, device: int = 0):
        self.db_handler.flush(device)

    def close(self, *, commit: bool = True):
        self.db_handler.close(commit=commit)


def dump(pickle_file: str, dump_file: str, device=None) -> bool:
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
    dump_path = Path(dump_file)
    dump_path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(prefix=f".{dump_path.name}.", suffix=".tmp", dir=dump_path.parent)
    os.close(fd)
    temporary_path = Path(temporary_name)
    hooker = None
    try:
        temporary_path.unlink()
        hooker = DumpEventHooker(str(temporary_path), need_dump_devices)
        for dev in need_dump_devices:
            dump_logger.info("Start to dump the snapshot to database for device %s.", dev)
            snapshot = SimulateDeviceSnapshot(data, dev, _raw_frames=True)
            snapshot.register_hooker(hooker)
            snapshot.register_allocator_hooker(hooker)
            if not snapshot.replay():
                dump_logger.error("Failed to dump the snapshot to database for device %s.", dev)
                return False
            dump_logger.info("Finished dump the snapshot to database for device %s.", dev)
            hooker.flush(dev)
        dump_logger.info("Successfully dump the snapshot to database for devices %s.", need_dump_devices)
        hooker.close()
        os.replace(temporary_path, dump_path)
        return True
    finally:
        try:
            if hooker is not None:
                hooker.close(commit=False)
        finally:
            temporary_path.unlink(missing_ok=True)


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
        help="Specify the directory to store the parsed database file. If not provided, "
        "the same directory as the specified snapshot file will be used by default.",
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
    args = parser.parse_args()
    snapshot_path = Path(args.snapshot_file)
    # 校验snapshot path
    if not snapshot_path.is_file() or not os.access(args.snapshot_file, os.R_OK):
        raise argparse.ArgumentError(
            arg_snapshot, "The specified snapshot file does not exist, or is not a file, or is not readable."
        )
    # 校验dump目标路径
    if not args.dump_dir:
        args.dump_dir = snapshot_path.parent
    if not Path(args.dump_dir).is_dir() or not check_dir_valid(args.dump_dir):
        raise argparse.ArgumentError(
            arg_dump_dir, "The specified directory does not exist, or is not a directory, or is not writable"
        )
    if args.log:
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
    if not dump(args.snapshot_file, Path(args.dump_dir) / f"{Path(args.snapshot_file).name}.db", args.device):
        dump_logger.error("Failed to dump the snapshot to database.")
        sys.exit(ExistCode.FAILED)
    sys.exit(ExistCode.SUCCESS)


if __name__ == '__main__':
    main()
