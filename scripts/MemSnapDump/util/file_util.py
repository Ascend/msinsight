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
"""

import os
import pickle  # nosec B403
from pathlib import Path
from typing import Any, Dict, Union


class SafeUnpickler(pickle.Unpickler):
    """
    安全的 Unpickler，只允许反序列化基本 Python 类型。
    防止恶意 pickle 文件执行任意代码。
    """

    ALLOWED_CLASSES = {
        'builtins': {'dict', 'list', 'tuple', 'set', 'str', 'int', 'float', 'bool', 'bytes', 'NoneType'},
    }

    def find_class(self, module: str, name: str):
        if module in self.ALLOWED_CLASSES and name in self.ALLOWED_CLASSES[module]:
            return super().find_class(module, name)

        raise pickle.UnpicklingError(
            f"Unsafe pickle: global '{module}.{name}' is not allowed. "
            f"Only basic Python types are permitted for security."
        )


def load_pickle_to_dict(pickle_file: Path) -> dict:
    """
    从指定路径加载 pickle 文件，并确保其内容为 dict 类型。
    使用 SafeUnpickler 进行安全反序列化，防止恶意代码执行。

    Args:
        pickle_file (Path): pickle 文件路径

    Returns:
        dict: 加载的字典数据

    Raises:
        FileNotFoundError: 文件不存在
        ValueError: 文件内容不是 dict 类型
        pickle.UnpicklingError: 反序列化失败（如文件损坏、非 pickle 格式或包含不安全类型）
    """
    if not pickle_file.is_file():
        raise FileNotFoundError(f"Cannot found pickle file: {pickle_file}")

    try:
        with open(pickle_file, "rb") as f:
            unpickler = SafeUnpickler(f)
            data = unpickler.load()
    except Exception as e:
        raise pickle.UnpicklingError(f"Cannot load pickle file: {pickle_file}, error: {e}") from e

    if not isinstance(data, dict):
        raise ValueError(f"The content of the pickle file is not of type dict, actual type: {type(data).__name__}")

    return data


def save_dict_to_pickle(data: Dict[Any, Any], path: Path, protocol: int = 4) -> None:
    """
    将字典保存为 pickle 文件。

    Args:
        data (dict): 要保存的字典
        path (Path): 保存路径（会自动创建父目录）
        protocol (int): 保存版本

    Raises:
        TypeError: data 不是 dict 类型
        OSError: 文件写入失败（如权限不足、磁盘满等）
    """
    if not isinstance(data, dict):
        raise TypeError(f"Only dict type is supported, but received: {type(data).__name__}")
    path.parent.mkdir(parents=True, exist_ok=True)  # 自动创建父目录

    try:
        with open(path, "wb") as f:
            pickle.dump(data, f, protocol=protocol)
    except OSError as e:
        raise OSError(f"Unable to write to file {path}: {e}") from e


def check_dir_valid(path: Union[str, Path], need_readable: bool = True, need_writable: bool = True) -> bool:
    """
        校验目录是否合法, 默认要求可读可写
    :param path: 路径字符串或pathlib.Path对象
    :param need_readable: 是否要求可读
    :param need_writable: 是否要求可写
    :return: 是否合法
    """
    _path = path
    if not isinstance(path, Path):
        _path = Path(_path)
    if not _path.is_dir():
        return False
    if need_readable and not os.access(_path, os.R_OK):
        return False
    if need_writable and not os.access(_path, os.W_OK):
        return False
    return True


def check_file_valid(path: Union[str, Path], need_readable: bool = True, need_writable: bool = False) -> bool:
    """
        校验文件是否合法, 默认要求可读取，不要求可写
    :param path: 路径字符串或pathlib.Path对象
    :param need_readable: 是否要求可读
    :param need_writable: 是否要求可写
    :return:
    """
    _path = path
    if not isinstance(path, Path):
        _path = Path(_path)
    if not _path.is_file():
        return False
    if need_readable and not os.access(_path, os.R_OK):
        return False
    if need_writable and not os.access(_path, os.W_OK):
        return False
    return True
