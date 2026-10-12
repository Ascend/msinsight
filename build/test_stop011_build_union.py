"""Conflict-union regressions; all build and signing commands are stubbed."""

import importlib.util
from pathlib import Path

import pytest


def load_build_module():
    spec = importlib.util.spec_from_file_location("stop011_build_union", Path(__file__).with_name("build.py"))
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize("index", [None, "https://example.invalid/simple"])
def test_online_jupyter_dependency_helper_preserves_target_pip_index(monkeypatch, index):
    top = load_build_module()
    if index is None:
        monkeypatch.delenv("PIP_INDEX_URL", raising=False)
    else:
        monkeypatch.setenv("PIP_INDEX_URL", index)
    calls = []
    monkeypatch.setattr(top, "exec_command", lambda *args: calls.append(args) or 0)

    assert top.prepare_jupyterlab_build_dependencies("fixture-plugin", True) == 0
    assert calls == [(
        [top.Const.PIP, "install", "--retries", "5", "--timeout", "120", "-i",
         index or "https://pypi.org/simple", "-r", str(Path("fixture-plugin") / "requirements.txt")],
        "fixture-plugin", "jupyterlab_plugin",
    )]


@pytest.mark.parametrize("result", [0, 1])
def test_offline_jupyter_checks_only_preprovisioned_python_and_propagates_failure(monkeypatch, result):
    top = load_build_module()
    calls = []
    monkeypatch.setattr(top, "exec_command", lambda *args: calls.append(args) or result)

    assert top.prepare_jupyterlab_build_dependencies("fixture-plugin", False) == result
    assert calls == [(
        [top.Const.PYTHON, "-c", "import jupyter_packaging, jupyterlab, setuptools, wheel"],
        "fixture-plugin", "jupyterlab_plugin",
    )]


def test_jupyter_build_routes_offline_flag_and_stops_before_resource_copy_on_failure(monkeypatch):
    top = load_build_module()
    monkeypatch.setenv("BUILD_JUPYTERLAB", "true")
    calls = []
    monkeypatch.setattr(top, "prepare_jupyterlab_build_dependencies", lambda *args: calls.append(args) or 1)
    monkeypatch.setattr(top, "set_npm_config", lambda: pytest.fail("dependency failure must stop npm setup"))
    monkeypatch.setattr(top, "copy_resource_in_jupyterlab", lambda *_: pytest.fail("no build resource IO"))

    assert top.build_jupyterlab("26.1.1", "macos_aarch64", False) == 1
    assert calls == [(str(Path(top.PROJECT_PATH) / top.Const.JUPYTERLAB_PLUGINS_DIR), False)]


@pytest.mark.parametrize("packaged,enabled", [(True, True), (True, False), (False, True)])
def test_windows_installer_sign_verify_hash_are_only_after_success(monkeypatch, packaged, enabled):
    top = load_build_module()
    calls = []
    monkeypatch.setattr(top.platform, "system", lambda: top.Const.WINDOWS_OS)
    monkeypatch.setattr(top, "package_win", lambda *args: calls.append(("package", args)) or packaged)
    monkeypatch.setattr(top, "signing_enabled", lambda: enabled)
    monkeypatch.setattr(top, "sign_and_verify", lambda path: calls.append(("sign_verify", path)))
    monkeypatch.setattr(top, "print_sha256", lambda path: calls.append(("hash", path)))

    assert top.build_platform_package("fixture.exe", "package", "preview", "target") is packaged
    expected = [("package", ("fixture.exe", "preview"))]
    if packaged and enabled:
        expected.extend([("sign_verify", "fixture.exe"), ("hash", "fixture.exe")])
    assert calls == expected


@pytest.mark.parametrize("enabled", [True, False])
def test_windows_inner_signing_precedes_permission_changes_and_final_packaging(monkeypatch, enabled):
    top = load_build_module()
    calls = []
    monkeypatch.setattr(top.platform, "system", lambda: top.Const.WINDOWS_OS)
    monkeypatch.setattr(top.shutil, "copyfile", lambda *_: calls.append("copy_executable"))
    monkeypatch.setattr(top, "signing_enabled", lambda: enabled)
    targets = [Path("fixture.exe")]
    monkeypatch.setattr(top, "collect_inner_exes", lambda _: targets)
    monkeypatch.setattr(top, "sign_many", lambda paths: calls.append(("sign_many", paths)))
    monkeypatch.setattr(top, "traverse_folder_and_chmod", lambda *args: calls.append(("chmod", args)))
    monkeypatch.setattr(top.os.path, "isdir", lambda _: True)
    monkeypatch.setattr(top, "build_platform_package", lambda *args: calls.append(("package", args)) or True)

    assert top.zip_package("profiler", "fixture.exe", "preview", "target") == 0
    labels = [entry if isinstance(entry, str) else entry[0] for entry in calls]
    assert labels == ["copy_executable", *(["sign_many"] if enabled else []), "chmod", "chmod", "chmod", "package"]
    if enabled:
        assert calls[1] == ("sign_many", targets)
