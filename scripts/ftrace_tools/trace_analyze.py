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

import argparse
import logging
import os
import sqlite3
from dataclasses import dataclass, field
from typing import Dict, Optional, Tuple

logging.basicConfig(level=logging.INFO, format='[%(asctime)s] [%(levelname)s]:%(message)s')

DB_PATH_DEFAULT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ftrace_data.db")


def parse_tid(tid_str: str) -> Tuple[str, Optional[int]]:
    """解析 thread.tid 格式为 (comm, pid)。

    常见格式：
    - "comm:pid" -> ("comm", pid)
    - "WatchParentPid::2581884" -> ("WatchParentPid:", 2581884)  (comm 中有冒号)
    - "<idle>:0" -> ("<idle>", 0)
    - "CPU 000" -> ("CPU 000", None)  (CPU 线程，无 pid)

    策略：从右边找最后一个冒号，右边部分必须是纯数字。
    """
    if not tid_str:
        return tid_str, None

    last_colon = tid_str.rfind(':')
    if last_colon == -1:
        return tid_str, None

    potential_pid = tid_str[last_colon + 1 :]
    if potential_pid.isdigit():
        return tid_str[:last_colon], int(potential_pid)

    return tid_str, None


def parse_cpu_from_tid(tid_str: str) -> Optional[int]:
    """从 thread.tid 提取 CPU 编号。

    仅当 tid 是 CPU 线程格式时返回整数，如 "CPU 000" -> 0。
    进程 tid（如 "trace-cmd:3718847"）返回 None。
    """
    if not tid_str:
        return None
    if tid_str.startswith("CPU "):
        num_part = tid_str[4:].strip()
        try:
            return int(num_part)
        except ValueError:
            return None
    return None


@dataclass
class IrqInfo:
    """单个 IRQ 的统计信息"""

    count: int = 0
    time_s: float = 0.0


@dataclass
class TaskStats:
    """单个进程/线程在单个 CPU 上的汇聚统计。

    一个进程可能跨多个 CPU，每个 CPU 单独一行。
    db 内部统一存纳秒（ns），Excel 输出时转换为微秒（μs）。
    """

    comm: str
    pid: int
    cpu_id: Optional[int]  # None 表示未关联到具体 CPU
    running_ns: int = 0
    sleeping_ns: int = 0
    runnable_ns: int = 0
    cs_count: int = 0
    irqs: Dict[str, Dict] = field(default_factory=dict)  # key: "irq_type:irq_name"

    def add_duration_ns(self, event_name: str, duration_ns: int) -> None:
        """根据事件名累加纳秒级 duration"""
        if event_name == 'Running':
            self.running_ns += duration_ns
        elif event_name == 'Sleeping':
            self.sleeping_ns += duration_ns
        elif event_name == 'Runnable':
            self.runnable_ns += duration_ns

    def add_irq(self, irq_type: str, irq_name: str, duration_ns: int) -> None:
        """累加 IRQ 统计（ns 存储）"""
        key = f"{irq_type}:{irq_name}"
        if key not in self.irqs:
            self.irqs[key] = {"count": 0, "time_ns": 0, "type": irq_type, "name": irq_name}
        self.irqs[key]["count"] += 1
        self.irqs[key]["time_ns"] += duration_ns


def open_db(db_path: str) -> sqlite3.Connection:
    """打开 SQLite db 文件，返回连接。

    Args:
        db_path: db 文件绝对路径

    Returns:
        sqlite3.Connection

    Raises:
        FileNotFoundError: db 文件不存在
    """
    if not os.path.isfile(db_path):
        raise FileNotFoundError(f"DB file not found: {db_path}")
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    return conn


def build_arg_parser() -> argparse.ArgumentParser:
    """构建命令行参数解析器"""
    parser = argparse.ArgumentParser(description='Analyze ftrace data from trace_convert db output')
    parser.add_argument(
        '--input',
        '-i',
        default=DB_PATH_DEFAULT,
        help=f'Input db file path from trace_convert.py (default: {DB_PATH_DEFAULT})',
    )
    parser.add_argument('--output', '-o', default=None, help='Output Excel report path (default: <input>_report.xlsx)')
    return parser


def main():
    parser = build_arg_parser()
    args = parser.parse_args()

    db_path = args.input
    output_path = args.output

    if not output_path:
        base = os.path.splitext(db_path)[0]
        output_path = f"{base}_report.xlsx"

    logging.info("Opening db: %s", db_path)
    conn = open_db(db_path)

    try:
        # 后续子任务将在此处添加统计逻辑
        logging.info("Analysis complete")
    finally:
        conn.close()

    logging.info("Report saved to: %s", output_path)


if __name__ == '__main__':
    main()
