/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

interface FileSystemSyncAccessHandle {
    close: () => void;
    flush: () => void;
    getSize: () => number;
    read: (buffer: ArrayBuffer | ArrayBufferView, options?: { at?: number }) => number;
    truncate: (size: number) => void;
    write: (buffer: ArrayBuffer | ArrayBufferView, options?: { at?: number }) => number;
}

interface FileSystemFileHandle {
    createSyncAccessHandle: () => Promise<FileSystemSyncAccessHandle>;
    createWritable: () => Promise<FileSystemWritableFileStream>;
    getFile: () => Promise<FileSystemFile>;
}

interface FileSystemFile {
    size: number;
    arrayBuffer: () => Promise<ArrayBuffer>;
}

interface FileSystemWritableFileStream {
    write: (data: ArrayBuffer | ArrayBufferView) => Promise<void>;
    close: () => Promise<void>;
}

interface FileSystemDirectoryHandle {
    getDirectoryHandle: (name: string, options?: { create?: boolean }) => Promise<FileSystemDirectoryHandle>;
    getFileHandle: (name: string, options?: { create?: boolean }) => Promise<FileSystemFileHandle>;
    keys: () => AsyncIterableIterator<string>;
    removeEntry?: (name: string, options?: { recursive?: boolean }) => Promise<void>;
}

interface StorageManager {
    getDirectory: () => Promise<FileSystemDirectoryHandle>;
}

interface Navigator {
    storage: StorageManager;
}
