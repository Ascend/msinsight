"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2025 Huawei Technologies Co.,Ltd.

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
import argparse
import logging
import subprocess  # nosec B404
import sys
import time
import signal
import threading
import json
import shutil
from typing import Optional

TRACE_BACKEND_AUTO = "auto"
TRACE_BACKEND_TRACE_CMD = "trace-cmd"
TRACE_BACKEND_DEBUGFS = "debugfs"
DEFAULT_DEBUGFS_TRACE_ROOTS = ("/sys/kernel/tracing", "/sys/kernel/debug/tracing")
TRACE_COPY_CHUNK_SIZE = 1024 * 1024
DEFAULT_TRACE_BUFFER_SIZE = 40960  # 单位: KB(trace-cmd环形缓冲区大小默认值)
DEFAULT_TRACE_RECORD_TIME = 30  # 单位: 秒（trace-cmd采集时长默认值）

# trace-cmd 事件白名单
# cpu调度
SCHED_EVENT_LIST = {
    "sched:sched_switch",
    "sched:sched_wakeup",
    "sched:sched_waking",
    "sched:sched_wakeup_new",
    "sched:sched_migrate_task",
    "sched:sched_stat_runtime",
    "sched:sched_process_fork",
    "sched:sched_process_exec",
    "sched:sched_process_exit",
}
# 中断
IRQ_EVENT_LIST = {
    "irq:irq_handler_entry",
    "irq:irq_handler_exit",
    "irq:softirq_raise",
    "irq:softirq_entry",
    "irq:softirq_exit",
}
# 锁竞争
FUTEX_EVENT_LIST = {
    "syscalls:sys_enter_futex",
    "syscalls:sys_exit_futex",
}


def _is_root_user():
    if not hasattr(os, "getuid"):
        return True
    return os.getuid() == 0  # pylint: disable=no-member


def ftrace_record_start(
    cpu_mask=None,
    output="trace.dat",
    bf_size=DEFAULT_TRACE_BUFFER_SIZE,
    event_cfg: Optional["TraceEventConfig"] = None,
    args=None,
    backend=TRACE_BACKEND_AUTO,
):
    if not _is_root_user():
        logging.critical('Please run this script as root')
        return False
    if args is None and event_cfg is not None and not isinstance(event_cfg, TraceEventConfig):
        args = event_cfg
        event_cfg = None
    original_cpu_mask = cpu_mask
    cpu_mask = CPUParser.normalize_cpu_mask(cpu_mask)
    if original_cpu_mask is not None and cpu_mask is None:
        return False

    if event_cfg is None:
        event_cfg = TraceEventConfig()
    if args is not None:
        event_cfg.sched = args.sched
        event_cfg.irq = args.irq
        event_cfg.futex = args.futex
        backend = getattr(args, "backend", backend)

    try:
        TraceRecord.configure_backend(backend)
        TraceRecord.trace_clear()
        final_output = TraceRecord.trace_start(cpu_mask, output, event_cfg=event_cfg, buffer_size=bf_size)
        logging.info("Started ftrace recording via %s backend, output=%s", TraceRecord.get_backend_name(), final_output)
        return True
    except Exception as exc:
        logging.exception("Failed to start ftrace recording: %s", exc)
        return False


def _check_trace_data_warning(output_path: str) -> None:
    """检查采集输出文件是否包含有效数据，若无则打印告警。

    Args:
        output_path: 输出文件路径
    """
    hint = (
        "The collection time may have been too short to capture any data. "
        "Consider increasing --record_time, or reducing --bf_size "
        "(to shorten trace-cmd warmup time, provided no ring buffer data loss occurs)."
    )

    if output_path is None or not os.path.exists(output_path):
        logging.warning("No trace data collected: output file does not exist. %s", hint)
        return

    try:
        file_size = os.path.getsize(output_path)
    except OSError as exc:
        logging.warning("Failed to check trace data in %s: %s", output_path, exc)
        return

    if file_size == 0:
        logging.warning("No trace data collected: trace file is empty (0 bytes). %s", hint)


def ftrace_record_stop(output=None):
    logging.info("Ending record, cleaning up...")
    final_output = None
    try:
        final_output = TraceRecord.trace_stop(output)
    except Exception:
        logging.exception("Failed to stop ftrace recording")
        try:
            TraceRecord.trace_reset()
        except Exception as reset_exc:
            logging.exception("Failed to reset ftrace state after stop failure: %s", reset_exc)
        logging.warning("Trace buffer was not cleared because trace data may not have been saved.")
        raise
    else:
        TraceRecord.log_tracefs_stats()
        TraceRecord.trace_clear()
        TraceRecord.trace_reset(log_stats=False)
        if final_output is not None:
            logging.info("Trace data saved to %s", final_output)
        logging.info("Cleanup finished")
    _check_trace_data_warning(final_output or output)
    return final_output


def on_exit(signum=None, frame=None):
    exit_code = 0
    try:
        ftrace_record_stop()
    except Exception as exc:
        logging.exception("Failed to stop ftrace recording on signal %s: %s", signum, exc)
        exit_code = 1
    raise SystemExit(exit_code)


class TraceEventConfig:
    def __init__(self, sched=1, irq=1, futex=0):
        self.sched = sched
        self.irq = irq
        self.futex = futex


class CPUParser:
    """CPU参数解析器，支持字符串和列表格式的CPU掩码"""

    @staticmethod
    def parse_cpu_arg(cpu_str):
        cpus = set()
        if not cpu_str:
            return None

        format_hint = "Please use non-negative integers (e.g., '0-3,5'). Negative values are not supported."

        try:
            parts = cpu_str.split(',')
            for part in parts:
                part = part.strip()
                if not part:
                    continue

                if '-' in part:
                    sub_parts = part.split('-')
                    # 是两个部分，且两个部分都是纯数字
                    if len(sub_parts) != 2 or not (sub_parts[0].isdigit() and sub_parts[1].isdigit()):
                        logging.error("Invalid range format or negative value detected: '%s'. %s", part, format_hint)
                        return None

                    start, end = int(sub_parts[0]), int(sub_parts[1])
                    # 避免前大后小
                    if start > end:
                        logging.error("Range start must be not greater than end: '%s'.", part)
                        return None
                    cpus.update(range(start, end + 1))
                else:
                    # 检查是否为数字
                    if not part.isdigit():
                        logging.error("Invalid CPU ID detected: '%s'. %s", part, format_hint)
                        return None
                    cpus.add(int(part))
        except Exception:
            logging.error("Unexpected error parsing CPU arguments. %s", format_hint)
            return None

        return sorted(list(cpus))

    @staticmethod
    def normalize_cpu_mask(cpu_mask):
        format_hint = "Hint: CPU mask should be a string (e.g., '0-3,5') or a list of integers (e.g., [0, 1, 2])."

        # 合法：表示不绑定 CPU
        if cpu_mask is None:
            return None

        # 处理字符串输入
        if isinstance(cpu_mask, str):
            logging.info("Received cpu mask string input: '%s', parsing...", cpu_mask)
            parsed_mask = CPUParser.parse_cpu_arg(cpu_mask)
            if parsed_mask is None:
                # 在内部已有详细错误信息，直接返回None
                return None
            return parsed_mask

        # 处理列表输入
        elif isinstance(cpu_mask, list):
            invalid_elements = [c for c in cpu_mask if not (isinstance(c, int) and c >= 0)]
            if invalid_elements:
                logging.error("Invalid CPU ID(s) detected in list: %s. %s", invalid_elements, format_hint)
                return None
            return sorted(list(set(cpu_mask)))

        # 非法类型
        elif cpu_mask is not None:
            logging.error("Unsupported CPU mask type. %s", format_hint)
            return None
        return None

    @staticmethod
    def cpus_to_cpumask(cpus):
        # 确保输入合法
        for cpu in cpus:
            if not isinstance(cpu, int) or cpu < 0:
                raise ValueError(f"Invalid CPU ID: {cpu}")

        # 构建位掩码（支持任意大小）
        mask = 0
        for cpu in cpus:
            mask |= 1 << cpu
        if mask == 0:
            return "0"
        # 每 32 位一组，生成掩码字符串
        parts = []
        while mask:
            # 取低 32 位
            part = mask & 0xFFFFFFFF
            parts.append(f"{part:08x}")  # 8位十六进制，左补0
            mask >>= 32
        # 如果为空，说明 mask 为 0
        if not parts:
            return "0"
        # 反转顺序（低位在右，高位在左），用逗号连接
        cpumask_str = ",".join(reversed(parts))
        return cpumask_str

    @staticmethod
    def get_online_cpus():
        try:
            with open("/sys/devices/system/cpu/online", "r", encoding="utf-8") as file:
                cpu_range = file.read().strip()
        except OSError as exc:
            logging.warning("Failed to read online CPU list: %s", exc)
            return None
        return CPUParser.parse_cpu_arg(cpu_range)


class TraceRecord:
    _INT_RETRY_COUNT = 5
    _INT_INTERVAL_SEC = 1
    _WAIT_TIMEOUT_SEC = 20
    # Holds the running trace-cmd record subprocess (if any)
    _record_process = None
    _backend_name = None
    _trace_root = None
    _output_path = None

    @staticmethod
    def configure_backend(backend_name=TRACE_BACKEND_AUTO):
        if backend_name == TRACE_BACKEND_AUTO:
            if TraceRecord._is_trace_cmd_available():
                backend_name = TRACE_BACKEND_TRACE_CMD
            else:
                backend_name = TRACE_BACKEND_DEBUGFS
                logging.info("trace-cmd is unavailable, falling back to debugfs backend")

        if backend_name == TRACE_BACKEND_TRACE_CMD:
            if not TraceRecord._is_trace_cmd_available():
                raise FileNotFoundError("trace-cmd was not found in PATH or /usr/bin/trace-cmd")
            TraceRecord._backend_name = backend_name
            TraceRecord._trace_root = None
            return backend_name

        if backend_name == TRACE_BACKEND_DEBUGFS:
            resolved_root = TraceRecord._trace_root or TraceRecord.detect_tracing_root()
            if not resolved_root:
                raise FileNotFoundError(
                    "Tracing root was not found under /sys/kernel/tracing or /sys/kernel/debug/tracing"
                )
            TraceRecord._backend_name = backend_name
            TraceRecord._trace_root = resolved_root
            return backend_name

        raise ValueError(f"Unsupported trace backend: {backend_name}")

    @staticmethod
    def get_backend_name():
        TraceRecord._ensure_backend()
        return TraceRecord._backend_name

    @staticmethod
    def detect_tracing_root():
        for trace_root in DEFAULT_DEBUGFS_TRACE_ROOTS:
            if os.path.isdir(trace_root) and all(
                os.path.exists(os.path.join(trace_root, name)) for name in ("trace", "tracing_on")
            ):
                return trace_root
        return None

    @staticmethod
    def _ensure_backend():
        if TraceRecord._backend_name is None:
            TraceRecord.configure_backend()
        return TraceRecord._backend_name

    @staticmethod
    def _get_trace_cmd_path():
        return shutil.which("trace-cmd") or "/usr/bin/trace-cmd"

    @staticmethod
    def _is_trace_cmd_available():
        trace_cmd_path = shutil.which("trace-cmd")
        if trace_cmd_path:
            return True
        return os.path.isfile("/usr/bin/trace-cmd") and os.access("/usr/bin/trace-cmd", os.X_OK)

    @staticmethod
    def _get_final_output_path(output=None):
        if output is None:
            output = TraceRecord._output_path
        if output is None:
            output = "trace.txt" if TraceRecord._ensure_backend() == TRACE_BACKEND_DEBUGFS else "trace.dat"

        if TraceRecord._ensure_backend() == TRACE_BACKEND_DEBUGFS and output.lower().endswith(".dat"):
            normalized_output = output[:-4] + ".txt"
            logging.warning(
                "debugfs backend produces text trace data, output path is adjusted from %s to %s. "
                "Use the adjusted file as trace_convert.py input.",
                output,
                normalized_output,
            )
            output = normalized_output
        return output

    @staticmethod
    def _tracefs_path(relative_path):
        TraceRecord._ensure_backend()
        if TraceRecord._trace_root is None:
            raise RuntimeError("Trace root is not configured for debugfs backend")
        return os.path.join(TraceRecord._trace_root, relative_path)

    @staticmethod
    def _write_tracefs_text(relative_path, content):
        path = TraceRecord._tracefs_path(relative_path)
        with open(path, "w", encoding="utf-8") as file:
            file.write(content)

    @staticmethod
    def _list_tracefs_per_cpu_ids():
        per_cpu_root = TraceRecord._tracefs_path("per_cpu")
        if not os.path.isdir(per_cpu_root):
            return None

        cpus = []
        for cpu_dir in os.listdir(per_cpu_root):
            if cpu_dir.startswith("cpu") and cpu_dir[3:].isdigit():
                cpus.append(int(cpu_dir[3:]))
        return sorted(cpus)

    @staticmethod
    def _resolve_tracefs_cpus(cpu_mask):
        if cpu_mask is not None:
            return cpu_mask

        cpu_mask = CPUParser.get_online_cpus()
        if cpu_mask is not None:
            return cpu_mask

        cpu_mask = TraceRecord._list_tracefs_per_cpu_ids()
        if cpu_mask:
            logging.warning("Failed to read online CPU list, use tracefs per-cpu directories instead")
            return cpu_mask
        return None

    @staticmethod
    def _copy_tracefs_trace(output):
        """Copy the stopped tracefs snapshot into the final text output file."""
        path = TraceRecord._tracefs_path("trace")
        os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
        with open(path, "rb") as src_file, open(output, "wb") as dst_file:
            while True:
                chunk = src_file.read(TRACE_COPY_CHUNK_SIZE)
                if not chunk:
                    break
                dst_file.write(chunk)
        logging.info(
            "debugfs trace copy finished: output=%s",
            output,
        )

    @staticmethod
    def _parse_tracefs_stats(content):
        stats = {}
        for line in content.splitlines():
            if ":" not in line:
                continue
            key, value = line.split(":", 1)
            stats[key.strip()] = value.strip()
        return stats

    @staticmethod
    def _stat_value_to_int(value):
        if value is None:
            return 0
        first_token = value.strip().split()[0] if value.strip() else "0"
        try:
            return int(first_token)
        except ValueError:
            return 0

    @staticmethod
    def log_tracefs_stats():
        if TraceRecord._ensure_backend() != TRACE_BACKEND_DEBUGFS:
            return

        trace_root = TraceRecord._trace_root
        if not trace_root:
            logging.warning("tracefs root is unavailable, skip tracefs stats check")
            return

        per_cpu_root = os.path.join(trace_root, "per_cpu")
        if not os.path.isdir(per_cpu_root):
            logging.warning("tracefs per-cpu stats are unavailable: %s", per_cpu_root)
            return

        loss_keys = ("overrun", "commit overrun", "dropped events")
        found_stats = False
        total_loss = 0

        for cpu_dir in sorted(os.listdir(per_cpu_root)):
            if not (cpu_dir.startswith("cpu") and cpu_dir[3:].isdigit()):
                continue
            stats_path = os.path.join(per_cpu_root, cpu_dir, "stats")
            if not os.path.isfile(stats_path):
                continue

            try:
                with open(stats_path, "r", encoding="utf-8", errors="replace") as stats_file:
                    stats = TraceRecord._parse_tracefs_stats(stats_file.read())
            except OSError as exc:
                logging.warning("Failed to read tracefs stats from %s: %s", stats_path, exc)
                continue

            found_stats = True
            total_loss += sum(TraceRecord._stat_value_to_int(stats.get(key)) for key in loss_keys)

        if not found_stats:
            logging.warning("No tracefs per-cpu stats files found under %s", per_cpu_root)
        elif total_loss > 0:
            logging.warning(
                "tracefs stats report lost/overwritten events before reset: total_loss_counters=%d. Consider increasing --bf-size",
                total_loss,
            )

    @staticmethod
    def _set_tracefs_events(event_cfg):
        TraceRecord._write_tracefs_text("events/enable", "0")
        TraceRecord._write_tracefs_text("set_event", "")

        enabled_events = []
        if event_cfg.sched:
            enabled_events.extend(SCHED_EVENT_LIST)
        if event_cfg.irq:
            enabled_events.extend(IRQ_EVENT_LIST)
        if event_cfg.futex:
            enabled_events.extend(FUTEX_EVENT_LIST)

        for event_name in enabled_events:
            category, event = event_name.split(":", 1)
            event_enable_path = os.path.join("events", category, event, "enable")
            if not os.path.exists(TraceRecord._tracefs_path(event_enable_path)):
                logging.warning("Skip unsupported ftrace event: %s", event_name)
                continue
            TraceRecord._write_tracefs_text(event_enable_path, "1")

    @staticmethod
    def _set_tracefs_cpumask(cpu_mask):
        cpu_mask = TraceRecord._resolve_tracefs_cpus(cpu_mask)
        if cpu_mask is None:
            logging.warning("Failed to determine CPU list, keep existing tracing_cpumask configuration")
            return None
        TraceRecord._write_tracefs_text("tracing_cpumask", CPUParser.cpus_to_cpumask(cpu_mask))
        return cpu_mask

    @staticmethod
    def trace_clear():
        if TraceRecord._ensure_backend() == TRACE_BACKEND_TRACE_CMD:
            TraceRecord.__run([TraceRecord._get_trace_cmd_path(), 'clear'])
            return

        TraceRecord._write_tracefs_text("tracing_on", "0")
        # Writing an empty payload to trace clears the in-kernel ring buffer.
        TraceRecord._write_tracefs_text("trace", "")

    @staticmethod
    def trace_reset(log_stats=True):
        if TraceRecord._ensure_backend() == TRACE_BACKEND_TRACE_CMD:
            TraceRecord.__run([TraceRecord._get_trace_cmd_path(), 'reset'])
            return

        if log_stats:
            TraceRecord.log_tracefs_stats()
        TraceRecord._write_tracefs_text("tracing_on", "0")
        TraceRecord._write_tracefs_text("current_tracer", "nop")
        TraceRecord._write_tracefs_text("events/enable", "0")
        TraceRecord._write_tracefs_text("set_event", "")

    @staticmethod
    def trace_start(
        cpu_mask,
        output,
        event_cfg,
        buffer_size=DEFAULT_TRACE_BUFFER_SIZE,
    ):
        final_output = TraceRecord._get_final_output_path(output)
        TraceRecord._output_path = final_output

        if TraceRecord._ensure_backend() == TRACE_BACKEND_TRACE_CMD:
            start_command = [TraceRecord._get_trace_cmd_path(), 'record', '-b', str(buffer_size), '-C', 'mono_raw']

            if event_cfg.sched:
                for event in SCHED_EVENT_LIST:
                    start_command.extend(['-e', event])
            if event_cfg.irq:
                for event in IRQ_EVENT_LIST:
                    start_command.extend(['-e', event])
            if event_cfg.futex:
                for event in FUTEX_EVENT_LIST:
                    start_command.extend(['-e', event])

            if cpu_mask is not None:
                start_command.append('-M')
                start_command.append(CPUParser.cpus_to_cpumask(cpu_mask))

            start_command.extend(['-o', final_output])

            # Start trace-cmd in background so we can stop it dynamically later
            logging.info("Starting trace-cmd record process: %s", " ".join(start_command))
            # pylint: disable=consider-using-with
            TraceRecord._record_process = subprocess.Popen(  # nosec B603
                start_command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            )
            # pylint: enable=consider-using-with
            return final_output

        TraceRecord._write_tracefs_text("tracing_on", "0")
        TraceRecord._write_tracefs_text("current_tracer", "nop")
        TraceRecord._write_tracefs_text("trace_clock", "mono_raw")
        TraceRecord._write_tracefs_text("buffer_size_kb", str(buffer_size))
        TraceRecord._set_tracefs_cpumask(cpu_mask)
        TraceRecord._set_tracefs_events(event_cfg)
        TraceRecord._write_tracefs_text("trace", "")
        TraceRecord._write_tracefs_text("tracing_on", "1")
        logging.info("Starting debugfs ftrace recording at %s", TraceRecord._trace_root)
        return final_output

    @staticmethod
    def trace_stop(output=None):
        if TraceRecord._ensure_backend() == TRACE_BACKEND_TRACE_CMD:
            # If we launched a trace-cmd record subprocess, try to stop it gracefully
            proc = TraceRecord._record_process
            if proc:
                try:
                    logging.info("Stopping trace-cmd record process")
                    # Try sending SIGINT a few times to let trace-cmd finish cleanly
                    for _ in range(TraceRecord._INT_RETRY_COUNT):
                        try:
                            os.kill(proc.pid, signal.SIGINT)
                        except Exception as e:
                            logging.exception("Failed to send SIGINT to kill trace-cmd record process: %s", e)
                            pass
                        time.sleep(TraceRecord._INT_INTERVAL_SEC)
                        if proc.poll() is not None:
                            break
                    return_code = proc.wait(timeout=TraceRecord._WAIT_TIMEOUT_SEC)
                    if return_code in (0, -signal.SIGINT):
                        logging.info("trace-cmd record process stopped normally")
                    else:
                        logging.warning("trace-cmd record process stopped with exit code %s", return_code)
                except subprocess.TimeoutExpired:
                    try:
                        proc.terminate()
                    except Exception as terminate_exception:
                        logging.exception("Failed to terminate trace-cmd record process: %s", terminate_exception)
                    logging.error(
                        "Forcefully stopped trace-cmd record process due to timeout after waiting for %s seconds. "
                        "This might indicate that the process is taking longer than expected to finish. "
                        "Consider increasing the _WAIT_TIMEOUT_SEC value or investigating the cause of delay. ",
                        TraceRecord._WAIT_TIMEOUT_SEC,
                    )
                finally:
                    TraceRecord._record_process = None
                return TraceRecord._output_path

            # Fallback: run trace-cmd stop if we don't have a process handle
            logging.error("No trace-cmd record process found.")
            return TraceRecord._output_path

        final_output = TraceRecord._get_final_output_path(output)
        TraceRecord._write_tracefs_text("tracing_on", "0")
        logging.info("debugfs tracing disabled")
        TraceRecord._copy_tracefs_trace(final_output)
        TraceRecord._output_path = final_output
        return final_output

    @staticmethod
    def trace_show(output):
        logging.info("Write result to file")
        if TraceRecord._ensure_backend() == TRACE_BACKEND_TRACE_CMD:
            with open(output, 'w', encoding="utf-8") as file:
                TraceRecord.__run([TraceRecord._get_trace_cmd_path(), 'show'], stdout=file)
        else:
            TraceRecord._copy_tracefs_trace(output)
        logging.info("Write end")
        return output

    @staticmethod
    def __run(commands: list, stdout=None):
        logging.info("Run command: %s", " ".join(commands))
        subprocess.run(  # nosec B603
            commands, stdout=subprocess.DEVNULL if stdout is None else stdout, check=True
        )


def normal_mode(args):
    signal.signal(signal.SIGTERM, on_exit)
    try:
        if args.NSpid:
            nspid_recorder = ContainerPidMapper()
            nspid_recorder.start(None)
        started = ftrace_record_start(args.cpu, args.output, args.bf_size, args=args)
        if not started:
            return
    except KeyboardInterrupt:
        ftrace_record_stop(args.output)
        return
    if args.record_time <= 0:
        logging.warning('Record time equals -1, start long term record')
        while True:
            try:
                time.sleep(1)
            except KeyboardInterrupt:
                break
    else:
        time.sleep(args.record_time)
    ftrace_record_stop(args.output)


class ContainerPidMapper:
    def __init__(self, output_file: str = "pid_mapping.json"):
        self.output_file = output_file
        self.stop_flag = threading.Event()
        self.work_thread = None
        self.pid_dict = {}

    def read_process_status(self, host_pid: int):
        """从 /proc/<pid>/status 中读取 NSpid，返回容器内 PID（若存在）"""

        try:
            status_path = f"/proc/{host_pid}/status"
            with open(status_path, "r", encoding="utf-8") as status_file:
                return self.parse_process_status_file(status_file)
        except FileNotFoundError:
            pass
        return {}

    def parse_process_status_file(self, file):
        status = {"name": "", "NSpid": "", "Tgid": "", "PPid": ""}
        for line in file:
            if line.startswith("NSpid:"):
                status['NSpid'] = self.get_status_value(line, 2)
            if line.startswith("Name:"):
                status['name'] = self.get_status_value(line, 1)
            if line.startswith("Tgid"):
                status['Tgid'] = self.get_status_value(line, 1)
            if line.startswith('PPid'):
                status['PPid'] = self.get_status_value(line, 1)
        return status

    def get_status_value(self, line: str, index: int):
        parts = line.strip().split()
        if len(parts) <= index:
            return ""
        return parts[index]

    def list_all_host_pids(self):
        """列出 /proc 下所有数字目录（即当前系统所有进程 PID）"""
        pids = []
        try:
            for entry in os.listdir("/proc"):
                if entry.isdigit():
                    pids.append(int(entry))
        except OSError as e:
            logging.error("Find pid status error, error=%s", e)
            pass
        return pids

    def get_pid_mapping(self):
        """获取容器内所有进程的 {host_pid: container_pid} 映射"""
        all_pids = self.list_all_host_pids()
        for pid in all_pids:
            status = self.read_process_status(pid)
            self.pid_dict[pid] = status

    def _write_json(self, data: dict):
        try:
            with open(self.output_file, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2)
            logging.info("Write nspid collect result to file")
        except Exception as e:
            print(f"Write json failed: {e}")

    def _worker(self, duration):
        """后台工作线程主循环"""
        if duration is None:
            self.get_pid_mapping()
            self._write_json(self.pid_dict)
            return
        while not self.stop_flag.is_set():
            self.get_pid_mapping()
            time.sleep(duration)

    def start(self, duration=None):
        """启动后台采集线程（非阻塞）"""
        if duration is None:
            self._worker(duration)
            return
        self.work_thread = threading.Thread(target=self._worker, args=(duration,))
        self.work_thread.start()

    def stop(self):
        """停止采集"""
        self.stop_flag.set()
        if self.work_thread and self.work_thread.is_alive():
            self.work_thread.join(timeout=5)
        self._worker(None)
        self._write_json(self.pid_dict)

    def is_running(self) -> bool:
        """检查是否正在运行"""
        return not self.stop_flag.is_set()


class TraceRecordDaemon(TraceRecord):
    def __init__(self, output="ftrace.txt", rotation_time=30, backup_count=4, buffer_size=40960):
        self.rotation_time = rotation_time
        self.backup_count = backup_count
        self.output_prefix = output
        self.buffer_size = buffer_size
        self.record_process = None
        self.show_process = None
        self.show_thread = None
        self.output_files = []
        self.cur_output_file = None
        self.cur_file_path = None
        self.stop_event = threading.Event()
        self.read_end_event = threading.Event()
        self.pid_mapping_recorder = ContainerPidMapper()

    def trace_record(self, cpu_mask, duration=None):
        TraceRecord.trace_clear()
        TraceRecord.trace_reset()
        self.pid_mapping_recorder.start(duration=duration)
        # 强制不使用IO缓冲区
        logging.info("Start record process")
        logging.info("Start capture thread")
        self.show_thread = threading.Thread(target=self.capture_trace_show_out)
        self.show_thread.start()

    def capture_trace_show_out(self):
        stdout = getattr(self.show_process, "stdout", None)
        if stdout is None:
            logging.error("No trace show process found.")
            self.read_end_event.set()
            return

        last_time = int(time.time())
        self.cur_output_file = self.get_file_handler()
        count = 0
        while True:
            # 每100次检查一次,避免过于频繁
            cur_time = int(time.time())
            if count > 100 and cur_time - last_time > self.rotation_time:
                self.cur_output_file.close()
                self.cur_output_file = self.get_file_handler()
                count = 0
                last_time = cur_time
                logging.info("Switch to file %s", self.cur_file_path)
            # 非堵塞读取，每次最多读取40Kb
            context = stdout.read1(40960)
            if not context:
                if self.stop_event.is_set():
                    logging.info("Capture thread read file end")
                    self.read_end_event.set()
                    return
                else:
                    time.sleep(0.2)
                    continue
            self.cur_output_file.write(context)
            count += 1
            time.sleep(0.1)

    def trace_record_stop(self):
        logging.info("End trace record")
        TraceRecord.trace_stop()
        TraceRecord.trace_show(self.output_prefix)
        TraceRecord.trace_clear()
        TraceRecord.trace_reset()
        self.pid_mapping_recorder.stop()

    def stop_process(self, process, name):
        try:
            for i in range(0, 3):
                os.kill(process.pid, signal.SIGINT)
                time.sleep(0.5)
            process.wait(timeout=10)
            logging.info("Stop process %s normally", name)
        except subprocess.TimeoutExpired:
            process.terminate()
            logging.warning("Force stop process %s", name)

    def get_file_handler(self):
        cur_time = int(time.time())
        self.cur_file_path = self.output_prefix + "_" + str(cur_time)
        if self.backup_count > 0:
            self.output_files.append(self.cur_file_path)
            if len(self.output_files) > self.backup_count:
                try:
                    os.remove(self.output_files[0])
                    self.output_files.pop(0)
                except Exception as e:
                    logging.warning("This is an exception when remove file, exception=%s", e)
        return open(self.cur_file_path, 'wb')


def daemon_mode(args):
    recorder = TraceRecordDaemon(args.output, args.rotation, args.backup_count)
    if args.NSpid:
        recorder.trace_record(args.cpu, args.duration)
    else:
        recorder.trace_record(args.cpu)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        recorder.trace_record_stop()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG, format='[%(asctime)s] [%(levelname)s]:%(message)s')
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--cpu',
        type=str,
        default=None,
        help="Specify CPU cores to collect. Supports single numbers, commas, and hyphen ranges. e.g., '0,1,4' or '0-3,8'. Default: collect all CPUs.",
    )
    parser.add_argument('--output', type=str, default='trace.dat')
    parser.add_argument(
        '--record_time',
        type=int,
        default=DEFAULT_TRACE_RECORD_TIME,
        help='record time, if pass <=0 will start long term record that user should attention the disk space',
    )
    parser.add_argument(
        '--bf_size',
        type=int,
        default=DEFAULT_TRACE_BUFFER_SIZE,
        help='trace-cmd ring buffer size in KB, used to store ftrace events during tracing. '
        'Increase this value to avoid event loss when trace volume is large.',
    )
    parser.add_argument(
        '--backend',
        choices=[TRACE_BACKEND_AUTO, TRACE_BACKEND_TRACE_CMD, TRACE_BACKEND_DEBUGFS],
        default=TRACE_BACKEND_AUTO,
        help='Trace backend selection. Default is auto: prefer trace-cmd and fall back to debugfs when trace-cmd is unavailable.',
    )
    parser.add_argument(
        '--sched', type=int, choices=[0, 1], default=1, help='Enable sched events (default on). Use 0 to disable.'
    )
    parser.add_argument(
        '--irq', type=int, choices=[0, 1], default=1, help='Enable irq/softirq events (default on). Use 0 to disable.'
    )
    parser.add_argument(
        '--futex',
        type=int,
        choices=[0, 1],
        default=0,
        help='Enable futex syscall events (default off). Use 1 to enable.',
    )
    parser.add_argument('--NSpid', action='store_true', help='will try to record the pid flex map')
    parser.add_argument('--duration', type=int, default=30)
    confirm = (
        input("This script requires root privileges, irreverisble action may occur. Continue? (y/N):").strip().lower()
    )
    if confirm not in ('y', 'yes'):
        logging.critical("Aborted")
        sys.exit(1)

    cli_args = parser.parse_args()
    normal_mode(cli_args)
