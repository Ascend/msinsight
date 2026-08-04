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
"""

# The two standalone lab scripts intentionally share small Linux utility helpers.
# pylint: disable=duplicate-code

from __future__ import annotations

import argparse
import ctypes
import json
import multiprocessing as mp
import os
from pathlib import Path
import queue
import signal
import sys
import time
from typing import Iterable


TRACE_MARKER_CANDIDATES = (
    "/sys/kernel/tracing/trace_marker",
    "/sys/kernel/debug/tracing/trace_marker",
)


def parse_cpu_list(value: str) -> list[int]:
    """Parse Linux CPU-list syntax such as '0-3,8,10-11'."""
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


def set_process_name(name: str) -> None:
    """Set Linux comm so noise workers are easy to identify in ftrace."""
    try:
        libc = ctypes.CDLL(None, use_errno=True)
        encoded = name.encode("utf-8")[:15]
        libc.prctl(15, ctypes.c_char_p(encoded), 0, 0, 0)  # PR_SET_NAME
    except (AttributeError, OSError) as error:
        print(f"[warning] unable to set process name: {error}", file=sys.stderr)


def arm_parent_death_signal(expected_parent_pid: int) -> None:
    """Ensure a CPU-noise worker dies if the controlling process is SIGKILLed."""
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(1, signal.SIGTERM, 0, 0, 0) != 0:  # PR_SET_PDEATHSIG
        error_number = ctypes.get_errno()
        raise OSError(error_number, os.strerror(error_number))
    if os.getppid() != expected_parent_pid:
        raise RuntimeError("CPU-noise worker parent exited before PR_SET_PDEATHSIG was armed")


def write_trace_marker(message: str) -> bool:
    payload = f"HOST_BOUND_LAB|{message}\n"
    for marker_path in TRACE_MARKER_CANDIDATES:
        try:
            with open(marker_path, "w", encoding="utf-8") as marker:
                marker.write(payload)
            return True
        except OSError:
            continue
    return False


def read_children(pid: int) -> set[int]:
    children: set[int] = set()
    task_dir = Path(f"/proc/{pid}/task")
    try:
        tids = list(task_dir.iterdir())
    except OSError:
        return children
    for tid_dir in tids:
        try:
            text = (tid_dir / "children").read_text(encoding="utf-8").strip()
        except OSError:
            continue
        for item in text.split():
            try:
                children.add(int(item))
            except ValueError:
                pass
    return children


def collect_process_tree(root_pid: int) -> list[int]:
    found: set[int] = set()
    pending = [root_pid]
    while pending:
        pid = pending.pop()
        if pid in found or not Path(f"/proc/{pid}").exists():
            continue
        found.add(pid)
        pending.extend(read_children(pid) - found)
    return sorted(found)


def collect_tids(pids: Iterable[int]) -> list[int]:
    tids: set[int] = set()
    for pid in pids:
        try:
            tids.update(int(entry.name) for entry in Path(f"/proc/{pid}/task").iterdir())
        except OSError:
            continue
    return sorted(tids)


def read_comm(pid: int) -> str:
    try:
        return Path(f"/proc/{pid}/comm").read_text(encoding="utf-8").strip()
    except OSError:
        return "<exited>"


def write_capture_metadata(
    state_dir: Path,
    mode: str,
    workload_cpus: list[int],
    hog_cpus: list[int],
    hog_pids: list[int],
    profile_dir: Path,
    generation_metadata: dict[str, object],
) -> tuple[list[int], list[int], dict[str, object]]:
    state_dir.mkdir(parents=True, exist_ok=True)
    process_pids = collect_process_tree(os.getpid())
    tids = collect_tids(process_pids)
    metadata = {
        "mode": mode,
        "created_at_unix_ns": time.time_ns(),
        "root_pid": os.getpid(),
        "workload_cpus": workload_cpus,
        "workload_cpu_list": format_cpu_list(workload_cpus),
        "hog_cpus": hog_cpus,
        "hog_cpu_list": format_cpu_list(hog_cpus),
        "hog_pids": hog_pids,
        "processes": [{"pid": pid, "comm": read_comm(pid), "is_hog": pid in hog_pids} for pid in process_pids],
        "tids": tids,
        "profile_dir": str(profile_dir),
        "generation": generation_metadata,
    }
    (state_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    (state_dir / "capture_pids.txt").write_text(
        "\n".join(str(pid) for pid in process_pids) + "\n",
        encoding="utf-8",
    )
    (state_dir / "capture_tids.txt").write_text(
        "\n".join(str(tid) for tid in tids) + "\n",
        encoding="utf-8",
    )
    return process_pids, tids, metadata


def write_metadata(state_dir: Path, metadata: dict[str, object]) -> None:
    (state_dir / "metadata.json").write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


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


def cpu_hog_worker(
    index: int,
    parent_pid: int,
    cpus: list[int],
    start_event: mp.synchronize.Event,
    stop_event: mp.synchronize.Event,
    origin_ns: mp.sharedctypes.Synchronized,
    ready_queue: mp.queues.Queue,
    period_ms: int,
    duty_ms: int,
) -> None:
    try:
        arm_parent_death_signal(parent_pid)
        set_process_name(f"hb_hog{index:02d}")
        os.sched_setaffinity(0, set(cpus))  # pylint: disable=no-member
        ready_queue.put((os.getpid(), None))
    except Exception as error:
        ready_queue.put((os.getpid(), repr(error)))
        return

    while not stop_event.is_set() and not start_event.wait(timeout=0.1):
        pass
    if stop_event.is_set():
        return

    period_ns = period_ms * 1_000_000
    duty_ns = duty_ms * 1_000_000
    state = (0x9E3779B97F4A7C15 ^ os.getpid()) & 0xFFFFFFFFFFFFFFFF

    while not stop_event.is_set():
        phase_ns = (time.monotonic_ns() - origin_ns.value) % period_ns
        if phase_ns < duty_ns:
            busy_deadline = time.monotonic_ns() + min(duty_ns - phase_ns, 2_000_000)
            while time.monotonic_ns() < busy_deadline:
                # Integer arithmetic keeps the process runnable without allocating memory.
                state ^= (state << 13) & 0xFFFFFFFFFFFFFFFF
                state ^= state >> 7
                state ^= (state << 17) & 0xFFFFFFFFFFFFFFFF
        else:
            sleep_seconds = min((period_ns - phase_ns) / 1e9, 0.01)
            stop_event.wait(timeout=max(sleep_seconds, 0.0005))


def wait_for_hogs(
    processes: list[mp.Process],
    ready_queue: mp.queues.Queue,
    timeout_seconds: float = 30.0,
) -> list[int]:
    ready: list[int] = []
    deadline = time.monotonic() + timeout_seconds
    for _ in processes:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError("timed out while starting CPU-noise processes")
        try:
            pid, error = ready_queue.get(timeout=remaining)
        except queue.Empty as exc:
            raise RuntimeError("timed out while starting CPU-noise processes") from exc
        if error:
            raise RuntimeError(f"CPU-noise process {pid} failed: {error}")
        ready.append(pid)
    return sorted(ready)


def stop_hogs(
    processes: list[mp.Process],
    start_event: mp.synchronize.Event,
    stop_event: mp.synchronize.Event,
) -> None:
    errors: list[str] = []
    try:
        stop_event.set()
        start_event.set()
    except Exception as error:  # pragma: no cover - multiprocessing backend failure
        errors.append(f"set stop events: {error}")
    for process in processes:
        try:
            process.join(timeout=2.0)
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            errors.append(f"join {process.name}: {error}")
    for process in processes:
        try:
            if process.is_alive():
                process.terminate()
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            errors.append(f"terminate {process.name}: {error}")
    for process in processes:
        try:
            process.join(timeout=2.0)
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            errors.append(f"final join {process.name}: {error}")
    for process in processes:
        try:
            if process.is_alive():
                errors.append(f"{process.name} remained alive after terminate")
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            errors.append(f"verify {process.name} exit: {error}")
    if errors:
        raise RuntimeError("CPU-noise process cleanup failed: " + "; ".join(errors))


def make_prompts(batch_size: int) -> list[str]:
    seeds = (
        "Hello, my name is",
        "The future of AI is",
        "A reliable distributed system should",
        "The most important lesson from debugging is",
        "In a quiet city at midnight",
        "To explain kernel scheduling simply",
        "A good performance experiment must",
        "The next generation of computers will",
    )
    return [f"{seeds[index % len(seeds)]} (request {index})" for index in range(batch_size)]


def countdown(seconds: int) -> None:
    if seconds <= 0:
        return
    print(f"[ARMED] Start ftrace now; workload begins in {seconds} seconds.", flush=True)
    for remaining in range(seconds, 0, -1):
        if remaining <= 5 or remaining % 5 == 0:
            print(f"[ARMED] {remaining:2d}s", flush=True)
        time.sleep(1)


def validate_cpus(
    parser: argparse.ArgumentParser,
    mode: str,
    workload_cpus: list[int],
    hog_cpus: list[int],
    allowed_cpus: set[int],
) -> None:
    unavailable = (set(workload_cpus) | set(hog_cpus)) - allowed_cpus
    if unavailable:
        parser.error(
            "requested CPUs are outside the current cgroup/cpuset: "
            f"{format_cpu_list(unavailable)}; allowed={format_cpu_list(allowed_cpus)}"
        )
    overlap = set(workload_cpus) & set(hog_cpus)
    if mode == "fault" and not overlap:
        parser.error("fault mode requires workload CPUs and hog CPUs to overlap")
    if mode == "fixed" and overlap:
        parser.error(f"fixed mode requires disjoint workload/hog CPUs; overlap={format_cpu_list(overlap)}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Construct and remove a schedulable vLLM Host-Bound fault.")
    parser.add_argument("--mode", choices=("fault", "fixed"), required=True)
    parser.add_argument("--model", default="Qwen/Qwen3-0.6B")
    parser.add_argument("--max-model-len", type=int, default=26240)
    parser.add_argument("--max-tokens", type=int, default=256)
    parser.add_argument("--request-seed", type=int, default=2026, help="Request-level sampling seed used in every run")
    parser.add_argument("--batch-size", type=int, default=16)
    parser.add_argument("--rounds", type=int, default=2)
    parser.add_argument("--tensor-parallel-size", type=int, default=1)
    parser.add_argument(
        "--workload-cpus",
        required=True,
        help="CPUs used by the Python/vLLM process tree, e.g. 120-127",
    )
    parser.add_argument(
        "--hog-cpus",
        required=True,
        help="CPUs used by synthetic interference processes",
    )
    parser.add_argument("--hog-count", type=int, default=32)
    parser.add_argument(
        "--hog-period-ms",
        type=int,
        default=500,
        help="Interference cycle length",
    )
    parser.add_argument(
        "--hog-duty-ms",
        type=int,
        default=400,
        help="Busy part of each interference cycle",
    )
    parser.add_argument(
        "--workload-nice",
        type=int,
        default=5,
        help="Positive nice increment inherited by vLLM; 0 disables amplification",
    )
    parser.add_argument(
        "--arm-seconds",
        type=int,
        default=10,
        help="Delay after initialization/warmup so ftrace can be started",
    )
    parser.add_argument(
        "--warmup-tokens",
        type=int,
        default=8,
        help="Unprofiled warmup tokens; 0 disables warmup",
    )
    parser.add_argument(
        "--profile-dir",
        help="VLLM_TORCH_PROFILER_DIR; default: ./profiling_<mode>",
    )
    parser.add_argument(
        "--state-dir",
        help="PID/TID metadata directory; default: ./host_bound_state_<mode>",
    )
    parser.add_argument(
        "--enforce-eager",
        action="store_true",
        help="Pass enforce_eager=True to vLLM",
    )
    return parser


def run_lab(args: argparse.Namespace, parser: argparse.ArgumentParser) -> int:
    if args.hog_count < 1:
        parser.error("--hog-count must be at least 1")
    if args.hog_period_ms <= 0:
        parser.error("--hog-period-ms must be positive")
    if not 0 < args.hog_duty_ms < args.hog_period_ms:
        parser.error("--hog-duty-ms must be > 0 and < --hog-period-ms")
    if args.batch_size < 1 or args.rounds < 1 or args.max_tokens < 1:
        parser.error("batch size, rounds, and max tokens must be positive")
    if args.request_seed < 0:
        parser.error("--request-seed must be non-negative")
    if not 0 <= args.workload_nice <= 19:
        parser.error("--workload-nice must be in [0, 19]")

    allowed_cpus = set(os.sched_getaffinity(0))  # pylint: disable=no-member
    try:
        workload_cpus = parse_cpu_list(args.workload_cpus)
        hog_cpus = parse_cpu_list(args.hog_cpus)
    except ValueError as error:
        parser.error(str(error))
    validate_cpus(parser, args.mode, workload_cpus, hog_cpus, allowed_cpus)

    profile_dir = Path(args.profile_dir or f"profiling_{args.mode}").resolve()
    state_dir = Path(args.state_dir or f"host_bound_state_{args.mode}").resolve()
    profile_dir.mkdir(parents=True, exist_ok=True)
    state_dir.mkdir(parents=True, exist_ok=True)
    os.environ["VLLM_TORCH_PROFILER_DIR"] = str(profile_dir)

    # The whole vLLM process tree inherits this affinity.
    os.sched_setaffinity(0, set(workload_cpus))  # pylint: disable=no-member
    set_process_name(f"hb_{args.mode}")

    ctx = mp.get_context("spawn")
    start_event = ctx.Event()
    stop_event = ctx.Event()
    origin_ns = ctx.Value("q", 0)
    ready_queue = ctx.Queue()
    parent_pid = os.getpid()
    hog_processes = [
        ctx.Process(
            target=cpu_hog_worker,
            name=f"hb_hog{index:02d}",
            args=(
                index,
                parent_pid,
                hog_cpus,
                start_event,
                stop_event,
                origin_ns,
                ready_queue,
                args.hog_period_ms,
                args.hog_duty_ms,
            ),
        )
        for index in range(args.hog_count)
    ]

    llm = None
    profiling_started = False
    hog_pids: list[int] = []
    outputs = []
    interrupted = False

    def handle_signal(signum: int, _frame: object) -> None:
        nonlocal interrupted
        interrupted = True
        stop_event.set()
        print(f"\n[signal] received {signum}; stopping after the current call", flush=True)

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    try:
        # Start at normal priority. The parent changes its own nice only after
        # the noise workers exist, so vLLM is disadvantaged only in fault mode
        # where both groups share CPUs.
        for process in hog_processes:
            process.start()
        hog_pids = wait_for_hogs(hog_processes, ready_queue)

        if args.workload_nice:
            new_nice = os.nice(args.workload_nice)  # pylint: disable=no-member
            print(f"[setup] workload nice={new_nice}", flush=True)

        # Import after profiler output and process settings have been prepared.
        from vllm import LLM, SamplingParams

        print(
            f"[setup] mode={args.mode} workload_cpus={format_cpu_list(workload_cpus)} "
            f"hog_cpus={format_cpu_list(hog_cpus)} hog_count={args.hog_count}",
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

        prompts = make_prompts(args.batch_size)
        if args.warmup_tokens > 0:
            print("[warmup] compiling/warming up outside the measurement window", flush=True)
            warmup_params = SamplingParams(
                temperature=0.0,
                min_tokens=args.warmup_tokens,
                max_tokens=args.warmup_tokens,
            )
            llm.generate(prompts[: min(2, len(prompts))], warmup_params, use_tqdm=False)

        generation_rounds: list[dict[str, object]] = []
        generation_metadata: dict[str, object] = {
            "model": args.model,
            "request_count": len(prompts),
            "round_count": args.rounds,
            "sampling": {
                "temperature": 0.8,
                "top_p": 0.95,
                "request_seed": args.request_seed,
                "min_tokens": args.max_tokens,
                "max_tokens": args.max_tokens,
            },
            "round_results": generation_rounds,
        }
        process_pids, tids, metadata = write_capture_metadata(
            state_dir,
            args.mode,
            workload_cpus,
            hog_cpus,
            hog_pids,
            profile_dir,
            generation_metadata,
        )
        print(
            f"[metadata] {state_dir} (processes={len(process_pids)}, tids={len(tids)})",
            flush=True,
        )
        marker_ok = write_trace_marker(
            f"ARMED mode={args.mode} workload={format_cpu_list(workload_cpus)} hogs={format_cpu_list(hog_cpus)}"
        )
        if not marker_ok:
            print("[marker] trace_marker is unavailable; continuing without markers", flush=True)

        countdown(args.arm_seconds)
        if interrupted:
            return 130

        sampling_params = SamplingParams(
            temperature=0.8,
            top_p=0.95,
            seed=args.request_seed,
            min_tokens=args.max_tokens,
            max_tokens=args.max_tokens,
        )
        print(f"[profile] output={profile_dir}", flush=True)
        llm.start_profile()
        profiling_started = True
        write_trace_marker(f"PROFILE_BEGIN mode={args.mode}")

        # All noise processes use the same phase origin. The fault therefore
        # creates repeated busy/recovery intervals rather than random slowdown.
        origin_ns.value = time.monotonic_ns()
        start_event.set()
        for round_index in range(args.rounds):
            if interrupted:
                break
            write_trace_marker(f"GENERATE_BEGIN mode={args.mode} round={round_index}")
            begin = time.perf_counter()
            outputs = llm.generate(prompts, sampling_params, use_tqdm=False)
            elapsed = time.perf_counter() - begin
            token_counts = collect_output_token_counts(outputs)
            valid_token_counts = len(token_counts) == len(prompts) and all(
                count == args.max_tokens for count in token_counts
            )
            generation_rounds.append(
                {
                    "round_index": round_index,
                    "elapsed_seconds": elapsed,
                    "output_token_counts": token_counts,
                    "length_valid": valid_token_counts,
                }
            )
            write_trace_marker(
                f"GENERATE_END mode={args.mode} round={round_index} elapsed_s={elapsed:.6f} "
                f"tokens={','.join(str(count) for count in token_counts)}"
            )
            write_metadata(state_dir, metadata)
            print(f"[round {round_index}] generate={elapsed:.3f}s", flush=True)
            if not valid_token_counts:
                raise RuntimeError(
                    "generation workload is invalid: "
                    f"expected {len(prompts)} outputs with {args.max_tokens} tokens each, got {token_counts}"
                )

        return 130 if interrupted else 0
    finally:
        active_exception = sys.exc_info()[0] is not None
        cleanup_error: RuntimeError | None = None
        try:
            stop_event.set()
        except Exception as error:  # pragma: no cover - multiprocessing backend failure
            cleanup_error = RuntimeError(f"unable to signal CPU-noise processes: {error}")
        write_trace_marker(f"PROFILE_END mode={args.mode}")
        if profiling_started and llm is not None:
            try:
                llm.stop_profile()
            except Exception as error:
                print(f"[warning] stop_profile failed: {error!r}", file=sys.stderr)
        try:
            stop_hogs(hog_processes, start_event, stop_event)
        except RuntimeError as error:
            cleanup_error = (
                error
                if cleanup_error is None
                else RuntimeError(f"{cleanup_error}; additional cleanup failure: {error}")
            )
            print(f"[warning] {error}", file=sys.stderr)
        if outputs:
            try:
                print("[sample outputs]", flush=True)
                for output in outputs[:2]:
                    generated_text = output.outputs[0].text
                    print(
                        f"Prompt: {output.prompt!r}, Generated text: {generated_text!r}",
                        flush=True,
                    )
            except Exception as error:  # pragma: no cover - defensive display-only path
                print(f"[warning] unable to print sample outputs: {error!r}", file=sys.stderr)
        if cleanup_error is not None and not active_exception:
            raise cleanup_error


def main() -> int:
    parser = build_parser()
    return run_lab(parser.parse_args(), parser)


if __name__ == "__main__":
    raise SystemExit(main())
