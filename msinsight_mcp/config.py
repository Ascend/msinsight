"""
Configuration management for the MSInsight MCP server.

All settings can be overridden via environment variables with the
MSINSIGHT_ prefix, or by placing a .env file in the mcp/ directory.

Example .env:
    MSINSIGHT_CPP_BACKEND_HOST=192.168.1.100
    MSINSIGHT_CPP_BACKEND_PORT=9000
    MSINSIGHT_MCP_TRANSPORT=sse
    MSINSIGHT_MCP_PORT=8765
"""

from typing import Literal

from pydantic import Field
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    model_config = {"env_prefix": "MSINSIGHT_", "env_file": ".env", "extra": "ignore"}

    # ------------------------------------------------------------------
    # C++ backend connection
    # ------------------------------------------------------------------
    cpp_backend_host: str = Field("localhost", description="C++ backend WebSocket host")
    cpp_backend_port: int = Field(9000, description="C++ backend WebSocket port")
    cpp_reconnect_interval: float = Field(3.0, description="Seconds to wait between reconnect attempts")
    cpp_request_timeout: float = Field(30.0, description="Seconds to wait for a C++ backend response")
    cpp_keepalive_interval: float = Field(
        60.0,
        description=(
            "Seconds of idle time before sending a heartCheck keep-alive to the "
            "C++ backend. Prevents the server's idleTimeout from closing the "
            "WebSocket connection. Set to 0 to disable."
        ),
    )
    cpp_auto_start_binary: str = Field(
        r"", description="Path to the C++ backend executable to auto-start. If empty, auto-start is disabled."
    )
    cpp_log_path: str = Field(
        r"C:\Users\Administrator\.mindstudio_insight", description="Path to the C++ backend log directory."
    )

    # ------------------------------------------------------------------
    # MCP server
    # ------------------------------------------------------------------
    mcp_transport: Literal["stdio", "sse", "websocket"] = Field(
        "stdio",
        description=(
            "Transport for the MCP protocol interface. "
            "'stdio' for Claude Desktop / local CLI; "
            "'sse' for HTTP+SSE (LangChain remote); "
            "'websocket' for raw WebSocket."
        ),
    )
    mcp_host: str = Field("0.0.0.0", description="Bind host (sse / websocket modes)")  # nosec B104
    mcp_port: int = Field(8765, description="Bind port (sse / websocket modes)")

    # ------------------------------------------------------------------
    # Logging
    # ------------------------------------------------------------------
    log_level: str = Field("INFO", description="Logging level (DEBUG/INFO/WARNING/ERROR)")
    log_file: str = Field("mcp_server.log", description="Path to the rotating log file")

    # ------------------------------------------------------------------
    # Derived helpers
    # ------------------------------------------------------------------
    @property
    def cpp_backend_url(self) -> str:
        return f"ws://{self.cpp_backend_host}:{self.cpp_backend_port}/"


settings = Settings()
