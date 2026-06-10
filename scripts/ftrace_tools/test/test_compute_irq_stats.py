"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

         http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

# pylint: disable=duplicate-code
import os
import shutil
import sqlite3
import json
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
from trace_analyze import (
    TaskStats,
    open_db,
    compute_irq_stats,
    _add_irq_to_entry,
)


class TestComputeIrqStats(unittest.TestCase):
    """测试 IRQ 耗时与次数统计 — 进程视角（子任务4）"""

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.db_path = os.path.join(self.tmpdir, "test.db")

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

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

    def _make_stats_map(self, keys):
        """根据 keys 列表创建空的 stats_map"""
        result = {}
        for comm, pid, cpu_id in keys:
            result[(comm, pid, cpu_id)] = TaskStats(comm=comm, pid=pid, cpu_id=cpu_id)
        return result

    def test_basic_irq_stats_by_process(self):
        """基本的 irq 事件统计，按被中断的进程 + irq_name 分组"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "bash:100")],
            slices_with_args=[
                ("irq", 1000, 5000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "bash:100"}),
                ("irq", 2000, 3000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "bash:100"}),
                ("irq", 3000, 2000, 1, {"irq": "2", "name": "timer", "ret": "handled", "task": "bash:100"}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("bash", 100, 0)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        self.assertIn("irq:eth0", stats_map[("bash", 100, 0)].irqs)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["irq:eth0"]["count"], 2)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["irq:eth0"]["time_ns"], 8000)
        self.assertIn("irq:timer", stats_map[("bash", 100, 0)].irqs)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["irq:timer"]["count"], 1)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["irq:timer"]["time_ns"], 2000)

    def test_irq_different_processes_same_cpu(self):
        """同一 CPU 上不同进程被中断，irq 统计应分别记录"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "bash:100"), (3, "node:200")],
            slices_with_args=[
                ("irq", 1000, 5000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "bash:100"}),
                ("irq", 2000, 3000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "node:200"}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("bash", 100, 0), ("node", 200, 0)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        # bash 只有 1 次 eth0
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["irq:eth0"]["count"], 1)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["irq:eth0"]["time_ns"], 5000)
        # node 只有 1 次 eth0
        self.assertEqual(stats_map[("node", 200, 0)].irqs["irq:eth0"]["count"], 1)
        self.assertEqual(stats_map[("node", 200, 0)].irqs["irq:eth0"]["time_ns"], 3000)

    def test_irq_different_cpus_same_process(self):
        """同一进程在不同 CPU 上被中断，应分别统计"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "CPU 001"), (3, "worker:50")],
            slices_with_args=[
                ("irq", 1000, 5000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "worker:50"}),
                ("irq", 2000, 3000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "worker:50"}),
                ("irq", 3000, 7000, 2, {"irq": "2", "name": "timer", "ret": "handled", "task": "worker:50"}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("worker", 50, 0), ("worker", 50, 1)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        # CPU 0 上的 worker 只有 eth0
        self.assertIn("irq:eth0", stats_map[("worker", 50, 0)].irqs)
        self.assertNotIn("irq:timer", stats_map[("worker", 50, 0)].irqs)
        self.assertEqual(stats_map[("worker", 50, 0)].irqs["irq:eth0"]["count"], 2)
        self.assertEqual(stats_map[("worker", 50, 0)].irqs["irq:eth0"]["time_ns"], 8000)
        # CPU 1 上的 worker 只有 timer
        self.assertNotIn("irq:eth0", stats_map[("worker", 50, 1)].irqs)
        self.assertIn("irq:timer", stats_map[("worker", 50, 1)].irqs)
        self.assertEqual(stats_map[("worker", 50, 1)].irqs["irq:timer"]["time_ns"], 7000)

    def test_softirq_stats(self):
        """softirq 事件统计"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "bash:100")],
            slices_with_args=[
                ("softirq", 1000, 4000, 1, {"vec": "1", "action": "NET_RX", "task": "bash:100"}),
                ("softirq", 2000, 2000, 1, {"vec": "1", "action": "NET_RX", "task": "bash:100"}),
                ("softirq", 3000, 1000, 1, {"vec": "0", "action": "TIMER", "task": "bash:100"}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("bash", 100, 0)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        self.assertIn("softirq:NET_RX", stats_map[("bash", 100, 0)].irqs)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["softirq:NET_RX"]["count"], 2)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["softirq:NET_RX"]["time_ns"], 6000)
        self.assertIn("softirq:TIMER", stats_map[("bash", 100, 0)].irqs)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["softirq:TIMER"]["count"], 1)
        self.assertEqual(stats_map[("bash", 100, 0)].irqs["softirq:TIMER"]["time_ns"], 1000)

    def test_no_irq_events(self):
        """没有 irq/softirq 事件时，irqs 字典应为空"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "bash:100")],
            slices_with_args=[
                ("Running", 1000, 500_000, 2, {"cpu": 0}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("bash", 100, 0)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        self.assertEqual(len(stats_map[("bash", 100, 0)].irqs), 0)

    def test_irq_task_field_missing_skipped(self):
        """task 字段缺失的 irq 事件应被跳过"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "bash:100")],
            slices_with_args=[
                ("irq", 1000, 5000, 1, {"irq": "1", "name": "eth0", "ret": "handled"}),  # 无 task 字段
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("bash", 100, 0)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        self.assertEqual(len(stats_map[("bash", 100, 0)].irqs), 0)

    def test_irq_idle_task_skipped(self):
        """idle 进程（<idle>）的 irq 事件应被跳过"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "<idle>:0")],
            slices_with_args=[
                ("irq", 1000, 5000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "<idle>:0"}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("<idle>", 0, 0)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        self.assertEqual(len(stats_map[("<idle>", 0, 0)].irqs), 0)

    def test_irq_task_not_in_stats_map_skipped(self):
        """task 不在 stats_map 中时应被跳过（没有 running 事件的进程）"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "bash:100"), (3, "unknown:999")],
            slices_with_args=[
                ("irq", 1000, 5000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "unknown:999"}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("bash", 100, 0)])
        compute_irq_stats(conn, stats_map)  # 不应抛异常
        conn.close()

        self.assertEqual(len(stats_map[("bash", 100, 0)].irqs), 0)

    def test_irq_and_softirq_mixed(self):
        """irq 和 softirq 混合，统计应正确区分"""
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "worker:50")],
            slices_with_args=[
                ("irq", 1000, 5000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "worker:50"}),
                ("softirq", 2000, 3000, 1, {"vec": "1", "action": "NET_RX", "task": "worker:50"}),
                ("irq", 3000, 2000, 1, {"irq": "2", "name": "timer", "ret": "handled", "task": "worker:50"}),
                ("softirq", 4000, 1500, 1, {"vec": "1", "action": "NET_RX", "task": "worker:50"}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("worker", 50, 0)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        irqs = stats_map[("worker", 50, 0)].irqs
        self.assertEqual(len(irqs), 3)  # irq:eth0, irq:timer, softirq:NET_RX
        self.assertEqual(irqs["irq:eth0"]["count"], 1)
        self.assertEqual(irqs["irq:eth0"]["time_ns"], 5000)
        self.assertEqual(irqs["irq:timer"]["count"], 1)
        self.assertEqual(irqs["irq:timer"]["time_ns"], 2000)
        self.assertEqual(irqs["softirq:NET_RX"]["count"], 2)
        self.assertEqual(irqs["softirq:NET_RX"]["time_ns"], 4500)

    def test_multiple_irq_types_same_process_same_cpu(self):
        """同一进程在同一 CPU 上同时被多个不同类型的 irq 打断。

        验证：count 和 time_ns 正确累加，irq 字典的 key 正确区分不同类型。
        这是实际系统中的常见场景（如网卡 + 定时器 + softirq 同时中断同一进程）。
        """
        self._create_test_db(
            threads=[(1, "CPU 000"), (2, "nginx:2000")],
            slices_with_args=[
                # irq eth0 x3（应该累加）
                ("irq", 1000, 5000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "nginx:2000"}),
                ("irq", 2000, 3000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "nginx:2000"}),
                ("irq", 3000, 2000, 1, {"irq": "1", "name": "eth0", "ret": "handled", "task": "nginx:2000"}),
                # irq timer x2（应该累加）
                ("irq", 4000, 1000, 1, {"irq": "2", "name": "timer", "ret": "handled", "task": "nginx:2000"}),
                ("irq", 5000, 1500, 1, {"irq": "2", "name": "timer", "ret": "handled", "task": "nginx:2000"}),
                # softirq NET_RX x2（应该累加）
                ("softirq", 6000, 4000, 1, {"vec": "1", "action": "NET_RX", "task": "nginx:2000"}),
                ("softirq", 7000, 2000, 1, {"vec": "1", "action": "NET_RX", "task": "nginx:2000"}),
                # softirq TIMER x1
                ("softirq", 8000, 500, 1, {"vec": "0", "action": "TIMER", "task": "nginx:2000"}),
            ],
        )
        conn = open_db(self.db_path)
        stats_map = self._make_stats_map([("nginx", 2000, 0)])
        compute_irq_stats(conn, stats_map)
        conn.close()

        irqs = stats_map[("nginx", 2000, 0)].irqs
        # 验证 4 种 irq key 都存在
        self.assertEqual(len(irqs), 4)
        self.assertIn("irq:eth0", irqs)
        self.assertIn("irq:timer", irqs)
        self.assertIn("softirq:NET_RX", irqs)
        self.assertIn("softirq:TIMER", irqs)
        # 验证 count 累加
        self.assertEqual(irqs["irq:eth0"]["count"], 3)
        self.assertEqual(irqs["irq:timer"]["count"], 2)
        self.assertEqual(irqs["softirq:NET_RX"]["count"], 2)
        self.assertEqual(irqs["softirq:TIMER"]["count"], 1)
        # 验证 time_ns 累加
        self.assertEqual(irqs["irq:eth0"]["time_ns"], 10000)  # 5000+3000+2000
        self.assertEqual(irqs["irq:timer"]["time_ns"], 2500)  # 1000+1500
        self.assertEqual(irqs["softirq:NET_RX"]["time_ns"], 6000)  # 4000+2000
        self.assertEqual(irqs["softirq:TIMER"]["time_ns"], 500)


class TestAddIrqToEntry(unittest.TestCase):
    """测试 _add_irq_to_entry 辅助函数"""

    def test_add_irq_single(self):
        """单次 irq 累加"""
        stats = TaskStats("proc", 1, cpu_id=0)
        _add_irq_to_entry(stats, "irq", "eth0", 1, 5000)
        self.assertEqual(stats.irqs["irq:eth0"]["count"], 1)
        self.assertEqual(stats.irqs["irq:eth0"]["time_ns"], 5000)

    def test_add_irq_accumulates(self):
        """多次调用应累加 count 和 time_ns"""
        stats = TaskStats("proc", 1, cpu_id=0)
        _add_irq_to_entry(stats, "irq", "eth0", 3, 6000)
        _add_irq_to_entry(stats, "irq", "eth0", 2, 4000)
        info = stats.irqs["irq:eth0"]
        self.assertEqual(info["count"], 5)
        self.assertEqual(info["time_ns"], 10000)

    def test_add_irq_different_types(self):
        """不同类型的 irq 应分别记录"""
        stats = TaskStats("proc", 1, cpu_id=0)
        _add_irq_to_entry(stats, "irq", "eth0", 1, 5000)
        _add_irq_to_entry(stats, "softirq", "NET_RX", 2, 8000)
        self.assertEqual(len(stats.irqs), 2)
        self.assertEqual(stats.irqs["irq:eth0"]["count"], 1)
        self.assertEqual(stats.irqs["softirq:NET_RX"]["count"], 2)


if __name__ == '__main__':
    unittest.main()
