/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
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
                // A copied pinned lane can display the same underlying slice twice.
                const key = JSON.stringify([meta.dbPath, meta.cardId, meta.processId, meta.threadId, meta.metaType, slice.id, slice.startTime, slice.duration, slice.depth]);
                const existing = slices.get(key);
                if (!existing || top < existing.top) slices.set(key, { key, source, slice, top });
            }
        }
    }
    return {
        revision: JSON.stringify(revisions),
        slices: [...slices.values()].sort((a, b) => a.top - b.top || a.slice.startTime - b.slice.startTime || a.key.localeCompare(b.key)),
        loading,
        sourceCount,
    };
};
