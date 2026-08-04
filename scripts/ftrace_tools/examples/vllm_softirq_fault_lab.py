"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this file according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

         http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

# The two standalone lab scripts intentionally share small Linux utility helpers.
# pylint: disable=duplicate-code,too-many-lines

from __future__ import annotations

import argparse
import ctypes
import json
import multiprocessing as mp
import os
from pathlib import Path
import queue
import re
import signal
import socket
import sys
import time
from typing import Iterable
import uuid


TRACE_MARKER_PATHS = (
    "/sys/kernel/tracing/trace_marker",
    "/sys/kernel/debug/tracing/trace_marker",
)

RPS_SYSFS_ROOT = Path("/sys/class/net")
RPS_GLOBAL_FLOW_ENTRIES_PATH = Path("/proc/sys/net/core/rps_sock_flow_entries")
RPS_RUNTIME_STATE_ROOT = Path("/run/lock/vllm_softirq_fault_lab")
BOOT_ID_PATH = Path("/proc/sys/kernel/random/boot_id")
NETNS_PATH = Path("/proc/self/ns/net")
SOFTNET_STAT_PATH = "/proc/net/softnet_stat"

RECOVERY_GUARD_FIELDS = ("boot_id", "netns", "run_id")
RECOVERY_VALUE_FIELDS = ("device", "queue", "owner_pid", "original_rps_cpus", "original_rps_flow_cnt")

PROMPTS = [
    "Hello, my name is",
    "The future of AI is",
]


def parse_cpu_list(value: str) -> list[int]:
    cpus: set[int] = set()
    for raw_part in value.split(","):
        part = raw_part.strip()
        if not part:
            continue
        if "-" in part:
            start_text, end_text = part.split("-", 1)
            start, end = int(start_text), int(end_text)
            if start < 0 or end < start:
                raise ValueError(f"invalid CPU range: {part!r}")
            cpus.update(range(start, end + 1))
        else:
            cpu = int(part)
            if cpu < 0:
                raise ValueError(f"invalid CPU number: {part!r}")
            cpus.add(cpu)
    if not cpus:
        raise ValueError("CPU list is empty")
    return sorted(cpus)


def format_cpu_list(cpus: Iterable[int]) -> str:
    ordered = sorted(set(cpus))
    if not ordered:
        return ""
    ranges: list[str] = []
    start = previous = ordered[0]
    for cpu in ordered[1:]:
        if cpu == previous + 1:
            previous = cpu
            continue
        ranges.append(str(start) if start == previous else f"{start}-{previous}")
        start = previous = cpu
    ranges.append(str(start) if start == previous else f"{start}-{previous}")
    return ",".join(ranges)


def format_cpu_mask(cpus: Iterable[int]) -> str:
    """Format CPUs as the comma-separated 32-bit mask accepted by rps_cpus."""
    groups: dict[int, int] = {}
    for cpu in set(cpus):
        if cpu < 0:
            raise ValueError(f"invalid CPU number: {cpu}")
        group = cpu // 32
        groups[group] = groups.get(group, 0) | (1 << (cpu % 32))
    if not groups:
        return "00000000"
    return ",".join(f"{groups.get(group, 0):08x}" for group in range(max(groups), -1, -1))


def parse_cpu_mask(value: str) -> set[int]:
    """Parse a Linux hexadecimal cpumask independently of readback padding."""
    raw_groups = value.strip().lower().split(",")
    if not raw_groups or any(not group for group in raw_groups):
        raise ValueError("CPU mask is empty")
    cpus: set[int] = set()
    for group_index, raw_group in enumerate(reversed(raw_groups)):
        group = raw_group[2:] if len(raw_groups) == 1 and raw_group.startswith("0x") else raw_group
        if re.fullmatch(r"[0-9a-f]{1,8}", group) is None:
            raise ValueError(f"invalid 32-bit CPU-mask group: {raw_group!r}")
        bits = int(group, 16)
        bit = 0
        while bits:
            if bits & 1:
                cpus.add(group_index * 32 + bit)
            bit += 1
            bits >>= 1
    return cpus


def collect_output_token_counts(outputs: Iterable[object]) -> list[int]:
    """Return the generated token count for each single-completion request."""
    token_counts: list[int] = []
    for request_index, request_output in enumerate(outputs):
        completions = getattr(request_output, "outputs", None)
        if not completions or len(completions) != 1:
            raise RuntimeError(f"request {request_index} did not return exactly one completion")
        token_ids = getattr(completions[0], "token_ids", None)
        if token_ids is None:
            raise RuntimeError(f"request {request_index} did not expose generated token_ids")
        token_counts.append(len(token_ids))
    return token_counts


def read_runtime_identity() -> tuple[str, str]:
    """Return identities that prevent recovery state reuse across boots or netns."""
    boot_id = read_control_value(BOOT_ID_PATH)
    netns = os.readlink(NETNS_PATH)
    if not boot_id or not netns:
        raise RuntimeError("boot ID and network namespace identity must not be empty")
    return boot_id, netns


def parse_recovery_guard(state: object) -> tuple[str, str, str]:
    if not isinstance(state, dict):
        raise ValueError("recovery state must be a JSON object")
    guard: list[str] = []
    for field in RECOVERY_GUARD_FIELDS:
        value = state.get(field)
        if not isinstance(value, str) or not value:
            raise ValueError(f"{field} must be a non-empty string")
        guard.append(value)
    return guard[0], guard[1], guard[2]


def validate_sysfs_component(value: str, label: str) -> None:
    if not value or Path(value).name != value or value in (".", ".."):
        raise ValueError(f"{label} must be a single sysfs path component: {value!r}")


def set_process_name(name: str) -> None:
    try:
        libc = ctypes.CDLL(None, use_errno=True)
        libc.prctl(15, ctypes.c_char_p(name.encode("utf-8")[:15]), 0, 0, 0)
    except (AttributeError, OSError) as error:
        print(f"[warning] unable to set process name: {error}", file=sys.stderr)


def arm_parent_death_signal(expected_parent_pid: int) -> None:
    """Ensure a traffic worker dies if the controlling process is SIGKILLed."""
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(1, signal.SIGTERM, 0, 0, 0) != 0:  # PR_SET_PDEATHSIG
        error_number = ctypes.get_errno()
        raise OSError(error_number, os.strerror(error_number))
    if os.getppid() != expected_parent_pid:
        raise RuntimeError("traffic-worker parent exited before PR_SET_PDEATHSIG was armed")


def write_trace_marker(message: str) -> bool:
    payload = f"SOFTIRQ_LAB|{message}\n"
    for path in TRACE_MARKER_PATHS:
        try:
            with open(path, "w", encoding="utf-8") as marker:
                marker.write(payload)
            return True
        except OSError:
            continue
    return False


def read_proc_file(path: str) -> str:
    try:
        return Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        return f"<unavailable: {error}>\n"


def read_control_value(path: Path) -> str:
    return path.read_text(encoding="ascii").strip()


def write_control_value(path: Path, value: str) -> None:
    with path.open("w", encoding="ascii") as control_file:
        control_file.write(f"{value}\n")


def write_json_exclusive(path: Path, value: dict[str, object]) -> None:
    payload = (json.dumps(value, indent=2) + "\n").encode("utf-8")
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        offset = 0
        while offset < len(payload):
            written = os.write(descriptor, payload[offset:])
            if written <= 0:
                raise OSError(f"short write while creating recovery state {path}")
            offset += written
        os.fsync(descriptor)
    except OSError:
        os.close(descriptor)
        path.unlink(missing_ok=True)
        raise
    else:
        os.close(descriptor)


class RpsController:
    """Temporarily configure one RX queue and leave a SIGKILL recovery file."""

    def __init__(self, device: str, queue_name: str, target_cpus: list[int], state_dir: Path) -> None:
        self.boot_id, self.netns = read_runtime_identity()
        self.run_id = uuid.uuid4().hex
        self.device = device
        self.queue_name = queue_name
        self.target_cpus = target_cpus
        self.target_mask = format_cpu_mask(target_cpus)
        self.queue_dir = RPS_SYSFS_ROOT / device / "queues" / queue_name
        self.rps_cpus_path = self.queue_dir / "rps_cpus"
        self.rps_flow_cnt_path = self.queue_dir / "rps_flow_cnt"
        self.pending_path = state_dir / "rps_restore_pending.json"
        self.restored_path = state_dir / "rps_state_restored.json"
        self.global_pending_path = RPS_RUNTIME_STATE_ROOT / f"{device}_{queue_name}.json"
        self.original_rps_cpus: str | None = None
        self.original_rps_flow_cnt: str | None = None
        self.global_rps_sock_flow_entries: str | None = None
        self.restore_pending = False

    def inspect(self) -> dict[str, object]:
        if not self.rps_cpus_path.is_file():
            raise RuntimeError(
                f"RPS control is unavailable: {self.rps_cpus_path}; the kernel/device must expose CONFIG_RPS support"
            )
        try:
            self.original_rps_cpus = read_control_value(self.rps_cpus_path)
            self.original_rps_flow_cnt = (
                read_control_value(self.rps_flow_cnt_path) if self.rps_flow_cnt_path.is_file() else None
            )
            self.global_rps_sock_flow_entries = (
                read_control_value(RPS_GLOBAL_FLOW_ENTRIES_PATH) if RPS_GLOBAL_FLOW_ENTRIES_PATH.is_file() else None
            )
        except OSError as error:
            raise RuntimeError(f"unable to read RPS controls under {self.queue_dir}: {error}") from error
        required_writes = [self.rps_cpus_path]
        if self.original_rps_flow_cnt is not None:
            required_writes.append(self.rps_flow_cnt_path)
        unwritable = [path for path in required_writes if not os.access(path, os.W_OK)]
        if unwritable:
            raise RuntimeError(
                "RPS controls are not writable: "
                f"{', '.join(str(path) for path in unwritable)}; run with sufficient privileges"
            )
        return self.snapshot()

    def snapshot(self) -> dict[str, object]:
        current_rps_cpus: str | None = None
        current_rps_flow_cnt: str | None = None
        try:
            if self.rps_cpus_path.is_file():
                current_rps_cpus = read_control_value(self.rps_cpus_path)
            if self.rps_flow_cnt_path.is_file():
                current_rps_flow_cnt = read_control_value(self.rps_flow_cnt_path)
        except OSError:
            pass
        return {
            "boot_id": self.boot_id,
            "netns": self.netns,
            "run_id": self.run_id,
            "device": self.device,
            "queue": self.queue_name,
            "rps_cpus_path": str(self.rps_cpus_path),
            "rps_flow_cnt_path": str(self.rps_flow_cnt_path),
            "local_pending_path": str(self.pending_path),
            "global_pending_path": str(self.global_pending_path),
            "owner_pid": os.getpid(),
            "requested_cpus": self.target_cpus,
            "requested_cpu_list": format_cpu_list(self.target_cpus),
            "requested_mask": self.target_mask,
            "original_rps_cpus": self.original_rps_cpus,
            "original_rps_flow_cnt": self.original_rps_flow_cnt,
            "global_rps_sock_flow_entries_unchanged": self.global_rps_sock_flow_entries,
            "current_rps_cpus": current_rps_cpus,
            "current_rps_flow_cnt": current_rps_flow_cnt,
            "restore_pending": self.restore_pending,
        }

    def _write_pending_state(self) -> None:
        self.pending_path.parent.mkdir(parents=True, exist_ok=True)
        self.global_pending_path.parent.mkdir(parents=True, exist_ok=True)
        if self.pending_path.exists():
            raise RuntimeError(
                f"stale RPS recovery state exists: {self.pending_path}; "
                f"restore it first with --restore-rps-state {self.pending_path}"
            )
        state = self.snapshot()
        state["restore_pending"] = True
        state["created_at_unix_ns"] = time.time_ns()
        try:
            write_json_exclusive(self.global_pending_path, state)
        except FileExistsError as error:
            raise RuntimeError(
                f"another run or an un-restored crash owns {self.device}/{self.queue_name}: "
                f"{self.global_pending_path}; restore it with --restore-rps-state {self.global_pending_path}"
            ) from error
        self.restore_pending = True
        try:
            write_json_exclusive(self.pending_path, state)
        except OSError:
            self.restore_pending = False
            self.global_pending_path.unlink(missing_ok=True)
            raise

    def apply(self) -> None:
        self.inspect()
        try:
            self._write_pending_state()
        except OSError as error:
            raise RuntimeError(f"unable to create RPS recovery state: {error}") from error
        try:
            if self.original_rps_flow_cnt is not None:
                write_control_value(self.rps_flow_cnt_path, "0")
            write_control_value(self.rps_cpus_path, self.target_mask)
            self.verify_applied()
        except (OSError, ValueError, RuntimeError) as error:
            try:
                self.restore()
            except RuntimeError as restore_error:
                raise RuntimeError(
                    f"unable to apply RPS configuration ({error}); automatic restoration also failed: {restore_error}; "
                    f"recovery state: {self.pending_path} or {self.global_pending_path}"
                ) from error
            raise RuntimeError(
                f"unable to apply RPS configuration under {self.queue_dir}: {error}; "
                "run with permission to write the queue RPS controls"
            ) from error

    def verify_applied(self) -> None:
        readback = parse_cpu_mask(read_control_value(self.rps_cpus_path))
        if readback != set(self.target_cpus):
            raise RuntimeError(
                "RPS readback differs from requested CPUs: "
                f"requested={format_cpu_list(self.target_cpus)} "
                f"readback={format_cpu_list(readback)}; check online/housekeeping CPU restrictions"
            )
        if self.original_rps_flow_cnt is not None and int(read_control_value(self.rps_flow_cnt_path), 0) != 0:
            raise RuntimeError(f"RFS was re-enabled through {self.rps_flow_cnt_path}")

    def restore(self) -> None:
        if not self.restore_pending:
            return
        errors: list[str] = []
        rps_cpus_restored = False
        try:
            if self.original_rps_cpus is not None:
                write_control_value(self.rps_cpus_path, self.original_rps_cpus)
                if parse_cpu_mask(read_control_value(self.rps_cpus_path)) != parse_cpu_mask(self.original_rps_cpus):
                    errors.append(f"rps_cpus readback mismatch at {self.rps_cpus_path}")
                else:
                    rps_cpus_restored = True
        except (OSError, ValueError) as error:
            try:
                already_restored = parse_cpu_mask(read_control_value(self.rps_cpus_path)) == parse_cpu_mask(
                    self.original_rps_cpus or ""
                )
            except (OSError, ValueError):
                already_restored = False
            if not already_restored:
                errors.append(f"restore {self.rps_cpus_path}: {error}")
            else:
                rps_cpus_restored = True
        try:
            if rps_cpus_restored and self.original_rps_flow_cnt is not None:
                write_control_value(self.rps_flow_cnt_path, self.original_rps_flow_cnt)
                if int(read_control_value(self.rps_flow_cnt_path), 0) != int(self.original_rps_flow_cnt, 0):
                    errors.append(f"rps_flow_cnt readback mismatch at {self.rps_flow_cnt_path}")
        except (OSError, ValueError) as error:
            try:
                already_restored = int(read_control_value(self.rps_flow_cnt_path), 0) == int(
                    self.original_rps_flow_cnt or "", 0
                )
            except (OSError, ValueError):
                already_restored = False
            if not already_restored:
                errors.append(f"restore {self.rps_flow_cnt_path}: {error}")
        try:
            original_rfs_enabled = self.original_rps_flow_cnt is not None and int(self.original_rps_flow_cnt, 0) != 0
        except ValueError:
            original_rfs_enabled = True
        if not rps_cpus_restored and original_rfs_enabled:
            errors.append("RFS was deliberately left disabled because rps_cpus restoration was not verified")
        if errors:
            raise RuntimeError("; ".join(errors))

        self.restore_pending = False
        state = self.snapshot()
        state["restored_at_unix_ns"] = time.time_ns()
        try:
            self.restored_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
            self.pending_path.unlink()
            self.global_pending_path.unlink()
        except OSError as error:
            raise RuntimeError(f"RPS was restored, but recovery state cleanup failed: {error}") from error


def restore_rps_from_state(state_path: Path) -> None:
    try:
        state = json.loads(state_path.read_text(encoding="utf-8"))
        recovery_guard = parse_recovery_guard(state)
        device = str(state["device"])
        queue_name = str(state["queue"])
        original_rps_cpus_value = state["original_rps_cpus"]
        if not isinstance(original_rps_cpus_value, str):
            raise ValueError("original_rps_cpus must be a string")
        original_rps_cpus = original_rps_cpus_value
        original_rps_flow_cnt = state.get("original_rps_flow_cnt")
        if original_rps_flow_cnt is not None and not isinstance(original_rps_flow_cnt, str):
            raise ValueError("original_rps_flow_cnt must be a string or null")
        parse_cpu_mask(original_rps_cpus)
        if original_rps_flow_cnt is not None:
            int(original_rps_flow_cnt, 0)
        validate_sysfs_component(device, "RPS device")
        validate_sysfs_component(queue_name, "RPS queue")
        owner_pid = int(state.get("owner_pid", -1))
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        raise RuntimeError(f"invalid RPS recovery state {state_path}: {error}") from error

    try:
        current_boot_id, current_netns = read_runtime_identity()
    except (OSError, RuntimeError) as error:
        raise RuntimeError(f"unable to identify the current boot and network namespace: {error}") from error
    if recovery_guard[:2] != (current_boot_id, current_netns):
        raise RuntimeError(
            f"refusing recovery from {state_path}: snapshot boot ID or network namespace does not match this runtime"
        )

    canonical_global_path = RPS_RUNTIME_STATE_ROOT / f"{device}_{queue_name}.json"
    if state_path != canonical_global_path:
        try:
            canonical_state = json.loads(canonical_global_path.read_text(encoding="utf-8"))
            canonical_guard = parse_recovery_guard(canonical_state)
        except (OSError, ValueError, TypeError, json.JSONDecodeError) as error:
            raise RuntimeError(
                f"refusing recovery from local state {state_path}: "
                f"unable to validate canonical guard {canonical_global_path}: {error}"
            ) from error
        if canonical_guard != recovery_guard:
            raise RuntimeError(
                f"refusing recovery from stale local state {state_path}: "
                f"recovery identity does not match canonical guard {canonical_global_path}"
            )
        mismatched_values = [field for field in RECOVERY_VALUE_FIELDS if canonical_state.get(field) != state.get(field)]
        if mismatched_values:
            raise RuntimeError(
                f"refusing recovery from modified local state {state_path}: "
                f"fields differ from canonical guard {canonical_global_path}: {', '.join(mismatched_values)}"
            )

    if owner_pid > 0 and owner_pid != os.getpid() and Path(f"/proc/{owner_pid}").exists():
        raise RuntimeError(f"refusing recovery while owner process {owner_pid} is still running")

    queue_dir = RPS_SYSFS_ROOT / device / "queues" / queue_name
    rps_cpus_path = queue_dir / "rps_cpus"
    rps_flow_cnt_path = queue_dir / "rps_flow_cnt"
    try:
        write_control_value(rps_cpus_path, original_rps_cpus)
        if parse_cpu_mask(read_control_value(rps_cpus_path)) != parse_cpu_mask(original_rps_cpus):
            raise RuntimeError(f"rps_cpus readback mismatch at {rps_cpus_path}")
        if original_rps_flow_cnt is not None:
            write_control_value(rps_flow_cnt_path, str(original_rps_flow_cnt))
            if int(read_control_value(rps_flow_cnt_path), 0) != int(str(original_rps_flow_cnt), 0):
                raise RuntimeError(f"rps_flow_cnt readback mismatch at {rps_flow_cnt_path}")
    except (OSError, ValueError) as error:
        raise RuntimeError(f"unable to restore RPS state under {queue_dir}: {error}") from error

    recovered_name = (
        f"{device}_{queue_name}_recovered.json" if state_path == canonical_global_path else "rps_state_recovered.json"
    )
    recovered_path = state_path.with_name(recovered_name)
    state["restore_pending"] = False
    state["recovered_at_unix_ns"] = time.time_ns()
    try:
        recovered_path.write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
        cleanup_paths = {
            state_path,
            canonical_global_path,
        }
        local_pending_value = state.get("local_pending_path")
        if state_path == canonical_global_path and local_pending_value:
            local_pending_path = Path(str(local_pending_value))
            if local_pending_path.name == "rps_restore_pending.json" and local_pending_path.is_file():
                local_state = json.loads(local_pending_path.read_text(encoding="utf-8"))
                if parse_recovery_guard(local_state) == recovery_guard:
                    cleanup_paths.add(local_pending_path)
        for cleanup_path in cleanup_paths:
            cleanup_path.unlink(missing_ok=True)
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as error:
        raise RuntimeError(f"RPS was restored, but recovery state cleanup failed: {error}") from error
    print(f"[RPS] restored from {state_path}; record={recovered_path}", flush=True)


def parse_softirqs(text: str) -> dict[str, dict[int, int]]:
    lines = [line for line in text.splitlines() if line.strip()]
    if not lines:
        return {}
    cpu_columns: list[int] = []
    for item in lines[0].split():
        if item.startswith("CPU") and item[3:].isdigit():
            cpu_columns.append(int(item[3:]))
    parsed: dict[str, dict[int, int]] = {}
    for line in lines[1:]:
        parts = line.split()
        if len(parts) < 2:
            continue
        name = parts[0].rstrip(":")
        values: list[int] = []
        try:
            values = [int(value) for value in parts[1 : 1 + len(cpu_columns)]]
        except ValueError:
            continue
        parsed[name] = dict(zip(cpu_columns, values))
    return parsed


def diff_softirqs(
    before: dict[str, dict[int, int]],
    after: dict[str, dict[int, int]],
) -> dict[str, dict[int, int]]:
    result: dict[str, dict[int, int]] = {}
    for name in sorted(set(before) | set(after)):
        cpus = sorted(set(before.get(name, {})) | set(after.get(name, {})))
        result[name] = {cpu: after.get(name, {}).get(cpu, 0) - before.get(name, {}).get(cpu, 0) for cpu in cpus}
    return result


def softirq_summary(
    delta: dict[str, dict[int, int]],
    workload_cpus: list[int],
    softirq_cpus: list[int],
    traffic_worker_cpus: list[int],
) -> dict[str, dict[str, int]]:
    interesting = ("NET_RX", "NET_TX", "TIMER", "SCHED", "RCU")
    summary: dict[str, dict[str, int]] = {}
    for name in interesting:
        per_cpu = delta.get(name, {})
        summary[name] = {
            "workload_cpus": sum(per_cpu.get(cpu, 0) for cpu in workload_cpus),
            "softirq_target_cpus": sum(per_cpu.get(cpu, 0) for cpu in softirq_cpus),
            "traffic_worker_cpus": sum(per_cpu.get(cpu, 0) for cpu in traffic_worker_cpus),
            "all_cpus": sum(per_cpu.values()),
        }
    return summary


def parse_softnet_stat(text: str) -> dict[int, dict[str, int]]:
    """Parse stable /proc/net/softnet_stat fields used by this experiment."""
    parsed: dict[int, dict[str, int]] = {}
    for line_index, line in enumerate(text.splitlines()):
        parts = line.split()
        if len(parts) < 3:
            continue
        try:
            values = [int(part, 16) for part in parts]
        except ValueError:
            continue
        cpu = values[12] if len(values) > 12 else line_index
        parsed[cpu] = {
            "processed": values[0],
            "dropped": values[1],
            "time_squeeze": values[2],
            "received_rps": values[9] if len(values) > 9 else 0,
            "flow_limit": values[10] if len(values) > 10 else 0,
        }
    return parsed


def diff_softnet_stat(
    before: dict[int, dict[str, int]],
    after: dict[int, dict[str, int]],
) -> dict[int, dict[str, int]]:
    result: dict[int, dict[str, int]] = {}
    for cpu in sorted(set(before) | set(after)):
        fields = set(before.get(cpu, {})) | set(after.get(cpu, {}))
        result[cpu] = {
            field: (after.get(cpu, {}).get(field, 0) - before.get(cpu, {}).get(field, 0)) & 0xFFFFFFFF
            for field in fields
        }
    return result


def softnet_summary(
    delta: dict[int, dict[str, int]],
    workload_cpus: list[int],
    softirq_cpus: list[int],
    traffic_worker_cpus: list[int],
) -> dict[str, dict[str, int]]:
    summary: dict[str, dict[str, int]] = {}
    for field in ("processed", "dropped", "time_squeeze", "received_rps", "flow_limit"):
        summary[field] = {
            "workload_cpus": sum(delta.get(cpu, {}).get(field, 0) for cpu in workload_cpus),
            "softirq_target_cpus": sum(delta.get(cpu, {}).get(field, 0) for cpu in softirq_cpus),
            "traffic_worker_cpus": sum(delta.get(cpu, {}).get(field, 0) for cpu in traffic_worker_cpus),
            "all_cpus": sum(per_cpu.get(field, 0) for per_cpu in delta.values()),
        }
    return summary


def traffic_is_active(origin_ns: int, period_ns: int, duty_ns: int) -> bool:
    return (time.monotonic_ns() - origin_ns) % period_ns < duty_ns


def udp_receiver(
    index: int,
    port: int,
    parent_pid: int,
    cpus: list[int],
    stop_event: mp.synchronize.Event,
    ready_queue: mp.queues.Queue,
    stats: mp.sharedctypes.SynchronizedArray,
) -> None:
    sock: socket.socket | None = None
    received = 0
    try:
        arm_parent_death_signal(parent_pid)
        set_process_name(f"sir_rx{index:02d}")
        os.sched_setaffinity(0, set(cpus))  # pylint: disable=no-member
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
        sock.bind(("127.0.0.1", port))
        sock.settimeout(0.01)
        ready_queue.put((os.getpid(), f"rx{index}", None))

        buffer = bytearray(65535)
        view = memoryview(buffer)
        while not stop_event.is_set():
            try:
                sock.recvfrom_into(view)
                received += 1
                stats[index] = received
            except socket.timeout:
                continue
            except BlockingIOError:
                stop_event.wait(timeout=0.0005)
    except Exception as error:
        ready_queue.put((os.getpid(), f"rx{index}", repr(error)))
    finally:
        stats[index] = received
        if sock is not None:
            sock.close()


def udp_sender(
    index: int,
    port: int,
    source_port: int,
    parent_pid: int,
    cpus: list[int],
    sender_event: mp.synchronize.Event,
    stop_event: mp.synchronize.Event,
    origin_ns: mp.sharedctypes.Synchronized,
    ready_queue: mp.queues.Queue,
    stats: mp.sharedctypes.SynchronizedArray,
    stats_offset: int,
    packet_size: int,
    pps: int,
    period_ms: int,
    duty_ms: int,
) -> None:
    sock: socket.socket | None = None
    sent = 0
    try:
        arm_parent_death_signal(parent_pid)
        set_process_name(f"sir_tx{index:02d}")
        os.sched_setaffinity(0, set(cpus))  # pylint: disable=no-member
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4 * 1024 * 1024)
        sock.bind(("127.0.0.1", source_port))
        sock.connect(("127.0.0.1", port))
        sock.setblocking(False)
        payload = bytes((index + offset) & 0xFF for offset in range(packet_size))
        ready_queue.put((os.getpid(), f"tx{index}", None))

        period_ns = period_ms * 1_000_000
        duty_ns = duty_ms * 1_000_000
        tokens = 0.0
        last_refill_ns = time.monotonic_ns()
        while not stop_event.is_set():
            if not sender_event.wait(timeout=0.1):
                tokens = 0.0
                last_refill_ns = time.monotonic_ns()
                continue
            now_ns = time.monotonic_ns()
            if not traffic_is_active(origin_ns.value, period_ns, duty_ns):
                tokens = 0.0
                last_refill_ns = now_ns
                stop_event.wait(timeout=0.001)
                continue

            elapsed = max(0, now_ns - last_refill_ns)
            last_refill_ns = now_ns
            tokens = min(tokens + elapsed * pps / 1e9, 256.0)
            budget = min(int(tokens), 64)
            if budget <= 0:
                stop_event.wait(timeout=0.0001)
                continue

            completed = 0
            for _ in range(budget):
                try:
                    sock.send(payload)
                    sent += 1
                    completed += 1
                except (BlockingIOError, OSError):
                    break
            tokens -= completed
            stats[stats_offset + index] = sent
    except Exception as error:
        ready_queue.put((os.getpid(), f"tx{index}", repr(error)))
    finally:
        stats[stats_offset + index] = sent
        if sock is not None:
            sock.close()


def start_traffic_workers(
    ctx: mp.context.BaseContext,
    parent_pid: int,
    pairs: int,
    port_base: int,
    traffic_cpus: list[int],
    packet_size: int,
    pps_per_sender: int,
    period_ms: int,
    duty_ms: int,
    sender_event: mp.synchronize.Event,
    stop_event: mp.synchronize.Event,
    origin_ns: mp.sharedctypes.Synchronized,
    ready_queue: mp.queues.Queue,
    stats: mp.sharedctypes.SynchronizedArray,
) -> list[mp.Process]:
    receivers = [
        ctx.Process(
            target=udp_receiver,
            name=f"sir_rx{index:02d}",
            args=(
                index,
                port_base + index,
                parent_pid,
                traffic_cpus,
                stop_event,
                ready_queue,
                stats,
            ),
        )
        for index in range(pairs)
    ]
    senders = [
        ctx.Process(
            target=udp_sender,
            name=f"sir_tx{index:02d}",
            args=(
                index,
                port_base + index,
                port_base + pairs + index,
                parent_pid,
                traffic_cpus,
                sender_event,
                stop_event,
                origin_ns,
                ready_queue,
                stats,
                pairs,
                packet_size,
                pps_per_sender,
                period_ms,
                duty_ms,
            ),
        )
        for index in range(pairs)
    ]
    workers = receivers + senders
    started: list[mp.Process] = []
    try:
        for worker in workers:
            worker.start()
            started.append(worker)
    except Exception:
        try:
            stop_workers(started, sender_event, stop_event)
        except RuntimeError as cleanup_error:
            print(f"[warning] partial worker cleanup failed: {cleanup_error}", file=sys.stderr)
        raise
    return workers


def wait_workers_ready(
    workers: list[mp.Process],
    ready_queue: mp.queues.Queue,
    timeout_seconds: float = 30.0,
) -> list[dict[str, object]]:
    ready: list[dict[str, object]] = []
    deadline = time.monotonic() + timeout_seconds
    for _ in workers:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError("timed out while starting UDP traffic workers")
        try:
            pid, role, error = ready_queue.get(timeout=remaining)
        except queue.Empty as exc:
            raise RuntimeError("timed out while starting UDP traffic workers") from exc
        if error:
            raise RuntimeError(f"traffic worker {role} ({pid}) failed: {error}")
        ready.append({"pid": pid, "role": role})
    return ready


def ensure_workers_alive(workers: list[mp.Process], stage: str) -> None:
    dead_workers = [worker.name for worker in workers if not worker.is_alive()]
    if dead_workers:
        raise RuntimeError(f"traffic workers exited {stage}: {', '.join(dead_workers)}")


def stop_workers(
    workers: list[mp.Process],
    sender_event: mp.synchronize.Event,
    stop_event: mp.synchronize.Event,
) -> None:
    errors: list[str] = []
    try:
        stop_event.set()
        sender_event.set()
    except Exception as error:  # pragma: no cover - multiprocessing backend failure
        errors.append(f"set stop events: {error}")
    for worker in workers:
        try:
            worker.join(timeout=2.0)
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            errors.append(f"join {worker.name}: {error}")
    for worker in workers:
        try:
            if worker.is_alive():
                worker.terminate()
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            errors.append(f"terminate {worker.name}: {error}")
    for worker in workers:
        try:
            worker.join(timeout=2.0)
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            errors.append(f"final join {worker.name}: {error}")
    if errors:
        raise RuntimeError("traffic-worker cleanup failed: " + "; ".join(errors))


def run_rps_probe(
    state_dir: Path,
    workers: list[mp.Process],
    sender_event: mp.synchronize.Event,
    stop_event: mp.synchronize.Event,
    origin_ns: mp.sharedctypes.Synchronized,
    stats: mp.sharedctypes.SynchronizedArray,
    pairs: int,
    seconds: float,
    workload_cpus: list[int],
    softirq_cpus: list[int],
    traffic_worker_cpus: list[int],
) -> dict[str, object]:
    ensure_workers_alive(workers, "before the RPS probe")
    stats_before = [int(stats[index]) for index in range(pairs * 2)]
    before_softirqs = read_proc_file("/proc/softirqs")
    before_softnet = read_proc_file(SOFTNET_STAT_PATH)
    origin_ns.value = time.monotonic_ns()
    sender_event.set()
    try:
        if stop_event.wait(timeout=seconds):
            raise InterruptedError("RPS probe interrupted")
    finally:
        sender_event.clear()
    if stop_event.wait(timeout=0.5):
        raise InterruptedError("RPS probe drain interrupted")
    stats_after = [int(stats[index]) for index in range(pairs * 2)]
    stats_delta = [after - before for before, after in zip(stats_before, stats_after)]
    packets_received = sum(stats_delta[:pairs])
    packets_sent = sum(stats_delta[pairs:])
    ensure_workers_alive(workers, "during the RPS probe")
    if packets_sent <= 0 or packets_received <= 0:
        raise RuntimeError(
            f"RPS probe traffic was not generated/received: sent={packets_sent} received={packets_received}"
        )
    after_softirqs = read_proc_file("/proc/softirqs")
    after_softnet = read_proc_file(SOFTNET_STAT_PATH)

    softirq_delta = diff_softirqs(parse_softirqs(before_softirqs), parse_softirqs(after_softirqs))
    irq_summary = softirq_summary(softirq_delta, workload_cpus, softirq_cpus, traffic_worker_cpus)
    softnet_delta = diff_softnet_stat(parse_softnet_stat(before_softnet), parse_softnet_stat(after_softnet))
    net_summary = softnet_summary(softnet_delta, workload_cpus, softirq_cpus, traffic_worker_cpus)
    target_net_rx = irq_summary["NET_RX"]["softirq_target_cpus"]
    target_received_rps = net_summary["received_rps"]["softirq_target_cpus"]
    covered_target_cpus = [cpu for cpu in softirq_cpus if softnet_delta.get(cpu, {}).get("received_rps", 0) > 0]
    result: dict[str, object] = {
        "seconds": seconds,
        "softirq_summary": irq_summary,
        "softnet_summary": net_summary,
        "softirq_per_cpu": softirq_delta,
        "softnet_per_cpu": softnet_delta,
        "covered_softirq_target_cpus": covered_target_cpus,
        "packets_sent": packets_sent,
        "packets_received": packets_received,
        "worker_stats_before": stats_before,
        "worker_stats_after": stats_after,
    }
    (state_dir / "rps_probe.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    (state_dir / "rps_probe_softirqs_before.txt").write_text(before_softirqs, encoding="utf-8")
    (state_dir / "rps_probe_softirqs_after.txt").write_text(after_softirqs, encoding="utf-8")
    (state_dir / "rps_probe_softnet_before.txt").write_text(before_softnet, encoding="utf-8")
    (state_dir / "rps_probe_softnet_after.txt").write_text(after_softnet, encoding="utf-8")

    if target_net_rx <= 0:
        raise RuntimeError(
            "RPS probe produced no NET_RX increment on the requested SoftIRQ CPUs; "
            "do not collect data with this kernel/configuration"
        )
    if target_received_rps <= 0:
        raise RuntimeError(
            "RPS probe produced no received_rps increment on the requested SoftIRQ CPUs; "
            "loopback RPS did not steer the probe traffic"
        )
    minimum_covered_cpus = max(1, (len(softirq_cpus) * 3 + 3) // 4)
    if len(covered_target_cpus) < minimum_covered_cpus:
        raise RuntimeError(
            "RPS probe covered too few requested CPUs: "
            f"covered={format_cpu_list(covered_target_cpus)} required_count={minimum_covered_cpus}; "
            "increase --udp-pairs or narrow --softirq-cpus"
        )
    print(
        f"[RPS probe] target_NET_RX={target_net_rx} "
        f"covered_cpus={format_cpu_list(covered_target_cpus)} "
        f"received_rps={target_received_rps} sent={packets_sent} received={packets_received}",
        flush=True,
    )
    if len(covered_target_cpus) < len(softirq_cpus):
        print(
            "[warning] RPS probe did not cover every requested CPU; increase --udp-pairs or narrow --softirq-cpus",
            file=sys.stderr,
        )
    return result


def countdown(seconds: int) -> None:
    if seconds <= 0:
        return
    print(f"[ARMED] Start ftrace now; profiling begins in {seconds}s.", flush=True)
    for remaining in range(seconds, 0, -1):
        if remaining <= 5 or remaining % 5 == 0:
            print(f"[ARMED] {remaining:2d}s", flush=True)
        time.sleep(1)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Construct a vLLM Host-Bound experiment with UDP SoftIRQ load.")
    parser.add_argument("--mode", choices=("fault", "fixed"))
    parser.add_argument("--workload-cpus", help="CPUs inherited by vLLM, e.g. 120-127")
    parser.add_argument("--softirq-cpus", help="RPS target CPUs for NET_RX, e.g. 120-127 or 128-135")
    parser.add_argument(
        "--traffic-worker-cpus",
        "--traffic-cpus",
        dest="traffic_worker_cpus",
        help="CPUs used by all sir_tx/sir_rx processes; must be isolated from workload and SoftIRQ CPUs",
    )
    parser.add_argument("--rps-device", default="lo", help="loopback device; currently must be lo")
    parser.add_argument("--rps-queue", default="rx-0", help="loopback RX queue; currently must be rx-0")
    parser.add_argument(
        "--rps-probe-seconds",
        type=float,
        default=1.2,
        help="pre-vLLM traffic probe duration used to verify NET_RX placement",
    )
    parser.add_argument(
        "--preflight-only",
        action="store_true",
        help="configure RPS, run its placement probe, restore RPS, and exit without loading vLLM",
    )
    parser.add_argument(
        "--restore-rps-state",
        metavar="FILE",
        help="restore RPS/RFS values from a rps_restore_pending.json file and exit",
    )
    parser.add_argument("--model", default="Qwen/Qwen3-0.6B")
    parser.add_argument("--max-model-len", type=int, default=26240)
    parser.add_argument("--max-tokens", type=int, default=64)
    parser.add_argument("--request-seed", type=int, default=2026, help="Request-level sampling seed used in every run")
    parser.add_argument("--tensor-parallel-size", type=int, default=1)
    parser.add_argument("--udp-pairs", type=int, default=16)
    parser.add_argument("--port-base", type=int, default=39000)
    parser.add_argument("--packet-size", type=int, default=128)
    parser.add_argument(
        "--pps-per-sender",
        type=int,
        default=25_000,
        help="Target loopback UDP packets/s for each sender",
    )
    parser.add_argument("--traffic-period-ms", type=int, default=500)
    parser.add_argument("--traffic-duty-ms", type=int, default=450)
    parser.add_argument("--arm-seconds", type=int, default=10)
    parser.add_argument("--warmup-tokens", type=int, default=8)
    parser.add_argument("--profile-dir")
    parser.add_argument("--state-dir")
    parser.add_argument("--enforce-eager", action="store_true")
    return parser


def validate_args(
    args: argparse.Namespace,
    parser: argparse.ArgumentParser,
) -> tuple[list[int], list[int], list[int]]:
    missing = [
        option
        for option, value in (
            ("--mode", args.mode),
            ("--workload-cpus", args.workload_cpus),
            ("--softirq-cpus", args.softirq_cpus),
            ("--traffic-worker-cpus", args.traffic_worker_cpus),
        )
        if value is None
    ]
    if missing:
        parser.error(f"the following arguments are required for a lab run: {', '.join(missing)}")
    try:
        workload_cpus = parse_cpu_list(args.workload_cpus)
        softirq_cpus = parse_cpu_list(args.softirq_cpus)
        traffic_worker_cpus = parse_cpu_list(args.traffic_worker_cpus)
        validate_sysfs_component(args.rps_device, "RPS device")
        validate_sysfs_component(args.rps_queue, "RPS queue")
    except ValueError as error:
        parser.error(str(error))
    if args.rps_device != "lo" or args.rps_queue != "rx-0":
        parser.error("this loopback traffic generator requires --rps-device lo --rps-queue rx-0")

    allowed = set(os.sched_getaffinity(0))  # pylint: disable=no-member
    unavailable = (set(workload_cpus) | set(softirq_cpus) | set(traffic_worker_cpus)) - allowed
    if unavailable:
        parser.error(
            "requested CPUs are outside the current cgroup/cpuset: "
            f"{format_cpu_list(unavailable)}; allowed={format_cpu_list(allowed)}"
        )
    worker_workload_overlap = set(workload_cpus) & set(traffic_worker_cpus)
    if worker_workload_overlap:
        parser.error(
            f"traffic workers must not overlap workload CPUs; overlap={format_cpu_list(worker_workload_overlap)}"
        )
    worker_softirq_overlap = set(softirq_cpus) & set(traffic_worker_cpus)
    if worker_softirq_overlap:
        parser.error(
            f"traffic workers must not overlap SoftIRQ target CPUs; overlap={format_cpu_list(worker_softirq_overlap)}"
        )
    workload_softirq_overlap = set(workload_cpus) & set(softirq_cpus)
    if args.mode == "fault" and not set(softirq_cpus).issubset(workload_cpus):
        parser.error("fault mode requires --softirq-cpus to be a subset of --workload-cpus")
    if args.mode == "fixed" and workload_softirq_overlap:
        parser.error(
            "fixed mode requires workload and SoftIRQ target CPUs to be disjoint; "
            f"overlap={format_cpu_list(workload_softirq_overlap)}"
        )
    if args.udp_pairs < 1 or args.udp_pairs > 64:
        parser.error("--udp-pairs must be in [1, 64]")
    if not 1 <= args.port_base <= 65535 - 2 * args.udp_pairs:
        parser.error("--port-base and two ports per UDP pair exceed the UDP port range")
    if not 32 <= args.packet_size <= 65507:
        parser.error("--packet-size must be in [32, 65507]")
    if args.pps_per_sender < 1:
        parser.error("--pps-per-sender must be positive")
    if args.traffic_period_ms <= 0:
        parser.error("--traffic-period-ms must be positive")
    if not 0 < args.traffic_duty_ms < args.traffic_period_ms:
        parser.error("--traffic-duty-ms must be > 0 and < --traffic-period-ms")
    if args.max_tokens < 1 or args.warmup_tokens < 0:
        parser.error("token counts are invalid")
    if args.request_seed < 0:
        parser.error("--request-seed must be non-negative")
    if not 0.1 <= args.rps_probe_seconds <= 60:
        parser.error("--rps-probe-seconds must be in [0.1, 60]")
    if args.udp_pairs < len(softirq_cpus):
        print(
            "[warning] fewer UDP flows than SoftIRQ target CPUs; RPS cannot cover every target CPU",
            file=sys.stderr,
        )
    return workload_cpus, softirq_cpus, traffic_worker_cpus


def write_state(
    state_dir: Path,
    metadata: dict[str, object],
    before_softirqs: str,
    after_softirqs: str,
    before_interrupts: str,
    after_interrupts: str,
    before_softnet: str,
    after_softnet: str,
    softirq_delta: dict[str, dict[int, int]],
    irq_summary: dict[str, dict[str, int]],
    softnet_delta: dict[int, dict[str, int]],
    net_summary: dict[str, dict[str, int]],
) -> None:
    state_dir.mkdir(parents=True, exist_ok=True)
    (state_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    (state_dir / "softirqs_before.txt").write_text(before_softirqs, encoding="utf-8")
    (state_dir / "softirqs_after.txt").write_text(after_softirqs, encoding="utf-8")
    (state_dir / "interrupts_before.txt").write_text(before_interrupts, encoding="utf-8")
    (state_dir / "interrupts_after.txt").write_text(after_interrupts, encoding="utf-8")
    (state_dir / "softnet_before.txt").write_text(before_softnet, encoding="utf-8")
    (state_dir / "softnet_after.txt").write_text(after_softnet, encoding="utf-8")
    (state_dir / "softirq_delta.json").write_text(
        json.dumps({"summary": irq_summary, "per_cpu": softirq_delta}, indent=2) + "\n",
        encoding="utf-8",
    )
    (state_dir / "softnet_delta.json").write_text(
        json.dumps({"summary": net_summary, "per_cpu": softnet_delta}, indent=2) + "\n",
        encoding="utf-8",
    )


def run(args: argparse.Namespace, parser: argparse.ArgumentParser) -> int:
    workload_cpus, softirq_cpus, traffic_worker_cpus = validate_args(args, parser)
    profile_dir = Path(args.profile_dir or f"profiling_softirq_{args.mode}").resolve()
    state_dir = Path(args.state_dir or f"softirq_state_{args.mode}").resolve()
    profile_dir.mkdir(parents=True, exist_ok=True)
    state_dir.mkdir(parents=True, exist_ok=True)
    os.environ["VLLM_TORCH_PROFILER_DIR"] = str(profile_dir)

    ctx = mp.get_context("spawn")
    workers: list[mp.Process] = []
    sender_event = ctx.Event()
    stop_event = ctx.Event()
    origin_ns = ctx.Value("q", 0)
    ready_queue = ctx.Queue()
    stats = ctx.Array("Q", args.udp_pairs * 2, lock=False)
    measurement_stats_baseline = [0] * (args.udp_pairs * 2)
    worker_metadata: list[dict[str, object]] = []
    llm = None
    profiling_started = False
    interrupted = False
    before_softirqs = ""
    after_softirqs = ""
    before_interrupts = ""
    after_interrupts = ""
    before_softnet = ""
    after_softnet = ""
    rps_probe: dict[str, object] = {}
    rps_controller = RpsController(args.rps_device, args.rps_queue, softirq_cpus, state_dir)
    generation_metadata: dict[str, object] = {
        "model": args.model,
        "request_count": len(PROMPTS),
        "sampling": {
            "temperature": 0.8,
            "top_p": 0.95,
            "request_seed": args.request_seed,
            "min_tokens": args.max_tokens,
            "max_tokens": args.max_tokens,
        },
        "result": None,
    }

    def handle_signal(signum: int, _frame: object) -> None:
        nonlocal interrupted
        interrupted = True
        stop_event.set()
        print(f"\n[signal] received {signum}; stopping", flush=True)

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)
    if hasattr(signal, "SIGHUP"):
        signal.signal(signal.SIGHUP, handle_signal)

    try:
        print(
            f"[RPS] configuring {args.rps_device}/{args.rps_queue} -> {format_cpu_list(softirq_cpus)}; "
            "this temporarily affects all traffic on that RX queue",
            flush=True,
        )
        rps_controller.apply()
        print(
            f"[RPS] applied mask={rps_controller.target_mask}; RFS disabled for this queue; "
            f"recovery={rps_controller.pending_path}; guard={rps_controller.global_pending_path}",
            flush=True,
        )
        if interrupted:
            return 130

        workers = start_traffic_workers(
            ctx,
            os.getpid(),
            args.udp_pairs,
            args.port_base,
            traffic_worker_cpus,
            args.packet_size,
            args.pps_per_sender,
            args.traffic_period_ms,
            args.traffic_duty_ms,
            sender_event,
            stop_event,
            origin_ns,
            ready_queue,
            stats,
        )
        worker_metadata = wait_workers_ready(workers, ready_queue)
        if interrupted:
            return 130
        try:
            rps_probe = run_rps_probe(
                state_dir,
                workers,
                sender_event,
                stop_event,
                origin_ns,
                stats,
                args.udp_pairs,
                args.rps_probe_seconds,
                workload_cpus,
                softirq_cpus,
                traffic_worker_cpus,
            )
        except InterruptedError:
            return 130
        measurement_stats_baseline = [int(stats[index]) for index in range(args.udp_pairs * 2)]
        if args.preflight_only:
            print("[preflight] RPS placement probe passed; vLLM was not loaded", flush=True)
            return 0

        # vLLM and its future children inherit only the workload CPU set.
        os.sched_setaffinity(0, set(workload_cpus))  # pylint: disable=no-member
        set_process_name(f"sir_{args.mode}")

        from vllm import LLM, SamplingParams

        print(
            f"[setup] mode={args.mode} workload_cpus={format_cpu_list(workload_cpus)} "
            f"softirq_cpus={format_cpu_list(softirq_cpus)} "
            f"traffic_worker_cpus={format_cpu_list(traffic_worker_cpus)} "
            f"target_pps={args.udp_pairs * args.pps_per_sender:,}",
            flush=True,
        )
        llm_kwargs = {
            "model": args.model,
            "max_model_len": args.max_model_len,
            "tensor_parallel_size": args.tensor_parallel_size,
        }
        if args.enforce_eager:
            llm_kwargs["enforce_eager"] = True
        llm = LLM(**llm_kwargs)
        if interrupted:
            return 130

        if args.warmup_tokens:
            warmup_params = SamplingParams(
                temperature=0.0,
                min_tokens=args.warmup_tokens,
                max_tokens=args.warmup_tokens,
            )
            print("[warmup] running outside the measurement window", flush=True)
            llm.generate(PROMPTS, warmup_params, use_tqdm=False)
        if interrupted:
            return 130

        ensure_workers_alive(workers, "before arming the measurement")
        rps_controller.verify_applied()

        metadata: dict[str, object] = {
            "mode": args.mode,
            "root_pid": os.getpid(),
            "workload_cpus": workload_cpus,
            "softirq_cpus": softirq_cpus,
            "traffic_worker_cpus": traffic_worker_cpus,
            "traffic_workers": worker_metadata,
            "udp_pairs": args.udp_pairs,
            "packet_size": args.packet_size,
            "pps_per_sender": args.pps_per_sender,
            "target_total_pps": args.udp_pairs * args.pps_per_sender,
            "traffic_period_ms": args.traffic_period_ms,
            "traffic_duty_ms": args.traffic_duty_ms,
            "profile_dir": str(profile_dir),
            "generation": generation_metadata,
            "rps": rps_controller.snapshot(),
            "rps_probe": rps_probe,
            "network_settings": {
                "netdev_max_backlog": read_proc_file("/proc/sys/net/core/netdev_max_backlog").strip(),
                "netdev_budget": read_proc_file("/proc/sys/net/core/netdev_budget").strip(),
                "netdev_budget_usecs": read_proc_file("/proc/sys/net/core/netdev_budget_usecs").strip(),
            },
        }
        (state_dir / "metadata_armed.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        write_trace_marker(
            f"ARMED mode={args.mode} workload={format_cpu_list(workload_cpus)} "
            f"softirq={format_cpu_list(softirq_cpus)} workers={format_cpu_list(traffic_worker_cpus)}"
        )
        countdown(args.arm_seconds)
        if interrupted:
            return 130
        ensure_workers_alive(workers, "before profiling")
        rps_controller.verify_applied()

        before_softirqs = read_proc_file("/proc/softirqs")
        before_interrupts = read_proc_file("/proc/interrupts")
        before_softnet = read_proc_file(SOFTNET_STAT_PATH)
        llm.start_profile()
        profiling_started = True
        write_trace_marker(f"PROFILE_BEGIN mode={args.mode}")

        origin_ns.value = time.monotonic_ns()
        sender_event.set()
        write_trace_marker(f"TRAFFIC_BEGIN mode={args.mode}")
        sampling_params = SamplingParams(
            temperature=0.8,
            top_p=0.95,
            seed=args.request_seed,
            min_tokens=args.max_tokens,
            max_tokens=args.max_tokens,
        )
        begin = time.perf_counter()
        write_trace_marker(f"GENERATE_BEGIN mode={args.mode}")
        outputs = llm.generate(PROMPTS, sampling_params, use_tqdm=False)
        elapsed = time.perf_counter() - begin
        token_counts = collect_output_token_counts(outputs)
        valid_token_counts = len(token_counts) == len(PROMPTS) and all(
            count == args.max_tokens for count in token_counts
        )
        generation_metadata["result"] = {
            "elapsed_seconds": elapsed,
            "output_token_counts": token_counts,
            "length_valid": valid_token_counts,
        }
        write_trace_marker(
            f"GENERATE_END mode={args.mode} elapsed_s={elapsed:.6f} "
            f"tokens={','.join(str(count) for count in token_counts)}"
        )
        print(f"[result] generate={elapsed:.3f}s", flush=True)
        if not valid_token_counts:
            raise RuntimeError(
                "generation workload is invalid: "
                f"expected {len(PROMPTS)} outputs with {args.max_tokens} tokens each, got {token_counts}"
            )
        if not interrupted:
            ensure_workers_alive(workers, "during the measurement")

        stop_event.set()
        write_trace_marker(f"TRAFFIC_END mode={args.mode}")
        for output in outputs:
            generated_text = output.outputs[0].text
            print(
                f"Prompt: {output.prompt!r}, Generated text: {generated_text!r}",
                flush=True,
            )
        return 130 if interrupted else 0
    finally:
        active_exception = sys.exc_info()[0] is not None
        cleanup_error: RuntimeError | None = None
        try:
            stop_event.set()
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            cleanup_error = RuntimeError(f"unable to signal traffic workers: {error}")
        if profiling_started and llm is not None:
            write_trace_marker(f"PROFILE_END mode={args.mode}")
            try:
                llm.stop_profile()
            except Exception as error:
                print(f"[warning] stop_profile failed: {error!r}", file=sys.stderr)
        try:
            stop_workers(workers, sender_event, stop_event)
        except RuntimeError as error:
            cleanup_error = error
            print(f"[warning] {error}", file=sys.stderr)

        after_softirqs = read_proc_file("/proc/softirqs")
        after_interrupts = read_proc_file("/proc/interrupts")
        after_softnet = read_proc_file(SOFTNET_STAT_PATH)
        restore_error: RuntimeError | None = None
        try:
            rps_controller.restore()
            if rps_controller.original_rps_cpus is not None:
                print("[RPS] original queue configuration restored", flush=True)
        except RuntimeError as error:
            restore_error = error
            print(
                f"[error] RPS restoration failed: {error}; recovery state: {rps_controller.pending_path} "
                f"or {rps_controller.global_pending_path}",
                file=sys.stderr,
            )
        if before_softirqs:
            softirq_delta = diff_softirqs(
                parse_softirqs(before_softirqs),
                parse_softirqs(after_softirqs),
            )
            irq_summary = softirq_summary(softirq_delta, workload_cpus, softirq_cpus, traffic_worker_cpus)
            softnet_delta = diff_softnet_stat(
                parse_softnet_stat(before_softnet),
                parse_softnet_stat(after_softnet),
            )
            net_summary = softnet_summary(softnet_delta, workload_cpus, softirq_cpus, traffic_worker_cpus)
            packets_received = [int(stats[i]) - measurement_stats_baseline[i] for i in range(args.udp_pairs)]
            packets_sent = [
                int(stats[args.udp_pairs + i]) - measurement_stats_baseline[args.udp_pairs + i]
                for i in range(args.udp_pairs)
            ]
            metadata = {
                "mode": args.mode,
                "root_pid": os.getpid(),
                "workload_cpus": workload_cpus,
                "softirq_cpus": softirq_cpus,
                "traffic_worker_cpus": traffic_worker_cpus,
                "traffic_workers": worker_metadata,
                "udp_pairs": args.udp_pairs,
                "packet_size": args.packet_size,
                "pps_per_sender": args.pps_per_sender,
                "packets_received": packets_received,
                "packets_sent": packets_sent,
                "worker_stats_including_probe": [int(stats[i]) for i in range(args.udp_pairs * 2)],
                "profile_dir": str(profile_dir),
                "generation": generation_metadata,
                "rps": rps_controller.snapshot(),
                "rps_probe": rps_probe,
                "rps_restore_error": str(restore_error) if restore_error else None,
            }
            write_state(
                state_dir,
                metadata,
                before_softirqs,
                after_softirqs,
                before_interrupts,
                after_interrupts,
                before_softnet,
                after_softnet,
                softirq_delta,
                irq_summary,
                softnet_delta,
                net_summary,
            )
            print(f"[softirq delta] {json.dumps(irq_summary, ensure_ascii=False)}", flush=True)
            print(f"[softnet delta] {json.dumps(net_summary, ensure_ascii=False)}", flush=True)
            if net_summary["dropped"]["softirq_target_cpus"] > 0:
                print("[warning] target CPUs reported softnet drops; lower --pps-per-sender", file=sys.stderr)
            if net_summary["time_squeeze"]["softirq_target_cpus"] > 0:
                print(
                    "[warning] target CPUs reported softnet time_squeeze; lower packet rate to avoid ksoftirqd",
                    file=sys.stderr,
                )
            measurement_errors: list[str] = []
            if not interrupted and sum(packets_sent) <= 0:
                measurement_errors.append("traffic workers sent no packets during the measurement")
            if not interrupted and irq_summary["NET_RX"]["softirq_target_cpus"] <= 0:
                measurement_errors.append("target CPUs recorded no NET_RX increment during the measurement")
            if not interrupted and net_summary["received_rps"]["softirq_target_cpus"] <= 0:
                measurement_errors.append("target CPUs recorded no received_rps increment during the measurement")
            if sum(packets_sent) > 0 and sum(packets_received) * 10 < sum(packets_sent) * 9:
                print(
                    "[warning] fewer than 90% of sent packets were received; lower --pps-per-sender",
                    file=sys.stderr,
                )
            print(f"[state] {state_dir}", flush=True)
        if restore_error is not None and not active_exception:
            raise restore_error
        if cleanup_error is not None and not active_exception:
            raise cleanup_error
        if before_softirqs and measurement_errors and not active_exception:
            raise RuntimeError("invalid SoftIRQ measurement: " + "; ".join(measurement_errors))


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.restore_rps_state:
        try:
            restore_rps_from_state(Path(args.restore_rps_state).resolve())
        except RuntimeError as error:
            parser.exit(2, f"error: {error}\n")
        return 0
    return run(args, parser)


if __name__ == "__main__":
    raise SystemExit(main())
