/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { COMMAND_ERROR_CODES, CommandError, type CommandContext, type CommandDefinition, type JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { StackStatusData } from '../entity/chart';
import type { ThreadMetaData } from '../entity/data';
import type { InsightUnit } from '../entity/insight';
import type { Session } from '../entity/session';
import { getTimeOffset } from '../insight/units/utils';

export interface VisibleSliceSource {
    unit: InsightUnit;
    element: HTMLElement;
    data: StackStatusData[][];
    domainStart: number;
    domainEnd: number;
    rowHeight: number;
    ready: boolean;
    scope: string;
    select: (slice: StackStatusData) => void;
}

export interface VisibleSlice {
    key: string;
    source: VisibleSliceSource;
    slice: StackStatusData;
    top: number;
}

interface Bounds { left: number; right: number; top: number; bottom: number }
const sources = new WeakMap<Session, Map<number, VisibleSliceSource>>();
let sourceId = 0;

export const visibleSliceScope = (session: Session, unit: InsightUnit): string => {
    const metadata = unit.metadata as ThreadMetaData;
    return JSON.stringify([
        session.phase, session.endTimeAll, session.isNsMode, session.domainRange, session.alignRender,
        session.unitsConfig.filterConfig.pythonFunction, session.autoAdjustUnitHeight, session.areFlagEventsHidden,
        metadata.cardId, metadata.dbPath, metadata.processId, metadata.threadId, metadata.metaType,
        getTimeOffset(session, metadata),
    ]);
};

export const registerVisibleSliceSource = (session: Session, source: VisibleSliceSource): (() => void) => {
    let registered = sources.get(session);
    if (!registered) {
        registered = new Map();
        sources.set(session, registered);
    }
    const id = ++sourceId;
    registered.set(id, source);
    return () => { registered?.delete(id); };
};

const visibleBounds = (element: HTMLElement): Bounds | undefined => {
    if (!element.isConnected) return undefined;
    const bounds = { left: 0, top: 0, right: window.innerWidth, bottom: window.innerHeight };
    let current: HTMLElement | null = element;
    while (current) {
        const style = getComputedStyle(current);
        if (style.display === 'none' || style.visibility === 'hidden' || style.visibility === 'collapse' || style.opacity === '0') return undefined;
        const rect = current.getBoundingClientRect();
        if (current === element || /auto|scroll|hidden|clip/.test(style.overflowX || style.overflow)) {
            bounds.left = Math.max(bounds.left, rect.left);
            bounds.right = Math.min(bounds.right, rect.right);
        }
        if (current === element || /auto|scroll|hidden|clip/.test(style.overflowY || style.overflow)) {
            bounds.top = Math.max(bounds.top, rect.top);
            bounds.bottom = Math.min(bounds.bottom, rect.bottom);
        }
        current = current.parentElement;
    }
    return bounds.right > bounds.left && bounds.bottom > bounds.top ? bounds : undefined;
};

export const readVisibleSlices = (session: Session): { revision: string; slices: VisibleSlice[]; loading: boolean; sourceCount: number } => {
    const { domainStart, domainEnd } = session.domainRange;
    const revisions: unknown[] = [domainStart, domainEnd, session.isNsMode];
    const slices = new Map<string, VisibleSlice>();
    let loading = false;
    let sourceCount = 0;
    for (const [id, source] of sources.get(session) ?? []) {
        const { unit, element } = source;
        if (!unit.isDisplay || !unit.isUnitVisible || unit.isMultiDeviceHidden) continue;
        const bounds = visibleBounds(element);
        if (!bounds) continue;
        sourceCount++;
        const rect = element.getBoundingClientRect();
        const ready = source.ready && !unit.isTraceLoading && source.domainStart === domainStart && source.domainEnd === domainEnd &&
            source.scope === visibleSliceScope(session, unit);
        revisions.push([id, bounds, rect.top, rect.left, rect.width, rect.height, ready]);
        if (!ready) {
            loading = true;
            continue;
        }
        const meta = unit.metadata as ThreadMetaData;
        for (const row of source.data) {
            for (const slice of row) {
                if (!slice.id || !Number.isFinite(slice.startTime) || !Number.isFinite(slice.duration) || !Number.isFinite(slice.depth)) continue;
                const end = slice.duration < 0 ? domainEnd : slice.startTime + slice.duration;
                if (slice.duration === 0 ? slice.startTime < domainStart || slice.startTime > domainEnd : end <= domainStart || slice.startTime >= domainEnd) continue;
                const top = rect.top + slice.depth * source.rowHeight;
                if (top + source.rowHeight <= bounds.top || top >= bounds.bottom) continue;
                const x = rect.left + (slice.startTime - domainStart) / (domainEnd - domainStart) * rect.width;
                const right = Math.max(x + 1, rect.left + (end - domainStart) / (domainEnd - domainStart) * rect.width);
                if (right <= bounds.left || x >= bounds.right) continue;
                // 内容变化也参与原有 revision，使用注册来源编号而非数据库路径。
                revisions.push([id, slice.id, slice.name, slice.startTime, slice.duration, slice.depth, slice.threadId, meta.threadName]);
                // A copied pinned lane can display the same underlying slice twice.
                const key = JSON.stringify([meta.dbPath, meta.cardId, meta.processId, meta.threadId, meta.metaType, slice.id, slice.startTime, slice.duration, slice.depth]);
                const existing = slices.get(key);
                if (!existing || top < existing.top) slices.set(key, { key, source, slice, top });
            }
        }
    }
    const ordered = [...slices.values()].sort((a, b) => a.top - b.top || a.slice.startTime - b.slice.startTime || a.key.localeCompare(b.key));
    return {
        revision: JSON.stringify(revisions),
        slices: ordered,
        loading,
        sourceCount,
    };
};

export const listVisibleSlicesDefinition: CommandDefinition = {
    name: 'Timeline.listVisibleSlices',
    title: 'List visible timeline operators',
    description: 'Read only currently rendered, visible operators, deduplicating normal and pinned copies. Not an imported-data search. ' +
        'Returns at most 50 summaries (default 20). For offset > 0, pass the revision from the first page; restart at offset 0 if stale. ' +
        'loading means the visible data is incomplete. sourceRef identifies a source lane, not a selectSlice candidateRef. No selection or viewport changes.',
    inputSchema: {
        type: 'object',
        properties: {
            offset: { type: 'integer', minimum: 0, default: 0 },
            limit: { type: 'integer', minimum: 1, maximum: 50, default: 20 },
            revision: { type: 'string', minLength: 1 },
        },
        additionalProperties: false,
    },
};

const sourceRefs = new WeakMap<Session, Map<string, string>>();
let sourceRefId = 0;
const summarize = (session: Session, { source, slice }: VisibleSlice): JsonObject => {
    const meta = source.unit.metadata as ThreadMetaData;
    let refs = sourceRefs.get(session);
    if (!refs) { refs = new Map(); sourceRefs.set(session, refs); }
    // 合并卡片可能包含多个数据库；不暴露私有路径，也不混淆不同来源的同名泳道。
    const key = JSON.stringify([meta.cardId, meta.dbPath, meta.processId, meta.threadId, meta.metaType]);
    let sourceRef = refs.get(key);
    if (!sourceRef) { sourceRef = `visible-source-${++sourceRefId}`; refs.set(key, sourceRef); }
    return {
        sourceRef,
        id: slice.id ?? null,
        cardId: meta.cardId,
        processId: meta.processId ?? null,
        threadId: slice.threadId ?? meta.threadId,
        metaType: meta.metaType,
        name: slice.name,
        lane: meta.threadName ?? meta.threadId,
        startTime: slice.startTime,
        duration: slice.duration,
        depth: slice.depth,
    };
};

const invalid = (message: string): CommandError => new CommandError({ code: COMMAND_ERROR_CODES.INVALID, message, retryable: false });
const validate = (args: JsonObject): void => {
    if (Object.keys(args).some(key => !['offset', 'limit', 'revision'].includes(key)) ||
        ('offset' in args && (typeof args.offset !== 'number' || !Number.isSafeInteger(args.offset) || args.offset < 0)) ||
        ('limit' in args && (typeof args.limit !== 'number' || !Number.isSafeInteger(args.limit) || args.limit < 1 || args.limit > 50)) ||
        ('revision' in args && (typeof args.revision !== 'string' || !args.revision.trim())) ||
        (typeof args.offset === 'number' && args.offset > 0 && !('revision' in args))) {
        throw invalid('Provide offset >= 0, limit from 1 to 50 and a nonempty revision; offset > 0 requires the first page revision.');
    }
};

export const listVisibleSlices = (args: JsonObject, context: CommandContext, session: Session, isCurrent: () => boolean): JsonObject => {
    validate(args);
    const check = (): void => {
        if (context.signal.aborted) throw new CommandError({ code: COMMAND_ERROR_CODES.CANCELLED, message: 'Visible slice listing was cancelled.', retryable: false });
        if (Date.now() >= context.deadline) throw new CommandError({ code: COMMAND_ERROR_CODES.TIMEOUT, message: 'Visible slice listing timed out.', retryable: false });
        if (!isCurrent()) throw new CommandError({ code: COMMAND_ERROR_CODES.UNAVAILABLE, message: 'The active Timeline session changed.', retryable: true });
    };
    check();
    const visible = readVisibleSlices(session);
    if (args.revision !== undefined && args.revision !== visible.revision) {
        throw invalid('The visible slice revision is stale. Restart listing at offset 0 without revision.');
    }
    const offset = (args.offset as number | undefined) ?? 0;
    const limit = (args.limit as number | undefined) ?? 20;
    const slices = visible.slices.slice(offset, offset + limit).map(slice => summarize(session, slice));
    check();
    return {
        revision: visible.revision,
        totalCount: visible.slices.length,
        loading: visible.loading,
        sourceCount: visible.sourceCount,
        offset,
        limit,
        hasMore: offset + limit < visible.slices.length,
        timeUnit: session.isNsMode ? 'ns' : 'ms',
        slices,
    };
};

export const summarizeVisibleSlices = (session: Session): JsonObject => {
    const { slices, revision, loading, sourceCount } = readVisibleSlices(session);
    return { count: slices.length, loading, sourceCount, revision };
};
