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

import json
import logging
import os
import shutil
import sys
import tempfile
import unittest

# Add parent directory to sys.path to import trace_convert
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import trace_convert

# Disable logging output during tests to keep console clean
logging.disable(logging.CRITICAL)


class TestTran(unittest.TestCase):
    """Test tran function - timestamp conversion"""

    def test_tran_valid_integer(self):
        self.assertEqual(trace_convert.tran(1000000), "1000.000")

    def test_tran_valid_float(self):
        self.assertEqual(trace_convert.tran(1234567.89), "1234.568")

    def test_tran_zero(self):
        self.assertEqual(trace_convert.tran(0), "0.000")

    def test_tran_invalid_string(self):
        self.assertEqual(trace_convert.tran("invalid"), "0")

    def test_tran_invalid_none(self):
        self.assertEqual(trace_convert.tran(None), "0")


class TestIsKernelProcess(unittest.TestCase):
    """Test is_kernel_process function"""

    def test_migration_process(self):
        self.assertTrue(trace_convert.is_kernel_process("migration/0"))

    def test_swapper_process(self):
        self.assertTrue(trace_convert.is_kernel_process("swapper/1"))

    def test_kworker_process(self):
        self.assertTrue(trace_convert.is_kernel_process("kworker/0:1"))

    def test_normal_process(self):
        self.assertFalse(trace_convert.is_kernel_process("python"))

    def test_empty_string(self):
        self.assertFalse(trace_convert.is_kernel_process(""))

    def test_invalid_type(self):
        self.assertFalse(trace_convert.is_kernel_process(None))

    def test_partial_match(self):
        self.assertTrue(trace_convert.is_kernel_process("kworker_u:0"))


class TestTraceEventHelpers(unittest.TestCase):
    """Test get_trace_event and get_meta_event functions"""

    def test_get_trace_event_basic(self):
        result = trace_convert.get_trace_event("test", "pid1", "tid1", 1000, 500)
        self.assertEqual(result["name"], "test")
        self.assertEqual(result["ph"], "X")
        self.assertEqual(result["pid"], "pid1")
        self.assertEqual(result["tid"], "tid1")
        self.assertEqual(result["ts"], 1000)
        self.assertEqual(result["dur"], 500)

    def test_get_trace_event_with_args(self):
        args = {"key": "value", "num": 123}
        result = trace_convert.get_trace_event("test", "pid1", "tid1", 1000, 500, args)
        self.assertEqual(result["args"]["key"], "value")
        self.assertEqual(result["args"]["num"], 123)

    def test_get_trace_event_without_args(self):
        result = trace_convert.get_trace_event("test", "pid1", "tid1", 1000, 500, None)
        self.assertNotIn("args", result)

    def test_get_meta_event(self):
        result = trace_convert.get_meta_event("pid1", "tid1")
        self.assertEqual(result["name"], "process_name")
        self.assertEqual(result["ph"], "M")
        self.assertEqual(result["pid"], "pid1")
        self.assertEqual(result["tid"], "tid1")
        self.assertEqual(result["args"]["name"], "pid1")


class TestInterruptEvent(unittest.TestCase):
    """Test InterruptEvent class"""

    def test_init(self):
        event = trace_convert.InterruptEvent("irq", 1000, "0", irq=12)
        self.assertEqual(event.comm, "irq")
        self.assertEqual(event.st, 1000)
        self.assertEqual(event.cpu, "CPU 0")
        self.assertEqual(event.dur, 0)
        self.assertEqual(event.kwargs, {"irq": 12})

    def test_to_event_json(self):
        event = trace_convert.InterruptEvent("softirq", 1000, "1", vec=1, action="TIMER")
        event.dur = 500
        event.et = 1500
        result = event.to_event_json()
        self.assertEqual(result["name"], "softirq")
        self.assertEqual(result["pid"], trace_convert.CPU_SCHED_PID)
        self.assertEqual(result["tid"], "CPU 1")
        self.assertEqual(result["args"]["vec"], 1)
        self.assertEqual(result["args"]["action"], "TIMER")


class TestCompleteEvent(unittest.TestCase):
    """Test CompleteEvent class"""

    def test_init(self):
        event = trace_convert.CompleteEvent("python", "123", 1000, "0", 120)
        self.assertEqual(event.comm, "python")
        self.assertEqual(event.pid, "123")
        self.assertEqual(event.st, 1000)
        self.assertEqual(event.cpu, "CPU 0")
        self.assertEqual(event.prio, 120)
        self.assertEqual(event.total_runtime, 0)

    def test_update_runtime(self):
        event = trace_convert.CompleteEvent("python", "123", 1000, "0", 120)
        event.update_runtime("1000 ns", "12345")
        self.assertEqual(event.total_runtime, 1000)
        self.assertEqual(event.vruntime, "12345")

    def test_update_runtime_none(self):
        event = trace_convert.CompleteEvent("python", "123", 1000, "0", 120)
        event.update_runtime(None, None)
        self.assertEqual(event.total_runtime, 0)
        self.assertEqual(event.vruntime, 0)

    def test_end(self):
        event = trace_convert.CompleteEvent("python", "123", 1000, "0", 120)
        event.end(2000, "S", 100)
        self.assertEqual(event.dur, 1000)
        self.assertEqual(event.end_state, "S")
        self.assertEqual(event.prio, 100)

    def test_to_event_json(self):
        event = trace_convert.CompleteEvent("python", "123", 1000, "0", 120)
        event.total_runtime = 500000
        event.vruntime = "1000"
        event.end_state = "S"
        event.dur = 1000
        result = event.to_event_json()
        self.assertEqual(result["name"], "python:123")
        self.assertEqual(result["args"]["host_pid"], "123")
        self.assertEqual(result["args"]["end_state"], "S")


class TestProcess(unittest.TestCase):
    """Test Process class"""

    def test_init(self):
        process = trace_convert.Process("python", "123", 1000)
        self.assertEqual(process.pid, "123")
        self.assertEqual(process.comm, "python")
        self.assertEqual(process.process_name, "python:123")
        self.assertEqual(process.state, 'W')

    def test_state_transitions(self):
        process = trace_convert.Process("python", "123", 1000)
        self.assertEqual(process.state, 'W')  # Initial state
        process.wakeup(1100)
        self.assertEqual(process.state, 'W')
        process.run(1200)
        self.assertEqual(process.state, 'R')
        process.sleep(1300)
        self.assertEqual(process.state, 'S')

    def test_get_event_name(self):
        process = trace_convert.Process("python", "123", 1000)
        process.state = 'W'
        self.assertEqual(process.get_event_name(), 'Runnable')
        process.state = 'R'
        self.assertEqual(process.get_event_name(), 'Running')
        process.state = 'S'
        self.assertEqual(process.get_event_name(), 'Sleeping')

    def test_add_process_meta(self):
        process = trace_convert.Process("python", "123", 1000)
        self.assertEqual(len(process.events), 1)
        self.assertEqual(process.events[0]["name"], "process_name")


class TestPidTran(unittest.TestCase):
    """Test PidTran singleton class"""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        # Reset singleton instance
        trace_convert.PidTran._instances = {}
        self.pid_tran = trace_convert.PidTran()
        self.pid_tran.pid_status = None
        self.pid_tran.pid_mapping_path = None

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def test_singleton(self):
        instance1 = trace_convert.PidTran()
        instance2 = trace_convert.PidTran()
        self.assertIs(instance1, instance2)

    def test_get_ns_pid_no_mapping(self):
        self.pid_tran.pid_status = None
        self.assertEqual(self.pid_tran.get_ns_pid("123"), "123")

    def test_get_ns_pid_with_mapping(self):
        mapping_path = os.path.join(self.temp_dir, "pid_mapping.json")
        mapping = {"123": {"NSpid": "456"}}
        with open(mapping_path, 'w', encoding='utf-8') as f:
            json.dump(mapping, f)

        self.pid_tran.initialize(mapping_path)
        self.assertEqual(self.pid_tran.get_ns_pid("123"), "456")

    def test_load_pid_mapping_file_not_found(self):
        self.assertFalse(self.pid_tran.load_pid_mapping("/nonexistent/path.json"))

    def test_load_pid_mapping_invalid_json(self):
        invalid_path = os.path.join(self.temp_dir, "invalid.json")
        with open(invalid_path, 'w', encoding='utf-8') as f:
            f.write("invalid json content")
        self.assertFalse(self.pid_tran.load_pid_mapping(invalid_path))


class TestTimeStampTran(unittest.TestCase):
    """Test TimeStampTran singleton class"""

    def setUp(self):
        # Reset singleton instance
        trace_convert.TimeStampTran._instances = {}
        self.ts_tran = trace_convert.TimeStampTran()

    def test_singleton(self):
        instance1 = trace_convert.TimeStampTran()
        instance2 = trace_convert.TimeStampTran()
        self.assertIs(instance1, instance2)

    def test_initialize_none(self):
        self.ts_tran.initialize(None)
        self.assertEqual(self.ts_tran.mono_raw_start, 0)
        self.assertEqual(self.ts_tran.utc_start_timestamp, 0)

    def test_str_to_int_with_decimal(self):
        # 123456.789 -> (123456 * 1000000 + 789) * 1000 = 123456000789000
        result = self.ts_tran._TimeStampTran__str_to_int("123456.789")
        self.assertEqual(result, 123456000789000)

    def test_str_to_int_no_decimal(self):
        # 123456 -> 123456 * 1000000 = 123456000000
        result = self.ts_tran._TimeStampTran__str_to_int("123456")
        self.assertEqual(result, 123456000000)

    def test_str_to_int_invalid(self):
        self.assertEqual(self.ts_tran._TimeStampTran__str_to_int("invalid"), 0)


class TestInterruptFtraceParse(unittest.TestCase):
    """Test InterruptFtraceParse class"""

    def setUp(self):
        # Reset singleton instances
        trace_convert.PidTran._instances = {}
        trace_convert.TimeStampTran._instances = {}
        trace_convert.PidTran().pid_status = None
        trace_convert.TimeStampTran().initialize(None)
        self.parser = trace_convert.InterruptFtraceParse(file_type='txt')

    def test_belong_irq_event(self):
        line = "task-123 [0] .... 123456.789: irq_handler_entry: irq=12"
        self.assertIsNotNone(self.parser.belong(line))

    def test_belong_softirq_event(self):
        line = "task-123 [0] .... 123456.789: softirq_entry: vec=1 [action=TIMER]"
        self.assertIsNotNone(self.parser.belong(line))

    def test_belong_non_irq_event(self):
        line = "task-123 [0] .... 123456.789: sched_switch: prev_comm=python"
        self.assertIsNone(self.parser.belong(line))

    def test_parse_softirq_param(self):
        result = self.parser.parse_softirq_param("vec=1 [action=TIMER]")
        self.assertEqual(result["vec"], "1")
        self.assertEqual(result["action"], "TIMER")

    def test_parse_softirq_param_invalid(self):
        self.assertIsNone(self.parser.parse_softirq_param("invalid format"))

    def test_get_result_empty(self):
        self.assertEqual(self.parser.get_result(), [])


class TestSchedFtraceParse(unittest.TestCase):
    """Test SchedFtraceParse class"""

    def setUp(self):
        # Reset singleton instances
        trace_convert.PidTran._instances = {}
        trace_convert.TimeStampTran._instances = {}
        trace_convert.PidTran().pid_status = None
        trace_convert.TimeStampTran().initialize(None)
        self.parser = trace_convert.SchedFtraceParse(file_type='txt')

    def test_belong_sched_event(self):
        line = "task-123 [0] .... 123456.789: sched_switch: prev_comm=python prev_pid=123"
        self.assertIsNotNone(self.parser.belong(line))

    def test_belong_non_sched_event(self):
        line = "task-123 [0] .... 123456.789: irq_handler_entry: irq=12"
        self.assertIsNone(self.parser.belong(line))

    def test_parse_switch_sched_param(self):
        args = "python:123 [120] R ==> bash:456 [100]"
        result = self.parser.parse_switch_sched_param(args)
        self.assertEqual(result["prev_comm"], "python")
        self.assertEqual(result["prev_pid"], "123")
        self.assertEqual(result["prev_state"], "R")
        self.assertEqual(result["next_comm"], "bash")
        self.assertEqual(result["next_pid"], "456")

    def test_parse_switch_sched_param_invalid(self):
        self.assertIsNone(self.parser.parse_switch_sched_param("invalid format"))

    def test_parse_weakup_sched_param(self):
        args = "python:123 [120] success=1 CPU:0"
        result = self.parser.parse_weakup_sched_param(args)
        self.assertEqual(result["comm"], "python")
        self.assertEqual(result["pid"], "123")
        self.assertEqual(result["prio"], "120")
        self.assertEqual(result["cpu"], "0")

    def test_parse_base_param(self):
        args = "pid=123 comm=python prio=120"
        result = self.parser.parse_base_param(args)
        self.assertEqual(result["pid"], "123")
        self.assertEqual(result["comm"], "python")
        self.assertEqual(result["prio"], "120")

    def test_get_result_not_empty(self):
        result = self.parser.get_result()
        self.assertTrue(len(result) >= 1)


class TestTimeFilterResult(unittest.TestCase):
    """Test TimeFilterResult constants"""

    def test_constants(self):
        self.assertEqual(trace_convert.TimeFilterResult.OK, 0)
        self.assertEqual(trace_convert.TimeFilterResult.BEFORE_START, 1)
        self.assertEqual(trace_convert.TimeFilterResult.AFTER_END, 2)


class TestTraceConverterE2E(unittest.TestCase):
    """End-to-end tests for TraceConverter class using test data files"""

    TEST_DATA_DIR = os.path.join(os.path.dirname(__file__), 'test-data')

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        # Reset singleton instances before each test
        trace_convert.PidTran._instances = {}
        trace_convert.TimeStampTran._instances = {}
        trace_convert.PidTran().pid_status = None
        trace_convert.PidTran().pid_mapping_path = None
        trace_convert.TimeStampTran().mono_raw_start = None
        trace_convert.TimeStampTran().mono_raw_end = None
        trace_convert.TimeStampTran().utc_start_timestamp = None

    def tearDown(self):
        shutil.rmtree(self.temp_dir)

    def test_parse_basic_trace_file(self):
        """Test parsing a basic trace.txt file without profiling data"""
        trace_file = os.path.join(self.TEST_DATA_DIR, 'trace.txt')
        if not os.path.exists(trace_file):
            self.skipTest("Test data file not found")

        # Initialize without profiling data
        trace_convert.PidTran().initialize(None)
        trace_convert.TimeStampTran().initialize(None)

        converter = trace_convert.TraceConverter(trace_file)
        result = converter.parse()

        # Verify result contains events
        self.assertTrue(len(result) > 0)

        # Verify result structure - should contain process scheduling meta event
        meta_events = [e for e in result if e.get('ph') == 'M']
        self.assertTrue(len(meta_events) >= 1)

    def test_parse_with_pid_mapping(self):
        """Test parsing trace file with PID mapping"""
        trace_file = os.path.join(self.TEST_DATA_DIR, 'trace.txt')
        pid_mapping = os.path.join(self.TEST_DATA_DIR, 'pid_mapping.json')
        if not os.path.exists(trace_file) or not os.path.exists(pid_mapping):
            self.skipTest("Test data files not found")

        # Reset and reinitialize singletons
        trace_convert.PidTran._instances = {}
        trace_convert.TimeStampTran._instances = {}
        trace_convert.TimeStampTran().initialize(None)

        converter = trace_convert.TraceConverter(trace_file, pid_mapping=pid_mapping)
        result = converter.parse()

        # Verify events were parsed
        self.assertTrue(len(result) > 0)

    def test_parse_with_profiling_data(self):
        """Test parsing trace file with profiling data for time alignment"""
        trace_file = os.path.join(self.TEST_DATA_DIR, 'trace.txt')
        profiling_data = os.path.join(self.TEST_DATA_DIR, 'profiling_data')
        if not os.path.exists(trace_file) or not os.path.exists(profiling_data):
            self.skipTest("Test data files not found")

        converter = trace_convert.TraceConverter(trace_file, profiling_data=profiling_data)
        result = converter.parse()

        # Verify events were parsed
        self.assertTrue(len(result) > 0)

    def test_parse_with_non_overlapping_profiling_data_logs_warning(self):
        """Test warning when ftrace and profiling time windows do not overlap"""
        trace_file = os.path.join(self.TEST_DATA_DIR, 'trace.txt')
        if not os.path.exists(trace_file):
            self.skipTest("Test data file not found")

        profiling_data = os.path.join(self.temp_dir, 'profiling_data')
        os.makedirs(profiling_data)
        with open(os.path.join(profiling_data, 'start_info'), 'w', encoding='utf-8') as f:
            json.dump({"clockMonotonicRaw": "223456000000000", "collectionTimeBegin": "223456000"}, f)
        with open(os.path.join(profiling_data, 'end_info'), 'w', encoding='utf-8') as f:
            json.dump({"clockMonotonicRaw": "223457000000000", "collectionTimeBegin": "223457000"}, f)

        original_disable_level = logging.getLogger().manager.disable
        logging.disable(logging.NOTSET)
        self.addCleanup(logging.disable, original_disable_level)
        converter = trace_convert.TraceConverter(trace_file, profiling_data=profiling_data)

        with self.assertLogs(level='WARNING') as logs:
            converter.parse()

        self.assertTrue(any("No ftrace events are within the profiling time window" in log for log in logs.output))
        self.assertEqual(trace_convert.TimeStampTran().ftrace_in_window_count, 0)
        self.assertGreater(trace_convert.TimeStampTran().ftrace_before_start_count, 0)

    def test_export_to_json(self):
        """Test exporting parsed data to JSON format"""
        trace_file = os.path.join(self.TEST_DATA_DIR, 'trace.txt')
        if not os.path.exists(trace_file):
            self.skipTest("Test data file not found")

        output_path = os.path.join(self.temp_dir, 'output.json')

        from exporters import JsonExport

        converter = trace_convert.TraceConverter(trace_file)
        converter.export(JsonExport(), output_path)

        # Verify output file was created
        self.assertTrue(os.path.exists(output_path))

        # Verify output file is valid JSON
        with open(output_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        self.assertTrue(len(data) > 0)

    def test_sched_events_parsed(self):
        """Test that sched_switch events are properly parsed"""
        trace_file = os.path.join(self.TEST_DATA_DIR, 'trace.txt')
        if not os.path.exists(trace_file):
            self.skipTest("Test data file not found")

        # Initialize singletons
        trace_convert.PidTran().initialize(None)
        trace_convert.TimeStampTran().initialize(None)

        converter = trace_convert.TraceConverter(trace_file)
        result = converter.parse()

        # Verify CPU Scheduling events exist
        cpu_sched_events = [e for e in result if e.get('pid') == trace_convert.CPU_SCHED_PID]
        self.assertTrue(len(cpu_sched_events) > 0)

    def test_process_scheduling_events_parsed(self):
        """Test that Process Scheduling events are properly parsed"""
        trace_file = os.path.join(self.TEST_DATA_DIR, 'trace.txt')
        if not os.path.exists(trace_file):
            self.skipTest("Test data file not found")

        # Initialize singletons
        trace_convert.PidTran().initialize(None)
        trace_convert.TimeStampTran().initialize(None)

        converter = trace_convert.TraceConverter(trace_file)
        result = converter.parse()

        # Verify Process Scheduling meta event exists
        process_sched_meta = [
            e for e in result if e.get('pid') == trace_convert.PROCESS_SCHED_PID and e.get('ph') == 'M'
        ]
        self.assertTrue(len(process_sched_meta) >= 1)

    def test_kernel_process_filtered(self):
        """Test that kernel processes are properly filtered out"""
        trace_file = os.path.join(self.TEST_DATA_DIR, 'trace.txt')
        if not os.path.exists(trace_file):
            self.skipTest("Test data file not found")

        # Initialize singletons
        trace_convert.PidTran().initialize(None)
        trace_convert.TimeStampTran().initialize(None)

        converter = trace_convert.TraceConverter(trace_file)
        result = converter.parse()

        # Verify no kernel process names in Process Scheduling events
        process_events = [e for e in result if e.get('pid') == trace_convert.PROCESS_SCHED_PID]
        for event in process_events:
            name = event.get('name', '')
            if name and name not in ['Process Scheduling', 'process_name']:
                self.assertFalse(trace_convert.is_kernel_process(name))


if __name__ == '__main__':
    unittest.main()
