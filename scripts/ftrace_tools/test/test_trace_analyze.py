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

import unittest
import os
import tempfile
import sqlite3
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from trace_analyze import parse_tid, parse_cpu_from_tid, TaskStats, DB_PATH_DEFAULT, build_arg_parser


class TestDefaultPath(unittest.TestCase):
    """测试 --input 默认路径逻辑"""

    def test_default_path_points_to_sibling_ftrace_data_db(self):
        """默认值应指向脚本同目录下的 ftrace_data.db"""
        script_dir = os.path.dirname(os.path.abspath(__file__))
        # trace_analyze.py 在 scripts/ftrace_tools/ 下，测试文件在 scripts/ftrace_tools/test/ 下
        expected = os.path.join(os.path.dirname(script_dir), "ftrace_data.db")
        self.assertEqual(DB_PATH_DEFAULT, expected)
        self.assertTrue(DB_PATH_DEFAULT.endswith("ftrace_data.db"))

    def test_arg_parser_default_input(self):
        """argparse 的 --input 默认值应为 DB_PATH_DEFAULT"""
        parser = build_arg_parser()
        args = parser.parse_args([])
        self.assertEqual(args.input, DB_PATH_DEFAULT)

    def test_arg_parser_custom_input(self):
        """--input 可以覆盖默认值"""
        parser = build_arg_parser()
        args = parser.parse_args(['-i', '/custom/path/data.db'])
        self.assertEqual(args.input, '/custom/path/data.db')


class TestParseTid(unittest.TestCase):
    """测试 parse_tid 函数解析 comm:pid 格式"""

    def test_normal_comm_pid(self):
        """正常格式: comm:pid"""
        comm, pid = parse_tid("trace-cmd:3718847")
        self.assertEqual(comm, "trace-cmd")
        self.assertEqual(pid, 3718847)

    def test_comm_with_colon(self):
        """comm 中包含冒号: WatchParentPid::2581884"""
        comm, pid = parse_tid("WatchParentPid::2581884")
        self.assertEqual(comm, "WatchParentPid:")
        self.assertEqual(pid, 2581884)

    def test_kernel_idle(self):
        """内核 idle 进程: <idle>:0"""
        comm, pid = parse_tid("<idle>:0")
        self.assertEqual(comm, "<idle>")
        self.assertEqual(pid, 0)

    def test_cpu_thread(self):
        """CPU 线程: CPU 000 — 应该返回原始 tid"""
        comm, pid = parse_tid("CPU 000")
        self.assertEqual(comm, "CPU 000")
        self.assertIsNone(pid)

    def test_empty_string(self):
        """空字符串"""
        comm, pid = parse_tid("")
        self.assertEqual(comm, "")
        self.assertIsNone(pid)

    def test_no_colon(self):
        """没有冒号的字符串"""
        comm, pid = parse_tid("single_name")
        self.assertEqual(comm, "single_name")
        self.assertIsNone(pid)


class TestParseCpuFromTid(unittest.TestCase):
    """测试 parse_cpu_from_tid 函数提取 CPU 编号"""

    def test_cpu_with_leading_zeros(self):
        """CPU 000 -> 0"""
        self.assertEqual(parse_cpu_from_tid("CPU 000"), 0)
        self.assertEqual(parse_cpu_from_tid("CPU 007"), 7)
        self.assertEqual(parse_cpu_from_tid("CPU 031"), 31)

    def test_cpu_no_padding(self):
        """CPU 0 -> 0, CPU 15 -> 15"""
        self.assertEqual(parse_cpu_from_tid("CPU 0"), 0)
        self.assertEqual(parse_cpu_from_tid("CPU 15"), 15)

    def test_non_cpu_tid(self):
        """进程 tid 应返回 None"""
        self.assertIsNone(parse_cpu_from_tid("trace-cmd:3718847"))
        self.assertIsNone(parse_cpu_from_tid("<idle>:0"))
        self.assertIsNone(parse_cpu_from_tid("WatchParentPid::2581884"))

    def test_empty_and_none(self):
        """空字符串返回 None"""
        self.assertIsNone(parse_cpu_from_tid(""))
        self.assertIsNone(parse_cpu_from_tid("CPU "))


class TestTaskStats(unittest.TestCase):
    """测试 TaskStats 数据模型（含 cpu_id 维度，ns 存储）"""

    def test_default_values(self):
        """测试默认值初始化"""
        stats = TaskStats("test_proc", 1234, cpu_id=0)
        self.assertEqual(stats.comm, "test_proc")
        self.assertEqual(stats.pid, 1234)
        self.assertEqual(stats.cpu_id, 0)
        self.assertEqual(stats.running_ns, 0)
        self.assertEqual(stats.sleeping_ns, 0)
        self.assertEqual(stats.runnable_ns, 0)
        self.assertEqual(stats.cs_count, 0)
        self.assertIsInstance(stats.irqs, dict)
        self.assertEqual(len(stats.irqs), 0)

    def test_cpu_id_none(self):
        """cpu_id=None 表示未关联到具体 CPU"""
        stats = TaskStats("proc", 1, cpu_id=None)
        self.assertIsNone(stats.cpu_id)

    def test_add_duration_ns(self):
        """测试添加纳秒级 duration（直接存 ns，不转换）"""
        stats = TaskStats("proc", 1, cpu_id=0)
        stats.add_duration_ns("Running", 1_000_000_000)  # 1e9 ns
        stats.add_duration_ns("Running", 500_000_000)  # 5e8 ns
        self.assertEqual(stats.running_ns, 1_500_000_000)

    def test_add_duration_ns_sleeping(self):
        stats = TaskStats("proc", 1, cpu_id=0)
        stats.add_duration_ns("Sleeping", 2_000_000_000)
        self.assertEqual(stats.sleeping_ns, 2_000_000_000)

    def test_add_duration_ns_runnable(self):
        stats = TaskStats("proc", 1, cpu_id=0)
        stats.add_duration_ns("Runnable", 3_000_000_000)
        self.assertEqual(stats.runnable_ns, 3_000_000_000)

    def test_add_irq(self):
        """测试添加 IRQ 统计（ns 存储）"""
        stats = TaskStats("proc", 1, cpu_id=0)
        stats.add_irq("irq", "eth0", 1_000_000)
        stats.add_irq("irq", "eth0", 2_000_000)
        stats.add_irq("irq", "timer", 500_000)

        self.assertIn("irq:eth0", stats.irqs)
        self.assertIn("irq:timer", stats.irqs)
        self.assertEqual(stats.irqs["irq:eth0"]["time_ns"], 3_000_000)
        self.assertEqual(stats.irqs["irq:eth0"]["count"], 2)
        self.assertEqual(stats.irqs["irq:timer"]["time_ns"], 500_000)
        self.assertEqual(stats.irqs["irq:timer"]["count"], 1)


class TestDBConnection(unittest.TestCase):
    """测试 SQLite 连接管理"""

    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.addCleanup(self.tmpdir.cleanup)
        self.db_path = os.path.join(self.tmpdir.name, "test.db")

    def tearDown(self):
        pass

    def test_open_nonexistent_db(self):
        """打开不存在的 db 文件应报错"""
        from trace_analyze import open_db

        with self.assertRaises(FileNotFoundError):
            open_db("/nonexistent/path/test.db")

    def test_open_valid_db(self):
        """打开有效的 db 文件应返回连接"""
        from trace_analyze import open_db

        # Create a minimal test db
        conn = sqlite3.connect(self.db_path)
        conn.execute("CREATE TABLE test (id INTEGER PRIMARY KEY)")
        conn.commit()
        conn.close()

        db_conn = open_db(self.db_path)
        self.assertIsNotNone(db_conn)
        cur = db_conn.cursor()
        cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='test'")
        self.assertIsNotNone(cur.fetchone())
        db_conn.close()


if __name__ == '__main__':
    unittest.main()
