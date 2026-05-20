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
import os
import sys
import json
import subprocess  # nosec B404
import socket
import logging
import re
import shutil
import uuid
import psutil
from jupyter_server.base.handlers import APIHandler
from jupyter_server.utils import url_path_join
import tornado
from tornado.web import StaticFileHandler


# 配置日志
logging.basicConfig(
    level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s', handlers=[logging.StreamHandler()]
)
# init profiler_server process
profiler_process = {}
# default profiler_server port
available_port = 9000
# default profiler_server_id
profiler_server_id = str(uuid.uuid4())


def check_jupyter_server_proxy_installed():
    try:
        # 动态查找 jupyter 的绝对路径
        jupyter_path = shutil.which('jupyter')
        if not jupyter_path:
            raise FileNotFoundError("jupyter executable not found in PATH")

        # 执行命令
        result = subprocess.run(  # nosec B603
            [jupyter_path, 'labextension', 'list'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
        )
        # 获取标准输出和标准错误输出
        output = result.stdout + result.stderr
        # 检查扩展是否在输出中
        return 'jupyter-server-proxy' in output
    except subprocess.CalledProcessError as e:
        logging.error(f"Failed to check jupyter-server-proxy, because {e}")
    except FileNotFoundError as e:
        logging.error(f"Failed to check jupyter-server-proxy, because {e}")
    return False


def get_host_ip():
    user_dir = os.path.expanduser('~')
    config_path = os.path.join(user_dir, '.jupyter', 'jupyter_lab_config.py')
    host_ip = '127.0.0.1'  # 默认值

    if not os.path.exists(config_path):
        return host_ip

    try:
        with open(config_path, 'r') as f:
            content = f.read()

        # 匹配 c.ServerApp.ip 的值
        ip_pattern = r'^[ \t]*(?!\s*#)c\.ServerApp\.ip\s*=\s*[\'"](.*?)[\'"]'
        match = re.search(ip_pattern, content, flags=re.MULTILINE)

        if match:
            configured_ip = match.group(1).strip()
            if configured_ip == '*':
                host_ip = '0.0.0.0'  # nosec B104
            else:
                try:
                    socket.getaddrinfo(configured_ip, None, socket.AF_INET)
                    host_ip = configured_ip
                except socket.gaierror:
                    logging.error(f"Invalid ServerApp.ip in jupyter config: {configured_ip}")
    except Exception as e:
        logging.error(f"Failed to check jupyter-lab-config, because {e}")

    return host_ip


def get_local_ip():
    if check_jupyter_server_proxy_installed():
        return '127.0.0.1'
    else:
        return get_host_ip()


def find_available_port(host, start_port=9000, max_tries=100):
    global available_port
    available_port = start_port
    tries = 0
    while tries < max_tries:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind((host, available_port))
                return available_port
            except socket.error:
                available_port += 1
                tries += 1
                continue


def start_profiler_server():
    user_home_dir = os.path.expanduser('~')
    mindstudio_insight_dir = os.path.join(user_home_dir, '.mindstudio_insight')

    if not os.path.exists(mindstudio_insight_dir):
        os.makedirs(mindstudio_insight_dir, 0o750)

    global profiler_process
    profiler_server_path = os.path.join(os.path.dirname(__file__), 'resources', 'profiler', 'server', 'profiler_server')

    global available_port
    available_port = find_available_port(get_local_ip())
    # 配置参数
    command = [
        profiler_server_path,
        '--wsPort',
        str(available_port),
        '--wsHost',
        get_local_ip(),
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
        process = subprocess.Popen(command)  # nosec B603
        profiler_process[profiler_server_id] = process
    else:
        # 设置执行权限
        os.chmod(profiler_server_path, 0o550)  # nosec B103
        server_dir = os.path.join(os.path.dirname(__file__), 'resources', 'profiler', 'server')
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = f".:{env.get('LD_LIBRARY_PATH', '')}"
        command[0] = './profiler_server'
        process = subprocess.Popen(command, cwd=server_dir, env=env)  # nosec B603
        profiler_process[profiler_server_id] = process


def is_port_in_use(port):
    for conn in psutil.net_connections():
        if conn.status == 'LISTEN' and conn.laddr.port == port:
            return True
    return False


def stop_profiler_server():
    global profiler_process
    if not profiler_process:
        return
    for server_id, process in profiler_process.items():
        if process:
            try:
                process.terminate()
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()  # 强制终止进程
            finally:
                del profiler_process[server_id]
    profiler_process = {}


def shutdown_hook(web_app):
    stop_profiler_server()


class IFrameConfigHandler(APIHandler):
    @tornado.web.authenticated
    def get(self):
        start_profiler_server()
        # find available port
        global available_port
        # find start profiler server id
        global profiler_server_id
        self.finish(
            json.dumps(
                {
                    "proxy": check_jupyter_server_proxy_installed(),
                    "port": available_port,
                    "profilerServerId": profiler_server_id,
                }
            )
        )


class TerminateProfilerHandler(APIHandler):
    @tornado.web.authenticated
    def get(self):
        query_profiler_server_id = self.get_query_argument("profilerServerId")
        global profiler_process
        process = profiler_process.get(query_profiler_server_id)
        if process:
            try:
                process.terminate()
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
            finally:
                del profiler_process[query_profiler_server_id]

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
    def prepare(self):
        if is_port_in_use(available_port):
            super().prepare()
        else:
            self.set_status(403)
            self.finish(json.dumps({"error": "Failed to start profiler server, please check it."}))
            return


def setup_handlers(web_app):
    web_app.settings["shutdown_hook"] = shutdown_hook
    host_pattern = "^[A-Za-z0-9.-]{1,255}$"
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
