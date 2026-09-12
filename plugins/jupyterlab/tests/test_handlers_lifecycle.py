#!/usr/bin/env python
# -*- coding: UTF-8 -*-

"""Focused process lifecycle tests for the JupyterLab handlers."""

import ast
import json
import os
import socket
import sys
import types

import pytest

PACKAGE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
if PACKAGE_DIR not in sys.path:
    sys.path.insert(0, PACKAGE_DIR)

if 'psutil' not in sys.modules:
    psutil_stub = types.ModuleType('psutil')
    psutil_stub.net_connections = lambda: []
    sys.modules['psutil'] = psutil_stub

if 'jupyter_server' not in sys.modules:
    jupyter_server_stub = types.ModuleType('jupyter_server')
    base_stub = types.ModuleType('jupyter_server.base')
    handlers_stub = types.ModuleType('jupyter_server.base.handlers')
    utils_stub = types.ModuleType('jupyter_server.utils')

    class APIHandler:
        pass

    handlers_stub.APIHandler = APIHandler
    utils_stub.url_path_join = lambda *parts: '/'.join(str(part).strip('/') for part in parts if part != '')
    sys.modules['jupyter_server'] = jupyter_server_stub
    sys.modules['jupyter_server.base'] = base_stub
    sys.modules['jupyter_server.base.handlers'] = handlers_stub
    sys.modules['jupyter_server.utils'] = utils_stub

if 'tornado' not in sys.modules:
    tornado_stub = types.ModuleType('tornado')
    web_stub = types.ModuleType('tornado.web')

    def authenticated(function):
        return function

    class StaticFileHandler:
        def prepare(self):
            return None

    web_stub.authenticated = authenticated
    web_stub.StaticFileHandler = StaticFileHandler
    tornado_stub.web = web_stub
    sys.modules['tornado'] = tornado_stub
    sys.modules['tornado.web'] = web_stub

from mindstudio_insight_jupyterlab import handlers


class FakeProcess:
    def __init__(self, wait_error=None):
        self.wait_error = wait_error
        self.terminated = False
        self.killed = False

    def terminate(self):
        self.terminated = True

    def wait(self, timeout):
        assert timeout == 3
        if self.wait_error:
            raise self.wait_error

    def kill(self):
        self.killed = True


class NodeVersionResult:
    def __init__(self, version='v22.14.0'):
        self.stdout = version


def _patch_node(monkeypatch, path='/bin/node', version='v22.14.0'):
    monkeypatch.setattr(handlers.shutil, 'which', lambda *_args, **_kwargs: path)
    monkeypatch.setattr(handlers.subprocess, 'run', lambda *_args, **_kwargs: NodeVersionResult(version))
    monkeypatch.setattr(handlers, 'wait_for_tcp', lambda *_args, **_kwargs: True)


@pytest.fixture(autouse=True)
def reset_process_state():
    handlers.profiler_process.clear()
    handlers.acp_process.clear()
    handlers.acp_capability_tokens.clear()
    yield
    handlers.profiler_process.clear()
    handlers.acp_process.clear()
    handlers.acp_capability_tokens.clear()


def test_handlers_module_has_one_header_and_one_definition_per_name():
    with open(handlers.__file__, 'r', encoding='utf-8') as file:
        source = file.read()
    tree = ast.parse(source)
    definitions = [node.name for node in tree.body if isinstance(node, (ast.FunctionDef, ast.ClassDef))]

    assert source.count('#!/usr/bin/env python') == 1
    assert len(definitions) == len(set(definitions))


def test_start_profiler_server_records_spawn_and_returns_true(monkeypatch):
    process = FakeProcess()
    monkeypatch.setattr(handlers, 'ensure_mindstudio_insight_dir', lambda: '/tmp/cache')
    monkeypatch.setattr(handlers, 'get_local_ip', lambda: '127.0.0.1')
    monkeypatch.setattr(handlers, 'find_available_port', lambda *_args: 9010)
    monkeypatch.setattr(handlers.os.path, 'isfile', lambda _path: True)
    monkeypatch.setattr(handlers.os, 'chmod', lambda *_args: None)
    monkeypatch.setattr(handlers, 'get_process_env', lambda: {})
    monkeypatch.setattr(handlers.subprocess, 'Popen', lambda *_args, **_kwargs: process)
    monkeypatch.setattr(handlers.uuid, 'uuid4', lambda: 'new-profiler')

    assert handlers.start_profiler_server() is True
    assert handlers.available_port == 9010
    assert handlers.profiler_server_id == 'new-profiler'
    assert handlers.profiler_process == {'new-profiler': process}


@pytest.mark.parametrize('failure', ['allocation', 'path', 'spawn'])
def test_start_profiler_server_controls_expected_failures(monkeypatch, failure):
    monkeypatch.setattr(handlers, 'ensure_mindstudio_insight_dir', lambda: '/tmp/cache')
    monkeypatch.setattr(handlers, 'get_local_ip', lambda: '127.0.0.1')
    monkeypatch.setattr(handlers, 'find_available_port', lambda *_args: None if failure == 'allocation' else 9010)
    monkeypatch.setattr(handlers.os.path, 'isfile', lambda _path: failure != 'path')
    monkeypatch.setattr(handlers.os, 'chmod', lambda *_args: None)
    monkeypatch.setattr(handlers, 'get_process_env', lambda: {})

    def popen(*_args, **_kwargs):
        if failure == 'spawn':
            raise OSError('spawn failed')
        return FakeProcess()

    monkeypatch.setattr(handlers.subprocess, 'Popen', popen)

    assert handlers.start_profiler_server() is False
    assert handlers.profiler_process == {}


@pytest.mark.parametrize('failure', ['entry', 'node', 'spawn'])
def test_start_acp_returns_status_for_path_node_and_spawn_failures(monkeypatch, failure):
    handlers.profiler_server_id = 'profiler'
    handlers.profiler_process['profiler'] = FakeProcess()
    monkeypatch.setattr(handlers, 'ensure_mindstudio_insight_dir', lambda: '/tmp/cache')
    monkeypatch.setattr(handlers.os.path, 'isfile', lambda _path: failure != 'entry')
    monkeypatch.setattr(handlers, 'get_process_env', lambda: {'PATH': '/bin'})
    if failure == 'node':
        monkeypatch.setattr(handlers.shutil, 'which', lambda *_args, **_kwargs: None)
    else:
        _patch_node(monkeypatch)
    monkeypatch.setattr(handlers, 'get_local_ip', lambda: '127.0.0.1')
    monkeypatch.setattr(handlers, 'find_available_port', lambda *_args: 9011)

    def popen(*_args, **_kwargs):
        if failure == 'spawn':
            raise OSError('spawn failed')
        return FakeProcess()

    monkeypatch.setattr(handlers.subprocess, 'Popen', popen)

    result = handlers.start_acp_node_service('http://127.0.0.1:8888')
    expected = {
        'entry': 'start-failed',
        'node': 'missing-node',
        'spawn': 'start-failed',
    }[failure]
    assert result['status'] == expected
    assert result['port'] is None
    assert handlers.acp_process == {}


def test_acp_failure_keeps_profiler_and_returns_status(monkeypatch):
    old_profiler = FakeProcess()
    new_profiler = FakeProcess()
    handlers.profiler_process['old'] = old_profiler

    def start_profiler():
        handlers.profiler_server_id = 'new'
        handlers.profiler_process['new'] = new_profiler
        handlers.available_port = 9010
        return True

    monkeypatch.setattr(handlers, 'start_profiler_server', start_profiler)
    monkeypatch.setattr(handlers, 'start_acp_node_service', lambda _origin: {
        'status': 'missing-node',
        'port': None,
        'token': None,
        'nodeVersion': None,
    })
    monkeypatch.setattr(handlers, 'check_jupyter_server_proxy_installed', lambda: False)

    class FakeHandler:
        request = types.SimpleNamespace(protocol='http', host='127.0.0.1:8888')

        def set_status(self, status):
            self.status = status

        def finish(self, body):
            self.body = body

    get_method = getattr(handlers.IFrameConfigHandler.get, '__wrapped__', handlers.IFrameConfigHandler.get)
    request = FakeHandler()
    get_method(request)

    assert getattr(request, 'status', 200) != 503
    payload = json.loads(request.body)
    assert payload['acpStatus'] == 'missing-node'
    assert payload['port'] == 9010
    assert payload['profilerServerId'] == 'new'
    assert 'acpPort' not in payload
    assert handlers.profiler_process == {'old': old_profiler, 'new': new_profiler}
    assert not new_profiler.terminated
    assert not old_profiler.terminated


def test_id_scoped_and_shutdown_cleanup_terminate_paired_children_safely():
    profiler_one = FakeProcess()
    profiler_two = FakeProcess()
    acp_one = FakeProcess()
    acp_two = FakeProcess()
    orphan_acp = FakeProcess()
    handlers.profiler_process.update({'one': profiler_one, 'two': profiler_two})
    handlers.acp_process.update({'one': acp_one, 'two': acp_two, 'orphan': orphan_acp})

    handlers.stop_profiler_server('one')

    assert handlers.profiler_process == {'two': profiler_two}
    assert handlers.acp_process == {'two': acp_two, 'orphan': orphan_acp}
    assert profiler_one.terminated and acp_one.terminated
    assert not profiler_two.terminated and not acp_two.terminated

    handlers.shutdown_hook(None)

    assert handlers.profiler_process == {}
    assert handlers.acp_process == {}
    assert profiler_two.terminated and acp_two.terminated and orphan_acp.terminated


def test_rejects_old_node_before_allocating_or_spawning_acp(monkeypatch):
    handlers.profiler_server_id = 'profiler'
    handlers.profiler_process['profiler'] = FakeProcess()
    monkeypatch.setattr(handlers, 'get_process_env', lambda: {'PATH': '/bin'})
    monkeypatch.setattr(handlers.shutil, 'which', lambda *_args, **_kwargs: '/bin/node')
    monkeypatch.setattr(handlers.subprocess, 'run', lambda *_args, **_kwargs: NodeVersionResult('v22.13.9'))
    monkeypatch.setattr(handlers, 'find_available_port', lambda *_args: pytest.fail('port must not be exposed'))
    monkeypatch.setattr(handlers.subprocess, 'Popen', lambda *_args, **_kwargs: pytest.fail('must not spawn'))

    result = handlers.start_acp_node_service('http://127.0.0.1:8888')
    assert result['status'] == 'unsupported-node'
    assert result['nodeVersion'] == '22.13.9'
    assert result['port'] is None


def test_acp_launch_passes_random_capability_and_allowed_origin(monkeypatch):
    handlers.profiler_server_id = 'profiler'
    handlers.profiler_process['profiler'] = FakeProcess()
    monkeypatch.setattr(handlers, 'ensure_mindstudio_insight_dir', lambda: '/tmp/cache')
    monkeypatch.setattr(handlers.os.path, 'isfile', lambda _path: True)
    monkeypatch.setattr(handlers, 'get_process_env', lambda: {'PATH': '/bin'})
    _patch_node(monkeypatch)
    monkeypatch.setattr(handlers, 'get_local_ip', lambda: '127.0.0.1')
    monkeypatch.setattr(handlers, 'find_available_port', lambda *_args: 9011)
    monkeypatch.setattr(handlers.secrets, 'token_urlsafe', lambda _size: 'random-capability')
    captured = {}

    def popen(command, **options):
        captured['command'] = command
        captured['options'] = options
        return FakeProcess()

    monkeypatch.setattr(handlers.subprocess, 'Popen', popen)

    result = handlers.start_acp_node_service('http://127.0.0.1:8888')
    assert result == {
        'status': 'ready',
        'port': 9011,
        'token': 'random-capability',
        'nodeVersion': '22.14.0',
    }
    assert captured['command'][-2:] == ['--allowed-origin', 'http://127.0.0.1:8888']
    assert captured['command'][0] == '/bin/node'
    assert '--capability-token' not in captured['command']
    assert captured['options']['env']['ACP_CAPABILITY_TOKEN'] == 'random-capability'
    assert handlers.acp_capability_tokens == {'profiler': 'random-capability'}
    if sys.platform != 'win32':
        assert captured['options']['start_new_session'] is True


def test_allowed_origin_uses_forwarded_values_only_when_proxy_headers_are_trusted():
    request = types.SimpleNamespace(
        protocol='http',
        host='127.0.0.1:8888',
        headers={'X-Forwarded-Proto': 'https', 'X-Forwarded-Host': 'notebooks.example.com'},
    )

    assert handlers._get_allowed_origin(request) == 'http://127.0.0.1:8888'
    assert handlers._get_allowed_origin(request, trust_xheaders=True) == 'https://notebooks.example.com'


def test_parse_node_version():
    assert handlers.parse_node_version('v22.14.0') == (22, 14, 0)
    assert handlers.parse_node_version('22.13.9') == (22, 13, 9)
    assert handlers.parse_node_version('not-node') is None


def test_wait_for_tcp_detects_listening_port():
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    try:
        assert handlers.wait_for_tcp('127.0.0.1', port, timeout=1)
    finally:
        listener.close()
    assert not handlers.wait_for_tcp('127.0.0.1', port, timeout=0.2)
