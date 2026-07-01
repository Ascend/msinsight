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

import logging
import os
import sys
import unittest

# Add parent directory to sys.path to import trace_convert
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import trace_convert

# Disable logging output during tests to keep console clean
logging.disable(logging.CRITICAL)


def build_sched_parser():
    trace_convert.PidTran._instances = {}
    trace_convert.TimeStampTran._instances = {}
    trace_convert.PidTran().pid_status = None
    trace_convert.TimeStampTran().initialize(None)
    return trace_convert.SchedFtraceParse(file_type='txt')


class TestTraceConvertRobustness(unittest.TestCase):
    """Test trace_convert robustness for malformed trace events."""

    def setUp(self):
        self.parser = build_sched_parser()

    def enable_warning_logs(self):
        original_disable_level = logging.getLogger().manager.disable
        logging.disable(logging.NOTSET)
        self.addCleanup(logging.disable, original_disable_level)
        trace_convert._seen_log_warning.clear()

    def test_parse_base_param_invalid_token_returns_none(self):
        """验证基础参数存在非法 token 时返回 None，不中断转换。"""
        self.assertIsNone(self.parser.parse_base_param("broken pid=123"))

    def test_sched_switch_missing_required_fields_warns_and_skips(self):
        """验证 sched_switch 缺少关键字段时告警并跳过。"""
        self.enable_warning_logs()

        with self.assertLogs(level='WARNING') as logs:
            self.parser.parse_sched_event(
                {
                    "cpu": "0",
                    "timestamp": 1000,
                    "action": "sched_switch",
                    "args": "prev_comm=python prev_pid=123 prev_state=R ==> next_comm=bash next_pid=456",
                }
            )
            self.parser.parse_sched_event(
                {
                    "cpu": "0",
                    "timestamp": 2000,
                    "action": "sched_switch",
                    "args": "prev_comm=python prev_pid=123 prev_state=R ==> next_comm=bash next_pid=456",
                }
            )

        self.assertEqual(sum("sched_switch" in message for message in logs.output), 1)

    def test_sched_wakeup_missing_required_fields_warns_and_skips(self):
        """验证 sched_wakeup 缺少关键字段时告警并跳过。"""
        self.enable_warning_logs()

        with self.assertLogs(level='WARNING') as logs:
            self.parser.parse_sched_event(
                {"cpu": "0", "timestamp": 1000, "action": "sched_wakeup", "args": "comm=python prio=120"}
            )
            self.parser.parse_sched_event(
                {"cpu": "0", "timestamp": 2000, "action": "sched_wakeup", "args": "comm=python prio=120"}
            )

        self.assertEqual(sum("sched_wakeup" in message for message in logs.output), 1)

    def test_sched_process_exit_bad_args_warns_and_skips(self):
        """验证 sched_process_exit 坏数据告警并跳过。"""
        self.enable_warning_logs()

        with self.assertLogs(level='WARNING') as logs:
            self.parser.parse_sched_process_exit({"timestamp": 1000, "args": "comm=python"})
            self.parser.parse_sched_process_exit({"timestamp": 2000, "args": "comm=python"})

        self.assertEqual(sum("sched_process_exit" in message for message in logs.output), 1)

    def test_sched_process_exec_bad_args_warns_and_skips(self):
        """验证 sched_process_exec 坏数据告警并跳过。"""
        self.enable_warning_logs()

        with self.assertLogs(level='WARNING') as logs:
            self.parser.parse_sched_process_exec({"timestamp": 1000, "cpu": "0", "task": "python", "args": "pid=123"})
            self.parser.parse_sched_process_exec({"timestamp": 2000, "cpu": "0", "task": "python", "args": "pid=123"})

        self.assertEqual(sum("sched_process_exec" in message for message in logs.output), 1)

    def test_sched_stat_runtime_bad_args_warns_and_skips(self):
        """验证 sched_stat_runtime 坏数据告警并跳过。"""
        self.enable_warning_logs()

        with self.assertLogs(level='WARNING') as logs:
            self.parser.parse_sched_stat_runtime({"timestamp": 1000, "cpu": "0", "args": "comm=python runtime=10"})
            self.parser.parse_sched_stat_runtime({"timestamp": 2000, "cpu": "0", "args": "comm=python runtime=10"})

        self.assertEqual(sum("sched_stat_runtime" in message for message in logs.output), 1)

    def test_sched_stat_runtime_accepts_runtime_with_unit(self):
        """验证带 [ns] 单位的 runtime 正常解析，不误报坏数据。"""
        self.parser.parse_sched_event(
            {
                "cpu": "0",
                "timestamp": 1000,
                "action": "sched_stat_runtime",
                "args": "comm=trace-cmd pid=3274278 runtime=167412 [ns] vruntime=27615885484538 [ns]",
            }
        )

        self.assertEqual(self.parser.cpu_stats["0"]["trace-cmd:3274278"].total_runtime, 0)

    def test_negative_sched_stat_runtime_warns_and_skips(self):
        """验证负 runtime 告警并丢弃，不创建 CPU 运行片段。"""
        self.enable_warning_logs()

        with self.assertLogs(level='WARNING') as logs:
            self.parser.parse_sched_event(
                {
                    "cpu": "0",
                    "timestamp": 1000,
                    "action": "sched_stat_runtime",
                    "args": "comm=python pid=123 runtime=-1",
                }
            )
            self.parser.parse_sched_event(
                {
                    "cpu": "0",
                    "timestamp": 2000,
                    "action": "sched_stat_runtime",
                    "args": "comm=python pid=123 runtime=-2",
                }
            )

        self.assertEqual(sum("Negative sched_stat_runtime runtime" in message for message in logs.output), 1)
        self.assertNotIn("python:123", self.parser.cpu_stats["0"])

    def test_negative_process_duration_warns_and_skips(self):
        """验证进程调度负时长告警并丢弃。"""
        self.enable_warning_logs()
        process = trace_convert.Process("python", "123", 2000)
        process.state = 'R'

        with self.assertLogs(level='WARNING') as logs:
            process.sleep(1000)
            process.runnable(900)

        self.assertEqual(sum("Negative process scheduling duration" in message for message in logs.output), 1)
        self.assertEqual(len(process.events), 1)

    def test_negative_cpu_running_duration_warns_and_skips(self):
        """验证 CPU running 负时长告警并丢弃。"""
        self.enable_warning_logs()
        self.parser.cpu_stats["0"] = {"python:123": trace_convert.CompleteEvent("python", "123", 2000, "0", "120")}

        with self.assertLogs(level='WARNING') as logs:
            self.parser.parse_sched_switch(
                {
                    "cpu": "0",
                    "timestamp": 1000,
                    "args": (
                        "prev_comm=python prev_pid=123 prev_prio=120 prev_state=S ==> "
                        "next_comm=bash next_pid=456 next_prio=120"
                    ),
                }
            )
            self.parser.cpu_stats["0"]["python:123"] = trace_convert.CompleteEvent("python", "123", 2000, "0", "120")
            self.parser.parse_sched_switch(
                {
                    "cpu": "0",
                    "timestamp": 900,
                    "args": (
                        "prev_comm=python prev_pid=123 prev_prio=120 prev_state=S ==> "
                        "next_comm=bash next_pid=456 next_prio=120"
                    ),
                }
            )

        self.assertEqual(sum("Negative CPU running duration" in message for message in logs.output), 1)
        self.assertFalse(any(event.get("name") == "python:123" for event in self.parser.trace_event))

    def test_irq_bad_args_warns_and_skips(self):
        """验证 irq 参数坏数据告警并跳过。"""
        parser = trace_convert.InterruptFtraceParse(file_type='txt')
        self.enable_warning_logs()

        with self.assertLogs(level='WARNING') as logs:
            parser.parse_irq_event(
                {
                    "cpu": "0",
                    "pid": "123",
                    "timestamp": 1000,
                    "task": "python",
                    "action": "irq_handler_entry",
                    "args": "broken",
                }
            )
            parser.parse_irq_event(
                {
                    "cpu": "0",
                    "pid": "123",
                    "timestamp": 2000,
                    "task": "python",
                    "action": "irq_handler_entry",
                    "args": "broken",
                }
            )

        self.assertEqual(sum("irq" in message for message in logs.output), 1)
