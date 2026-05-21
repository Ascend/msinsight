"""Tool descriptors and dispatcher for the loader module."""

from __future__ import annotations

from typing import Any

from mcp import types

from .handler import import_trace_file

TOOLS: list[types.Tool] = [
    types.Tool(
        name="import_trace_file",
        description=(
            "Import / load a trace or profile file into the C++ profiling backend. "
            "The backend parses the file asynchronously. "
            "After calling this tool, poll 'get_parse_cards' to confirm completion."
        ),
        inputSchema={
            "type": "object",
            "properties": {
                "project_name": {
                    "type": "string",
                    "description": "Logical project name to associate with this trace.",
                },
                "file_path": {
                    "type": "string",
                    "description": "Absolute path to the trace file on the backend host.",
                },
            },
            "required": ["project_name", "file_path"],
        },
        outputSchema={
            "type": "object",
            "properties": {
                "message": {
                    "type": "string",
                    "description": "Import status: 'succeeded', 'pending', or error message.",
                },
            },
        },
        annotations=types.ToolAnnotations(
            readOnlyHint=False,
            idempotentHint=True,
            openWorldHint=True,
        ),
    ),
]

DISPATCH: dict[str, Any] = {
    "import_trace_file": import_trace_file,
}
