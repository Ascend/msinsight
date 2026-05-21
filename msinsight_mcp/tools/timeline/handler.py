"""Handler implementations for the timeline module."""

from __future__ import annotations

from typing import Any, Optional

from mcp import types

from mapping.timeline import (
    query_communication_kernel_detail_api,
    get_thread_detail_api,
    get_unit_flows_api,
    get_units_in_range_api,
)
from utils.response import ok, err
from state import state


async def query_communication_kernel_detail(
    rank_id: str,
    operator_name: str,
    file_path: Optional[str] = None,
    cluster_path: Optional[str] = None,
) -> types.CallToolResult:
    """Query kernel-level detail for a communication operator."""
    cp = state.current_project
    if cp is None:
        return err(ValueError("No current project set. Call state.set_current_project() first."))
    resolved_path = state.resolve_cluster_path(cluster_path)
    if not isinstance(resolved_path, str):
        return err(ValueError(f"Invalid cluster path: {resolved_path}"))

    # Validate rank_id exists in current project's rank_list
    rank_info = None
    for r in cp.rank_list:
        if str(r.get("rankId", "")) == str(rank_id) or r.get("cardName") == str(rank_id):
            rank_info = r
            break
    if rank_info is None:
        return err(ValueError(f"Rank '{rank_id}' not found in current project's rank_list"))

    db_path = rank_info.get("dbPath")
    target_file = file_path or db_path

    try:
        body = await query_communication_kernel_detail_api(
            project_name=cp.project_name,
            file_path=target_file,
            db_path=db_path,
            rank_id=rank_id,
            operator_name=operator_name,
            cluster_path=resolved_path,
        )

        body["dbPath"] = db_path
        cache_key = f"{body.get('rankId')}_{body.get('id')}"
        timeline_module = cp.get_module("timeline")
        cache = timeline_module.get("kernel_detail_cache")
        if cache:
            cache[cache_key] = body
        else:
            timeline_module.set("kernel_detail_cache", {cache_key: body})

        return ok(
            {
                "id": body.get("id"),
                "rankId": body.get("rankId"),
                "depth": body.get("depth"),
                "threadId": body.get("threadId"),
                "pid": body.get("pid"),
                "step": body.get("step"),
                "group": body.get("group"),
                "startTime": body.get("startTime"),
            }
        )
    except Exception as exc:
        return err(exc)


async def get_thread_detail(
    kernel_id: Optional[str] = None,
    rank_id: Optional[str] = None,
    pid: Optional[str] = None,
    tid: Optional[str] = None,
    start_time: Optional[int] = None,
    depth: Optional[int] = None,
    file_path: Optional[str] = None,
    meta_type: str = "HCCL",
) -> types.CallToolResult:
    """Retrieve thread detail data for a specific event/operator in the timeline."""
    cp = state.current_project
    if cp is None:
        return err(ValueError("No current project set. Call state.set_current_project() first."))

    cache = cp.get_module("timeline").get("kernel_detail_cache", {})
    kernel = cache.get(f"{rank_id}_{kernel_id}") if rank_id and kernel_id else None
    db_path = None
    if kernel is None and cache:
        kernel = next(iter(cache.values()))
    if kernel:
        rank_id = rank_id or kernel.get("rankId")
        kernel_id = kernel_id or kernel.get("id")
        pid = pid or kernel.get("pid")
        tid = tid or kernel.get("threadId")
        start_time = start_time if start_time is not None else kernel.get("startTime")
        depth = depth if depth is not None else kernel.get("depth")
        db_path = kernel.get("dbPath")

    missing = [
        n
        for n, v in zip(
            ["rank_id", "kernel_id", "pid", "tid", "start_time", "depth"],
            [rank_id, kernel_id, pid, tid, start_time, depth],
        )
        if v is None
    ]
    if missing:
        return err(
            ValueError(
                f"Missing required fields: {', '.join(missing)}. Query a kernel first to populate kernel_detail_cache."
            )
        )

    target_file = file_path or db_path
    try:
        body = await get_thread_detail_api(
            project_name=cp.project_name,
            file_path=target_file,
            rank_id=rank_id,
            kernel_id=kernel_id,
            pid=pid,
            tid=tid,
            start_time=start_time,
            depth=depth,
            meta_type=meta_type,
        )

        # Save duration from thread detail into kernel cache for get_unit_flows
        duration = body.get("data", {}).get("duration")
        if duration is not None and kernel:
            kernel["duration"] = duration

        return ok(body)
    except Exception as exc:
        return err(exc)


async def get_unit_flows(
    rank_id: Optional[str] = None,
    tid: Optional[str] = None,
    pid: Optional[str] = None,
    start_time: Optional[int] = None,
    op_id: Optional[str] = None,
    file_path: Optional[str] = None,
    meta_type: Optional[str] = "HCCL",
    is_simulation: bool = False,
) -> types.CallToolResult:
    """Retrieve flow data for a specific operator/event in the timeline."""
    cp = state.current_project
    if cp is None:
        return err(ValueError("No current project set. Call state.set_current_project() first."))

    cache = cp.get_module("timeline").get("kernel_detail_cache", {})
    kernel = cache.get(f"{rank_id}_{op_id}", {}) if rank_id and op_id else None
    if kernel is None and cache:
        kernel = next(iter(cache.values()))
    if kernel:
        rank_id = rank_id or kernel.get("rankId")
        pid = pid or kernel.get("pid")
        tid = tid or kernel.get("threadId")
        op_id = op_id or kernel.get("id")
    duration = kernel.get("duration") if kernel else None

    missing = [
        n
        for n, v in zip(
            ["rank_id", "tid", "pid", "start_time", "op_id"],
            [rank_id, tid, pid, start_time, op_id],
        )
        if v is None
    ]
    if missing:
        return err(
            ValueError(
                f"Missing required fields: {', '.join(missing)}. Query a kernel first to populate kernel_detail_cache."
            )
        )

    # Compute end_time from current_kernel.duration
    if duration is None:
        return err(ValueError("Missing duration in kernel cache. Call get_thread_detail first."))
    end_time = int(start_time) + int(duration)

    target_file = file_path or cp.file_path
    try:
        body = await get_unit_flows_api(
            project_name=cp.project_name,
            file_path=target_file,
            rank_id=rank_id,
            tid=tid,
            pid=pid,
            start_time=start_time,
            end_time=end_time,
            op_id=op_id,
            meta_type=meta_type,
            is_simulation=is_simulation,
        )
        return ok(body)
    except Exception as exc:
        return err(exc)


async def get_units_in_range(
    rank_id: str,
    metadata_list: list[dict[str, Any]],
    start_time: int,
    end_time: int,
    file_path: Optional[str] = None,
    start_depth: Optional[str] = None,
    end_depth: Optional[str] = None,
    extract_features: bool = True,
) -> types.CallToolResult:
    """Retrieve list of operators within a selected time range from timeline swimlanes."""
    cp = state.current_project
    if cp is None:
        return err(ValueError("No current project set. Call state.set_current_project() first."))

    target_file = file_path or cp.file_path
    try:
        body = await get_units_in_range_api(
            project_name=cp.project_name,
            file_path=target_file,
            rank_id=rank_id,
            metadata_list=metadata_list,
            start_time=start_time,
            end_time=end_time,
            start_depth=start_depth,
            end_depth=end_depth,
        )

        if extract_features and isinstance(body, dict) and "data" in body:
            data_list = body.get("data", [])
            total_count = len(data_list)

            features = _extract_unit_features(data_list)
            features["total_count"] = total_count
            features["time_range"] = {
                "start_time": start_time,
                "end_time": end_time,
                "duration_us": end_time - start_time,
            }
            features["rank_id"] = rank_id

            return ok(
                {
                    "features": features,
                    "emptyFlag": body.get("emptyFlag", False),
                }
            )

        return ok(body)
    except Exception as exc:
        return err(exc)


def _extract_unit_features(data_list: list[dict[str, Any]]) -> dict[str, Any]:
    """Extract feature statistics from unit/operator data for slow-rank analysis."""
    if not data_list:
        return {
            "top_10_by_duration": [],
            "top_5_by_occurrences": [],
            "summary": {
                "total_operators": 0,
                "total_wall_duration": 0,
                "avg_wall_duration": 0,
            },
        }

    sorted_by_duration = sorted(
        data_list,
        key=lambda x: x.get("wallDuration", 0),
        reverse=True,
    )
    top_10_duration = []
    for item in sorted_by_duration[:10]:
        top_10_duration.append(
            {
                "title": item.get("title", "unknown"),
                "wallDuration": item.get("wallDuration", 0),
                "occurrences": item.get("occurrences", 0),
            }
        )

    sorted_by_occurrences = sorted(
        data_list,
        key=lambda x: x.get("occurrences", 0),
        reverse=True,
    )
    top_5_occurrences = []
    for item in sorted_by_occurrences[:5]:
        top_5_occurrences.append(
            {
                "title": item.get("title", "unknown"),
                "occurrences": item.get("occurrences", 0),
                "wallDuration": item.get("wallDuration", 0),
            }
        )

    total_wall_duration = sum(x.get("wallDuration", 0) for x in data_list)
    total_occurrences = sum(x.get("occurrences", 0) for x in data_list)
    avg_wall_duration = total_wall_duration / len(data_list) if data_list else 0

    metatype_counts: dict[str, int] = {}
    for item in data_list:
        meta_type_list = item.get("metaTypeList", [])
        for mt in meta_type_list:
            metatype_counts[mt] = metatype_counts.get(mt, 0) + 1

    return {
        "top_10_by_duration": top_10_duration,
        "top_5_by_occurrences": top_5_occurrences,
        "summary": {
            "total_operators": len(data_list),
            "total_wall_duration": total_wall_duration,
            "avg_wall_duration": avg_wall_duration,
            "total_occurrences": total_occurrences,
            "metatype_distribution": metatype_counts,
        },
    }
