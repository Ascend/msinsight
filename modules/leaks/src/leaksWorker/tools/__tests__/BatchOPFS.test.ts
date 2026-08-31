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

import { BatchOPFS } from '../BatchOPFS';
import { isLeaksOpfsEnabled } from '../opfsConfig';

describe('BatchOPFS', () => {
    it('falls back to memory when file access fails after directory initialization', async () => {
        const storageDescriptor = Object.getOwnPropertyDescriptor(navigator, 'storage');
        const getFileHandle = jest.fn().mockRejectedValue(new Error('File access denied'));
        const directory = { getFileHandle } as unknown as FileSystemDirectoryHandle;
        const root = {
            getDirectoryHandle: jest.fn().mockResolvedValue(directory),
        } as unknown as FileSystemDirectoryHandle;
        Object.defineProperty(navigator, 'storage', {
            configurable: true,
            value: { getDirectory: jest.fn().mockResolvedValue(root) },
        });
        const warn = jest.spyOn(console, 'warn').mockImplementation(() => undefined);

        try {
            const batches = new BatchOPFS('file-access-failure');

            await batches.init();
            await expect(batches.write(0, new Float32Array([1, 2]))).resolves.toBeUndefined();
            await expect(batches.write(1, new Float32Array([3, 4]))).resolves.toBeUndefined();

            expect(getFileHandle).toHaveBeenCalledTimes(1);
            expect(Array.from(batches.read(0) ?? [])).toEqual([1, 2]);
            expect(Array.from(batches.read(1) ?? [])).toEqual([3, 4]);
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

    it('stores independent in-memory batches when OPFS is unavailable', async () => {
        const storageDescriptor = Object.getOwnPropertyDescriptor(navigator, 'storage');
        Object.defineProperty(navigator, 'storage', { configurable: true, value: undefined });
        const warn = jest.spyOn(console, 'warn').mockImplementation(() => undefined);

        try {
            const batches = new BatchOPFS('unsupported-environment');
            const source = new Float32Array([1, 2, 3, 4]);
            const target = new Float32Array(8);

            await batches.init();
            await batches.write(0, source);
            source.fill(0);

            expect(Array.from(batches.read(0, target) ?? [])).toEqual([1, 2, 3, 4]);

            const readWithoutTarget = batches.read(0);
            readWithoutTarget?.fill(5);
            expect(Array.from(batches.read(0) ?? [])).toEqual([1, 2, 3, 4]);

            const readWithSmallTarget = batches.read(0, new Float32Array(2));
            readWithSmallTarget?.fill(6);
            expect(Array.from(batches.read(0) ?? [])).toEqual([1, 2, 3, 4]);

            await batches.clear();
            expect(batches.read(0)).toBeNull();
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
