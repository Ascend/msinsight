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

import type { ThreadMetaData, ThreadSource } from '../entity/data';
import type { InsightUnit } from '../entity/insight';
import type { Session } from '../entity/session';
import { buildCardIdIndex, type OffsetSide } from '../insight/units/offset';
import { getTimeOffset } from '../insight/units/utils';

/**
 * Search and flow queries keep the track identity and time range in the same
 * fields as a locked selection. See `TraceDatabaseHelper::GetLockRangeSql`.
 */
export interface RangeMetadata {
    tid?: string;
    pid?: string;
    metaType?: string;
    rankId?: string;
    dbPath?: string;
    lockStartTime: number;
    lockEndTime: number;
}

interface LaneIdentity {
    cardId?: string;
    dbPath?: string;
    processId?: string;
    threadId?: string;
    metaType?: string;
    offsetSide?: OffsetSide;
}

const getLaneIdentities = (unit: InsightUnit): LaneIdentity[] => {
    const metadata = unit.metadata as ThreadMetaData | undefined;
    if (metadata === undefined) {
        return [];
    }
    const { cardId, dbPath, processId, metaType, offsetSide, threadId, threadIdList, threadSourceList } = metadata;
    if (Array.isArray(threadSourceList) && threadSourceList.length > 0) {
        return threadSourceList.map((source: ThreadSource) => ({
            cardId: source.cardId ?? cardId,
            dbPath: source.dbPath ?? dbPath,
            processId: source.processId ?? processId,
            threadId: source.threadId,
            metaType: source.metaType ?? metaType,
            offsetSide,
        }));
    }
    const threadIds = Array.isArray(threadIdList) && threadIdList.length > 0 ? threadIdList : [threadId];
    return threadIds
        .filter((tid): tid is string => typeof tid === 'string' && tid !== '')
        .map(tid => ({ cardId, dbPath, processId, threadId: tid, metaType, offsetSide }));
};
const buildMetadataList = (
    session: Session, identities: LaneIdentity[], range: [number, number],
): RangeMetadata[] => {
    const cardIdIndex = buildCardIdIndex(session.units);
    const timestampOffset = session.unitsConfig.offsetConfig.timestampOffset;
    const metadataList: RangeMetadata[] = [];
    const seen = new Set<string>();
    for (const lane of identities) {
        if (lane.processId === undefined || lane.processId === '' ||
            lane.threadId === undefined || lane.threadId === '') {
            continue;
        }
        const offset = getTimeOffset(session, lane, session.units, timestampOffset, cardIdIndex);
        const metadata: RangeMetadata = {
            tid: lane.threadId,
            pid: lane.processId,
            metaType: lane.metaType,
            rankId: lane.cardId,
            dbPath: lane.dbPath,
            lockStartTime: Math.floor(range[0] + offset),
            lockEndTime: Math.ceil(range[1] + offset),
        };
        const key = [metadata.rankId, metadata.dbPath, metadata.pid, metadata.tid, metadata.metaType,
            metadata.lockStartTime, metadata.lockEndTime].join('\u001f');
        if (seen.has(key)) { continue; }
        seen.add(key);
        metadataList.push(metadata);
    }
    return metadataList;
};

/** True when search and flow queries must be limited to the time analysis range. */
export const isTimeAnalysisRangeRestricted = (session: Session): boolean =>
    session.isTimeAnalysisMode && session.timeAnalysisRange !== undefined;

/**
 * Time range analysis covers every lane in the timeline, restricted to the
 * analysis time range. It reuses the same track-and-time fields as the locked
 * selection so both features share one range rule.
 */
export const getTimeAnalysisRangeMetadataList = (session: Session): RangeMetadata[] => {
    if (!session.isTimeAnalysisMode || session.timeAnalysisRange === undefined) {
        return [];
    }
    // Read the tree without preOrderFlatten, which writes rendering levels to each node.
    const identities: LaneIdentity[] = [];
    const visited = new Set<InsightUnit>();
    const visit = (unit: InsightUnit): void => {
        if (visited.has(unit) || !unit.isDisplay || unit.isMultiDeviceHidden) { return; }
        visited.add(unit);
        identities.push(...getLaneIdentities(unit));
        unit.children?.forEach(visit);
    };
    session.units.forEach(visit);
    return buildMetadataList(session, identities, session.timeAnalysisRange);
};

/** One existing flow request per distinct lane time boundary, not per lane. */
export const getTimeAnalysisFlowRanges = (
    metadataList: RangeMetadata[], cardId: string, sourceDbPath: string,
    analysisRange: [number, number], domainStart: number, domainEnd: number,
): Array<{
    metadataList: Array<Pick<RangeMetadata, 'tid' | 'pid' | 'metaType' | 'rankId'>>;
    lockStartTime: number;
    lockEndTime: number;
    startTime: number;
    endTime: number;
}> => {
    const groups = new Map<string, ReturnType<typeof getTimeAnalysisFlowRanges>[number]>();
    for (const metadata of metadataList) {
        if (metadata.rankId !== cardId || metadata.dbPath !== sourceDbPath) { continue; }
        const { tid, pid, metaType, lockStartTime, lockEndTime } = metadata;
        const key = `${lockStartTime}/${lockEndTime}`;
        if (!groups.has(key)) {
            groups.set(key, {
                metadataList: [],
                lockStartTime,
                lockEndTime,
                // Boundaries already include floor/ceil rounding. Applying their shifts
                // also to the viewport adds at most 1 ns to normal floor/ceil padding.
                startTime: Math.floor(domainStart + lockStartTime - analysisRange[0]),
                endTime: Math.ceil(domainEnd + lockEndTime - analysisRange[1]),
            });
        }
        groups.get(key)?.metadataList.push({ tid, pid, metaType, rankId: cardId });
    }
    return [...groups.values()];
};
