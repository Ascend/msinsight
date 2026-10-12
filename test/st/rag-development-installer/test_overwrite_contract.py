from pathlib import Path

import pytest


@pytest.mark.parametrize(
    "failure",
    ["old-removal", "copy"],
)
def test_overwrite_failures_never_commit_registry_or_launch_and_preserve_non_rag(
    tmp_path: Path,
    failure: str,
) -> None:
    installed = tmp_path / "installed"
    rag_data = installed / "resources" / "profiler" / "server" / "insight_web_agent" / "rag-data"
    rag_data.mkdir(parents=True)
    (rag_data / "old-package.zip").write_bytes(b"old")
    for name in ("settings", "logs", "business-data"):
        path = installed / name / "sentinel.txt"
        path.parent.mkdir()
        path.write_text("preserve", encoding="utf-8")
    staged = tmp_path / "staged-rag-data"
    staged.mkdir()
    (staged / "active.json").write_text("new", encoding="utf-8")
    registry = tmp_path / "registry-committed"
    launch = tmp_path / "finish-launch-enabled"

    with pytest.raises(RuntimeError, match=failure):
        simulate_overwrite(
            staged=staged,
            installed_rag=rag_data,
            registry=registry,
            launch=launch,
            failure=failure,
        )

    assert not registry.exists()
    assert not launch.exists()
    assert not (installed / "rag-backup").exists()
    for name in ("settings", "logs", "business-data"):
        assert (installed / name / "sentinel.txt").read_text(encoding="utf-8") == "preserve"
    if failure == "old-removal":
        assert (rag_data / "old-package.zip").is_file()


def simulate_overwrite(
    *,
    staged: Path,
    installed_rag: Path,
    registry: Path,
    launch: Path,
    failure: str,
) -> None:
    if failure == "old-removal":
        raise RuntimeError(failure)
    for child in tuple(installed_rag.iterdir()):
        child.unlink()
    if failure == "copy":
        raise RuntimeError(failure)
    (installed_rag / "active.json").write_bytes((staged / "active.json").read_bytes())
    registry.write_text("committed", encoding="utf-8")
    launch.write_text("enabled", encoding="utf-8")
