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
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------

"""HTML report renderer."""

from __future__ import annotations

import html
import json
from pathlib import Path
from typing import Any

from scripts.cpulist import parse_cpu_list
from scripts.process_tree import build_process_tree
from scripts.topology_view import build_topology_view, render_topology_html


def render_report(
    snapshot: dict[str, Any],
    findings: list[dict[str, Any]],
    plan: dict[str, Any],
    output_path: str | Path,
) -> None:
    path = Path(output_path)
    path.parent.mkdir(parents=True, exist_ok=True)
    document = _document(snapshot, findings, plan)
    path.write_text(document, encoding="utf-8")


def _document(snapshot: dict[str, Any], findings: list[dict[str, Any]], plan: dict[str, Any]) -> str:
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>mindstudio-cpu-binding CPU 绑核优化报告</title>
<style>
body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 24px; color: #1f2937; background: #f8fafc; }}
h1, h2 {{ color: #0f172a; }}
.card {{ background: #fff; border: 1px solid #e5e7eb; border-radius: 12px; padding: 16px; margin: 16px 0; box-shadow: 0 1px 2px rgba(15, 23, 42, 0.06); }}
details.card {{ padding: 0; overflow: hidden; }}
details.card > summary {{ cursor: pointer; list-style: none; padding: 16px; font-weight: 700; color: #0f172a; }}
details.card > summary::-webkit-details-marker {{ display: none; }}
details.card > summary::before {{ content: '▸'; display: inline-block; margin-right: 8px; transition: transform 0.15s ease; }}
details.card[open] > summary::before {{ transform: rotate(90deg); }}
details.card > :not(summary) {{ padding-left: 16px; padding-right: 16px; }}
details.card > :last-child {{ padding-bottom: 16px; }}
.collapsible-section {{ scroll-margin-top: 16px; }}
.summary {{ display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 12px; }}
.metric {{ background: #eff6ff; border-radius: 10px; padding: 12px; }}
.metric strong {{ display: block; font-size: 22px; color: #1d4ed8; }}
table {{ border-collapse: collapse; width: 100%; background: #fff; }}
th, td {{ border: 1px solid #e5e7eb; padding: 8px; text-align: left; vertical-align: top; }}
th {{ background: #f1f5f9; }}
.command-cell {{ max-width: 680px; white-space: pre-wrap; overflow-wrap: anywhere; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 12px; }}
.comm-cell {{ font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }}
.badge {{ display: inline-block; border-radius: 999px; padding: 2px 8px; font-size: 12px; background: #e0f2fe; color: #0369a1; }}
.high {{ background: #fee2e2; color: #991b1b; }}
.medium {{ background: #fef3c7; color: #92400e; }}
.low {{ background: #e0f2fe; color: #075985; }}
.cpu-grid {{ display: flex; flex-wrap: wrap; gap: 4px; }}
.cpu {{ min-width: 28px; padding: 5px; border-radius: 6px; text-align: center; background: #e5e7eb; font-size: 12px; }}
.allowed {{ background: #bfdbfe; }}
.target {{ background: #bbf7d0; }}
.overlap {{ background: #99f6e4; }}
.topology-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap: 12px; margin-top: 12px; }}
.topology-graph {{ margin: 16px 0; border: 1px solid #e5e7eb; border-radius: 12px; background: #fafafa; overflow-x: auto; }}
.topology-svg {{ display: block; width: 100%; min-width: 640px; height: auto; }}
.graph-server {{ fill: #455A64; stroke: #263238; stroke-width: 2; }}
.graph-numa {{ fill: #4CAF50; stroke: #2E7D32; stroke-width: 1.5; }}
.graph-npu {{ fill: #FF5722; stroke: #BF360C; stroke-width: 1.5; }}
.graph-node-label {{ fill: #fff; font-size: 13px; font-weight: 700; text-anchor: middle; dominant-baseline: middle; }}
.graph-node-detail {{ fill: #f8fafc; font-size: 9px; text-anchor: middle; dominant-baseline: middle; }}
.topology-edge-label {{ fill: #475569; font-size: 10px; text-anchor: middle; paint-order: stroke; stroke: #fafafa; stroke-width: 3px; }}
.topology-legend text {{ fill: #334155; font-size: 12px; dominant-baseline: middle; }}
.topology-node {{ border: 1px solid #dbeafe; border-radius: 12px; padding: 12px; background: #f8fafc; }}
.topology-item {{ border: 1px solid #e5e7eb; border-radius: 10px; padding: 8px; margin: 8px 0; background: #fff; }}
.topology-item strong, .topology-item span {{ display: block; }}
.process-item.cross-numa {{ border-color: #fca5a5; background: #fef2f2; }}
.process-item.local {{ border-color: #86efac; background: #f0fdf4; }}
pre {{ background: #0f172a; color: #e2e8f0; padding: 12px; border-radius: 8px; overflow: auto; }}
</style>
</head>
<body>
<h1>mindstudio-cpu-binding CPU 绑核优化报告</h1>
{_attention(findings)}
{_summary(snapshot, findings, plan)}
{_findings(findings)}
{_process_table(snapshot, plan)}
{_process_tree(snapshot, plan)}
{_topology_view(snapshot, plan)}
{_cpu_grid(snapshot, plan)}
{_key_processes(snapshot)}
{_plan(plan)}
{_gaps(snapshot)}
</body>
</html>
"""


def _attention(findings: list[dict[str, Any]]) -> str:
    high = sum(1 for finding in findings if finding.get("severity") == "high")
    medium = sum(1 for finding in findings if finding.get("severity") == "medium")
    if not findings:
        top_findings = "<p>未识别到明确 CPU 绑核问题。</p>"
    else:
        top_findings = "<ol>" + "".join(_attention_item(finding) for finding in findings[:3]) + "</ol>"
    return f"""
<div class="card">
<h2>需要关注的问题</h2>
<p>共识别 {len(findings)} 个问题；High / Medium：{high} / {medium}</p>
{top_findings}
</div>
"""


def _attention_item(finding: dict[str, Any]) -> str:
    severity = html.escape(str(finding.get("severity", "info")))
    title = html.escape(str(finding.get("title", "")))
    judgement = html.escape(_short_judgement(finding))
    return f"<li><span class='badge {severity}'>{severity}</span> <strong>{title}</strong> - {judgement}</li>"


def _short_judgement(finding: dict[str, Any]) -> str:
    judgement = str(finding.get("judgement", ""))
    return judgement if len(judgement) <= 80 else judgement[:77] + "..."


def _summary(snapshot: dict[str, Any], findings: list[dict[str, Any]], plan: dict[str, Any]) -> str:
    high = sum(1 for finding in findings if finding.get("severity") == "high")
    medium = sum(1 for finding in findings if finding.get("severity") == "medium")
    return f"""
<div class="summary">
  <div class="metric"><span>目标 PID</span><strong>{len(snapshot.get("processes", []))}</strong></div>
  <div class="metric"><span>问题发现</span><strong>{len(findings)}</strong></div>
  <div class="metric"><span>High / Medium</span><strong>{high} / {medium}</strong></div>
  <div class="metric"><span>执行后端</span><strong>{html.escape(plan.get("executor_backend", "dry-run"))}</strong></div>
</div>
<div class="card"><strong>总体结论：</strong>{html.escape(plan.get("summary", ""))}</div>
"""


def _process_table(snapshot: dict[str, Any], plan: dict[str, Any]) -> str:
    action_by_pid = {action["pid"]: action for action in plan.get("apply_actions", [])}
    rows = []
    for process in snapshot.get("processes", []):
        pid = int(process.get("pid"))
        action = action_by_pid.get(pid, {})
        rows.append(
            "<tr>"
            f"<td>{pid}</td>"
            f"<td>{html.escape(_process_role(process))}</td>"
            f"<td class='comm-cell'>{html.escape(_process_comm(process))}</td>"
            f"<td class='command-cell'>{html.escape(_process_command(process))}</td>"
            f"<td>{html.escape(str(process.get('npu_device', '')))}</td>"
            f"<td>{html.escape(str(process.get('cpus_allowed_list', '')))}</td>"
            f"<td>{html.escape(str(action.get('effective_cpu_list', '')))}</td>"
            f"<td>{html.escape(str(action.get('target_cpu_list', '无需调整')))}</td>"
            "</tr>"
        )
    return (
        """
<div class="card">
<h2>当前 CPU 绑定状态</h2>
<p>Role/Rank、Comm 和完整命令分列展示：Comm 来自 Linux 短进程名，完整命令来自 cmdline。</p>
<table>
<thead><tr><th>PID</th><th>Role/Rank</th><th>Comm</th><th>完整命令</th><th>NPU</th><th>当前 CPU Range</th><th>有效 CPU Range</th><th>推荐 CPU Range</th></tr></thead>
<tbody>
"""
        + "\n".join(rows)
        + "\n</tbody></table></div>"
    )


def _process_tree(snapshot: dict[str, Any], plan: dict[str, Any]) -> str:
    processes = snapshot.get("processes", [])
    process_by_pid = {int(process.get("pid")): process for process in processes if process.get("pid") is not None}
    action_by_pid = {
        int(action["pid"]): action for action in plan.get("apply_actions", []) if action.get("pid") is not None
    }
    tree = snapshot.get("process_tree") or build_process_tree(processes)
    rows = []
    for node in _ordered_process_tree_nodes(tree):
        pid = int(node.get("pid"))
        process = process_by_pid.get(pid, {})
        action = action_by_pid.get(pid, {})
        rows.append(
            "<tr>"
            f"<td>{pid}</td>"
            f"<td>{html.escape(str(node.get('ppid', '')))}</td>"
            f"<td>{html.escape(_tree_process_comm(process, int(node.get('depth', 0))))}</td>"
            f"<td>{html.escape(_process_role(process))}</td>"
            f"<td class='command-cell'>{html.escape(_process_command(process))}</td>"
            f"<td>{html.escape(str(process.get('npu_device', '')))}</td>"
            f"<td>{html.escape(str(process.get('cpus_allowed_list', '')))}</td>"
            f"<td>{html.escape(str(action.get('target_cpu_list', '无需调整')))}</td>"
            f"<td>{html.escape(_key_thread_summary(pid, snapshot.get('key_processes') or {}))}</td>"
            "</tr>"
        )
    if not rows:
        rows.append("<tr><td colspan='9'>未采集到进程关系信息。</td></tr>")
    missing_notice = _missing_parent_notice(tree.get("missing_parent_pids", []))
    description = (
        "<p>本表按父子关系展示进程。Comm 保留父子树缩进，完整命令单独展示；最后一列统计该进程中识别到的关键线程数量；"
        "具体线程明细请查看下方“关键进程与线程”章节。</p>"
    )
    return (
        "<div class='card'>"
        "<h2>进程关系 / 父子进程树</h2>"
        f"{description}"
        f"{missing_notice}"
        "<table><thead><tr>"
        "<th>PID</th><th>PPID</th><th>Comm</th><th>Role/Rank</th><th>完整命令</th><th>NPU</th>"
        "<th>当前 CPU Range</th><th>推荐 CPU Range</th><th>识别到的关键线程</th>"
        "</tr></thead><tbody>" + "\n".join(rows) + "</tbody></table></div>"
    )


def _ordered_process_tree_nodes(tree: dict[str, Any]) -> list[dict[str, Any]]:
    nodes_by_pid = {int(node.get("pid")): node for node in tree.get("nodes", []) if node.get("pid") is not None}
    ordered: list[dict[str, Any]] = []
    visited: set[int] = set()

    def walk(pid: int) -> None:
        if pid in visited or pid not in nodes_by_pid:
            return
        visited.add(pid)
        node = nodes_by_pid[pid]
        ordered.append(node)
        for child_pid in sorted(int(child) for child in node.get("children", [])):
            walk(child_pid)

    for root in sorted(int(root) for root in tree.get("roots", [])):
        walk(root)
    for pid in sorted(nodes_by_pid):
        walk(pid)
    return ordered


def _process_role(process: dict[str, Any]) -> str:
    role = process.get("role_hint") or process.get("role") or process.get("rank") or process.get("instance")
    return str(role) if role is not None else ""


def _process_comm(process: dict[str, Any]) -> str:
    return str(process.get("comm") or "")


def _process_command(process: dict[str, Any]) -> str:
    return str(process.get("command") or process.get("args") or "")


def _tree_process_comm(process: dict[str, Any], depth: int) -> str:
    return f"{'　' * depth}{_process_comm(process)}"


def _key_thread_summary(pid: int, key_processes: dict[str, Any]) -> str:
    buckets = [
        ("sq_task_threads", "SQ 线程"),
        ("communication_threads", "通信线程"),
        ("npu_fixed_threads", "NPU 固定线程"),
        ("dataloader_threads", "DataLoader 线程"),
        ("top_threads", "高 CPU 线程"),
    ]
    parts = []
    for key, label in buckets:
        count = sum(1 for item in key_processes.get(key, []) if _item_pid(item) == pid)
        if count:
            parts.append(f"{label} {count} 个")
    return "，".join(parts) or "未识别到关键线程"


def _item_pid(item: Any) -> int | None:
    if isinstance(item, int):
        return item
    if isinstance(item, dict) and item.get("pid") is not None:
        return int(item.get("pid"))
    return None


def _missing_parent_notice(missing_parent_pids: list[Any]) -> str:
    if not missing_parent_pids:
        return ""
    pids = "，".join(html.escape(str(pid)) for pid in missing_parent_pids)
    return f"<p>以下父进程未在本次采集中出现：{pids}</p>"


def _topology_view(snapshot: dict[str, Any], plan: dict[str, Any]) -> str:
    view = build_topology_view(snapshot, plan)
    return render_topology_html(view)


def _cpu_grid(snapshot: dict[str, Any], plan: dict[str, Any]) -> str:
    allowed = set()
    target = set()
    for process in snapshot.get("processes", []):
        allowed |= parse_cpu_list(process.get("cpus_allowed_list"))
    for action in plan.get("apply_actions", []):
        target |= parse_cpu_list(action.get("target_cpu_list"))

    sections = []
    for node in snapshot.get("numa_topology", {}).get("nodes", []):
        cpus = sorted(parse_cpu_list(node.get("cpus")))
        cells = []
        for cpu in cpus:
            classes = ["cpu"]
            if cpu in allowed and cpu in target:
                classes.append("overlap")
            elif cpu in target:
                classes.append("target")
            elif cpu in allowed:
                classes.append("allowed")
            cells.append(f"<span class='{' '.join(classes)}'>{cpu}</span>")
        sections.append(f"<h3>NUMA {node.get('node')}</h3><div class='cpu-grid'>{''.join(cells)}</div>")
    return "<div class='card'><h2>CPU / NUMA 视图</h2>" + "\n".join(sections) + "</div>"


def _key_processes(snapshot: dict[str, Any]) -> str:
    key_processes = snapshot.get("key_processes") or {}
    if not key_processes:
        return "<details class='card collapsible-section' open><summary>关键进程与线程</summary><p>未采集到 key_processes 信息。</p></details>"

    process_by_pid = {
        int(process.get("pid")): process for process in snapshot.get("processes", []) if process.get("pid") is not None
    }
    rows = []
    rows.extend(_key_process_rows("主调度进程", key_processes.get("main_scheduler_pids", []), process_by_pid))
    rows.extend(_key_process_rows("SQ 线程", key_processes.get("sq_task_threads", []), process_by_pid))
    rows.extend(_key_process_rows("NPU 固定线程", key_processes.get("npu_fixed_threads", []), process_by_pid))
    rows.extend(_key_process_rows("通信线程", key_processes.get("communication_threads", []), process_by_pid))
    rows.extend(
        _key_process_rows(
            "DataLoader 线程",
            key_processes.get("dataloader_threads", []),
            process_by_pid,
        )
    )
    rows.extend(_key_process_rows("Top CPU 线程", key_processes.get("top_threads", []), process_by_pid))
    if not rows:
        rows.append("<tr><td colspan='6'>未识别到关键进程或线程。</td></tr>")

    return (
        "<details class='card collapsible-section' open><summary>关键进程与线程</summary>"
        "<table><thead><tr>"
        "<th>类别</th><th>PID</th><th>TID</th><th>名称</th><th>NPU</th><th>CPU%</th>"
        "</tr></thead><tbody>" + "\n".join(rows) + "</tbody></table></details>"
    )


def _key_process_rows(category: str, items: list[Any], process_by_pid: dict[int, dict[str, Any]]) -> list[str]:
    rows = []
    for item in items:
        data = {"pid": item, "tid": item} if isinstance(item, int) else item
        process = process_by_pid.get(int(data.get("pid"))) if data.get("pid") is not None else None
        rows.append(
            "<tr>"
            f"<td>{html.escape(category)}</td>"
            f"<td>{html.escape(str(data.get('pid', '')))}</td>"
            f"<td>{html.escape(str(data.get('tid', '')))}</td>"
            f"<td>{html.escape(_key_process_name(data, process))}</td>"
            f"<td>{html.escape(str(data.get('npu_id', data.get('npu_device', ''))))}</td>"
            f"<td>{html.escape(str(data.get('cpu_percent', '')))}</td>"
            "</tr>"
        )
    return rows


def _key_process_name(data: dict[str, Any], process: dict[str, Any] | None) -> str:
    process_command = str((process or {}).get("command") or "")
    process_comm = str((process or {}).get("comm") or "")
    return process_command or process_comm or str(data.get("name") or data.get("key_class", ""))


def _findings(findings: list[dict[str, Any]]) -> str:
    if not findings:
        return "<h2>问题发现</h2><div class='card'><p>未识别到明确 CPU 绑核问题。</p></div>"

    if len(findings) <= 3:
        cards = [_finding_card(finding) for finding in findings]
    else:
        cards = [_collapsible_finding_card(finding) for finding in findings]
    return "<h2>问题发现</h2>" + "\n".join(cards)


def _finding_card(finding: dict[str, Any]) -> str:
    severity = html.escape(str(finding.get("severity", "info")))
    evidence = _finding_list(finding.get("evidence", []))
    recommendations = _finding_list(finding.get("recommendations", []))
    return (
        f"<div class='card'><span class='badge {severity}'>{severity}</span> "
        f"<strong>{html.escape(str(finding.get('id')))} {html.escape(str(finding.get('title')))}</strong>"
        f"<p>{html.escape(str(finding.get('judgement')))}</p>"
        f"<h4>证据</h4><ul>{evidence}</ul>"
        f"<h4>建议</h4><ul>{recommendations}</ul></div>"
    )


def _collapsible_finding_card(finding: dict[str, Any]) -> str:
    severity = html.escape(str(finding.get("severity", "info")))
    title = html.escape(str(finding.get("title", "")))
    judgement = html.escape(str(finding.get("judgement", "")))
    evidence = _finding_list(finding.get("evidence", []))
    recommendations = _finding_list(finding.get("recommendations", []))
    open_attr = " open" if finding.get("severity") == "high" else ""
    summary = f"<span class='badge {severity}'>{severity}</span> {title} - {html.escape(_short_judgement(finding))}"
    return (
        f"<details class='card finding-card'{open_attr}>"
        f"<summary>{summary}</summary>"
        f"<p><strong>ID：</strong>{html.escape(str(finding.get('id')))}</p>"
        f"<p>{judgement}</p>"
        f"<h4>证据</h4><ul>{evidence}</ul>"
        f"<h4>建议</h4><ul>{recommendations}</ul></details>"
    )


def _finding_list(items: list[Any]) -> str:
    return "".join(f"<li>{html.escape(str(item))}</li>" for item in items)


def _plan(plan: dict[str, Any]) -> str:
    commands = "\n".join(action.get("apply_command", "") for action in plan.get("apply_actions", []))
    rollback = "\n".join(action.get("rollback_command", "") for action in plan.get("rollback_actions", []))
    state = json.dumps(plan.get("rollback_state_preview", {}), ensure_ascii=False, indent=2)
    return f"""
<div class="card">
<h2>推荐方案与回滚预览</h2>
<p>执行前需要用户确认；当前实现默认 dry-run，不修改系统状态。</p>
<h3>应用命令</h3><pre>{html.escape(commands)}</pre>
<h3>回滚命令</h3><pre>{html.escape(rollback)}</pre>
<h3>rollback-state 预览</h3><pre>{html.escape(state)}</pre>
</div>
"""


def _gaps(snapshot: dict[str, Any]) -> str:
    missing = snapshot.get("availability", {}).get("missing", [])
    if not missing:
        return "<div class='card'><h2>信息缺口</h2><p>未发现关键缺失字段。</p></div>"
    items = "".join(f"<li>{html.escape(str(item))}</li>" for item in missing)
    return f"<div class='card'><h2>信息缺口</h2><ul>{items}</ul></div>"
