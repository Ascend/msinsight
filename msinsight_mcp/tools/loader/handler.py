"""Handler implementation for the loader module."""

from __future__ import annotations

from mcp import types

from mapping.framework import import_trace_file_api
from state import state


async def import_trace_file(
    project_name: str,
    file_path: str,
) -> types.CallToolResult:
    """Import / load a trace or profile file into the C++ backend."""
    try:
        file_path = file_path.rstrip("/")
        body = await import_trace_file_api(project_name, file_path)
        ps = state.get_or_create_project(project_name, file_path)
        state.set_current_project(project_name)
        ps.set_import_result(body)
        status = "succeeded" if body else "pending"
        result = {"message": f"Import {status} for project '{project_name}'."}
        return types.CallToolResult(
            content=[types.TextContent(type="text", text=str(result["message"]))],
            structuredContent=result,
            isError=False,
        )
    except Exception as exc:
        return types.CallToolResult(
            content=[types.TextContent(type="text", text=f"ERROR: {exc}")],
            isError=True,
        )
