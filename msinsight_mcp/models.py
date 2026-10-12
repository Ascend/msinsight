"""
Pydantic data models for the MSInsight C++ backend WebSocket protocol.

Protocol overview
-----------------
Client → Server  : CppRequest  (type="request")
Server → Client  : CppResponse (type="response")
Server → Client  : CppEvent    (type="event", unsolicited push)

All messages are plain JSON text frames (no Content-Length framing on
the wire; the C++ server adds that framing only internally).
"""

from typing import Any, Dict, Literal, Optional

from pydantic import BaseModel, Field


class CppRequest(BaseModel):
    """Request message sent from the MCP bridge to the C++ backend."""

    type: Literal["request"] = "request"
    id: int = Field(..., description="Unique, monotonically increasing request identifier")
    command: str = Field(..., description="Command name, e.g. 'files/get'")
    moduleName: str = Field(..., description="Module routing key, e.g. 'global'")
    projectName: Optional[str] = Field(None, description="Project context (if applicable)")
    fileId: Optional[str] = Field(None, description="File context (if applicable)")
    resultCallbackId: Optional[int] = Field(None, description="Async callback identifier")
    params: Dict[str, Any] = Field(default_factory=dict, description="Command parameters")


class ErrorInfo(BaseModel):
    """Error detail embedded in a failed CppResponse."""

    code: int
    message: str


class CppResponse(BaseModel):
    """Response received from the C++ backend."""

    type: str
    id: int
    requestId: int = Field(..., description="Echoes the originating request id")
    result: bool = Field(..., description="True = success, False = error")
    command: str
    moduleName: str
    resultCallbackId: Optional[int] = None
    error: Optional[ErrorInfo] = None
    body: Optional[Any] = Field(None, description="Command-specific result payload")


class CppEvent(BaseModel):
    """Unsolicited server-pushed event from the C++ backend."""

    type: Literal["event"] = "event"
    id: int
    event: str = Field(..., description="Event name, e.g. 'parse/success'")
    moduleName: str
    result: bool
    resultCallbackId: Optional[int] = None
    body: Optional[Any] = None


class McpToolResult(BaseModel):
    """Normalised tool result returned to the MCP caller."""

    success: bool
    data: Optional[Any] = None
    error: Optional[str] = None
