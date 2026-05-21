"""Project-level state.

A ProjectState represents a single data import within a session.
It owns:
  - rank_list: list of imported rank/card info from import result
  - is_cluster: whether this is a cluster-level import
  - cluster_path: single cluster path for this project
  - a collection of ModuleState instances (timeline, cluster, etc.)
"""

from __future__ import annotations

from typing import Any, Optional

from .module import ModuleState


class ProjectState:
    """State for a single imported project/trace within a session."""

    def __init__(self, project_name: str, file_path: str) -> None:
        self._project_name = project_name
        self._file_path = file_path
        self._rank_list: list[dict[str, Any]] = []
        self._is_cluster: bool = False
        self._cluster_path: Optional[str] = None
        self._modules: dict[str, ModuleState] = {}

    @property
    def project_name(self) -> str:
        return self._project_name

    @property
    def file_path(self) -> str:
        return self._file_path

    # -- Import metadata ------------------------------------------------

    def set_import_result(self, result: dict) -> None:
        self._rank_list = result.get("result", [])
        self._is_cluster = result.get("isCluster", False)

    @property
    def rank_list(self) -> list[dict[str, Any]]:
        return list(self._rank_list)

    @property
    def is_cluster(self) -> bool:
        return self._is_cluster

    @property
    def cluster_path(self) -> Optional[str]:
        return self._cluster_path

    def set_cluster_path(self, path: str) -> None:
        self._cluster_path = path

    # -- Module state ---------------------------------------------------

    def get_module(self, name: str) -> ModuleState:
        if name not in self._modules:
            self._modules[name] = ModuleState(name)
        return self._modules[name]

    def list_modules(self) -> list[str]:
        return list(self._modules.keys())

    def reset(self) -> None:
        for mod in self._modules.values():
            mod.clear()

    def snapshot(self) -> dict[str, Any]:
        return {
            "project_name": self._project_name,
            "file_path": self._file_path,
            "rank_list": self._rank_list,
            "is_cluster": self._is_cluster,
            "cluster_path": self._cluster_path,
            "modules": {n: m.snapshot() for n, m in self._modules.items()},
        }

    def __repr__(self) -> str:
        return (
            f"ProjectState(name={self._project_name!r}, "
            f"file={self._file_path!r}, "
            f"ranks={len(self._rank_list)}, "
            f"cluster={self._is_cluster}, "
            f"modules={list(self._modules.keys())})"
        )
