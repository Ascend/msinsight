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

"""Diagnosis rules for the minimum runnable loop."""

from __future__ import annotations

from typing import Any

from scripts.cpulist import (
    count_cpu_list,
    cpus_by_numa,
    numa_nodes_for_cpu_list,
    parse_cpu_list,
)
from scripts.snapshot import (
    effective_cpu_list,
    get_cgroup_for_pid,
    get_npu_for_device,
    get_rank_mapping_for_pid,
)

Finding = dict[str, Any]


def diagnose(snapshot: dict[str, Any]) -> list[Finding]:
    findings: list[Finding] = []
    findings.extend(_missing_info(snapshot))
    findings.extend(_cgroup_conflicts(snapshot))
    findings.extend(_unbound_processes(snapshot))
    findings.extend(_cross_numa(snapshot))
    findings.extend(_thread_oversubscription(snapshot))
    return findings


def _finding(
    rule_id: str,
    title: str,
    severity: str,
    impact: list[str],
    evidence: list[str],
    judgement: str,
    recommendations: list[str],
    risk: str,
    verification: list[str],
) -> Finding:
    return {
        "id": rule_id,
        "title": title,
        "severity": severity,
        "impact": impact,
        "evidence": evidence,
        "judgement": judgement,
        "recommendations": recommendations,
        "risk": risk,
        "verification": verification,
    }


def _missing_info(snapshot: dict[str, Any]) -> list[Finding]:
    missing = snapshot.get("availability", {}).get("missing", [])
    if not missing:
        return []
    return [
        _finding(
            "R010",
            "信息不足",
            "low",
            ["stability"],
            [f"availability.missing 包含 {item}" for item in missing],
            "当前 Snapshot 缺少部分字段，本次只执行可由现有证据支持的诊断。",
            ["补齐缺失字段后重新生成报告。"],
            "信息不足可能导致部分建议偏保守。",
            ["补齐字段后再次运行 analyze。"],
        )
    ]


def _cgroup_conflicts(snapshot: dict[str, Any]) -> list[Finding]:
    findings: list[Finding] = []
    for process in snapshot.get("processes", []):
        pid = int(process.get("pid"))
        group = get_cgroup_for_pid(snapshot, pid)
        if not group or not group.get("cpuset_cpus_effective"):
            continue
        allowed = parse_cpu_list(process.get("cpus_allowed_list"))
        cpuset = parse_cpu_list(group.get("cpuset_cpus_effective"))
        outside = allowed - cpuset
        throttled = int(group.get("nr_throttled") or 0)
        if outside or throttled > 0:
            evidence = [
                f"PID {pid} Cpus_allowed_list={process.get('cpus_allowed_list')}",
                f"PID {pid} cpuset_cpus_effective={group.get('cpuset_cpus_effective')}",
            ]
            if throttled > 0:
                evidence.append(f"PID {pid} nr_throttled={throttled}")
            findings.append(
                _finding(
                    "R007",
                    "cgroup/cpuset 与应用绑核冲突",
                    "medium",
                    ["latency", "stability"],
                    evidence,
                    "容器或 cgroup 限制会决定真实可用 CPU，推荐方案必须以 cpuset_cpus_effective 为上限。",
                    ["生成推荐 CPU range 时只使用 cgroup 允许的 CPU。"],
                    "绑定到 cgroup 不允许的 CPU 会无效或造成误判。",
                    ["确认实际线程 CPU 落在 cpuset_cpus_effective 内。"],
                )
            )
    return findings


def _unbound_processes(snapshot: dict[str, Any]) -> list[Finding]:
    findings: list[Finding] = []
    online_count = count_cpu_list(snapshot.get("system", {}).get("online_cpus"))
    numa_nodes = snapshot.get("numa_topology", {}).get("nodes", [])
    if len(numa_nodes) <= 1:
        return findings
    for process in snapshot.get("processes", []):
        allowed_count = count_cpu_list(process.get("cpus_allowed_list"))
        if online_count and allowed_count >= max(1, int(online_count * 0.8)):
            pid = process.get("pid")
            findings.append(
                _finding(
                    "R001",
                    "进程未绑定 CPU",
                    "high",
                    ["throughput", "stability"],
                    [
                        f"PID {pid} Cpus_allowed_list={process.get('cpus_allowed_list')}",
                        f"system.online_cpus={snapshot.get('system', {}).get('online_cpus')}",
                        f"NUMA 节点数={len(numa_nodes)}",
                    ],
                    "目标进程允许在接近全机 CPU 上运行，存在跨 NUMA 调度和 CPU 竞争风险。",
                    ["优先将每个 rank/实例绑定到对应 NPU 本地 NUMA 的 CPU 子集。"],
                    "CPU range 过窄可能导致线程竞争，应先使用保守方案并验证。",
                    ["对比 CPU migration、step time p99、NPU utilization 波动。"],
                )
            )
    return findings


def _cross_numa(snapshot: dict[str, Any]) -> list[Finding]:
    findings: list[Finding] = []
    numa_nodes = snapshot.get("numa_topology", {}).get("nodes", [])
    if len(numa_nodes) <= 1:
        return findings
    for process in snapshot.get("processes", []):
        pid = process.get("pid")
        observed = {
            thread.get("numa_node") for thread in process.get("threads", []) if thread.get("numa_node") is not None
        }
        top_observed = {
            thread.get("numa_node")
            for thread in snapshot.get("runtime_sample", {}).get("top_threads", [])
            if int(thread.get("pid", -1)) == int(pid) and thread.get("numa_node") is not None
        }
        nodes = {int(node) for node in observed | top_observed}
        if len(nodes) > 1:
            findings.append(
                _finding(
                    "R002",
                    "进程跨 NUMA 运行",
                    "medium",
                    ["throughput", "stability"],
                    [f"PID {pid} top/active threads 分布在 NUMA {sorted(nodes)}"],
                    "关键线程跨 NUMA 调度，可能增加访问延迟并放大抖动。",
                    ["将目标进程 CPU range 收敛到对应 NPU 本地 NUMA。"],
                    "如果单进程服务多个 NPU，强行单 NUMA 绑定可能变差。",
                    ["对比优化前后 top_threads 的 NUMA 分布。"],
                )
            )
    return findings


def _thread_oversubscription(snapshot: dict[str, Any]) -> list[Finding]:
    findings: list[Finding] = []
    threading = snapshot.get("pytorch", {}).get("threading", {})
    dataloader = snapshot.get("pytorch", {}).get("dataloader", {})
    torch_threads = _to_int(threading.get("torch_num_threads"))
    omp_threads = _to_int(threading.get("omp_num_threads"))
    dataloader_workers = _to_int(dataloader.get("num_workers"))
    for process in snapshot.get("processes", []):
        effective = effective_cpu_list(snapshot, process)
        effective_count = count_cpu_list(effective)
        demand = max(
            torch_threads or 0,
            omp_threads or 0,
            (dataloader_workers or 0) + 1,
            int(process.get("num_threads") or 0),
        )
        if effective_count and demand > effective_count:
            pid = process.get("pid")
            findings.append(
                _finding(
                    "R006",
                    "PyTorch 线程池过载",
                    "medium",
                    ["throughput", "stability"],
                    [
                        f"PID {pid} effective_cpu_count={effective_count}",
                        f"PID {pid} num_threads={process.get('num_threads')}",
                        f"torch_num_threads={threading.get('torch_num_threads')}",
                        f"OMP_NUM_THREADS={threading.get('omp_num_threads')}",
                        f"DataLoader num_workers={dataloader.get('num_workers')}",
                    ],
                    "线程需求超过有效 CPU 数，可能造成 oversubscription 和调度竞争。",
                    ["按推荐 CPU range 调整 OMP_NUM_THREADS、torch_num_threads 和 DataLoader workers。"],
                    "线程数调小可能影响 CPU 算子或数据预处理吞吐。",
                    ["对比 context switch、CPU utilization、step time p99。"],
                )
            )
    return findings


def recommended_numa_cpus(snapshot: dict[str, Any], process: dict[str, Any]) -> str | None:
    mapping = get_rank_mapping_for_pid(snapshot, int(process.get("pid", -1)))
    npu = get_npu_for_device(snapshot, mapping.get("npu_device") if mapping else process.get("npu_device"))
    if npu and npu.get("local_cpus"):
        return npu.get("local_cpus")
    numa_nodes = snapshot.get("numa_topology", {}).get("nodes", [])
    process_nodes = numa_nodes_for_cpu_list(process.get("cpus_allowed_list"), numa_nodes)
    by_numa = cpus_by_numa(numa_nodes)
    if len(process_nodes) == 1:
        return next(iter(by_numa.values())) and process.get("cpus_allowed_list")
    if by_numa:
        return next(iter(by_numa.values())) and next(iter(numa_nodes)).get("cpus")
    return None


def _to_int(value: Any) -> int | None:
    if value is None or value == "":
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None
