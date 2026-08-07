from __future__ import annotations

import asyncio
import importlib
import sys
from types import ModuleType

from mcp import types


try:
    import websockets  # noqa: F401
except ModuleNotFoundError:
    websockets_stub = ModuleType("websockets")
    exceptions_stub = ModuleType("websockets.exceptions")

    class ConnectionClosed(Exception):
        pass

    exceptions_stub.ConnectionClosed = ConnectionClosed
    websockets_stub.WebSocketClientProtocol = object
    websockets_stub.WebSocketServerProtocol = object
    websockets_stub.exceptions = exceptions_stub
    sys.modules["websockets"] = websockets_stub
    sys.modules["websockets.exceptions"] = exceptions_stub

try:
    import loguru  # noqa: F401
except ModuleNotFoundError:
    loguru_stub = ModuleType("loguru")

    class LoggerStub:
        def __getattr__(self, name):
            return lambda *args, **kwargs: None

    loguru_stub.logger = LoggerStub()
    sys.modules["loguru"] = loguru_stub


def test_package_modules_import_from_repo_parent() -> None:
    for module_name in (
        "msinsight_mcp.main",
        "msinsight_mcp.mcp_server",
        "msinsight_mcp.tools",
        "msinsight_mcp.tools.loader.global_tools",
        "msinsight_mcp.tools.operator",
    ):
        assert importlib.import_module(module_name)


def test_profiler_host_and_port_are_environment_configurable(monkeypatch) -> None:
    from msinsight_mcp.config import Settings

    monkeypatch.setenv("MSINSIGHT_CPP_BACKEND_HOST", "profiler.internal")
    monkeypatch.setenv("MSINSIGHT_CPP_BACKEND_PORT", "19000")

    configured = Settings(_env_file=None)

    assert configured.cpp_backend_url == "ws://profiler.internal:19000/"


def test_registered_tools_match_dispatch_and_include_implemented_tools() -> None:
    from msinsight_mcp.tools import ALL_DISPATCH, ALL_TOOLS

    tool_names = {tool.name for tool in ALL_TOOLS}

    assert tool_names == set(ALL_DISPATCH)
    assert {"heartbeat", "get_memory_usage", "get_memory_operators", "get_memory_leaks"} <= tool_names
    assert "get_parse_cards" not in tool_names


def test_call_tool_dispatches_call_tool_result(monkeypatch) -> None:
    from msinsight_mcp import mcp_server

    expected = types.CallToolResult(
        content=[types.TextContent(type="text", text="pending")],
        structuredContent={"message": "pending"},
        isError=False,
    )

    async def handler(value: str) -> types.CallToolResult:
        assert value == "trace"
        return expected

    monkeypatch.setitem(mcp_server.ALL_DISPATCH, "test_dispatch", handler)

    assert asyncio.run(mcp_server.call_tool("test_dispatch", {"value": "trace"})) == expected


def test_import_trace_file_treats_empty_body_as_pending(monkeypatch) -> None:
    from msinsight_mcp.state import state
    from msinsight_mcp.tools.loader import handler

    async def empty_import(project_name: str, file_path: str) -> None:
        return None

    state.reset()
    monkeypatch.setattr(handler, "import_trace_file_api", empty_import)

    result = asyncio.run(handler.import_trace_file("demo", "/tmp/trace/"))

    assert result.isError is False
    assert result.structuredContent == {"message": "Import pending for project 'demo'."}
    assert state.current_project is not None
    assert state.current_project.file_path == "/tmp/trace"
    assert state.current_project.rank_list == []
    state.reset()


def test_disconnected_client_starts_background_reconnect(monkeypatch) -> None:
    from msinsight_mcp.cpp_client import CppBackendClient

    async def scenario() -> None:
        client = CppBackendClient(
            "ws://profiler.invalid/",
            reconnect_interval=0,
            keepalive_interval=0,
        )
        attempted = asyncio.Event()

        async def reconnect_once() -> None:
            attempted.set()
            client._closing = True

        monkeypatch.setattr(client, "_do_connect", reconnect_once)
        client.start_background_tasks()

        await asyncio.wait_for(attempted.wait(), timeout=1)
        assert client._receive_task is not None
        await client._receive_task

    asyncio.run(scenario())


def test_main_starts_mcp_while_initial_connection_reconnects(monkeypatch) -> None:
    from msinsight_mcp import cpp_client, main
    from msinsight_mcp.utils.errors import BackendConnectionError

    async def scenario() -> None:
        client = cpp_client.CppBackendClient(
            "ws://profiler.invalid/",
            reconnect_interval=0,
            keepalive_interval=0,
        )
        reconnect_attempted = asyncio.Event()
        mcp_started = asyncio.Event()

        async def reconnect_once() -> None:
            reconnect_attempted.set()
            client._closing = True

        async def failed_initialise(**kwargs):
            cpp_client._client = client
            raise BackendConnectionError("initial connection failed")

        async def run_stdio() -> None:
            mcp_started.set()
            await asyncio.wait_for(reconnect_attempted.wait(), timeout=1)

        monkeypatch.setattr(client, "_do_connect", reconnect_once)
        monkeypatch.setattr(cpp_client, "initialise", failed_initialise)
        monkeypatch.setattr(main, "start_profiler_server_if_needed", lambda: None)
        monkeypatch.setattr(main.mcp_server, "run_stdio", run_stdio)
        monkeypatch.setattr(main.settings, "mcp_transport", "stdio")

        await main._main()

        assert mcp_started.is_set()
        assert reconnect_attempted.is_set()
        assert cpp_client._client is None

    asyncio.run(scenario())


def test_transport_shutdown_awaits_cancelled_server_task() -> None:
    from msinsight_mcp import main

    async def scenario() -> None:
        shutdown = asyncio.Event()
        server_started = asyncio.Event()
        server_stopped = asyncio.Event()

        async def server() -> None:
            server_started.set()
            try:
                await asyncio.Future()
            finally:
                await asyncio.sleep(0)
                server_stopped.set()

        lifecycle = asyncio.create_task(main._run_until_shutdown(server(), shutdown))
        await asyncio.wait_for(server_started.wait(), timeout=1)
        shutdown.set()
        await asyncio.wait_for(lifecycle, timeout=1)

        assert server_stopped.is_set()

    asyncio.run(scenario())
