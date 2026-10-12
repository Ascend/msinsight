# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
#
# MindStudio is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------

"""Build deterministic process tree metadata."""

from typing import Any


def _to_int(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, str) and value.isdigit():
        return int(value)
    return None


def build_process_tree(processes: list[dict[str, Any]]) -> dict[str, Any]:
    """Return process tree roots, nodes, and missing parent PIDs."""
    by_pid: dict[int, dict[str, Any]] = {}
    for process in processes:
        pid = _to_int(process.get("pid"))
        if pid is None:
            continue
        by_pid[pid] = {
            "pid": pid,
            "ppid": _to_int(process.get("ppid")),
            "children": [],
            "tree_root": pid,
            "depth": 0,
            "parent_missing": False,
        }

    for pid, node in by_pid.items():
        ppid = node["ppid"]
        if ppid in by_pid and ppid != pid:
            by_pid[ppid]["children"].append(pid)

    for node in by_pid.values():
        node["children"].sort()

    roots: list[int] = []
    missing_parent_pids: set[int] = set()
    for pid, node in by_pid.items():
        ppid = node["ppid"]
        if ppid is None or ppid == 0 or ppid == pid:
            roots.append(pid)
        elif ppid not in by_pid:
            roots.append(pid)
            node["parent_missing"] = True
            missing_parent_pids.add(ppid)

    if not roots and by_pid:
        roots.append(min(by_pid))

    visited: set[int] = set()

    def walk(pid: int, root: int, depth: int, path: set[int]) -> None:
        if pid in visited:
            return
        visited.add(pid)
        node = by_pid[pid]
        node["tree_root"] = root
        node["depth"] = depth
        acyclic_children = []
        for child_pid in node["children"]:
            if child_pid in path:
                continue
            acyclic_children.append(child_pid)
            walk(child_pid, root, depth + 1, path | {child_pid})
        node["children"] = acyclic_children

    for root in sorted(roots):
        walk(root, root, 0, {root})

    for pid in sorted(by_pid):
        if pid not in visited:
            root = pid
            roots.append(root)
            walk(pid, root, 0, {root})

    roots = sorted(set(roots))
    nodes = [by_pid[pid] for pid in sorted(by_pid)]
    return {
        "roots": roots,
        "nodes": nodes,
        "missing_parent_pids": sorted(missing_parent_pids),
    }
