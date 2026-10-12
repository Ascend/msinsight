"""Loader module — re-exports tool descriptors and dispatch from tool.py."""

from .tool import DISPATCH, TOOLS

__all__ = ["TOOLS", "DISPATCH"]
