# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2026 Huawei Technologies Co.,Ltd.
#
# MindStudio is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------

import ast
import os
import logging

logger = logging.getLogger('PATCH_SOURCE')


def check_ast_if_node_is_main_chunk(node: ast.If):
    result = (
        isinstance(node.test, ast.Compare) and isinstance(node.test.left, ast.Name) and node.test.left.id == '__name__'
    )
    result = result and (len(node.test.ops) == 1 and isinstance(node.test.ops[0], ast.Eq))
    result = result and (
        len(node.test.comparators) == 1
        and isinstance(node.test.comparators[0], ast.Constant)
        and node.test.comparators[0].value == '__main__'
    )
    return result


def extra_replacements(new_source_path: str):
    """从new_source_path中提取所有顶层方法和__main__的源码片段"""
    with open(new_source_path, 'r', encoding='utf-8') as f:
        new_source = f.read()
    try:
        new_tree = ast.parse(new_source, filename=new_source_path)
    except SyntaxError as e:
        logger.error("Syntax error in new_source_path: %s, %s", new_source_path, e)
        return None, new_source

    replacements = {}

    for node in new_tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            src = ast.get_source_segment(new_source, node)
            if src is not None:
                replacements[node.name] = src
        elif isinstance(node, ast.If):
            # 检查是否为 if __name__ == '__main__':
            if check_ast_if_node_is_main_chunk(node):
                src = ast.get_source_segment(new_source, node)
                if src is not None:
                    replacements['__main__'] = src

    return replacements, new_source


def patch_old_source_file(old_source_path: str, replacements: dict):
    """读取old_source.py, 替换同名函数和__main__, 返回新的源码"""
    with open(old_source_path, 'r', encoding='utf-8') as f:
        old_source = f.read()
    try:
        old_tree = ast.parse(old_source, filename=old_source_path)
    except SyntaxError as e:
        logger.error("Syntax error in old_source_path: %s, %s", old_source_path, e)
        return old_source

    # 将需要替换的节点位置记录下来
    replace_ranges = []  # [(start, end, new_code), ...]

    for node in old_tree.body:
        key = None
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.name in replacements:
                key = node.name
        elif isinstance(node, ast.If):
            # 检查是否是__main__块
            if check_ast_if_node_is_main_chunk(node):
                if '__main__' in replacements:
                    key = '__main__'

        if key is not None:
            old_src_seg = ast.get_source_segment(old_source, node)
            if old_src_seg is None:
                continue
            # 在源码中定位该片段的位置( 从lineno 附近开始查找更安全)
            pos = old_source.find(old_src_seg, max(0, old_source.find('\n', 0, node.lineno * 2)))
            if pos == -1:
                # fallback: 全局查找
                pos = old_source.find(old_src_seg)
            if pos == -1:
                logger.warning("Failed to find segment[%s] in %s", key, old_source_path)
                continue
            end = pos + len(old_src_seg)
            replace_ranges.append((pos, end, replacements[key]))

    # 从后往前替换，避免偏移出现错乱
    new_source = old_source
    for start, end, new_src_seg in sorted(replace_ranges, key=lambda x: x[0], reverse=True):
        new_source = new_source[:start] + new_src_seg + new_source[end:]

    return new_source


def patch_source(old_source_path: str, new_source_path: str, output_path: str = None) -> bool:
    """
        用于将old_source_path所指定的旧py源文件中的方法(含if main块) 替换为 new_source_path 文件中的同名方法实现
        输出到output_path
    :param old_source_path: 待替换源文件.py路径
    :param new_source_path: 待用于替换的新实现.py路径
    :param output_path: 输出路径，缺省将直接覆盖old_source_path
    :return: 是否成功
    """
    if output_path is None:
        output_path = old_source_path  # ！！！直接替换原文件

    if not os.path.exists(old_source_path):
        logger.error("The old source file not found: %s", old_source_path)
        return False
    if not os.path.exists(new_source_path):
        logger.error("The new source file not found: %s", new_source_path)
        return False

    replacements, _ = extra_replacements(new_source_path)
    new_source = patch_old_source_file(old_source_path, replacements)

    # 写入时保留原始换行风格
    with open(output_path, 'w', encoding='utf-8', newline='') as f:
        f.write(new_source)

    logger.info("Successfully patched functions from new source path %s into %s", new_source_path, output_path)
    return True
