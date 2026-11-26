/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
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

import { importRemoteHandler, parseUnitCompletedHandler, removeSingleRemoteHandler } from './handler';
import connector from './index';
import { store } from '../store';
import { CardUnit } from '../insight/units/AscendUnit';
import type { Session } from '../entity/session';
import type { CardRankInfo } from '../api/interface';
import { SimpleCache } from '../cache/simplecache';

jest.mock('../store', () => ({ store: { sessionStore: { activeSession: undefined } } }));
jest.mock('./index', () => ({ __esModule: true, default: { send: jest.fn() } }));
jest.mock('../insight/units/AscendUnit', () => ({ CardUnit: jest.fn(), ROOT_UNIT: jest.fn(), ThreadUnit: jest.fn() }));
jest.mock('../insight/units/unitFunc', () => ({
    updateDataSourceAndParentMetaDataMap: jest.fn(), recursiveExpandUnit: jest.fn(), clearParentMap: jest.fn(),
}));
jest.mock('../utils/PageSetting', () => ({ savePageSetting: jest.fn(), recoverPageSetting: jest.fn(), updatePageSetting: jest.fn() }));
jest.mock('../actions/actionMergeUnits', () => ({ mergeUnitsWhenLoadProject: jest.fn() }));
jest.mock('../components/detailViews/Common', () => ({ queryOneKernel: jest.fn() }));
jest.mock('../components/CategorySearch', () => ({ calculateDomainRange: jest.fn() }));

const dataSource: DataSource = {
    remote: 'local', port: 0, projectName: 'next-project', dataPath: [], projectPath: [], children: [],
};

const cardInfo = (rankId: string): CardRankInfo => ({
    rankInfo: { clusterId: '', host: '', rankName: rankId, rankId, deviceId: '0' }, dbPath: `${rankId}.db`,
});

const incomingCard = (rankId: string): Record<string, unknown> => ({
    rankId, dbPath: `${rankId}.db`, cardName: rankId, cardPath: rankId, cluster: '', dataPathList: [rankId], result: true,
});

const importNotification = (ranks: string[], switchProject: boolean): Record<string, unknown> => ({
    dataSource,
    switchProject,
    importResult: {
        reset: false,
        isPending: false,
        isSimulation: false,
        isIE: false,
        isIpynb: false,
        isCluster: false,
        isMultiDevice: false,
        result: ranks.map(incomingCard),
    },
});

const makeSession = (): Session => ({
    projectName: 'previous-project',
    units: [{ metadata: { cardId: 'cached-rank', dbPath: 'cached-rank.db' }, phase: 'download', children: [] }],
    rankCardInfoMap: new Map([['previous-rank', cardInfo('previous-rank')]]),
    threadsToFetch: new Set(),
    mergedThreadData: { clear: jest.fn() },
    resetKernelMfuState: jest.fn(),
    pmuCounterCache: { clear: jest.fn() },
    pmuCounterGeneration: 3,
    updateUnitsForMultiDevice: jest.fn(),
    timelineMaker: { refreshTrigger: 0 },
    simpleCache: new SimpleCache(),
    asyncDataLoadingList: {},
} as unknown as Session);

it.each(['cached', 'empty', 'pending', 'native', 'unchanged'])('handles origin changes after deleting a source: %s', async mode => {
    const session = makeSession();
    const removedSource = { ...dataSource, dataPath: ['removed'] };
    session.units = ['removed', 'kept'].map(cardId => ({
        metadata: { cardId, dbPath: `${cardId}.db`, dataSource: { ...dataSource, dataPath: [cardId] } },
        children: [],
    })) as unknown as Session['units'];
    session.selectedUnits = [];
    session.eventUnits = [];
    session.pinnedUnits = [];
    session.remoteAttrs = new Map([['local', { maxTimeStamp: 1000 }]]);
    session.mergedThreadData.removeThreadGroupsByCardIds = jest.fn();
    session.deleteRankCardInfoMapItemByDbPath = jest.fn();
    Object.defineProperty(store.sessionStore, 'activeSession', { value: session, writable: true, configurable: true });
    const old = { data: mode === 'empty' ? [] : [[{ id: 'slice', startTime: 150 }]], maxDepth: 1, currentMaxDepth: 1 };
    const fresh = { data: [[{ id: 'slice', startTime: 50 }]], maxDepth: 1, currentMaxDepth: 1 };
    let finish!: (value: unknown) => void;
    let deleted = false;
    window.request = jest.fn(async (_source, { command }) => {
        if (command === 'remote/delete') {
            deleted = true;
            return { startTimeUpdated: mode !== 'unchanged', maxTimeStamp: 900 };
        }
        if (deleted) { return fresh; }
        return mode === 'pending' ? new Promise(resolve => { finish = resolve; }) : old;
    }) as typeof window.request;
    const params = { metaType: mode === 'native' ? 'PYTORCH_API' : 'TEXT', cardId: 'kept', threadId: '2' };
    const initial = session.simpleCache.fetchThreadTraces(dataSource, params);
    await Promise.resolve();
    if (mode !== 'pending') { await initial; }
    const revision = session.simpleCache.threadTraceRevision;
    await removeSingleRemoteHandler({ dataSource: removedSource, singleDataPath: 'removed' });
    expect(session.units).toHaveLength(1);
    expect(session.units[0].metadata).toMatchObject({ cardId: 'kept' });
    expect(session.simpleCache.threadTraceRevision).toBe(revision + (mode === 'unchanged' ? 0 : 1));
    if (mode !== 'unchanged') { expect(session.endTimeAll).toBe(900); }
    const current = session.simpleCache.fetchThreadTraces(dataSource, params);
    if (mode === 'pending') {
        finish(old);
        await initial;
    }
    expect(await current).toEqual(mode === 'unchanged' ? old : fresh);
    expect(await session.simpleCache.fetchThreadTraces(dataSource, params)).toEqual(mode === 'unchanged' ? old : fresh);
    const calls = (window.request as jest.Mock).mock.calls.filter(call => call[1].command === 'unit/threadTraces');
    expect(calls).toHaveLength(mode === 'unchanged' ? 1 : mode === 'native' ? 3 : 2);
});

beforeEach(() => {
    (CardUnit as jest.Mock).mockImplementation(metadata => ({ metadata, phase: 'configuring', children: [] }));
});

it('clears previous project registrations even when every incoming card reuses a downloaded unit', async () => {
    const session = makeSession();
    Object.defineProperty(store.sessionStore, 'activeSession', { value: session, writable: true, configurable: true });
    const cached = session.units[0];
    const clear = jest.spyOn(session.rankCardInfoMap, 'clear');
    const traceClear = jest.spyOn(session.simpleCache, 'clearThreadTraces');
    await importRemoteHandler(importNotification(['cached-rank'], true));

    expect(session.projectName).toBe('next-project');
    expect(session.units).toEqual([cached]);
    expect(cached.phase).toBe('download');
    expect(CardUnit).not.toHaveBeenCalled();
    expect(clear).toHaveBeenCalledTimes(1);
    expect(traceClear).toHaveBeenCalledTimes(1);
    expect(session.rankCardInfoMap.size).toBe(0);
    expect(session.pmuCounterCache.clear).toHaveBeenCalledTimes(1);
    expect(session.pmuCounterGeneration).toBe(4);
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ event: 'updateCategory' }));
});

it('clears previous registrations once before constructing the next project cards', async () => {
    const session = makeSession();
    Object.defineProperty(store.sessionStore, 'activeSession', { value: session, writable: true, configurable: true });
    const clear = jest.spyOn(session.rankCardInfoMap, 'clear');
    await importRemoteHandler(importNotification(['new-rank-0', 'new-rank-1'], true));

    expect(CardUnit).toHaveBeenCalledTimes(2);
    expect(clear).toHaveBeenCalledTimes(1);
    expect(clear.mock.invocationCallOrder[0]).toBeLessThan((CardUnit as jest.Mock).mock.invocationCallOrder[0]);
    expect(session.rankCardInfoMap.size).toBe(0);
    expect(session.units.filter(unit => unit.phase === 'analyzing')).toHaveLength(2);
});

it('preserves registered primary and baseline cards when appending within the same project', async () => {
    const session = makeSession();
    Object.defineProperty(store.sessionStore, 'activeSession', { value: session, writable: true, configurable: true });
    session.rankCardInfoMap.set('baseline-rank', cardInfo('baseline-rank'));
    const clear = jest.spyOn(session.rankCardInfoMap, 'clear');
    const traceClear = jest.spyOn(session.simpleCache, 'clearThreadTraces');
    await importRemoteHandler(importNotification(['additional-rank'], false));

    expect(clear).not.toHaveBeenCalled();
    expect(traceClear).toHaveBeenCalledTimes(1);
    expect(session.rankCardInfoMap.has('previous-rank')).toBe(true);
    expect(session.rankCardInfoMap.has('baseline-rank')).toBe(true);
    expect(CardUnit).toHaveBeenCalledTimes(1);
    expect(session.mergedThreadData.clear).not.toHaveBeenCalled();
    expect(session.pmuCounterCache.clear).not.toHaveBeenCalled();
});

it('invalidates previously cached overlap data without changing completion notifications', async () => {
    const session = makeSession();
    Object.defineProperty(store.sessionStore, 'activeSession', { value: session, writable: true, configurable: true });
    const request = jest.fn().mockResolvedValueOnce({ data: [], maxDepth: 0, currentMaxDepth: 0 })
        .mockResolvedValue({ data: [], maxDepth: 2, currentMaxDepth: 0 });
    window.request = request;
    const params = { dbPath: 'a.db', metaType: 'TEXT' };
    await session.simpleCache.fetchThreadTraces(dataSource, params);
    await session.simpleCache.fetchThreadTraces(dataSource, params);
    expect(request).toHaveBeenCalledTimes(1);
    parseUnitCompletedHandler({ dbId: 'a.db', unitName: 'OVERLAP_ANALYSIS' });
    expect(session.asyncDataLoadingList['a.db'].OVERLAP_ANALYSIS).toBe(true);
    expect(session.asyncDataLoadingList['a.db'].OVERLAP_ANALYSIS_VERSION).toBe(1);
    expect((await session.simpleCache.fetchThreadTraces(dataSource, params))?.maxDepth).toBe(2);
    expect(request).toHaveBeenCalledTimes(2);
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ event: 'updateAnalysisData' }));
});
