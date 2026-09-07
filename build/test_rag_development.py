import hashlib
import importlib.util
import json
import os
import subprocess
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

from rag_development import (
    DevelopmentVersion,
    ProductVersion,
    VersionFileTransaction,
    capture_source_snapshot,
    preflight_rag_inputs,
    select_exact_installer,
    validate_rag_arguments,
    write_installer_evidence,
    write_internal_metadata,
)


def test_development_version_maps_string_to_exact_pe_numeric_identity() -> None:
    version = DevelopmentVersion.parse("26.1.1-rag-dev.1")

    assert version.product == "26.1.1-rag-dev.1"
    assert version.pe_numeric == "26.1.1.1"
    for invalid in ["26.1.1", "26.1.1-rag-dev.0", "26.1-rag-dev.1", "26.1.1-rag-dev.01"]:
        with pytest.raises(ValueError):
            DevelopmentVersion.parse(invalid)


def test_product_version_maps_plain_release_to_stable_pe_identity() -> None:
    version = ProductVersion.parse("26.1.1")
    assert version.product == "26.1.1"
    assert version.pe_numeric == "26.1.1.0"
    for invalid in ["26.1.1-rag-dev.1", "26.1", "26.1.1.0"]:
        with pytest.raises(ValueError):
            ProductVersion.parse(invalid)


def test_rag_arguments_are_all_or_none_and_reject_forbidden_values(tmp_path: Path) -> None:
    pack = tmp_path / "knowledge-pack-v4.zip"
    sidecar = tmp_path / "knowledge-pack-v4.zip.sha256"
    model = tmp_path / "model"
    model.mkdir()

    options = validate_rag_arguments(
        build_version="26.1.1-rag-dev.1",
        mode="development",
        pack=pack,
        sidecar=sidecar,
        model_dir=model,
    )
    assert options.version.pe_numeric == "26.1.1.1"

    product = validate_rag_arguments(
        build_version="26.1.1",
        mode="product-bundled",
        pack=pack,
        sidecar=sidecar,
        model_dir=model,
    )
    assert product.version.pe_numeric == "26.1.1.0"
    assert product.mode == "product-bundled"

    with pytest.raises(ValueError):
        validate_rag_arguments(
            build_version="26.1.1-rag-dev.1",
            mode="development",
            pack=pack,
            sidecar=None,
            model_dir=model,
        )
    with pytest.raises(ValueError):
        validate_rag_arguments(
            build_version="26.1.1-rag-dev.1",
            mode="release",
            pack=pack,
            sidecar=sidecar,
            model_dir=model,
        )


def test_preflight_binds_exact_package_sidecar_before_output(tmp_path: Path) -> None:
    import zipfile

    pack = tmp_path / "knowledge-pack-v4.zip"
    with zipfile.ZipFile(pack, "w") as archive:
        archive.writestr(
            "manifest.json",
            json.dumps(
                {
                    "schemaVersion": "4.0",
                    "kbVersion": "26.1.2",
                    "sourceSet": {"sourceSetId": "ss_" + "1" * 64, "sha256": "2" * 64},
                }
            ),
        )
    digest = hashlib.sha256(pack.read_bytes()).hexdigest()
    sidecar = tmp_path / "knowledge-pack-v4.zip.sha256"
    sidecar.write_bytes(f"{digest}  knowledge-pack-v4.zip\n".encode())
    model = tmp_path / "model"
    model.mkdir()
    (model / "model-manifest.json").write_text("{}", encoding="utf-8")

    facts = preflight_rag_inputs(pack, sidecar, model)

    assert facts.package_sha256 == digest
    assert facts.kb_version == "26.1.2"
    assert facts.source_set_id == "ss_" + "1" * 64
    sidecar.write_bytes(f"{'0' * 64}  knowledge-pack-v4.zip\n".encode())
    with pytest.raises(ValueError):
        preflight_rag_inputs(pack, sidecar, model)


def test_source_snapshot_distinguishes_clean_dirty_staged_and_untracked(tmp_path: Path) -> None:
    repo = tmp_path / "repo"
    repo.mkdir()
    git(repo, "init")
    git(repo, "config", "user.email", "test@example.com")
    git(repo, "config", "user.name", "Test")
    tracked = repo / "tracked.txt"
    tracked.write_text("one\n", encoding="utf-8")
    git(repo, "add", "tracked.txt")
    git(repo, "commit", "-m", "initial")

    clean = capture_source_snapshot(repo)
    assert clean.tree_state == "clean"

    tracked.write_text("two\n", encoding="utf-8")
    git(repo, "add", "tracked.txt")
    (repo / "untracked.txt").write_text("new\n", encoding="utf-8")
    dirty = capture_source_snapshot(repo)

    assert dirty.tree_state == "dirty"
    assert dirty.sha256 != clean.sha256
    assert {record["kind"] for record in dirty.records} == {"tracked", "untracked"}
    assert all("\\" not in record["path"] and not Path(record["path"]).is_absolute() for record in dirty.records)


def test_version_file_transaction_restores_bytes_and_absence(tmp_path: Path) -> None:
    existing = tmp_path / "existing.txt"
    created = tmp_path / "created.txt"
    existing.write_bytes(b"before")

    with VersionFileTransaction((existing, created)):
        existing.write_bytes(b"after")
        created.write_bytes(b"created")

    assert existing.read_bytes() == b"before"
    assert not created.exists()


def test_internal_metadata_is_single_write_path_safe_and_false_release(tmp_path: Path) -> None:
    destination = tmp_path / "rag-build-mode.json"
    source = capture_fixture_snapshot()
    metadata = {
        "schemaVersion": "1.0",
        "mode": "development",
        "productVersion": "26.1.1-rag-dev.1",
        "peNumericVersion": "26.1.1.1",
        "consumerAcceptanceEvaluated": False,
        "promotionEvaluated": False,
        "releaseEligible": False,
        "releaseStatus": "development-integration-only",
        "softwareSource": source,
    }

    write_internal_metadata(destination, metadata)

    payload = json.loads(destination.read_bytes())
    serialized = destination.read_text(encoding="utf-8")
    assert payload["releaseEligible"] is False
    assert payload["softwareSource"]["treeState"] == "dirty"
    assert str(tmp_path) not in serialized
    assert "secret" not in serialized.casefold()
    with pytest.raises(FileExistsError):
        write_internal_metadata(destination, metadata)


def test_exact_installer_selection_never_uses_prefix_first_match(tmp_path: Path) -> None:
    expected = tmp_path / "MindStudio-Insight_26.1.1-rag-dev.1_win.exe"
    expected.write_bytes(b"final")
    (tmp_path / "MindStudio-Insight_26.1.1-rag-dev.10_win.exe").write_bytes(b"other")

    assert select_exact_installer(tmp_path, "26.1.1-rag-dev.1", "win") == expected


def test_installer_evidence_is_created_only_after_exact_final_exe(tmp_path: Path) -> None:
    installer = tmp_path / "MindStudio-Insight_26.1.1-rag-dev.1_win.exe"
    installer.write_bytes(b"final-installer")
    metadata = {
        "schemaVersion": "1.0",
        "mode": "development",
        "productVersion": "26.1.1-rag-dev.1",
        "releaseEligible": False,
    }

    sidecar, companion = write_installer_evidence(installer, metadata)

    digest = hashlib.sha256(installer.read_bytes()).hexdigest()
    assert sidecar.read_bytes() == f"{digest}  {installer.name}\n".encode()
    payload = json.loads(companion.read_bytes())
    assert payload["installer"] == {
        "fileName": installer.name,
        "sha256": digest,
        "sizeBytes": len(installer.read_bytes()),
    }
    assert payload["releaseEligible"] is False


def test_top_level_version_helpers_update_string_numeric_and_cargo_identity(tmp_path: Path) -> None:
    top = load_top_build_module()
    assert top.extract_numeric_part("26.1.1-rag-dev.1") == ["26", "1", "1", "1"]
    rc = (
        "FILEVERSION 1, 0, 0, 0\n"
        "PRODUCTVERSION 1, 0, 0, 0\n"
        'VALUE "FileVersion", "1.0.0.0"\n'
        'VALUE "ProductVersion", "1.0.0"\n'
    )
    updated = top.replace_version_block(
        rc,
        "26, 1, 1, 1",
        "26.1.1.1",
        "26.1.1-rag-dev.1",
    )
    assert "FILEVERSION 26, 1, 1, 1" in updated
    assert 'VALUE "ProductVersion", "26.1.1-rag-dev.1"' in updated

    cargo = tmp_path / "Cargo.toml"
    cargo.write_text('[package]\nname = "insight"\nversion = "0.1.0"\n', encoding="utf-8")
    top.update_cargo_package_version(cargo, "26.1.1-rag-dev.1")
    assert 'version = "26.1.1-rag-dev.1"' in cargo.read_text(encoding="utf-8")


def test_top_level_reads_complete_rag_environment(tmp_path: Path) -> None:
    top = load_top_build_module()
    args = SimpleNamespace(
        build_version="26.1.1-rag-dev.1",
        whl_version=None,
        type=None,
    )
    environment = {
        top.Const.RAG_MODE_ENV: "development",
        top.Const.RAG_PACKAGE_ENV: str(tmp_path / "environment-pack.zip"),
        top.Const.RAG_PACKAGE_SHA256_ENV: str(tmp_path / "environment-pack.zip.sha256"),
        top.Const.RAG_MODEL_DIR_ENV: str(tmp_path / "environment-model"),
    }

    context = top.build_context_from_args(args, environment)

    assert context.rag.pack == tmp_path / "environment-pack.zip"
    assert context.rag.sidecar == tmp_path / "environment-pack.zip.sha256"
    assert context.rag.model_dir == tmp_path / "environment-model"
    assert context.rag.version.product == "26.1.1-rag-dev.1"


def test_top_level_rejects_partial_rag_environment() -> None:
    top = load_top_build_module()
    args = SimpleNamespace(
        build_version="26.1.1-rag-dev.1",
        whl_version=None,
        type=None,
    )

    with pytest.raises(ValueError, match="bundled RAG options must be complete"):
        top.build_context_from_args(args, {top.Const.RAG_PACKAGE_ENV: "pack.zip"})
    with pytest.raises(ValueError, match="bundled RAG options must be complete"):
        top.build_context_from_args(args, {top.Const.RAG_MODE_ENV: "development"})
    with pytest.raises(ValueError, match="bundled RAG options must be complete"):
        top.build_context_from_args(args, {top.Const.RAG_MODE_ENV: "release"})


def test_top_level_passes_resolved_rag_inputs_only_through_child_environment(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    top = load_top_build_module()
    # Pin the product target so the expected child environment is host-independent.
    monkeypatch.setattr(top, "get_os_tag", lambda: "win")
    args = SimpleNamespace(
        build_version="26.1.1-rag-dev.1",
        whl_version=None,
        type=None,
    )
    environment = {
        top.Const.RAG_MODE_ENV: "development",
        top.Const.RAG_PACKAGE_ENV: str(tmp_path / "pack.zip"),
        top.Const.RAG_PACKAGE_SHA256_ENV: str(tmp_path / "pack.zip.sha256"),
        top.Const.RAG_MODEL_DIR_ENV: str(tmp_path / "model"),
    }
    context = top.build_context_from_args(args, environment)

    child_environment = top.rag_subprocess_environment(context, {"PATH": "test-path"})
    source = (Path(__file__).parent / "build.py").read_text(encoding="utf-8")
    acp_builder = source[source.index("def build_acp_node_service") : source.index("def set_npm_config")]

    assert child_environment == {
        "PATH": "test-path",
        top.Const.RAG_TARGET_PLATFORM_ENV: "win32",
        top.Const.RAG_TARGET_ARCH_ENV: "x64",
        top.Const.RAG_MODE_ENV: "development",
        top.Const.RAG_PACKAGE_ENV: str(tmp_path / "pack.zip"),
        top.Const.RAG_PACKAGE_SHA256_ENV: str(tmp_path / "pack.zip.sha256"),
        top.Const.RAG_MODEL_DIR_ENV: str(tmp_path / "model"),
    }
    assert "--rag-dev-pack" not in acp_builder
    assert "--rag-dev-sidecar" not in acp_builder
    assert "--rag-model-dir" not in acp_builder

    captured = {}

    def fail_after_capture(command, path, module_name, env=None):  # type: ignore[no-untyped-def]
        captured.update(command=command, path=path, module_name=module_name, env=env)
        return 1

    monkeypatch.setattr(top.os, "environ", {"PATH": "test-path"})
    monkeypatch.setattr(top, "exec_command", fail_after_capture)

    assert top.build_acp_node_service(context) == 1
    assert captured["command"] == [top.Const.PNPM, "server:build"]
    assert captured["env"] == child_environment


def test_top_level_maps_every_product_os_tag_to_one_rag_target() -> None:
    top = load_top_build_module()
    assert top.rag_target_environment("win") == {
        top.Const.RAG_TARGET_PLATFORM_ENV: "win32",
        top.Const.RAG_TARGET_ARCH_ENV: "x64",
    }
    assert top.rag_target_environment("linux_x86_64") == {
        top.Const.RAG_TARGET_PLATFORM_ENV: "linux",
        top.Const.RAG_TARGET_ARCH_ENV: "x64",
        top.Const.RAG_TARGET_LIBC_ENV: "glibc",
    }
    assert top.rag_target_environment("linux_aarch64")[top.Const.RAG_TARGET_ARCH_ENV] == "arm64"
    assert top.rag_target_environment("macos_x86_64")[top.Const.RAG_TARGET_PLATFORM_ENV] == "darwin"
    assert top.rag_target_environment("macos_aarch64")[top.Const.RAG_TARGET_ARCH_ENV] == "arm64"
    with pytest.raises(ValueError, match="Unsupported RAG product target"):
        top.rag_target_environment("linux_musl")


def test_mac_native_runtime_signing_orders_dylibs_before_node_bindings(tmp_path: Path) -> None:
    top = load_top_build_module()
    dylib = tmp_path / "runtime" / "libonnxruntime.1.22.0.dylib"
    shared_object = tmp_path / "runtime" / "python-extension.so"
    binding = tmp_path / "runtime" / "onnxruntime_binding.node"
    executable = tmp_path / "runtime" / "profiler_server"
    ignored = tmp_path / "runtime" / "package.json"
    dylib.parent.mkdir(parents=True)
    for path in (dylib, shared_object, binding, executable):
        path.write_bytes(b"\xcf\xfa\xed\xfe" + b"fixture")
    ignored.write_text("fixture", encoding="utf-8")

    assert top.mac_native_sign_targets(str(tmp_path)) == [
        str(dylib),
        str(shared_object),
        str(binding),
        str(executable),
    ]


def test_mac_rag_bundle_root_supports_only_standard_resources_layout(tmp_path: Path) -> None:
    top = load_top_build_module()
    app = tmp_path / "MindStudioInsight.app"
    legacy = app / "Contents" / "MacOS" / "resources" / "profiler" / "server" / "insight_web_agent"
    manifest_path = legacy / "rag-runtime" / "native-runtime-manifest.json"
    manifest_path.parent.mkdir(parents=True)
    manifest_path.write_text(json.dumps({"schemaVersion": "1.0", "files": []}), encoding="utf-8")

    assert top.find_mac_rag_bundle_root(str(app)) is None
    # Legacy layouts are skipped instead of refreshed.
    assert top.refresh_mac_native_runtime_manifest(str(app)) is True
    assert top.verify_mac_packaged_rag(str(app)) is True


def test_mac_signing_does_not_deep_resign_native_runtime(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    top = load_top_build_module()
    app = tmp_path / "MindStudioInsight.app"
    bundle = app / "Contents" / "Resources" / "profiler" / "server" / "insight_web_agent"
    native = bundle / "node_modules" / "onnxruntime-node" / "binding.node"
    native.parent.mkdir(parents=True)
    native.write_bytes(b"\xcf\xfa\xed\xfe" + b"unsigned")
    manifest_path = bundle / "rag-runtime" / "native-runtime-manifest.json"
    manifest_path.parent.mkdir(parents=True)
    manifest_path.write_text(
        json.dumps(
            {
                "schemaVersion": "1.0",
                "files": [
                    {
                        "path": str(native.relative_to(bundle)).replace("\\", "/"),
                        "sha256": "0" * 64,
                        "sizeBytes": 1,
                    }
                ],
            }
        ),
        encoding="utf-8",
    )
    calls = []

    def sign(command, path, module_name, env=None):  # type: ignore[no-untyped-def]
        calls.append(command)
        if command[-1] == str(native):
            native.write_bytes(native.read_bytes() + b"-signed")
        return 0

    monkeypatch.setattr(top, "exec_command", sign)

    assert top.sign_mac_app(str(app), "-") is True
    outer_sign = calls[-2]
    verification = calls[-1]
    assert outer_sign[-1] == str(app)
    assert "--deep" not in outer_sign
    assert verification == ["codesign", "--verify", "--deep", "--strict", str(app)]
    refreshed = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert refreshed["files"][0]["sha256"] == hashlib.sha256(native.read_bytes()).hexdigest()


def test_package_mac_uses_standard_resources_with_executable_compatibility_link(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    top = load_top_build_module()
    preview = tmp_path / "preview"
    target = tmp_path / "target"
    app = target / "release" / "bundle" / "osx" / top.Const.MAC_OS_APPNAME
    macos = app / "Contents" / "MacOS"
    resources = app / "Contents" / "Resources"
    macos.mkdir(parents=True)
    resources.mkdir(parents=True)
    (macos / "MindStudioInsight").write_bytes(b"binary")
    python = preview / "resources" / "profiler" / "server" / "python" / "bin" / "python3"
    python.parent.mkdir(parents=True)
    python.write_bytes(b"python")
    symlinks = []

    monkeypatch.setattr(top.platform, "system", lambda: top.Const.MAC_OS)
    monkeypatch.setattr(
        top.os,
        "symlink",
        lambda source, destination, target_is_directory=False: symlinks.append(
            (source, destination, target_is_directory)
        ),
    )
    monkeypatch.setattr(top, "resign_mac_app", lambda _: True)
    monkeypatch.setattr(top, "chmod_mac_app", lambda *_: True)
    monkeypatch.setattr(top, "verify_mac_packaged_rag", lambda _: True)
    monkeypatch.setattr(top, "build_dmg_for_mac_app", lambda path: Path(path).write_bytes(b"dmg") > 0)

    assert (
        top.package_mac(
            str(tmp_path / "out.dmg"), "package_macos_aarch64.dmg", str(preview), str(target)
        )
        is True
    )
    packaged = preview / top.Const.MAC_OS_APPNAME
    assert (packaged / "Contents" / "Resources" / "python" / "bin" / "python3").is_file()
    assert not (packaged / "Contents" / "Resources" / "profiler" / "server" / "python").exists()
    assert symlinks == [("../Resources", str(macos / "resources"), True)]


def test_mac_signing_refreshes_native_runtime_manifest_after_nested_signatures(tmp_path: Path) -> None:
    top = load_top_build_module()
    bundle = (
        tmp_path
        / "MindStudioInsight.app"
        / "Contents"
        / "Resources"
        / "profiler"
        / "server"
        / "insight_web_agent"
    )
    native = bundle / "node_modules" / "onnxruntime-node" / "binding.node"
    native.parent.mkdir(parents=True)
    native.write_bytes(b"signed-native-bytes")
    manifest_path = bundle / "rag-runtime" / "native-runtime-manifest.json"
    manifest_path.parent.mkdir(parents=True)
    manifest = {
        "schemaVersion": "1.0",
        "files": [
            {
                "path": "node_modules/onnxruntime-node/binding.node",
                "sha256": "0" * 64,
                "sizeBytes": 1,
            }
        ],
    }
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    assert top.refresh_mac_native_runtime_manifest(str(tmp_path / "MindStudioInsight.app")) is True
    refreshed = json.loads(manifest_path.read_text(encoding="utf-8"))
    assert refreshed["files"][0]["sizeBytes"] == len(b"signed-native-bytes")
    assert refreshed["files"][0]["sha256"] == hashlib.sha256(b"signed-native-bytes").hexdigest()

    refreshed["files"][0]["path"] = "../../outside.node"
    manifest_path.write_text(json.dumps(refreshed), encoding="utf-8")
    assert top.refresh_mac_native_runtime_manifest(str(tmp_path / "MindStudioInsight.app")) is False


def test_offline_jupyter_build_checks_preprovisioned_dependencies_without_pip(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    top = load_top_build_module()
    calls = []

    def capture(command, path, module_name, env=None):  # type: ignore[no-untyped-def]
        calls.append((command, path, module_name, env))
        return 0

    monkeypatch.setattr(top, "exec_command", capture)
    assert top.prepare_jupyterlab_build_dependencies(str(tmp_path), False) == 0
    assert calls == [
        (
            [top.Const.PYTHON, "-c", "import jupyter_packaging, jupyterlab, setuptools, wheel"],
            str(tmp_path),
            "jupyterlab_plugin",
            None,
        )
    ]

    calls.clear()
    assert top.prepare_jupyterlab_build_dependencies(str(tmp_path), True) == 0
    assert calls[0][0][0:2] == [top.Const.PIP, "install"]
    assert calls[0][0][-2:] == ["-r", str(tmp_path / "requirements.txt")]


def test_top_level_rejects_removed_rag_options_and_invalid_inputs_before_cleanup() -> None:
    script = Path(__file__).with_name("build.py")
    clean_environment = os.environ.copy()
    for name in ("MSINSIGHT_RAG_MODE", "MSINSIGHT_RAG_PACKAGE", "MSINSIGHT_RAG_PACKAGE_SHA256", "MSINSIGHT_RAG_MODEL_DIR"):
        clean_environment.pop(name, None)
    help_result = subprocess.run(
        [str(Path(sys.executable)), str(script), "--help"],
        check=False,
        capture_output=True,
        env=clean_environment,
    )
    assert help_result.returncode == 0
    assert b"--rag-" not in help_result.stdout

    for args in [
        ["--unknown-option"],
        [
            "--build_version",
            "26.1.1-rag-dev.1",
            "--rag-mode",
            "development",
            "--rag-dev-pack",
            "pack.zip",
        ],
        [
            "--build_version",
            "26.1.1-rag-dev.1",
            "--rag-mode",
            "development",
            "--rag-dev-pack",
            "pack.zip",
            "--rag-dev-sidecar",
            "pack.sha256",
            "--rag-model-dir",
            "model",
            "--data-dir",
            "other",
        ],
    ]:
        result = subprocess.run(
            [str(Path(sys.executable)), str(script), *args],
            check=False,
            capture_output=True,
            env=clean_environment,
        )
        assert result.returncode != 0
        # Removed options must be rejected by argparse itself, not by later validation.
        assert b"unrecognized arguments" in result.stderr

    partial_environment = {**clean_environment, "MSINSIGHT_RAG_PACKAGE": "pack.zip"}
    result = subprocess.run(
        [str(Path(sys.executable)), str(script), "--build_version", "26.1.1-rag-dev.1"],
        check=False,
        capture_output=True,
        env=partial_environment,
    )
    assert result.returncode != 0
    assert b"bundled RAG options must be complete" in result.stderr

    result = subprocess.run(
        [str(Path(sys.executable)), str(script), "clean"],
        check=False,
        capture_output=True,
        env=partial_environment,
    )
    assert result.returncode != 0
    assert b"clean command does not accept" in result.stderr


def test_cleanup_preserves_historical_rag_evidence_and_tracked_test_paths(tmp_path: Path) -> None:
    top = load_top_build_module()
    top.PROJECT_PATH = str(tmp_path)
    evidence = tmp_path / "out" / "rag" / "dev-integration" / "26.1.1" / "evidence.json"
    disposable = tmp_path / "out" / "temporary.txt"
    tracked_test = tmp_path / "scripts" / "MemSnapDump" / "test" / "sentinel.txt"
    evidence.parent.mkdir(parents=True)
    disposable.parent.mkdir(parents=True, exist_ok=True)
    tracked_test.parent.mkdir(parents=True)
    evidence.write_text("keep", encoding="utf-8")
    disposable.write_text("remove", encoding="utf-8")
    tracked_test.write_text("keep", encoding="utf-8")

    top.clean()

    assert evidence.read_text(encoding="utf-8") == "keep"
    assert tracked_test.read_text(encoding="utf-8") == "keep"
    assert not disposable.exists()


def test_nsis_uses_forced_crc_and_extraction_errors_without_rag_validation() -> None:
    installer = (Path(__file__).parents[1] / "platform" / "bundle" / "installer.nsi").read_text(
        encoding="utf-8"
    )

    remove = installer.index('RMDir /r "$INSTDIR\\resources\\profiler\\server\\insight_web_agent\\rag-data"')
    copy = installer.index('File /r "resources\\*"')
    extraction_error = installer.index("${If} ${Errors}", copy)
    shortcut = installer.index("CreateShortCut")
    registry = installer.index('WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\MindStudio Insight"')

    assert remove < copy < extraction_error < shortcut < registry
    assert "RequestExecutionLevel admin" in installer
    assert "CRCCheck force" in installer
    assert '!define PE_NUMERIC_VERSION "{plugins_numeric_version}"' in installer
    assert 'VIProductVersion "${PE_NUMERIC_VERSION}"' in installer
    assert 'VIAddVersionKey /LANG=1033 "ProductVersion" "${CURRENT_VERSION}"' in installer
    for forbidden in ("RagResolveNode", "VerifyRagBundle", "$PLUGINSDIR\\rag-stage", "rag-install.nsh"):
        assert forbidden not in installer
    assert not (Path(__file__).parents[1] / "platform" / "bundle" / "rag-install.nsh").exists()
    assert "rag-backup" not in installer.casefold()
    assert "$profile\\rag-data" not in installer.casefold()


def test_development_server_build_uses_prepared_offline_dependencies() -> None:
    top_level = (Path(__file__).parent / "build.py").read_text(encoding="utf-8")
    server_build = (Path(__file__).parents[1] / "server" / "build" / "build.py").read_text(
        encoding="utf-8"
    )
    preprocess = (
        Path(__file__).parents[1] / "server" / "build" / "preprocess_third_party.py"
    ).read_text(encoding="utf-8")
    server_library = (
        Path(__file__).parents[1] / "server" / "msinsight" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    sqlite_library = (
        Path(__file__).parents[1] / "server" / "third_party" / "sqlite" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    server_cmake = (Path(__file__).parents[1] / "server" / "CMakeLists.txt").read_text(
        encoding="utf-8"
    )

    assert "def build_server_offline" in top_level
    assert "preprocess_command.append('--offline')" in top_level
    assert "server_command.extend(['--no-install', '--jobs', '2'])" in top_level
    assert "cmd_list.insert(1, '--offline')" in top_level
    assert "cmd_list.append('--offline')" not in top_level
    assert "if context.rag is not None:" in top_level
    assert "for name, builder in [('server', server_builder), ('frontend', frontend_builder)]" in top_level
    assert "allow_dependency_install and pip_install_third_party" in server_build
    assert "['cmake', '--build', '.', '-j', str(jobs)]" in server_build
    assert "shutil.copytree(os.path.join(SCRIPTS_DIR, 'MemSnapDump')" in server_build
    assert "shutil.move(os.path.join(SCRIPTS_DIR, 'MemSnapDump')" not in server_build
    assert "if offline:" in preprocess
    assert "urlretrieve" in preprocess  # ordinary builds retain their existing preparation path
    for symbol in ("__stack_chk_fail", "__stack_chk_fail_local", "__stack_chk_guard"):
        assert symbol in server_library
        assert symbol in sqlite_library
    assert 'ENV{MSINSIGHT_SERVER_DATABASE_VERSION}' in server_cmake
    assert 'MATCHES "^[0-9]+$"' in server_cmake
    assert "COMPILE_TIME_LENGTH EQUAL 10" in server_cmake


def test_final_metadata_is_copied_after_single_writer_and_before_packaging() -> None:
    source = (Path(__file__).parent / "build.py").read_text(encoding="utf-8")

    writer = source.index("result = write_development_bundle_metadata(context)", source.index("def main"))
    propagation = source.index("result = propagate_final_bundle_metadata(context)", writer)
    packaging = source.index("result = package_products(context)", propagation)
    assert writer < propagation < packaging
    assert "shutil.copyfile(source, destination)" in source
    assert "destination.read_bytes() != source.read_bytes()" in source
    assert "sentinel.unlink()" in source
    assert "staging.rmdir()" in source
    packaged = source.index("('rag_packaged_tests', [Const.PNPM, 'server:build:test'])")
    smoke = source.index("('rag_required_smoke', [Const.PNPM, 'rag:smoke:required'])")
    preview = source.index("('rag_preview_scan', [Const.PNPM, 'rag:preview:scan'])")
    assert packaged < smoke < preview < writer


def load_top_build_module():  # type: ignore[no-untyped-def]
    path = Path(__file__).with_name("build.py")
    spec = importlib.util.spec_from_file_location("insight_top_build", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def git(root: Path, *args: str) -> None:
    subprocess.run(["git", *args], cwd=root, check=True, capture_output=True)


def capture_fixture_snapshot() -> dict[str, object]:
    return {
        "algorithm": "git-worktree-snapshot-v1",
        "head": "a" * 40,
        "headTree": "b" * 40,
        "treeState": "dirty",
        "sha256": "c" * 64,
    }
