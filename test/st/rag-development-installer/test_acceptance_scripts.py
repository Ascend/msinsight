from pathlib import Path


ROOT = Path(__file__).parent


def test_preflight_is_read_only_and_reports_all_blocker_classes() -> None:
    source = (ROOT / "fresh-install-preflight.ps1").read_text(encoding="utf-8")

    for forbidden in (
        "Remove-Item",
        "Remove-ItemProperty",
        "Set-ItemProperty",
        "Stop-Process",
        "Start-Process",
        ".DeleteSubKey",
        "Uninstall.exe",
    ):
        assert forbidden not in source
    for blocker in (
        "valid_install_present",
        "stale_registry",
        "incomplete_product_directory",
        "related_or_unclassified_process",
        "elevation_required",
    ):
        assert blocker in source


def test_mutating_acceptance_requires_approval_sha_and_clean_preflight_first() -> None:
    source = (ROOT / "fresh-install-acceptance.ps1").read_text(encoding="utf-8")

    approval = source.index("if (-not $ApproveElevatedInstall)")
    digest = source.index("Get-FileHash")
    clean_gate = source.index('$preflight.status -ne "clean"')
    install = source.index("Start-Process")
    installed_verify = source.index('rag-cli.mjs") verify')
    required_smoke = source.index("rag-required-smoke.mjs")
    assert approval < digest < clean_gate < install < installed_verify < required_smoke
    assert "& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $preflightScript -Json" in source
