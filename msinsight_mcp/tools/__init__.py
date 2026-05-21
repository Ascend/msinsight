"""tools package — aggregates all MCP tool descriptors and dispatch maps."""

from tools import loader, timeline, cluster

# Merged tool list exposed to the MCP server
ALL_TOOLS = loader.TOOLS + timeline.TOOLS + cluster.TOOLS

# Merged dispatch map: tool_name -> async handler function
ALL_DISPATCH: dict = {
    **loader.DISPATCH,
    **timeline.DISPATCH,
    **cluster.DISPATCH,
}

__all__ = ["ALL_TOOLS", "ALL_DISPATCH"]
