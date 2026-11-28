/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { CommandContext, CommandHandler, JsonObject, ObservationData } from '@insight/lib/FrontendAgentCommand';
import { CommandError, COMMAND_ERROR_CODES, withAbortSignal } from '@insight/lib/FrontendAgentCommand';
import { reaction } from 'mobx';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import type { Session } from '../entity/session';
import connector from '../connection';
import { queryTimelineUnitKernelDetail } from '../utils/RequestUtils';
import type { GetSlowRankListResult } from '../utils/interface';
import { AnalysisType, prepareAnalysisType, type ConditionDataType } from '../components/communication/Filter';
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

interface ResultContext<T> {
    result: T;
    session: Session;
    clusterPath: string;
    conditions: ConditionDataType;
    isCompare: boolean;
}
export type SlowRanksContext = ResultContext<GetSlowRankListResult>;
export type ChartContext = ResultContext<AnalysisChartData>;

type ResourceStatus = 'pending' | 'ready' | 'error';
interface CommunicationState {
    conditions: ConditionDataType;
    slowRanksContext?: SlowRanksContext;
    slowRanks: GetSlowRankListResult | null;
    adviceStatus?: ResourceStatus;
    chart: AnalysisChartData;
    chartContext?: ChartContext;
    chartStatus?: ResourceStatus;
    filterLoading?: boolean;
    unitcount: number;
    isCompare: boolean;
}

const stateListeners = new Set<() => void>();
const notifyState = (): void => stateListeners.forEach(listener => listener());
let currentState: CommunicationState | undefined;
export const setCommunicationAgentState = (state: CommunicationState | undefined): void => {
    currentState = state;
    notifyState();
};

interface SlowRankTarget { rankId: string; name: string; startValue: number; endValue: number }
let chartConsumer: { session: Session; data: AnalysisChartData; highlight: (target: SlowRankTarget) => void } | undefined;
export const registerCommunicationChartConsumer = (
    session: Session, data: AnalysisChartData, highlight: (target: SlowRankTarget) => void,
): (() => void) => {
    const consumer = { session, data, highlight };
    chartConsumer = consumer;
    notifyState();
    return () => {
        if (chartConsumer === consumer) {
            chartConsumer = undefined;
            notifyState();
        }
    };
};

export const setCommunicationAgentSession = (session: Session | undefined): void => {
    if (activeSession !== session) {
        pageState = undefined;
        setCommunicationAgentState(undefined);
    }
    activeSession = session;
    notifyState();
};

export const updateCommunicationPageState = (state: CommunicationPageState): void => {
    pageState = {
        analysisType: state.analysisType,
        scope: scopeOf(state.scope, true),
        selection: selectionOf(state.selection),
    };
    notifyState();
};

export const clearCommunicationPageState = (): void => {
    pageState = undefined;
    notifyState();
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

const checkCommand = (context: CommandContext): void => {
    if (context.signal.aborted) throw context.signal.reason ?? new Error('Communication command was cancelled.');
    if (Date.now() >= context.deadline) throw new Error('Communication command exceeded its deadline.');
};

const waitForState = <T>(read: () => T | undefined, context: CommandContext): Promise<T> => new Promise((resolve, reject) => {
    const cleanup = (): void => {
        stateListeners.delete(check);
        context.signal.removeEventListener('abort', check);
    };
    const check = (): void => {
        try {
            checkCommand(context);
            const value = read();
            if (value === undefined) return;
            cleanup();
            resolve(value);
        } catch (error) {
            cleanup();
            reject(error);
        }
    };
    stateListeners.add(check);
    context.signal.addEventListener('abort', check, { once: true });
    check();
});

const withAnalysis = async <T>(
    type: AnalysisType, context: CommandContext, slowRank: boolean,
    execute: (state: CommunicationState, context: CommandContext) => Promise<T> | T,
): Promise<T> => {
    checkCommand(context);
    const session = activeSession;
    if (!session?.selectedClusterPath || !session.clusterCompleted ||
        (type === AnalysisType.COMMUNICATION_DURATION_ANALYSIS && !session.durationFileCompleted)) {
        throw new Error('Communication analysis data is unavailable or has not been parsed.');
    }
    if (slowRank && session.isCompare) throw new Error('Slow-rank actions are unavailable in comparison mode.');
    const clusterPath = session.selectedClusterPath;
    const compare = session.isCompare;
    const abort = new AbortController();
    const forwardAbort = (): void => abort.abort(context.signal.reason ?? new Error('Communication command was cancelled.'));
    context.signal.addEventListener('abort', forwardAbort, { once: true });
    const preparedContext = { ...context, signal: abort.signal };
    let pageObserved = false;
    let expectedConditions: ConditionDataType | undefined;
    let executionState: CommunicationState | undefined;
    const validate = (): void => {
        if (activeSession !== session || session.selectedClusterPath !== clusterPath || session.isCompare !== compare ||
            !session.clusterCompleted || (type === AnalysisType.COMMUNICATION_DURATION_ANALYSIS && !session.durationFileCompleted) ||
            (pageObserved && (!pageState || !currentState)) ||
            (expectedConditions !== undefined && (!currentState || currentState.filterLoading === true || pageState?.analysisType !== type ||
                currentState.conditions.type !== type || !sameScope(currentState.conditions, expectedConditions) ||
                !sameScope(currentState.conditions, pageState.scope))) ||
            (executionState !== undefined && (currentState !== executionState || pageState?.analysisType !== type ||
                !sameScope(executionState.conditions, pageState.scope)))) {
            abort.abort(new Error('The Communication context changed during the action.'));
        }
    };
    // 数据只允许在同一会话和筛选内变为就绪；取消不回滚已提交的视图。
    stateListeners.add(validate);
    const disposeReaction = reaction(() => [session.selectedClusterPath, session.isCompare,
        session.clusterCompleted, session.durationFileCompleted, session.unitcount], () => {
        validate();
        notifyState();
    });
    const timeout = window.setTimeout(() => abort.abort(new Error('Communication command exceeded its deadline.')), Math.max(0, context.deadline - Date.now()));
    try {
        const initial = await waitForState(() => currentState && pageState && !currentState.filterLoading ? currentState : undefined, preparedContext);
        pageObserved = true;
        validate();
        checkCommand(preparedContext);
        let conditions = initial.conditions;
        const switching = conditions.type !== type;
        if (switching) conditions = await prepareAnalysisType(type, preparedContext);
        checkCommand(preparedContext);
        if (conditions.type !== type) throw new Error('The requested Communication analysis is unavailable.');
        if (!switching && (pageState?.analysisType !== type || !sameScope(conditions, pageState.scope))) {
            throw new Error('The Communication page context is unavailable.');
        }
        const state = await waitForState(() => {
            const value = currentState;
            // Filter 完成先于父提交；首次匹配后锁定条件，后续用户修改不能漂移到新数据。
            if (!value || value.filterLoading === true || value.conditions.type !== type || pageState?.analysisType !== type ||
                !sameScope(value.conditions, conditions) || !sameScope(value.conditions, pageState.scope)) return undefined;
            expectedConditions ??= { ...conditions };
            if (!slowRank) return value;
            if (value.adviceStatus === 'error' || value.chartStatus === 'error') throw new Error('Failed to load current Communication advice or chart data.');
            if (value.adviceStatus === 'pending') return undefined;
            if (value.isCompare) throw new Error('Slow-rank actions are unavailable in comparison mode.');
            if ((observeSlowRankAdvice(session, pageState) as { status: string }).status !== 'ready') {
                throw new Error('No current slow-rank advice is available for the current Communication context.');
            }
            if (value.chartStatus === 'pending') return undefined;
            const source = value.chartContext;
            if (!source || source.conditions.type !== type || source.session !== session || source.clusterPath !== clusterPath || source.isCompare !== compare ||
                source.result !== value.chart || !sameScope(source.conditions, value.conditions)) {
                throw new Error('No current chart data is available for the Communication context.');
            }
            return value;
        }, preparedContext);
        executionState = state;
        validate();
        checkCommand(preparedContext);
        return await execute(state, preparedContext);
    } finally {
        abort.abort(new Error('Communication action ended.'));
        clearTimeout(timeout);
        disposeReaction();
        stateListeners.delete(validate);
        context.signal.removeEventListener('abort', forwardAbort);
    }
};

const switchAnalysis: CommandHandler = (args, context) => {
    if (args.type !== AnalysisType.COMMUNICATION_MATRIX && args.type !== AnalysisType.COMMUNICATION_DURATION_ANALYSIS) {
        throw new Error('type must be CommunicationMatrix or CommunicationDurationAnalysis.');
    }
    return withAnalysis(args.type, context, false, state => ({ analysisType: state.conditions.type }));
};

const highlightSlowRank: CommandHandler = (args, context) => {
    if (typeof args.rankId !== 'string' || !args.rankId || (args.operatorName !== undefined && (typeof args.operatorName !== 'string' || !args.operatorName))) {
        throw new Error('rankId must be a non-empty string and operatorName must be a non-empty string.');
    }
    return withAnalysis(AnalysisType.COMMUNICATION_DURATION_ANALYSIS, context, true,
        (state, preparedContext) => highlightPreparedTarget(args, state, preparedContext));
};

const highlightPreparedTarget = async (args: JsonObject, state: CommunicationState, context: CommandContext): Promise<JsonObject> => {
    const rank = state.slowRanks?.data.find(item => item.rankId === args.rankId);
    const operator = rank?.opList.find(item => args.operatorName === undefined || item.name === args.operatorName);
    if (!operator || !rank) throw new Error('Slow rank or operator is not in the current advice list.');
    if (!state.chart.data?.some(item => item.rankId === rank.rankId)) {
        throw new Error('The slow rank is not available in the current communication chart.');
    }
    const consumer = await waitForState(() => chartConsumer?.session === activeSession && chartConsumer?.data === state.chart ? chartConsumer : undefined, context);
    checkCommand(context);
    if (chartConsumer !== consumer || currentState !== state ||
        (observeSlowRankAdvice(activeSession, pageState) as { status: string }).status !== 'ready') {
        throw new Error('The Communication data or chart changed before highlighting.');
    }
    consumer.highlight({
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
    const rankId = args.rankId;
    const name = args.operatorName;
    return withAnalysis(AnalysisType.COMMUNICATION_DURATION_ANALYSIS, context, true,
        (state, preparedContext) => locatePreparedTarget(rankId, name, state, preparedContext));
};

const locatePreparedTarget = async (rankId: string, name: string, state: CommunicationState, context: CommandContext): Promise<JsonObject> => {
    const session = activeSession;
    if (state.unitcount <= 0 || !session || session.unitcount <= 0) throw new Error('Timeline data is unavailable.');
    const operator = state.slowRanks?.data.find(item => item.rankId === rankId)?.opList.find(item => item.name === name);
    const dbPath = state.chart.data?.find(item => item.rankId === rankId)?.dbPath;
    if (!operator || !dbPath) throw new Error('The specified slow rank or operator is not available in the current advice and chart.');
    const params = { name, rankId, dbPath };
    // 同时保存目标身份和值，原地修改也不能复用旧查询结果。
    const targetSnapshot = JSON.stringify(operator);
    const scopeSnapshot = JSON.stringify(state.conditions);
    const clusterPath = session.selectedClusterPath;
    const detail = await withAbortSignal(queryTimelineUnitKernelDetail(params), context.signal, () => new Error('Timeline navigation was cancelled.'));
    checkCommand(context);
    if (activeSession !== session || session.selectedClusterPath !== clusterPath || currentState !== state ||
        (observeSlowRankAdvice(session, pageState) as { status: string }).status !== 'ready' ||
        JSON.stringify(state.conditions) !== scopeSnapshot || state.unitcount <= 0 || session.unitcount <= 0 ||
        state.slowRanks?.data.find(item => item.rankId === rankId)?.opList.find(item => item.name === name) !== operator ||
        JSON.stringify(operator) !== targetSnapshot ||
        state.chart.data?.find(item => item.rankId === rankId)?.dbPath !== dbPath) {
        throw new Error('The communication context or target changed during Timeline navigation.');
    }
    const backendError = (detail as unknown as { error?: { code?: unknown; message?: unknown } })?.error;
    if (backendError && typeof backendError.message === 'string') {
        const backendCode = typeof backendError.code === 'number' || (typeof backendError.code === 'string' &&
            /^[\w.-]{1,80}$/.test(backendError.code))
            ? backendError.code
            : undefined;
        throw new CommandError({
            code: COMMAND_ERROR_CODES.EXECUTION_FAILED,
            message: safeBackendMessage(backendError.message),
            retryable: false,
            ...(backendCode === undefined ? {} : { details: { backendCode } }),
        });
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

const safeBackendMessage = (message: string): string => {
    if (/<\s*(?:html|body|script|!doctype)\b/i.test(message)) return 'Timeline backend query failed.';
    return message.slice(0, 4096)
        .replace(/\bBearer\s+[\w.~+/-]+=*/gi, 'Bearer [REDACTED]')
        .replace(/(\b(?:authorization|proxy-authorization)\b["']?\s*[:=]\s*)(?:basic|bearer|digest|ntlm)\s+[A-Za-z0-9._~+/=-]+/gi, '$1[REDACTED]')
        .replace(/(\b(?:token|secret|password|api[_-]?key|authorization)\b["']?\s*[:=]\s*)("(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[^\s,}]+)/gi, '$1[REDACTED]')
        .replace(/\b[a-z][a-z0-9+.-]*:\/\/[^/\s@]+@/gi, '[REDACTED]@')
        .replace(/\b[A-Za-z]:[\\/][^\s"'<>]+|(?:^|\s)\/[^\s"'<>]+/g, ' [path]')
        .replace(/\p{Cc}/gu, ' ').slice(0, 600) || 'Timeline backend query failed.';
};

export const registerCommunicationCommands = (client: ModuleAgentCommandClient): (() => void) => {
    const unregister = [
        client.registerCommand({
            name: 'Communication.switchAnalysis',
            uiEffect: 'workspace',
            title: 'Switch communication analysis',
            description: 'Switch between Communication Matrix and Communication Duration Analysis. Completes after normalized filters and the analysis view are committed, not after all data loads.',
            inputSchema: {
                type: 'object',
                properties: { type: { type: 'string', enum: [AnalysisType.COMMUNICATION_MATRIX, AnalysisType.COMMUNICATION_DURATION_ANALYSIS] } },
                required: ['type'],
                additionalProperties: false,
            },
        }, switchAnalysis),
        client.registerCommand({
            name: 'Communication.highlightSlowRank',
            uiEffect: 'workspace',
            title: 'Highlight a slow-rank operator',
            description: 'Automatically open Communication Duration Analysis and highlight an operator from current valid slow-rank advice after its data and chart are ready. Defaults to the first advised operator on that rank.',
            inputSchema: {
                type: 'object',
                properties: { rankId: { type: 'string', minLength: 1 }, operatorName: { type: 'string', minLength: 1 } },
                required: ['rankId'],
                additionalProperties: false,
            },
        }, highlightSlowRank),
        client.registerCommand({
            name: 'Communication.locateInTimeline',
            uiEffect: 'workspace',
            title: 'Locate slow-rank operator in Timeline',
            description: 'Automatically prepare Communication Duration Analysis, then open Timeline at the explicitly specified operator from current valid slow-rank advice (parsed cluster and duration, non-compare, Timeline data available). No prior highlight is required. Navigation selects the exact operator and loads its available connections using the chart-click behavior.',
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
