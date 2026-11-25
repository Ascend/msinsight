/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import { runInAction } from 'mobx';
import { createAdapter } from '../adapter';

jest.mock('../../common/leaksWorker/blockWorker/worker', () => ({ workerDestroy: jest.fn() }));

it('clears dataset state while preserving the mounted viewport on removal', () => {
    const adapter = createAdapter();
    runInAction(() => {
        adapter.session.deviceIds = { 0: ['PTA'] };
        adapter.session.dbPath = '/synthetic/data';
        adapter.session.loadingBlocks = true;
        adapter.session.loadingOverview = true;
        adapter.session.threadIds = [42];
        adapter.session.loadingFunc = true;
        adapter.session.leaksWorkerInfo.renderOptions.viewport = { width: 1000, height: 480 };
        adapter.session.leaksWorkerInfo.renderOptions.transform = { x: 20, y: 30, scaleX: 2, scaleY: 2 };
        adapter.session.leaksWorkerInfo.sizeInfo.maxSize = 1024;
    });
    adapter.reset();
    expect(adapter.session.deviceIds).toEqual({});
    expect(adapter.session.dbPath).toBe('');
    expect(adapter.session.loadingBlocks).toBe(false);
    expect(adapter.session.loadingOverview).toBe(false);
    expect(adapter.session.threadIds).toEqual([]);
    expect(adapter.session.loadingFunc).toBe(false);
    expect(adapter.session.leaksWorkerInfo.renderOptions.viewport).toEqual({ width: 1000, height: 480 });
    expect(adapter.session.leaksWorkerInfo.renderOptions.transform).toEqual({ x: 0, y: 0, scaleX: 1, scaleY: 1 });
    expect(adapter.session.leaksWorkerInfo.sizeInfo.maxSize).toBe(0);
});
