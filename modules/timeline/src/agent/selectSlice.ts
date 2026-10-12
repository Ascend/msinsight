/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { COMMAND_ERROR_CODES, CommandError, type CommandContext, type CommandDefinition, type JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ThreadMetaData } from '../entity/data';
import type { Session } from '../entity/session';
import { readVisibleSlices, type VisibleSlice } from './visibleSlices';

export const selectSliceDefinition: CommandDefinition = {
    name: 'Timeline.selectSlice',
    title: 'View or select a visible timeline operator (查看、选中算子)',
    description: 'Use when the user wants to view, inspect, show, locate or select a named operator on the current Timeline screen, ' +
        'for example "查看 MatMul 算子", "看看 MatMul", "定位 MatMul 算子" or "选中 MatMul 算子". ' +
        'Pass only the operator name as name, e.g. {"name":"MatMul"}. Questions about what an operator means or how it works do not imply selection. ' +
        'Selects an operator already rendered within the current screen: visible expanded lanes and the current time viewport, including pinned lanes. ' +
        'Uses the chart-click selection behavior to show its details and keeps the viewport unchanged. ' +
        'Supply name to find matches; one match is selected, multiple matches return needsChoice and candidates without changing selection. ' +
        'Show the candidate names, cards, lanes, start times and durations to the user and ask which to select; never choose the first arbitrarily. ' +
        'After the user chooses, supply only that candidateRef. References expire when the viewport, layout or rendered data changes. ' +
        'offset/limit paginate candidates. Matching is exact and case-sensitive by default. No global search or automatic lane expansion. ' +
        'Observe after selection to verify the selected operator before reporting success. Timeline.focus only frames an existing selection; it does not find a named operator.',
    inputSchema: {
        type: 'object',
        properties: {
            name: { type: 'string', minLength: 1 },
            candidateRef: { type: 'string', minLength: 1 },
            cardId: { type: 'string', minLength: 1 },
            isMatchCase: { type: 'boolean', default: true },
            isMatchExact: { type: 'boolean', default: true },
            offset: { type: 'integer', minimum: 0, default: 0 },
            limit: { type: 'integer', minimum: 1, maximum: 50, default: 20 },
        },
        oneOf: [{ required: ['name'] }, { required: ['candidateRef'] }],
        additionalProperties: false,
    },
};

interface CandidateSnapshot {
    revision: string;
    byRef: Map<string, VisibleSlice>;
    byKey: Map<string, string>;
}
const snapshots = new WeakMap<Session, CandidateSnapshot>();
let referenceId = 0;
const error = (code: string, message: string, retryable = false): CommandError => new CommandError({ code, message, retryable });

const validateArgs = (args: JsonObject): void => {
    if ('candidateRef' in args) {
        if (Object.keys(args).length === 1 && typeof args.candidateRef === 'string' && args.candidateRef.trim()) return;
        throw error(COMMAND_ERROR_CODES.INVALID, 'To confirm a choice, provide only candidateRef returned by the current candidate list.');
    }
    const allowed = ['name', 'cardId', 'isMatchCase', 'isMatchExact', 'offset', 'limit'];
    if (Object.keys(args).some(key => !allowed.includes(key)) || typeof args.name !== 'string' || !args.name.trim() ||
        ('cardId' in args && (typeof args.cardId !== 'string' || !args.cardId.trim())) ||
        ['isMatchCase', 'isMatchExact'].some(key => key in args && typeof args[key] !== 'boolean') ||
        ('offset' in args && (typeof args.offset !== 'number' || !Number.isSafeInteger(args.offset) || args.offset < 0)) ||
        ('limit' in args && (typeof args.limit !== 'number' || !Number.isSafeInteger(args.limit) || args.limit < 1 || args.limit > 50))) {
        throw error(COMMAND_ERROR_CODES.INVALID, 'Provide name with optional cardId, boolean match options and offset/limit, or only candidateRef to confirm a choice.');
    }
};

const select = (target: VisibleSlice, totalCount: number): JsonObject => {
    target.source.select(target.slice);
    return { status: 'selected', needsChoice: false, totalCount };
};

export const selectSlice = async (
    args: JsonObject, context: CommandContext, session: Session, isCurrent: () => boolean,
): Promise<JsonObject> => {
    validateArgs(args);
    if (context.signal.aborted) throw error(COMMAND_ERROR_CODES.CANCELLED, 'Slice selection was cancelled.');
    if (Date.now() >= context.deadline) throw error(COMMAND_ERROR_CODES.TIMEOUT, 'Slice selection timed out.');
    if (!isCurrent()) throw error(COMMAND_ERROR_CODES.UNAVAILABLE, 'The active Timeline session changed.');
    if (session.selectedRangeIsLock) throw error(COMMAND_ERROR_CODES.UNAVAILABLE, 'Unlock the current selection before selecting a slice.');
    if (session.locateUnit !== undefined) throw error(COMMAND_ERROR_CODES.BUSY, 'Another slice navigation is in progress.');
    const visible = readVisibleSlices(session);
    if (visible.loading || visible.sourceCount === 0) {
        snapshots.delete(session);
        throw error(COMMAND_ERROR_CODES.UNAVAILABLE, 'Visible operator lanes are loading or unavailable. Expand a lane on screen and wait for it to render.', true);
    }
    let snapshot = snapshots.get(session);
    if (!snapshot || snapshot.revision !== visible.revision) {
        snapshot = { revision: visible.revision, byRef: new Map(), byKey: new Map() };
        snapshots.set(session, snapshot);
    }
    if (typeof args.candidateRef === 'string') {
        const target = snapshot.byRef.get(args.candidateRef);
        if (!target || !visible.slices.some(item => item.key === target.key)) {
            throw error(COMMAND_ERROR_CODES.NOT_FOUND, 'The candidate reference is stale or unknown. Search the current screen again and ask the user to choose.');
        }
        return select(target, 1);
    }
    const matchCase = args.isMatchCase !== false;
    const name = matchCase ? args.name as string : (args.name as string).toLowerCase();
    const matches = visible.slices.filter(item => {
        const metadata = item.source.unit.metadata as ThreadMetaData;
        if (args.cardId !== undefined && metadata.cardId !== args.cardId) return false;
        const candidateName = matchCase ? item.slice.name : item.slice.name.toLowerCase();
        return args.isMatchExact === false ? candidateName.includes(name) : candidateName === name;
    });
    const offset = (args.offset as number | undefined) ?? 0;
    const limit = (args.limit as number | undefined) ?? 20;
    if (matches.length === 0) {
        throw error(COMMAND_ERROR_CODES.NOT_FOUND, 'No matching operator is visible on the current screen.');
    }
    if (matches.length === 1 && offset === 0) return select(matches[0], 1);
    return {
        status: 'needsChoice',
        needsChoice: true,
        totalCount: matches.length,
        offset,
        hasMore: offset + limit < matches.length,
        timeUnit: session.isNsMode ? 'ns' : 'ms',
        candidates: matches.slice(offset, offset + limit).map((target, index) => {
            let ref = snapshot.byKey.get(target.key);
            if (!ref) {
                ref = `visible-slice-${++referenceId}`;
                snapshot.byKey.set(target.key, ref);
                snapshot.byRef.set(ref, target);
            }
            const metadata = target.source.unit.metadata as ThreadMetaData;
            return {
                candidateRef: ref,
                index: offset + index + 1,
                name: target.slice.name,
                cardId: metadata.cardId,
                lane: metadata.threadName,
                processId: metadata.processId ?? null,
                threadId: metadata.threadId,
                startTime: target.slice.startTime,
                duration: target.slice.duration,
                depth: target.slice.depth,
            };
        }),
    };
};
