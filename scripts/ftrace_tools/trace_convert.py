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

# ruff: noqa
# pylint: skip-file
# fmt: off
# black: skip-file
from collections import deque
import json
import re
import os
import logging
from abc import abstractmethod

import tqdm
import argparse
import glob
import time
import subprocess  # nosec B404
from typing import Dict, Deque, List, Any

from exporters import ExportStrategy, JsonExport, DbExport

logging.basicConfig(level=logging.INFO, format='[%(asctime)s] [%(levelname)s]:%(message)s')
CPU_SCHED_PID = "CPU Scheduling"
PROCESS_SCHED_PID = 'Process Scheduling'
KERNEL_PROCESS = ['migration', 'swapper', 'kworker']
_seen_log_warning = set()

def tran(ts):
    if not isinstance(ts, (int, float)):
        logging.warning("Invalid timestamp")
        return "0"
    return "{:.3f}".format(ts / 1000)


def is_kernel_process(comm):
    if not isinstance(comm, str):
        logging.error("Param must be string")
        return False
    for process in KERNEL_PROCESS:
        if comm.find(process) != -1:
            return True
    return False


def get_trace_event(name, pid, tid, ts, dur, args=None):
    event = {"name": name, "ph": "X", "pid": pid, "tid": tid, "ts": ts, "dur": dur}
    if args is not None:
        event['args'] = args
    return event


def get_meta_event(pid, tid):
    return {"name": "process_name", 'ph': 'M', 'pid': pid, 'tid': tid, 'args': {'name': pid}}


class InterruptEvent(object):
    def __init__(self, comm, st, cpu, **kwargs):
        self.comm = comm
        self.st = st
        self.cpu = "CPU " + cpu
        self.dur = 0
        self.et = 0
        self.kwargs = kwargs

    def to_event_json(self):
        return get_trace_event(self.comm, CPU_SCHED_PID, self.cpu, tran(self.st), tran(self.dur), args=self.kwargs)


class CompleteEvent(object):
    def __init__(self, comm, pid, st, cpu, prio):
        self.comm = comm
        self.pid = pid
        self.st = st
        self.cpu = "CPU " + cpu
        self.total_runtime = 0
        self.vruntime = 0
        self.end_state = "unknown"
        self.prio = prio

    def update_runtime(self, runtime, vruntime):
        if runtime is not None:
            self.total_runtime += int(runtime.split()[0])
        if vruntime is not None:
            self.vruntime = vruntime

    def end(self, ts, end_state=None, prio=None):
        self.dur = ts - self.st
        if end_state is not None:
            self.end_state = end_state
        if prio is not None:
            self.prio = prio

    def to_event_json(self):
        return get_trace_event(self.comm + ":" + self.pid, CPU_SCHED_PID, self.cpu, self.st, self.dur,
                               {'host_pid': self.pid, 'total_runtime': str(self.total_runtime) + " [ns]",
                                'vruntime': self.vruntime, 'end_state': self.end_state, 'prio': self.prio})


class Process(object):
    def __init__(self, comm, pid, ts):
        self.pid = pid
        self.ppid = PROCESS_SCHED_PID
        self.ts = ts
        self.comm = comm
        self.process_name = self.comm + ":" + self.pid
        self.events = []
        self.add_process_meta()
        self.state = 'W'  # R-running S-sleep W-runable, X-exit

    def add_process_meta(self):
        # 存在父进程, 挂在父进程下
        self.events.append(get_meta_event(self.ppid, self.pid))

    def get_event_name(self):
        if self.state == 'W':
            return 'Runnable'
        elif self.state == 'R':
            return 'Running'
        elif self.state == 'S':
            return 'Sleeping'
        return 'Unknown'

    def add_event(self, ts):
        event_name = self.get_event_name()
        event = get_trace_event(event_name, self.ppid, self.process_name, tran(self.ts), tran(ts - self.ts))
        self.events.append(event)

    def sleep(self, ts):
        if self.state == 'R':
            self.add_event(ts)
        self.ts = ts
        self.state = 'S'

    def wakeup(self, ts):
        self.state = 'W'
        self.ts = ts

    def run(self, ts):
        if self.state == 'W':
            self.add_event(ts)
        self.ts = ts
        self.state = 'R'

    def get_process_events(self, ts):
        self.exit(ts)
        return self.events

    def exit(self, ts):
        self.add_event(ts)
        self.state = 'X'


def singleton(cls):
    instances = {}

    def get_instance(*args, **kwargs):
        if cls not in instances:
            instances[cls] = cls(*args, **kwargs)
        return instances[cls]

    return get_instance


@singleton
class PidTran:
    def __init__(self):
        self.pid_mapping_path = None
        self.pid_status = None

    def initialize(self, pid_mapping=None):
        self.pid_mapping_path = pid_mapping
        self.load_pid_mapping(self.pid_mapping_path)

    def load_pid_mapping(self, pid_mapping_path: str) -> bool:
        if pid_mapping_path is None:
            logging.info("No pid mapping path provided, pid mapping disabled.")
            return True
        if not os.path.exists(pid_mapping_path):
            logging.warning("Pid mapping file not found: %s. pid mapping disabled.", pid_mapping_path)
            return False

        try:
            with open(pid_mapping_path, 'r') as file:
                self.pid_status = json.load(file)
                logging.info("Found pid mapping json, pid mapping enabled.")
                return True
        except json.JSONDecodeError as e:
            logging.error("Invalid JSON in pid mapping file: %s, error=%s", pid_mapping_path, e)
            return False
        except (IOError, OSError) as e:
            logging.error("Open pid mapping file failed: %s, error=%s", pid_mapping_path, e)
            return False

    def get_ns_pid(self, pid):
        if self.pid_status is None:
            return pid
        if pid not in self.pid_status:
            return pid
        return self.pid_status[pid]['NSpid']


@singleton
class TimeStampTran:
    def __init__(self):
        self.mono_raw_start = None
        self.mono_raw_end = None
        self.utc_start_timestamp = None
        self.__reset_time_filter_stats()

    def initialize(self, profiling_data):
        self.__reset_time_filter_stats()
        if profiling_data is None:
            logging.warning("No profiling data path detected. Time alignment will be skipped.")
            self.mono_raw_start = 0
            self.mono_raw_end = None
            self.utc_start_timestamp = 0
            return
        start_info = self.__get_profiling_time_info(profiling_data, "start_info")
        if start_info is None:
            logging.error("Can't find profiling start time info")
            return
        # 原始profiling文件中，mono_raw时间单位为ns，utc时间单位为us，此处归一为ns
        self.mono_raw_start = int(start_info['clockMonotonicRaw'])
        self.utc_start_timestamp = int(start_info['collectionTimeBegin']) * 1000 # us -> ns

        end_info = self.__get_profiling_time_info(profiling_data, "end_info")
        if end_info is None:
            logging.error("Can't find profiling end time info")
            return
        self.mono_raw_end = int(end_info['clockMonotonicRaw'])

    def __reset_time_filter_stats(self):
        self.ftrace_event_count = 0
        self.ftrace_in_window_count = 0
        self.ftrace_before_start_count = 0
        self.ftrace_after_end_count = 0
        self.first_ftrace_timestamp = None
        self.last_ftrace_timestamp = None
        self.__time_window_warning_reported = False

    def __record_ftrace_timestamp(self, timestamp: int):
        self.ftrace_event_count += 1
        if self.first_ftrace_timestamp is None:
            self.first_ftrace_timestamp = timestamp
        self.last_ftrace_timestamp = timestamp

    def report_time_filter_summary(self):
        if self.__time_window_warning_reported:
            return
        if self.mono_raw_start is None or self.mono_raw_end is None:
            return
        if self.ftrace_event_count == 0 or self.ftrace_in_window_count > 0:
            return

        self.__time_window_warning_reported = True
        if self.ftrace_before_start_count > 0 and self.ftrace_after_end_count == 0:
            logging.warning(
                "No ftrace events are within the profiling time window. "
                "All matched ftrace events are before profiling start. "
                "ftrace range=[%s, %s] ns, profiling range=[%s, %s] ns.",
                self.first_ftrace_timestamp, self.last_ftrace_timestamp,
                self.mono_raw_start, self.mono_raw_end
            )
            return
        if self.ftrace_after_end_count > 0 and self.ftrace_before_start_count == 0:
            logging.warning(
                "No ftrace events are within the profiling time window. "
                "The first matched ftrace event is after profiling end. "
                "ftrace range=[%s, %s] ns, profiling range=[%s, %s] ns.",
                self.first_ftrace_timestamp, self.last_ftrace_timestamp,
                self.mono_raw_start, self.mono_raw_end
            )
            return
        logging.warning(
            "No ftrace events are within the profiling time window. "
            "Matched ftrace events were filtered by profiling start/end. "
            "ftrace range=[%s, %s] ns, profiling range=[%s, %s] ns.",
            self.first_ftrace_timestamp, self.last_ftrace_timestamp,
            self.mono_raw_start, self.mono_raw_end
        )

    def __get_profiling_time_info(self, profiling_data, info_name:str):
        """
        info_name: "start_info" or "end_info"
        return: dict or None
        """
        if not os.path.exists(profiling_data):
            logging.error("Profiling data path not exist")
            # 递归查找time_info
            return None
        time_info_path = glob.glob(os.path.join(profiling_data, "**", info_name), recursive=True)
        if len(time_info_path) == 0:
            logging.error(f"Not find {info_name} in profiling data")
            return None
        with open(time_info_path[0], 'r') as f:
            time_info = json.load(f)
        return time_info

    def get_utc_timestamp(self, uptime: str):
        # ns
        timestamp = self.__str_to_int(uptime)
        self.__record_ftrace_timestamp(timestamp)

        # Filter events before profiling started
        if self.mono_raw_start is not None and timestamp < self.mono_raw_start:
            self.ftrace_before_start_count += 1
            return None, TimeFilterResult.BEFORE_START

        # Filter events after profiling ended
        if self.mono_raw_end is not None and timestamp > self.mono_raw_end:
            self.ftrace_after_end_count += 1
            return None, TimeFilterResult.AFTER_END

        self.ftrace_in_window_count += 1
        utc_ts = (timestamp - self.mono_raw_start) + self.utc_start_timestamp
        return utc_ts, TimeFilterResult.OK

    def __str_to_int(self, num_str):
        if not all((c.isdigit() or c == '.') for c in num_str) or num_str.count('.') > 1:
            logging.error("Invalid format")
            return 0
        pos = num_str.find('.')
        if pos == -1:
            return int(num_str) * 1000000
        return (int(num_str[:pos]) * 1000000 + int(num_str[pos + 1:])) * 1000

class TimeFilterResult:
     OK = 0 # ftrace时间范围在profiling的起止区间内
     BEFORE_START = 1
     AFTER_END = 2

class FtraceParse:
    def __init__(self, file_type='dat'):
        self.file_type = file_type
        self.ftrace_pattern = re.compile(
            r'(?P<task>.*)-(?P<pid>\d+)\s+\[(?P<cpu>\d+)\]\s.{4,5}\s(?P<timestamp>[\d.]+):\s+(?P<action>.*):\s+(?P<args>.*)')
        self.ftrace_pattern_for_dat = re.compile(
            r'(?P<task>.*)-(?P<pid>\d+)\s+\[(?P<cpu>\d+)\]\s+(?P<timestamp>[\d.]+):\s+(?P<action>.*):\s+(?P<args>.*)')

    def parse_one_event(self, match):
        timestamp, status = TimeStampTran().get_utc_timestamp(match.group('timestamp'))

        if status == TimeFilterResult.BEFORE_START:
            return True  # 跳过当前事件，继续解析

        if status == TimeFilterResult.AFTER_END:
            return False  # 超出profiling时间范畴，终止解析

        task = match.group('task')
        pid = match.group('pid')
        pid = PidTran().get_ns_pid(pid)
        cpu = match.group('cpu')
        action = match.group('action')
        args = match.group('args')
        result = {"task": task, "pid": pid, "cpu": cpu, "timestamp": timestamp, "action": action, "args": args}
        self.trans_to_trace_event(result)
        return True

    @abstractmethod
    def trans_to_trace_event(self, event):
        return

    @abstractmethod
    def belong(self, event: str):
        return None

    @abstractmethod
    def get_result(self):
        return

    def parse_base_param(self, string: str):
        kv = string.split(' ')
        result = [kv[0]]
        for i in range(1, len(kv)):
            if '=' in kv[i]:
                result.append(kv[i])
            else:
                result[-1] += " " + kv[i]
        kv_dic = {}
        for item in result:
            if item == '==>':
                continue
            k, v = item.split("=")
            kv_dic[k] = v

        # 需要做 pid 映射的字段
        pid_keys = ("pid", "prev_pid", "next_pid")

        for k in pid_keys:
            if k in kv_dic and kv_dic[k]:
                kv_dic[k] = PidTran().get_ns_pid(kv_dic[k])

        return kv_dic


class InterruptFtraceParse(FtraceParse):
    def __init__(self, file_type='dat'):
        super().__init__(file_type=file_type)
        self.parse_softirq_pattern = re.compile(r'vec=(?P<vec>\d+)\s+\[action=(?P<action>.+)\]')
        self.interrupt_events_res = []
        self.interrupt_events: Dict[int, Deque[InterruptEvent]] = {}

    def belong(self, event: str):
        if "irq" not in event and "softirq" not in event:
            return None
        return self.ftrace_pattern_for_dat.search(event.strip()) if self.file_type == 'dat' else self.ftrace_pattern.search(event.strip())

    def trans_to_trace_event(self, event):
        action = event['action']
        if action in ['irq_handler_entry', 'irq_handler_exit']:
            self.parse_irq_event(event)
        if action in ['softirq_entry', 'softirq_exit']:
            self.parse_softirq_event(event)

    def parse_irq_event(self, entry: dict):
        cpu = entry['cpu']
        pid = entry['pid']
        timestamp = entry['timestamp']
        kwargs = self.parse_base_param(entry['args'])
        kwargs['task'] = entry['task'] + ":" + pid
        if entry['action'] == 'irq_handler_entry':
            if cpu not in self.interrupt_events:
                self.interrupt_events[cpu] = deque()
            self.interrupt_events[cpu].append(InterruptEvent("irq", timestamp, cpu, **kwargs))
        elif entry['action'] == 'irq_handler_exit':
            if cpu not in self.interrupt_events or len(self.interrupt_events[cpu]) == 0:
                logging.warning("IRQ exit event without matching entry event on CPU %s", cpu)
                return
            event = self.interrupt_events[cpu].pop()
            event.dur = timestamp - event.st
            event.kwargs.update(kwargs)
            self.interrupt_events_res.append(event.to_event_json())

    def parse_softirq_event(self, entry: dict):
        cpu = entry['cpu']
        pid = entry['pid']
        task = entry['task']
        timestamp = entry['timestamp']
        kwargs = self.parse_softirq_param(entry['args'])

        if kwargs is None:
            # 非debug模式下，同类解析失败事件仅打印一次日志告警，避免刷屏
            key = "Failed to parse softirq event"
            if key not in _seen_log_warning:
                logging.warning(f"{key}, args: {entry['args']}")
                _seen_log_warning.add(key)
            return

        kwargs['task'] = task + ":" + pid
        if entry['action'] == 'softirq_entry':
            if cpu not in self.interrupt_events:
                self.interrupt_events[cpu] = deque()
            self.interrupt_events[cpu].append(InterruptEvent("softirq", timestamp, cpu, **kwargs))
        elif entry['action'] == 'softirq_exit':
            if cpu not in self.interrupt_events or len(self.interrupt_events[cpu]) == 0:
                logging.warning("Softirq exit event without matching entry event on CPU %s", cpu)
                return
            event = self.interrupt_events[cpu].pop()
            event.dur = timestamp - event.st
            self.interrupt_events_res.append(event.to_event_json())

    def parse_softirq_param(self, string: str):
        match = self.parse_softirq_pattern.search(string.strip())
        if match is None:
            logging.debug("Not match regex:{}", string)
            return
        vec = match.group('vec')
        action = match.group('action')
        result = {"vec": vec, "action": action}
        return result

    def get_result(self):
        return self.interrupt_events_res

class TimeFilterResult:
    OK = 0 # ftrace时间范围在profiling的起止区间内
    BEFORE_START = 1
    AFTER_END = 2

class SchedFtraceParse(FtraceParse):
    def __init__(self, file_type='dat'):
        super().__init__(file_type=file_type)
        self.cpu_stats = dict()
        self.cpu_set = set()
        self.process_state = {}
        self.process_set = set()
        self.parse_sched_switch_pattern = re.compile(
            r'(?P<prev_comm>.+):(?P<prev_pid>\d+)\s+\[(?P<prev_prio>\d+)\]\s+(?P<prev_state>\w+)\s+==>\s+(?P<next_comm>.+):(?P<next_pid>\d+)\s+\[(?P<next_prio>\d+)\]')
        self.parse_sched_wakeup_pattern = re.compile(r'(?P<comm>.+):(?P<pid>\d+)\s+\[(?P<prio>\d+)\].+CPU:(?P<cpu>\d+)')

        self.trace_event = []

    def belong(self, event: str):
        if "sched" not in event:
            return None

        return self.ftrace_pattern_for_dat.search(event.strip()) if self.file_type == 'dat' else self.ftrace_pattern.search(event.strip())

    def trans_to_trace_event(self, event):
        if 'action' not in event.keys():
            return
        if 'sched' in event['action']:
            self.parse_sched_event(event)

    def parse_sched_event(self, sched_event):
        cpu = sched_event['cpu']
        if cpu not in self.cpu_set:
            self.cpu_set.add(cpu)
            self.cpu_stats[cpu] = {}
            cpu_thread = {"name": "CPU " + cpu, "ph": "M", "pid": CPU_SCHED_PID, "tid": "CPU " + cpu}
            self.add_trace_event(cpu_thread)
        self.last_time = sched_event['timestamp']
        if sched_event['action'] == 'sched_switch':
            self.parse_sched_switch(sched_event)
        if sched_event['action'] == 'sched_wakeup' or sched_event['action'] == 'sched_wakeup_new':
            self.parse_sched_wakeup(sched_event)
        if sched_event['action'] == 'sched_process_exit':
            self.parse_sched_process_exit(sched_event)
        if sched_event['action'] == 'sched_process_exec':
            self.parse_sched_process_exec(sched_event)
        if sched_event['action'] == 'sched_stat_runtime':
            self.parse_sched_stat_runtime(sched_event)

    def parse_sched_switch(self, entry: dict):
        # draw kernel part
        cpu = entry['cpu']
        timestamp = entry['timestamp']
        kwargs = self.parse_switch_sched_param(entry['args']) if self.file_type == 'dat' else self.parse_base_param(entry['args'])

        if kwargs is None:
            # 非debug模式下，同类解析失败事件仅打印一次日志告警，避免刷屏
            key = "Failed to parse sched_switch event"
            if key not in _seen_log_warning:
                logging.warning(f"{key}, args: {entry['args']}")
                _seen_log_warning.add(key)
            return

        if not is_kernel_process(kwargs['prev_comm']) and not is_kernel_process(kwargs['next_comm']):
            self.add_trace_event(get_trace_event("sched_switch", CPU_SCHED_PID, "CPU " + cpu, timestamp, 0, args=kwargs))
        prev_comm = kwargs['prev_comm'] + ':' + kwargs['prev_pid']
        if prev_comm in self.cpu_stats[cpu].keys():
            self.cpu_stats[cpu][prev_comm].end(timestamp, kwargs['prev_state'], kwargs['prev_prio'])
            self.add_trace_event(self.cpu_stats[cpu][prev_comm].to_event_json())
            del self.cpu_stats[cpu][prev_comm]
        next_name = kwargs['next_comm'] + ':' + kwargs['next_pid']
        self.cpu_stats[cpu][next_name] = CompleteEvent(kwargs['next_comm'], kwargs['next_pid'], timestamp,
                                                       cpu, kwargs['next_prio'])
        if prev_comm not in self.process_state.keys():
            self.process_state[prev_comm] = Process(kwargs['prev_comm'], kwargs['prev_pid'], timestamp)
        self.process_state[prev_comm].sleep(timestamp)
        if next_name not in self.process_state.keys():
            self.process_state[next_name] = Process(kwargs['next_comm'], kwargs['next_pid'], timestamp)
            self.process_state[next_name].run(timestamp)
        else:
            self.process_state[next_name].run(timestamp)

    def parse_sched_wakeup(self, entry):
        timestamp = entry['timestamp']
        args_dict = self.parse_weakup_sched_param(entry['args']) if self.file_type == 'dat' else self.parse_base_param(entry['args'])

        if args_dict is None:
            # 非debug模式下，同类解析失败事件仅打印一次日志告警，避免刷屏
            key = "Failed to parse sched_wakeup event"
            if key not in _seen_log_warning:
                logging.warning(f"{key}, args: {entry['args']}")
                _seen_log_warning.add(key)
            return

        comm = args_dict['comm'] + ':' + args_dict['pid']
        if comm in self.process_state.keys():
            self.process_state[comm].wakeup(timestamp)
        else:
            self.process_state[comm] = Process(args_dict['comm'], args_dict['pid'], timestamp)

    def parse_sched_process_exit(self, entry):
        args_dict = self.parse_base_param(entry['args'])
        timestamp = entry['timestamp']
        comm = args_dict['comm'] + ':' + args_dict['pid']
        if comm in self.process_state.keys():
            self.process_state[comm].exit(timestamp)
            del self.process_state[comm]

    def parse_sched_process_exec(self, entry):
        ts = entry['timestamp']
        cpu = entry['cpu']
        kwargs = self.parse_base_param(entry['args'])
        pid = kwargs['pid']
        old_key = entry['task'] + ':' + pid
        new_comm = kwargs['filename'].split('/')[-1]
        new_key = new_comm + ':' + pid

        if old_key == new_key:
            if new_key not in self.cpu_stats[cpu].keys():
                self.cpu_stats[cpu][new_key] = CompleteEvent(new_comm, pid, ts, cpu, "unknown")
            if new_key not in self.process_state:
                self.process_state[new_key] = Process(new_comm, pid, ts)
                self.process_state[new_key].run(ts)
            return

        # old key != new key, update cpu_stats and process_state
        # ---------- cpu_stats ----------
        if old_key in self.cpu_stats[cpu].keys():
            self.cpu_stats[cpu][old_key].end(ts)
            self.add_trace_event(self.cpu_stats[cpu][old_key].to_event_json())
            del self.cpu_stats[cpu][old_key]
        self.cpu_stats[cpu][new_key] = CompleteEvent(new_comm, pid, ts, cpu, "unknown")

        # ---------- process_state ----------
        if old_key in self.process_state:
            self.process_state[old_key].exit(ts)
            del self.process_state[old_key]
        self.process_state[new_key] = Process(new_comm, pid, ts)
        self.process_state[new_key].run(ts)

    def parse_sched_stat_runtime(self, entry):
        ts = entry['timestamp']
        cpu = entry['cpu']
        kwargs = self.parse_base_param(entry['args'])
        comm = kwargs['comm']
        pid = kwargs['pid']
        process = comm + ':' + pid
        # 当前进程已存在，更新运行时间
        if process in self.cpu_stats[cpu]:
            self.cpu_stats[cpu][process].update_runtime(kwargs.get('runtime'), kwargs.get('vruntime', None))
        else:
            self.cpu_stats[cpu][process] = CompleteEvent(comm, pid, ts, cpu, "unknown")

    def parse_switch_sched_param(self, string: str):
        match = self.parse_sched_switch_pattern.search(string.strip())
        if match is None:
            logging.debug("Not match regex:{}", string)
            return
        prev_comm = match.group('prev_comm')
        prev_pid = PidTran().get_ns_pid(match.group('prev_pid'))
        prev_prio = match.group('prev_prio')
        prev_state = match.group('prev_state')
        next_comm = match.group('next_comm')
        next_pid = PidTran().get_ns_pid(match.group('next_pid'))
        next_prio = match.group('next_prio')
        result = {"prev_comm": prev_comm, "prev_pid": prev_pid, "prev_prio": prev_prio, "prev_state": prev_state,
                  "next_comm": next_comm, "next_pid": next_pid, "next_prio": next_prio}
        return result

    def parse_weakup_sched_param(self, string: str):
        match = self.parse_sched_wakeup_pattern.search(string.strip())
        if match is None:
            logging.debug("Not match regex:{}", string)
            return
        comm = match.group('comm')
        pid = PidTran().get_ns_pid(match.group('pid'))
        prio = match.group('prio')
        cpu = match.group('cpu')
        result = {"comm": comm, "pid": pid, "prio": prio, "cpu": cpu}
        return result

    def add_trace_event(self, event):
        # swapper进程为内核的特殊进程，忽略
        if is_kernel_process(event['name']):
            return
        if 'ts' in event.keys():
            event['ts'] = tran(event['ts'])
        if 'dur' in event.keys():
            event['dur'] = tran(event['dur'])
        self.trace_event.append(event)

    def get_result(self):
        result = []
        result.extend(self.trace_event)
        result.append({'name': 'Process Scheduling', 'ph': 'M', 'pid': PROCESS_SCHED_PID, 'tid': PROCESS_SCHED_PID})
        for k, value in self.process_state.items():
            if is_kernel_process(k):
                continue
            result.extend(value.get_process_events(self.last_time))
        return result


class TraceConverter:
    def __init__(self, trace_file_path, profiling_data=None, pid_mapping=None):
        self.trace_file_path = trace_file_path
        self.file_type = 'dat' if trace_file_path.endswith('.dat') else 'txt'
        self.parse_func_map = []
        self.pid_status = PidTran()
        self.pid_status.initialize(pid_mapping)
        self.time_tran = TimeStampTran()
        self.time_tran.initialize(profiling_data)
        self.register_parser(self.file_type)

    def register_parser(self, file_type='dat'):
        self.parse_func_map.append(SchedFtraceParse(file_type=file_type))
        self.parse_func_map.append(InterruptFtraceParse(file_type=file_type))

    def parse(self):
        self.parse_trace_file()
        result = []
        for parser in self.parse_func_map:
            result.extend(parser.get_result())
        return result

    def export(self, strategy: ExportStrategy, output_path: str):
        data = self.parse()
        strategy.export(data, output_path)

    def get_lines_from_trace_cmd(self):
        cmd = ["trace-cmd", "report", "-i", self.trace_file_path]
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)  # nosec B603

        for line in proc.stdout:
            yield line.rstrip("\n")

        proc.wait()

        if proc.returncode != 0:
            err = proc.stderr.read()
            logging.error(f"trace-cmd failed: {err}")

    def get_lines(self):
        if self.file_type == 'dat':
            return self.get_lines_from_trace_cmd()
        return self.get_lines_from_file()

    def get_lines_from_file(self):
        with open(self.trace_file_path, 'r', encoding='utf-8', errors='replace') as file:
            for line in file:
                yield line.rstrip("\n")

    def parse_trace_file(self):
        logging.info(f"start parse ftrace file: {self.trace_file_path}")
        # check whether file exist
        if not os.path.exists(self.trace_file_path):
            logging.critical(f"File not exists: {self.trace_file_path}")
            return False
        lines = self.get_lines()
        for line in tqdm.tqdm(lines, desc="Parsing trace file", unit="line"):
            for parser in self.parse_func_map:
                match = parser.belong(line)
                if not match:
                    continue
                # parse_one_event 返回 False =>AFTER_END，直接结束解析
                keep_parsing = parser.parse_one_event(match)
                if keep_parsing is False:
                    self.time_tran.report_time_filter_summary()
                    return True
                # 当前行已经被某个parser消费，不再匹配其他parser
                break
        self.time_tran.report_time_filter_summary()
        return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert ftrace data to JSON or DB format')
    parser.add_argument('--input', type=str, default='trace.dat', help='Input trace file (.dat or .txt)')
    parser.add_argument('--output', type=str, default='ftrace_data.db', help='Output file path')
    parser.add_argument('--format', type=str, choices=['json', 'db'], default='db',
                        help='Output format: json or db (default: db)')
    parser.add_argument('--profiling_data', type=str, help='Use profiling data to adjust start time')
    parser.add_argument('--pid_mapping', type=str, help='Container pid map file')
    args = parser.parse_args()

    t_start = time.perf_counter()
    converter = TraceConverter(args.input, args.profiling_data, args.pid_mapping)

    if args.format == 'json':
        strategy = JsonExport()
    elif args.format == 'db':
        strategy = DbExport()
    else:
        logging.error(f"Unsupported format: {args.format}")
        exit(1)

    logging.info(f"Start converting {args.input} to {args.format} format...")
    converter.export(strategy, args.output)

    t_end = time.perf_counter()
    logging.info(f"Total convert time: {t_end - t_start:.3f}s")
