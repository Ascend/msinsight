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
import sys
import os

# Add parent directory to sys.path to import exporters
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import unittest
import os
import json
import sqlite3
import tempfile
import logging
from exporters import JsonExport, DbExport

# Disable logging output during tests to keep console clean
logging.disable(logging.CRITICAL)

class TestJsonExport(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.output_path = os.path.join(self.temp_dir.name, "test_output.json")
        self.exporter = JsonExport()
        self.sample_data = [
            {"name": "event1", "ph": "X", "ts": 1000},
            {"name": "event2", "ph": "M", "args": {"key": "value"}}
        ]

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_export_success(self):
        self.exporter.export(self.sample_data, self.output_path)
        self.assertTrue(os.path.exists(self.output_path))
        
        with open(self.output_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            
        self.assertEqual(len(data), 2)
        self.assertEqual(data[0]["name"], "event1")
        self.assertEqual(data[1]["args"]["key"], "value")

    def test_export_io_error(self):
        # Provide an invalid path (e.g., directory that doesn't exist)
        invalid_path = os.path.join(self.temp_dir.name, "nonexistent_dir", "test.json")
        with self.assertRaises(IOError):
            self.exporter.export(self.sample_data, invalid_path)

class TestDbExport(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.output_path = os.path.join(self.temp_dir.name, "test_output.db")
        self.exporter = DbExport()
        
        self.sample_data = [
            # Note: The logic for thread extraction now relies on ph='X' events
            {
                "ph": "X",
                "pid": "CPU Scheduling",
                "tid": "CPU 0",
                "ts": "1.5",
                "dur": "0.5",
                "name": "CPU Task",
                "args": {"irq": "12"}
            },
            {
                "ph": "X",
                "pid": "Process Scheduling",
                "tid": "myapp:123",
                "ts": 2.0,
                "dur": 1.5,
                "name": "Process Task",
                "args": {}
            },
            # Event without track_id matching or missing info
            {
                "ph": "X",
                "pid": "Unknown",
                "tid": "unknown_tid",
                "ts": 3.0,
                "dur": 1.0,
                "name": "Unknown Task"
            },
            # Edge case with overflow/invalid values
            {
                "ph": "X",
                "pid": "Process Scheduling",
                "tid": "myapp:123",
                "ts": "invalid_ts",
                "dur": None,
                "name": "Invalid Task"
            }
        ]

    def tearDown(self):
        self.temp_dir.cleanup()

    def test_export_success(self):
        self.exporter.export(self.sample_data, self.output_path)
        self.assertTrue(os.path.exists(self.output_path))
        
        conn = sqlite3.connect(self.output_path)
        cursor = conn.cursor()
        
        # 1. Test process table
        cursor.execute("SELECT * FROM process ORDER BY pid")
        processes = cursor.fetchall()
        self.assertEqual(len(processes), 2)
        self.assertEqual(processes[0][0], "CPU Scheduling")
        self.assertEqual(processes[1][0], "Process Scheduling")
        
        # 2. Test thread table
        cursor.execute("SELECT tid, pid, thread_name, thread_sort_index FROM thread ORDER BY track_id")
        threads = cursor.fetchall()
        self.assertEqual(len(threads), 2)
        # CPU Scheduling is sorted first
        self.assertEqual(threads[0], ("CPU 0", "CPU Scheduling", "CPU 0", 0))
        self.assertEqual(threads[1], ("myapp:123", "Process Scheduling", "myapp:123", 0))
        
        # 3. Test slice table
        cursor.execute("SELECT timestamp, duration, name, args, end_time FROM slice ORDER BY timestamp")
        slices = cursor.fetchall()
        # The unknown_tid won't be mapped because it wasn't added to thread table
        self.assertEqual(len(slices), 3)
        
        # First slice: the invalid one (timestamp becomes 0)
        self.assertEqual(slices[0][0], 0)
        self.assertEqual(slices[0][1], 0)
        self.assertEqual(slices[0][2], "Invalid Task")
        self.assertIsNone(slices[0][3])
        self.assertEqual(slices[0][4], 0)
        
        # Second slice: CPU Task (ts=1.5 -> 1500, dur=0.5 -> 500)
        self.assertEqual(slices[1][0], 1500)
        self.assertEqual(slices[1][1], 500)
        self.assertEqual(slices[1][2], "CPU Task")
        self.assertEqual(slices[1][3], '{"irq": "12"}')
        self.assertEqual(slices[1][4], 2000)
        
        # Third slice: Process Task (ts=2.0 -> 2000, dur=1.5 -> 1500)
        self.assertEqual(slices[2][0], 2000)
        self.assertEqual(slices[2][1], 1500)
        self.assertEqual(slices[2][2], "Process Task")
        self.assertIsNone(slices[2][3]) # empty args {} becomes NULL
        self.assertEqual(slices[2][4], 3500)

        # 4. Test counter table
        cursor.execute("SELECT * FROM counter")
        counters = cursor.fetchall()
        self.assertEqual(len(counters), 0) # Counter is empty as expected
        
        conn.close()

    def test_export_db_error_handling(self):
        # Test providing an invalid path to trigger sqlite3.Error / Exception
        invalid_path = os.path.join(self.temp_dir.name, "nonexistent_dir", "test.db")
        with self.assertRaises(sqlite3.OperationalError):
            self.exporter.export(self.sample_data, invalid_path)


if __name__ == '__main__':
    unittest.main()
