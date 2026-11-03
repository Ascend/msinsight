/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
import { runInAction } from 'mobx';
import { createAdapter } from '../adapter';

jest.mock('../../common/leaksWorker/blockWorker/worker', () => ({ workerDestroy: jest.fn() }));
jest.mock('../../rendering/stateWorker/worker', () => ({ workerDestroy: jest.fn() }));

it('clears application loading and dataset metadata when removing data', () => {
    const adapter = createAdapter();
    runInAction(() => {
        adapter.session.deviceIds = { 0: ['PTA'] };
        adapter.session.dbPath = '/synthetic/data';
        adapter.session.loadingBlocks = true;
        adapter.session.loadingOverview = true;
        adapter.session.memSnapshotParseLoading = true;
        adapter.session.memSnapshotParseFileId = '/synthetic/data.pickle';
    });
    adapter.reset();
    expect(adapter.session.deviceIds).toEqual({});
    expect(adapter.session.dbPath).toBe('');
    expect(adapter.session.loadingBlocks).toBe(false);
    expect(adapter.session.loadingOverview).toBe(false);
    expect(adapter.session.memSnapshotParseLoading).toBe(false);
    expect(adapter.session.memSnapshotParseFileId).toBe('');
});
