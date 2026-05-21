"""Tool descriptors and dispatcher for the timeline module."""

from __future__ import annotations

from typing import Any

from mcp import types

from utils.response import READ_ONLY_ANNOTATIONS

from .handler import (
    get_thread_detail,
    get_unit_flows,
    get_units_in_range,
    query_communication_kernel_detail,
)

TOOLS: list[types.Tool] = [
    types.Tool(
        name="query_communication_kernel_detail",
        description=(
            "Query kernel-level detail for a communication operator. "
            "Returns basic kernel information including id, pid, tid, startTime, and depth. "
            "Use this to locate a communication operator in the kernel execution timeline. "
            "project_name and file_path are auto-detected from the current project."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "rank_id": {
                    "type": "string",
                    "description": "Device ID (e.g. '0').",
                },
                "operator_name": {
                    "type": "string",
                    "description": "Communication operator name (e.g. 'AllReduce', 'AllGather').",
                },
                "file_path": {
                    "type": "string",
                    "description": "Optional override for the profiling database file path. Defaults to current project's file path.",
                },
                "cluster_path": {
                    "type": "string",
                    "description": "Optional override for the cluster path. Auto-detected if not provided.",
                },
            },
            "required": ["rank_id", "operator_name"],
        },
        outputSchema={
            "type": "object",
            "properties": {
                "id": {"type": "string", "description": "Kernel ID."},
                "rankId": {"type": "string", "description": "Device rank ID."},
                "depth": {"type": "integer", "description": "Call stack depth level."},
                "threadId": {"type": "string", "description": "Thread ID."},
                "pid": {"type": "string", "description": "Process ID."},
                "step": {"type": "string", "description": "Training step identifier."},
                "group": {"type": "string", "description": "Communication group name."},
                "startTime": {"type": "number", "description": "Kernel start timestamp in microseconds."},
            },
        },
        annotations=READ_ONLY_ANNOTATIONS,
    ),
    types.Tool(
        name="get_thread_detail",
        description=(
            "Retrieve thread detail data for a specific event/operator in the timeline. "
            "Returns the timeline context around the specified kernel, including sibling events "
            "and parent/child relationships. "
            "All parameters are auto-filled from the kernel_detail_cache (populated by query_communication_kernel_detail). "
            "In multi-card comparison scenarios, BOTH rank_id AND kernel_id MUST be passed together for exact cache lookup. "
            "The rank_id must be the complete value from query_communication_kernel_detail response (e.g. 'device_id_0 0')."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "kernel_id": {
                    "type": "string",
                    "description": "Kernel/operator ID to query detail for. Combined with rank_id for exact lookup in kernel_detail_cache.",
                },
                "rank_id": {
                    "type": "string",
                    "description": "Complete device rank ID from query_communication_kernel_detail response. Combined with kernel_id for exact cache lookup.",
                },
                "pid": {
                    "type": "string",
                    "description": "Process ID. Auto-filled from matched kernel.",
                },
                "tid": {
                    "type": "string",
                    "description": "Thread ID. Auto-filled from matched kernel.",
                },
                "start_time": {
                    "type": "integer",
                    "description": "Start time in microseconds. Auto-filled from matched kernel.",
                },
                "depth": {
                    "type": "integer",
                    "description": "Depth level in the call hierarchy. Auto-filled from matched kernel.",
                },
                "file_path": {
                    "type": "string",
                    "description": "Optional override for the profiling database file path. Defaults to current project's file path.",
                },
                "meta_type": {
                    "type": "string",
                    "description": "Meta type for the query. Default: 'HCCL'. Options: 'HCCL', 'COMMUNICATION'.",
                    "default": "HCCL",
                },
            },
            "required": [],
        },
        outputSchema={
            "type": "object",
            "properties": {
                "emptyFlag": {
                    "type": "boolean",
                    "description": "True if no thread detail data was found for the given parameters.",
                },
                "data": {
                    "type": "object",
                    "description": "Thread detail data for the specified kernel/event.",
                    "properties": {
                        "title": {"type": "string", "description": "Operator/event name."},
                        "selfTime": {"type": "number", "description": "Self (exclusive) time in microseconds."},
                        "duration": {"type": "number", "description": "Total (inclusive) duration in microseconds."},
                        "cat": {"type": "string", "description": "Event category."},
                        "args": {"type": "string", "description": "Additional arguments/metadata as JSON string."},
                        "rawStartTime": {"type": "string", "description": "Raw start timestamp as string."},
                        "rawEndTime": {"type": "string", "description": "Raw end timestamp as string."},
                        "inputShapes": {"type": "string", "description": "Input tensor shapes as JSON string."},
                        "inputDataTypes": {"type": "string", "description": "Input data types as JSON string."},
                        "inputFormats": {"type": "string", "description": "Input memory formats as JSON string."},
                        "outputShapes": {"type": "string", "description": "Output tensor shapes as JSON string."},
                        "outputDataTypes": {"type": "string", "description": "Output data types as JSON string."},
                        "outputFormats": {"type": "string", "description": "Output memory formats as JSON string."},
                        "attrInfo": {"type": "string", "description": "Operator attributes as JSON string."},
                    },
                },
            },
        },
        annotations=READ_ONLY_ANNOTATIONS,
    ),
    types.Tool(
        name="get_unit_flows",
        description=(
            "Retrieve flow data for a specific operator/event in the timeline. "
            "Shows the relationship and data flow between different events/kernels in the trace. "
            "Use this to understand causal dependencies between operators. "
            "Parameters are auto-filled from the kernel_detail_cache (populated by query_communication_kernel_detail). "
            "IMPORTANT: rank_id and op_id are BOTH required for cache lookup. In multi-card comparison, you must pass the complete "
            "rank_id (e.g. 'device_id_0 0') from query_communication_kernel_detail response AND the op_id from the same response. "
            "Without both, the wrong rank's cached data will be returned."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "rank_id": {
                    "type": "string",
                    "description": "Complete device rank ID from query_communication_kernel_detail response (e.g. 'device_id_0 0'). Must be passed with op_id for exact cache lookup.",
                },
                "tid": {
                    "type": "string",
                    "description": "Thread ID. Auto-filled from matched kernel.",
                },
                "pid": {
                    "type": "string",
                    "description": "Process ID. Auto-filled from matched kernel.",
                },
                "start_time": {
                    "type": "integer",
                    "description": "Start time in microseconds. Auto-filled from matched kernel.",
                },
                "op_id": {
                    "type": "string",
                    "description": "Operator/event ID from query_communication_kernel_detail response. Must be passed with rank_id for exact cache lookup.",
                },
                "file_path": {
                    "type": "string",
                    "description": "Optional override for the profiling database file path. Defaults to current project's file path.",
                },
                "meta_type": {
                    "type": "string",
                    "description": "Optional meta type filter (e.g. 'HCCL', 'COMMUNICATION').",
                },
                "is_simulation": {
                    "type": "boolean",
                    "description": "Whether to run in simulation mode (default: False).",
                    "default": False,
                },
            },
            "required": [],
        },
        outputSchema={
            "type": "object",
            "properties": {
                "unitAllFlows": {
                    "type": "array",
                    "description": "Flow categories, each containing a list of flow items showing causal dependencies between events.",
                    "items": {
                        "type": "object",
                        "properties": {
                            "cat": {"type": "string", "description": "Flow category name."},
                            "flows": {
                                "type": "array",
                                "description": "Individual flow items linking source and target events.",
                                "items": {
                                    "type": "object",
                                    "properties": {
                                        "title": {"type": "string", "description": "Flow label/name."},
                                        "cat": {"type": "string", "description": "Flow sub-category."},
                                        "id": {"type": "string", "description": "Flow identifier."},
                                        "from": {
                                            "type": "object",
                                            "description": "Source event metadata.",
                                            "properties": {
                                                "pid": {"type": "string", "description": "Process ID."},
                                                "tid": {"type": "string", "description": "Thread ID."},
                                                "id": {"type": "string", "description": "Event/kernel ID."},
                                                "name": {"type": "string", "description": "Event name."},
                                                "rankId": {"type": "string", "description": "Device rank ID."},
                                                "metaType": {
                                                    "type": "string",
                                                    "description": "Meta type (e.g. 'HCCL').",
                                                },
                                                "timestamp": {
                                                    "type": "number",
                                                    "description": "Event timestamp in microseconds.",
                                                },
                                                "duration": {
                                                    "type": "number",
                                                    "description": "Event duration in microseconds.",
                                                },
                                                "depth": {"type": "integer", "description": "Call stack depth."},
                                            },
                                        },
                                        "to": {
                                            "type": "object",
                                            "description": "Target event metadata (same fields as 'from').",
                                            "properties": {
                                                "pid": {"type": "string", "description": "Process ID."},
                                                "tid": {"type": "string", "description": "Thread ID."},
                                                "id": {"type": "string", "description": "Event/kernel ID."},
                                                "name": {"type": "string", "description": "Event name."},
                                                "rankId": {"type": "string", "description": "Device rank ID."},
                                                "metaType": {
                                                    "type": "string",
                                                    "description": "Meta type (e.g. 'HCCL').",
                                                },
                                                "timestamp": {
                                                    "type": "number",
                                                    "description": "Event timestamp in microseconds.",
                                                },
                                                "duration": {
                                                    "type": "number",
                                                    "description": "Event duration in microseconds.",
                                                },
                                                "depth": {"type": "integer", "description": "Call stack depth."},
                                            },
                                        },
                                    },
                                },
                            },
                        },
                    },
                },
            },
        },
        annotations=READ_ONLY_ANNOTATIONS,
    ),
    types.Tool(
        name="get_units_in_range",
        description=(
            "Retrieve list of operators (units) within a selected time range from timeline swimlanes. "
            "This tool queries the C++ backend for operators captured within a user-selected time range "
            "(brush selection) on the timeline view. Corresponds to the frontend `unit/threads` API. "
            "By default, extracts feature statistics (TOP10 by duration, TOP5 by occurrences, summary) "
            "for efficient slow-rank analysis. "
            "rank_id is required and must be provided explicitly."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "metadata_list": {
                    "type": "array",
                    "items": {
                        "type": "object",
                        "properties": {
                            "tid": {
                                "type": "string",
                                "description": "Thread ID.",
                            },
                            "pid": {
                                "type": "string",
                                "description": "Process ID.",
                            },
                            "metaType": {
                                "type": "string",
                                "description": "Metadata type (e.g. 'CANN_API', 'PYTORCH_API').",
                            },
                            "hidePythonFunction": {
                                "type": "boolean",
                                "description": "Whether to hide Python functions (default: false).",
                                "default": False,
                            },
                        },
                        "required": ["tid", "pid", "metaType"],
                    },
                    "description": "List of thread metadata to query.",
                },
                "rank_id": {
                    "type": "string",
                    "description": "Device rank ID (e.g. 'device_id_0 0'). Must be provided explicitly.",
                },
                "start_time": {
                    "type": "integer",
                    "description": "Start time in nanoseconds (absolute, not offset).",
                },
                "end_time": {
                    "type": "integer",
                    "description": "End time in nanoseconds (absolute, not offset).",
                },
                "file_path": {
                    "type": "string",
                    "description": "Optional override for the profiling database file path. Defaults to current project's file path.",
                },
                "start_depth": {
                    "type": "string",
                    "description": "Optional start depth for slice mode (e.g. '0').",
                },
                "end_depth": {
                    "type": "string",
                    "description": "Optional end depth for slice mode (e.g. '5').",
                },
                "extract_features": {
                    "type": "boolean",
                    "description": "Whether to extract feature statistics instead of returning raw data. Default: true.",
                    "default": True,
                },
            },
            "required": ["rank_id", "metadata_list", "start_time", "end_time"],
        },
        outputSchema={
            "type": "object",
            "properties": {
                "emptyFlag": {
                    "type": "boolean",
                    "description": "True if no operators were found in the selected time range.",
                },
                "features": {
                    "type": "object",
                    "description": "Present when extract_features=true. Aggregated feature statistics for operators in the selected time range.",
                    "properties": {
                        "top_10_by_duration": {
                            "type": "array",
                            "description": "Top 10 operators by wall duration.",
                            "items": {
                                "type": "object",
                                "properties": {
                                    "title": {"type": "string", "description": "Operator name."},
                                    "wallDuration": {"type": "number", "description": "Wall duration in microseconds."},
                                    "occurrences": {"type": "integer", "description": "Number of occurrences."},
                                },
                            },
                        },
                        "top_5_by_occurrences": {
                            "type": "array",
                            "description": "Top 5 operators by call count.",
                            "items": {
                                "type": "object",
                                "properties": {
                                    "title": {"type": "string", "description": "Operator name."},
                                    "occurrences": {"type": "integer", "description": "Number of occurrences."},
                                    "wallDuration": {"type": "number", "description": "Wall duration in microseconds."},
                                },
                            },
                        },
                        "summary": {
                            "type": "object",
                            "description": "Overall summary statistics.",
                            "properties": {
                                "total_operators": {"type": "integer", "description": "Total operator count."},
                                "total_wall_duration": {"type": "number", "description": "Sum of all wall durations."},
                                "avg_wall_duration": {"type": "number", "description": "Average wall duration."},
                                "total_occurrences": {
                                    "type": "integer",
                                    "description": "Total call count across all operators.",
                                },
                                "metatype_distribution": {
                                    "type": "object",
                                    "description": "Operator count grouped by meta type (e.g. 'CANN_API', 'PYTORCH_API').",
                                    "additionalProperties": {"type": "integer"},
                                },
                            },
                        },
                        "total_count": {"type": "integer", "description": "Total number of operators in the raw data."},
                        "time_range": {
                            "type": "object",
                            "description": "The queried time window.",
                            "properties": {
                                "start_time": {"type": "number", "description": "Start time in microseconds."},
                                "end_time": {"type": "number", "description": "End time in microseconds."},
                                "duration_us": {"type": "number", "description": "Window duration in microseconds."},
                            },
                        },
                        "rank_id": {"type": "string", "description": "Device rank ID."},
                    },
                },
                "data": {
                    "type": "array",
                    "description": "Present when extract_features=false. Raw operator list within the selected time range.",
                    "items": {
                        "type": "object",
                        "properties": {
                            "title": {"type": "string", "description": "Operator name."},
                            "wallDuration": {"type": "number", "description": "Wall duration in microseconds."},
                            "occurrences": {"type": "integer", "description": "Call count."},
                            "avgWallDuration": {
                                "type": "number",
                                "description": "Average wall duration in microseconds.",
                            },
                            "maxWallDuration": {
                                "type": "number",
                                "description": "Maximum wall duration in microseconds.",
                            },
                            "minWallDuration": {
                                "type": "number",
                                "description": "Minimum wall duration in microseconds.",
                            },
                            "selfTime": {"type": "number", "description": "Self (exclusive) time in microseconds."},
                            "metaTypeList": {
                                "type": "array",
                                "items": {"type": "string"},
                                "description": "List of meta types for this operator.",
                            },
                            "processes": {
                                "type": "array",
                                "description": "Process/thread info.",
                                "items": {
                                    "type": "object",
                                    "properties": {
                                        "pid": {"type": "string", "description": "Process ID."},
                                        "tidList": {
                                            "type": "array",
                                            "items": {"type": "string"},
                                            "description": "Thread IDs within this process.",
                                        },
                                    },
                                },
                            },
                        },
                    },
                },
            },
        },
        annotations=READ_ONLY_ANNOTATIONS,
    ),
]

DISPATCH: dict[str, Any] = {
    "query_communication_kernel_detail": query_communication_kernel_detail,
    "get_thread_detail": get_thread_detail,
    "get_unit_flows": get_unit_flows,
    "get_units_in_range": get_units_in_range,
    # "get_parse_cards": get_parse_cards,
    # "get_thread_list": get_thread_list,
    # "get_thread_detail": get_thread_detail,
    # "get_system_view": get_system_view,
    # "get_thread_traces": get_thread_traces,
    # "search_timeline_events": search_timeline_events,
    # "get_operator_duration": get_operator_duration,
    # "get_kernel_details": get_kernel_details,
}
