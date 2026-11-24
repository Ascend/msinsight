/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { runInAction } from 'mobx';
import { createAdapter } from '../adapter';

jest.mock('../../common/leaksWorker/blockWorker/worker', () => ({ workerDestroy: jest.fn() }));
jest.mock('../../rendering/stateWorker/worker', () => ({ workerDestroy: jest.fn() }));

it('clears dataset state while preserving the mounted viewport on removal', () => {
    const adapter = createAdapter();
    runInAction(() => {
        adapter.session.deviceIds = { 0: ['PTA'] };
        adapter.session.dbPath = '/synthetic/data';
        adapter.session.loadingBlocks = true;
        adapter.session.loadingOverview = true;
        adapter.session.memSnapshotParseLoading = true;
        adapter.session.memSnapshotParseFileId = '/synthetic/data.pickle';
        adapter.session.leaksWorkerInfo.renderOptions.viewport = { width: 1000, height: 480 };
        adapter.session.leaksWorkerInfo.sizeInfo.maxSize = 1024;
    });
    adapter.reset();
    expect(adapter.session.deviceIds).toEqual({});
    expect(adapter.session.dbPath).toBe('');
    expect(adapter.session.loadingBlocks).toBe(false);
    expect(adapter.session.loadingOverview).toBe(false);
    expect(adapter.session.memSnapshotParseLoading).toBe(false);
    expect(adapter.session.memSnapshotParseFileId).toBe('');
    expect(adapter.session.leaksWorkerInfo.renderOptions.viewport).toEqual({ width: 1000, height: 480 });
    expect(adapter.session.leaksWorkerInfo.sizeInfo.maxSize).toBe(0);
});
