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

import contextlib
import logging
import os
import shutil
import subprocess  # nosec B404
import signal
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

# Add parent directory to sys.path to import trace_record
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import trace_record

# Disable logging output during tests to keep console clean
logging.disable(logging.CRITICAL)


class TestConstants(unittest.TestCase):
    """Test constants defined in trace_record"""

    def test_backend_constants(self):
        self.assertEqual(trace_record.TRACE_BACKEND_AUTO, "auto")
        self.assertEqual(trace_record.TRACE_BACKEND_TRACE_CMD, "trace-cmd")
        self.assertEqual(trace_record.TRACE_BACKEND_DEBUGFS, "debugfs")

    def test_default_values(self):
        self.assertEqual(trace_record.DEFAULT_TRACE_BUFFER_SIZE, 40960)
        self.assertEqual(trace_record.DEFAULT_TRACE_RECORD_TIME, 30)
        self.assertEqual(trace_record.TRACE_COPY_CHUNK_SIZE, 1024 * 1024)

    def test_debugfs_trace_roots(self):
        expected_roots = ("/sys/kernel/tracing", "/sys/kernel/debug/tracing")
        self.assertEqual(trace_record.DEFAULT_DEBUGFS_TRACE_ROOTS, expected_roots)


class TestEventLists(unittest.TestCase):
    """Test event list constants"""

    def test_sched_event_list(self):
        self.assertEqual(len(trace_record.SCHED_EVENT_LIST), 9)
        self.assertIn("sched:sched_switch", trace_record.SCHED_EVENT_LIST)
        self.assertIn("sched:sched_process_exit", trace_record.SCHED_EVENT_LIST)
        self.assertTrue(all(event.startswith("sched:") for event in trace_record.SCHED_EVENT_LIST))

    def test_irq_event_list(self):
        self.assertEqual(len(trace_record.IRQ_EVENT_LIST), 5)
        self.assertIn("irq:irq_handler_entry", trace_record.IRQ_EVENT_LIST)
        self.assertIn("irq:softirq_exit", trace_record.IRQ_EVENT_LIST)
        self.assertTrue(all(event.startswith(("irq:", "irq:softirq")) for event in trace_record.IRQ_EVENT_LIST))

    def test_futex_event_list(self):
        expected_events = {
            "syscalls:sys_enter_futex",
            "syscalls:sys_exit_futex",
        }
        self.assertEqual(trace_record.FUTEX_EVENT_LIST, expected_events)


class TestIsRootUser(unittest.TestCase):
    """Test _is_root_user function"""

    def test_root_user(self):
        with patch.object(os, 'getuid', return_value=0):
            self.assertTrue(trace_record._is_root_user())

    def test_non_root_user(self):
        with patch.object(os, 'getuid', return_value=1000):
            self.assertFalse(trace_record._is_root_user())

    def test_no_getuid_attribute(self):
        original_getuid = getattr(os, 'getuid', None)
        if hasattr(os, 'getuid'):
            delattr(os, 'getuid')
        self.assertTrue(trace_record._is_root_user())
        if original_getuid is not None:
            os.getuid = original_getuid


class TestTraceEventConfig(unittest.TestCase):
    """Test TraceEventConfig class"""

    def test_default_config(self):
        config = trace_record.TraceEventConfig()
        self.assertEqual(config.sched, 1)
        self.assertEqual(config.irq, 1)
        self.assertEqual(config.futex, 0)

    def test_custom_config(self):
        config = trace_record.TraceEventConfig(sched=0, irq=0, futex=1)
        self.assertEqual(config.sched, 0)
        self.assertEqual(config.irq, 0)
        self.assertEqual(config.futex, 1)

    def test_partial_config(self):
        config = trace_record.TraceEventConfig(sched=0)
        self.assertEqual(config.sched, 0)
        self.assertEqual(config.irq, 1)
        self.assertEqual(config.futex, 0)


class TestCPUParser(unittest.TestCase):
    """Test CPUParser class"""

    def test_parse_cpu_arg_single(self):
        self.assertEqual(trace_record.CPUParser.parse_cpu_arg("0"), [0])

    def test_parse_cpu_arg_comma_separated(self):
        self.assertEqual(trace_record.CPUParser.parse_cpu_arg("0,1,2"), [0, 1, 2])

    def test_parse_cpu_arg_range(self):
        self.assertEqual(trace_record.CPUParser.parse_cpu_arg("0-3"), [0, 1, 2, 3])

    def test_parse_cpu_arg_mixed(self):
        self.assertEqual(trace_record.CPUParser.parse_cpu_arg("0-2,5,7-8"), [0, 1, 2, 5, 7, 8])

    def test_parse_cpu_arg_empty(self):
        self.assertIsNone(trace_record.CPUParser.parse_cpu_arg(None))

    def test_parse_cpu_arg_whitespace(self):
        self.assertEqual(trace_record.CPUParser.parse_cpu_arg("0, 1, 2"), [0, 1, 2])

    def test_parse_cpu_arg_invalid_negative(self):
        self.assertIsNone(trace_record.CPUParser.parse_cpu_arg("-1"))

    def test_parse_cpu_arg_invalid_range(self):
        self.assertIsNone(trace_record.CPUParser.parse_cpu_arg("5-2"))

    def test_parse_cpu_arg_invalid_format(self):
        self.assertIsNone(trace_record.CPUParser.parse_cpu_arg("abc"))

    def test_parse_cpu_arg_duplicate(self):
        self.assertEqual(trace_record.CPUParser.parse_cpu_arg("0,0,1"), [0, 1])

    def test_normalize_cpu_mask_none(self):
        self.assertIsNone(trace_record.CPUParser.normalize_cpu_mask(None))

    def test_normalize_cpu_mask_string(self):
        self.assertEqual(trace_record.CPUParser.normalize_cpu_mask("0-2"), [0, 1, 2])

    def test_normalize_cpu_mask_list(self):
        self.assertEqual(trace_record.CPUParser.normalize_cpu_mask([0, 1, 2]), [0, 1, 2])

    def test_normalize_cpu_mask_list_duplicates(self):
        self.assertEqual(trace_record.CPUParser.normalize_cpu_mask([0, 0, 1, 2, 2]), [0, 1, 2])

    def test_normalize_cpu_mask_list_unsorted(self):
        self.assertEqual(trace_record.CPUParser.normalize_cpu_mask([2, 0, 1]), [0, 1, 2])

    def test_normalize_cpu_mask_invalid_type(self):
        self.assertIsNone(trace_record.CPUParser.normalize_cpu_mask(123))

    def test_normalize_cpu_mask_invalid_list_element(self):
        self.assertIsNone(trace_record.CPUParser.normalize_cpu_mask([0, -1, 2]))

    def test_cpus_to_cpumask_single(self):
        self.assertEqual(trace_record.CPUParser.cpus_to_cpumask([0]), "00000001")

    def test_cpus_to_cpumask_multiple(self):
        self.assertEqual(trace_record.CPUParser.cpus_to_cpumask([0, 1, 2]), "00000007")

    def test_cpus_to_cpumask_empty(self):
        self.assertEqual(trace_record.CPUParser.cpus_to_cpumask([]), "0")

    def test_cpus_to_cpumask_invalid(self):
        with self.assertRaises(ValueError):
            trace_record.CPUParser.cpus_to_cpumask([-1])

    def test_cpus_to_cpumask_large_cpu(self):
        self.assertEqual(trace_record.CPUParser.cpus_to_cpumask([32]), "00000001,00000000")


class TestTraceRecordBackend(unittest.TestCase):
    """Test TraceRecord backend configuration"""

    def setUp(self):
        trace_record.TraceRecord._backend_name = None
        trace_record.TraceRecord._trace_root = None
        trace_record.TraceRecord._output_path = None
        trace_record.TraceRecord._record_process = None

    def test_detect_tracing_root_exists(self):
        with patch('os.path.isdir', return_value=True), patch('os.path.exists', return_value=True):
            self.assertEqual(trace_record.TraceRecord.detect_tracing_root(), "/sys/kernel/tracing")

    def test_detect_tracing_root_not_exists(self):
        with patch('os.path.isdir', return_value=False):
            self.assertIsNone(trace_record.TraceRecord.detect_tracing_root())

    def test_configure_backend_trace_cmd_available(self):
        with patch.object(trace_record.TraceRecord, '_is_trace_cmd_available', return_value=True):
            result = trace_record.TraceRecord.configure_backend(trace_record.TRACE_BACKEND_TRACE_CMD)
            self.assertEqual(result, trace_record.TRACE_BACKEND_TRACE_CMD)

    def test_configure_backend_trace_cmd_unavailable(self):
        with patch.object(trace_record.TraceRecord, '_is_trace_cmd_available', return_value=False):
            with self.assertRaises(FileNotFoundError):
                trace_record.TraceRecord.configure_backend(trace_record.TRACE_BACKEND_TRACE_CMD)

    def test_configure_backend_unsupported(self):
        with self.assertRaises(ValueError):
            trace_record.TraceRecord.configure_backend("unsupported_backend")

    def test_is_trace_cmd_available_with_which(self):
        with patch('shutil.which', return_value='/usr/bin/trace-cmd'):
            self.assertTrue(trace_record.TraceRecord._is_trace_cmd_available())

    def test_is_trace_cmd_available_fallback(self):
        with (
            patch('shutil.which', return_value=None),
            patch('os.path.isfile', return_value=True),
            patch('os.access', return_value=True),
        ):
            self.assertTrue(trace_record.TraceRecord._is_trace_cmd_available())

    def test_is_trace_cmd_available_not_found(self):
        with patch('shutil.which', return_value=None), patch('os.path.isfile', return_value=False):
            self.assertFalse(trace_record.TraceRecord._is_trace_cmd_available())


class TestTraceRecordOutputPath(unittest.TestCase):
    """Test TraceRecord output path handling"""

    def setUp(self):
        trace_record.TraceRecord._backend_name = None
        trace_record.TraceRecord._output_path = None

    def test_get_final_output_path_none_trace_cmd(self):
        trace_record.TraceRecord._backend_name = trace_record.TRACE_BACKEND_TRACE_CMD
        self.assertEqual(trace_record.TraceRecord._get_final_output_path(None), "trace.dat")

    def test_get_final_output_path_none_debugfs(self):
        trace_record.TraceRecord._backend_name = trace_record.TRACE_BACKEND_DEBUGFS
        self.assertEqual(trace_record.TraceRecord._get_final_output_path(None), "trace.txt")

    def test_get_final_output_path_dat_debugfs(self):
        trace_record.TraceRecord._backend_name = trace_record.TRACE_BACKEND_DEBUGFS
        self.assertEqual(trace_record.TraceRecord._get_final_output_path("output.dat"), "output.txt")

    def test_get_final_output_path_custom(self):
        trace_record.TraceRecord._backend_name = trace_record.TRACE_BACKEND_TRACE_CMD
        self.assertEqual(trace_record.TraceRecord._get_final_output_path("custom.dat"), "custom.dat")


class TestTraceRecordStats(unittest.TestCase):
    """Test TraceRecord stats parsing"""

    def test_parse_tracefs_stats(self):
        content = "entries: 123\noverrun: 45\ncommit overrun: 0"
        result = trace_record.TraceRecord._parse_tracefs_stats(content)
        self.assertEqual(result["entries"], "123")
        self.assertEqual(result["overrun"], "45")
        self.assertEqual(result["commit overrun"], "0")

    def test_parse_tracefs_stats_empty(self):
        self.assertEqual(trace_record.TraceRecord._parse_tracefs_stats(""), {})

    def test_parse_tracefs_stats_no_colon(self):
        result = trace_record.TraceRecord._parse_tracefs_stats("invalid line\nentries: 100")
        self.assertEqual(result["entries"], "100")

    def test_stat_value_to_int_valid(self):
        self.assertEqual(trace_record.TraceRecord._stat_value_to_int("123 entries"), 123)

    def test_stat_value_to_int_empty(self):
        self.assertEqual(trace_record.TraceRecord._stat_value_to_int(""), 0)

    def test_stat_value_to_int_none(self):
        self.assertEqual(trace_record.TraceRecord._stat_value_to_int(None), 0)

    def test_stat_value_to_int_invalid(self):
        self.assertEqual(trace_record.TraceRecord._stat_value_to_int("abc"), 0)


class TestContainerPidMapper(unittest.TestCase):
    """Test ContainerPidMapper class"""

    def setUp(self):
        # pylint: disable=consider-using-with
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_init(self):
        mapper = trace_record.ContainerPidMapper("test.json")
        self.assertEqual(mapper.output_file, "test.json")
        self.assertEqual(mapper.pid_dict, {})
        self.assertFalse(mapper.stop_flag.is_set())

    def test_get_status_value_valid(self):
        mapper = trace_record.ContainerPidMapper()
        self.assertEqual(mapper.get_status_value("NSpid:\t123\t456", 2), "456")

    def test_get_status_value_index_out_of_range(self):
        mapper = trace_record.ContainerPidMapper()
        self.assertEqual(mapper.get_status_value("NSpid:\t123", 3), "")

    def test_get_status_value_basic(self):
        mapper = trace_record.ContainerPidMapper()
        self.assertEqual(mapper.get_status_value("Name: python", 1), "python")

    def test_get_status_value_multiple_values(self):
        mapper = trace_record.ContainerPidMapper()
        self.assertEqual(mapper.get_status_value("NSpid: 123 456 789", 2), "456")

    def test_get_status_value_empty_line(self):
        mapper = trace_record.ContainerPidMapper()
        self.assertEqual(mapper.get_status_value("", 1), "")

    def test_parse_process_status_file(self):
        mapper = trace_record.ContainerPidMapper()
        content = ["Name:\ttest_process\n", "NSpid:\t123\t456\n", "Tgid:\t100\n", "PPid:\t99\n"]
        mock_file = MagicMock()
        mock_file.__iter__ = MagicMock(return_value=iter(content))
        result = mapper.parse_process_status_file(mock_file)
        self.assertEqual(result["name"], "test_process")
        self.assertEqual(result["NSpid"], "456")
        self.assertEqual(result["Tgid"], "100")
        self.assertEqual(result["PPid"], "99")

    def test_list_all_host_pids(self):
        mapper = trace_record.ContainerPidMapper()
        with patch('os.listdir', return_value=['123', '456', 'abc', '789']):
            self.assertEqual(mapper.list_all_host_pids(), [123, 456, 789])

    def test_is_running(self):
        mapper = trace_record.ContainerPidMapper()
        self.assertTrue(mapper.is_running())
        mapper.stop_flag.set()
        self.assertFalse(mapper.is_running())


class TestTraceRecordDaemon(unittest.TestCase):
    """Test TraceRecordDaemon class"""

    def setUp(self):
        # pylint: disable=consider-using-with
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_init_default(self):
        daemon = trace_record.TraceRecordDaemon()
        self.assertEqual(daemon.rotation_time, 30)
        self.assertEqual(daemon.backup_count, 4)
        self.assertEqual(daemon.buffer_size, 40960)
        self.assertEqual(daemon.output_prefix, "ftrace.txt")

    def test_init_custom(self):
        daemon = trace_record.TraceRecordDaemon(
            output="custom.txt", rotation_time=60, backup_count=2, buffer_size=81920
        )
        self.assertEqual(daemon.rotation_time, 60)
        self.assertEqual(daemon.backup_count, 2)
        self.assertEqual(daemon.buffer_size, 81920)
        self.assertEqual(daemon.output_prefix, "custom.txt")

    def test_get_file_handler(self):
        daemon = trace_record.TraceRecordDaemon(output="test", backup_count=2)
        daemon.output_prefix = os.path.join(self.temp_dir, "test")
        handler = daemon.get_file_handler()
        with contextlib.closing(handler):
            self.assertIsNotNone(handler)
            self.assertTrue(os.path.exists(daemon.cur_file_path))


class TestFtraceRecordFunctions(unittest.TestCase):
    """Test ftrace_record_start and ftrace_record_stop functions"""

    def _build_args(self, record_time=-1):
        args = MagicMock()
        args.cpu = None
        args.output = "trace.dat"
        args.bf_size = trace_record.DEFAULT_TRACE_BUFFER_SIZE
        args.record_time = record_time
        args.NSpid = False
        args.sched = 1
        args.irq = 1
        args.futex = 0
        args.backend = trace_record.TRACE_BACKEND_TRACE_CMD
        return args

    def test_normal_mode_long_term_record_stops_once_on_keyboard_interrupt(self):
        args = self._build_args(record_time=-1)

        with (
            patch('trace_record.ftrace_record_start', return_value=True) as mock_start,
            patch('trace_record.ftrace_record_stop') as mock_stop,
            patch('time.sleep', side_effect=KeyboardInterrupt),
        ):
            trace_record.normal_mode(args)

        mock_start.assert_called_once_with(args.cpu, args.output, args.bf_size, args=args)
        mock_stop.assert_called_once_with(args.output)

    def test_ftrace_record_start_non_root(self):
        with patch('trace_record._is_root_user', return_value=False):
            self.assertFalse(trace_record.ftrace_record_start())

    def test_ftrace_record_start_invalid_cpu_mask(self):
        with patch('trace_record._is_root_user', return_value=True):
            self.assertFalse(trace_record.ftrace_record_start(cpu_mask="-1"))


class TestTraceRecordStop(unittest.TestCase):
    """Test TraceRecord stop handling"""

    def setUp(self):
        trace_record.TraceRecord._backend_name = trace_record.TRACE_BACKEND_TRACE_CMD
        trace_record.TraceRecord._trace_root = None
        trace_record.TraceRecord._output_path = "trace.dat"
        trace_record.TraceRecord._record_process = None

    def tearDown(self):
        trace_record.TraceRecord._backend_name = None
        trace_record.TraceRecord._trace_root = None
        trace_record.TraceRecord._output_path = None
        trace_record.TraceRecord._record_process = None

    def test_trace_cmd_stop_without_process_returns_output_path(self):
        self.assertEqual(trace_record.TraceRecord.trace_stop(), "trace.dat")

    def test_trace_cmd_stop_sends_sigint_once_and_waits_for_merge(self):
        proc = MagicMock()
        proc.pid = 1234
        proc.poll.return_value = None
        proc.wait.return_value = 0
        trace_record.TraceRecord._record_process = proc

        with patch('os.kill') as mock_kill, patch('time.sleep') as mock_sleep:
            result = trace_record.TraceRecord.trace_stop()

        mock_kill.assert_called_once_with(proc.pid, signal.SIGINT)
        mock_sleep.assert_not_called()
        proc.wait.assert_called_once_with(timeout=trace_record.TraceRecord._WAIT_TIMEOUT_SEC)
        self.assertEqual(result, "trace.dat")
        self.assertIsNone(trace_record.TraceRecord._record_process)

    def test_trace_cmd_stop_timeout_raises(self):
        proc = MagicMock()
        proc.pid = 1234
        proc.poll.return_value = None
        proc.wait.side_effect = [subprocess.TimeoutExpired(cmd="trace-cmd record", timeout=60), 0]
        trace_record.TraceRecord._record_process = proc

        with (
            patch('os.kill'),
            self.assertRaisesRegex(TimeoutError, "trace.dat.cpu\\* files may remain"),
        ):
            trace_record.TraceRecord.trace_stop()

        proc.terminate.assert_called_once()
        self.assertIsNone(trace_record.TraceRecord._record_process)

    def test_request_trace_cmd_stop_ignores_signal_failure(self):
        proc = MagicMock()
        proc.pid = 1234
        proc.poll.return_value = None

        with patch('os.kill', side_effect=ProcessLookupError("missing process")) as mock_kill:
            trace_record.TraceRecord._request_trace_cmd_stop(proc)

        mock_kill.assert_called_once_with(proc.pid, signal.SIGINT)

    def test_ftrace_record_stop_failure_does_not_clear_trace(self):
        with (
            patch.object(trace_record.TraceRecord, 'trace_stop', side_effect=TimeoutError("stop timeout")),
            patch.object(trace_record.TraceRecord, 'trace_reset') as mock_reset,
            patch.object(trace_record.TraceRecord, 'trace_clear') as mock_clear,
            patch.object(trace_record.TraceRecord, 'log_tracefs_stats') as mock_stats,
            self.assertRaises(TimeoutError),
        ):
            trace_record.ftrace_record_stop("trace.dat")

        mock_reset.assert_called_once()
        mock_clear.assert_not_called()
        mock_stats.assert_not_called()


class TestCheckTraceDataWarning(unittest.TestCase):
    """Test _check_trace_data_warning function"""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _create_trace_file(self, content: str) -> str:
        path = os.path.join(self.temp_dir, "trace.txt")
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        return path

    def test_file_not_exists(self):
        """文件不存在时应返回告警"""
        with patch('logging.warning') as mock_warning:
            trace_record._check_trace_data_warning("/nonexistent/path")
            mock_warning.assert_called_once()
            full_msg = mock_warning.call_args[0][0] % mock_warning.call_args[0][1:]
            self.assertIn("No trace data collected", full_msg)
            self.assertIn("record_time", full_msg)

    def test_file_empty(self):
        """文件为空时应返回告警"""
        path = self._create_trace_file("")
        with patch('logging.warning') as mock_warning:
            trace_record._check_trace_data_warning(path)
            mock_warning.assert_called_once()
            full_msg = mock_warning.call_args[0][0] % mock_warning.call_args[0][1:]
            self.assertIn("No trace data collected", full_msg)
            self.assertIn("record_time", full_msg)

    def test_non_empty_file(self):
        """文件非空时不应告警"""
        path = self._create_trace_file("# tracer: nop\n#\n")
        with patch('logging.warning') as mock_warning:
            trace_record._check_trace_data_warning(path)
            mock_warning.assert_not_called()

    def test_getsize_failed(self):
        """读取文件大小失败时只打印告警，不抛出异常"""
        path = os.path.join(self.temp_dir, "trace.txt")
        with (
            patch('os.path.exists', return_value=True),
            patch('os.path.getsize', side_effect=OSError("permission denied")),
            patch('logging.warning') as mock_warning,
        ):
            trace_record._check_trace_data_warning(path)
            mock_warning.assert_called_once()
            full_msg = mock_warning.call_args[0][0] % mock_warning.call_args[0][1:]
            self.assertIn("Failed to check trace data", full_msg)

    def test_ftrace_record_stop_calls_check_and_returns_output(self):
        """ftrace_record_stop 应内部调用 _check_trace_data_warning 并返回输出路径"""
        output_path = os.path.join(self.temp_dir, "trace.txt")
        with (
            patch.object(trace_record.TraceRecord, 'trace_stop', return_value=output_path),
            patch.object(trace_record.TraceRecord, 'log_tracefs_stats'),
            patch.object(trace_record.TraceRecord, 'trace_clear'),
            patch.object(trace_record.TraceRecord, 'trace_reset'),
            patch('trace_record._check_trace_data_warning') as mock_check,
        ):
            result = trace_record.ftrace_record_stop(output_path)
            mock_check.assert_called_once_with(output_path)
            self.assertEqual(result, output_path)


if __name__ == '__main__':
    unittest.main()
