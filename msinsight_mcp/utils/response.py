"""Shared helpers for formatting MCP tool responses."""

from __future__ import annotations

import json
from typing import Any

from mcp import types


def fmt_json(data: Any) -> str:
    """Serialise arbitrary data to a pretty JSON string."""
    return json.dumps(data, indent=2, ensure_ascii=False)


def ok(body: dict) -> types.CallToolResult:
    """Format a successful result as a CallToolResult."""
    return types.CallToolResult(
        content=[types.TextContent(type="text", text=fmt_json(body))],
        structuredContent=body,
        isError=False,
    )


def err(exc: Exception) -> types.CallToolResult:
    """Format an exception as an error CallToolResult."""
    return types.CallToolResult(
        content=[types.TextContent(type="text", text=f"ERROR: {exc}")],
        isError=True,
    )


# Shared annotations for read-only, idempotent, open-world tools.
READ_ONLY_ANNOTATIONS = types.ToolAnnotations(
    readOnlyHint=True,
    idempotentHint=True,
    openWorldHint=True,
)
