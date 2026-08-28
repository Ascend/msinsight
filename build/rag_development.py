"""Strict development-RAG preflight, source identity, and metadata helpers."""

from __future__ import annotations

import hashlib
import json
import os
import re
import stat
import subprocess
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

VERSION_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)-rag-dev\.([1-9]\d*)$")
KB_VERSION_RE = re.compile(r"^\d{2}\.[012]\.[1-9]\d*$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


@dataclass(frozen=True)
class DevelopmentVersion:
    product: str
    pe_numeric: str

    @classmethod
    def parse(cls, value: str) -> "DevelopmentVersion":
        match = VERSION_RE.fullmatch(value)
        if match is None:
            raise ValueError("development version must use MAJOR.MINOR.PATCH-rag-dev.SERIAL")
        major, minor, patch, serial = match.groups()
        return cls(value, f"{int(major)}.{int(minor)}.{int(patch)}.{int(serial)}")


@dataclass(frozen=True)
class RagArguments:
    version: DevelopmentVersion
    mode: str
    pack: Path
    sidecar: Path
    model_dir: Path


@dataclass(frozen=True)
class RagInputFacts:
    package_sha256: str
    package_size: int
    kb_version: str
    source_set_id: str
    source_set_sha256: str
    model_dir: Path


@dataclass(frozen=True)
class SourceSnapshot:
    head: str
    head_tree: str
    tree_state: str
    sha256: str
    records: tuple[dict[str, str], ...]

    def metadata(self) -> dict[str, object]:
        return {
            "algorithm": "git-worktree-snapshot-v1",
            "head": self.head,
            "headTree": self.head_tree,
            "treeState": self.tree_state,
            "sha256": self.sha256,
        }


def validate_rag_arguments(
    *,
    build_version: str,
    mode: str | None,
    pack: Path | None,
    sidecar: Path | None,
    model_dir: Path | None,
) -> RagArguments | None:
    values = (mode, pack, sidecar, model_dir)
    if not any(value is not None for value in values):
        return None
    if mode != "development" or pack is None or sidecar is None or model_dir is None:
        raise ValueError("development RAG options must be complete and mode must be development")
    return RagArguments(
        version=DevelopmentVersion.parse(build_version),
        mode=mode,
        pack=Path(pack),
        sidecar=Path(sidecar),
        model_dir=Path(model_dir),
    )


def preflight_rag_inputs(pack: Path, sidecar: Path, model_dir: Path) -> RagInputFacts:
    archive = _regular_file(pack, "Package")
    digest = _sha256_file(archive)
    expected_sidecar = f"{digest}  knowledge-pack-v4.zip\n".encode("ascii")
    sidecar_path = _regular_file(sidecar, "sidecar")
    if sidecar_path.name != "knowledge-pack-v4.zip.sha256" or sidecar_path.read_bytes() != expected_sidecar:
        raise ValueError("Package sidecar is noncanonical or divergent")
    if archive.name != "knowledge-pack-v4.zip":
        raise ValueError("Package basename must be knowledge-pack-v4.zip")
    model = Path(model_dir)
    if model.is_symlink() or not model.is_dir():
        raise ValueError("model directory must be a regular directory")
    _regular_file(model / "model-manifest.json", "model manifest")
    try:
        with zipfile.ZipFile(archive) as package:
            manifest = json.loads(package.read("manifest.json"))
    except (OSError, KeyError, json.JSONDecodeError, zipfile.BadZipFile) as error:
        raise ValueError("unable to read Package manifest during external preflight") from error
    version = manifest.get("kbVersion")
    source_set = manifest.get("sourceSet")
    if not isinstance(version, str) or KB_VERSION_RE.fullmatch(version) is None:
        raise ValueError("Package kbVersion is invalid")
    if not isinstance(source_set, dict):
        raise ValueError("Package sourceSet identity is absent")
    source_set_id = source_set.get("sourceSetId")
    source_set_sha256 = source_set.get("sha256")
    if (
        not isinstance(source_set_id, str)
        or not re.fullmatch(r"ss_[0-9a-f]{64}", source_set_id)
        or not isinstance(source_set_sha256, str)
        or SHA256_RE.fullmatch(source_set_sha256) is None
    ):
        raise ValueError("Package sourceSet identity is invalid")
    return RagInputFacts(
        package_sha256=digest,
        package_size=archive.stat().st_size,
        kb_version=version,
        source_set_id=source_set_id,
        source_set_sha256=source_set_sha256,
        model_dir=model,
    )


def capture_source_snapshot(root: Path) -> SourceSnapshot:
    repository = Path(root).resolve()
    head = _git(repository, "rev-parse", "HEAD").strip()
    head_tree = _git(repository, "rev-parse", "HEAD^{tree}").strip()
    if not re.fullmatch(r"[0-9a-f]{40,64}", head) or not re.fullmatch(r"[0-9a-f]{40,64}", head_tree):
        raise ValueError("Git returned a noncanonical source identity")
    staged = _staged_records(repository)
    records: list[dict[str, str]] = []
    working_changed = _git_returncode(repository, "diff", "--quiet") != 0
    for path, (mode, oid) in staged.items():
        target = repository / Path(path)
        if not os.path.lexists(target):
            records.append(
                {
                    "kind": "tracked",
                    "path": path,
                    "mode": mode,
                    "stagedOid": oid,
                    "workingState": "deleted",
                }
            )
            continue
        content, working_mode = _working_file(target, mode)
        digest = hashlib.sha256(content).hexdigest()
        records.append(
            {
                "kind": "tracked",
                "path": path,
                "mode": working_mode,
                "stagedOid": oid,
                "contentSha256": digest,
            }
        )
    untracked = _nul_paths(_git_bytes(repository, "ls-files", "--others", "--exclude-standard", "-z"))
    for path in untracked:
        target = repository / Path(path)
        content, mode = _working_file(target, "100644")
        records.append(
            {
                "kind": "untracked",
                "path": path,
                "mode": mode,
                "contentSha256": hashlib.sha256(content).hexdigest(),
            }
        )
    records.sort(key=lambda record: record["path"])
    index_changed = _git_returncode(repository, "diff", "--cached", "--quiet") != 0
    tree_state = "dirty" if working_changed or index_changed or untracked else "clean"
    payload = {
        "algorithm": "git-worktree-snapshot-v1",
        "head": head,
        "headTree": head_tree,
        "treeState": tree_state,
        "records": records,
    }
    return SourceSnapshot(
        head=head,
        head_tree=head_tree,
        tree_state=tree_state,
        sha256=hashlib.sha256(_canonical_json(payload)).hexdigest(),
        records=tuple(records),
    )


class VersionFileTransaction:
    def __init__(self, paths: Iterable[Path]) -> None:
        self.paths = tuple(Path(path) for path in paths)
        self._original: dict[Path, bytes | None] = {}

    def __enter__(self) -> "VersionFileTransaction":
        self._original = {
            path: path.read_bytes() if path.exists() else None
            for path in self.paths
        }
        return self

    def __exit__(self, *_args: object) -> None:
        for path, content in self._original.items():
            if content is None:
                if path.exists():
                    path.unlink()
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)


def write_internal_metadata(destination: Path, metadata: dict[str, object]) -> None:
    content = _canonical_json(metadata)
    _reject_unsafe_metadata(content)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("xb") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())


def select_exact_installer(directory: Path, version: str, os_tag: str) -> Path:
    expected = Path(directory) / f"MindStudio-Insight_{version}_{os_tag}.exe"
    if expected.is_symlink() or not expected.is_file():
        raise FileNotFoundError("exact installer output is absent")
    return expected


def write_installer_evidence(
    installer: Path,
    metadata: dict[str, object],
) -> tuple[Path, Path]:
    path = _regular_file(installer, "installer")
    content = path.read_bytes()
    digest = hashlib.sha256(content).hexdigest()
    sidecar = path.with_name(f"{path.name}.sha256")
    companion = path.with_name(f"{path.stem}.build.json")
    with sidecar.open("xb") as stream:
        stream.write(f"{digest}  {path.name}\n".encode("ascii"))
    payload = {
        **metadata,
        "installer": {
            "fileName": path.name,
            "sha256": digest,
            "sizeBytes": len(content),
        },
    }
    try:
        write_internal_metadata(companion, payload)
    except Exception:
        sidecar.unlink(missing_ok=True)
        raise
    return sidecar, companion


def _staged_records(root: Path) -> dict[str, tuple[str, str]]:
    output = _git_bytes(root, "ls-files", "--stage", "-z")
    records: dict[str, tuple[str, str]] = {}
    for row in _nul_paths(output):
        facts, path = row.split("\t", maxsplit=1)
        mode, oid, stage = facts.split(" ")
        if stage != "0":
            raise ValueError("unmerged index entries are not buildable")
        records[_safe_git_path(path)] = (mode, oid)
    return records


def _working_file(path: Path, index_mode: str) -> tuple[bytes, str]:
    facts = os.lstat(path)
    if stat.S_ISLNK(facts.st_mode):
        return os.readlink(path).encode("utf-8"), "120000"
    if not stat.S_ISREG(facts.st_mode):
        raise ValueError("source snapshot contains a nonregular path")
    executable = index_mode == "100755" or (
        os.name != "nt" and bool(facts.st_mode & stat.S_IXUSR)
    )
    mode = "100755" if executable else "100644"
    return path.read_bytes(), mode


def _safe_git_path(value: str) -> str:
    normalized = value.replace("\\", "/")
    path = Path(normalized)
    if path.is_absolute() or not normalized or any(part in {"", ".", ".."} for part in path.parts):
        raise ValueError("Git source path is unsafe")
    return normalized


def _nul_paths(value: bytes) -> tuple[str, ...]:
    try:
        return tuple(
            _safe_git_path(item.decode("utf-8"))
            for item in value.split(b"\0")
            if item
        )
    except UnicodeDecodeError as error:
        raise ValueError("Git source path is not UTF-8") from error


def _canonical_json(value: object) -> bytes:
    return (
        json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")
        + b"\n"
    )


def _reject_unsafe_metadata(content: bytes) -> None:
    text = content.decode("utf-8").casefold().replace("\\", "/")
    forbidden = (str(Path.home()).casefold().replace("\\", "/"), "api_key", "credential", "question", "knowledgeText".casefold())
    if any(value and value in text for value in forbidden):
        raise ValueError("development metadata contains a forbidden local or secret field")


def _regular_file(path: Path, label: str) -> Path:
    value = Path(path)
    if value.is_symlink() or not value.is_file():
        raise ValueError(f"{label} must be a regular file")
    return value


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _git(root: Path, *args: str) -> str:
    return _git_bytes(root, *args).decode("ascii")


def _git_bytes(root: Path, *args: str) -> bytes:
    result = subprocess.run(
        ["git", *args],
        cwd=root,
        check=False,
        capture_output=True,
    )
    if result.returncode != 0:
        raise ValueError("unable to capture Git source identity")
    return result.stdout


def _git_returncode(root: Path, *args: str) -> int:
    return subprocess.run(
        ["git", *args],
        cwd=root,
        check=False,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ).returncode
