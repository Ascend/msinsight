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

有序帧旁表，与 pt-snap-cli 的 ptSnapOrderedFrames v2 扩展保持一致。
"""

import json
import sqlite3

from base import Frame, TraceEntry


def _fits_frame_depth(value, remaining=127):
    # The outer stack array consumes one level of the 128-level JSON budget.
    if remaining < 0:
        return False
    if isinstance(value, dict):
        children = value.values()
    elif isinstance(value, list):
        children = value
    else:
        return True
    for child in children:
        if not _fits_frame_depth(child, remaining - 1):
            return False
    return True


class OrderedFrameWriter:
    def __init__(self, connection: sqlite3.Connection):
        self.connection = connection
        connection.executescript(
            "CREATE TABLE pt_snap_frame_value (id INTEGER PRIMARY KEY, frameJson TEXT);"
            "CREATE TABLE pt_snap_stack (stackId INTEGER PRIMARY KEY, frameCount INTEGER);"
            "CREATE TABLE pt_snap_stack_frame (stackId INTEGER, frameIndex INTEGER, frameId INTEGER,"
            " PRIMARY KEY(stackId,frameIndex));"
            "CREATE TABLE pt_snap_event_stack (eventId INTEGER PRIMARY KEY, stackId INTEGER);"
        )
        self.frames = {}
        self.stacks = {}
        self.containers = {}

    def insert(self, event: TraceEntry) -> None:
        if event.idx is None or event.idx < 0:
            return
        frames = event._raw_frames if event._raw_frames is not None else event.frames
        key = tuple(map(id, frames))
        cached = self.containers.get(key)
        if cached is not None:
            stack_id = cached[1]
        else:
            try:
                encoded = [
                    json.dumps(
                        frame.to_dict() if isinstance(frame, Frame) else frame,
                        sort_keys=True,
                        ensure_ascii=True,
                        separators=(",", ":"),
                        allow_nan=False,
                    )
                    for frame in frames
                ]
                # 按读取端看到的 JSON 形状校验，包含 tuple 转为 list 的情况。
                # 必须在写入任何旁表前检查整条栈，避免留下部分可选证据。
                if any(not _fits_frame_depth(json.loads(value)) for value in encoded):
                    return
            except (ValueError, TypeError, RecursionError, OverflowError):
                # 可选证据无法编码时保留原有解析行为，查询端明确显示 missingFrame。
                return
            ids = []
            for value in encoded:
                if value not in self.frames:
                    frame_id = len(self.frames)
                    self.frames[value] = frame_id
                    self.connection.execute("INSERT INTO pt_snap_frame_value VALUES (?,?)", (frame_id, value))
                ids.append(self.frames[value])
            stack = tuple(ids)
            if stack not in self.stacks:
                stack_id = len(self.stacks)
                self.stacks[stack] = stack_id
                self.connection.execute("INSERT INTO pt_snap_stack VALUES (?,?)", (stack_id, len(stack)))
                self.connection.executemany(
                    "INSERT INTO pt_snap_stack_frame VALUES (?,?,?)",
                    [(stack_id, index, frame_id) for index, frame_id in enumerate(stack)],
                )
            stack_id = self.stacks[stack]
            # 保留原容器，避免对象回收后 id 被复用；不复制帧字典。
            self.containers[key] = (frames, stack_id)
        self.connection.execute("INSERT INTO pt_snap_event_stack VALUES (?,?)", (event.idx, stack_id))
