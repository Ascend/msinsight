"""
MSInsight MCP Bridge — main entry point.

Startup sequence
----------------
1. Configure logging.
2. Connect the ``CppBackendClient`` singleton to the C++ backend.
3. Optionally register event listeners (parse/success, parse/fail, …).
4. Launch the MCP server on the configured transport.

Usage
-----
    # stdio (for Claude Desktop or any MCP-over-stdio client)
    python -m msinsight_mcp.main

    # SSE (for remote LangChain / web UIs)
    MSINSIGHT_MCP_TRANSPORT=sse MSINSIGHT_MCP_PORT=8765 python -m msinsight_mcp.main

    # WebSocket
    MSINSIGHT_MCP_TRANSPORT=websocket MSINSIGHT_MCP_PORT=8765 python -m msinsight_mcp.main

Configuration
-------------
All settings are in config.py and can be overridden via environment
variables prefixed with ``MSINSIGHT_`` or via a .env file.
"""

import asyncio
import signal
import sys

from . import cpp_client as cpp
from . import mcp_server
from .config import settings
from .internal.profiler_server import start_profiler_server_if_needed
from .state import state
from .utils.logger import logger, setup_logger


# --------------------------------------------------------------------
# Event listeners (optional — extend as needed)
# --------------------------------------------------------------------


def _on_parse_cluster_success(event: dict) -> None:
    logger.info(
        "Parse cluster completed successfully for module='{}' body={}",
        event.get("moduleName"),
        event,
    )
    state.mark_event_completed("parse/clusterCompleted", event)


def _on_parse_cluster_step2_success(event: dict) -> None:
    logger.info(
        "Parse cluster step 2 completed successfully for module='{}' body={}",
        event.get("moduleName"),
        event.get("body"),
    )
    state.mark_event_completed("parse/clusterStep2Completed", event)


def _on_parse_success(event: dict) -> None:
    logger.info(
        "Parse completed successfully for module='{}'",
        event.get("moduleName"),
    )


def _on_parse_fail(event: dict) -> None:
    logger.warning(
        "Parse FAILED for module='{}' body={}",
        event.get("moduleName"),
        event.get("body"),
    )


def _on_any_event(event: dict) -> None:
    logger.debug("Backend event: {}", event.get("event"))


# --------------------------------------------------------------------
# Graceful shutdown
# --------------------------------------------------------------------

_shutdown_event = asyncio.Event()


def _handle_signal(sig: int) -> None:
    logger.info("Received signal {}, initiating graceful shutdown …", sig)
    _shutdown_event.set()


# --------------------------------------------------------------------
# Main coroutine
# --------------------------------------------------------------------


async def _main() -> None:
    setup_logger()

    logger.info(
        "MSInsight MCP Bridge starting — backend={} transport={}",
        settings.cpp_backend_url,
        settings.mcp_transport,
    )

    start_profiler_server_if_needed()

    # --- Connect to C++ backend ---
    try:
        client = await cpp.initialise(
            url=settings.cpp_backend_url,
            request_timeout=settings.cpp_request_timeout,
            reconnect_interval=settings.cpp_reconnect_interval,
        )
    except Exception as exc:
        logger.error("Failed to connect to C++ backend: {}", exc)
        logger.warning(
            "Proceeding without a live backend connection. "
            "Tools will return errors until the backend becomes available."
        )
        client = cpp.get_client()
        client.start_background_tasks()

    # Register event listeners
    client.on_event("parse/clusterCompleted", _on_parse_cluster_success)
    client.on_event("parse/clusterStep2Completed", _on_parse_cluster_step2_success)
    client.on_event("parse/success", _on_parse_success)
    client.on_event("parse/fail", _on_parse_fail)
    client.on_event("*", _on_any_event)

    # Register OS signal handlers (not available on Windows for SIGTERM in all contexts)
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT,):
        try:
            loop.add_signal_handler(sig, _handle_signal, sig)
        except (NotImplementedError, RuntimeError):
            pass  # Windows does not support loop.add_signal_handler for all signals

    # --- Start MCP server ---
    transport = settings.mcp_transport

    try:
        if transport == "stdio":
            await mcp_server.run_stdio()

        elif transport == "sse":
            await _run_until_shutdown(
                mcp_server.run_sse(settings.mcp_host, settings.mcp_port)
            )

        elif transport == "websocket":
            await _run_until_shutdown(
                mcp_server.run_websocket(settings.mcp_host, settings.mcp_port)
            )

        else:
            logger.error("Unknown transport '{}'. Use stdio | sse | websocket.", transport)
            sys.exit(1)

    finally:
        logger.info("Shutting down C++ backend connection …")
        await cpp.shutdown()
        logger.info("MSInsight MCP Bridge stopped.")


async def _run_until_shutdown(server, shutdown_event: asyncio.Event = _shutdown_event) -> None:
    server_task = asyncio.create_task(server)
    shutdown_task = asyncio.create_task(shutdown_event.wait())
    try:
        done, _ = await asyncio.wait(
            [server_task, shutdown_task], return_when=asyncio.FIRST_COMPLETED
        )
        if server_task in done:
            await server_task
    finally:
        for task in (server_task, shutdown_task):
            if not task.done():
                task.cancel()
        await asyncio.gather(server_task, shutdown_task, return_exceptions=True)


# --------------------------------------------------------------------
# Entry point
# --------------------------------------------------------------------

if __name__ == "__main__":
    try:
        asyncio.run(_main())
    except KeyboardInterrupt:
        pass
