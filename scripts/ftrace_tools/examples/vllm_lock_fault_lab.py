#!/usr/bin/env python3
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

Single-card vLLM-Ascend lab: a long shared-lock critical section stalls dispatch.
Only standard-library imports are needed for --help.
"""

from __future__ import annotations

import argparse
import ctypes
from datetime import datetime
import hashlib
import importlib.metadata
import inspect
import json
import math
import os
from pathlib import Path
import platform
import queue
import signal
import sys
import threading
import time


TRACE_MARKERS = ("/sys/kernel/tracing/trace_marker", "/sys/kernel/debug/tracing/trace_marker")


def raw_ns() -> int:
    """Use the same CLOCK_MONOTONIC_RAW clock as trace_record.py."""
    # These APIs are available on Linux, which validate_args requires.
    # pylint: disable-next=no-member
    return time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)


def stamp() -> dict:
    return {"wall_time": datetime.now().astimezone().isoformat(timespec="milliseconds"), "raw_ns": raw_ns()}


def write_json(path: Path, data: dict) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def announce(label: str, message: str) -> dict:
    point = stamp()
    print(f"[{label}] {point['wall_time']} monotonic_raw_ns={point['raw_ns']} | {message}", flush=True)
    return point


def parse_cpu_list(value: str) -> list[int]:
    cpus = set()
    for part in value.split(","):
        if "-" in part:
            begin, end = (int(item) for item in part.split("-", 1))
            if begin < 0 or end < begin:
                raise ValueError(f"invalid CPU range: {part}")
            cpus.update(range(begin, end + 1))
        else:
            cpu = int(part)
            if cpu < 0:
                raise ValueError("CPU numbers must be non-negative")
            cpus.add(cpu)
    if not cpus:
        raise ValueError("CPU list must not be empty")
    return sorted(cpus)


def trace_marker(message: str) -> bool:
    # Markers are auxiliary raw-trace evidence; current trace_convert does not import them.
    for path in TRACE_MARKERS:
        try:
            with open(path, "w", encoding="utf-8") as stream:
                stream.write(f"LOCK_LAB|{message}\n")
            return True
        except OSError:
            continue
    return False


def set_thread_name(name: str) -> None:
    if sys.platform == "linux":
        libc = ctypes.CDLL(None, use_errno=True)
        if libc.prctl(15, ctypes.c_char_p(name.encode()[:15]), 0, 0, 0) != 0:
            raise OSError(ctypes.get_errno(), "PR_SET_NAME failed")


class LockContentionLab:
    """One dispatch thread, one helper, and one shared statistics lock.

    The ready/go handshake makes contention reproducible. It has its own range;
    only the real lock acquisition is measured as lock_wait. Hash work is fixed,
    never calibrated separately in Fault and Fixed and never replaced by sleep.
    """

    def __init__(self, config: dict, annotate):
        self.config = config
        self.annotate = annotate
        self.payload = b"L" * (config["hash_mib"] * 1024 * 1024)
        self.expected_digest = hashlib.sha256(self.payload).hexdigest()
        self.lock = threading.Lock()
        self.jobs_queue = queue.Queue(maxsize=1)
        self.started = threading.Event()
        self.cancel = threading.Event()
        self.execution_guard = threading.Lock()
        self.enabled = False
        self.closed = False
        self.probe_calls = 0
        self.step = 0
        self.counter = 0
        self.dispatch_tids = set()
        self.helper_tid = None
        self.helper_affinity = []
        self.error = None
        self.jobs = []
        self.waits = []
        self.marker_ok = False
        self.thread = threading.Thread(target=self._helper_main, name="hb_lock_helper", daemon=True)
        self.thread.start()
        if not self.started.wait(config["timeout_seconds"]):
            self.cancel.set()
            raise RuntimeError("helper did not start")
        self.check_error()

    def check_error(self) -> None:
        if self.error:
            raise RuntimeError(self.error)

    def marker(self, message: str) -> bool:
        return trace_marker(message)

    def _hash(self, job: dict) -> None:
        job["work_start_raw_ns"] = raw_ns()
        job["hash_calls"] = 0
        for _ in range(self.config["hash_rounds"]):
            if self.cancel.is_set():
                raise RuntimeError("background hash work cancelled; this capture is invalid")
            # A large single update releases the GIL on CPython.
            digest = hashlib.sha256(self.payload).hexdigest()
            if digest != self.expected_digest:
                raise RuntimeError("background hash digest mismatch")
            job["hash_calls"] += 1
        job["digest"] = digest
        job["work_end_raw_ns"] = raw_ns()

    def _await_go(self, job: dict) -> None:
        job["ready"].set()
        if not job["go"].wait(self.config["timeout_seconds"]):
            raise RuntimeError("dispatch/helper handshake timed out")

    def _run_job(self, job: dict) -> None:
        with self.lock:
            job["hold_start_raw_ns"] = raw_ns()
            # The shared snapshot is immutable. Both modes process identical bytes.
            job["snapshot_counter"] = self.counter
            if self.config["mode"] == "fault":
                self.marker(f"HOLD_BEGIN job={job['job_id']} tid={self.helper_tid}")
                self._await_go(job)
                self._hash(job)
            job["hold_end_raw_ns"] = raw_ns()
        if self.config["mode"] == "fixed":
            self._await_go(job)
            self._hash(job)
        self.marker(f"JOB_END job={job['job_id']} mode={self.config['mode']} tid={self.helper_tid}")

    def _helper_main(self) -> None:
        try:
            set_thread_name("hb_lock_helper")
            self.helper_tid = threading.get_native_id()
            if self.config["helper_cpus"]:
                os.sched_setaffinity(0, self.config["helper_cpus"])  # pylint: disable=no-member
                self.helper_affinity = sorted(os.sched_getaffinity(0))  # pylint: disable=no-member
            self.started.set()
            while True:
                job = self.jobs_queue.get()
                if job is None:
                    return
                try:
                    self._run_job(job)
                except Exception as error:
                    self.error = repr(error)
                    job["error"] = self.error
                finally:
                    job["ready"].set()
                    job["done"].set()
                if self.error:
                    return
        except Exception as error:
            self.error = repr(error)
            self.started.set()

    def start(self) -> dict:
        if self.closed or self.enabled or self.step:
            raise RuntimeError("lab can be enabled only once")
        self.check_error()
        if self.probe_calls == 0:
            raise RuntimeError("execute_model hook was not hit during the probe; unsupported execution path")
        self.dispatch_tids.clear()
        self.enabled = True
        self.marker_ok = self.marker(f"WORKER_BEGIN mode={self.config['mode']} pid={os.getpid()}")
        return self.status()

    def _trigger(self) -> dict:
        if self.jobs and not self.jobs[-1]["done"].is_set():
            raise RuntimeError(
                "previous background job is still running; increase --inject-every in BOTH modes and recapture"
            )
        job = {
            "job_id": len(self.jobs) + 1,
            "step": self.step,
            "ready": threading.Event(),
            "go": threading.Event(),
            "done": threading.Event(),
        }
        self.jobs.append(job)
        self.jobs_queue.put_nowait(job)
        with self.annotate("LOCK_LAB::handshake"):
            job["handshake_start_raw_ns"] = raw_ns()
            if not job["ready"].wait(self.config["timeout_seconds"]):
                raise RuntimeError("helper ready handshake timed out")
            job["handshake_end_raw_ns"] = raw_ns()
        self.check_error()
        return job

    def before_submit(self) -> None:
        self.check_error()
        self.step += 1
        tid = threading.get_native_id()
        self.dispatch_tids.add(tid)
        if len(self.dispatch_tids) != 1:
            raise RuntimeError("this lab supports a single model dispatch thread only")
        if self.config["workload_cpus"]:
            affinity = set(os.sched_getaffinity(0))  # pylint: disable=no-member
            if not affinity or not affinity.issubset(self.config["workload_cpus"]):
                raise RuntimeError("vLLM changed dispatch CPU affinity; check CPU_AFFINITY_CONF/cpuset settings")
        job = self._trigger() if self.step % self.config["inject_every"] == 0 else None
        # No profiler or marker I/O between go.set() and acquiring the shared lock.
        # In Fault, helper holds it until the fixed hash work completes.
        acquired = False
        try:
            with self.annotate("LOCK_LAB::lock_wait"):
                begin = raw_ns()
                if job is not None:
                    job["go"].set()
                # Timed acquisition is measured separately from the protected update.
                acquired = self.lock.acquire(timeout=self.config["timeout_seconds"])  # pylint: disable=consider-using-with
                end = raw_ns()
            if not acquired:
                raise RuntimeError("shared lock acquisition timed out")
            self.counter += 1
        finally:
            if acquired:
                self.lock.release()
        self.waits.append({"step": self.step, "tid": tid, "start_raw_ns": begin, "end_raw_ns": end})
        if job is not None:
            job["wait_start_raw_ns"] = begin
            job["wait_end_raw_ns"] = end
            job["dispatch_tid"] = tid
        self.check_error()

    def wrap(self, original):
        def execute(*args, **kwargs):
            if not self.enabled:
                self.probe_calls += 1
                self.dispatch_tids.add(threading.get_native_id())
                return original(*args, **kwargs)
            if not self.execution_guard.acquire(blocking=False):  # pylint: disable=consider-using-with
                raise RuntimeError("concurrent/reentrant execute_model is unsupported by this lab")
            try:
                self.before_submit()
                with self.annotate("LOCK_LAB::host_submit"):
                    return original(*args, **kwargs)
            except Exception as error:
                self.error = repr(error)
                try:
                    self.persist("worker_error.json")
                except OSError as write_error:
                    print(f"[LOCK_LAB] cannot save worker failure: {write_error}", file=sys.stderr, flush=True)
                raise
            finally:
                self.execution_guard.release()

        return execute

    def drain(self) -> float:
        begin = time.perf_counter()
        if self.jobs and not self.jobs[-1]["done"].wait(self.config["timeout_seconds"]):
            raise RuntimeError("background drain timed out")
        self.check_error()
        return time.perf_counter() - begin

    def status(self) -> dict:
        jobs = [{k: v for k, v in job.copy().items() if k not in ("ready", "go", "done")} for job in self.jobs]
        waits_ms = sorted((item["end_raw_ns"] - item["start_raw_ns"]) / 1e6 for item in self.waits)
        contended = (
            sum(
                max(
                    0,
                    min(job.get("wait_end_raw_ns", 0), job.get("hold_end_raw_ns", 0))
                    - max(job.get("wait_start_raw_ns", 0), job.get("work_start_raw_ns", 0)),
                )
                >= 1_000_000
                for job in jobs
            )
            if self.config["mode"] == "fault"
            else 0
        )
        return {
            "pid": os.getpid(),
            "clock": "CLOCK_MONOTONIC_RAW",
            "mode": self.config["mode"],
            "lock_id": "shared_statistics_lock",
            "helper_tid": self.helper_tid,
            "helper_affinity": self.helper_affinity,
            "dispatch_tids": sorted(self.dispatch_tids),
            "probe_calls": self.probe_calls,
            "execute_calls": self.step,
            "job_count": len(jobs),
            "jobs": jobs,
            "waits": list(self.waits),
            "expected_digest": self.expected_digest,
            "hash_mib": self.config["hash_mib"],
            "hash_rounds": self.config["hash_rounds"],
            "contended_jobs_ge_1ms": contended,
            "wait_total_ms": sum(waits_ms),
            "wait_max_ms": max(waits_ms, default=0.0),
            "wait_p95_ms": waits_ms[min(len(waits_ms) - 1, int(len(waits_ms) * 0.95))] if waits_ms else 0.0,
            "trace_marker_available": self.marker_ok,
            "error": self.error,
        }

    def persist(self, filename: str) -> None:
        if self.config.get("state_dir"):
            write_json(Path(self.config["state_dir"]) / filename, self.status())

    def finish(self) -> dict:
        self.enabled = False
        drain_seconds = self.drain()
        result = self.status()
        result["drain_seconds"] = drain_seconds
        complete = all(job.get("hash_calls") == self.config["hash_rounds"] for job in result["jobs"])
        result["workload_valid"] = bool(self.step and self.jobs and complete and not self.error)
        result["contention_observed"] = result["contended_jobs_ge_1ms"] > 0
        if self.config.get("state_dir"):
            write_json(Path(self.config["state_dir"]) / "worker_stats.json", result)
        self.marker(f"WORKER_END mode={self.config['mode']} steps={self.step} jobs={len(self.jobs)}")
        return result

    def close(self) -> None:
        self.enabled = False
        if self.closed:
            return
        self.closed = True
        try:
            self.drain()
        finally:
            self.cancel.set()
            for job in self.jobs:
                job["go"].set()
            if self.thread.is_alive():
                try:
                    self.jobs_queue.put_nowait(None)
                except queue.Full:
                    pass
                self.thread.join(timeout=self.config["timeout_seconds"])
            if self.thread.is_alive():
                raise RuntimeError("helper thread did not stop")


class LockLabWorkerExtension:
    """String RPC controls avoid sending executable pickles to EngineCore.

    vLLM adds this class as a worker base; it does NOT call a separate __init__.
    """

    # vLLM's mixin lifecycle requires initialization through the install RPC.
    # pylint: disable=attribute-defined-outside-init

    def _mi_lock_lab_install(self, config: dict) -> dict:
        from torch.autograd.profiler import record_function

        if hasattr(self, "_mi_lock_lab_state"):
            raise RuntimeError("lock lab is already installed")
        runner = getattr(self, "model_runner", None)
        original = getattr(runner, "execute_model", None)
        if not callable(original) or inspect.iscoroutinefunction(original):
            raise RuntimeError("expected synchronous worker.model_runner.execute_model; unsupported vLLM version")
        lab = LockContentionLab(config, annotate=record_function)
        self._mi_lock_lab_state = lab
        self._mi_lock_lab_runner = runner
        self._mi_lock_lab_original = original
        self._mi_lock_lab_had_override = "execute_model" in vars(runner)
        runner.execute_model = lab.wrap(original)
        result = lab.status()
        result["runner_class"] = f"{type(runner).__module__}.{type(runner).__qualname__}"
        return result

    def _mi_lock_lab_status(self) -> dict:
        return self._mi_lock_lab_state.status()

    def _mi_lock_lab_start(self) -> dict:
        return self._mi_lock_lab_state.start()

    def _mi_lock_lab_finish(self) -> dict:
        return self._mi_lock_lab_state.finish()

    def _mi_lock_lab_uninstall(self) -> dict:
        lab = getattr(self, "_mi_lock_lab_state", None)
        if lab is None:
            return {"installed": False}
        if self._mi_lock_lab_had_override:
            self._mi_lock_lab_runner.execute_model = self._mi_lock_lab_original
        else:
            del self._mi_lock_lab_runner.execute_model
        try:
            lab.close()
        finally:
            del self._mi_lock_lab_state
        return {"installed": False}


def rpc_one(llm, method: str, *args) -> dict:
    results = llm.collective_rpc(method, args=args)
    if not isinstance(results, list) or len(results) != 1 or not isinstance(results[0], dict):
        raise RuntimeError(f"{method}: expected one local worker, got {results!r}")
    return results[0]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__.rsplit("\n\n", maxsplit=1)[-1])
    parser.add_argument("--mode", choices=("fault", "fixed"))
    parser.add_argument("--model", default="Qwen/Qwen3-0.6B")
    parser.add_argument("--workload-cpus", help="CPU-list inherited by the vLLM process tree, e.g. 120-127")
    parser.add_argument("--helper-cpus", help="Disjoint helper CPU-list, e.g. 128")
    parser.add_argument("--max-model-len", type=int, default=1024)
    parser.add_argument("--gpu-memory-utilization", type=float, default=0.2)
    parser.add_argument("--max-num-batched-tokens", type=int, default=1024)
    parser.add_argument("--max-tokens", type=int, default=128)
    parser.add_argument("--batch-size", type=int, default=2)
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--warmup-tokens", type=int, default=8)
    parser.add_argument("--request-seed", type=int, default=2026)
    parser.add_argument("--inject-every", type=int, default=32, help="Trigger one job every N execute_model calls")
    parser.add_argument("--hash-mib", type=int, default=8, help="Immutable background buffer size in MiB")
    parser.add_argument(
        "--hash-rounds", type=int, default=32, help="Number of SHA-256 passes per job, identical in both modes"
    )
    parser.add_argument("--timeout-seconds", type=float, default=60, help="Helper startup/handshake/lock/drain timeout")
    parser.add_argument(
        "--arm-seconds", type=int, default=30, help="Countdown after ARMED (and optional Enter confirmation)"
    )
    parser.add_argument("--wait-for-ftrace", action="store_true", help="Wait at ARMED for Enter before the countdown")
    parser.add_argument("--profile-dir", help="New/empty profiler output directory; default profiling_lock_<mode>")
    parser.add_argument("--state-dir", help="New/empty metadata directory; default lock_state_<mode>")
    return parser


def prepare_paths(args, parser) -> tuple[Path, Path]:
    profile_dir = Path(args.profile_dir or f"profiling_lock_{args.mode}").resolve()
    state_dir = Path(args.state_dir or f"lock_state_{args.mode}").resolve()
    if profile_dir == state_dir or profile_dir in state_dir.parents or state_dir in profile_dir.parents:
        parser.error("profile/state directories must be separate, non-nested directories")
    for path in (profile_dir, state_dir):
        if path.exists() and (not path.is_dir() or any(path.iterdir())):
            parser.error(f"output directory must be new or empty: {path}")
    for path in (profile_dir, state_dir):
        path.mkdir(parents=True, exist_ok=True)
    return profile_dir, state_dir


def validate_args(args, parser) -> tuple[list[int], list[int]]:
    if sys.platform != "linux":
        parser.error("the vLLM lab requires Linux")
    if not args.mode or not args.workload_cpus or not args.helper_cpus:
        parser.error("--mode, --workload-cpus and --helper-cpus are required")
    for name in (
        "max_model_len",
        "max_num_batched_tokens",
        "max_tokens",
        "batch_size",
        "rounds",
        "warmup_tokens",
        "inject_every",
        "hash_mib",
        "hash_rounds",
        "timeout_seconds",
    ):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be positive")
    if args.arm_seconds < 0 or args.request_seed < 0:
        parser.error("--arm-seconds and --request-seed must be non-negative")
    if not math.isfinite(args.timeout_seconds):
        parser.error("--timeout-seconds must be finite")
    if not math.isfinite(args.gpu_memory_utilization) or not 0 < args.gpu_memory_utilization <= 1:
        parser.error("--gpu-memory-utilization must be finite and in (0, 1]")
    if args.max_tokens >= args.max_model_len:
        parser.error("--max-model-len must exceed --max-tokens and leave room for the prompt")
    if args.hash_mib > 1024:
        parser.error("--hash-mib must be <= 1024; increase --hash-rounds for more work")
    try:
        workload = parse_cpu_list(args.workload_cpus)
        helper = parse_cpu_list(args.helper_cpus)
    except ValueError as error:
        parser.error(str(error))
    allowed = set(os.sched_getaffinity(0))  # pylint: disable=no-member
    if set(workload) & set(helper):
        parser.error("workload/helper CPUs must be disjoint in BOTH modes")
    if not (set(workload) | set(helper)).issubset(allowed):
        parser.error(f"requested CPUs are outside startup affinity/cpuset; allowed={sorted(allowed)}")
    return workload, helper


def package_versions() -> dict:
    versions = {"python": sys.version, "kernel": platform.release()}
    for package in ("vllm", "vllm-ascend", "torch", "torch-npu"):
        try:
            versions[package] = importlib.metadata.version(package)
        except importlib.metadata.PackageNotFoundError:
            versions[package] = "not found"
    return versions


def wait_for_capture(args, metadata: dict, state_dir: Path) -> None:
    worker = metadata["worker_probe"]
    metadata["armed"] = announce(
        "ARMED",
        f"Worker PID={worker['pid']} dispatch TID={worker['dispatch_tids']} helper TID={worker['helper_tid']}. "
        "START FTRACE NOW in the other terminal; confirm the recorder is running and tracefs events are enabled.",
    )
    write_json(state_dir / "metadata.json", metadata)
    if args.wait_for_ftrace:
        try:
            input("[ARMED] ftrace 已开始采集后，在此按 Enter；随后倒计时开始故障实验：")
        except EOFError as error:
            raise RuntimeError("--wait-for-ftrace needs interactive stdin; keep the terminal open") from error
    for remaining in range(args.arm_seconds, 0, -1):
        if remaining <= 5 or remaining % 5 == 0:
            announce("COUNTDOWN", f"Profiling/injection starts in {remaining}s")
        time.sleep(1)


def run_lab(args, parser) -> int:
    workload, helper = validate_args(args, parser)
    profile_dir, state_dir = prepare_paths(args, parser)
    # The module must be importable by the local spawned worker extension loader.
    module_dir = str(Path(__file__).resolve().parent)
    sys.path.insert(0, module_dir)
    os.environ["PYTHONPATH"] = os.pathsep.join(filter(None, (module_dir, os.environ.get("PYTHONPATH", ""))))
    os.sched_setaffinity(0, workload)  # pylint: disable=no-member
    metadata = {
        "config": vars(args),
        "versions": package_versions(),
        "root_pid": os.getpid(),
        "created": stamp(),
        "workload_cpus": workload,
        "helper_cpus": helper,
        "capture_cpus": sorted(set(workload) | set(helper)),
        "profile_dir": str(profile_dir),
        "state_dir": str(state_dir),
        "round_results": [],
        "status": "initializing",
        "execution_mode": "single-card eager; worker.model_runner.execute_model",
    }
    write_json(state_dir / "metadata.json", metadata)
    llm = None
    installed = False
    profiling = False
    cleanup_errors = []
    try:
        from vllm import LLM, SamplingParams

        announce("SETUP", f"mode={args.mode}; workload={workload}; helper={helper}; loading {args.model}")
        llm = LLM(
            model=args.model,
            max_model_len=args.max_model_len,
            gpu_memory_utilization=args.gpu_memory_utilization,
            max_num_seqs=args.batch_size,
            max_num_batched_tokens=args.max_num_batched_tokens,
            tensor_parallel_size=1,
            enforce_eager=True,
            seed=args.request_seed,
            # Preserve the explicit disjoint workload/helper CPU placement.
            additional_config={"enable_cpu_binding": False},
            profiler_config={
                "profiler": "torch",
                "torch_profiler_dir": str(profile_dir),
                "torch_profiler_with_stack": False,
            },
            worker_extension_cls="vllm_lock_fault_lab.LockLabWorkerExtension",
        )
        if not callable(getattr(llm, "collective_rpc", None)):
            raise RuntimeError("this vLLM version has no LLM.collective_rpc")
        prompts = [
            f"Explain why performance measurements should be reproducible. Example {i}:" for i in range(args.batch_size)
        ]
        warmup = SamplingParams(temperature=0.0, min_tokens=args.warmup_tokens, max_tokens=args.warmup_tokens)
        announce("WARMUP", "Compiling/warming up outside capture; do not start ftrace yet.")
        llm.generate(prompts, warmup, use_tqdm=False)
        config = {
            "mode": args.mode,
            "helper_cpus": helper,
            "workload_cpus": workload,
            "hash_mib": args.hash_mib,
            "hash_rounds": args.hash_rounds,
            "inject_every": args.inject_every,
            "timeout_seconds": args.timeout_seconds,
            "state_dir": str(state_dir),
        }
        installed = True
        metadata["worker_install"] = rpc_one(llm, "_mi_lock_lab_install", config)
        # Probe the real model execution path without enabling any fault.
        llm.generate(prompts, warmup, use_tqdm=False)
        metadata["worker_probe"] = rpc_one(llm, "_mi_lock_lab_status")
        if not metadata["worker_probe"]["probe_calls"]:
            raise RuntimeError("execute_model probe had zero hits; this vLLM execution path is unsupported")
        metadata["status"] = "armed"
        wait_for_capture(args, metadata, state_dir)
        sampling = SamplingParams(
            temperature=0.0, seed=args.request_seed, min_tokens=args.max_tokens, max_tokens=args.max_tokens
        )
        llm.start_profile()
        profiling = True
        rpc_one(llm, "_mi_lock_lab_start")
        metadata["profile_begin"] = announce("PROFILE_BEGIN", f"mode={args.mode}; lock injection is now enabled")
        trace_marker(f"PROFILE_BEGIN mode={args.mode}")
        metadata["status"] = "capturing"
        write_json(state_dir / "metadata.json", metadata)
        for round_index in range(args.rounds):
            begin = time.perf_counter()
            outputs = llm.generate(prompts, sampling, use_tqdm=False)
            elapsed = time.perf_counter() - begin
            counts = [len(output.outputs[0].token_ids) for output in outputs if len(output.outputs) == 1]
            valid = len(counts) == args.batch_size and all(count == args.max_tokens for count in counts)
            metadata["round_results"].append(
                {"round": round_index, "seconds": elapsed, "output_token_counts": counts, "length_valid": valid}
            )
            announce("ROUND", f"{round_index + 1}/{args.rounds}: generate={elapsed:.3f}s tokens={counts}")
            write_json(state_dir / "metadata.json", metadata)
            if not valid:
                raise RuntimeError("generated token counts differ from the requested fixed workload")
        stats = rpc_one(llm, "_mi_lock_lab_finish")
        metadata["worker_result"] = stats
        trace_marker(f"PROFILE_END mode={args.mode}")
        metadata["profile_end"] = announce(
            "PROFILE_END",
            "Measured work/drain finished; stop ftrace with Ctrl+C in the other terminal. Exporting Profiling.",
        )
        # Preserve the measurement boundary before the potentially slow export.
        write_json(state_dir / "metadata.json", metadata)
        llm.stop_profile()
        profiling = False
        metadata["profiling_session_dirs"] = [str(path) for path in profile_dir.rglob("*_ascend_pt") if path.is_dir()]
        if not any(path.is_file() for path in profile_dir.rglob("*")):
            raise RuntimeError(
                "Profiling directory is empty after stop_profile; check this vLLM/vllm-ascend profiler configuration"
            )
        if not stats["workload_valid"]:
            raise RuntimeError("no injected jobs or incomplete work; decrease --inject-every or increase --max-tokens")
        if args.mode == "fault" and not stats["contention_observed"]:
            raise RuntimeError("no >=1ms lock/hash overlap; increase --hash-rounds in BOTH modes and recapture")
        metadata["status"] = "complete"
        announce(
            "RESULT",
            f"steps={stats['execute_calls']} jobs={stats['job_count']} "
            f"lock_wait_total={stats['wait_total_ms']:.3f}ms max={stats['wait_max_ms']:.3f}ms",
        )
        return 0
    except BaseException as error:
        metadata["status"] = "failed"
        metadata["error"] = repr(error)
        raise
    finally:
        active_error = sys.exc_info()[0] is not None
        if installed:
            try:
                rpc_one(llm, "_mi_lock_lab_uninstall")
            except Exception as error:
                cleanup_errors.append(f"uninstall: {error!r}")
        if profiling:
            try:
                llm.stop_profile()
            except Exception as error:
                cleanup_errors.append(f"stop_profile: {error!r}")
        metadata["cleanup_errors"] = cleanup_errors
        if cleanup_errors:
            metadata["status"] = "failed"
        metadata["capture_done"] = announce(
            "CAPTURE_DONE", f"status={metadata['status']}; stop ftrace if still running. Metadata: {state_dir}"
        )
        write_json(state_dir / "metadata.json", metadata)
        if cleanup_errors and not active_error:
            raise RuntimeError("; ".join(cleanup_errors))


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    def on_terminate(_signum, _frame):
        raise KeyboardInterrupt("received SIGTERM")

    signal.signal(signal.SIGTERM, on_terminate)
    try:
        return run_lab(args, parser)
    except KeyboardInterrupt:
        print("[STOPPED] Interrupted; inspect metadata before using this capture.", file=sys.stderr)
        return 130
    except Exception as error:
        print(f"[ERROR] {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
