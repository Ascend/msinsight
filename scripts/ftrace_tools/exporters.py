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
import json
import logging
import sqlite3
from abc import ABC, abstractmethod
from typing import Dict, List, Any


class ExportStrategy(ABC):
    @abstractmethod
    def export(self, data: List[Dict[str, Any]], output_path: str):
        pass


class JsonExport(ExportStrategy):
    def export(self, data: List[Dict[str, Any]], output_path: str):
        try:
            with open(output_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            logging.info(f"Exported {len(data)} events to JSON: {output_path}")
        except IOError as e:
            logging.error(f"Failed to write JSON file {output_path}: {e}")
            raise
        except Exception as e:
            logging.error(f"Unexpected error exporting JSON: {e}")
            raise


class DbExport(ExportStrategy):
    def export(self, data: List[Dict[str, Any]], output_path: str):
        conn = None
        try:
            conn = sqlite3.connect(output_path)
            cursor = conn.cursor()
            
            self._create_process_table(cursor)
            self._insert_process_data(cursor)
            
            self._create_thread_table(cursor)
            self._insert_thread_data(cursor, data)
            
            self._create_slice_table(cursor)
            self._insert_slice_data(cursor, data)
            
            self._create_counter_table(cursor)
            
            conn.commit()
            logging.info(f"Exported to SQLite: {output_path}")
        except sqlite3.Error as e:
            logging.error(f"SQLite error occurred during export: {e}")
            if conn:
                conn.rollback()
            raise
        except Exception as e:
            logging.error(f"Unexpected error during export: {e}")
            if conn:
                conn.rollback()
            raise
        finally:
            if conn:
                conn.close()
    
    def _create_process_table(self, cursor):
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS process (
                pid TEXT PRIMARY KEY,
                process_name TEXT,
                label TEXT,
                process_sort_index INTEGER,
                parentPid TEXT DEFAULT '0'
            )
        ''')
    
    def _insert_process_data(self, cursor):
        process_data = [
            ('Process Scheduling', 'Process Scheduling', None, None, '0'),
            ('CPU Scheduling', 'CPU Scheduling', None, None, '0')
        ]
        cursor.executemany(
            'INSERT OR REPLACE INTO process (pid, process_name, label, process_sort_index, parentPid) VALUES (?, ?, ?, ?, ?)',
            process_data
        )
    
    def _create_thread_table(self, cursor):
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS thread (
                track_id INTEGER PRIMARY KEY,
                tid TEXT,
                pid TEXT,
                thread_name TEXT,
                thread_sort_index INTEGER
            )
        ''')
    
    def _insert_thread_data(self, cursor, data: List[Dict[str, Any]], batch_size=10000):
        cpu_scheduling_threads = set()
        process_scheduling_threads = set()
        
        for event in data:
            if event.get('ph') != 'X':
                continue
            
            pid = event.get('pid', '')
            tid = event.get('tid', '')
            
            if pid == 'CPU Scheduling':
                cpu_scheduling_threads.add(tid)
            elif pid == 'Process Scheduling':
                process_scheduling_threads.add(tid)
        
        thread_data = []
        for tid in sorted(cpu_scheduling_threads):
            thread_data.append((tid, 'CPU Scheduling', tid, 0))
        for tid in sorted(process_scheduling_threads):
            thread_data.append((tid, 'Process Scheduling', tid, 0))
        
        for i in range(0, len(thread_data), batch_size):
            cursor.executemany(
                'INSERT INTO thread (tid, pid, thread_name, thread_sort_index) VALUES (?, ?, ?, ?)',
                thread_data[i:i+batch_size]
            )
    
    def _create_slice_table(self, cursor):
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS slice (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp INTEGER,
                duration INTEGER,
                name TEXT,
                depth INTEGER,
                track_id INTEGER,
                cat TEXT,
                args TEXT,
                cname TEXT,
                end_time INTEGER,
                flag_id TEXT
            )
        ''')
        cursor.execute('CREATE INDEX IF NOT EXISTS track_id_cat_index ON slice (track_id, cat)')
        cursor.execute('CREATE INDEX IF NOT EXISTS track_id_timestamp_end_time_index ON slice (timestamp, end_time, track_id)')
    
    def _insert_slice_data(self, cursor, data: List[Dict[str, Any]], batch_size=10000):
        cursor.execute('SELECT track_id, tid FROM thread')
        tid_to_track_id = {row[1]: row[0] for row in cursor.fetchall()}
        
        slice_events = [event for event in data if event.get('ph') == 'X']
        
        slice_data = []
        max_sqlite_int = 9223372036854775807
        min_sqlite_int = -9223372036854775808
        
        for event in slice_events:
            tid = event.get('tid', '')
            track_id = tid_to_track_id.get(tid)
            
            if track_id is None:
                continue
            
            ts = event.get('ts', 0)
            dur = event.get('dur', 0)
            
            try:
                timestamp = int(float(ts) * 1000)
                duration = int(float(dur) * 1000)
                
                # Prevent SQLite INTEGER overflow
                if not (min_sqlite_int <= timestamp <= max_sqlite_int):
                    timestamp = 0
                if not (min_sqlite_int <= duration <= max_sqlite_int):
                    duration = 0
            except (ValueError, TypeError, OverflowError):
                timestamp = 0
                duration = 0
            
            end_time = timestamp + duration
            if not (min_sqlite_int <= end_time <= max_sqlite_int):
                end_time = max_sqlite_int if end_time > 0 else min_sqlite_int
            
            name = event.get('name', '')
            args_dict = event.get('args')
            
            try:
                args = json.dumps(args_dict, ensure_ascii=False) if args_dict and isinstance(args_dict, dict) and len(args_dict) > 0 else None
            except (TypeError, ValueError) as e:
                logging.warning(f"Failed to serialize args to JSON: {e}")
                args = None
            
            slice_data.append((
                timestamp, duration, name, None, track_id, None, args, None, end_time, None
            ))
        
        slice_data.sort(key=lambda x: x[0])
        
        for i in range(0, len(slice_data), batch_size):
            cursor.executemany(
                'INSERT INTO slice (timestamp, duration, name, depth, track_id, cat, args, cname, end_time, flag_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
                slice_data[i:i+batch_size]
            )
    
    def _create_counter_table(self, cursor):
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS counter (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT,
                pid TEXT,
                timestamp INTEGER,
                cat TEXT,
                args TEXT
            )
        ''')
