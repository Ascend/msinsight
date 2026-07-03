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

import os
import sys
import json
import subprocess  # nosec B404 - subprocess is required to manage bundled services.
import socket
import logging
import re
import shutil
import uuid
import shlex
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
# init ACP node service process
acp_process = {}
# default profiler_server port
available_port = 9000
# default ACP node service port
acp_available_port = 9001
# default profiler_server_id
profiler_server_id = str(uuid.uuid4())
# login shell env cache
login_shell_env = None

DEFAULT_AGENT_SERVERS_CONFIG = """{
  "activeAgent": "OpenCode",
  "agentServers": [
    {
      "name": "OpenCode",
      "command": "opencode",
      "args": [
        "acp"
      ]
    }
  ]
}
"""


def check_jupyter_server_proxy_installed():
    try:
        # 动态查找 jupyter 的绝对路径
        env = get_process_env()
        jupyter_path = shutil.which('jupyter', path=env.get('PATH'))
        if not jupyter_path:
            raise FileNotFoundError("jupyter executable not found in PATH")

        # 执行命令
        result = subprocess.run(  # nosec B603 - command uses resolved jupyter path and shell=False.
            [jupyter_path, 'labextension', 'list'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
            env=env,
        )
        # 获取标准输出和标准错误输出
        output = result.stdout + result.stderr
        # 检查扩展是否在输出中
        return 'jupyter-server-proxy' in output
    except subprocess.CalledProcessError as e:
        logging.error("Failed to check jupyter-server-proxy, because %s", e)
    except FileNotFoundError as e:
        logging.error("Failed to check jupyter-server-proxy, because %s", e)
    return False


def get_host_ip():
    user_dir = os.path.expanduser('~')
    config_path = os.path.join(user_dir, '.jupyter', 'jupyter_lab_config.py')
    host_ip = '127.0.0.1'  # 默认值

    if not os.path.exists(config_path):
        return host_ip

    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 匹配 c.ServerApp.ip 的值
        ip_pattern = r'^[ \t]*(?!\s*#)c\.ServerApp\.ip\s*=\s*[\'"](.*?)[\'"]'
        match = re.search(ip_pattern, content, flags=re.MULTILINE)

        if match:
            host_ip = match.group(1)
    except Exception as e:
        logging.error("Failed to check jupyter-lab-config, because %s", e)

    return host_ip


def get_local_ip():
    if check_jupyter_server_proxy_installed():
        return '127.0.0.1'
    else:
        return get_host_ip()


def find_available_port(host, start_port=9000, max_tries=100):
    current_port = start_port
    tries = 0
    while tries < max_tries:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind((host, current_port))
                return current_port
            except socket.error:
                current_port += 1
                tries += 1
                continue
    return None


def start_profiler_server():
    global available_port
    global profiler_server_id

    mindstudio_insight_dir = ensure_mindstudio_insight_dir()

    profiler_server_path = os.path.join(os.path.dirname(__file__), 'resources', 'profiler', 'server', 'profiler_server')

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
    profiler_server_id = str(uuid.uuid4())

    if sys.platform == 'win32':
        profiler_server_path = profiler_server_path + '.exe'
        # 设置执行权限
        os.chmod(profiler_server_path, 0o550)  # nosec B103 - restrict bundled executable permissions.
        command[0] = profiler_server_path
        # start profiler server and set port
        process = subprocess.Popen(command, env=get_process_env())  # nosec B603 - shell=False with fixed args.
        profiler_process[profiler_server_id] = process
    else:
        # 设置执行权限
        os.chmod(profiler_server_path, 0o550)  # nosec B103 - restrict bundled executable permissions.
        server_dir = os.path.join(os.path.dirname(__file__), 'resources', 'profiler', 'server')
        env = get_process_env()
        env["LD_LIBRARY_PATH"] = f".:{env.get('LD_LIBRARY_PATH', '')}"
        command[0] = './profiler_server'
        process = subprocess.Popen(  # pylint: disable=consider-using-with
            command, cwd=server_dir, env=env
        )  # nosec B603 - shell=False with fixed args.
        profiler_process[profiler_server_id] = process


def ensure_mindstudio_insight_dir():
    user_home_dir = os.path.expanduser('~')
    mindstudio_insight_dir = os.path.join(user_home_dir, '.mindstudio_insight')

    if not os.path.exists(mindstudio_insight_dir):
        os.makedirs(mindstudio_insight_dir, 0o750)

    agent_servers_config_path = os.path.join(mindstudio_insight_dir, 'agent-servers.json')
    if not os.path.exists(agent_servers_config_path):
        with open(agent_servers_config_path, 'w', encoding='utf-8') as file:
            file.write(DEFAULT_AGENT_SERVERS_CONFIG)

    return mindstudio_insight_dir


def start_acp_node_service():
    mindstudio_insight_dir = ensure_mindstudio_insight_dir()
    acp_service_dir = os.path.join(os.path.dirname(__file__), 'resources', 'profiler', 'server', 'insight_web_agent')
    acp_entry_path = os.path.join(acp_service_dir, 'index.mjs')
    if not os.path.exists(acp_entry_path):
        logging.error('ACP node service entry does not exist: %s', acp_entry_path)
        return None

    global acp_available_port
    acp_host = get_local_ip()
    acp_available_port = find_available_port(acp_host, available_port + 1)
    command = [
        'node',
        acp_entry_path,
        '--path',
        mindstudio_insight_dir,
        '--resource-path',
        acp_service_dir,
        '--port',
        str(acp_available_port),
        '--host',
        acp_host,
    ]
    env = get_process_env()

    process = subprocess.Popen(  # pylint: disable=consider-using-with
        command, cwd=acp_service_dir, env=env
    )  # nosec B603 - shell=False with fixed args.
    acp_process[profiler_server_id] = process
    logging.info(
        'ACP node service started, host=%s, port=%s, path=%s', acp_host, acp_available_port, mindstudio_insight_dir
    )
    return acp_available_port


def get_process_env():
    if sys.platform != 'darwin':
        return os.environ.copy()

    global login_shell_env
    if login_shell_env is None:
        login_shell_env = load_login_shell_env()
    return login_shell_env.copy()


def load_login_shell_env():
    env = os.environ.copy()
    shell = get_login_shell(env)
    home = os.path.expanduser('~')
    user = env.get('USER', '')
    logname = env.get('LOGNAME', user)
    command = f"cd {shlex.quote(home)}; env -0"
    capture_env = {
        'HOME': home,
        'USER': user,
        'LOGNAME': logname,
        'PATH': '/usr/bin:/bin:/usr/sbin:/sbin',
        'SHELL': shell,
    }

    try:
        result = subprocess.run(  # nosec B603 - shell=False, shell path comes from current user environment.
            [shell, '-l', '-i', '-c', command],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            env=capture_env,
            timeout=10,
        )
        captured_env = parse_env_output(result.stdout)
        if captured_env:
            captured_env.pop('SHLVL', None)
            logging.info('Loaded login shell environment from %s, PATH=%s', shell, captured_env.get('PATH', ''))
            return captured_env
        logging.warning(
            'Login shell environment output is empty, stderr=%s', result.stderr.decode('utf-8', errors='ignore')
        )
    except Exception as error:
        logging.error('Failed to load login shell environment, because %s', error)

    env['PATH'] = merge_path(
        [
            env.get('PATH', ''),
            '/opt/homebrew/bin',
            '/usr/local/bin',
            '/usr/bin',
            '/bin',
            '/usr/sbin',
            '/sbin',
        ]
    )
    return env


def get_login_shell(env):
    shell = env.get('SHELL')
    if shell and os.path.exists(shell):
        return shell
    return '/bin/zsh'


def parse_env_output(output):
    result = {}
    for entry in output.split(b'\0'):
        if not entry or b'=' not in entry:
            continue
        name, value = entry.split(b'=', 1)
        try:
            result[name.decode('utf-8')] = value.decode('utf-8')
        except UnicodeDecodeError:
            continue
    return result


def merge_path(paths):
    result = []
    for path in paths:
        for item in str(path).split(os.pathsep):
            if item and item not in result:
                result.append(item)
    return os.pathsep.join(result)


def is_port_in_use(port):
    for conn in psutil.net_connections():
        if conn.status == 'LISTEN' and conn.laddr.port == port:
            return True
    return False


def stop_profiler_server():
    if not profiler_process:
        return
    for server_id, process in list(profiler_process.items()):
        if process:
            try:
                process.terminate()
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()  # 强制终止进程
            finally:
                profiler_process.pop(server_id, None)
    profiler_process.clear()


def stop_acp_node_service(server_id=None):
    target_processes = acp_process if server_id is None else {server_id: acp_process.get(server_id)}
    for current_server_id, process in list(target_processes.items()):
        if process:
            try:
                process.terminate()
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
            finally:
                acp_process.pop(current_server_id, None)


def shutdown_hook(web_app):
    stop_profiler_server()
    stop_acp_node_service()


class IFrameConfigHandler(APIHandler):
    @tornado.web.authenticated
    def get(self):
        start_profiler_server()
        start_acp_node_service()
        # find available port
        # find start profiler server id
        self.finish(
            json.dumps(
                {
                    "proxy": check_jupyter_server_proxy_installed(),
                    "port": available_port,
                    "acpPort": acp_available_port,
                    "profilerServerId": profiler_server_id,
                }
            )
        )


class TerminateProfilerHandler(APIHandler):
    @tornado.web.authenticated
    def get(self):
        query_profiler_server_id = self.get_query_argument("profilerServerId")
        process = profiler_process.get(query_profiler_server_id)
        if process:
            try:
                process.terminate()
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
            finally:
                del profiler_process[query_profiler_server_id]
                stop_acp_node_service(query_profiler_server_id)

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
