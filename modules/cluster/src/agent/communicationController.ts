/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import type { ObservationData } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import type { Session } from '../entity/session';

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

export const setCommunicationAgentSession = (session: Session | undefined): void => {
    if (activeSession !== session) pageState = undefined;
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

export const registerCommunicationCommands = (client: ModuleAgentCommandClient): (() => void) => {
    // Read-only context uses observe; add real client.registerCommand calls here when available.
    void client;
    const unregister: Array<() => void> = [];
    return () => unregister.forEach(stop => stop());
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
