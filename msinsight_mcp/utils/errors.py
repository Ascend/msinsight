"""Custom exception hierarchy for the MSInsight MCP bridge."""

from models import ErrorInfo

# --------------------------------------------------------------------
# Known C++ backend error codes
# --------------------------------------------------------------------
_CPP_ERROR_NAMES: dict[int, str] = {
    4003: "REQUEST_PARAMS_ERROR",
    9999: "UNKNOWN_ERROR",
}


def get_error_name(code: int) -> str:
    """Return a human-readable name for a C++ error code."""
    return _CPP_ERROR_NAMES.get(code, f"ERROR_{code}")


# --------------------------------------------------------------------
# Exception classes
# --------------------------------------------------------------------


class CppBackendError(Exception):
    """Raised when the C++ backend returns result=false."""

    def __init__(self, code: int, message: str) -> None:
        self.code = code
        self.message = message
        super().__init__(f"[{get_error_name(code)}] {message}")

    @classmethod
    def from_error_info(cls, error: ErrorInfo) -> "CppBackendError":
        return cls(error.code, error.message)


class BackendConnectionError(Exception):
    """Raised when the WebSocket connection to the C++ backend fails."""


class RequestTimeoutError(Exception):
    """Raised when the C++ backend does not respond within the configured timeout."""


class NotConnectedError(Exception):
    """Raised when the cpp client is used before being initialised."""
