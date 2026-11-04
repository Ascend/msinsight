# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
# Licensed under Mulan PSL v2: http://license.coscl.org.cn/MulanPSL2
"""Snapshot frame-tree query entry point, independent of the GUI transport.

query_frame_tree is the replacement boundary for the future pt-snap-cli backend.
It never unpickles the source, mutates parser tables, or persists query results.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import sqlite3
import time
from contextlib import contextmanager
from pathlib import Path

MAX_NODES = 200000
MAX_FRAME_BYTES = 1024 * 1024
MAX_WORK_ROWS = 5000000
MAX_WORK_BYTES = 256 * 1024 * 1024
MAX_UINT64 = 2**64 - 1
MAX_FRAME_JSON_DEPTH = 128
FRAME_SCHEMA = {
    "pt_snap_frame_value": [("id", "INTEGER", 1), ("frameJson", "TEXT", 0)],
    "pt_snap_stack": [("stackId", "INTEGER", 1), ("frameCount", "INTEGER", 0)],
    "pt_snap_stack_frame": [("stackId", "INTEGER", 1), ("frameIndex", "INTEGER", 2), ("frameId", "INTEGER", 0)],
    "pt_snap_event_stack": [("eventId", "INTEGER", 1), ("stackId", "INTEGER", 0)],
}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Ambiguous JSON object")
        result[key] = value
    return result


class QueryCancelledError(ValueError):
    """The caller superseded this query; no partial tree may be published."""


class Budget:
    def __init__(self, timeout=60, cancelled=None):
        self.deadline = time.monotonic() + timeout
        self.cancelled = cancelled
        self.rows = 0
        self.bytes = 0

    def check_cancelled(self):
        if self.cancelled is not None and self.cancelled():
            raise QueryCancelledError("Frame tree query cancelled by a newer request or reset")

    def check(self, size=0):
        self.check_cancelled()
        self.rows += 1
        self.bytes += size
        if self.rows > MAX_WORK_ROWS or self.bytes > MAX_WORK_BYTES or time.monotonic() >= self.deadline:
            raise ValueError("Frame tree analysis exceeded its time or work limit")


def is_integer(value):
    # bool 是 int 的子类，但不能作为事件号、计数或版本号。
    return isinstance(value, int) and not isinstance(value, bool)


def integer(value, minimum=0, maximum=2**63 - 1):
    if not is_integer(value) or not minimum <= value <= maximum:
        raise ValueError("Invalid integer in frame tree source")
    return value


def fingerprint(path, required=True):
    try:
        stat = path.stat()
    except FileNotFoundError:
        if required:
            raise ValueError("Frame tree dataset member is missing") from None
        return None
    if not required and stat.st_size == 0:
        return None
    return stat.st_size, stat.st_mtime_ns, stat.st_ino


class Sources:
    def __init__(self, manifest_path, device_id, event_id, budget):
        self.manifest_path = Path(manifest_path).resolve()
        self.initial_manifest = fingerprint(self.manifest_path)
        if self.manifest_path.stat().st_size > 16 * 1024 * 1024:
            raise ValueError("Frame tree manifest is too large")
        self.manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
        if (
            not isinstance(self.manifest, dict)
            or not is_integer(self.manifest.get("schemaVersion"))
            or self.manifest["schemaVersion"] != 1
            or self.manifest.get("status") != "complete"
        ):
            raise ValueError("Frame tree requires a completely parsed snapshot dataset")
        self.device = device_id
        self.budget = budget
        self.paths = {}
        self.all_paths = []
        devices = self.manifest["devices"]
        if not isinstance(devices, dict) or device_id not in devices:
            raise ValueError("Frame tree device does not exist")
        seen = set()
        for device, data in devices.items():
            if re.fullmatch(r"0|[1-9][0-9]*", device) is None:
                raise ValueError("Invalid frame tree device")
            if not isinstance(data, dict):
                raise TypeError("Invalid frame tree device inventory")
            count = integer(data["eventCount"], 1)
            slices = data["slices"]
            if not isinstance(slices, list) or not isinstance(data["readySlices"], list):
                raise TypeError("Invalid frame tree slice inventory")
            if integer(data["sliceCount"], 1) != len(slices):
                raise ValueError("Incomplete frame tree slice inventory")
            if sorted(integer(i) for i in data["readySlices"]) != list(range(len(slices))):
                raise ValueError("Frame tree dataset member is not ready")
            previous = -1
            for index, item in enumerate(slices):
                budget.check()
                if (
                    integer(item["index"]) != index
                    or integer(item["startEventId"]) != previous + 1
                    or item["ready"] is not True
                ):
                    raise ValueError("Invalid frame tree slice range")
                previous = integer(item["endEventId"], item["startEventId"])
                relative = item["file"]
                if (
                    not isinstance(relative, str)
                    or not relative
                    or "\\" in relative
                    or Path(relative).is_absolute()
                    or any(p in ("", ".", "..") for p in relative.split("/"))
                ):
                    raise ValueError("Unsafe frame tree member path")
                path = self.manifest_path.parent / relative
                if path.resolve() != path or not path.is_file() or path in seen:
                    raise ValueError("Missing, duplicate or aliased frame tree member")
                for suffix in ("-wal", "-shm", "-journal"):
                    sidecar = Path(str(path) + suffix)
                    if sidecar.is_symlink() or (suffix == "-journal" and sidecar.exists()):
                        raise ValueError("Unsafe frame tree sidecar")
                seen.add(path)
                self.all_paths.append(path)
                if device == device_id:
                    self.paths[index] = path
            if previous != count - 1:
                raise ValueError("Incomplete frame tree event range")
        self.slices = devices[device_id]["slices"]
        if event_id >= devices[device_id]["eventCount"]:
            raise ValueError("Frame tree event does not exist")
        extensions = self.manifest.get("extensions", {})
        extension = extensions.get("ptSnapOrderedFrames", {}) if isinstance(extensions, dict) else {}
        self.ordered = (
            isinstance(extension, dict) and is_integer(extension.get("version")) and extension["version"] == 2
        )
        self.generation = self.signature()

    def signature(self):
        self.budget.check()
        if fingerprint(self.manifest_path) != self.initial_manifest:
            raise ValueError("Frame tree manifest changed; restart loading")
        return tuple(
            (fingerprint(p), fingerprint(Path(str(p) + "-wal"), False), fingerprint(Path(str(p) + "-journal"), False))
            for p in self.all_paths
        )

    @contextmanager
    def open(self, path):
        self.budget.check()
        conn = sqlite3.connect(path.as_uri() + "?mode=ro", uri=True, timeout=1)
        try:

            def interrupted():
                try:
                    self.budget.check_cancelled()
                except QueryCancelledError:
                    return 1
                return int(time.monotonic() >= self.budget.deadline)

            conn.set_progress_handler(interrupted, 1000)
            conn.execute("PRAGMA query_only=ON")
            conn.execute("BEGIN")
            yield conn
        except sqlite3.OperationalError:
            # SQLite suppresses callback exceptions; restore intentional cancellation.
            self.budget.check_cancelled()
            raise
        finally:
            conn.close()


def has_frame_schema(conn):
    for name, expected in FRAME_SCHEMA.items():
        if conn.execute("SELECT type FROM sqlite_master WHERE name=?", (name,)).fetchone() != ("table",):
            return False
        # Names are constants, never request values.
        rows = conn.execute(f"PRAGMA table_info({name})").fetchall()
        if [(row[1], row[2], row[5]) for row in rows] != expected:
            return False
    return True


def decode_frame_json(raw):
    """Keep optional raw evidence within the producer/reader JSON contract."""

    def invalid_number(value):
        raise ValueError("Non-finite frame number")

    def finite_number(value):
        number = float(value)
        if not math.isfinite(number):
            return invalid_number(value)
        return number

    frame = json.loads(raw, object_pairs_hook=unique_object, parse_constant=invalid_number, parse_float=finite_number)
    # The containing frame array has depth zero, matching the ordered reader.
    pending = [(frame, 1)]
    while pending:
        value, depth = pending.pop()
        if depth > MAX_FRAME_JSON_DEPTH:
            raise ValueError("Frame JSON nesting exceeds the evidence limit")
        if isinstance(value, dict):
            pending.extend((child, depth + 1) for child in value.values())
        elif isinstance(value, list):
            pending.extend((child, depth + 1) for child in value)
    return frame


def read_stack(conn, stack_id, count, budget):
    if not is_integer(count) or not 0 <= count <= MAX_NODES:
        return None
    result = []
    for index, raw in conn.execute(
        "SELECT sf.frameIndex,f.frameJson FROM pt_snap_stack_frame sf "
        "LEFT JOIN pt_snap_frame_value f ON f.id=sf.frameId WHERE sf.stackId=? ORDER BY sf.frameIndex",
        (stack_id,),
    ):
        if len(result) >= count or not is_integer(index) or index != len(result) or not isinstance(raw, str):
            return None
        size = len(raw.encode("utf-8"))
        if size > MAX_FRAME_BYTES:
            raise ValueError("Oversized frame tree source text")
        budget.check(size)
        try:
            frame = decode_frame_json(raw)
            if (
                not isinstance(frame, dict)
                or not isinstance(frame.get("filename"), str)
                or not isinstance(frame.get("name"), str)
            ):
                return None
            integer(frame.get("line"), -(2**63))
            result.append((frame["filename"], frame["line"], frame["name"]))
        except (ValueError, RecursionError):
            return None
    return result if len(result) == count else None


def read_groups(sources, event_id, include_static):
    owner = next(s for s in sources.slices if s["startEventId"] <= event_id <= s["endEventId"])
    groups = {}
    with sources.open(sources.paths[owner["index"]]) as conn:
        # device 已严格限制为十进制数字；事件号始终绑定参数，表名不能使用 SQLite 参数绑定。
        if conn.execute(f"SELECT 1 FROM trace_entry_{sources.device} WHERE id=?", (event_id,)).fetchone() is None:  # nosec B608
            raise ValueError("Frame tree event does not exist")
        sql = (
            f"SELECT allocEventId,freeEventId,size,COALESCE(requestedSize,0) FROM block_{sources.device} "  # nosec B608
            "WHERE (allocEventId=-1 OR allocEventId<=?) AND (freeEventId IS NULL OR freeEventId<0 OR freeEventId>?) "
        )
        if not include_static:
            sql += "AND allocEventId!=-1"
        for allocation, free, size, requested in conn.execute(sql, (event_id, event_id)):
            sources.budget.check()
            integer(allocation, -1)
            integer(size)
            integer(requested)
            kind = "frame" if allocation != -1 else ("static" if is_integer(free) and free == -1 else "preexisting")
            key = allocation, kind
            if key not in groups:
                sources.budget.check(256)
                groups[key] = {"metrics": [0, 0, 0], "frames": None, "resolved": False}
            accumulate(groups[key]["metrics"], [size, requested, 1])
    return groups


def read_allocation_frames(sources, groups):
    # Sort once; do not rescan all allocations for every slice.
    allocations = sorted(key for key in groups if key[0] >= 0)
    position = 0
    for item in sources.slices:
        end = position
        while end < len(allocations) and allocations[end][0] <= item["endEventId"]:
            end += 1
        if end == position:
            continue
        with sources.open(sources.paths[item["index"]]) as conn:
            evidence = sources.ordered and has_frame_schema(conn)
            stacks = {}
            for key in allocations[position:end]:
                sources.budget.check()
                group = groups[key]
                group["resolved"] = conn.execute(
                    f"SELECT action FROM trace_entry_{sources.device} WHERE id=?",  # nosec B608
                    (key[0],),
                ).fetchone() == (4,)
                if not evidence or not group["resolved"]:
                    continue
                stack = conn.execute(
                    "SELECT e.stackId,s.frameCount FROM pt_snap_event_stack e "
                    "LEFT JOIN pt_snap_stack s ON s.stackId=e.stackId WHERE e.eventId=?",
                    (key[0],),
                ).fetchone()
                if stack is None or not is_integer(stack[0]):
                    continue
                if stack[0] not in stacks:
                    stacks[stack[0]] = read_stack(conn, *stack, sources.budget)
                group["frames"] = stacks[stack[0]]
        position = end


def accumulate(target, source):
    for i, value in enumerate(source):
        target[i] += value
        if target[i] > MAX_UINT64:
            raise ValueError("Frame tree metric overflow")


def build_tree(groups, budget):
    def node(node_id, parent, depth, kind, frame=None):
        return {
            "nodeId": node_id,
            "parentId": parent,
            "depth": depth,
            "kind": kind,
            "frame": frame,
            "total": [0, 0, 0],
            "self": [0, 0, 0],
            "childCount": 0,
        }

    nodes = [node("root", None, 0, "root")]
    children = {}
    coverage = [0, 0, 0]
    for (_, kind), group in sorted(groups.items()):
        budget.check()
        metrics, frames = group["metrics"], group["frames"]
        dynamic = kind == "frame"
        if dynamic:
            accumulate(
                coverage, [metrics[2], metrics[2] if group["resolved"] else 0, metrics[2] if frames is not None else 0]
            )
        parent = 0
        accumulate(nodes[0]["total"], metrics)
        if dynamic and frames:
            path = [("frame", frame) for frame in reversed(frames)]
        else:
            path = [(kind if not dynamic else ("emptyStack" if frames is not None else "missingFrame"), None)]
        for category, frame in path:
            budget.check()
            key = parent, category, frame
            if key not in children:
                if len(nodes) >= MAX_NODES:
                    raise ValueError("Frame tree has too many nodes")
                text_bytes = sum(len(x.encode("utf-8")) for x in (frame[0], frame[2])) if frame else 0
                budget.check(512 + 2 * text_bytes)
                index = len(nodes)
                node_id = f"frame/{index}" if frame else category
                nodes.append(
                    node(
                        node_id,
                        nodes[parent]["nodeId"],
                        nodes[parent]["depth"] + 1,
                        category,
                        dict(zip(("filename", "line", "name"), frame)) if frame else None,
                    )
                )
                nodes[parent]["childCount"] += 1
                children[key] = index
            parent = children[key]
            accumulate(nodes[parent]["total"], metrics)
        accumulate(nodes[parent]["self"], metrics)
    adjacency = [[] for _ in nodes]
    for (parent, _, _), child in children.items():
        adjacency[parent].append(child)
    for siblings in adjacency:
        siblings.sort(key=lambda i: (-nodes[i]["total"][0], nodes[i]["nodeId"]))
    pending, ordered = [0], []
    while pending:
        budget.check()
        index = pending.pop()
        ordered.append(nodes[index])
        pending.extend(reversed(adjacency[index]))
    return {"version": 1, "nodes": ordered, "coverage": coverage}


def query_frame_tree(manifest_path, device_id, event_id, include_static=True, *, timeout=60, cancelled=None):
    """Return the complete preorder tree; consumers paginate this result, not SQL rows."""
    if not isinstance(device_id, str) or len(device_id) > 20 or re.fullmatch(r"0|[1-9][0-9]*", device_id) is None:
        raise ValueError("Invalid frame tree device ID")
    integer(event_id, 0, 9007199254740991)
    if not isinstance(include_static, bool):
        raise TypeError("Invalid frame tree includeStatic")
    budget = Budget(timeout, cancelled)
    budget.check_cancelled()
    sources = Sources(manifest_path, device_id, event_id, budget)
    groups = read_groups(sources, event_id, include_static)
    read_allocation_frames(sources, groups)
    tree = build_tree(groups, budget)
    if sources.generation != sources.signature():
        raise ValueError("Frame tree dataset changed; restart loading")
    budget.check_cancelled()
    return tree


def cancellation_file(path):
    """Adapt a caller-owned marker to the API cancellation callback.

    Poll at most every 20 ms rather than once per row. Cancellation is sticky.
    """
    if path is None:
        return None
    marker = Path(path)
    next_check = 0.0
    stopped = False

    def cancelled():
        nonlocal next_check, stopped
        now = time.monotonic()
        if not stopped and now >= next_check:
            stopped = marker.exists()
            next_check = now + 0.02
        return stopped

    return cancelled


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--device", required=True)
    parser.add_argument("--event", required=True, type=int)
    parser.add_argument("--include-static", required=True, choices=("0", "1"))
    parser.add_argument("--output", required=True)
    parser.add_argument("--cancel-file", help="Creation of this caller-owned marker cancels the query")
    args = parser.parse_args(argv)
    cancelled = cancellation_file(args.cancel_file)
    budget = Budget(cancelled=cancelled)
    try:
        result = query_frame_tree(
            args.manifest, args.device, args.event, args.include_static == "1", cancelled=cancelled
        )
    except QueryCancelledError:
        return 2
    except (ValueError, KeyError, TypeError, StopIteration, OSError, sqlite3.Error, RecursionError) as error:
        result = {"version": 1, "error": str(error)[:2048]}
    # The caller owns a private temporary directory. Exclusive creation prevents
    # accidental reuse; a failed/partial result is never treated as a cache hit.
    created = False
    try:
        budget.check_cancelled()
        with Path(args.output).open("x", encoding="utf-8") as output:
            created = True
            size = 0
            for chunk in json.JSONEncoder(ensure_ascii=True, separators=(",", ":")).iterencode(result):
                budget.check_cancelled()
                size += len(chunk)
                if size > MAX_WORK_BYTES:
                    raise ValueError("Frame tree result exceeded transfer budget")
                output.write(chunk)
            budget.check_cancelled()
    except QueryCancelledError:
        if created:
            Path(args.output).unlink(missing_ok=True)
        return 2
    return 1 if "error" in result else 0


if __name__ == "__main__":
    raise SystemExit(main())
