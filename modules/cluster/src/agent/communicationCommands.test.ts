import type { CommandDefinition, CommandHandler, JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import { AnalysisType, defaultCondition, updateData } from '../components/communication/Filter';
import { queryTimelineUnitKernelDetail } from '../utils/RequestUtils';
import connector from '../connection';
import eventBus from '../utils/eventBus';
import { registerCommunicationCommands, setCommunicationAgentState } from './communicationController';

jest.mock('../components/communication/Filter', () => ({
    AnalysisType: { COMMUNICATION_MATRIX: 'CommunicationMatrix', COMMUNICATION_DURATION_ANALYSIS: 'CommunicationDurationAnalysis' },
    defaultCondition: { type: 'CommunicationMatrix', stage: 'stage-one' },
    updateData: jest.fn(),
}));
jest.mock('../utils/RequestUtils', () => ({ queryTimelineUnitKernelDetail: jest.fn() }));
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
        hasAdvice: true, fastRankId: 0, fastTotalElapseTime: 100,
        data: [{ rankId: 3, totalElapseTime: 200, totalDiffTime: 100, opList: [{
            name: 'AllReduce', startTime: 1000, elapseTime: 2000, diffTime: 100,
            maxStartTime: 2000, maxTime: 3000,
        }] }],
    },
    chart: { minTime: 0, maxTime: 10, data: [{ rankId: '3', dbPath: '/data/rank3', lists: { compare: [], baseline: [], diff: [] } }] },
    unitcount: 1,
    isCompare: false,
});

beforeEach(() => {
    jest.clearAllMocks();
    handlers.clear();
    definitions.clear();
    setCommunicationAgentState(undefined);
    registerCommunicationCommands(client);
});

afterEach(() => setCommunicationAgentState(undefined));

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
    await expect(invoke('Communication.highlightSlowRank', { rankId: 7 })).rejects.toThrow('not in the current advice');
    expect(await invoke('Communication.highlightSlowRank', { rankId: 3 })).toEqual({ rankId: 3, operatorName: 'AllReduce' });
    expect(eventBus.emit).toHaveBeenCalledWith('onClickSlowRankOp', {
        rankId: 3, name: 'AllReduce', startValue: 2, endValue: 5,
    });
});

test('navigates only after selecting an available operator and resolving timeline details', async () => {
    setCommunicationAgentState(durationState());
    await expect(invoke('Communication.locateInTimeline')).rejects.toThrow('Select a slow-rank operator');
    await invoke('Communication.highlightSlowRank', { rankId: 3 });
    (queryTimelineUnitKernelDetail as jest.Mock).mockResolvedValue({ pid: 12, startTime: 0, rankId: '3' });
    await invoke('Communication.locateInTimeline');
    expect(queryTimelineUnitKernelDetail).toHaveBeenCalledWith({ name: 'AllReduce', rankId: '3', dbPath: '/data/rank3' });
    expect(connector.send).toHaveBeenCalledWith(expect.objectContaining({
        event: 'switchModule', body: expect.objectContaining({
            switchTo: 'timeline', toModuleEvent: 'locateUnit',
            params: expect.objectContaining({ duration: 2000000, processId: 12, startTime: 0 }),
        }),
    }));
    setCommunicationAgentState({ ...durationState(), conditions: { ...defaultCondition, type: AnalysisType.COMMUNICATION_MATRIX } });
    await expect(invoke('Communication.locateInTimeline')).rejects.toThrow('Open Communication Duration Analysis');
});
