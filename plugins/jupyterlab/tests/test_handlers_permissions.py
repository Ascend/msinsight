"""Permission and success-return regressions for the rebased profiler launcher.

These tests use the existing lifecycle stubs, never start a real service, and
must not load the user's Jupyter configuration or login shell.
"""

import errno
import types

import pytest

from test_handlers_lifecycle import FakeProcess, handlers


@pytest.fixture(autouse=True)
def clear_process_state():
    handlers.profiler_process.clear()
    handlers.acp_process.clear()
    yield
    handlers.profiler_process.clear()
    handlers.acp_process.clear()


def patch_profiler(monkeypatch, system, executable=True, spawn_error=False):
    process = FakeProcess()
    calls = []
    monkeypatch.setattr(handlers, 'sys', types.SimpleNamespace(platform=system))
    monkeypatch.setattr(handlers, 'ensure_mindstudio_insight_dir', lambda: '/tmp/test-only-cache')
    monkeypatch.setattr(handlers, 'get_local_ip', lambda: '127.0.0.1')
    monkeypatch.setattr(handlers, 'find_available_port', lambda *_args: 9010)
    monkeypatch.setattr(handlers.os.path, 'isfile', lambda _path: True)
    monkeypatch.setattr(handlers.os, 'access', lambda *_args: executable)
    monkeypatch.setattr(handlers, 'get_process_env', lambda: {})
    monkeypatch.setattr(handlers.uuid, 'uuid4', lambda: 'permission-test-profiler')

    def spawn(command, **options):
        calls.append((command, options))
        if spawn_error:
            raise OSError('test-only spawn failure')
        return process

    monkeypatch.setattr(handlers.subprocess, 'Popen', spawn)
    return process, calls


@pytest.mark.parametrize('system', ['linux', 'darwin'])
def test_already_executable_profiler_skips_chmod(monkeypatch, system):
    process, calls = patch_profiler(monkeypatch, system)
    monkeypatch.setattr(handlers.os, 'chmod', lambda *_args: pytest.fail('Executable file must not need chmod'))

    assert handlers.start_profiler_server() is True
    assert len(calls) == 1
    assert handlers.profiler_process == {'permission-test-profiler': process}
    assert calls[0][0][0] == './profiler_server'


@pytest.mark.parametrize('system', ['linux', 'darwin'])
@pytest.mark.parametrize('error_number', [errno.EPERM, errno.EROFS])
def test_chmod_restriction_does_not_prevent_successful_spawn(monkeypatch, system, error_number):
    process, calls = patch_profiler(monkeypatch, system, executable=False)

    def denied_chmod(*_args):
        raise OSError(error_number, 'test-only permission restriction')

    monkeypatch.setattr(handlers.os, 'chmod', denied_chmod)

    assert handlers.start_profiler_server() is True
    assert len(calls) == 1
    assert handlers.profiler_process == {'permission-test-profiler': process}


def test_windows_success_explicitly_returns_true(monkeypatch):
    process, calls = patch_profiler(monkeypatch, 'win32')
    monkeypatch.setattr(handlers.os, 'chmod', lambda *_args: None)

    assert handlers.start_profiler_server() is True
    assert len(calls) == 1
    assert calls[0][0][0].endswith('profiler_server.exe')
    assert 'cwd' not in calls[0][1]
    assert handlers.profiler_process == {'permission-test-profiler': process}


def test_real_spawn_failure_remains_false_after_permission_tolerance(monkeypatch):
    _process, calls = patch_profiler(monkeypatch, 'linux', executable=False, spawn_error=True)

    def denied_chmod(*_args):
        raise OSError(errno.EPERM, 'test-only permission restriction')

    monkeypatch.setattr(handlers.os, 'chmod', denied_chmod)

    assert handlers.start_profiler_server() is False
    assert len(calls) == 1
    assert handlers.profiler_process == {}
