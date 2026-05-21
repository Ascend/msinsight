"""Mapping layer — wraps C++ backend API calls.

This module is the sole place where ``cpp_client`` is imported.
All tool handlers in ``tools/`` should call functions defined here
instead of talking to the backend directly.
"""

from cpp_client import get_client


async def import_trace_file_api(project_name: str, file_path: str) -> dict:
    """Dispatch a trace/profile file import to the C++ backend."""
    return await get_client().request(
        "import/action",
        "timeline",
        params={"path": [file_path], "projectAction": 1, "projectName": project_name},
        project_name=project_name,
    )
