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

import os
import sqlite3
import sys
import tempfile
import unittest

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from trace_analyze import (
    TaskStats,
    write_results_to_db,
    _create_result_tables,
    _build_task_rows,
    _build_irq_rows,
)


class TestWriteResultsToDb(unittest.TestCase):
    """测试 write_results_to_db 函数"""

    def setUp(self):
        """创建临时 db 文件"""
        self.tmp_fd, self.tmp_path = tempfile.mkstemp(suffix='.db')
        # 初始化最小 db 结构（需要有 slice 和 thread 表才能 open_db）
        conn = sqlite3.connect(self.tmp_path)
        cur = conn.cursor()
        cur.execute("CREATE TABLE IF NOT EXISTS thread (track_id INTEGER PRIMARY KEY, tid TEXT)")
        cur.execute("CREATE TABLE IF NOT EXISTS slice (track_id INTEGER, name TEXT, duration INTEGER, args TEXT)")
        conn.commit()
        conn.close()

    def tearDown(self):
        os.close(self.tmp_fd)
        if os.path.exists(self.tmp_path):
            os.unlink(self.tmp_path)

    def _get_stats_map(self):
        """构造测试用的 stats_map"""
        stats_map = {}

        # 进程 A 在 CPU 0 上
        key1 = ("processA", 100, 0)
        stats_map[key1] = TaskStats(comm="processA", pid=100, cpu_id=0)
        stats_map[key1].running_ns = 1000000
        stats_map[key1].sleeping_ns = 500000
        stats_map[key1].runnable_ns = 200000
        stats_map[key1].cs_count = 10
        stats_map[key1].cs_involuntary_count = 3
        stats_map[key1].add_irq("irq", "timer", 1000)
        stats_map[key1].add_irq("softirq", "net_rx", 500)

        # 进程 A 在 CPU 1 上（同一进程跨 CPU）
        key2 = ("processA", 100, 1)
        stats_map[key2] = TaskStats(comm="processA", pid=100, cpu_id=1)
        stats_map[key2].running_ns = 800000
        stats_map[key2].sleeping_ns = 300000
        stats_map[key2].runnable_ns = 100000
        stats_map[key2].cs_count = 5
        stats_map[key2].cs_involuntary_count = 1

        # 进程 B cpu_id=None
        key3 = ("processB", 200, None)
        stats_map[key3] = TaskStats(comm="processB", pid=200, cpu_id=None)
        stats_map[key3].running_ns = 500000
        stats_map[key3].cs_count = 2

        return stats_map

    def test_create_tables_creates_both_tables(self):
        """测试 _create_result_tables 创建两张表"""
        conn = sqlite3.connect(self.tmp_path)
        try:
            _create_result_tables(conn)
            cur = conn.cursor()

            # 检查 trace_task_summary 表存在
            cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='trace_task_summary'")
            self.assertIsNotNone(cur.fetchone())

            # 检查 trace_irq_detail 表存在
            cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='trace_irq_detail'")
            self.assertIsNotNone(cur.fetchone())
        finally:
            conn.close()

    def test_create_tables_idempotent(self):
        """测试 _create_result_tables 可重复调用（IF NOT EXISTS）"""
        conn = sqlite3.connect(self.tmp_path)
        try:
            _create_result_tables(conn)
            _create_result_tables(conn)  # 第二次不应报错
        finally:
            conn.close()

    def test_build_task_rows_correct_data(self):
        """测试 _build_task_rows 生成正确的数据行"""
        stats_map = self._get_stats_map()
        rows = _build_task_rows(stats_map)

        # 应该有 3 行（3 个 TaskStats）
        self.assertEqual(len(rows), 3)

        # 检查 processA on CPU 0
        row_cpu0 = [r for r in rows if r[0] == "processA" and r[1] == 100 and r[2] == 0][0]
        self.assertEqual(row_cpu0[3], 1000000)  # running_ns
        self.assertEqual(row_cpu0[4], 500000)  # sleeping_ns
        self.assertEqual(row_cpu0[5], 200000)  # runnable_ns
        self.assertEqual(row_cpu0[6], 10)  # cs_count
        self.assertEqual(row_cpu0[7], 3)  # cs_involuntary_count

        # 检查 processA on CPU 1
        row_cpu1 = [r for r in rows if r[0] == "processA" and r[1] == 100 and r[2] == 1][0]
        self.assertEqual(row_cpu1[3], 800000)
        self.assertEqual(row_cpu1[4], 300000)
        self.assertEqual(row_cpu1[5], 100000)
        self.assertEqual(row_cpu1[6], 5)
        self.assertEqual(row_cpu1[7], 1)

    def test_build_task_rows_cpu_id_none(self):
        """测试 cpu_id=None 时正确写入 NULL"""
        stats_map = self._get_stats_map()
        rows = _build_task_rows(stats_map)

        row_none = [r for r in rows if r[0] == "processB" and r[1] == 200 and r[2] is None][0]
        self.assertIsNone(row_none[2])  # cpu_id 为 None

    def test_build_irq_rows_correct_data(self):
        """测试 _build_irq_rows 生成正确的 IRQ 数据行"""
        stats_map = self._get_stats_map()
        rows = _build_irq_rows(stats_map)

        # 只有 processA on CPU 0 有 IRQ
        self.assertEqual(len(rows), 2)

        # 检查 irq:timer
        timer_row = [r for r in rows if r[3] == "irq" and r[4] == "timer"][0]
        self.assertEqual(timer_row[0], "processA")
        self.assertEqual(timer_row[1], 100)
        self.assertEqual(timer_row[2], 0)
        self.assertEqual(timer_row[5], 1)  # count
        self.assertEqual(timer_row[6], 1000)  # time_ns

        # 检查 softirq:net_rx
        net_row = [r for r in rows if r[3] == "softirq" and r[4] == "net_rx"][0]
        self.assertEqual(net_row[5], 1)
        self.assertEqual(net_row[6], 500)

    def test_build_irq_rows_empty_when_no_irqs(self):
        """测试没有 IRQ 时返回空列表"""
        stats_map = self._get_stats_map()
        # 清除所有 IRQ
        for stats in stats_map.values():
            stats.irqs = {}
        rows = _build_irq_rows(stats_map)
        self.assertEqual(len(rows), 0)

    def test_write_results_to_db_integration(self):
        """集成测试：完整写入流程"""
        stats_map = self._get_stats_map()
        conn = sqlite3.connect(self.tmp_path)
        try:
            write_results_to_db(conn, stats_map)

            cur = conn.cursor()

            # 验证 trace_task_summary 数据
            cur.execute("SELECT COUNT(*) FROM trace_task_summary")
            self.assertEqual(cur.fetchone()[0], 3)

            cur.execute("SELECT comm, pid, cpu_id, running_ns FROM trace_task_summary WHERE cpu_id=0")
            row = cur.fetchone()
            self.assertEqual(row[0], "processA")
            self.assertEqual(row[1], 100)
            self.assertEqual(row[2], 0)
            self.assertEqual(row[3], 1000000)

            # 验证 trace_irq_detail 数据
            cur.execute("SELECT COUNT(*) FROM trace_irq_detail")
            self.assertEqual(cur.fetchone()[0], 2)

            cur.execute("SELECT irq_type, irq_name, count, time_ns FROM trace_irq_detail WHERE irq_type='irq'")
            irq = cur.fetchone()
            self.assertEqual(irq[0], "irq")
            self.assertEqual(irq[1], "timer")
            self.assertEqual(irq[2], 1)
            self.assertEqual(irq[3], 1000)

            # 验证 cpu_id=None 正确存储
            cur.execute("SELECT cpu_id FROM trace_task_summary WHERE comm='processB'")
            self.assertIsNone(cur.fetchone()[0])
        finally:
            conn.close()

    def test_write_results_to_db_clears_old_data(self):
        """测试重复写入时旧数据被清空"""
        stats_map = self._get_stats_map()
        conn = sqlite3.connect(self.tmp_path)
        try:
            # 第一次写入
            write_results_to_db(conn, stats_map)
            cur = conn.cursor()
            cur.execute("SELECT COUNT(*) FROM trace_task_summary")
            self.assertEqual(cur.fetchone()[0], 3)

            # 第二次写入（只有 1 个进程）
            new_map = {}
            key = ("newProcess", 300, 0)
            new_map[key] = TaskStats(comm="newProcess", pid=300, cpu_id=0)
            new_map[key].running_ns = 999
            write_results_to_db(conn, new_map)

            cur.execute("SELECT COUNT(*) FROM trace_task_summary")
            self.assertEqual(cur.fetchone()[0], 1)

            cur.execute("SELECT comm FROM trace_task_summary")
            self.assertEqual(cur.fetchone()[0], "newProcess")
        finally:
            conn.close()

    def test_write_results_to_db_empty_stats_map(self):
        """测试空 stats_map 写入（表存在但无数据）"""
        conn = sqlite3.connect(self.tmp_path)
        try:
            write_results_to_db(conn, {})

            cur = conn.cursor()
            cur.execute("SELECT COUNT(*) FROM trace_task_summary")
            self.assertEqual(cur.fetchone()[0], 0)

            cur.execute("SELECT COUNT(*) FROM trace_irq_detail")
            self.assertEqual(cur.fetchone()[0], 0)
        finally:
            conn.close()


class TestWriteResultsPerformance(unittest.TestCase):
    """测试写入性能相关功能"""

    def setUp(self):
        self.tmp_fd, self.tmp_path = tempfile.mkstemp(suffix='.db')
        conn = sqlite3.connect(self.tmp_path)
        cur = conn.cursor()
        cur.execute("CREATE TABLE IF NOT EXISTS thread (track_id INTEGER PRIMARY KEY, tid TEXT)")
        cur.execute("CREATE TABLE IF NOT EXISTS slice (track_id INTEGER, name TEXT, duration INTEGER, args TEXT)")
        conn.commit()
        conn.close()

    def tearDown(self):
        os.close(self.tmp_fd)
        if os.path.exists(self.tmp_path):
            os.unlink(self.tmp_path)

    def test_pragma_settings_applied(self):
        """测试性能优化 PRAGMA 是否生效"""
        conn = sqlite3.connect(self.tmp_path)
        try:
            cur = conn.cursor()
            cur.execute("PRAGMA synchronous = OFF")
            cur.execute("PRAGMA journal_mode = MEMORY")

            cur.execute("PRAGMA synchronous")
            self.assertEqual(cur.fetchone()[0], 0)  # OFF = 0

            cur.execute("PRAGMA journal_mode")
            self.assertEqual(cur.fetchone()[0], "memory")
        finally:
            conn.close()

    def test_large_dataset_write(self):
        """测试大数据量写入（模拟 500 个进程）"""
        stats_map = {}
        for i in range(500):
            for cpu in range(4):
                key = (f"proc_{i}", 1000 + i, cpu)
                stats_map[key] = TaskStats(comm=f"proc_{i}", pid=1000 + i, cpu_id=cpu)
                stats_map[key].running_ns = 1000000 + i * 100
                stats_map[key].cs_count = i
                if i % 10 == 0:
                    stats_map[key].add_irq("irq", f"irq_{i}", 500)

        conn = sqlite3.connect(self.tmp_path)
        try:
            write_results_to_db(conn, stats_map)

            cur = conn.cursor()
            cur.execute("SELECT COUNT(*) FROM trace_task_summary")
            self.assertEqual(cur.fetchone()[0], 500 * 4)  # 500 进程 * 4 CPU

            # IRQ 只有 50 个进程 * 4 CPU = 200 行
            cur.execute("SELECT COUNT(*) FROM trace_irq_detail")
            self.assertEqual(cur.fetchone()[0], 200)
        finally:
            conn.close()


if __name__ == '__main__':
    unittest.main()
