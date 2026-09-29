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

interface CommunicationState {
    conditions: ConditionDataType;
    slowRanks: GetSlowRankListResult | null;
    chart: AnalysisChartData;
    unitcount: number;
    isCompare: boolean;
}

let currentState: CommunicationState | undefined;
let selectedOperator: { rankId: number; name: string } | undefined;

export const setCommunicationAgentState = (state: CommunicationState | undefined): void => {
    if (currentState?.conditions !== state?.conditions) selectedOperator = undefined;
    currentState = state;
    if (!state || state.conditions.type !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS || state.isCompare ||
        (selectedOperator && !state.slowRanks?.data.some(rank => rank.rankId === selectedOperator?.rankId &&
            rank.opList.some(operator => operator.name === selectedOperator?.name)))) selectedOperator = undefined;
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
    };
};

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
    selectedOperator = undefined;
    updateData({ ...currentState.conditions, type: args.type });
    return { analysisType: args.type };
};

const highlightSlowRank: CommandHandler = (args) => {
    const state = requireDurationState();
    if (!Number.isInteger(args.rankId) || (args.operatorName !== undefined && (typeof args.operatorName !== 'string' || !args.operatorName))) {
        throw new Error('rankId must be an integer and operatorName must be a non-empty string.');
    }
    const rank = state.slowRanks?.data.find(item => item.rankId === args.rankId);
    const operator = rank?.opList.find(item => args.operatorName === undefined || item.name === args.operatorName);
    if (!operator || !rank) throw new Error('Slow rank or operator is not in the current advice list.');
    if (!state.chart.data?.some(item => Number(item.rankId) === rank.rankId)) {
        throw new Error('The slow rank is not available in the current communication chart.');
    }
    selectedOperator = { rankId: rank.rankId, name: operator.name };
    eventBus.emit('onClickSlowRankOp', {
        rankId: rank.rankId,
        name: operator.name,
        startValue: Math.floor(operator.maxStartTime / 1000),
        endValue: Math.ceil((operator.maxStartTime + operator.maxTime) / 1000),
    });
    return { rankId: rank.rankId, operatorName: operator.name };
};

const locateInTimeline: CommandHandler = async (_args, context) => {
    const state = requireDurationState();
    if (!selectedOperator) throw new Error('Select a slow-rank operator before locating it in Timeline.');
    if (state.unitcount <= 0) throw new Error('Timeline data is unavailable.');
    const { rankId, name } = selectedOperator;
    const operator = state.slowRanks?.data.find(item => item.rankId === rankId)?.opList.find(item => item.name === name);
    const dbPath = state.chart.data?.find(item => Number(item.rankId) === rankId)?.dbPath;
    if (!operator || !dbPath) throw new Error('The selected operator is no longer available.');
    const params = { name, rankId: String(rankId), dbPath };
    const detail = await queryTimelineUnitKernelDetail(params);
    if (context.signal.aborted) throw new Error('Timeline navigation was cancelled.');
    if (currentState !== state || selectedOperator?.rankId !== rankId || selectedOperator.name !== name) {
        throw new Error('The communication selection changed during Timeline navigation.');
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
                properties: { rankId: { type: 'integer' }, operatorName: { type: 'string', minLength: 1 } },
                required: ['rankId'],
                additionalProperties: false,
            },
        }, highlightSlowRank),
        client.registerCommand({
            name: 'Communication.locateInTimeline',
            title: 'Locate slow-rank operator in Timeline',
            description: 'Open Timeline at the slow-rank operator most recently highlighted by Communication.highlightSlowRank.',
            inputSchema: { type: 'object', properties: {}, additionalProperties: false },
        }, locateInTimeline),
    ];
    return () => unregister.forEach(dispose => dispose());
};
