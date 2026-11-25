/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
import { useEffect, useState } from 'react';
import { runInAction } from 'mobx';
import type { Session } from '../../entity/session';
import type { InsightUnit } from '../../entity/insight';
import { emptyPmuResponse, getPmuSourceIdentityKey, type PmuCounterRequest, type PmuCounterResponse, type PmuLaneMetadata } from '../../entity/pmuCounter';
import { createPmuRequestKey, PmuRequestCancelled } from '../../cache/pmuCounterCache';
import { getTimeOffset } from '../../insight/units/offset';
import { buildPmuCounterRequest } from './pmuCounterUtils';

function emptyData(metadata: PmuLaneMetadata): PmuCounterResponse {
    return {
        ...emptyPmuResponse(),
        counterId: metadata.pmu.counterId ?? '',
        nativeUnit: metadata.pmu.unit ?? 'unknown',
    };
}

export async function fetchPmuCounterData(session: Session, metadata: PmuLaneMetadata, width: number): Promise<PmuCounterResponse> {
    if (width <= 0 || metadata.pmu.hasSamples === false) { return emptyData(metadata); }
    const { domainStart, domainEnd } = session.domainRange;
    const params = buildPmuCounterRequest(metadata, domainStart, domainEnd, getTimeOffset(session, metadata), width);
    if (params.endTime <= params.startTime) { return emptyData(metadata); }
    return session.pmuCounterCache.fetch(metadata.dataSource, params);
}

interface PmuDataState {
    key: string;
    identity: string;
    data: PmuCounterResponse;
    loading: boolean;
    error: boolean;
}

function retainedData(state: PmuDataState, identity: string, metadata: PmuLaneMetadata): PmuCounterResponse {
    return state.identity === identity && state.data.status === 'ok' ? state.data : emptyData(metadata);
}

/** A PMU request is tied to its complete source, range and generation. */
export function usePmuCounterData(session: Session, metadata: PmuLaneMetadata, unit: InsightUnit, width: number):
{ data: PmuCounterResponse; loading: boolean; error: boolean; retry: () => void; offset: number } {
    const offset = getTimeOffset(session, metadata);
    const { domainStart, domainEnd } = session.domainRange;
    const [attempt, setAttempt] = useState(0);
    const visible = unit.isUnitVisible && unit.isDisplay && width > 0;
    let params: PmuCounterRequest | undefined;
    try {
        params = buildPmuCounterRequest(metadata, domainStart, domainEnd, offset, width);
    } catch {
        // An invalid range affects this lane only; it must not crash the timeline.
    }
    const identity = JSON.stringify([getPmuSourceIdentityKey(metadata), metadata.dbPath, metadata.pmu.counterId,
        session.pmuCounterGeneration, session.pmuCounterCache.generation]);
    const key = JSON.stringify([identity, params ? createPmuRequestKey(metadata.dataSource, params) : 'invalid_range',
        offset, visible, metadata.pmu.hasSamples, attempt]);
    const [state, setState] = useState<PmuDataState>({ key: '', identity, data: emptyData(metadata), loading: false, error: false });
    const canRequest = visible && params !== undefined && params.endTime > params.startTime && metadata.pmu.hasSamples !== false;
    const cached = canRequest && params ? session.pmuCounterCache.peek(metadata.dataSource, params) : undefined;
    const unavailable = !params
        ? { ...emptyData(metadata), status: 'invalid-data' as const }
        : params.endTime <= params.startTime || metadata.pmu.hasSamples === false ? emptyData(metadata) : undefined;

    useEffect(() => {
        let current = true;
        if (!canRequest || !params) {
            setState(previous => ({ key, identity, data: unavailable ?? retainedData(previous, identity, metadata), loading: false, error: false }));
            runInAction(() => { unit.phase = 'download'; });
            return;
        }
        setState(previous => ({ key, identity, data: cached ?? retainedData(previous, identity, metadata), loading: cached === undefined, error: false }));
        // Coalesce viewport changes before they enter the shared request queue.
        // Each chart owns its timer, preserving independent offsets for pinned copies.
        const request = params;
        const fetch = (): void => {
            session.pmuCounterCache.fetch(metadata.dataSource, request).then(data => {
                if (current) { setState({ key, identity, data, loading: false, error: false }); }
            }).catch(error => {
                if (current) {
                    setState(previous => ({
                        key,
                        identity,
                        data: retainedData(previous, identity, metadata),
                        loading: false,
                        error: !(error instanceof PmuRequestCancelled),
                    }));
                }
            }).finally(() => {
                if (current) { runInAction(() => { unit.phase = 'download'; }); }
            });
        };
        // A completed cache hit is rendered immediately; fetch only refreshes its LRU position.
        const timer = cached === undefined ? setTimeout(fetch, 80) : undefined;
        if (cached !== undefined) { fetch(); }
        return () => { current = false; clearTimeout(timer); };
    }, [key, session, unit]);

    // Reproject the same counter's data during viewport refresh, but never reuse another source/session.
    const result = unavailable
        ? { data: unavailable, loading: false, error: false }
        : cached
            ? { data: cached, loading: false, error: false }
            : state.key === key ? state : { data: retainedData(state, identity, metadata), loading: canRequest, error: false };
    return { ...result, offset, retry: () => setAttempt(value => value + 1) };
}
