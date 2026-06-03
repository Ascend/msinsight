#!/usr/bin/env python3
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

Standalone Python script to extract PyTorch function call stacks from
Ascend PyTorch Profiler DB files and generate interactive flame graphs.

Usage:
    python flamegraph.py <db_path> [--output OUTPUT]
"""

import argparse
import html
import json
import logging
import os
import re
import sqlite3
import sys
from datetime import datetime
from typing import Dict, Iterator, List, NoReturn, Optional, Tuple


ApiCall = Tuple[int, int, Optional[str]]
ThreadInfo = Tuple[int, int, int]
BuildStats = Dict[str, int]


logger = logging.getLogger(__name__)


def _exit_with_error(message: str) -> NoReturn:
    logger.error(message)
    sys.exit(1)


def _validate_readable_file(file_path: str, description: str) -> str:
    path: str = os.path.abspath(file_path)
    if not os.path.exists(path):
        _exit_with_error(f"{description} does not exist: {path}")
    if not os.path.isfile(path):
        _exit_with_error(f"{description} is not a file: {path}")
    if not os.access(path, os.R_OK):
        _exit_with_error(f"{description} is not readable: {path}")
    return path


REQUIRED_TABLES = ["PYTORCH_API", "STRING_IDS"]

QUERY_API_CALLS_FILTERED = """
SELECT
    CAST(api.startNs AS INTEGER) AS startNs,
    CAST(api.endNs AS INTEGER) AS endNs,
    api.globalTid,
    api_name.value AS api_name
FROM PYTORCH_API AS api
LEFT JOIN STRING_IDS AS api_name ON api.name = api_name.id
WHERE api.startNs IS NOT NULL
  AND api.endNs IS NOT NULL
  AND api.globalTid = ?
  AND (api_name.value IS NULL OR api_name.value NOT LIKE 'ProfilerStep#%')
ORDER BY api.startNs ASC
"""

QUERY_THREADS = """
SELECT DISTINCT globalTid,
       globalTid >> 32 AS pid,
       globalTid & 0xFFFFFFFF AS tid
FROM PYTORCH_API
ORDER BY globalTid
"""

FETCH_SIZE = 10000
MAX_DB_FILE_SIZE = 10 * 1024 * 1024 * 1024
MAX_API_STACK_DEPTH = 1000


class ProfilerDBReader:
    """Reads PyTorch profiler DB and extracts call stacks with timing data."""

    def __init__(self, db_path: str) -> None:
        self.db_path: str = db_path
        self._conn: Optional[sqlite3.Connection] = None
        self._validate_and_connect()

    def close(self) -> None:
        if self._conn is not None:
            self._conn.close()
            self._conn = None

    def _validate_and_connect(self) -> None:
        self.db_path = _validate_readable_file(self.db_path, "DB file")
        if not self.db_path.lower().endswith(".db"):
            _exit_with_error(f"DB file must have .db extension, got: {self.db_path}")
        db_file_size = os.path.getsize(self.db_path)
        if db_file_size > MAX_DB_FILE_SIZE:
            _exit_with_error(f"DB file exceeds the 10GB size limit: {self.db_path} ({db_file_size / (1024**3):.2f} GB)")
        try:
            with open(self.db_path, "rb") as f:
                header: bytes = f.read(16)
        except OSError as err:
            _exit_with_error(f"Failed to read DB file header {self.db_path}: {err}")
        if not header.startswith(b"SQLite format 3\x00"):
            _exit_with_error(f"DB file is not a valid SQLite database: {self.db_path}")
        try:
            self._conn = sqlite3.connect(f"file:{self.db_path}?mode=ro", uri=True)
        except sqlite3.Error as err:
            _exit_with_error(f"Failed to open DB file {self.db_path}: {err}")
        try:
            cursor = self._conn.cursor()
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
            tables = {row[0] for row in cursor.fetchall()}
            missing = [t for t in REQUIRED_TABLES if t not in tables]
            if missing:
                _exit_with_error(f"DB is missing required tables: {', '.join(missing)}")
        except sqlite3.Error as err:
            _exit_with_error(f"Failed to validate DB file {self.db_path}: {err}")

    def list_threads(self) -> List[ThreadInfo]:
        """Return list of (globalTid, pid, tid) tuples."""
        try:
            cursor = self._conn.cursor()
            cursor.execute(QUERY_THREADS)
            return cursor.fetchall()
        except sqlite3.Error as err:
            _exit_with_error(f"Failed to query threads from DB file {self.db_path}: {err}")

    def read_api_calls(self, global_tid: int) -> Iterator[ApiCall]:
        """
        Yield (start_ns, end_ns, api_name) tuples for the given thread.

        Uses streaming read to handle large DBs efficiently.
        """
        try:
            cursor = self._conn.cursor()
            cursor.execute(QUERY_API_CALLS_FILTERED, (global_tid,))

            while True:
                rows = cursor.fetchmany(FETCH_SIZE)
                if not rows:
                    break
                for start_ns, end_ns, _, api_name in rows:
                    yield (start_ns, end_ns, api_name)
        except sqlite3.Error as err:
            _exit_with_error(f"Failed to query API calls from DB file {self.db_path}: {err}")


class FlameNode:
    """A node in the flame graph call tree."""

    __slots__ = ("name", "category", "value", "self_time", "count", "children")

    def __init__(self, name: str, category: str = "unknown") -> None:
        self.name: str = str(name)
        self.category: str = category
        self.value: float = 0.0
        self.self_time: float = 0.0
        self.count: int = 0
        self.children: Dict[str, FlameNode] = {}

    def _to_dict_node(self) -> Dict[str, object]:
        """Create a dict for this node only (no children)."""
        return {
            "name": self.name,
            "category": self.category,
            "value": round(self.value, 2),
            "self_time": round(self.self_time, 2),
            "count": self.count,
        }


def _to_dict_iterative(root: "FlameNode") -> Dict[str, object]:
    """Iterative serialization of FlameNode tree with a max API call stack depth guard."""
    result = root._to_dict_node()
    stack = [(root, result, 0)]
    truncated_nodes = 0
    while stack:
        node, parent_dict, depth = stack.pop()
        if not node.children:
            continue
        children_list = sorted(node.children.values(), key=lambda n: n.value, reverse=True)
        parent_dict["children"] = []
        if depth >= MAX_API_STACK_DEPTH:
            truncated_nodes += len(children_list)
            continue
        for child in children_list:
            child_dict = child._to_dict_node()
            parent_dict["children"].append(child_dict)
            stack.append((child, child_dict, depth + 1))
    if truncated_nodes:
        logger.warning(
            "Flame graph serialization reached the max API call stack depth %d; truncated %d child nodes.",
            MAX_API_STACK_DEPTH,
            truncated_nodes,
        )
    return result


FRAMEWORK_KEYWORDS_LOWER = ("torch", "torch_npu", "aten::", "c10::", "aten_")
CANN_KEYWORDS_LOWER = ("cann", "ascendcl", "aclnn", "aclrt", "aclmdl", "aclprof", "hccl")


def classify_frame(frame_text: str) -> str:
    """Classify a stack frame or API name into a category."""
    text_lower = frame_text.lower()
    for ckw in CANN_KEYWORDS_LOWER:
        if ckw in text_lower:
            return "cann"
    for fkw in FRAMEWORK_KEYWORDS_LOWER:
        if fkw in text_lower:
            return "python_framework"
    if ".py" in frame_text or "python" in text_lower:
        return "python"
    return "unknown"


def _build_single_thread_tree(reader: ProfilerDBReader, global_tid: int) -> Tuple[FlameNode, int, float, BuildStats]:
    """Build a call tree for a single thread and return (root, calls, duration, stats)."""
    root = FlameNode("all", "root")
    root_end_ns = float("inf")
    stack = [(root, root_end_ns)]
    total_calls = 0
    stats = {"non_positive_duration": 0, "empty_api_name": 0}

    for start_ns, end_ns, api_name in reader.read_api_calls(global_tid):
        duration_us = (end_ns - start_ns) / 1000.0
        total_calls += 1
        if duration_us <= 0:
            stats["non_positive_duration"] += 1
            continue
        if not api_name:
            stats["empty_api_name"] += 1
            api_name = "unknown"

        while len(stack) > 1 and stack[-1][1] <= start_ns:
            stack.pop()

        parent = stack[-1][0]
        category = classify_frame(api_name)
        if api_name not in parent.children:
            parent.children[api_name] = FlameNode(api_name, category)
        child = parent.children[api_name]
        child.value += duration_us
        child.count += 1
        for ancestor, _ in stack:
            ancestor.count += 1

        if end_ns > start_ns:
            stack.append((child, end_ns))

    _compute_self_time(root, is_root=True)
    return root, total_calls, root.value, stats


def _merge_trees(dst: FlameNode, src: FlameNode) -> None:
    """Merge src tree into dst tree, combining inclusive values and counts."""
    stack = [(dst, src)]
    while stack:
        dst_node, src_node = stack.pop()
        dst_node.value += src_node.value
        dst_node.count += src_node.count
        for name, src_child in src_node.children.items():
            if name not in dst_node.children:
                dst_node.children[name] = FlameNode(name, src_child.category)
            stack.append((dst_node.children[name], src_child))


def _compute_self_time(node: FlameNode, is_root: bool = False) -> None:
    """Recompute exclusive self time from inclusive value and children."""
    stack = [(node, False)]
    while stack:
        current, visited = stack.pop()
        if visited:
            children_value = sum(c.value for c in current.children.values())
            if current is node and is_root:
                current.value = children_value
                current.self_time = 0.0
            else:
                current.self_time = max(0.0, current.value - children_value)
            continue
        stack.append((current, True))
        for child in current.children.values():
            stack.append((child, False))


def build_flame_tree(reader: ProfilerDBReader) -> Tuple[FlameNode, int, float, BuildStats]:
    """
    Build a flame graph tree by processing each thread independently
    and merging the results.

    Returns (root_node, total_calls, total_duration_us, stats).
    """
    root = FlameNode("all", "root")
    total_calls = 0
    stats = {"non_positive_duration": 0, "empty_api_name": 0}

    threads = reader.list_threads()

    for global_tid, pid, tid in threads:
        thread_root, t_calls, _, thread_stats = _build_single_thread_tree(reader, global_tid)
        thread_key = f"thread_{tid} (globalTid: {global_tid})"
        if thread_key not in root.children:
            root.children[thread_key] = FlameNode(thread_key, "root")
        _merge_trees(root.children[thread_key], thread_root)
        total_calls += t_calls
        for key, value in thread_stats.items():
            stats[key] = stats.get(key, 0) + value

    root.value = sum(child.value for child in root.children.values())
    _compute_self_time(root)
    return root, total_calls, root.value, stats


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def generate_html(
    root: FlameNode,
    total_calls: int,
    total_duration_us: float,
    title: str,
    output_path: str,
) -> None:
    """Generate a self-contained HTML flame graph file with inlined JS."""

    template_path = _validate_readable_file(
        os.path.join(SCRIPT_DIR, "flamegraph_template.html"),
        "HTML template file",
    )
    js_path = _validate_readable_file(
        os.path.join(SCRIPT_DIR, "flamegraph.js"),
        "JavaScript file",
    )

    try:
        with open(template_path, "r", encoding="utf-8") as f:
            template_text = f.read()
    except OSError as err:
        _exit_with_error(f"Failed to read HTML template file {template_path}: {err}")

    try:
        with open(js_path, "r", encoding="utf-8") as f:
            js_code = f.read()
    except OSError as err:
        _exit_with_error(f"Failed to read JavaScript file {js_path}: {err}")

    metadata = {
        "total_duration_us": round(total_duration_us, 2),
        "num_calls": total_calls,
        "generated_at": datetime.now().isoformat(),
    }
    data = {
        "metadata": metadata,
        "flamegraph": _to_dict_iterative(root),
    }
    # json.dumps with ensure_ascii=True escapes < as \u003c and > as \u003e,
    # which prevents HTML injection when JSON is inlined inside a script tag.
    json_str = json.dumps(data, ensure_ascii=True, separators=(",", ":"))
    # Only escape script-closing sequences in JS; escaping all angle brackets
    # would break JavaScript comparison operators such as "<" and ">".
    js_code_safe = re.sub(r"</script", r"<\\/script", js_code, flags=re.IGNORECASE)

    html_text = template_text.replace("$title", html.escape(title, quote=False))
    html_text = html_text.replace("$json_data", json_str)
    html_text = html_text.replace("$js_code", js_code_safe)

    try:
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(html_text)
    except OSError as err:
        _exit_with_error(f"Failed to write output file {output_path}: {err}")

    try:
        size_mb = os.path.getsize(output_path) / (1024 * 1024)
    except OSError as err:
        _exit_with_error(f"Failed to inspect output file {output_path}: {err}")
    logger.info("Flame graph written to: %s (%.1f MB)", output_path, size_mb)


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    parser = argparse.ArgumentParser(
        description="Extract PyTorch API call hierarchy from Ascend Profiler DB "
        "and generate an interactive flame graph.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "db_path",
        nargs="?",
        help="Path to ascend_pytorch_profiler_{{Rank_ID}}.db file",
    )
    parser.add_argument(
        "--output",
        "-o",
        default=".",
        help="Output directory for flamegraph.html (default: current directory)",
    )
    args = parser.parse_args()

    if not args.db_path:
        parser.error("db_path is required")

    reader = ProfilerDBReader(args.db_path)
    try:
        logger.info("Reading: %s", args.db_path)

        logger.info("Building flame tree from API call intervals...")
        root, total_calls, total_duration_us, build_stats = build_flame_tree(reader)

        if total_calls == 0:
            logger.error("No API call data found in the DB.")
            sys.exit(1)

        skipped_calls = build_stats.get("non_positive_duration", 0)
        empty_names = build_stats.get("empty_api_name", 0)
        logger.info("Total: %s API calls, total duration: %.2f s", total_calls, total_duration_us / 1000000)
        if skipped_calls or empty_names:
            logger.info(
                "Invalid records: %s non-positive durations skipped, %s empty API names replaced with unknown",
                skipped_calls,
                empty_names,
            )

        title = os.path.basename(reader.db_path)

        output_dir = os.path.abspath(args.output)
        if not os.path.isdir(output_dir):
            _exit_with_error(f"Output path is not a directory: {output_dir}")
        if not os.access(output_dir, os.W_OK):
            _exit_with_error(f"Output directory is not writable: {output_dir}")
        output_path = os.path.join(output_dir, "flamegraph.html")

        logger.info("Generating flame graph...")
        generate_html(root, total_calls, total_duration_us, title, output_path)
        logger.info("Done! Open the HTML file in a browser to view.")
    finally:
        reader.close()


if __name__ == "__main__":
    main()
