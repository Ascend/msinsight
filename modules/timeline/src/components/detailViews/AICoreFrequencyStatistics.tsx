/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */

import React, { useEffect, useMemo, useState } from 'react';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import { fetchColumnFilterProps, ResizeTable } from '@insight/lib/resize';
import { Tooltip } from '@insight/lib/components';
import type { ColumnsType } from 'antd/es/table';
import { queryExpertAnalysis } from '../../api/request';
import type { GetAICoreParamsResult } from '../../api/interface';
import type { CounterMetaData } from '../../entity/data';
import type { InsightUnit } from '../../entity/insight';
import { getTimeOffset } from '../../insight/units/utils';
import { getOffsetSide } from '../../insight/units/offset';
import { DETAIL_HEADER_HEIGHT_ETC_PX, type SelectContentViewProps } from './SystemView';

interface FrequencyLane extends Pick<CounterMetaData, 'cardId' | 'dbPath' | 'metaType' | 'offsetSide'> {
    laneName: string;
    lanePid: string;
    parentLane: string;
    chartWidth: number;
}

interface FrequencyRow extends FrequencyLane {
    offset: number;
    result?: GetAICoreParamsResult;
    failed?: boolean;
}

export const collectFrequencyLanes = (units: InsightUnit[], selected: SelectContentViewProps['card']): FrequencyLane[] => {
    const visited = new Set<InsightUnit>();
    const lanes = new Map<string, FrequencyLane>();
    const stack = units.slice().reverse().map(unit => ({ unit, card: undefined as InsightUnit | undefined }));
    while (stack.length > 0) {
        const entry = stack.pop() as { unit: InsightUnit; card?: InsightUnit };
        const { unit } = entry;
        if (visited.has(unit)) { continue; }
        visited.add(unit);
        const metadata = unit.metadata as CounterMetaData;
        let card = entry.card;
        if (unit.name === 'Card' || (!card && !metadata.threadName && metadata.cardId)) { card = unit; }
        // 固定泳道可能先于原卡出现，仍通过原父链确定加载状态和卡名称。
        if (!card) {
            const parents = new Set<InsightUnit>();
            for (let parent = unit.parent; parent && !parents.has(parent); parent = parent.parent) {
                parents.add(parent);
                if (parent.name === 'Card') { card = parent; break; }
            }
        }
        const children = unit.children ?? [];
        for (let index = children.length - 1; index >= 0; index--) { stack.push({ unit: children[index], card }); }
        if (card?.phase !== 'download' || !selected.cardId || metadata.cardId !== selected.cardId || metadata.dbPath !== selected.dbPath ||
            !/^AI Core Freq(?: Die \d+)?$/.test(metadata.threadName ?? '')) { continue; }
        const parent = unit.parent?.metadata;
        const name = String(parent?.processName ?? metadata.processName ?? metadata.processId ?? '');
        const label = parent?.label ? ` (${String(parent.label)})` : '';
        lanes.set(JSON.stringify([metadata.cardId, metadata.dbPath, metadata.processId, metadata.threadName]), {
            cardId: metadata.cardId,
            dbPath: metadata.dbPath,
            laneName: metadata.threadName,
            lanePid: metadata.processId,
            parentLane: name + label,
            chartWidth: unit.counterRenderWidth ?? 0,
            metaType: metadata.metaType,
            offsetSide: metadata.offsetSide,
        });
    }
    return Array.from(lanes.values());
};

export const AICoreFrequencyStatistics = observer((props: SelectContentViewProps) => {
    const { t } = useTranslation('timeline', { keyPrefix: 'systemView' });
    const { t: commonT } = useTranslation();
    const [data, setData] = useState<FrequencyRow[]>([]);
    const [loading, setLoading] = useState(false);
    const range = props.session.timeAnalysisRange;
    // 仅收集系统视图当前卡；同侧共享偏移，每轮至多查找Host、Device两次。
    const offsets = new Map<string, number>();
    const laneKey = JSON.stringify(collectFrequencyLanes(props.session.units, props.card)
        .map(lane => {
            const side = getOffsetSide(lane.metaType, lane.offsetSide);
            if (!offsets.has(side)) {
                offsets.set(side, getTimeOffset(props.session, lane));
            }
            return { ...lane, offset: offsets.get(side) ?? 0, chartWidth: range === undefined ? 0 : lane.chartWidth };
        }));
    const lanes: FrequencyRow[] = useMemo(() => JSON.parse(laneKey), [laneKey]);
    const endTimeAll = props.session.endTimeAll ?? 0;
    const viewStartTime = range === undefined ? 0 : props.session.domainRange.domainStart;
    const viewEndTime = range === undefined ? 0 : props.session.domainRange.domainEnd;
    const fallbackWidth = range === undefined ? 0 : props.session.domain.chartViewWidth;

    useEffect(() => {
        let cancelled = false;
        setData([]);
        setLoading(lanes.length > 0);
        if (lanes.length === 0) { return; }
        const queryLanes = async (): Promise<void> => {
            const rows: FrequencyRow[] = [];
            for (let index = 0; index < lanes.length; index += 4) {
                if (cancelled) { break; }
                const batch = await Promise.all(lanes.slice(index, index + 4).map(lane => {
                    const offset = lane.offset;
                    return queryExpertAnalysis({
                        rankId: lane.cardId,
                        dbPath: lane.dbPath,
                        statisticsOnly: true,
                        laneName: lane.laneName,
                        lanePid: lane.lanePid,
                        viewStartTime: range === undefined ? 0 : viewStartTime + offset,
                        viewEndTime: range === undefined ? 0 : viewEndTime + offset,
                        chartWidth: lane.chartWidth || fallbackWidth,
                        hasCounterRange: true,
                        // 与 CounterUnit 使用同一泳道偏移和全量边界，不能改用卡级 Host 偏移。
                        counterStartTime: Math.floor(Math.max(0, offset)),
                        counterEndTime: Math.ceil(Math.max(0, endTimeAll + offset)),
                        hasTimeRange: range !== undefined,
                        startTime: range === undefined ? 0 : Math.floor(Math.max(0, range[0]) + offset),
                        endTime: range === undefined ? 0 : Math.ceil(Math.max(0, range[1]) + offset),
                    }).then(result => ({ ...lane, result }), () => ({ ...lane, failed: true }));
                }));
                rows.push(...batch);
            }
            if (!cancelled) {
                setData(rows);
                setLoading(false);
            }
        };
        // 平移或连续缩放时只查询稳定后的视图，避免无法撤回的请求在服务端堆积。
        const timer = setTimeout(queryLanes, 100);
        return (): void => { cancelled = true; clearTimeout(timer); };
    }, [range, lanes, viewStartTime, viewEndTime, fallbackWidth, endTimeAll]);

    const frequency = (row: FrequencyRow, field: 'avgFreq' | 'minFreq' | 'maxFreq'): number | undefined =>
        (row.result?.sampleCount ?? 0) > 0 ? row.result?.[field] : undefined;
    const columns: ColumnsType<FrequencyRow> = [
        ...(['parentLane', 'laneName'] as const).map(field => ({
            title: t(field === 'parentLane' ? 'AI Core Frequency Parent Lane' : 'AI Core Frequency Child Lane'),
            dataIndex: field,
            ellipsis: true,
            ...fetchColumnFilterProps(field, 'Name'),
            onFilter: (value: React.Key | boolean, row: FrequencyRow): boolean => row[field].toLowerCase().includes(String(value).toLowerCase()),
            sorter: (a: FrequencyRow, b: FrequencyRow): number => a[field].localeCompare(b[field]),
        })),
        ...(['avgFreq', 'minFreq', 'maxFreq'] as const).map((field, index) => ({
            title: t(['Mean Frequency(MHz)', 'Min Frequency(MHz)', 'Max Frequency(MHz)'][index]),
            key: field,
            sorter: (a: FrequencyRow, b: FrequencyRow): number => {
                const left = frequency(a, field) ?? -Infinity; const right = frequency(b, field) ?? -Infinity;
                return left === right ? 0 : left - right;
            },
            render: (_: unknown, row: FrequencyRow): React.ReactNode => row.failed
                ? <Tooltip title={t('AI Core Frequency Query Failed')}><span>-</span></Tooltip>
                : frequency(row, field)?.toFixed(2) ?? '-',
        })),
    ];
    return <ResizeTable
        rowKey={(row: FrequencyRow) => JSON.stringify([row.cardId, row.dbPath, row.lanePid, row.laneName])}
        loading={loading}
        pagination={{ defaultPageSize: 10, showSizeChanger: true, showTotal: total => commonT('PaginationTotal', { total }) }}
        locale={{ emptyText: t('AI Core Frequency No Data') }}
        dataSource={data.filter(row => row.cardId === props.card.cardId && row.dbPath === props.card.dbPath)}
        columns={columns}
        scroll={{ y: props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX }}
        size="small"
    />;
});
