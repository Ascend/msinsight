/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import { useEffect, useState } from 'react';

export interface SummaryParams {
    clusterPath: string;
    step: string;
}

export const timeFields = [
    'computingNotOverlapped', 'communicationNotOverlapped',
    'computingCommunicationOverlapped', 'free', 'preparing',
] as const;
export const communicationFields = ['transit', 'wait', 'synchronization', 'free'] as const;

export type TimeField = typeof timeFields[number];
type TimeSegments = Record<TimeField, number>;
export interface RankTime {
    rankId: string;
    time: TimeSegments & { total: number; computing: number; communication: number };
    ratio: { compute: number; communication: number };
}
export interface CommunicationTime {
    rankId: string;
    time: Record<typeof communicationFields[number], number>;
}
export interface SummaryResponses {
    clusterTime: { units: { time: string }; time: TimeSegments };
    rankTime: { units: { time: string; ratio: 'percent' }; ranks: RankTime[] };
    communicationTime: { units: { time: string }; ranks: CommunicationTime[] };
    clusterBandwidth: {
        units: { bandwidth: string };
        transportTypes: Array<{ transportType: string; bandwidth: number }>;
    };
}
export type SummaryKind = keyof SummaryResponses;
export type SummaryResponse = SummaryResponses[SummaryKind];

function object(value: unknown): Record<string, unknown> {
    if (value === null || typeof value !== 'object' || Array.isArray(value)) {
        throw new Error('Invalid summary response');
    }
    return value as Record<string, unknown>;
}

function text(value: unknown): void {
    if (typeof value !== 'string' || value.trim() === '') {
        throw new Error('Missing summary label or unit');
    }
}

function metrics(value: unknown, fields: readonly string[]): void {
    const record = object(value);
    fields.forEach(field => {
        const metric = record[field];
        if (typeof metric !== 'number' || !Number.isFinite(metric) || metric < 0) {
            throw new Error(`Invalid summary metric: ${field}`);
        }
    });
}

// Reject malformed payloads instead of silently rendering missing metrics as zero.
export function parseSummaryResponse<K extends SummaryKind>(kind: K, value: unknown): SummaryResponses[K] {
    const data = object(value);
    const units = object(data.units);
    text(units[kind === 'clusterBandwidth' ? 'bandwidth' : 'time']);
    if (kind === 'clusterTime') {
        metrics(data.time, timeFields);
    } else {
        const rows = kind === 'clusterBandwidth' ? data.transportTypes : data.ranks;
        if (!Array.isArray(rows)) {
            throw new Error('Missing summary rows');
        }
        const ids = new Set<string>();
        rows.forEach(value => {
            const row = object(value);
            const id = kind === 'clusterBandwidth' ? row.transportType : row.rankId;
            text(id);
            if (ids.has(id as string)) {
                throw new Error('Duplicate summary row');
            }
            ids.add(id as string);
            if (kind === 'clusterBandwidth') {
                metrics(row, ['bandwidth']);
            } else if (kind === 'communicationTime') {
                metrics(row.time, communicationFields);
            } else {
                metrics(row.time, [...timeFields, 'total', 'computing', 'communication']);
                metrics(row.ratio, ['compute', 'communication']);
                if (Object.values(object(row.ratio)).some(value => typeof value === 'number' && value > 100)) {
                    throw new Error('Invalid summary percentage');
                }
            }
        });
        if (kind === 'rankTime' && units.ratio !== 'percent') {
            throw new Error('Invalid summary ratio unit');
        }
    }
    return value as SummaryResponses[K];
}

interface QueryState {
    key: string;
    status: 'loading' | 'success' | 'error';
    data?: SummaryResponse;
}

export function useSummaryData(
    kind: SummaryKind, params: SummaryParams, ready: boolean,
): { status: 'idle' | QueryState['status']; data?: SummaryResponse; retry: () => void } {
    const key = JSON.stringify([kind, params.clusterPath, params.step]);
    const [state, setState] = useState<QueryState>();
    const [attempt, setAttempt] = useState(0);
    useEffect(() => {
        if (!ready || !params.clusterPath) {
            return;
        }
        const controller = new AbortController();
        setState({ key, status: 'loading' });
        window.requestData(`summary/${kind}`, { clusterPath: params.clusterPath, step: params.step }, undefined, { signal: controller.signal })
            .then(response => parseSummaryResponse(kind, response)).then(
                data => { if (!controller.signal.aborted) { setState({ key, status: 'success', data }); } },
                error => { if (!controller.signal.aborted && error?.name !== 'AbortError') { setState({ key, status: 'error' }); } },
            );
        return () => controller.abort();
    }, [key, kind, params.clusterPath, params.step, ready, attempt]);
    if (!ready || !params.clusterPath) {
        return { status: 'idle', retry: () => setAttempt(value => value + 1) };
    }
    return {
        ...(state?.key === key ? state : { status: 'loading' as const }),
        retry: () => setAttempt(value => value + 1),
    };
}
