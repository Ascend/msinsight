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
import json
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from trace_analyze import (
    parse_tid,
    parse_cpu_from_tid,
    TaskStats,
    DB_PATH_DEFAULT,
    build_arg_parser,
    compute_running_sleeping_runnable_stats,
    open_db,
)


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
        self.db_path = os.path.join(self.tmpdir.name, "test.db")

    def tearDown(self):
        self.tmpdir.cleanup()

    def test_open_nonexistent_db(self):
        """打开不存在的 db 文件应报错"""

        with self.assertRaises(FileNotFoundError):
            open_db("/nonexistent/path/test.db")

    def test_open_valid_db(self):
        """打开有效的 db 文件应返回连接"""

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


class TestComputeRunningSleepingRunnable(unittest.TestCase):
    """测试 running/sleeping/runnable 时间统计（含 CPU 维度）"""

    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
        self.db_path = os.path.join(self.tmpdir.name, "test.db")

    def tearDown(self):
        self.tmpdir.cleanup()

    def _create_test_db(self, threads, slices_with_args):
        """创建测试 db。
        threads: list of (track_id, tid)
        slices_with_args: list of (name, timestamp, duration, track_id, args_dict_or_None)
        """
        conn = sqlite3.connect(self.db_path)
        conn.execute("CREATE TABLE thread (track_id INTEGER PRIMARY KEY, tid TEXT)")
        conn.execute(
            "CREATE TABLE slice (id INTEGER PRIMARY KEY, name TEXT, timestamp INTEGER, duration INTEGER, track_id INTEGER, args TEXT DEFAULT NULL)"
        )
        for track_id, tid in threads:
            conn.execute("INSERT INTO thread (track_id, tid) VALUES (?, ?)", (track_id, tid))
        for i, (name, ts, dur, track_id, args) in enumerate(slices_with_args):
            args_json = json.dumps(args) if args else None
            conn.execute(
                "INSERT INTO slice (id, name, timestamp, duration, track_id, args) VALUES (?, ?, ?, ?, ?, ?)",
                (i + 1, name, ts, dur, track_id, args_json),
            )
        conn.commit()
        conn.close()

    def test_basic_running_stats_with_cpu_in_args(self):
        """基本 Running 事件统计，cpu 来自 args"""
        self._create_test_db(
            threads=[(1, "node:3501822")],
            slices_with_args=[
                ("Running", 1000, 500_000, 1, {"cpu": 3}),
                ("Running", 2000, 300_000, 1, {"cpu": 3}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = compute_running_sleeping_runnable_stats(conn)
        conn.close()

        self.assertEqual(len(stats_map), 1)
        key = ("node", 3501822, 3)
        self.assertIn(key, stats_map)
        self.assertEqual(stats_map[key].running_ns, 800_000)

    def test_multiple_event_types_with_cpu(self):
        """Running + Sleeping + Runnable 混合"""
        self._create_test_db(
            threads=[(1, "worker:100")],
            slices_with_args=[
                ("Running", 1000, 1_000_000, 1, {"cpu": 5}),
                ("Sleeping", 2000, 2_000_000, 1, {"cpu": 5}),
                ("Runnable", 3000, 500_000, 1, {"cpu": 5}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = compute_running_sleeping_runnable_stats(conn)
        conn.close()

        key = ("worker", 100, 5)
        self.assertIn(key, stats_map)
        self.assertEqual(stats_map[key].running_ns, 1_000_000)
        self.assertEqual(stats_map[key].sleeping_ns, 2_000_000)
        self.assertEqual(stats_map[key].runnable_ns, 500_000)

    def test_no_cpu_in_args_defaults_to_none(self):
        """args 中没有 cpu 字段时，cpu_id 应为 None"""
        self._create_test_db(
            threads=[(1, "orphan:999")],
            slices_with_args=[
                ("Running", 1000, 100_000, 1, None),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = compute_running_sleeping_runnable_stats(conn)
        conn.close()

        key = ("orphan", 999, None)
        self.assertIn(key, stats_map)
        self.assertEqual(stats_map[key].running_ns, 100_000)

    def test_different_cpu_values_split_groups(self):
        """同一 tid 不同 cpu 的事件应分到不同的 TaskStats"""
        self._create_test_db(
            threads=[(1, "migrator:42")],
            slices_with_args=[
                ("Running", 1000, 500_000, 1, {"cpu": 0}),
                ("Running", 2000, 300_000, 1, {"cpu": 1}),
                ("Running", 3000, 200_000, 1, {"cpu": 0}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = compute_running_sleeping_runnable_stats(conn)
        conn.close()

        # 应该有 2 条记录：cpu=0 和 cpu=1
        self.assertEqual(len(stats_map), 2)
        self.assertEqual(stats_map[("migrator", 42, 0)].running_ns, 700_000)
        self.assertEqual(stats_map[("migrator", 42, 1)].running_ns, 300_000)


if __name__ == '__main__':
    unittest.main()
