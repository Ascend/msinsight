/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { CommandHandler, ObservationData } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import type { Session } from '../entity/session';
import connector from '../connection';
import { queryTimelineUnitKernelDetail } from '../utils/RequestUtils';
import eventBus from '../utils/eventBus';
import type { GetSlowRankListResult } from '../utils/interface';
import { AnalysisType, updateData, type ConditionDataType } from '../components/communication/Filter';
import type { AnalysisChartData } from '../components/communication/CommunicationTimeAnalysisChart';

export interface CommunicationScope {
    iterationId: string | null;
    baselineIterationId: string | null;
    stage: string | null;
    pgName: string | null;
    groupIdHash: string | null;
    baselineGroupIdHash: string | null;
    operatorName: string | null;
}

export interface CommunicationPageState {
    analysisType: 'CommunicationMatrix' | 'CommunicationDurationAnalysis' | null;
    scope: CommunicationScope;
    selection: { kind: 'rank-detail'; rankId: string } | null;
}

let activeSession: Session | undefined;
let pageState: CommunicationPageState | undefined;

export interface SlowRanksContext {
    result: GetSlowRankListResult;
    session: Session;
    clusterPath: string;
    conditions: ConditionDataType;
    isCompare: boolean;
}

interface CommunicationState {
    conditions: ConditionDataType;
    slowRanksContext?: SlowRanksContext;
    slowRanks: GetSlowRankListResult | null;
    chart: AnalysisChartData;
    unitcount: number;
    isCompare: boolean;
}

let currentState: CommunicationState | undefined;
export const setCommunicationAgentState = (state: CommunicationState | undefined): void => {
    currentState = state;
};

export const setCommunicationAgentSession = (session: Session | undefined): void => {
    if (activeSession !== session) {
        pageState = undefined;
        setCommunicationAgentState(undefined);
    }
    activeSession = session;
};

export const updateCommunicationPageState = (state: CommunicationPageState): void => {
    pageState = {
        analysisType: state.analysisType,
        scope: scopeOf(state.scope, true),
        selection: selectionOf(state.selection),
    };
};

export const clearCommunicationPageState = (): void => {
    pageState = undefined;
};

export const observeCommunication = (): ObservationData => {
    const session = activeSession;
    const page = session?.clusterCompleted ? pageState : undefined;
    return {
        moduleId: 'Communication',
        observedAt: Date.now(),
        available: session !== undefined,
        dataStatus: session === undefined
            ? null
            : {
                hasSelectedCluster: Boolean(session.selectedClusterPath),
                clusterParsed: session.clusterCompleted,
                durationParsed: session.durationFileCompleted,
                compareMode: session.isCompare,
                hasTimelineUnits: session.unitcount > 0,
            },
        analysisType: page?.analysisType ?? null,
        scope: page ? scopeOf(page.scope, session?.isCompare ?? false) : null,
        // The existing rank detail does not identify a comparison side.
        selection: page && !session?.isCompare ? selectionOf(page.selection) : null,
        slowRankAdvice: observeSlowRankAdvice(session, page),
    };
};

const observeSlowRankAdvice = (session: Session | undefined, page: CommunicationPageState | undefined): object => {
    const state = currentState;
    const context = state?.slowRanksContext;
    const result = state?.slowRanks;
    // Result identity binds the provenance to this response, not a previous filter's response.
    // Compare all published filters as well as the session/cluster, without exporting private paths.
    if (!session?.clusterCompleted || !session.durationFileCompleted || session.isCompare ||
        page?.analysisType !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS ||
        state?.conditions.type !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS || state.isCompare ||
        !result || !context || context.result !== result || context.session !== session || context.isCompare ||
        context.conditions.type !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS ||
        context.clusterPath !== session.selectedClusterPath ||
        !sameScope(context.conditions, state.conditions) || !sameScope(context.conditions, page.scope)) {
        return { status: 'unavailable' };
    }
    // The backend currently returns Top 3 ranks with Top 3 operators each. Keep a defensive
    // bound if that contract grows, and explicitly report every omitted row.
    const ranks = result.hasAdvice ? (result.data ?? []) : [];
    return {
        status: result.hasAdvice ? (ranks.length > 0 ? 'ready' : 'empty') : 'no-advice',
        hasAdvice: result.hasAdvice,
        timeUnit: 'us',
        ...(result.hasAdvice ? { fastRankId: result.fastRankId, fastTotalElapseTime: result.fastTotalElapseTime } : {}),
        totalRanks: ranks.length,
        truncated: ranks.length > 3 || ranks.some(rank => rank.opList.length > 3),
        ranks: ranks.slice(0, 3).map(rank => ({
            rankId: rank.rankId,
            totalElapseTime: rank.totalElapseTime,
            totalDiffTime: rank.totalDiffTime,
            totalOperators: rank.opList.length,
            truncated: rank.opList.length > 3,
            operators: rank.opList.slice(0, 3).map(operator => ({
                name: operator.name,
                elapseTime: operator.elapseTime,
                diffTime: operator.diffTime,
                maxTime: operator.maxTime,
            })),
        })),
    };
};

const sameScope = (conditions: ConditionDataType, scope: CommunicationScope | ConditionDataType): boolean => (
    (['iterationId', 'baselineIterationId', 'stage', 'pgName', 'groupIdHash', 'baselineGroupIdHash', 'operatorName'] as const)
        .every(key => (conditions[key] === '' ? null : conditions[key] ?? null) ===
            (scope[key] === '' ? null : scope[key] ?? null))
);

const scopeOf = (scope: CommunicationScope, isCompare: boolean): CommunicationScope => ({
    iterationId: scope.iterationId === '' ? null : scope.iterationId,
    baselineIterationId: isCompare && scope.baselineIterationId !== '' ? scope.baselineIterationId : null,
    stage: scope.stage === '' ? null : scope.stage,
    pgName: scope.pgName === '' ? null : scope.pgName,
    groupIdHash: scope.groupIdHash === '' ? null : scope.groupIdHash,
    baselineGroupIdHash: isCompare && scope.baselineGroupIdHash !== '' ? scope.baselineGroupIdHash : null,
    operatorName: scope.operatorName === '' ? null : scope.operatorName,
});

const selectionOf = (selection: CommunicationPageState['selection']): CommunicationPageState['selection'] => (
    selection?.rankId ? { kind: 'rank-detail', rankId: selection.rankId } : null
);

const requireDurationState = (): CommunicationState => {
    if (!currentState || currentState.conditions.type !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS) {
        throw new Error('Open Communication Duration Analysis before selecting a slow rank.');
    }
    if (currentState.isCompare || !currentState.slowRanks?.hasAdvice) {
        throw new Error('No slow-rank advice is available for the current analysis.');
    }
    return currentState;
};

const switchAnalysis: CommandHandler = (args) => {
    if (args.type !== AnalysisType.COMMUNICATION_MATRIX && args.type !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS) {
        throw new Error('type must be CommunicationMatrix or CommunicationDurationAnalysis.');
    }
    if (!currentState) throw new Error('Communication page is unavailable.');
    updateData({ ...currentState.conditions, type: args.type });
    return { analysisType: args.type };
};

const highlightSlowRank: CommandHandler = (args) => {
    const state = requireDurationState();
    if (typeof args.rankId !== 'string' || !args.rankId || (args.operatorName !== undefined && (typeof args.operatorName !== 'string' || !args.operatorName))) {
        throw new Error('rankId must be a non-empty string and operatorName must be a non-empty string.');
    }
    const rank = state.slowRanks?.data.find(item => item.rankId === args.rankId);
    const operator = rank?.opList.find(item => args.operatorName === undefined || item.name === args.operatorName);
    if (!operator || !rank) throw new Error('Slow rank or operator is not in the current advice list.');
    if (!state.chart.data?.some(item => item.rankId === rank.rankId)) {
        throw new Error('The slow rank is not available in the current communication chart.');
    }
    eventBus.emit('onClickSlowRankOp', {
        rankId: rank.rankId,
        name: operator.name,
        startValue: Math.floor(operator.maxStartTime / 1000),
        endValue: Math.ceil((operator.maxStartTime + operator.maxTime) / 1000),
    });
    return { rankId: rank.rankId, operatorName: operator.name };
};

const locateInTimeline: CommandHandler = async (args, context) => {
    if (typeof args.rankId !== 'string' || !args.rankId ||
        typeof args.operatorName !== 'string' || !args.operatorName.trim()) {
        throw new Error('rankId must be a non-empty string and operatorName must be a non-empty string.');
    }
    const state = requireDurationState();
    const session = activeSession;
    if ((observeSlowRankAdvice(session, pageState) as { status: string }).status !== 'ready') {
        throw new Error('No current slow-rank advice is available for the current Communication context.');
    }
    if (state.unitcount <= 0 || !session || session.unitcount <= 0) throw new Error('Timeline data is unavailable.');
    const rankId = args.rankId;
    const name = args.operatorName;
    const operator = state.slowRanks?.data.find(item => item.rankId === rankId)?.opList.find(item => item.name === name);
    const dbPath = state.chart.data?.find(item => item.rankId === rankId)?.dbPath;
    if (!operator || !dbPath) throw new Error('The specified slow rank or operator is not available in the current advice and chart.');
    const params = { name, rankId, dbPath };
    // Snapshot target values as well as identities: in-place result/chart changes must not reuse old details.
    const targetSnapshot = JSON.stringify(operator);
    const scopeSnapshot = JSON.stringify(state.conditions);
    const clusterPath = session.selectedClusterPath;
    const detail = await queryTimelineUnitKernelDetail(params);
    if (context.signal.aborted) throw new Error('Timeline navigation was cancelled.');
    if (activeSession !== session || session.selectedClusterPath !== clusterPath || currentState !== state ||
        (observeSlowRankAdvice(session, pageState) as { status: string }).status !== 'ready' ||
        JSON.stringify(state.conditions) !== scopeSnapshot || state.unitcount <= 0 || session.unitcount <= 0 ||
        state.slowRanks?.data.find(item => item.rankId === rankId)?.opList.find(item => item.name === name) !== operator ||
        JSON.stringify(operator) !== targetSnapshot ||
        state.chart.data?.find(item => item.rankId === rankId)?.dbPath !== dbPath) {
        throw new Error('The communication context or target changed during Timeline navigation.');
    }
    if (detail?.startTime === undefined || detail?.pid === undefined) throw new Error('The selected operator could not be located in Timeline.');
    connector.send({
        event: 'switchModule',
        body: {
            switchTo: 'timeline',
            toModuleEvent: 'locateUnit',
            params: {
                ...detail,
                ...params,
                processId: detail.pid,
                startTime: detail.startTime,
                rankId: detail.rankId,
                duration: operator.elapseTime * 1000,
                showSelectedData: true,
            },
        },
    });
    return { rankId, operatorName: name, navigated: true };
};

export const registerCommunicationCommands = (client: ModuleAgentCommandClient): (() => void) => {
    const unregister = [
        client.registerCommand({
            name: 'Communication.switchAnalysis',
            title: 'Switch communication analysis',
            description: 'Switch between Communication Matrix and Communication Duration Analysis.',
            inputSchema: {
                type: 'object',
                properties: { type: { type: 'string', enum: [AnalysisType.COMMUNICATION_MATRIX, AnalysisType.COMMUNICATION_DURATION_ANALYSIS] } },
                required: ['type'],
                additionalProperties: false,
            },
        }, switchAnalysis),
        client.registerCommand({
            name: 'Communication.highlightSlowRank',
            title: 'Highlight a slow-rank operator',
            description: 'Find and highlight an operator on a slow rank in Communication Duration Analysis. Defaults to the first advised operator on that rank.',
            inputSchema: {
                type: 'object',
                properties: { rankId: { type: 'string', minLength: 1 }, operatorName: { type: 'string', minLength: 1 } },
                required: ['rankId'],
                additionalProperties: false,
            },
        }, highlightSlowRank),
        client.registerCommand({
            name: 'Communication.locateInTimeline',
            title: 'Locate slow-rank operator in Timeline',
            description: 'Open Timeline at the explicitly specified operator from current valid slow-rank advice in Communication Duration Analysis (parsed cluster and duration, non-compare, Timeline data available). No prior highlight is required. Navigation selects the exact operator and loads its available connections using the chart-click behavior.',
            inputSchema: {
                type: 'object',
                properties: { rankId: { type: 'string', minLength: 1 }, operatorName: { type: 'string', minLength: 1 } },
                required: ['rankId', 'operatorName'],
                additionalProperties: false,
            },
        }, locateInTimeline),
    ];
    return () => unregister.forEach(dispose => dispose());
};
