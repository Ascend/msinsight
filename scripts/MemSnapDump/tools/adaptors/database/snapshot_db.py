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

from util.sqlite_meta import SqliteColumn, SqliteTable, SqliteDB
from .defs import EventFieldDefs, BlockFieldDefs

TRACE_ENTRY_ACTION_VALUE_MAP = {
    'segment_map': 0,
    'segment_unmap': 1,
    'segment_alloc': 2,
    'segment_free': 3,
    'alloc': 4,
    'free_requested': 5,
    'free_completed': 6,
    'workspace_snapshot': 7,
}

BLOCK_STATE_VALUE_MAP = {'inactive': -1, 'active_allocated': 1, 'active_pending_free': 0}

_TRACE_ENTRY_TABLE_COLUMNS = [
    SqliteColumn(name=EventFieldDefs.ID, data_type=int, primary_key=True),
    SqliteColumn(name=EventFieldDefs.ACTION, data_type=int, value_map=TRACE_ENTRY_ACTION_VALUE_MAP.copy()),
    SqliteColumn(name=EventFieldDefs.ADDR, data_type=int),
    SqliteColumn(name=EventFieldDefs.SIZE, data_type=int),
    SqliteColumn(name=EventFieldDefs.STREAM, data_type=int),
    SqliteColumn(name=EventFieldDefs.ALLOCATED, data_type=int),
    SqliteColumn(name=EventFieldDefs.ACTIVE, data_type=int),
    SqliteColumn(name=EventFieldDefs.RESERVED, data_type=int),
    SqliteColumn(name=EventFieldDefs.CALLSTACK),
]

_BLOCK_TABLE_COLUMNS = [
    SqliteColumn(name=BlockFieldDefs.ID, data_type=int, primary_key=True),
    SqliteColumn(name=BlockFieldDefs.ADDR, data_type=int),
    SqliteColumn(name=BlockFieldDefs.SIZE, data_type=int),
    SqliteColumn(name=BlockFieldDefs.REQUESTED_SIZE, data_type=int),
    SqliteColumn(name=BlockFieldDefs.STATE, default=99, data_type=int, value_map=BLOCK_STATE_VALUE_MAP.copy()),
    SqliteColumn(name=BlockFieldDefs.ALLOC_EVENT_ID, data_type=int),
    SqliteColumn(name=BlockFieldDefs.FREE_EVENT_ID, data_type=int),
]


class SnapshotDb(SqliteDB):
    TRACE_ENTRY_TABLE_NAME = "trace_entry"
    BLOCK_TABLE_NAME = "block"

    def __init__(self, path: str):
        super().__init__(path, auto_create=True, with_dictionary_table=True)
        # Import builds a replaceable database, so favor write throughput over crash recovery.
        self.conn.execute("PRAGMA journal_mode = MEMORY")
        self.conn.execute("PRAGMA synchronous = OFF")
        self.conn.execute("PRAGMA cache_size = -65536")
        # 清理旧版本表格
        self._clear_old_tables()

    def create_trace_entry_table(self, device: int = 0):
        self.create_table(
            SqliteTable(self.get_trace_table_name_by_device(device), _TRACE_ENTRY_TABLE_COLUMNS), delete_if_exists=True
        )
        table = self.get_trace_entry_table(device)
        for column in (EventFieldDefs.ALLOCATED, EventFieldDefs.ACTIVE, EventFieldDefs.RESERVED):
            table.create_index(self.conn, column)

    def create_block_table(self, device: int = 0):
        self.create_table(
            SqliteTable(self.get_block_table_name_by_device(device), _BLOCK_TABLE_COLUMNS), delete_if_exists=True
        )
        table = self.get_block_table(device)
        for column in (BlockFieldDefs.ALLOC_EVENT_ID, BlockFieldDefs.FREE_EVENT_ID, BlockFieldDefs.SIZE):
            table.create_index(self.conn, column)

    def get_trace_entry_table(self, device: int = 0):
        return self.get_table_by_name(self.get_trace_table_name_by_device(device))

    def get_block_table(self, device: int = 0):
        return self.get_table_by_name(self.get_block_table_name_by_device(device))

    def _clear_old_tables(self):
        """
        旧版本中创建的db不带device前缀，如果通过旧db打开，需要清理旧表
        """
        self.delete_table(self.TRACE_ENTRY_TABLE_NAME)
        self.delete_table(self.BLOCK_TABLE_NAME)

    @staticmethod
    def get_block_table_name_by_device(device: int = 0):
        return f"{SnapshotDb.BLOCK_TABLE_NAME}_{device}"

    @staticmethod
    def get_trace_table_name_by_device(device: int = 0):
        return f"{SnapshotDb.TRACE_ENTRY_TABLE_NAME}_{device}"
