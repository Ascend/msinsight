/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import {
    checkLeaksOpfsAvailability,
    checkLeaksOpfsSyncAvailability,
    createBlockPathCacheStorageKey,
    isLeaksOpfsEnabled,
    normalizeLeaksFileHash,
} from './opfsConfig';

describe('OPFS block path cache configuration', () => {
    it('normalizes a SHA-256 hash before using it as a persistent key', () => {
        const hash = 'ABCDEF'.repeat(10) + 'ABCD';

        expect(normalizeLeaksFileHash(` ${hash} `)).toBe(hash.toLowerCase());
        expect(createBlockPathCacheStorageKey(hash.toLowerCase())).toBe(
            `main-cache-v1-${hash.toLowerCase()}`,
        );
    });

    it.each([undefined, null, '', 'not-a-sha256', 'a'.repeat(63), 'g'.repeat(64)])(
        'keeps the temporary-cache behavior for an invalid hash: %p',
        hash => {
            expect(normalizeLeaksFileHash(hash)).toBe('');
        },
    );

    it('accepts a cache hash containing device and event type', () => {
        const fileHash = 'a'.repeat(64);
        const device0Block = `${fileHash}-0-BLOCK`;
        const device1Block = `${fileHash}-1-BLOCK`;
        const device0Segment = `${fileHash}-0-SEGMENT`;

        expect(normalizeLeaksFileHash(device0Block)).toBe(device0Block.toLowerCase());
        expect(normalizeLeaksFileHash(device1Block)).not.toBe(normalizeLeaksFileHash(device0Block));
        expect(normalizeLeaksFileHash(device0Segment)).not.toBe(normalizeLeaksFileHash(device0Block));
    });

    it('probes directory and asynchronous file writes', async () => {
        const storageDescriptor = Object.getOwnPropertyDescriptor(navigator, 'storage');
        const writable = {
            write: jest.fn().mockResolvedValue(undefined),
            close: jest.fn().mockResolvedValue(undefined),
        } as unknown as FileSystemWritableFileStream;
        const file = {
            createWritable: jest.fn().mockResolvedValue(writable),
        } as unknown as FileSystemFileHandle;
        const directory = {
            getFileHandle: jest.fn().mockResolvedValue(file),
        } as unknown as FileSystemDirectoryHandle;
        const root = {
            getDirectoryHandle: jest.fn().mockResolvedValue(directory),
            removeEntry: jest.fn().mockResolvedValue(undefined),
        } as unknown as FileSystemDirectoryHandle;
        Object.defineProperty(navigator, 'storage', {
            configurable: true,
            value: { getDirectory: jest.fn().mockResolvedValue(root) },
        });

        try {
            await expect(checkLeaksOpfsAvailability()).resolves.toBe('available');
            expect(directory.getFileHandle).toHaveBeenCalledTimes(1);
            expect(writable.write).toHaveBeenCalledTimes(1);
            expect(writable.close).toHaveBeenCalledTimes(1);
            expect(root.removeEntry).toHaveBeenCalledTimes(1);
        } finally {
            if (storageDescriptor) {
                Object.defineProperty(navigator, 'storage', storageDescriptor);
            } else {
                Reflect.deleteProperty(navigator, 'storage');
            }
        }
    });

    it('probes synchronous file reads and writes in a worker', async () => {
        const storageDescriptor = Object.getOwnPropertyDescriptor(navigator, 'storage');
        const accessHandle = {
            truncate: jest.fn(),
            write: jest.fn().mockReturnValue(1),
            flush: jest.fn(),
            read: jest.fn((target: Uint8Array) => {
                target[0] = 1;
                return 1;
            }),
            close: jest.fn(),
        } as unknown as FileSystemSyncAccessHandle;
        const file = {
            createSyncAccessHandle: jest.fn().mockResolvedValue(accessHandle),
        } as unknown as FileSystemFileHandle;
        const directory = {
            getFileHandle: jest.fn().mockResolvedValue(file),
        } as unknown as FileSystemDirectoryHandle;
        const root = {
            getDirectoryHandle: jest.fn().mockResolvedValue(directory),
            removeEntry: jest.fn().mockResolvedValue(undefined),
        } as unknown as FileSystemDirectoryHandle;
        Object.defineProperty(navigator, 'storage', {
            configurable: true,
            value: { getDirectory: jest.fn().mockResolvedValue(root) },
        });

        try {
            await expect(checkLeaksOpfsSyncAvailability()).resolves.toBe('available');
            expect(accessHandle.write).toHaveBeenCalledTimes(1);
            expect(accessHandle.read).toHaveBeenCalledTimes(1);
            expect(accessHandle.close).toHaveBeenCalledTimes(1);
            expect(root.removeEntry).toHaveBeenCalledTimes(1);
        } finally {
            if (storageDescriptor) {
                Object.defineProperty(navigator, 'storage', storageDescriptor);
            } else {
                Reflect.deleteProperty(navigator, 'storage');
            }
        }
    });

    it('reports transient when repeated synchronous access attempts fail recoverably', async () => {
        const storageDescriptor = Object.getOwnPropertyDescriptor(navigator, 'storage');
        const file = {
            createSyncAccessHandle: jest.fn().mockRejectedValue(new Error('Sync access denied')),
        } as unknown as FileSystemFileHandle;
        const directory = {
            getFileHandle: jest.fn().mockResolvedValue(file),
        } as unknown as FileSystemDirectoryHandle;
        const root = {
            getDirectoryHandle: jest.fn().mockResolvedValue(directory),
            removeEntry: jest.fn().mockResolvedValue(undefined),
        } as unknown as FileSystemDirectoryHandle;
        Object.defineProperty(navigator, 'storage', {
            configurable: true,
            value: { getDirectory: jest.fn().mockResolvedValue(root) },
        });
        const warn = jest.spyOn(console, 'warn').mockImplementation(() => undefined);

        try {
            await expect(checkLeaksOpfsSyncAvailability()).resolves.toBe('transient');
            expect(file.createSyncAccessHandle).toHaveBeenCalledTimes(2);
            expect(root.removeEntry).toHaveBeenCalledTimes(2);
            expect(isLeaksOpfsEnabled()).toBe(true);
        } finally {
            warn.mockRestore();
            if (storageDescriptor) {
                Object.defineProperty(navigator, 'storage', storageDescriptor);
            } else {
                Reflect.deleteProperty(navigator, 'storage');
            }
        }
    });

    it('recovers when a transient synchronous access failure succeeds on retry', async () => {
        const storageDescriptor = Object.getOwnPropertyDescriptor(navigator, 'storage');
        const accessHandle = {
            truncate: jest.fn(),
            write: jest.fn().mockReturnValue(1),
            flush: jest.fn(),
            read: jest.fn((target: Uint8Array) => {
                target[0] = 1;
                return 1;
            }),
            close: jest.fn(),
        } as unknown as FileSystemSyncAccessHandle;
        const transientError = new Error('The access handle is temporarily unavailable');
        transientError.name = 'NoModificationAllowedError';
        const createSyncAccessHandle = jest.fn()
            .mockRejectedValueOnce(transientError)
            .mockResolvedValueOnce(accessHandle);
        const file = { createSyncAccessHandle } as unknown as FileSystemFileHandle;
        const directory = {
            getFileHandle: jest.fn().mockResolvedValue(file),
        } as unknown as FileSystemDirectoryHandle;
        const root = {
            getDirectoryHandle: jest.fn().mockResolvedValue(directory),
            removeEntry: jest.fn().mockResolvedValue(undefined),
        } as unknown as FileSystemDirectoryHandle;
        Object.defineProperty(navigator, 'storage', {
            configurable: true,
            value: { getDirectory: jest.fn().mockResolvedValue(root) },
        });

        try {
            await expect(checkLeaksOpfsSyncAvailability()).resolves.toBe('available');
            expect(createSyncAccessHandle).toHaveBeenCalledTimes(2);
            expect(root.removeEntry).toHaveBeenCalledTimes(2);
            expect(isLeaksOpfsEnabled()).toBe(true);
        } finally {
            if (storageDescriptor) {
                Object.defineProperty(navigator, 'storage', storageDescriptor);
            } else {
                Reflect.deleteProperty(navigator, 'storage');
            }
        }
    });

    it('permanently disables OPFS for a definitive security restriction', async () => {
        const storageDescriptor = Object.getOwnPropertyDescriptor(navigator, 'storage');
        const securityError = new Error('OPFS requires a secure context');
        securityError.name = 'SecurityError';
        const createSyncAccessHandle = jest.fn().mockRejectedValue(securityError);
        const file = { createSyncAccessHandle } as unknown as FileSystemFileHandle;
        const directory = {
            getFileHandle: jest.fn().mockResolvedValue(file),
        } as unknown as FileSystemDirectoryHandle;
        const root = {
            getDirectoryHandle: jest.fn().mockResolvedValue(directory),
            removeEntry: jest.fn().mockResolvedValue(undefined),
        } as unknown as FileSystemDirectoryHandle;
        Object.defineProperty(navigator, 'storage', {
            configurable: true,
            value: { getDirectory: jest.fn().mockResolvedValue(root) },
        });
        const warn = jest.spyOn(console, 'warn').mockImplementation(() => undefined);

        try {
            await expect(checkLeaksOpfsSyncAvailability()).resolves.toBe('unavailable');
            expect(createSyncAccessHandle).toHaveBeenCalledTimes(1);
            expect(isLeaksOpfsEnabled()).toBe(false);
        } finally {
            warn.mockRestore();
            if (storageDescriptor) {
                Object.defineProperty(navigator, 'storage', storageDescriptor);
            } else {
                Reflect.deleteProperty(navigator, 'storage');
            }
        }
    });

});
