"""Tool descriptors and dispatcher for the cluster module."""

from __future__ import annotations

from typing import Any

from mcp import types

from utils.response import READ_ONLY_ANNOTATIONS

from .handler import (
    communication_duration_iterations,
    communication_duration_slow_rank_list,
    communication_matrix_group,
)

TOOLS: list[types.Tool] = [
    types.Tool(
        name="slow_rank_list",
        description=(
            "Get the slowest ranks (cards) ranked by communication operator duration. "
            "Use this when analyzing slow-rank issues in distributed training. "
            "Requires clusterPath from a completed cluster parse."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "operatorName": {
                    "type": "string",
                    "description": "Name of the communication operator to analyze (e.g. 'Total Op Info', 'AllReduce', 'AllGather').",
                },
                "stage": {
                    "type": "string",
                    "description": "Communication group member list, from the 'group' field of matrix_group result (e.g. '(0,1,2,3,4,5,6,7)').",
                },
                "clusterPath": {
                    "type": "string",
                    "description": "Path to the cluster data directory. If not provided, the tool will auto-resolve from parsed clusters.",
                },
                "iterationId": {
                    "type": "string",
                    "description": "Required. ID of the training iteration to analyze.",
                },
                "rankList": {
                    "type": "array",
                    "items": {"type": "string"},
                    "description": "Optional list of rank IDs to filter. Leave empty for all ranks.",
                },
                "targetOperatorName": {
                    "type": "string",
                    "description": "Optional. Name of a specific sub-operator to drill into.",
                },
                "isCompare": {
                    "type": "boolean",
                    "description": "Enable comparison mode to compare against a baseline iteration.",
                },
                "baselineIterationId": {
                    "type": "string",
                    "description": "Required when isCompare=true. Baseline iteration ID for comparison.",
                },
                "pgName": {
                    "type": "string",
                    "description": "Process group name, from the 'parallelStrategy' field of matrix_group result (e.g. 'default_group', 'dp', 'mp', 'tp').",
                },
                "groupIdHash": {
                    "type": "string",
                    "description": "Required. Hash identifier of the communication group to analyze.",
                },
                "baselineGroupIdHash": {
                    "type": "string",
                    "description": "Required when isCompare=true. Baseline group hash for comparison.",
                },
            },
            "required": ["iterationId", "groupIdHash"],
        },
        outputSchema={
            "type": "object",
            "properties": {
                "hasAdvice": {
                    "type": "boolean",
                    "description": "Whether there are actionable optimization suggestions for the slow ranks.",
                },
                "fastRankId": {
                    "type": "string",
                    "description": "ID of the fastest rank (least communication time).",
                },
                "fastTotalElapseTime": {
                    "type": "number",
                    "description": "Total communication elapsed time of the fastest rank, in microseconds.",
                },
                "data": {
                    "type": "array",
                    "description": "List of slow ranks sorted by total elapsed time descending, each with per-operator breakdown.",
                    "items": {
                        "type": "object",
                        "properties": {
                            "rankId": {"type": "string", "description": "Rank ID."},
                            "totalDiffTime": {
                                "type": "number",
                                "description": "Total time delta between this rank and the fastest rank, in microseconds.",
                            },
                            "totalElapseTime": {
                                "type": "number",
                                "description": "Total communication elapsed time for this rank, in microseconds.",
                            },
                            "opList": {
                                "type": "array",
                                "description": "Per-operator timing breakdown for this rank.",
                                "items": {
                                    "type": "object",
                                    "properties": {
                                        "name": {"type": "string", "description": "Operator name."},
                                        "startTime": {
                                            "type": "number",
                                            "description": "Operator start time in microseconds.",
                                        },
                                        "diffTime": {
                                            "type": "number",
                                            "description": "Time delta between this operator and the fastest rank's same operator, in microseconds.",
                                        },
                                        "elapseTime": {
                                            "type": "number",
                                            "description": "Operator elapsed time in microseconds.",
                                        },
                                        "maxTime": {
                                            "type": "number",
                                            "description": "Elapsed time of the same operator on the fastest rank, in microseconds.",
                                        },
                                        "maxStartTime": {
                                            "type": "number",
                                            "description": "Start time of the same operator on the fastest rank, in microseconds.",
                                        },
                                    },
                                },
                            },
                        },
                    },
                },
            },
            "required": ["hasAdvice", "fastRankId", "fastTotalElapseTime", "data"],
        },
        annotations=READ_ONLY_ANNOTATIONS,
    ),
    types.Tool(
        name="duration_iterations",
        description=(
            "Get training iteration data for the communication duration analysis. "
            "Use this to discover which iterationId values are available before calling slow_rank_list or matrix_group."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "clusterPath": {
                    "type": "string",
                    "description": "Path to the cluster data directory. Auto-resolves if not provided.",
                },
                "isCompare": {
                    "type": "boolean",
                    "description": "Whether to return comparison-mode iteration data (includes baseline iteration info).",
                },
            },
            "required": [],
        },
        outputSchema={
            "type": "object",
            "properties": {
                "iterationList": {
                    "type": "array",
                    "description": "List of iteration IDs in the current dataset.",
                    "items": {"type": "string"},
                },
            },
            "required": ["iterationList"],
        },
        annotations=READ_ONLY_ANNOTATIONS,
    ),
    types.Tool(
        name="matrix_group",
        description=(
            "Get communication matrix group data showing communication patterns between ranks. "
            "Use this to analyze collective communication topology and identify bottleneck ranks."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "clusterPath": {
                    "type": "string",
                    "description": "Path to the cluster data directory. Auto-resolves if not provided.",
                },
                "iterationId": {
                    "type": "string",
                    "description": "Required. Training iteration to analyze.",
                },
                "baselineIterationId": {
                    "type": "string",
                    "description": "Required when isCompare=true. Baseline iteration for comparison.",
                },
                "isCompare": {
                    "type": "boolean",
                    "description": "Enable comparison mode.",
                },
            },
            "required": ["iterationId"],
        },
        outputSchema={
            "type": "object",
            "properties": {
                "data": {
                    "type": "array",
                    "description": "List of communication matrix groups with their parallel strategy and group ID hashes.",
                    "items": {
                        "type": "object",
                        "properties": {
                            "group": {
                                "type": "string",
                                "description": "Group name (e.g. 'dp', 'tp', 'pp').",
                            },
                            "parallelStrategy": {
                                "type": "string",
                                "description": "Parallel strategy type for this group.",
                            },
                            "type": {
                                "type": "string",
                                "description": "Dataset type: 'COMPARE' or 'BASELINE'.",
                            },
                            "groupIdHash": {
                                "type": "string",
                                "description": "Hash identifier string used as groupIdHash in subsequent tool calls.",
                            },
                        },
                    },
                },
            },
            "required": ["data"],
        },
        annotations=READ_ONLY_ANNOTATIONS,
    ),
]

DISPATCH: dict[str, Any] = {
    "slow_rank_list": communication_duration_slow_rank_list,
    "duration_iterations": communication_duration_iterations,
    "matrix_group": communication_matrix_group,
}
