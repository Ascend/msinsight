"""Timeline module — C++ backend API mapping layer.

This module contains all direct calls to the C++ backend via cpp_client.
Handlers in tools/timeline/ call these functions, keeping MCP protocol
details separate from backend communication.
"""

from __future__ import annotations

from typing import Any, Optional

from cpp_client import get_client


async def query_communication_kernel_detail_api(
    project_name: str,
    file_path: str,
    db_path: str,
    rank_id: str,
    operator_name: str,
    cluster_path: str,
) -> dict[str, Any]:
    """Query kernel-level detail for a communication operator."""
    body = await get_client().request(
        "unit/kernelDetail",
        "timeline",
        params={
            "rankId": rank_id,
            "name": operator_name,
            "clusterPath": cluster_path,
            "dbPath": db_path,
        },
        project_name=project_name,
        file_id=file_path,
    )

    if isinstance(body, list):
        body = body[0] if body else {}
    return body


async def get_thread_detail_api(
    project_name: str,
    file_path: str,
    rank_id: str,
    kernel_id: str,
    pid: str,
    tid: str,
    start_time: int,
    depth: int,
    meta_type: str = "HCCL",
) -> dict[str, Any]:
    """Retrieve thread detail data for a specific event/operator."""
    return await get_client().request(
        "unit/threadDetail",
        "timeline",
        params={
            "rankId": rank_id,
            "dbPath": file_path,
            "metaType": meta_type,
            "pid": pid,
            "tid": tid,
            "id": kernel_id,
            "startTime": start_time,
            "depth": depth,
        },
        project_name=project_name,
        file_id=file_path,
    )


async def get_unit_flows_api(
    project_name: str,
    file_path: str,
    rank_id: str,
    tid: str,
    pid: str,
    start_time: int,
    end_time: int,
    op_id: str,
    meta_type: Optional[str] = "HCCL",
    is_simulation: bool = False,
) -> dict[str, Any]:
    """Retrieve flow data for a specific operator/event."""
    return await get_client().request(
        "unit/flows",
        "timeline",
        params={
            "rankId": rank_id,
            "tid": tid,
            "pid": pid,
            "startTime": start_time,
            "endTime": end_time,
            "id": op_id,
            "metaType": meta_type,
            "isSimulation": is_simulation,
        },
        project_name=project_name,
        file_id=file_path,
    )


async def get_units_in_range_api(
    project_name: str,
    file_path: str,
    rank_id: str,
    metadata_list: list[dict[str, Any]],
    start_time: int,
    end_time: int,
    start_depth: Optional[str] = None,
    end_depth: Optional[str] = None,
) -> dict[str, Any]:
    """Retrieve list of operators within a selected time range."""
    formatted_metadata = []
    for meta in metadata_list:
        formatted_metadata.append(
            {
                "tid": str(meta.get("tid", "")),
                "pid": str(meta.get("pid", "")),
                "metaType": meta.get("metaType", ""),
                "rankId": rank_id,
                "lockStartTime": 0,
                "lockEndTime": 0,
                "hidePythonFunction": meta.get("hidePythonFunction", False),
            }
        )

    params = {
        "rankId": rank_id,
        "metadataList": formatted_metadata,
        "startTime": start_time,
        "endTime": end_time,
    }

    if start_depth is not None:
        params["startDepth"] = str(start_depth)
    if end_depth is not None:
        params["endDepth"] = str(end_depth)

    return await get_client().request(
        "unit/threads",
        "timeline",
        params=params,
        project_name=project_name,
        file_id=file_path,
    )
