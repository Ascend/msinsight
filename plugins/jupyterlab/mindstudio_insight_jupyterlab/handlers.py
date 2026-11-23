#!/usr/bin/env python
# -*- coding: UTF-8 -*-

"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2025 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

         http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------
"""

# pylint: disable=logging-fstring-interpolation,unspecified-encoding,inconsistent-return-statements,global-variable-not-assigned,consider-using-with,useless-return
import asyncio
import os
import sys
import json
import subprocess  # nosec B404
import socket
import logging
import uuid
import time
from contextlib import contextmanager
from jupyter_server.base.handlers import APIHandler
from jupyter_server.utils import url_path_join
import tornado
from tornado.web import StaticFileHandler
from tornado.httputil import HTTPServerRequest


# 配置日志
logging.basicConfig(
    level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s', handlers=[logging.StreamHandler()]
)
startup_logger = logging.getLogger(__name__)

# init profiler_server process
profiler_process = {}
# Keep ports reserved while children initialize or are being terminated.
profiler_ports = {}
profiler_cleanup_tasks = {}
# default profiler_server port
available_port = 9000
# default profiler_server_id
profiler_server_id = str(uuid.uuid4())

HOST_PATTERN = (
    r"^(?=.{1,261}$)(?:"
    r"(?:[A-Za-z0-9-]{1,63}\.)*[A-Za-z0-9-]{1,63}(?::\d{1,5})?"
    r"|\d{1,3}(?:\.\d{1,3}){3}(?::\d{1,5})?"
    r"|[0-9A-Fa-f:]{2,45}"
    r"|\[[0-9A-Fa-f:]{2,45}\](?::\d{1,5})?"
    r")$"
)


@contextmanager
def log_startup_result(startup_id):
    """Keep one operational result per launch; normal cancellations stay silent."""
    record = {"stage": "config_request_total", "startup_id": startup_id, "status": "ok"}
    started_at = time.perf_counter()
    try:
        yield record
    except asyncio.CancelledError:
        record["status"] = "cancelled"
        raise
    except Exception as error:
        record.update(status="error", error_type=type(error).__name__, error=str(error))
        raise
    finally:
        if record["status"] != "cancelled":
            record["duration_ms"] = round((time.perf_counter() - started_at) * 1000, 3)
            record["event"] = "end"
            level = logging.INFO if record["status"] == "ok" else logging.ERROR
            if startup_logger.isEnabledFor(level):
                startup_logger.log(level, "[mindstudio-startup] %s", json.dumps(record))


def check_jupyter_server_proxy_installed(web_app, request, startup_id=None):
    # Inspect registered routes lazily, after service extensions have loaded.
    # Cache per Jupyter application and Host; an installed package alone is not enough.
    cache = web_app.settings.setdefault("mindstudio_proxy_cache", {})
    if request.host not in cache:
        path = url_path_join(web_app.settings["base_url"], "proxy/9000/")
        probe = HTTPServerRequest(method="GET", uri=path, host=request.host)
        probe.protocol = request.protocol
        handler_class = web_app.find_handler(probe).handler_class
        proxy_enabled = any(
            cls.__module__ == "jupyter_server_proxy.handlers" and cls.__name__ == "LocalProxyHandler"
            for cls in handler_class.__mro__
        )
        cache[request.host] = proxy_enabled
    return cache[request.host]


def get_host_ip(server_app):
    # Read effective configuration, including command-line overrides.
    host_ip = server_app.ip
    if host_ip in ("", "*"):
        return '0.0.0.0'  # nosec B104
    return host_ip


def get_local_ip(proxy_enabled, server_app):
    if proxy_enabled:
        return '127.0.0.1'
    else:
        return get_host_ip(server_app)


def _reap_exited_profiler_servers():
    # Run on the request thread, without await. Do not race executor-based cleanup.
    for server_id, process in list(profiler_process.items()):
        if server_id in profiler_cleanup_tasks:
            continue
        try:
            returncode = process.poll()
            if returncode is None:
                continue
            process.wait(timeout=0)
        except (OSError, subprocess.TimeoutExpired) as error:
            logging.warning("Failed to reap profiler server %s: %s", server_id, error)
            continue
        profiler_process.pop(server_id, None)
        port = profiler_ports.pop(server_id, None)
        if returncode != 0:
            logging.warning("Reclaimed exited profiler server %s (port=%s, returncode=%s)", server_id, port, returncode)


def find_available_port(host, start_port=9000, max_tries=100):
    # Keep the original 100 candidates: 9000 through 9099 by default.
    global available_port
    _reap_exited_profiler_servers()
    available_port = start_port
    tries = 0
    while tries < max_tries:
        if available_port in profiler_ports.values():
            available_port += 1
            tries += 1
            continue
        with socket.socket(socket.AF_INET6 if ":" in host else socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind((host, available_port))
                return available_port
            except socket.error:
                available_port += 1
                tries += 1
                continue


def spawn_profiler_process(command, startup_id=None, **kwargs):
    # Popen returning only means the process was created, not that it is ready.
    process = subprocess.Popen(command, **kwargs)  # nosec B603
    # Allocation, Popen and registration contain no await on the request thread.
    profiler_ports[profiler_server_id] = available_port
    return process


def start_profiler_server(host, startup_id=None):
    user_home_dir = os.path.expanduser('~')
    mindstudio_insight_dir = os.path.join(user_home_dir, '.mindstudio_insight')

    if not os.path.exists(mindstudio_insight_dir):
        os.makedirs(mindstudio_insight_dir, 0o750)

    global profiler_process
    profiler_server_path = os.path.join(os.path.dirname(__file__), 'resources', 'profiler', 'server', 'profiler_server')

    global available_port
    available_port = find_available_port(host)
    if available_port is None:
        logging.error("No available profiler port in 9000-9099 (startup_id=%s)", startup_id)
        return False
    # 配置参数
    command = [
        profiler_server_path,
        '--wsPort',
        str(available_port),
        '--wsHost',
        host,
        '--logPath',
        mindstudio_insight_dir,
    ]

    # 生成唯一profiler标识符
    global profiler_server_id
    profiler_server_id = str(uuid.uuid4())

    if sys.platform == 'win32':
        profiler_server_path = profiler_server_path + '.exe'
        # 设置执行权限
        os.chmod(profiler_server_path, 0o550)  # nosec B103
        command[0] = profiler_server_path
        # start profiler server and set port
        process = spawn_profiler_process(command, startup_id)
        profiler_process[profiler_server_id] = process
    else:
        # 已可执行则跳过；外部限制导致 chmod 失败时不中断启动
        try:
            if not os.access(profiler_server_path, os.X_OK):
                os.chmod(profiler_server_path, 0o550)  # nosec B103
        except OSError as e:
            logging.warning(f"Failed to set execute permission on profiler_server, because {e}")
        server_dir = os.path.join(os.path.dirname(__file__), 'resources', 'profiler', 'server')
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = f".:{env.get('LD_LIBRARY_PATH', '')}"
        command[0] = './profiler_server'
        try:
            process = spawn_profiler_process(command, startup_id, cwd=server_dir, env=env)
            profiler_process[profiler_server_id] = process
            return True
        except OSError as e:
            logging.error(f"Failed to start profiler server, because {e}")
            return False


async def wait_profiler_server(
    process, host, port, server_id, startup_id=None, timeout=30.0, connect_timeout=0.3, interval=0.1
):
    """Wait for TCP listening, not a WebSocket session or business-level readiness."""
    # This maps wildcard listen addresses to loopback; the probe never binds to them.
    probe_host = {"0.0.0.0": "127.0.0.1", "localhost": "127.0.0.1", "::": "::1"}.get(host, host)  # nosec B104
    family = socket.AF_INET6 if ":" in probe_host else socket.AF_INET
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout
    while True:
        returncode = process.poll()
        if returncode is not None:
            raise RuntimeError(f"Profiler server exited before listening (returncode={returncode})")
        if server_id in profiler_cleanup_tasks:
            raise RuntimeError("Profiler server is being terminated")
        remaining = deadline - loop.time()
        if remaining <= 0:
            raise asyncio.TimeoutError("Timed out waiting for profiler server to listen")
        with socket.socket(family, socket.SOCK_STREAM) as probe:
            probe.setblocking(False)
            try:
                await asyncio.wait_for(
                    loop.sock_connect(probe, (probe_host, port)), timeout=min(connect_timeout, remaining)
                )
            except (OSError, asyncio.TimeoutError):
                pass
            else:
                if process.poll() is None and server_id not in profiler_cleanup_tasks:
                    return
        await asyncio.sleep(min(interval, max(0, deadline - loop.time())))


def _terminate_process(process):
    if process.poll() is None:
        try:
            process.terminate()
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        try:
            process.kill()
        except ProcessLookupError:
            pass
        process.wait(timeout=3)


async def terminate_profiler_server(server_id, startup_id=None):
    """Share cleanup across a timeout, disconnect and explicit termination."""
    process = profiler_process.get(server_id)
    if process is None:
        return
    if server_id not in profiler_cleanup_tasks:

        async def cleanup():
            await asyncio.get_running_loop().run_in_executor(None, _terminate_process, process)
            # Only release the port after successful exit/reaping.
            profiler_process.pop(server_id, None)
            profiler_ports.pop(server_id, None)

        def completed(task):
            profiler_cleanup_tasks.pop(server_id, None)
            if not task.cancelled() and task.exception() is not None:
                logging.error(
                    "Failed to clean up profiler %s (startup_id=%s): %s", server_id, startup_id, task.exception()
                )

        task = asyncio.ensure_future(cleanup())
        profiler_cleanup_tasks[server_id] = task
        task.add_done_callback(completed)
    # Cancelling a requester must not abandon a child being terminated.
    await asyncio.shield(profiler_cleanup_tasks[server_id])


def stop_profiler_server():
    for server_id, process in list(profiler_process.items()):
        try:
            _terminate_process(process)
        except Exception:
            logging.exception("Failed to stop profiler %s", server_id)
        else:
            profiler_process.pop(server_id, None)
            profiler_ports.pop(server_id, None)


def shutdown_hook(web_app):
    stop_profiler_server()


class IFrameConfigHandler(APIHandler):
    def __init__(self, *args, **kwargs):
        self._readiness_task = None
        self._client_disconnected = False
        super().__init__(*args, **kwargs)

    @tornado.web.authenticated
    async def get(self):
        # Generate this before any work so failures and slow CLI calls can be correlated.
        startup_id = str(uuid.uuid4())
        with log_startup_result(startup_id) as timing:
            if getattr(self, "_client_disconnected", False):
                timing["status"] = "cancelled"
                return
            proxy_enabled = check_jupyter_server_proxy_installed(self.application, self.request, startup_id)
            host = get_local_ip(proxy_enabled, self.settings["mindstudio_server_app"])
            if start_profiler_server(host, startup_id) is False:
                timing.update(status="error", http_status=500)
                self.set_status(500)
                self.finish(json.dumps({"error": "Failed to start profiler server, please check it."}))
                return
            # Snapshot before the first await: another startup can overwrite globals.
            port, server_id = available_port, profiler_server_id
            process = profiler_process[server_id]
            timing.update(port=port, profiler_server_id=server_id, pid=process.pid, proxy_enabled=proxy_enabled)
            ready_started_at = time.perf_counter()
            self._readiness_task = asyncio.ensure_future(
                wait_profiler_server(process, host, port, server_id, startup_id)
            )
            try:
                try:
                    await self._readiness_task
                finally:
                    timing["tcp_wait_ms"] = round((time.perf_counter() - ready_started_at) * 1000, 3)
                if getattr(self, "_client_disconnected", False):
                    raise asyncio.CancelledError()
                if (
                    process.poll() is not None
                    or server_id in profiler_cleanup_tasks
                    or profiler_process.get(server_id) is not process
                ):
                    raise RuntimeError("Profiler server stopped during startup")
            except (Exception, asyncio.CancelledError) as error:
                cancelled = isinstance(error, asyncio.CancelledError)
                timing.update(status="cancelled" if cancelled else "error", error_type=type(error).__name__)
                try:
                    await terminate_profiler_server(server_id, startup_id)
                except Exception:
                    logging.exception("Failed to reclaim profiler %s after startup failure", server_id)
                if cancelled or getattr(self, "_client_disconnected", False):
                    return
                status = 504 if isinstance(error, asyncio.TimeoutError) else 500
                timing.update(http_status=status, error=str(error))
                self.set_status(status)
                self.finish(json.dumps({"error": str(error)}))
                return
            self.finish(
                json.dumps(
                    {
                        "proxy": proxy_enabled,
                        "port": port,
                        "profilerServerId": server_id,
                    }
                )
            )
            # Includes TCP readiness, but not browser/proxy connectivity or business handshakes.
            timing["http_status"] = 200

    def on_connection_close(self):
        self._client_disconnected = True
        task = getattr(self, "_readiness_task", None)
        if task is not None and not task.done():
            task.cancel()
        super().on_connection_close()


class TerminateProfilerHandler(APIHandler):
    @tornado.web.authenticated
    async def get(self):
        query_profiler_server_id = self.get_query_argument("profilerServerId")
        global profiler_process
        process = profiler_process.get(query_profiler_server_id)
        if process:
            await terminate_profiler_server(query_profiler_server_id)

            self.finish(
                json.dumps(
                    {
                        "status": "terminated",
                        "profilerServerId": query_profiler_server_id,
                    }
                )
            )
        else:
            self.set_status(404)
            self.finish(
                json.dumps(
                    {
                        "error": "Profiler server not found",
                    }
                )
            )


class RouteHandler(APIHandler):
    # The following decorator should be present on all verb methods (head, get, post,
    # patch, put, delete, options) to ensure only authorized user can request the
    # Jupyter server
    @tornado.web.authenticated
    def get(self):
        self.finish(json.dumps({"data": "This is mindstudio_insight_jupyterlab get_example endpoint!"}))


class IFrameStaticFileHandler(StaticFileHandler):
    """Serve frontend assets without checking profiler processes or ports."""


def setup_handlers(web_app, server_app):
    web_app.settings["mindstudio_server_app"] = server_app
    web_app.settings["shutdown_hook"] = shutdown_hook
    host_pattern = HOST_PATTERN
    base_url = web_app.settings["base_url"]

    iframe_route_pattern = url_path_join(base_url, "/mindstudio_insight_jupyterlab/get_iframe_config")
    iframe_handlers = [(iframe_route_pattern, IFrameConfigHandler)]

    terminate_route_pattern = url_path_join(base_url, "/mindstudio_insight_jupyterlab/terminate_profiler_server")
    terminate_handlers = [(terminate_route_pattern, TerminateProfilerHandler)]

    static_frontend_path = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), 'resources', 'profiler', 'frontend'
    )
    static_route_pattern = url_path_join(base_url, "/resources/profiler/frontend/(.*)")
    static_handlers = [(static_route_pattern, IFrameStaticFileHandler, {'path': static_frontend_path})]

    api_route_pattern = url_path_join(base_url, "/mindstudio_insight_jupyterlab/get_example")
    api_handlers = [(api_route_pattern, RouteHandler)]

    web_app.add_handlers(host_pattern, iframe_handlers + terminate_handlers + static_handlers + api_handlers)
