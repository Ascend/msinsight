/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { Session } from '../entity/session';
import type { ThreadMetaData } from '../entity/data';
import type { ChartDesc } from '../entity/insight';
import type { StackStatusData } from '../entity/chart';
import { ThreadUnit } from '../insight/units/AscendUnit';
import { getUnitFlows } from '../api/request';
import { selectRenderedSlice } from '../utils/selectRenderedSlice';
import { store } from '../store';
import { locateUnitHandler } from './handler';

// Detail panels are unrelated to navigation/flow selection and load browser-only UI dependencies.
jest.mock('../components/SelectedDataBottomPanel', () => ({ SelectedDataBottomPanel: () => null }));
jest.mock('../components/charts/ChartInteractor/draw', () => ({ checkIsValidSlice: jest.fn(() => false) }));
jest.mock('../store', () => ({ store: { sessionStore: { activeSession: undefined } } }));
jest.mock('../connection', () => ({ __esModule: true, default: { send: jest.fn(), addListener: jest.fn() } }));
jest.mock('../api/request', () => ({ getUnitFlows: jest.fn() }));
jest.mock('../insight/units/utils', () => ({
    ...jest.requireActual('../insight/units/utils'),
    getTimeOffset: () => 100,
}));

const metadata = {
    cardId: '1', dbPath: '/data.db', processId: 'p1', threadId: 't1', threadName: 'stream', metaType: 'TEXT',
} as ThreadMetaData;
const target = {
    id: 'exact-id', rankId: '1', dbPath: '/data.db', processId: 'p1', threadId: 't1',
    name: 'all_reduce', startTime: 1100, duration: 50, depth: 1, showSelectedData: true,
};
const flows = { unitAllFlows: [{ cat: 'async', flows: [{
    cat: 'async', from: { id: 'exact-id', pid: 'p1', tid: 't1', timestamp: 1100, duration: 50, depth: 1 },
    to: { id: 'other-id', pid: 'p1', tid: 't2', timestamp: 1200, duration: 20, depth: 1 },
}] }] };
let session: Session;
let lane: InstanceType<typeof ThreadUnit>;
const navigate = (): void => {
    locateUnitHandler(target);
    const navigation = session.locateUnit;
    if (navigation?.target(lane)) {
        session.selectedUnits = [lane];
        navigation.onSuccess(lane);
    }
    session.locateUnit = undefined;
};
const flushFlows = async (): Promise<void> => { await Promise.resolve(); await Promise.resolve(); };

beforeEach(() => {
    jest.spyOn(window, 'requestAnimationFrame').mockReturnValue(0);
    session = new Session({ isNsMode: true });
    session.phase = 'download';
    session.domainRange = { domainStart: 0, domainEnd: 2000 };
    store.sessionStore.activeSession = session;
    lane = new ThreadUnit(metadata);
    (getUnitFlows as jest.Mock).mockResolvedValue(flows);
});
afterEach(() => {
    store.sessionStore.activeSession = undefined;
    session.cancelZoomingHistory();
    jest.clearAllMocks();
    jest.restoreAllMocks();
});

test('one exact navigation selects with click semantics and loads connections without a rendered slice or name search', async () => {
    navigate();
    await flushFlows();
    expect(session.selectedUnits).toEqual([lane]);
    expect(session.locateUnit).toBeUndefined();
    expect(session.selectedData).toMatchObject({
        id: 'exact-id', name: 'all_reduce', startTime: 1000, timestamp: 1100,
        cardId: '1', dbPath: '/data.db', threadId: 't1', processId: 'p1', showSelectedData: true,
        metaType: 'TEXT', offsetSide: 'host',
    });
    expect(getUnitFlows).toHaveBeenCalledTimes(1);
    expect(getUnitFlows).toHaveBeenCalledWith(expect.objectContaining({
        id: 'exact-id', rankId: '1', dbPath: '/data.db', pid: 'p1', tid: 't1', startTime: 1100, endTime: 1150,
    }));
    expect(session.drawLineMode).toBe('single');
    expect(session.singleLinkLine.async).toHaveLength(1);
    expect(session.mapOfLinkLines.size).toBeGreaterThan(0);
});

test.each([
    { cardId: '11' }, { cardId: '0' }, { dbPath: '/other.db' }, { processId: 'p2' }, { threadId: 't2' },
])('does not claim a selection or query flows for an unmatched lane %j', mismatch => {
    lane = new ThreadUnit({ ...metadata, ...mismatch });
    navigate();
    expect(session.selectedData).toBeUndefined();
    expect(getUnitFlows).not.toHaveBeenCalled();
});

test('does not select a target after switching sessions or while the selection is locked', () => {
    locateUnitHandler(target);
    const navigation = session.locateUnit;
    store.sessionStore.activeSession = undefined;
    navigation?.onSuccess(lane);
    expect(session.selectedData).toBeUndefined();
    store.sessionStore.activeSession = session;
    session.selectedRangeIsLock = true;
    navigation?.onSuccess(lane);
    expect(session.selectedData).toBeUndefined();
    expect(getUnitFlows).not.toHaveBeenCalled();
});

test.each(['repeat', 'click', 'session', 'pending-navigation'])('discards stale asynchronous connections after %s', async change => {
    let finish!: (value: typeof flows) => void;
    (getUnitFlows as jest.Mock).mockImplementationOnce(() => new Promise(resolve => { finish = resolve; }));
    navigate();
    if (change === 'repeat') {
        (getUnitFlows as jest.Mock).mockResolvedValue({ unitAllFlows: [] });
        navigate();
        await flushFlows();
    } else if (change === 'click') {
        selectRenderedSlice(session, metadata, undefined, undefined, lane);
    } else if (change === 'session') {
        store.sessionStore.activeSession = undefined;
    } else {
        locateUnitHandler({ ...target, id: 'next-id' });
    }
    finish(flows);
    await flushFlows();
    expect(session.singleLinkLine).toEqual({});
    expect(session.mapOfLinkLines.size).toBe(0);
});

test('normal click still uses the same flow callback and original timestamp conversion', async () => {
    const chart = lane.chart as ChartDesc<'stackStatus'>;
    const clickedSlice: StackStatusData = {
        ...target, startTime: 1000, originalStartTime: 1100, cardId: '1',
        type: target.name, color: 'deepBlue', cname: '',
    };
    selectRenderedSlice(session, metadata, clickedSlice, chart.onClick, lane);
    await flushFlows();
    expect(getUnitFlows).toHaveBeenCalledWith(expect.objectContaining({ id: 'exact-id', startTime: 1100 }));
    expect(session.singleLinkLine.async).toHaveLength(1);
});
