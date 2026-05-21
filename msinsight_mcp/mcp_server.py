"""
MCP protocol server implementation for the MSInsight C++ profiling bridge.

This module owns the ``mcp.server.Server`` instance and wires it up with
all tool handlers from the ``handlers`` package.  It also provides three
transport runners:

- ``run_stdio()``     — stdio transport (Claude Desktop / local CLI)
- ``run_sse()``       — HTTP + SSE transport (remote LangChain / web clients)
- ``run_websocket()`` — raw WebSocket transport (LangChain WebSocket mode)

The active transport is chosen via ``config.settings.mcp_transport``.

WebSocket transport implementation
-----------------------------------
The MCP protocol is JSON-RPC 2.0.  We bridge each WebSocket connection to
the ``mcp.server.Server`` using ``anyio`` in-memory object streams:

    WebSocket ──► ws_to_mcp (MemoryObjectSendStream) ──► Server.run()
    WebSocket ◄── mcp_to_ws (MemoryObjectReceiveStream) ◄── Server.run()

Each connected client gets its own ``Server.run()`` coroutine so sessions
are fully isolated.
"""

from __future__ import annotations

import asyncio
import json
from typing import Any

import anyio
import websockets
from anyio.streams.memory import MemoryObjectReceiveStream, MemoryObjectSendStream
import mcp.server.stdio as mcp_stdio
from mcp import types
from mcp.server import NotificationOptions, Server
from mcp.server.models import InitializationOptions

from tools import ALL_DISPATCH, ALL_TOOLS
from utils.logger import logger

# --------------------------------------------------------------------
# MCP Server instance
# --------------------------------------------------------------------

server = Server("msinsight-profiler")


# --------------------------------------------------------------------
# Tool list handler
# --------------------------------------------------------------------


@server.list_tools()
async def list_tools() -> list[types.Tool]:
    return ALL_TOOLS


# --------------------------------------------------------------------
# Tool call dispatcher
# --------------------------------------------------------------------


@server.call_tool()
async def call_tool(
    name: str, arguments: dict[str, Any]
) -> list[types.TextContent | types.ImageContent | types.EmbeddedResource]:
    handler = ALL_DISPATCH.get(name)
    if handler is None:
        return [
            types.TextContent(
                type="text",
                text=f"ERROR: Unknown tool '{name}'. Available tools: {sorted(ALL_DISPATCH.keys())}",
            )
        ]
    logger.info("Tool call: {} args={}", name, arguments)
    try:
        results = await handler(**arguments)

        # 打印实际的 tool 输出，辅助排查 outputSchema 校验错误（如返回值未能完全匹配定义格式）
        for idx, res in enumerate(results):
            if isinstance(res, types.TextContent):
                # 如果返回文本较长，可以通过截断或完整打印来排查
                logger.info("Tool {} response part {} text: {}", name, idx, res.text)

        return results
    except Exception as exc:
        logger.exception("Error in tool handler for {}: {}", name, exc)
        raise


# --------------------------------------------------------------------
# Shared InitializationOptions
# --------------------------------------------------------------------


def _init_options() -> InitializationOptions:
    return InitializationOptions(
        server_name="msinsight-profiler",
        server_version="1.0.0",
        capabilities=server.get_capabilities(
            notification_options=NotificationOptions(),
            experimental_capabilities={},
        ),
    )


# --------------------------------------------------------------------
# Transport runners
# --------------------------------------------------------------------


async def run_stdio() -> None:
    """Run the MCP server over stdio (for Claude Desktop / local CLI usage)."""
    logger.info("Starting MCP server — transport: stdio")
    async with mcp_stdio.stdio_server() as (read_stream, write_stream):
        await server.run(read_stream, write_stream, _init_options())


async def run_sse(host: str, port: int) -> None:
    """Run the MCP server over HTTP + Server-Sent Events.

    Requires ``mcp[cli]`` which bundles Starlette + uvicorn.
    """
    try:
        from mcp.server.sse import SseServerTransport
        from starlette.applications import Starlette
        from starlette.requests import Request
        from starlette.responses import Response
        from starlette.routing import Mount, Route
        import uvicorn
    except ImportError as exc:
        raise RuntimeError(
            "SSE transport requires 'uvicorn' and 'starlette'. Install them with: pip install mcp[cli]"
        ) from exc

    sse_transport = SseServerTransport("/messages/")

    async def handle_sse(request: Request) -> Response:
        async with sse_transport.connect_sse(request.scope, request.receive, request._send) as (
            read_stream,
            write_stream,
        ):
            await server.run(read_stream, write_stream, _init_options())
        return Response()

    app = Starlette(
        routes=[
            Route("/sse", endpoint=handle_sse),
            Mount("/messages/", app=sse_transport.handle_post_message),
        ]
    )

    logger.info("Starting MCP server — transport: SSE  http://{}:{}/sse", host, port)
    config = uvicorn.Config(app, host=host, port=port, log_level="warning")
    await uvicorn.Server(config).serve()


async def run_websocket(host: str, port: int) -> None:
    """Run the MCP server over raw WebSocket.

    Each client connection gets an isolated MCP session.
    The MCP JSON-RPC messages are plain JSON text frames.
    """
    logger.info("Starting MCP server — transport: WebSocket  ws://{}:{}", host, port)

    async def handle_client(ws: websockets.WebSocketServerProtocol) -> None:
        remote = ws.remote_address
        logger.info("MCP WebSocket client connected: {}", remote)
        try:
            await _bridge_ws_session(ws)
        except Exception as exc:
            logger.exception("Session error for {}: {}", remote, exc)
        finally:
            logger.info("MCP WebSocket client disconnected: {}", remote)

    async with websockets.serve(handle_client, host, port):
        await asyncio.Future()  # run forever


async def _bridge_ws_session(ws: websockets.WebSocketServerProtocol) -> None:
    """Bridge a single WebSocket connection to an MCP Server session."""

    # anyio in-memory streams connecting the WebSocket ↔ mcp.Server
    ws_to_mcp_send: MemoryObjectSendStream[types.JSONRPCMessage | Exception]
    ws_to_mcp_recv: MemoryObjectReceiveStream[types.JSONRPCMessage | Exception]
    mcp_to_ws_send: MemoryObjectSendStream[types.JSONRPCMessage]
    mcp_to_ws_recv: MemoryObjectReceiveStream[types.JSONRPCMessage]

    ws_to_mcp_send, ws_to_mcp_recv = anyio.create_memory_object_stream(256)
    mcp_to_ws_send, mcp_to_ws_recv = anyio.create_memory_object_stream(256)

    async def ws_reader() -> None:
        """WebSocket → MCP: forward incoming frames as JSONRPCMessage objects."""
        try:
            async for raw in ws:
                try:
                    parsed = json.loads(raw)
                    msg = types.JSONRPCMessage.model_validate(parsed)
                    await ws_to_mcp_send.send(msg)
                except Exception as exc:
                    logger.warning("Malformed MCP message from client: {}", exc)
                    await ws_to_mcp_send.send(exc)
        finally:
            await ws_to_mcp_send.aclose()

    async def ws_writer() -> None:
        """MCP → WebSocket: forward outgoing MCP messages as JSON text frames."""
        try:
            async for msg in mcp_to_ws_recv:
                await ws.send(msg.model_dump_json(exclude_none=True))
        except websockets.ConnectionClosed:
            pass
        finally:
            await mcp_to_ws_recv.aclose()

    async def mcp_runner() -> None:
        """Run the MCP server session on the anyio stream pair."""
        try:
            await server.run(ws_to_mcp_recv, mcp_to_ws_send, _init_options())
        finally:
            await ws_to_mcp_send.aclose()
            await mcp_to_ws_send.aclose()

    async with anyio.create_task_group() as tg:
        tg.start_soon(ws_reader)
        tg.start_soon(ws_writer)
        tg.start_soon(mcp_runner)
