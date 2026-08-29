/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

import { detail, SummaryFunction } from '../../entity/insight';
import type { DetailDescriptor, InsightUnit } from '../../entity/insight';
import { isEmpty } from 'lodash';
import { runInAction } from 'mobx';
import type { AscendMultiSliceList, ThreadMetaData, ThreadTrace } from '../../entity/data';
import type { Session } from '../../entity/session';
import { getSliceTimeDisplay } from './AscendUnit';
import { getTimeOffset } from './utils';
import { checkIsValidSlice } from '../../components/charts/ChartInteractor/draw';
import { checkIsSameUnit } from '../../utils';
import { GetUnitFlowsParams } from '../../api/interface';

const isSelfTimeHidden = (session: Session): boolean => {
    return session.isSimulation;
};

const getSelectedUnitsOfSliceMode = (session: Session): InsightUnit[] => {
    const targetMetaData = (session.sliceSelection.targetUnit?.metadata ?? {}) as ThreadMetaData;
    return session.selectedUnits.filter(unit => {
        const itemMetaData = (unit?.metadata || {}) as ThreadMetaData;
        return checkIsSameUnit(targetMetaData, itemMetaData);
    });
};

export const slicesListDetail = detail<AscendMultiSliceList, any, any, ThreadMetaData>({
    name: 'Slice List',
    columns: [
        ['Name', (data): string => `${isEmpty(data.title) ? 'null' : data.title}`, 'max-content', 'scroll'],
        ['Wall Duration', (data): string => getSliceTimeDisplay(data.wallDuration as number), 180],
        ['Self Time', (data): string => getSliceTimeDisplay(data.selfTime as number), 180, undefined, isSelfTimeHidden],
        ['Average Wall Duration', (data): string => getSliceTimeDisplay(data.avgWallDuration as number), 180],
        ['Max Wall Duration', (data): string => getSliceTimeDisplay(data.maxWallDuration as number), 180],
        ['Min Wall Duration', (data): string => getSliceTimeDisplay(data.minWallDuration as number), 180],
        ['Occurrences', (data): string => `${data.occurrences ?? 0}`, 180],
    ],
    actions: [
        { sorter: (a: AscendMultiSliceList, b: AscendMultiSliceList): number => a.title?.localeCompare(b?.title ?? '') ?? 0, filterKey: 'title' },
        { sorter: (a: AscendMultiSliceList, b: AscendMultiSliceList): number => (a.wallDuration ?? 0) - (b.wallDuration ?? 0) },
        { sorter: (a: AscendMultiSliceList, b: AscendMultiSliceList): number => (a.selfTime ?? 0) - (b.selfTime ?? 0) },
        { sorter: (a: AscendMultiSliceList, b: AscendMultiSliceList): number => (a.avgWallDuration ?? 0) - (b.avgWallDuration ?? 0) },
        { sorter: (a: AscendMultiSliceList, b: AscendMultiSliceList): number => (a.maxWallDuration ?? 0) - (b.maxWallDuration ?? 0) },
        { sorter: (a: AscendMultiSliceList, b: AscendMultiSliceList): number => (a.minWallDuration ?? 0) - (b.minWallDuration ?? 0) },
        { sorter: (a: AscendMultiSliceList, b: AscendMultiSliceList): number => (a.occurrences ?? 0) - (b.occurrences ?? 0) },
    ],
    summaries: new Map<string, SummaryFunction<AscendMultiSliceList>>([
        ['Name', () => 'Totals'],
        ['Wall Duration', (dataSource) => getSliceTimeDisplay(dataSource.reduce((acc, { wallDuration }) => acc + (wallDuration ?? 0), 0))],
        ['Self Time', (dataSource) => getSliceTimeDisplay(dataSource.reduce((acc, { selfTime }) => acc + (selfTime ?? 0), 0))],
        ['Average Wall Duration', (dataSource) => {
            const [totalA, totalB] = dataSource.reduce(([a, b], { wallDuration, occurrences }) => [a + (wallDuration ?? 0), b + (occurrences ?? 0)], [0, 0]);
            return getSliceTimeDisplay(totalB === 0 ? totalA : totalA / totalB);
        }],
        ['Max Wall Duration', (dataSource) => {
            const durations = dataSource.map(({ maxWallDuration }) => maxWallDuration).filter((d): d is number => Number.isFinite(d));
            const maxDuration = durations.length > 0 ? Math.max(...durations) : 0;
            return getSliceTimeDisplay(maxDuration);
        }],
        ['Min Wall Duration', (dataSource) => {
            const durations = dataSource.map(({ minWallDuration }) => minWallDuration).filter((d): d is number => Number.isFinite(d));
            const minDuration = durations.length > 0 ? Math.min(...durations) : 0;
            return getSliceTimeDisplay(minDuration);
        }],
        ['Occurrences', (dataSource) => `${dataSource.reduce((acc, { occurrences }) => acc + (occurrences ?? 0), 0)}`],
    ]),
    fetchData: async (session: Session, metadata: ThreadMetaData) => {
        let startTime = session.selectedRange?.[0] ?? 0;
        startTime = startTime < 0 ? 0 : startTime;
        let endTime = session.selectedRange?.[1] ?? 0;
        endTime = endTime < 0 ? 0 : endTime;
        const timestampOffset = getTimeOffset(session, metadata);

        const { rangeOfLevels } = session.sliceSelection;
        const isValidSliceMode = checkIsValidSlice(session);
        const selectedUnits = isValidSliceMode ? getSelectedUnitsOfSliceMode(session) : [...session.selectedUnits];
        const paramsOfDepth = isValidSliceMode ? { startDepth: Math.round(rangeOfLevels[0]).toString(), endDepth: Math.round(rangeOfLevels[1]).toString() } : {};
        session.sliceSelection.searchOfSlice = isValidSliceMode;
        const pythonFunctionConfig = session.unitsConfig.filterConfig.pythonFunction;

        const metadataList = selectedUnits.flatMap(selectUnit => {
            const { cardId, threadId, threadIdList, threadName, processId, metaType } = selectUnit?.metadata as ThreadMetaData;
            const hidePythonFunction = pythonFunctionConfig?.[`${cardId}_${threadName}`] ?? false;
            if (Array.isArray(threadIdList)) {
                return threadIdList.map(tid => ({ tid, metaType, pid: processId, hidePythonFunction }));
            }
            return [{ tid: threadId, pid: processId, metaType, hidePythonFunction }];
        });

        const params = {
            rankId: metadata.cardId,
            dbPath: metadata.dbPath,
            startTime: Math.floor(startTime + timestampOffset),
            endTime: Math.ceil(endTime + timestampOffset),
            metadataList,
            ...paramsOfDepth,
        };

        // 这里判断必要入参，如隐藏的泳道无rankId，则不发起请求
        if (params.rankId === undefined) {
            return;
        }
        if (params.startDepth !== undefined && params.startDepth > params.endDepth) {
            return;
        }

        const raw = await window.request(metadata.dataSource, { command: 'unit/threads', params });
        const res = raw.data;

        res.forEach((element: AscendMultiSliceList) => {
            element.rankId = metadata.cardId;
            element.dbPath = metadata.dbPath;
            element.startTime = Math.floor(startTime + timestampOffset);
            element.endTime = Math.ceil(endTime + timestampOffset);
        });
        return res;
    },
    mouseEnterCallback: ({ session, row }) => {
        const selectedRangeData = session.selectedRangeData;
        const name = row.title;
        const hoveredData = selectedRangeData?.find((item) => item.name === name);
        runInAction(() => {
            session.sharedState.threadTrace = hoveredData;
        });
    },
    mouseLeaveCallback: ({ session, row }) => {
        const name = row.title;
        const hoveredData = session.sharedState.threadTrace as ThreadTrace | undefined;
        if (hoveredData && hoveredData.name === name) {
            runInAction(() => {
                session.sharedState.threadTrace = undefined;
            });
        }
    },
    clickCallback: async ({ row, session }): Promise<void> => {
        const data = {
            rankId: row.rankId,
            dbPath: row.dbPath,
            processes: row.processes,
            startTime: row.startTime,
            endTime: row.endTime,
            name: row.title,
            wallDuration: row.wallDuration,
            metaTypeList: row.metaTypeList,
            count: row.occurrences,
        };
        runInAction(() => {
            session.selectedMultiSlice = JSON.stringify(data);
        });
    },
}) as DetailDescriptor<unknown>;

export const generateFlowParam = function(metadata: ThreadMetaData, data: any, metaType?: string): Partial<GetUnitFlowsParams> {
    return {
        rankId: metadata.cardId ?? '',
        dbPath: metadata.dbPath ?? '',
        tid: data.tid ?? data.threadId ?? (metadata.threadId ?? ''),
        pid: data.pid ?? (metadata.processId ?? ''),
        id: data.id,
        metaType: metaType ?? metadata?.metaType ?? '',
        startTime: data.startTime ?? data.timestamp,
        endTime: (data.startTime ?? data.timestamp) + data.duration,
    };
};
