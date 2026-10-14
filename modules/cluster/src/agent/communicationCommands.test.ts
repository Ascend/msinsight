import type { CommandDefinition, CommandHandler, JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { AnalysisType, defaultCondition, updateData } from '../components/communication/Filter';
import { getSlowRankList, queryTimelineUnitKernelDetail } from '../utils/RequestUtils';
import type { Session } from '../entity/session';
import connector from '../connection';
import eventBus from '../utils/eventBus';
import {
    registerCommunicationCommands, setCommunicationAgentState, setCommunicationAgentSession,
    updateCommunicationPageState, observeCommunication,
} from './communicationController';

jest.mock('../components/communication/Filter', () => ({
    AnalysisType: { COMMUNICATION_MATRIX: 'CommunicationMatrix', COMMUNICATION_DURATION_ANALYSIS: 'CommunicationDurationAnalysis' },
    defaultCondition: { type: 'CommunicationMatrix', stage: 'stage-one' },
    updateData: jest.fn(),
}));
jest.mock('../utils/RequestUtils', () => ({
    ...jest.requireActual<typeof import('../utils/RequestUtils')>('../utils/RequestUtils'),
    queryTimelineUnitKernelDetail: jest.fn(),
}));
jest.mock('../store', () => ({ store: { sessionStore: { activeSession: { selectedClusterPath: '/cluster' } } } }));
jest.mock('@insight/lib/utils', () => ({
    createCancelableApi: (fn: unknown) => fn,
    createSmartDebounceRequestFunc: (fn: unknown) => fn,
}));
jest.mock('../connection', () => ({ __esModule: true, default: { send: jest.fn() } }));
jest.mock('../utils/eventBus', () => ({ __esModule: true, default: { emit: jest.fn() } }));

const handlers = new Map<string, CommandHandler>();
const definitions = new Map<string, CommandDefinition>();
const client = {
    registerCommand(definition: CommandDefinition, handler: CommandHandler): () => void {
        handlers.set(definition.name, handler);
        definitions.set(definition.name, definition);
        return () => { handlers.delete(definition.name); definitions.delete(definition.name); };
    },
} as ModuleAgentCommandClient;

const invoke = (name: string, args: JsonObject = {}): Promise<unknown> => Promise.resolve().then(() => handlers.get(name)?.(args, {
    requestId: 'test-request', deadline: Date.now() + 5000, signal: new AbortController().signal,
}));

const durationState = () => ({
    conditions: { ...defaultCondition, type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS },
    slowRanks: {
        hasAdvice: true, fastRankId: '0', fastTotalElapseTime: 100,
        data: [{ rankId: '3', totalElapseTime: 200, totalDiffTime: 100, opList: [{
            name: 'AllReduce', startTime: 1000, elapseTime: 2000, diffTime: 100,
            maxStartTime: 2000, maxTime: 3000,
        }] }],
    },
    chart: { minTime: 0, maxTime: 10, data: [{ rankId: '3', dbPath: '/data/rank3', lists: { compare: [], baseline: [], diff: [] } }] },
    unitcount: 1,
    isCompare: false,
});

const target = { rankId: '3', operatorName: 'AllReduce' };
const publish = (state = durationState()) => {
    const session = {
        clusterCompleted: true, durationFileCompleted: true, isCompare: false,
        selectedClusterPath: '/cluster', unitcount: 1,
    } as Session;
    setCommunicationAgentSession(session);
    updateCommunicationPageState({
        analysisType: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
        scope: { iterationId: null, baselineIterationId: null, stage: 'stage-one', pgName: null,
            groupIdHash: null, baselineGroupIdHash: null, operatorName: null }, selection: null,
    });
    const published = { ...state, slowRanksContext: { result: state.slowRanks, session,
        clusterPath: '/cluster', conditions: { ...state.conditions }, isCompare: false } };
    setCommunicationAgentState(published);
    return { state: published, session };
};

beforeEach(() => {
    jest.clearAllMocks();
    handlers.clear();
    definitions.clear();
    setCommunicationAgentState(undefined);
    registerCommunicationCommands(client);
});

afterEach(() => {
    setCommunicationAgentSession(undefined);
    setCommunicationAgentState(undefined);
});

test.each([undefined, 'AllReduce'])('string wire IDs remain consistent through observation, highlight (%s) and locate', async operatorName => {
    const state = durationState();
    const wire = {
        ...state.slowRanks, fastRankId: '0',
        data: state.slowRanks.data.map(rank => ({ ...rank, rankId: '3' })),
    };
    window.requestDataDebounced = Object.assign(jest.fn().mockResolvedValue(wire), {
        cancel: jest.fn(),
        flush: jest.fn(),
        getPendingCount: jest.fn(() => 0),
        getPendingKeys: jest.fn(() => [] as string[]),
    });
    const result = await getSlowRankList({ ...state.conditions, isCompare: state.isCompare });
    expect(result).toBe(wire);
    const session = {
        clusterCompleted: true, durationFileCompleted: true, isCompare: false,
        selectedClusterPath: '/cluster', unitcount: 1,
    } as Session;
    setCommunicationAgentSession(session);
    updateCommunicationPageState({
        analysisType: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
        scope: { iterationId: null, baselineIterationId: null, stage: 'stage-one', pgName: null,
            groupIdHash: null, baselineGroupIdHash: null, operatorName: null },
        selection: null,
    });
    setCommunicationAgentState({ ...state, slowRanks: result,
        slowRanksContext: { result, session, clusterPath: '/cluster', conditions: state.conditions, isCompare: false } });
    const advice = observeCommunication().slowRankAdvice as { fastRankId: string; ranks: Array<{ rankId: string; operators: Array<{ name: string }> }> };
    expect(advice).toMatchObject({ status: 'ready', fastRankId: '0', ranks: [{ rankId: '3' }] });
    const rankId = advice.ranks[0].rankId;
    const args: JsonObject = { rankId };
    if (operatorName !== undefined) args.operatorName = advice.ranks[0].operators[0].name;
    expect(await invoke('Communication.highlightSlowRank', args)).toEqual({ rankId: '3', operatorName: 'AllReduce' });
    expect(eventBus.emit).toHaveBeenCalledWith('onClickSlowRankOp', { rankId: '3', name: 'AllReduce', startValue: 2, endValue: 5 });
    jest.mocked(queryTimelineUnitKernelDetail).mockResolvedValue({ pid: 12, startTime: 0, rankId: '3' });
    expect(await invoke('Communication.locateInTimeline', target)).toEqual({ rankId: '3', operatorName: 'AllReduce', navigated: true });
    expect(queryTimelineUnitKernelDetail).toHaveBeenCalledWith({ name: 'AllReduce', rankId: '3', dbPath: '/data/rank3' });
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({ body: expect.objectContaining({
        params: expect.objectContaining({ rankId: '3', duration: 2000000, processId: 12, startTime: 0 }),
    }) }));
    // Membership is checked against the current chart, not just the advice rows.
    setCommunicationAgentState({ ...state, slowRanks: result, chart: { ...state.chart, data: [] } });
    await expect(invoke('Communication.highlightSlowRank', args)).rejects.toThrow('not available in the current communication chart');
    await expect(invoke('Communication.highlightSlowRank', { rankId, operatorName: 'Missing' })).rejects.toThrow('not in the current advice');
});

test('registers three discoverable commands and switches the active analysis without dropping filters', async () => {
    expect([...definitions.keys()]).toEqual([
        'Communication.switchAnalysis', 'Communication.highlightSlowRank', 'Communication.locateInTimeline',
    ]);
    setCommunicationAgentState({ ...durationState(), conditions: { ...defaultCondition, type: AnalysisType.COMMUNICATION_MATRIX } });
    await invoke('Communication.switchAnalysis', { type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS });
    expect(updateData).toHaveBeenCalledWith({ ...defaultCondition, type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS });
    await expect(invoke('Communication.switchAnalysis', { type: 'unknown' })).rejects.toThrow('type must be');
});

test('highlights an advised slow-rank operator', async () => {
    setCommunicationAgentState(durationState());
    await expect(invoke('Communication.highlightSlowRank', { rankId: '7' })).rejects.toThrow('not in the current advice');
    expect(await invoke('Communication.highlightSlowRank', { rankId: '3' })).toEqual({ rankId: '3', operatorName: 'AllReduce' });
    expect(eventBus.emit).toHaveBeenCalledWith('onClickSlowRankOp', {
        rankId: '3', name: 'AllReduce', startValue: 2, endValue: 5,
    });
});

test.each(['0', '01', 'worker-A'])('preserves opaque rank %s from highlight through Timeline navigation', async rankId => {
    const state = durationState();
    state.slowRanks.data[0].rankId = rankId;
    state.chart.data[0].rankId = rankId;
    state.chart.data[0].dbPath = `/data/${rankId}`;
    publish(state);
    expect(observeCommunication().slowRankAdvice).toMatchObject({ status: 'ready', ranks: [{ rankId }] });
    expect(definitions.get('Communication.highlightSlowRank')?.inputSchema).toMatchObject({
        properties: { rankId: { type: 'string', minLength: 1 } },
    });

    expect(await invoke('Communication.highlightSlowRank', { rankId })).toEqual({ rankId, operatorName: 'AllReduce' });
    expect(eventBus.emit).toHaveBeenCalledWith('onClickSlowRankOp', expect.objectContaining({ rankId }));
    (queryTimelineUnitKernelDetail as jest.Mock).mockResolvedValue({ pid: 12, startTime: 0, rankId });
    await invoke('Communication.locateInTimeline', { rankId, operatorName: 'AllReduce' });
    expect(queryTimelineUnitKernelDetail).toHaveBeenCalledWith({ name: 'AllReduce', rankId, dbPath: `/data/${rankId}` });
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({
        body: expect.objectContaining({ params: expect.objectContaining({ rankId }) }),
    }));
});

test('does not match rank 01 to rank 1 in advice or the rendered chart', async () => {
    const state = durationState();
    state.slowRanks.data[0].rankId = '01';
    state.chart.data[0].rankId = '1';
    publish(state);
    await expect(invoke('Communication.highlightSlowRank', { rankId: '1' })).rejects.toThrow('not in the current advice');
    await expect(invoke('Communication.highlightSlowRank', { rankId: '01' })).rejects.toThrow('not available in the current communication chart');
    await expect(invoke('Communication.locateInTimeline', { ...target, rankId: '1' })).rejects.toThrow('not available');
    await expect(invoke('Communication.locateInTimeline', { ...target, rankId: '01' })).rejects.toThrow('not available');
    expect(queryTimelineUnitKernelDetail).not.toHaveBeenCalled();
    expect(eventBus.emit).not.toHaveBeenCalled();
});

test.each([3, 0, '', null])('rejects non-string or empty rank identity %s', async rankId => {
    setCommunicationAgentState(durationState());
    await expect(invoke('Communication.highlightSlowRank', { rankId })).rejects.toThrow('rankId must be a non-empty string');
    expect(eventBus.emit).not.toHaveBeenCalled();
});

test('navigates directly without selecting or highlighting an operator', async () => {
    publish();
    (queryTimelineUnitKernelDetail as jest.Mock).mockResolvedValue({ pid: 12, startTime: 0, rankId: '3' });
    await invoke('Communication.locateInTimeline', target);
    expect(eventBus.emit).not.toHaveBeenCalled();
    expect(queryTimelineUnitKernelDetail).toHaveBeenCalledWith({ name: 'AllReduce', rankId: '3', dbPath: '/data/rank3' });
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({
        event: 'switchModule', body: expect.objectContaining({
            switchTo: 'timeline', toModuleEvent: 'locateUnit',
            params: expect.objectContaining({ duration: 2000000, processId: 12, startTime: 0 }),
        }),
    }));
    setCommunicationAgentState({ ...durationState(), conditions: { ...defaultCondition, type: AnalysisType.COMMUNICATION_MATRIX } });
    await expect(invoke('Communication.locateInTimeline', target)).rejects.toThrow('Open Communication Duration Analysis');
});


test('publishes the explicit target schema and context requirement', () => {
    expect(definitions.get('Communication.locateInTimeline')).toMatchObject({
        description: expect.stringContaining('No prior highlight is required'),
        inputSchema: { required: ['rankId', 'operatorName'], additionalProperties: false,
            properties: { rankId: { type: 'string', minLength: 1 },
                operatorName: { type: 'string', minLength: 1 } } },
    });
});

test.each<JsonObject>([{}, { rankId: '3' }, { operatorName: 'AllReduce' }, { ...target, rankId: 3 },
    { ...target, rankId: '' }, { ...target, rankId: null },
    { ...target, rankId: -1 }, { ...target, rankId: 1.5 }, { ...target, rankId: Number.MAX_SAFE_INTEGER + 1 },
    { ...target, operatorName: '' }, { ...target, operatorName: ' ' }, { ...target, operatorName: 3 }])(
    'rejects invalid explicit parameters without requests: %j', async args => {
        publish();
        await expect(invoke('Communication.locateInTimeline', args)).rejects.toThrow('rankId must be');
        expect(queryTimelineUnitKernelDetail).not.toHaveBeenCalled();
        expect(connector.send).not.toHaveBeenCalled();
    },
);

test.each(['rank', 'operator', 'chart', 'timeline', 'session-timeline', 'advice', 'provenance', 'cluster', 'compare', 'page'])(
    'rejects unavailable target or context: %s', async change => {
        const { state, session } = publish();
        let args = target;
        if (change === 'rank') args = { ...target, rankId: '99' };
        if (change === 'operator') args = { ...target, operatorName: 'allreduce' };
        if (change === 'chart') state.chart.data = [];
        if (change === 'timeline') state.unitcount = 0;
        if (change === 'session-timeline') session.unitcount = 0;
        if (change === 'advice') state.slowRanks.hasAdvice = false;
        if (change === 'provenance') state.slowRanksContext.result = durationState().slowRanks;
        if (change === 'cluster') session.selectedClusterPath = '/other';
        if (change === 'compare') session.isCompare = true;
        if (change === 'page') updateCommunicationPageState({ analysisType: 'CommunicationMatrix',
            scope: { iterationId: null, baselineIterationId: null, stage: null, pgName: null,
                groupIdHash: null, baselineGroupIdHash: null, operatorName: null }, selection: null });
        await expect(invoke('Communication.locateInTimeline', args)).rejects.toThrow();
        expect(queryTimelineUnitKernelDetail).not.toHaveBeenCalled();
        expect(connector.send).not.toHaveBeenCalled();
    },
);

const addOtherTarget = (state: ReturnType<typeof durationState>) => {
    state.slowRanks.data.push({ ...state.slowRanks.data[0], rankId: '4',
        opList: [{ ...state.slowRanks.data[0].opList[0], name: 'OtherReduce', elapseTime: 4000 }] });
    state.chart.data.push({ ...state.chart.data[0], rankId: '4', dbPath: '/data/rank4' });
};

test('explicit targets ignore latest highlights including during pending details, without cross-target pollution', async () => {
    const { state } = publish();
    addOtherTarget(state);
    await invoke('Communication.highlightSlowRank', { rankId: '4' });
    let complete!: (detail: unknown) => void;
    jest.mocked(queryTimelineUnitKernelDetail).mockImplementationOnce(() => new Promise(resolve => { complete = resolve; }));
    const first = invoke('Communication.locateInTimeline', target);
    await Promise.resolve();
    await invoke('Communication.highlightSlowRank', target);
    await invoke('Communication.highlightSlowRank', { rankId: '4' });
    complete({ pid: 12, startTime: 0, rankId: '3' });
    await expect(first).resolves.toEqual({ ...target, navigated: true });
    jest.mocked(queryTimelineUnitKernelDetail).mockResolvedValue({ pid: 13, startTime: 10, rankId: '4' });
    await invoke('Communication.locateInTimeline', { rankId: '4', operatorName: 'OtherReduce' });
    expect(queryTimelineUnitKernelDetail).toHaveBeenNthCalledWith(1, { name: 'AllReduce', rankId: '3', dbPath: '/data/rank3' });
    expect(queryTimelineUnitKernelDetail).toHaveBeenNthCalledWith(2, { name: 'OtherReduce', rankId: '4', dbPath: '/data/rank4' });
    expect(connector.send).toHaveBeenNthCalledWith(2, expect.objectContaining({ body: expect.objectContaining({
        params: expect.objectContaining({ name: 'OtherReduce', rankId: '4', duration: 4000000 }),
    }) }));
});

test.each(['cancel', 'session', 'cluster', 'filter', 'state', 'target', 'chart', 'compare', 'timeline', 'page'])(
    'refuses stale async navigation after %s', async change => {
        const { state, session } = publish();
        let complete!: (detail: unknown) => void;
        jest.mocked(queryTimelineUnitKernelDetail).mockImplementationOnce(() => new Promise(resolve => { complete = resolve; }));
        const abort = new AbortController();
        const pending = Promise.resolve().then(() => handlers.get('Communication.locateInTimeline')?.(target, {
            requestId: 'pending', deadline: Date.now() + 5000, signal: abort.signal,
        }));
        await Promise.resolve();
        if (change === 'cancel') abort.abort();
        if (change === 'session') setCommunicationAgentSession({ ...session } as Session);
        if (change === 'cluster') session.selectedClusterPath = '/other';
        if (change === 'filter') state.conditions.stage = 'changed';
        if (change === 'state') setCommunicationAgentState({ ...state });
        if (change === 'target') state.slowRanks.data[0].opList[0].elapseTime++;
        if (change === 'chart') state.chart.data[0].dbPath = '/other';
        if (change === 'compare') session.isCompare = true;
        if (change === 'timeline') session.unitcount = 0;
        if (change === 'page') updateCommunicationPageState({ analysisType: 'CommunicationMatrix',
            scope: { iterationId: null, baselineIterationId: null, stage: null, pgName: null,
                groupIdHash: null, baselineGroupIdHash: null, operatorName: null }, selection: null });
        complete({ pid: 12, startTime: 0, rankId: '3' });
        await expect(pending).rejects.toThrow();
        expect(connector.send).not.toHaveBeenCalled();
    },
);
