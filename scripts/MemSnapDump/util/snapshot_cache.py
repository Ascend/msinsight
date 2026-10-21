"""Portable msinsight cache contract; consumer-owned tables are not parser data."""

from __future__ import annotations

import hashlib
import json
import os
import re
import sqlite3
import sys
from collections import OrderedDict
from contextlib import closing
from pathlib import Path
from typing import Any

from util.logger import get_logger

PARSER_HASH_SALT = b"mem_snapshot_parser_v2"
CACHE_CONTRACT_VERSION = 1
TRACE_COLUMNS = (
    "id",
    "action",
    "address",
    "size",
    "stream",
    "allocated",
    "active",
    "reserved",
    "callstack",
)
BLOCK_COLUMNS = ("id", "address", "size", "requestedSize", "state", "allocEventId", "freeEventId")
LOGGER = get_logger("SnapshotCache")
# 全局文件 handler 会挂到各级 logger，禁止向父级传播导致同一条日志重复写入。
LOGGER.propagate = False


class SnapshotCacheBusyError(RuntimeError):
    """An active SQLite writer must not be treated as an invalid cache."""


def _probe_writer(path: Path) -> None:
    # Only used before rebuilding or recovering an invalid rollback-journal cache.
    try:
        with closing(sqlite3.connect(path.resolve().as_uri() + "?mode=rw", uri=True, timeout=5)) as conn:
            conn.execute("BEGIN IMMEDIATE")
            conn.rollback()
    except sqlite3.OperationalError as error:
        if "locked" in str(error).lower() or "busy" in str(error).lower():
            raise SnapshotCacheBusyError("Snapshot member is being written by another process") from error
        raise


def ensure_cache_not_busy(directory: Path) -> None:
    """Preflight all existing members, even when the manifest itself is invalid."""
    for device_dir in directory.glob("device_*"):
        if re.fullmatch(r"device_[0-9]+", device_dir.name) is None:
            continue
        if device_dir.is_symlink():
            raise ValueError("Snapshot device directory must not be a symlink")
        for path in device_dir.glob("*.db"):
            if path.is_symlink() or any(
                Path(str(path) + suffix).is_symlink() for suffix in ("-wal", "-shm", "-journal")
            ):
                raise ValueError("Snapshot has an aliased member or sidecar")
            try:
                _probe_writer(path)
            except sqlite3.DatabaseError as error:
                # Corrupt closed databases are rebuildable; lock errors propagate separately.
                if "malformed" not in str(error).lower() and "not a database" not in str(error).lower():
                    raise


class _RowEncoder:
    """Bound repeated callstack encoding; preserve the v1 digest bytes."""

    def __init__(self) -> None:
        self.stacks: OrderedDict[str, bytes] = OrderedDict()
        self.size = 0

    def encode(self, row: tuple[Any, ...], inline_stack: bool) -> bytes:
        if not inline_stack or not isinstance(row[-1], str):
            return json.dumps(row, ensure_ascii=True, separators=(",", ":"), allow_nan=False).encode()
        stack = row[-1]
        encoded = self.stacks.get(stack)
        if encoded is None:
            encoded = json.dumps(stack, ensure_ascii=True, allow_nan=False).encode()
            cost = sys.getsizeof(stack) + sys.getsizeof(encoded)
            if cost <= 8 * 1024 * 1024:
                while self.stacks and (self.size + cost > 8 * 1024 * 1024 or len(self.stacks) >= 2048):
                    key, value = self.stacks.popitem(last=False)
                    self.size -= sys.getsizeof(key) + sys.getsizeof(value)
                self.stacks[stack] = encoded
                self.size += cost
        else:
            self.stacks.move_to_end(stack)
        prefix = json.dumps(row[:-1], separators=(",", ":"), allow_nan=False).encode()
        return prefix[:-1] + b"," + encoded + b"]"


def source_hashes(source: Path) -> tuple[str, str]:
    """Return raw and salted SHA-256 in one streaming pass, without unpickling."""
    raw = hashlib.sha256()
    salted = hashlib.sha256(PARSER_HASH_SALT)
    with source.open("rb") as file:
        before = os.fstat(file.fileno())
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            raw.update(chunk)
            salted.update(chunk)
        after = os.fstat(file.fileno())
    current = source.stat()

    def signature(stat: os.stat_result) -> tuple[int, int, int, int]:
        # Windows fstat/stat disagree on ctime for some files; it is not content identity.
        return stat.st_dev, stat.st_ino, stat.st_size, stat.st_mtime_ns

    if signature(before) != signature(after) or signature(after) != signature(current):
        raise ValueError("Snapshot source changed while hashing")
    return raw.hexdigest(), salted.hexdigest()


def _integer(value: Any, minimum: int = 0) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum or value > 2**63 - 1:
        raise ValueError("Invalid nonnegative integer in snapshot manifest")
    return value


def _member(directory: Path, relative: str) -> Path:
    if not isinstance(relative, str) or not relative or "\\" in relative:
        raise ValueError("Invalid snapshot member path")
    path = Path(relative)
    if path.is_absolute() or any(part in (".", "..") for part in relative.split("/")):
        raise ValueError("Snapshot member escapes artifact directory")
    candidate = directory / path
    if not candidate.is_file() or candidate.resolve() != directory.resolve() / path:
        raise ValueError("Missing or aliased snapshot member")
    return candidate


def _table_rows(
    conn: sqlite3.Connection,
    name: str,
    columns: tuple[str, ...],
    where: str = "",
    parameters: tuple[str, ...] = (),
) -> sqlite3.Cursor:
    if conn.execute("SELECT type FROM sqlite_master WHERE name=?", (name,)).fetchone() != ("table",):
        raise ValueError(f"Missing parser table: {name}")
    schema = conn.execute(f'PRAGMA table_info("{name}")').fetchall()
    actual = {row[1]: row for row in schema}
    for column in columns:
        kind = "TEXT" if column in ("callstack", "table", "column", "key", "value") else "INTEGER"
        if column not in actual or actual[column][2].upper() != kind:
            raise ValueError(f"Invalid parser column: {name}.{column}")
    if "id" in columns and [row[1] for row in schema if row[5]] != ["id"]:
        raise ValueError(f"Missing parser primary key: {name}.id")
    selected = ",".join(f'"{column}"' for column in columns)
    order = '"id"' if "id" in columns else selected
    # Identifiers and the optional predicate are internal constants/validated device IDs.
    return conn.execute(f'SELECT {selected} FROM "{name}" {where} ORDER BY {order}', parameters)  # nosec B608


def member_digest(path: Path, device: str, start: int, end: int) -> str:
    """Hash fixed base columns; extra columns, tables, indexes, views and triggers are allowed.

    Unrelated dictionary entries are also allowed. Legacy artifacts without a seal
    receive structure/reference checks, not proof that all historical values are unchanged.
    """
    if re.fullmatch(r"0|[1-9][0-9]*", str(device)) is None:
        raise ValueError("Invalid device ID")
    digest = hashlib.sha256()
    encoder = _RowEncoder()
    # Consumers may start writing after the sidecar check. Always use SQLite's
    # locking/WAL protocol and one read snapshot, even for a currently closed DB.
    sidecars = [Path(str(path) + suffix) for suffix in ("-wal", "-shm", "-journal")]
    if any(item.is_symlink() for item in sidecars):
        raise ValueError("Snapshot has an aliased sidecar")
    if sidecars[2].exists():
        _probe_writer(path)
        raise ValueError("Snapshot has a rollback journal")
    uri = path.resolve().as_uri() + "?mode=ro"
    with closing(sqlite3.connect(uri, uri=True)) as conn:
        conn.execute("PRAGMA query_only=ON")
        conn.execute("BEGIN")
        if conn.execute("PRAGMA quick_check").fetchall() != [("ok",)]:
            raise ValueError("Snapshot member failed integrity check")
        tables = ((f"trace_entry_{device}", TRACE_COLUMNS), (f"block_{device}", BLOCK_COLUMNS))
        for table, columns in tables:
            digest.update(json.dumps((table, columns), separators=(",", ":")).encode())
            for row in _table_rows(conn, table, columns):
                digest.update(encoder.encode(row, columns == TRACE_COLUMNS))
                digest.update(b"\n")
        count, low, high = conn.execute(
            f'SELECT COUNT(*), MIN(id), MAX(id) FROM "trace_entry_{device}" WHERE id>=0'  # nosec B608 - device validated above
        ).fetchone()
        if (count, low, high) != (end - start + 1, start, end):
            raise ValueError("Snapshot event range is incomplete")
        for row in _table_rows(
            conn,
            "dictionary",
            ("table", "column", "key", "value"),
            'WHERE "table" IN (?, ?)',
            tuple(table for table, _ in tables),
        ):
            digest.update(json.dumps(row, ensure_ascii=True, separators=(",", ":")).encode())
            digest.update(b"\n")
        for table, column in ((f"trace_entry_{device}", "action"), (f"block_{device}", "state")):
            # Both identifiers are internal constants with the device validated above.
            missing = conn.execute(
                f'SELECT 1 FROM (SELECT DISTINCT "{column}" AS value FROM "{table}") AS base WHERE NOT EXISTS '  # nosec B608
                '(SELECT 1 FROM dictionary AS dict WHERE dict."table"=? AND dict."column"=? '
                'AND dict."key"=CAST(base.value AS TEXT)) LIMIT 1',
                (table, column),
            ).fetchone()
            if missing:
                raise ValueError("Snapshot dictionary reference is missing")
    return digest.hexdigest()


def parser_members(directory: Path, manifest: dict[str, Any]) -> dict[str, str]:
    """Validate the complete manifest and all DBs; never deserialize the source."""
    if _integer(manifest.get("schemaVersion"), 1) != 1:
        raise ValueError("Unsupported snapshot manifest version")
    if (
        manifest.get("status") != "complete"
        or not isinstance(manifest.get("sourceFile"), str)
        or not manifest["sourceFile"]
    ):
        raise ValueError("Snapshot is incomplete")
    capacity = _integer(manifest.get("eventsPerSlice"), 1)
    devices = manifest.get("devices")
    if not isinstance(devices, dict) or not devices:
        raise ValueError("Missing snapshot devices")
    members = {}
    for device, data in devices.items():
        if re.fullmatch(r"0|[1-9][0-9]*", device) is None:
            raise ValueError("Invalid device ID")
        if not isinstance(data, dict):
            raise ValueError("Invalid device inventory")
        count = _integer(data["eventCount"], 1)
        size = _integer(data["sliceCount"], 1)
        if size != (count + capacity - 1) // capacity or len(data["slices"]) != size:
            raise ValueError("Incomplete snapshot slice inventory")
        if sorted(_integer(index) for index in data["readySlices"]) != list(range(size)):
            raise ValueError("Snapshot slices are not ready")
        for index, item in enumerate(data["slices"]):
            start, end = index * capacity, min((index + 1) * capacity, count) - 1
            if (
                _integer(item["index"]),
                _integer(item["startEventId"]),
                _integer(item["endEventId"]),
            ) != (index, start, end) or item["ready"] is not True:
                raise ValueError("Invalid snapshot slice range")
            relative = item["file"]
            if relative in members:
                raise ValueError("Duplicate snapshot member")
            members[relative] = member_digest(_member(directory, relative), device, start, end)
            LOGGER.info("Snapshot cache member checked: %s", relative)
    return members


def seal_manifest(directory: Path, manifest: dict[str, Any], device: int | None = None) -> None:
    manifest["parserCache"] = {
        "version": CACHE_CONTRACT_VERSION,
        "requestedDevice": device,
        "members": parser_members(directory, manifest),
    }


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate snapshot manifest key")
        result[key] = value
    return result


def validate_cache(directory: Path, cache_hash: str, device: int | None = None) -> dict[str, Any]:
    path = directory / "manifest.json"
    original = path.read_bytes()
    try:
        manifest = json.loads(original, object_pairs_hook=_unique_object)
    except RecursionError as error:
        raise ValueError("Snapshot manifest nesting is too deep") from error
    if not isinstance(manifest, dict):
        raise ValueError("Invalid snapshot manifest")
    if not cache_hash or manifest.get("cacheHash") != cache_hash:
        raise ValueError("Snapshot source hash or parser salt changed")
    if device is not None and set(manifest["devices"]) != {str(device)}:
        raise ValueError("Snapshot device selection changed")
    for extension in ("parserCache", "ptSnap"):
        if extension in manifest and not isinstance(manifest[extension], dict):
            raise ValueError("Invalid snapshot extension")
    if not isinstance(manifest.get("ptSnap", {}).get("identity", {}), dict):
        raise ValueError("Invalid producer identity")
    # Validate each declaration independently; an absent selection in one seal
    # must not mask a producer's device-filtered export.
    for selection in (
        manifest.get("parserCache", {}),
        manifest.get("ptSnap", {}).get("identity", {}),
    ):
        selected_device = selection.get("requestedDevice")
        if selected_device is None:
            continue
        _integer(selected_device)
        if device is None:
            raise ValueError("Snapshot cache contains only a selected device")
        if selected_device != device or set(manifest["devices"]) != {str(selected_device)}:
            raise ValueError("Snapshot device selection changed")
    members = parser_members(directory, manifest)
    attestation = manifest.get("parserCache")
    # Legacy producers have no logical attestation; still validate every base table and range.
    if attestation is not None and (
        _integer(attestation.get("version"), 1) != CACHE_CONTRACT_VERSION or attestation.get("members") != members
    ):
        raise ValueError("Snapshot parser data changed")
    producer = manifest.get("ptSnap", {})
    if "memberHashKind" in producer and producer["memberHashKind"] != "parser-v1":
        raise ValueError("Unsupported member hash contract")
    if producer.get("memberHashKind") == "parser-v1" and producer.get("members") != members:
        raise ValueError("Snapshot producer member hash changed")
    if "members" in producer and producer.get("memberHashKind") != "parser-v1":
        physical = producer["members"]
        if not isinstance(physical, dict) or set(physical) != set(members):
            raise ValueError("Invalid legacy snapshot member inventory")
        for relative, expected in physical.items():
            member = _member(directory, relative)
            wal = Path(str(member) + "-wal")
            # A read-only open can create an empty WAL and SHM. They contain no
            # committed rows; only the DB bytes need the legacy physical digest.
            if Path(str(member) + "-journal").exists() or (wal.exists() and wal.stat().st_size):
                raise ValueError("Legacy physical snapshot hash cannot validate live sidecars")
            if source_hashes(member)[0] != expected:
                raise ValueError("Legacy snapshot member hash changed")
    if path.read_bytes() != original:
        raise ValueError("Snapshot manifest changed during validation")
    return manifest
