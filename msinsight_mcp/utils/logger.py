"""Logging configuration using loguru."""

import sys

from loguru import logger

from config import settings


def setup_logger() -> None:
    """Configure loguru for both stderr and a rotating file."""
    logger.remove()

    fmt = (
        "<green>{time:YYYY-MM-DD HH:mm:ss.SSS}</green> | "
        "<level>{level: <8}</level> | "
        "<cyan>{name}</cyan>:<cyan>{function}</cyan>:<cyan>{line}</cyan> - "
        "<level>{message}</level>"
    )

    logger.add(sys.stderr, level=settings.log_level, format=fmt, colorize=True)
    logger.add(
        settings.log_file,
        level=settings.log_level,
        format=fmt,
        rotation="10 MB",
        retention="7 days",
        compression="gz",
        enqueue=True,
    )
    logger.info("Logger initialised (level={})", settings.log_level)


__all__ = ["logger", "setup_logger"]
