/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import { act, cleanup, render } from '@testing-library/react';
import { runInAction } from 'mobx';
import { Session } from '../entity/session';
import CommunicationAnalysis, { type CardInfo } from '../components/communication/CommunicationAnalysis';
import { AnalysisType, defaultCondition, type ConditionDataType } from '../components/communication/Filter';
import { getSlowRankList, queryCommunication, queryCommunicationOperatorLists } from '../utils/RequestUtils';
import {
    clearCommunicationPageState,
    observeCommunication,
    setCommunicationAgentSession,
    updateCommunicationPageState,
    type CommunicationPageState,
} from './communicationController';

let mockFilterChange: (conditions: ConditionDataType) => Promise<void>;
let mockShowOperator: (card: CardInfo) => void;
let mockReturnHome: () => void;

jest.mock('../components/communication/Filter', () => ({
    __esModule: true,
    ...jest.requireActual<typeof import('../components/communication/Filter')>('../components/communication/Filter'),
    default: ({ handleFilterChange }: { handleFilterChange: typeof mockFilterChange }) => {
        mockFilterChange = handleFilterChange;
        return null;
    },
}));
jest.mock('../components/ClusterSelect', () => ({ ClusterSelect: () => null }));
jest.mock('../components/communication/CommunicationTimeTable', () => ({
    __esModule: true,
    default: ({ showOperator }: { showOperator: typeof mockShowOperator }) => {
        mockShowOperator = showOperator;
        return null;
    },
}));
jest.mock('../components/communication/CommunicationDuration/Opertators', () => ({
    __esModule: true,
    default: ({ returnHome }: { returnHome: typeof mockReturnHome }) => {
        mockReturnHome = returnHome;
        return null;
    },
}));
jest.mock('../components/communication/CommunicationTimeChart', () => ({ __esModule: true, default: () => null }));
jest.mock('../components/communication/CommunicationMatrix', () => ({ __esModule: true, default: () => null }));
jest.mock('../components/communication/CommunicationTimeAnalysisChart', () => ({ __esModule: true, default: () => null }));
jest.mock('../components/communication/CommunicationDuration/AdviceLabel', () => ({ __esModule: true, default: () => null }));
jest.mock('../components/communication/DiffTimeTable', () => ({ __esModule: true, default: () => null }));
jest.mock('@insight/lib/icon', () => ({ HelpIcon: () => null }), { virtual: true });
jest.mock('@insight/lib/components', () => ({
    Layout: ({ children }: { children: React.ReactNode }) => children,
    CollapsiblePanel: ({ children }: { children: React.ReactNode }) => children,
    Tooltip: ({ children }: { children: React.ReactNode }) => children,
}));
jest.mock('react-i18next', () => ({
    useTranslation: () => ({ t: (key: string, options?: { returnObjects?: boolean }) => options?.returnObjects ? [] : key }),
    initReactI18next: { type: '3rdParty', init: () => undefined },
}));
jest.mock('../utils/RequestUtils', () => ({
    getSlowRankList: jest.fn(),
    queryCommunication: jest.fn(),
    queryCommunicationOperatorLists: jest.fn(),
}));

beforeEach(() => {
    jest.mocked(getSlowRankList).mockResolvedValue({ hasAdvice: false } as Awaited<ReturnType<typeof getSlowRankList>>);
    jest.mocked(queryCommunication).mockResolvedValue({ advice: [], items: [] });
    jest.mocked(queryCommunicationOperatorLists).mockResolvedValue({ minTime: 0, maxTime: 0, data: [] });
});

const readySession = (): Session => {
    const session = new Session();
    runInAction(() => {
        session.selectedProjectName = 'private-project';
        session.selectedClusterPath = '/private/cluster-a';
        session.clusterList = [{ name: 'A', path: '/private/cluster-a', parsed: true, durationParsed: true }];
        session.unitcount = 2;
    });
    return session;
};
const page = (): CommunicationPageState => ({
    analysisType: 'CommunicationDurationAnalysis',
    scope: {
        iterationId: '1',
        baselineIterationId: '2',
        stage: '[0,1]',
        pgName: 'dp',
        groupIdHash: 'group-a',
        baselineGroupIdHash: 'group-b',
        operatorName: 'Total Op Info',
    },
    selection: { kind: 'rank-detail', rankId: '1' },
});

afterEach(() => {
    cleanup();
    setCommunicationAgentSession(undefined);
    clearCommunicationPageState();
    jest.restoreAllMocks();
});

describe('Communication controller', () => {
    test('returns an unavailable observation without a session', () => {
        jest.spyOn(Date, 'now').mockReturnValue(123);
        expect(observeCommunication()).toStrictEqual({
            moduleId: 'Communication',
            observedAt: 123,
            available: false,
            dataStatus: null,
            analysisType: null,
            scope: null,
            selection: null,
        });
    });

    test('reports current session flags without inventing a page selection', () => {
        expect(setCommunicationAgentSession(new Session())).toBeUndefined();
        expect(observeCommunication()).toMatchObject({
            available: true,
            dataStatus: {
                hasSelectedCluster: false,
                clusterParsed: false,
                durationParsed: false,
                compareMode: false,
                hasTimelineUnits: false,
            },
            analysisType: null,
            scope: null,
            selection: null,
        });
    });

    test('does not substitute the default analysis type before the page reports', () => {
        setCommunicationAgentSession(readySession());
        expect(observeCommunication()).toMatchObject({ available: true, analysisType: null, scope: null, selection: null });
    });

    test.each(['CommunicationMatrix', 'CommunicationDurationAnalysis'] as const)('observes reported %s context', analysisType => {
        setCommunicationAgentSession(readySession());
        updateCommunicationPageState({ ...page(), analysisType });
        expect(observeCommunication()).toMatchObject({
            analysisType,
            scope: {
                iterationId: '1',
                baselineIterationId: null,
                stage: '[0,1]',
                pgName: 'dp',
                groupIdHash: 'group-a',
                baselineGroupIdHash: null,
                operatorName: 'Total Op Info',
            },
            selection: { kind: 'rank-detail', rankId: '1' },
        });
    });

    test('keeps the report when setting the same session again', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        updateCommunicationPageState(page());
        expect(setCommunicationAgentSession(session)).toBeUndefined();
        expect(observeCommunication().analysisType).toBe('CommunicationDurationAnalysis');
    });

    test('clears the previous page report when the session changes', () => {
        setCommunicationAgentSession(readySession());
        updateCommunicationPageState(page());
        setCommunicationAgentSession(readySession());
        expect(observeCommunication()).toMatchObject({ available: true, analysisType: null, scope: null, selection: null });
    });

    test('clears reported state when the session is released', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        updateCommunicationPageState(page());
        setCommunicationAgentSession(undefined);
        expect(observeCommunication()).toMatchObject({ available: false, scope: null, selection: null });
        setCommunicationAgentSession(session);
        expect(observeCommunication().scope).toBeNull();
    });

    test('clear is idempotent and preserves the session flags', () => {
        setCommunicationAgentSession(readySession());
        updateCommunicationPageState(page());
        expect(clearCommunicationPageState()).toBeUndefined();
        expect(clearCommunicationPageState()).toBeUndefined();
        expect(observeCommunication()).toMatchObject({ available: true, dataStatus: { clusterParsed: true }, scope: null, selection: null });
    });

    test('reads flags for the selected cluster and hides page data while it is unavailable', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        updateCommunicationPageState(page());
        runInAction(() => {
            session.clusterList = [
                { name: 'A', path: '/private/cluster-a', parsed: false, durationParsed: false },
                { name: 'B', path: '/private/cluster-b', parsed: true, durationParsed: true },
            ];
            session.unitcount = 0;
        });
        expect(observeCommunication()).toMatchObject({
            available: true,
            dataStatus: { hasSelectedCluster: true, clusterParsed: false, durationParsed: false, hasTimelineUnits: false },
            analysisType: null,
            scope: null,
            selection: null,
        });
    });

    test('reports comparison filters without inventing the selected rank side', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        updateCommunicationPageState(page());
        runInAction(() => { session.isCompare = true; });
        expect(observeCommunication()).toMatchObject({
            dataStatus: { compareMode: true },
            scope: { iterationId: '1', baselineIterationId: '2', groupIdHash: 'group-a', baselineGroupIdHash: 'group-b' },
            selection: null,
        });
    });

    test.each(['0', '01'])('preserves the opaque rank ID %s', rankId => {
        setCommunicationAgentSession(readySession());
        updateCommunicationPageState({ ...page(), selection: { kind: 'rank-detail', rankId } });
        expect(observeCommunication().selection).toStrictEqual({ kind: 'rank-detail', rankId });
    });

    test('uses null for empty filters and an empty rank detail', () => {
        setCommunicationAgentSession(readySession());
        updateCommunicationPageState({
            analysisType: null,
            scope: { iterationId: '', baselineIterationId: '', stage: '', pgName: '', groupIdHash: '', baselineGroupIdHash: '', operatorName: '' },
            selection: { kind: 'rank-detail', rankId: '' },
        });
        expect(observeCommunication()).toMatchObject({
            analysisType: null,
            scope: { iterationId: null, baselineIterationId: null, stage: null, pgName: null, groupIdHash: null, baselineGroupIdHash: null, operatorName: null },
            selection: null,
        });
    });

    test('projects only business fields and never exports private source properties', () => {
        setCommunicationAgentSession(readySession());
        const state = Object.assign(page(), { rawStore: { secret: 'private-value' } });
        Object.assign(state.scope, { clusterPath: '/private/cluster-a', targetOperatorName: 'private-target' });
        if (state.selection) Object.assign(state.selection, { dbPath: '/private/data.db' });
        updateCommunicationPageState(state);
        const result = observeCommunication();
        expect(Object.keys(result).sort()).toEqual(['analysisType', 'available', 'dataStatus', 'moduleId', 'observedAt', 'scope', 'selection']);
        expect(Object.keys(result.scope as object).sort()).toEqual(['baselineGroupIdHash', 'baselineIterationId', 'groupIdHash', 'iterationId', 'operatorName', 'pgName', 'stage']);
        expect(JSON.stringify(result)).not.toContain('private');
        expect(JSON.parse(JSON.stringify(result))).toEqual(result);
    });

    test('copies the supplied fields and returns independent observation objects', () => {
        setCommunicationAgentSession(readySession());
        const input = page();
        updateCommunicationPageState(input);
        input.scope.iterationId = 'changed-input';
        if (input.selection) input.selection.rankId = 'changed-input';
        const first = observeCommunication();
        expect(first).toMatchObject({ scope: { iterationId: '1' }, selection: { rankId: '1' } });
        (first.scope as Record<string, unknown>).iterationId = 'changed-output';
        (first.selection as Record<string, unknown>).rankId = 'changed-output';
        expect(observeCommunication()).toMatchObject({ scope: { iterationId: '1' }, selection: { rankId: '1' } });
    });
});

describe('Communication page reporting through the real component', () => {
    test('reports the actual default page after mount and clears it on unmount', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        const view = render(React.createElement(CommunicationAnalysis, { session }));
        expect(observeCommunication()).toMatchObject({
            analysisType: defaultCondition.type,
            scope: { iterationId: null, operatorName: null },
            selection: null,
        });
        view.unmount();
        expect(observeCommunication()).toMatchObject({ available: true, analysisType: null, scope: null, selection: null });
    });

    test.each([AnalysisType.COMMUNICATION_MATRIX, AnalysisType.COMMUNICATION_DURATION_ANALYSIS])('reports committed %s filters without raw paths', async type => {
        const session = readySession();
        setCommunicationAgentSession(session);
        render(React.createElement(CommunicationAnalysis, { session }));
        const conditions = {
            ...defaultCondition,
            type,
            iterationId: '01',
            baselineIterationId: '02',
            stage: '[0,1]',
            operatorName: 'all_reduce',
            pgName: 'dp',
            groupIdHash: 'group-a',
            baselineGroupIdHash: 'group-b',
            clusterPath: '/private/cluster',
            targetOperatorName: 'private-jump-target',
        };
        await act(async () => { await mockFilterChange(conditions); });
        const result = observeCommunication();
        expect(result).toMatchObject({
            analysisType: type,
            scope: {
                iterationId: '01',
                baselineIterationId: null,
                stage: '[0,1]',
                operatorName: 'all_reduce',
                pgName: 'dp',
                groupIdHash: 'group-a',
                baselineGroupIdHash: null,
            },
            selection: null,
        });
        expect(JSON.stringify(result)).not.toContain('private');
    });

    test('reports rank zero from detail selection and clears it on return', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        render(React.createElement(CommunicationAnalysis, { session }));
        act(() => { mockShowOperator({ cardId: '0', dbPath: '/private/rank.db' }); });
        expect(observeCommunication().selection).toStrictEqual({ kind: 'rank-detail', rankId: '0' });
        expect(JSON.stringify(observeCommunication())).not.toContain('private');
        act(() => { mockReturnHome(); });
        expect(observeCommunication().selection).toBeNull();
    });

    test('clears an inactive page and republishes when it becomes active', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        const view = render(React.createElement(CommunicationAnalysis, { session }));
        expect(observeCommunication().analysisType).toBe(defaultCondition.type);
        view.rerender(React.createElement(CommunicationAnalysis, { session, active: false }));
        expect(observeCommunication()).toMatchObject({ analysisType: null, scope: null, selection: null });
        view.rerender(React.createElement(CommunicationAnalysis, { session, active: true }));
        expect(observeCommunication().analysisType).toBe(defaultCondition.type);
    });

    test('clears the report while cluster data is unavailable', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        render(React.createElement(CommunicationAnalysis, { session }));
        act(() => { runInAction(() => { session.clusterList = []; }); });
        expect(observeCommunication()).toMatchObject({ analysisType: null, scope: null, selection: null });
        act(() => {
            runInAction(() => {
                session.clusterList = [{ name: 'A', path: '/private/cluster-a', parsed: true, durationParsed: true }];
            });
        });
        expect(observeCommunication().analysisType).toBe(defaultCondition.type);
    });

    test('reports comparison scope without inventing a rank side', async () => {
        const session = readySession();
        runInAction(() => { session.isCompare = true; });
        setCommunicationAgentSession(session);
        render(React.createElement(CommunicationAnalysis, { session }));
        await act(async () => {
            await mockFilterChange({ ...defaultCondition, baselineIterationId: '02', baselineGroupIdHash: 'baseline-group' });
        });
        act(() => { mockShowOperator({ cardId: '1', dbPath: '/private/rank.db' }); });
        expect(observeCommunication()).toMatchObject({
            scope: { baselineIterationId: '02', baselineGroupIdHash: 'baseline-group' }, selection: null,
        });
    });

    test('reports again when the entry supplies a replacement session', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        const view = render(React.createElement(CommunicationAnalysis, { session }));
        const replacement = readySession();
        setCommunicationAgentSession(replacement);
        expect(observeCommunication().analysisType).toBeNull();
        view.rerender(React.createElement(CommunicationAnalysis, { session: replacement }));
        expect(observeCommunication().analysisType).toBe(defaultCondition.type);
    });

    test('StrictMode effect replay leaves a report and unmount clears it', () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        const view = render(React.createElement(React.StrictMode, null, React.createElement(CommunicationAnalysis, { session })));
        expect(observeCommunication().analysisType).toBe(defaultCondition.type);
        view.unmount();
        expect(observeCommunication()).toMatchObject({ analysisType: null, scope: null, selection: null });
    });

    test('observation does not trigger additional business requests', async () => {
        const session = readySession();
        setCommunicationAgentSession(session);
        render(React.createElement(CommunicationAnalysis, { session }));
        await act(async () => {
            await mockFilterChange({ ...defaultCondition, type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS, stage: '[0,1]', operatorName: 'all_reduce' });
        });
        const requests = [getSlowRankList, queryCommunication, queryCommunicationOperatorLists];
        const counts = requests.map(request => jest.mocked(request).mock.calls.length);
        expect(counts).toEqual([1, 1, 1]);
        expect(observeCommunication().analysisType).toBe(AnalysisType.COMMUNICATION_DURATION_ANALYSIS);
        observeCommunication();
        expect(requests.map(request => jest.mocked(request).mock.calls.length)).toEqual(counts);
    });

    test('exports only the reviewed page contract and never treats filter or jump hints as a slice', async () => {
        jest.spyOn(Date, 'now').mockReturnValue(123);
        const session = readySession();
        runInAction(() => {
            session.targetOperator = { name: 'private-jump-hint', rankId: 99, timestamp: 456, duration: 789 };
        });
        setCommunicationAgentSession(session);
        render(React.createElement(CommunicationAnalysis, { session }));
        await act(async () => {
            await mockFilterChange({
                ...defaultCondition,
                type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
                iterationId: '01',
                baselineIterationId: '02',
                stage: '[0,1]',
                operatorName: 'filtered_all_reduce',
                pgName: 'dp',
                groupIdHash: 'group-a',
                baselineGroupIdHash: 'group-b',
                ...{ clusterPath: '/private/cluster', targetOperatorName: 'private-target', dbPath: '/private/extra.db' },
            });
        });
        const expected = {
            moduleId: 'Communication',
            observedAt: 123,
            available: true,
            dataStatus: { hasSelectedCluster: true, clusterParsed: true, durationParsed: true, compareMode: false, hasTimelineUnits: true },
            analysisType: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
            scope: {
                iterationId: '01',
                baselineIterationId: null,
                stage: '[0,1]',
                pgName: 'dp',
                groupIdHash: 'group-a',
                baselineGroupIdHash: null,
                operatorName: 'filtered_all_reduce',
            },
            selection: null,
        };
        expect(observeCommunication()).toStrictEqual(expected);
        act(() => { mockShowOperator({ cardId: '01', dbPath: 'C:\\private\\rank.db' }); });
        expect(observeCommunication()).toStrictEqual({ ...expected, selection: { kind: 'rank-detail', rankId: '01' } });
        act(() => { mockReturnHome(); });
        expect(observeCommunication()).toStrictEqual(expected);
    });

    test('reports committed filters while business results are still pending without claiming result readiness', async () => {
        let complete!: (value: Awaited<ReturnType<typeof queryCommunicationOperatorLists>>) => void;
        const response = new Promise<Awaited<ReturnType<typeof queryCommunicationOperatorLists>>>(resolve => { complete = resolve; });
        jest.mocked(queryCommunicationOperatorLists).mockReturnValueOnce(response);
        const session = readySession();
        setCommunicationAgentSession(session);
        render(React.createElement(CommunicationAnalysis, { session }));
        let pending: Promise<void> | undefined;
        try {
            await act(async () => {
                pending = mockFilterChange({ ...defaultCondition, type: AnalysisType.COMMUNICATION_DURATION_ANALYSIS, stage: '[0,1]', operatorName: 'pending_all_reduce' });
            });
            expect(queryCommunication).not.toHaveBeenCalled();
            const result = observeCommunication();
            expect(result).toMatchObject({
                analysisType: AnalysisType.COMMUNICATION_DURATION_ANALYSIS,
                scope: { operatorName: 'pending_all_reduce' },
                selection: null,
            });
            expect(Object.keys(result).sort()).toEqual(['analysisType', 'available', 'dataStatus', 'moduleId', 'observedAt', 'scope', 'selection']);
        } finally {
            await act(async () => {
                complete({ minTime: 0, maxTime: 0, data: [] });
                await pending;
            });
        }
        expect(queryCommunication).toHaveBeenCalledTimes(1);
    });
});
