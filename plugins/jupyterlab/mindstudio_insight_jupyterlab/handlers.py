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
import json
import logging
import os
import re
import shlex
import shutil
import socket
import subprocess  # nosec B404
import sys
import uuid

import psutil
import tornado
from jupyter_server.base.handlers import APIHandler
from jupyter_server.utils import url_path_join
from tornado.web import StaticFileHandler


logging.basicConfig(
    level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s', handlers=[logging.StreamHandler()]
)

profiler_process = {}
acp_process = {}
available_port = 9000
acp_available_port = 9001
profiler_server_id = str(uuid.uuid4())
login_shell_env = None

HOST_PATTERN = (
    r"^(?=.{1,261}$)(?:"
    r"(?:[A-Za-z0-9-]{1,63}\.)*[A-Za-z0-9-]{1,63}(?::\d{1,5})?"
    r"|\d{1,3}(?:\.\d{1,3}){3}(?::\d{1,5})?"
    r"|[0-9A-Fa-f:]{2,45}"
    r"|\[[0-9A-Fa-f:]{2,45}\](?::\d{1,5})?"
    r")$"
)

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
        env = get_process_env()
        jupyter_path = shutil.which('jupyter', path=env.get('PATH'))
        if not jupyter_path:
            raise FileNotFoundError("jupyter executable not found in PATH")

        result = subprocess.run(  # nosec B603
            [jupyter_path, 'labextension', 'list'],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
            env=env,
        )
        return 'jupyter-server-proxy' in result.stdout + result.stderr
    except (subprocess.CalledProcessError, FileNotFoundError) as error:
        logging.error("Failed to check jupyter-server-proxy, because %s", error)
        return False


def get_host_ip():
    config_path = os.path.join(os.path.expanduser('~'), '.jupyter', 'jupyter_lab_config.py')
    host_ip = '127.0.0.1'
    if not os.path.exists(config_path):
        return host_ip

    try:
        with open(config_path, 'r', encoding='utf-8') as file:
            content = file.read()
        match = re.search(
            r'^[ \t]*(?!\s*#)c\.ServerApp\.ip\s*=\s*[\'\"](.*?)[\'\"]', content, flags=re.MULTILINE
        )
        if match:
            configured_ip = match.group(1).strip()
            if configured_ip == '*':
                host_ip = '0.0.0.0'  # nosec B104
            else:
                try:
                    socket.getaddrinfo(configured_ip, None, socket.AF_INET)
                    host_ip = configured_ip
                except socket.gaierror:
                    logging.error('Invalid ServerApp.ip in jupyter config: %s', configured_ip)
    except (OSError, UnicodeError) as error:
        logging.error("Failed to check jupyter-lab-config, because %s", error)
    return host_ip


def get_local_ip():
    return '127.0.0.1' if check_jupyter_server_proxy_installed() else get_host_ip()


def find_available_port(host, start_port=9000, max_tries=100):
    current_port = start_port
    for _ in range(max_tries):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            try:
                sock.bind((host, current_port))
                return current_port
            except OSError:
                current_port += 1
    return None


def ensure_mindstudio_insight_dir():
    mindstudio_insight_dir = os.path.join(os.path.expanduser('~'), '.mindstudio_insight')
    os.makedirs(mindstudio_insight_dir, mode=0o750, exist_ok=True)

    config_path = os.path.join(mindstudio_insight_dir, 'agent-servers.json')
    if not os.path.exists(config_path):
        with open(config_path, 'w', encoding='utf-8') as file:
            file.write(DEFAULT_AGENT_SERVERS_CONFIG)
    return mindstudio_insight_dir


def start_profiler_server():
    global available_port
    global profiler_server_id

    try:
        mindstudio_insight_dir = ensure_mindstudio_insight_dir()
        host = get_local_ip()
        selected_port = find_available_port(host)
        if selected_port is None:
            logging.error('No available profiler port')
            return False

        server_dir = os.path.join(os.path.dirname(__file__), 'resources', 'profiler', 'server')
        executable = os.path.join(server_dir, 'profiler_server')
        if sys.platform == 'win32':
            executable += '.exe'
        if not os.path.isfile(executable):
            logging.error('Profiler server executable does not exist: %s', executable)
            return False

        os.chmod(executable, 0o550)  # nosec B103
        command = [
            executable if sys.platform == 'win32' else './profiler_server',
            '--wsPort',
            str(selected_port),
            '--wsHost',
            host,
            '--logPath',
            mindstudio_insight_dir,
        ]
        env = get_process_env()
        popen_options = {'env': env}
        if sys.platform != 'win32':
            env['LD_LIBRARY_PATH'] = f".:{env.get('LD_LIBRARY_PATH', '')}"
            popen_options['cwd'] = server_dir
        process = subprocess.Popen(command, **popen_options)  # nosec B603
    except (OSError, subprocess.SubprocessError) as error:
        logging.error('Failed to start profiler server, because %s', error)
        return False

    server_id = str(uuid.uuid4())
    profiler_process[server_id] = process
    profiler_server_id = server_id
    available_port = selected_port
    return True


def start_acp_node_service():
    global acp_available_port

    try:
        mindstudio_insight_dir = ensure_mindstudio_insight_dir()
        acp_service_dir = os.path.join(
            os.path.dirname(__file__), 'resources', 'profiler', 'server', 'insight_web_agent'
        )
        acp_entry_path = os.path.join(acp_service_dir, 'index.mjs')
        if not os.path.isfile(acp_entry_path):
            logging.error('ACP node service entry does not exist: %s', acp_entry_path)
            return None

        env = get_process_env()
        node_path = shutil.which('node', path=env.get('PATH'))
        if not node_path:
            logging.error('Node executable was not found in PATH')
            return None
        if profiler_server_id not in profiler_process:
            logging.error('Cannot start ACP without a running profiler server')
            return None

        acp_host = get_local_ip()
        selected_port = find_available_port(acp_host, available_port + 1)
        if selected_port is None:
            logging.error('No available ACP port')
            return None

        command = [
            node_path,
            acp_entry_path,
            '--path',
            mindstudio_insight_dir,
            '--resource-path',
            acp_service_dir,
            '--port',
            str(selected_port),
            '--host',
            acp_host,
        ]
        process = subprocess.Popen(command, cwd=acp_service_dir, env=env)  # nosec B603
    except (OSError, subprocess.SubprocessError) as error:
        logging.error('Failed to start ACP node service, because %s', error)
        return None

    acp_process[profiler_server_id] = process
    acp_available_port = selected_port
    return selected_port


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
    capture_env = {
        'HOME': home,
        'USER': user,
        'LOGNAME': env.get('LOGNAME', user),
        'PATH': '/usr/bin:/bin:/usr/sbin:/sbin',
        'SHELL': shell,
    }
    try:
        result = subprocess.run(  # nosec B603
            [shell, '-l', '-i', '-c', f"cd {shlex.quote(home)}; env -0"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            env=capture_env,
            timeout=10,
        )
        captured_env = parse_env_output(result.stdout)
        if captured_env:
            captured_env.pop('SHLVL', None)
            return captured_env
    except (OSError, subprocess.SubprocessError) as error:
        logging.error('Failed to load login shell environment, because %s', error)

    env['PATH'] = merge_path(
        [env.get('PATH', ''), '/opt/homebrew/bin', '/usr/local/bin', '/usr/bin', '/bin', '/usr/sbin', '/sbin']
    )
    return env


def get_login_shell(env):
    shell = env.get('SHELL')
    return shell if shell and os.path.exists(shell) else '/bin/zsh'


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
    for connection in psutil.net_connections():
        if connection.status == 'LISTEN' and connection.laddr.port == port:
            return True
    return False


def _terminate_process(process):
    if process is None:
        return
    try:
        process.terminate()
        process.wait(timeout=3)
    except subprocess.TimeoutExpired:
        process.kill()
    except OSError as error:
        logging.error('Failed to stop child process, because %s', error)


def stop_acp_node_service(server_id=None):
    server_ids = list(acp_process) if server_id is None else [server_id]
    for current_server_id in server_ids:
        process = acp_process.pop(current_server_id, None)
        _terminate_process(process)


def stop_profiler_server(server_id=None):
    server_ids = list(profiler_process) if server_id is None else [server_id]
    for current_server_id in server_ids:
        process = profiler_process.pop(current_server_id, None)
        _terminate_process(process)
        stop_acp_node_service(current_server_id)


def shutdown_hook(web_app):
    del web_app
    stop_profiler_server()
    stop_acp_node_service()


class IFrameConfigHandler(APIHandler):
    @tornado.web.authenticated
    def get(self):
        if not start_profiler_server():
            self.set_status(503)
            self.finish(json.dumps({'error': 'Failed to start profiler server'}))
            return

        started_server_id = profiler_server_id
        acp_port = start_acp_node_service()
        if acp_port is None:
            stop_profiler_server(started_server_id)
            self.set_status(503)
            self.finish(json.dumps({'error': 'Failed to start ACP node service'}))
            return

        self.finish(
            json.dumps(
                {
                    'proxy': check_jupyter_server_proxy_installed(),
                    'port': available_port,
                    'acpPort': acp_port,
                    'profilerServerId': started_server_id,
                }
            )
        )


class TerminateProfilerHandler(APIHandler):
    @tornado.web.authenticated
    def get(self):
        server_id = self.get_query_argument('profilerServerId')
        if server_id not in profiler_process:
            self.set_status(404)
            self.finish(json.dumps({'error': 'Profiler server not found'}))
            return

        stop_profiler_server(server_id)
        self.finish(json.dumps({'status': 'terminated', 'profilerServerId': server_id}))


class RouteHandler(APIHandler):
    @tornado.web.authenticated
    def get(self):
        self.finish(json.dumps({'data': 'This is mindstudio_insight_jupyterlab get_example endpoint!'}))


class IFrameStaticFileHandler(StaticFileHandler):
    def prepare(self):
        if is_port_in_use(available_port):
            super().prepare()
            return
        self.set_status(403)
        self.finish(json.dumps({'error': 'Failed to start profiler server, please check it.'}))


def setup_handlers(web_app):
    web_app.settings['shutdown_hook'] = shutdown_hook
    base_url = web_app.settings['base_url']
    iframe_route = url_path_join(base_url, '/mindstudio_insight_jupyterlab/get_iframe_config')
    terminate_route = url_path_join(base_url, '/mindstudio_insight_jupyterlab/terminate_profiler_server')
    static_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'resources', 'profiler', 'frontend')
    static_route = url_path_join(base_url, '/resources/profiler/frontend/(.*)')
    api_route = url_path_join(base_url, '/mindstudio_insight_jupyterlab/get_example')
    web_app.add_handlers(
        HOST_PATTERN,
        [
            (iframe_route, IFrameConfigHandler),
            (terminate_route, TerminateProfilerHandler),
            (static_route, IFrameStaticFileHandler, {'path': static_path}),
            (api_route, RouteHandler),
        ],
    )
