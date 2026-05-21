"""Three-level state & cache management for the MCP server.

Level hierarchy
---------------
1. **Session**  — current conversation (session ID, event tracking, project registry).
2. **Project**  — per-project metadata (rank list, cluster path) and module states.
3. **Module**   — per-module state inside a project (e.g. timeline selected threads,
   cluster path, etc.).  Stored as plain dicts so each module can evolve independently.

Usage
-----
    from state import state

    # Session
    sid = state.session_id

    # Project
    ps = state.get_or_create_project("my_project", "/path/to/data")
    ps.set_import_result(import_result)

    # Module
    tl = ps.get_module("timeline")
    tl.set("selected_tid", "1234")

"""

from .session import SessionState, state

__all__ = ["SessionState", "state"]
