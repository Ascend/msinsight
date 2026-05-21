import subprocess  # nosec B404
import os
import atexit
import time
import threading
from typing import Optional

from config import settings
from utils.logger import logger

_cpp_process: Optional[subprocess.Popen] = None


def start_profiler_server_if_needed() -> None:
    global _cpp_process
    if not settings.cpp_auto_start_binary:
        return

    logger.info("Attempting to auto-start C++ backend from: {}", settings.cpp_auto_start_binary)
    if not os.path.exists(settings.cpp_auto_start_binary):
        logger.error("C++ backend binary not found at: {}", settings.cpp_auto_start_binary)
        return

    try:
        # Construct arguments if needed according to your C++ backend
        args = [
            settings.cpp_auto_start_binary,
            f"--wsPort={settings.cpp_backend_port}",
            f"--logPath={settings.cpp_log_path}",
        ]

        def _log_stream(stream, log_func):
            for line in iter(stream.readline, b''):
                if line:
                    log_func("C++ Backend: {}", line.decode('utf-8', errors='replace').rstrip())

        # Start the process in the background, piping stdout/stderr.
        # pylint: disable=consider-using-with
        _cpp_process = subprocess.Popen(  # nosec B603
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            cwd=os.path.dirname(settings.cpp_auto_start_binary) or None,
        )

        if _cpp_process.stdout:
            threading.Thread(target=_log_stream, args=(_cpp_process.stdout, logger.info), daemon=True).start()
        if _cpp_process.stderr:
            threading.Thread(target=_log_stream, args=(_cpp_process.stderr, logger.error), daemon=True).start()

        logger.info("C++ backend process started with PID: {}", _cpp_process.pid)

        # Give it a moment to bind to the port
        time.sleep(1)

        rc = _cpp_process.poll()
        if rc is not None:
            if rc in (-1073741515, 3221225781):  # 0xC0000135
                logger.error(
                    "C++ backend exited immediately with code 0xC0000135 (-1073741515). "
                    "This indicates a missing DLL (usually MinGW runtime like libgcc_s_seh-1.dll, "
                    "libstdc++-6.dll, or libwinpthread-1.dll) since the OS loader failed before main(). "
                    "Ensure your MinGW 'bin' folder is in your system PATH or passed in the env."
                )
            else:
                logger.error(f"C++ backend exited immediately with code: {rc}")

    except Exception as exc:
        logger.error("Failed to auto-start C++ backend: {}", exc)


def cleanup_profiler_server() -> None:
    if _cpp_process and _cpp_process.poll() is None:
        logger.info("Killing auto-started C++ backend process (PID {})", _cpp_process.pid)
        _cpp_process.terminate()
        try:
            _cpp_process.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            _cpp_process.kill()


atexit.register(cleanup_profiler_server)
