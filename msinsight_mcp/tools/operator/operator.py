"""
Handlers for the C++ backend's **operator**, **memory**, and **summary** modules.

Exposed MCP tools
-----------------
Operator analysis:
- get_operator_categories   — list operator category groups
- get_operator_statistics   — aggregated per-operator timing statistics
- get_operator_details      — detailed records for a specific operator

Memory profiling:
- get_memory_usage          — per-step memory usage over time
- get_memory_operators      — operator-level memory allocation breakdown
- get_memory_leaks          — memory leak block summary (MemScope)

Performance summary:
- get_summary_top_data      — top-N hotspot operators or communication ops
- get_summary_statistics    — overall performance statistics dashboard
- get_communication_advisor — AI-generated communication performance advice
"""

from __future__ import annotations

import json
from typing import Any, Optional

from mcp import types

from cpp_client import get_client


def _fmt(data: Any) -> str:
    return json.dumps(data, indent=2, ensure_ascii=False)


def _error_text(exc: Exception) -> list[types.TextContent]:
    return [types.TextContent(type="text", text=f"ERROR: {exc}")]


# --------------------------------------------------------------------
# Operator module tools
# --------------------------------------------------------------------


async def get_operator_categories(
    project_name: str,
    file_path: str,
) -> list[types.TextContent]:
    """Return the list of operator category groups present in the trace.

    Parameters
    ----------
    project_name: Project name.
    file_path:    Trace file path.
    """
    try:
        body = await get_client().request(
            "operator/category",
            "operator",
            params={},
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


async def get_operator_statistics(
    project_name: str,
    file_path: str,
    compute_unit: Optional[str] = None,
    category: Optional[str] = None,
) -> list[types.TextContent]:
    """Return aggregated per-operator timing statistics (count, total time, avg, ratio).

    Parameters
    ----------
    project_name: Project name.
    file_path:    Trace file path.
    compute_unit: Optional filter by compute unit (e.g. 'AiCore', 'AiCpu').
    category:     Optional filter by operator category.
    """
    try:
        params: dict[str, Any] = {}
        if compute_unit:
            params["computeUnit"] = compute_unit
        if category:
            params["category"] = category

        body = await get_client().request(
            "operator/statistic",
            "operator",
            params=params,
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


async def get_operator_details(
    project_name: str,
    file_path: str,
    operator_name: str,
    compute_unit: Optional[str] = None,
) -> list[types.TextContent]:
    """Return detailed profiling records for a specific operator.

    Parameters
    ----------
    project_name:  Project name.
    file_path:     Trace file path.
    operator_name: Operator name to query.
    compute_unit:  Optional compute unit filter.
    """
    try:
        params: dict[str, Any] = {"operatorName": operator_name}
        if compute_unit:
            params["computeUnit"] = compute_unit

        body = await get_client().request(
            "operator/details",
            "operator",
            params=params,
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


# --------------------------------------------------------------------
# Memory module tools
# --------------------------------------------------------------------


async def get_memory_usage(
    project_name: str,
    file_path: str,
    resource_type: Optional[str] = None,
) -> list[types.TextContent]:
    """Return memory usage over time, optionally filtered by resource type.

    Parameters
    ----------
    project_name:  Project name.
    file_path:     Trace file path.
    resource_type: Optional filter (e.g. 'HBM', 'workspace').
    """
    try:
        params: dict[str, Any] = {}
        if resource_type:
            params["resourceType"] = resource_type

        body = await get_client().request(
            "Memory/view/memoryUsage",
            "memory",
            params=params,
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


async def get_memory_operators(
    project_name: str,
    file_path: str,
) -> list[types.TextContent]:
    """Return the operator-level memory allocation breakdown.

    Shows which operators allocate the most memory, broken down by
    static and dynamic allocations.

    Parameters
    ----------
    project_name: Project name.
    file_path:    Trace file path.
    """
    try:
        body = await get_client().request(
            "Memory/view/operator",
            "memory",
            params={},
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


async def get_memory_leaks(
    project_name: str,
    file_path: str,
) -> list[types.TextContent]:
    """Return the memory leak block summary from the MemScope leak detector.

    Parameters
    ----------
    project_name: Project name.
    file_path:    Trace file path (MemScope output file).
    """
    try:
        body = await get_client().request(
            "Memory/leaks/blocks",
            "leaks",
            params={},
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


# --------------------------------------------------------------------
# Summary / performance module tools
# --------------------------------------------------------------------


async def get_summary_top_data(
    project_name: str,
    file_path: str,
    top_n: int = 10,
) -> list[types.TextContent]:
    """Return the top-N hotspot operators or communication operations.

    Parameters
    ----------
    project_name: Project name.
    file_path:    Trace file path.
    top_n:        Number of top entries to return (default 10).
    """
    try:
        body = await get_client().request(
            "summary/queryTopData",
            "summary",
            params={"topN": top_n},
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


async def get_summary_statistics(
    project_name: str,
    file_path: str,
) -> list[types.TextContent]:
    """Return the overall performance statistics dashboard.

    Includes compute/communication ratio, average step time, device
    utilisation, and key bottleneck indicators.

    Parameters
    ----------
    project_name: Project name.
    file_path:    Trace file path.
    """
    try:
        body = await get_client().request(
            "summary/statistic",
            "summary",
            params={},
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


async def get_communication_advisor(
    project_name: str,
    file_path: str,
) -> list[types.TextContent]:
    """Return AI-generated advice for improving communication performance.

    Analyses collective communication patterns (AllReduce, AllGather, etc.)
    and returns actionable optimisation suggestions.

    Parameters
    ----------
    project_name: Project name.
    file_path:    Trace file path.
    """
    try:
        body = await get_client().request(
            "communication/advisor",
            "communication",
            params={},
            project_name=project_name,
            file_id=file_path,
        )
        return [types.TextContent(type="text", text=_fmt(body))]
    except Exception as exc:
        return _error_text(exc)


# --------------------------------------------------------------------
# Tool descriptor registry
# --------------------------------------------------------------------

TOOLS: list[types.Tool] = [
    # --- Operator tools ---
    # types.Tool(
    #     name="get_operator_categories",
    #     description="Return the list of operator category groups present in the trace.",
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_name": {"type": "string"},
    #             "file_path": {"type": "string"},
    #         },
    #         "required": ["project_name", "file_path"],
    #     },
    # ),
    # types.Tool(
    #     name="get_operator_statistics",
    #     description=(
    #         "Return aggregated per-operator timing statistics: call count, "
    #         "total execution time, average time, and time ratio. "
    #         "Optionally filter by compute unit or operator category."
    #     ),
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_name": {"type": "string"},
    #             "file_path": {"type": "string"},
    #             "compute_unit": {
    #                 "type": "string",
    #                 "description": "Optional filter: 'AiCore', 'AiCpu', etc.",
    #             },
    #             "category": {
    #                 "type": "string",
    #                 "description": "Optional operator category filter.",
    #             },
    #         },
    #         "required": ["project_name", "file_path"],
    #     },
    # ),
    # types.Tool(
    #     name="get_operator_details",
    #     description=(
    #         "Return detailed profiling records (input/output shapes, duration, "
    #         "hardware counters) for every invocation of a named operator."
    #     ),
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_name": {"type": "string"},
    #             "file_path": {"type": "string"},
    #             "operator_name": {
    #                 "type": "string",
    #                 "description": "Exact operator name to query.",
    #             },
    #             "compute_unit": {
    #                 "type": "string",
    #                 "description": "Optional compute unit filter.",
    #             },
    #         },
    #         "required": ["project_name", "file_path", "operator_name"],
    #     },
    # ),
    # --- Memory tools ---
    types.Tool(
        name="get_memory_usage",
        description=(
            "Return memory usage over the profiling time range. "
            "Optionally filter by resource type (e.g. HBM, workspace)."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "project_name": {"type": "string"},
                "file_path": {"type": "string"},
                "resource_type": {
                    "type": "string",
                    "description": "Optional memory resource type filter.",
                },
            },
            "required": ["project_name", "file_path"],
        },
    ),
    types.Tool(
        name="get_memory_operators",
        description=(
            "Return the per-operator memory allocation breakdown, showing which "
            "operators consume the most memory and distinguishing static from dynamic allocations."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "project_name": {"type": "string"},
                "file_path": {"type": "string"},
            },
            "required": ["project_name", "file_path"],
        },
    ),
    types.Tool(
        name="get_memory_leaks",
        description=(
            "Return the memory leak block summary detected by MemScope. "
            "Use this tool when the trace file was collected with memory leak tracking enabled."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "project_name": {"type": "string"},
                "file_path": {"type": "string"},
            },
            "required": ["project_name", "file_path"],
        },
    ),
    # # --- Summary tools ---
    # types.Tool(
    #     name="get_summary_top_data",
    #     description=(
    #         "Return the top-N hotspot operators or communication operations "
    #         "ranked by total execution time."
    #     ),
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_name": {"type": "string"},
    #             "file_path": {"type": "string"},
    #             "top_n": {
    #                 "type": "integer",
    #                 "description": "Number of top entries to return (default 10).",
    #                 "default": 10,
    #             },
    #         },
    #         "required": ["project_name", "file_path"],
    #     },
    # ),
    # types.Tool(
    #     name="get_summary_statistics",
    #     description=(
    #         "Return the overall performance statistics dashboard: compute/communication "
    #         "ratio, average step time, device utilisation, and bottleneck indicators."
    #     ),
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_name": {"type": "string"},
    #             "file_path": {"type": "string"},
    #         },
    #         "required": ["project_name", "file_path"],
    #     },
    # ),
    # types.Tool(
    #     name="get_communication_advisor",
    #     description=(
    #         "Return AI-generated advice for improving collective communication "
    #         "performance (AllReduce, AllGather, ReduceScatter, etc.)."
    #     ),
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_name": {"type": "string"},
    #             "file_path": {"type": "string"},
    #         },
    #         "required": ["project_name", "file_path"],
    #     },
    # ),
]

DISPATCH: dict[str, Any] = {
    # "get_operator_categories": get_operator_categories,
    # "get_operator_statistics": get_operator_statistics,
    # "get_operator_details": get_operator_details,
    "get_memory_usage": get_memory_usage,
    "get_memory_operators": get_memory_operators,
    "get_memory_leaks": get_memory_leaks,
    # "get_summary_top_data": get_summary_top_data,
    # "get_summary_statistics": get_summary_statistics,
    # "get_communication_advisor": get_communication_advisor,
}
