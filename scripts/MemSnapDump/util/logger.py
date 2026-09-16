"""
-------------------------------------------------------------------------
This file is part of the MindStudio project.
Copyright (c) 2026 Huawei Technologies Co.,Ltd.

MindStudio is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:

         http://license.coscl.org.cn/MulanPSL2

THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
-------------------------------------------------------------------------

日志工具模块

提供日志记录器的创建和配置功能。
"""

import logging
from pathlib import Path
from typing import Dict, Optional
from .file_util import check_dir_valid

_global_log_file: Optional[str] = None
_global_file_handler: Optional[logging.FileHandler] = None


def set_global_log_file(log_file: str) -> None:
    """
    设置全局日志文件路径，所有 logger 都会输出到该文件。

    包括已创建的 logger 和后续创建的 logger。

    Args:
        log_file: 日志文件路径

    Raises:
        OSError: 目录不存在或不可写
    """
    global _global_log_file, _global_file_handler

    log_path = Path(log_file)
    parent_dir = log_path.parent

    if not parent_dir.exists():
        raise OSError(f"Log directory '{parent_dir}' does not exist.")

    if not parent_dir.is_dir() or not check_dir_valid(parent_dir):
        raise OSError(f"Log directory '{parent_dir}' is not writable or does not exist.")

    _global_log_file = str(log_path.absolute())

    if _global_file_handler is not None:
        _global_file_handler.close()

    formatter = logging.Formatter(
        fmt='{asctime} [{levelname:^6}][ {name:^12} ]: {message}', datefmt='%Y-%m-%d %H:%M:%S', style='{'
    )

    _global_file_handler = logging.FileHandler(_global_log_file, mode='w', encoding='utf-8')
    _global_file_handler.setLevel(logging.DEBUG)
    _global_file_handler.setFormatter(formatter)

    _attach_file_handler_to_existing_loggers()


_original_levels: Dict[str, int] = {}


def suppress_logs():
    """Suppress INFO level logs during tests"""
    for name in list(logging.Logger.manager.loggerDict.keys()):
        logger = logging.getLogger(name)
        _original_levels[name] = logger.level
        logger.setLevel(logging.WARNING)


def restore_logs():
    """Restore original log levels after tests"""
    for name, level in _original_levels.items():
        logging.getLogger(name).setLevel(level)
    _original_levels.clear()


def _attach_file_handler_to_existing_loggers() -> None:
    """为所有已存在的 logger 添加文件处理器"""
    if _global_file_handler is None:
        return

    manager = logging.Logger.manager
    logger_dict = manager.loggerDict

    for name in list(logger_dict.keys()):
        logger = logging.getLogger(name)
        if _global_file_handler not in logger.handlers:
            logger.addHandler(_global_file_handler)


def get_logger(name: str, level: int = logging.INFO) -> logging.Logger:
    """
    创建并返回一个配置好的 Logger 实例。

    该函数会创建一个带有控制台输出的日志记录器，使用统一的格式化样式。
    如果 logger 已存在 handlers，会先清除再添加新的 handler。
    如果设置了全局日志文件，同时会输出到文件。

    Args:
        name: logger 名称，通常使用 __name__ 或模块名
        level: 日志级别，默认为 logging.INFO

    Returns:
        logging.Logger: 配置好的 Logger 对象

    Examples:
        >>> logger = get_logger(__name__)
        >>> logger.info("This is an info message")
        2024-01-01 12:00:00 [ INFO ][ __main__ ]: This is an info message

        >>> logger = get_logger("my_module", level=logging.DEBUG)
        >>> logger.debug("Debug message")
    """
    logger = logging.getLogger(name)
    logger.setLevel(level)

    # 避免重复添加handler
    if logger.hasHandlers():
        logger.handlers.clear()

    formatter = logging.Formatter(
        fmt='{asctime} [{levelname:^6}][ {name:^12} ]: {message}', datefmt='%Y-%m-%d %H:%M:%S', style='{'
    )

    console_handler = logging.StreamHandler()
    console_handler.setLevel(level)
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)

    if _global_file_handler is not None:
        logger.addHandler(_global_file_handler)

    return logger
