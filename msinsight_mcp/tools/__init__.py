"""tools package — aggregates all MCP tool descriptors and dispatch maps."""

from . import cluster, loader, operator, timeline
from .loader import global_tools

# Merged tool list exposed to the MCP server
ALL_TOOLS = loader.TOOLS + global_tools.TOOLS + timeline.TOOLS + cluster.TOOLS + operator.TOOLS

# Merged dispatch map: tool_name -> async handler function
ALL_DISPATCH: dict = {
    **loader.DISPATCH,
    **global_tools.DISPATCH,
    **timeline.DISPATCH,
    **cluster.DISPATCH,
    **operator.DISPATCH,
}

__all__ = ["ALL_TOOLS", "ALL_DISPATCH"]
