"""Module-level state within a project.

Each module (timeline, cluster, etc.) maintains its own independent dict
of state variables inside a ProjectState.
"""

from __future__ import annotations

from typing import Any


class ModuleState:
    """Mutable dict-like state for a single analysis module."""

    def __init__(self, name: str) -> None:
        self._name = name
        self._data: dict[str, Any] = {}

    @property
    def name(self) -> str:
        return self._name

    def get(self, key: str, default: Any = None) -> Any:
        return self._data.get(key, default)

    def set(self, key: str, value: Any) -> None:
        self._data[key] = value

    def update(self, **kwargs: Any) -> None:
        self._data.update(kwargs)

    def delete(self, key: str) -> None:
        self._data.pop(key, None)

    def clear(self) -> dict[str, Any]:
        old = dict(self._data)
        self._data.clear()
        return old

    def keys(self) -> list[str]:
        return list(self._data.keys())

    def snapshot(self) -> dict[str, Any]:
        return dict(self._data)

    def __repr__(self) -> str:
        return f"ModuleState({self._name}, keys={list(self._data.keys())})"
