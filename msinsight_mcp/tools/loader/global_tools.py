"""
Handlers for the C++ backend's **global** module.

Exposed MCP tools
-----------------
- heartbeat               — keep-alive / connectivity check
- list_files              — list directory contents on the backend host
- get_module_config       — available analysis module configuration
- get_project_explorer    — all project entries known to the backend
- delete_project_data     — remove specific data paths from a project
- clear_projects          — clear one or more entire projects
- check_project_valid     — validate that project data paths still exist
- rename_project          — rename an existing project
"""

from __future__ import annotations

from typing import Any

from mcp import types

from cpp_client import get_client
from utils.errors import CppBackendError, NotConnectedError, RequestTimeoutError
from utils.logger import logger
from utils.response import fmt_json, err


# --------------------------------------------------------------------
# Tool implementations
# --------------------------------------------------------------------


async def heartbeat() -> list[types.TextContent]:
    """Check whether the C++ profiling backend is reachable and alive."""
    try:
        client = get_client()
        await client.request("heartCheck", "global")
        logger.info("Heartbeat OK")
        return [types.TextContent(type="text", text="OK – C++ backend is alive")]
    except (CppBackendError, NotConnectedError, RequestTimeoutError) as exc:
        return err(exc)


async def list_files(path: str) -> list[types.TextContent]:
    """List files and directories at *path* on the backend host.

    Parameters
    ----------
    path: Absolute path on the machine running the C++ backend.
    """
    try:
        body = await get_client().request("files/get", "global", params={"path": path})
        return [types.TextContent(type="text", text=fmt_json(body))]
    except Exception as exc:
        return err(exc)


async def get_module_config() -> list[types.TextContent]:
    """Retrieve the list of analysis modules available on the backend."""
    try:
        body = await get_client().request("moduleConfig/get", "global")
        return [types.TextContent(type="text", text=fmt_json(body))]
    except Exception as exc:
        return err(exc)


async def get_project_explorer() -> list[types.TextContent]:
    """Return all projects currently registered in the backend's project explorer."""
    try:
        body = await get_client().request("files/getProjectExplorer", "global")
        return [types.TextContent(type="text", text=fmt_json(body))]
    except Exception as exc:
        return err(exc)


async def delete_project_data(project_name: str, data_paths: list[str]) -> list[types.TextContent]:
    """Remove specific data paths from a project in the backend.

    Parameters
    ----------
    project_name: Name of the target project.
    data_paths:   List of data path strings to remove.
    """
    try:
        body = await get_client().request(
            "files/deleteProjectExplorer",
            "global",
            params={"projectName": project_name, "dataPath": data_paths},
        )
        return [types.TextContent(type="text", text=fmt_json(body or {"status": "deleted"}))]
    except Exception as exc:
        return err(exc)


async def clear_projects(project_names: list[str]) -> list[types.TextContent]:
    """Clear (reset) one or more entire projects on the backend.

    Parameters
    ----------
    project_names: List of project names to clear.
    """
    try:
        body = await get_client().request(
            "files/clearProjectExplorer",
            "global",
            params={"projectNameList": project_names},
        )
        return [types.TextContent(type="text", text=fmt_json(body or {"status": "cleared"}))]
    except Exception as exc:
        return err(exc)


async def check_project_valid(project_name: str, data_paths: list[str]) -> list[types.TextContent]:
    """Validate that data paths within a project still exist on disk.

    Parameters
    ----------
    project_name: Name of the project to validate.
    data_paths:   List of data path strings to check.
    """
    try:
        body = await get_client().request(
            "files/checkProjectValid",
            "global",
            params={"projectName": project_name, "dataPath": data_paths},
        )
        return [types.TextContent(type="text", text=fmt_json(body))]
    except Exception as exc:
        return err(exc)


async def rename_project(old_project_name: str, new_project_name: str) -> list[types.TextContent]:
    """Rename an existing project in the backend's project explorer.

    Parameters
    ----------
    old_project_name: Current name of the project.
    new_project_name: Desired new name for the project.
    """
    try:
        body = await get_client().request(
            "files/updateProjectExplorer",
            "global",
            params={
                "oldProjectName": old_project_name,
                "newProjectName": new_project_name,
            },
        )
        return [types.TextContent(type="text", text=fmt_json(body or {"status": "renamed"}))]
    except Exception as exc:
        return err(exc)


# --------------------------------------------------------------------
# Tool descriptor registry (consumed by mcp_server.py)
# --------------------------------------------------------------------

TOOLS: list[types.Tool] = [
    types.Tool(
        name="heartbeat",
        description=(
            "Check whether the MSInsight C++ profiling backend is reachable and responsive. Returns 'OK' on success."
        ),
        inputSchema={"type": "object", "properties": {}, "required": []},
    ),
    # types.Tool(
    #     name="list_files",
    #     description=(
    #         "List files and sub-directories at the given path on the machine "
    #         "running the C++ profiling backend."
    #     ),
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "path": {
    #                 "type": "string",
    #                 "description": "Absolute directory path on the backend host.",
    #             }
    #         },
    #         "required": ["path"],
    #     },
    # ),
    # types.Tool(
    #     name="get_module_config",
    #     description="Retrieve the list of analysis modules available on the backend.",
    #     inputSchema={"type": "object", "properties": {}, "required": []},
    # ),
    # types.Tool(
    #     name="get_project_explorer",
    #     description=(
    #         "Return all projects currently registered in the backend's "
    #         "project explorer, including their data file paths."
    #     ),
    #     inputSchema={"type": "object", "properties": {}, "required": []},
    # ),
    # types.Tool(
    #     name="delete_project_data",
    #     description="Remove specific data paths from a named project on the backend.",
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_name": {"type": "string", "description": "Target project name."},
    #             "data_paths": {
    #                 "type": "array",
    #                 "items": {"type": "string"},
    #                 "description": "List of data path strings to remove.",
    #             },
    #         },
    #         "required": ["project_name", "data_paths"],
    #     },
    # ),
    # types.Tool(
    #     name="clear_projects",
    #     description="Clear (reset) one or more entire projects on the backend.",
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_names": {
    #                 "type": "array",
    #                 "items": {"type": "string"},
    #                 "description": "Names of the projects to clear.",
    #             }
    #         },
    #         "required": ["project_names"],
    #     },
    # ),
    # types.Tool(
    #     name="check_project_valid",
    #     description=(
    #         "Validate that the data paths of a project still exist on disk. "
    #         "Returns per-path validity information."
    #     ),
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "project_name": {"type": "string"},
    #             "data_paths": {
    #                 "type": "array",
    #                 "items": {"type": "string"},
    #             },
    #         },
    #         "required": ["project_name", "data_paths"],
    #     },
    # ),
    # types.Tool(
    #     name="rename_project",
    #     description="Rename an existing project in the backend's project explorer.",
    #     inputSchema={
    #         "type": "object",
    #         "properties": {
    #             "old_project_name": {"type": "string", "description": "Current project name."},
    #             "new_project_name": {"type": "string", "description": "New project name."},
    #         },
    #         "required": ["old_project_name", "new_project_name"],
    #     },
    # ),
]

# Map tool name → handler function for dispatch in mcp_server.py
DISPATCH: dict[str, Any] = {
    "heartbeat": heartbeat,
    # "list_files": list_files,
    # "get_module_config": get_module_config,
    # "get_project_explorer": get_project_explorer,
    # "delete_project_data": delete_project_data,
    # "clear_projects": clear_projects,
    # "check_project_valid": check_project_valid,
    # "rename_project": rename_project,
}
