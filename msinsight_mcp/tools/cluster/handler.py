"""
Handlers for the C++ backend's **summary** and "communication" module.
"""

from __future__ import annotations

from typing import Optional

from mcp import types

from cpp_client import get_client
from utils.decorators import require_events
from utils.response import ok, err
from state import state


@require_events("parse/clusterCompleted", "parse/clusterStep2Completed")
async def communication_duration_slow_rank_list(
    operatorName: str = "Total Op Info",
    stage: str = "test",
    clusterPath: Optional[str] = "",
    iterationId: str = "",
    rankList: Optional[list[str]] = None,
    targetOperatorName: str = "",
    isCompare: bool = False,
    baselineIterationId: str = "",
    pgName: str = "",
    groupIdHash: str = "",
    baselineGroupIdHash: str = "",
) -> types.CallToolResult:
    resolved_path = state.resolve_cluster_path(clusterPath)
    if not isinstance(resolved_path, str):
        return resolved_path
    try:
        params = {
            "operatorName": operatorName,
            "stage": stage,
            "clusterPath": resolved_path,
            "iterationId": iterationId,
            "rankList": rankList or [],
            "targetOperatorName": targetOperatorName,
            "isCompare": isCompare,
            "baselineIterationId": baselineIterationId,
            "pgName": pgName,
            "groupIdHash": groupIdHash,
            "baselineGroupIdHash": baselineGroupIdHash,
        }
        body = await get_client().request(
            "communication/duration/slow-rank/list",
            "communication",
            params=params,
        )
        return ok(body)
    except Exception as exc:
        return err(exc)


@require_events("parse/clusterCompleted", "parse/clusterStep2Completed")
async def communication_duration_iterations(
    clusterPath: Optional[str] = "",
    isCompare: bool = False,
) -> types.CallToolResult:
    resolved_path = state.resolve_cluster_path(clusterPath)
    if not isinstance(resolved_path, str):
        return err(ValueError(f"Invalid cluster path: {resolved_path}"))

    params = {
        "clusterPath": resolved_path,
        "isCompare": isCompare,
    }
    try:
        body = await get_client().request(
            "communication/duration/iterations",
            "communication",
            params=params,
        )
        compare_list = []
        if isinstance(body, dict) and "iterationOrRankId" in body:
            compare_list = body["iterationOrRankId"].get("compare", [])

        result = {"iterationList": compare_list}
        return ok(result)
    except Exception as exc:
        return err(exc)


@require_events("parse/clusterCompleted", "parse/clusterStep2Completed")
async def communication_matrix_group(
    clusterPath: Optional[str] = "",
    iterationId: str = "",
    baselineIterationId: str = "",
    isCompare: bool = False,
) -> types.CallToolResult:
    resolved_path = state.resolve_cluster_path(clusterPath)
    if not isinstance(resolved_path, str):
        return err(ValueError(f"Invalid cluster path: {resolved_path}"))

    params = {
        "clusterPath": resolved_path,
        "iterationId": iterationId,
        "baselineIterationId": baselineIterationId,
        "isCompare": isCompare,
    }
    try:
        body = await get_client().request(
            "communication/matrix/group",
            "communication",
            params=params,
        )
        if isinstance(body, dict) and "data" in body and isinstance(body["data"], list):
            for item in body["data"]:
                if isinstance(item, dict) and "groupIdHash" in item and isinstance(item["groupIdHash"], dict):
                    item["groupIdHash"] = item["groupIdHash"].get("compare")

        return ok(body)
    except Exception as exc:
        return err(exc)
