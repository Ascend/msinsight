/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import { importRemoteHandler } from './handler';
import connector from './index';
import { store } from '../store';
import { CardUnit } from '../insight/units/AscendUnit';
import type { Session } from '../entity/session';
import type { CardRankInfo } from '../api/interface';

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
    updateUnitsForMultiDevice: jest.fn(),
    timelineMaker: { refreshTrigger: 0 },
} as unknown as Session);

beforeEach(() => {
    (CardUnit as jest.Mock).mockImplementation(metadata => ({ metadata, phase: 'configuring', children: [] }));
});

it('clears previous project registrations even when every incoming card reuses a downloaded unit', async () => {
    const session = makeSession();
    Object.defineProperty(store.sessionStore, 'activeSession', { value: session, writable: true, configurable: true });
    const cached = session.units[0];
    const clear = jest.spyOn(session.rankCardInfoMap, 'clear');
    await importRemoteHandler(importNotification(['cached-rank'], true));

    expect(session.projectName).toBe('next-project');
    expect(session.units).toEqual([cached]);
    expect(cached.phase).toBe('download');
    expect(CardUnit).not.toHaveBeenCalled();
    expect(clear).toHaveBeenCalledTimes(1);
    expect(session.rankCardInfoMap.size).toBe(0);
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
    await importRemoteHandler(importNotification(['additional-rank'], false));

    expect(clear).not.toHaveBeenCalled();
    expect(session.rankCardInfoMap.has('previous-rank')).toBe(true);
    expect(session.rankCardInfoMap.has('baseline-rank')).toBe(true);
    expect(CardUnit).toHaveBeenCalledTimes(1);
    expect(session.mergedThreadData.clear).not.toHaveBeenCalled();
});
