/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

import React, { useEffect, useState } from 'react';
import { observer } from 'mobx-react';
import { useTranslation } from 'react-i18next';
import type { ColumnsType, SorterResult } from 'antd/es/table/interface';
import { Button, Tooltip } from '@insight/lib/components';
import { fetchColumnFilterProps, ResizeTable } from '@insight/lib/resize';
import type { FreeAttributionDetail, FreeAttributionGroup, FreeAttributionParams } from '../../api/interface';
import { queryFreeAttributionDetails, queryFreeAttributionSummary } from '../../api/request';
import { getTimeOffset } from '../../insight/units/offset';
import jumpToUnitOperator from '../../utils/jumpToUnitOperator';
import { ProjectType } from '../../entity/insight';
import type { CardMetaData } from '../../entity/data';
import { getCardFlowSourceDbPaths } from '../../entity/data';
import { getPageData } from './Common';
import { DETAIL_HEADER_HEIGHT_ETC_PX, type SelectContentViewProps } from './SystemView';
import { freeApiCandidates, freeAttributionTimeRange, freeTargetStart, freeTimeUs } from './FreeAttributionUtils';

interface SortState {
    field: string;
    order: 'asc' | 'desc';
}

type ContextParams = Omit<FreeAttributionParams, 'current' | 'pageSize' | 'orderBy' | 'order'>;
const defaultPage = { current: 1, pageSize: 10, total: 0 };
const summarySort: SortState = { field: 'totalFreeDuration', order: 'desc' };
const detailSort: SortState = { field: 'freeStartTime', order: 'asc' };

const sortValue = <T extends object>(sorter: SorterResult<T> | Array<SorterResult<T>>, fallback: SortState): SortState => {
    const next = Array.isArray(sorter) ? sorter[0] : sorter;
    return next?.order && typeof next.field === 'string'
        ? { field: next.field, order: next.order === 'ascend' ? 'asc' : 'desc' }
        : fallback;
};

const longText = (value: string): JSX.Element => <Tooltip title={value}><span>{value}</span></Tooltip>;

const Retry = ({ onRetry }: { onRetry: () => void }): JSX.Element => {
    const { t } = useTranslation('timeline', { keyPrefix: 'freeAttribution' });
    return <div role="alert" style={{ padding: 8 }}>
        {t('Query Failed')} <Button type="link" onClick={onRetry}>{t('Retry')}</Button>
    </div>;
};

interface DetailRow extends FreeAttributionDetail {
    apiName: string;
    apiStartTime: string;
    apiDuration: string;
    freeStartTime: string;
    freeDurationLabel: string;
    statusLabel: string;
    reasonLabel: string;
}

interface DetailProps {
    context: ContextParams;
    groupId: string;
    session: SelectContentViewProps['session'];
}

const FreeAttributionDetails = observer(({ context, groupId, session }: DetailProps): JSX.Element => {
    const { t } = useTranslation('timeline', { keyPrefix: 'freeAttribution' });
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(detailSort);
    const [records, setRecords] = useState<FreeAttributionDetail[]>([]);
    const [loading, setLoading] = useState(false);
    const [hasError, setHasError] = useState(false);
    const [retry, setRetry] = useState(0);

    useEffect(() => {
        let stale = false;
        setLoading(true);
        setHasError(false);
        setRecords([]);
        void queryFreeAttributionDetails({
            ...context,
            groupId,
            current: page.current,
            pageSize: page.pageSize,
            orderBy: sorter.field,
            order: sorter.order,
        }).then(response => {
            if (stale) return;
            setRecords(response.records);
            setPage(previous => ({ ...previous, total: response.totalRecords }));
        }).catch(() => {
            if (stale) return;
            setHasError(true);
            setPage(previous => ({ ...previous, total: 0 }));
        }).finally(() => { if (!stale) setLoading(false); });
        return () => { stale = true; };
    }, [context.rankId, context.dbPath, context.rangeMode, context.startTime, context.endTime, context.searchName, groupId,
        page.current, page.pageSize, sorter.field, sorter.order, retry]);

    const data: DetailRow[] = records.map(record => ({
        ...record,
        apiName: freeApiCandidates(record).map(api => api.name).join(' / ') || '--',
        apiStartTime: freeApiCandidates(record).map(api => freeTargetStart(session, api)).join(' / ') || '--',
        apiDuration: freeApiCandidates(record).map(api => freeTimeUs(api.duration)).join(' / ') || '--',
        freeStartTime: freeTargetStart(session, record.freeTarget, 'device'),
        freeDurationLabel: freeTimeUs(record.freeDuration),
        statusLabel: t(`status.${record.attributionStatus}`, { defaultValue: record.attributionStatus }),
        reasonLabel: record.reason ? t(`reason.${record.reason}`, { defaultValue: record.reason }) : '--',
    }));
    const copyColumns: ColumnsType<DetailRow> = [
        { title: t('Host API'), dataIndex: 'apiName' },
        { title: t('API Start Time'), dataIndex: 'apiStartTime' },
        { title: t('API Duration'), dataIndex: 'apiDuration' },
        { title: t('Free Start Time'), dataIndex: 'freeStartTime' },
        { title: t('Free Duration'), dataIndex: 'freeDurationLabel' },
        { title: t('Next Behavior'), dataIndex: 'nextBehaviorType' },
        { title: t('Status'), dataIndex: 'statusLabel' },
        { title: t('Reason'), dataIndex: 'reasonLabel' },
    ];
    const apiSortableFields = ['apiName', 'apiStartTime', 'apiDuration'];
    const sortableFields = [...apiSortableFields, 'freeStartTime'];
    const renderApiValue = (value: string, record: DetailRow, field: string): JSX.Element => {
        const candidates = freeApiCandidates(record);
        if (candidates.length <= 1) return longText(value);
        const firstApi = candidates[0];
        const options = { count: candidates.length };
        const summaries: Record<string, string> = {
            apiName: t('API Name Summary', { ...options, value: firstApi.name }),
            apiStartTime: t('API Start Time Summary', { ...options, value: freeTargetStart(session, firstApi) }),
            apiDuration: t('API Duration Summary', { ...options, value: freeTimeUs(firstApi.duration) }),
        };
        return <Tooltip title={value}><span>{summaries[field]}</span></Tooltip>;
    };
    const columns: ColumnsType<DetailRow> = copyColumns.map(column => {
        const field = String('dataIndex' in column ? column.dataIndex : '');
        return {
            ...column,
            title: apiSortableFields.includes(field) && typeof column.title === 'string'
                ? <Tooltip title={t('Candidate API Sort Tip')}><span>{column.title}</span></Tooltip>
                : column.title,
            width: 150,
            ellipsis: true,
            sorter: sortableFields.includes(field) ? {} : false,
            sortOrder: sorter.field === field ? (sorter.order === 'asc' ? 'ascend' : 'descend') : null,
            render: (value: string, record): JSX.Element => apiSortableFields.includes(field)
                ? renderApiValue(value, record, field)
                : longText(value),
        };
    });
    columns[4] = {
        ...columns[4],
        dataIndex: 'freeDuration',
        sorter: {},
        render: freeTimeUs,
        sortOrder: sorter.field === 'freeDuration' ? (sorter.order === 'asc' ? 'ascend' : 'descend') : null,
    };
    columns.push({
        title: t('Actions'),
        key: 'actions',
        width: 240,
        render: (_value: unknown, record): JSX.Element => <div style={{ display: 'flex', flexWrap: 'wrap', alignItems: 'center', columnGap: 12, rowGap: 4 }}>
            {freeApiCandidates(record).map((target, index) => <Button key={`${target.dbPath}-${target.id}-${index}`}
                type="link" title={`${target.name}\n${t('API Start Time')}: ${freeTargetStart(session, target)}\nPID/TID: ${target.pid}/${target.tid}\n${t('Source')}: ${target.dbPath ?? '--'}`}
                onClick={(event): void => {
                    event.stopPropagation(); jumpToUnitOperator(target);
                }}>{t('Jump API')}{freeApiCandidates(record).length > 1 ? ` ${index + 1}` : ''}</Button>)}
            <Button type="link" onClick={(event): void => {
                event.stopPropagation(); jumpToUnitOperator(record.freeTarget);
            }}>{t('Jump Free')}</Button>
        </div>,
    });

    return <div>
        {hasError && <Retry onRetry={(): void => setRetry(value => value + 1)} />}
        <ResizeTable<DetailRow> size="small" rowKey="detailId" loading={loading} columns={columns}
            copyColumns={copyColumns} dataSource={data} pagination={getPageData(page, setPage)}
            sortDirections={['ascend', 'descend', 'ascend']}
            scroll={{ x: 1400 }} onChange={(_pagination, _filters, nextSorter, extra): void => {
                if (extra.action !== 'sort') return;
                setSorter(sortValue(nextSorter, detailSort));
                setPage(previous => ({ ...previous, current: 1 }));
            }} />
    </div>;
});

interface SummaryRow extends FreeAttributionGroup {
    displayName: string;
}

interface TableProps extends SelectContentViewProps {
    context: ContextParams;
    ready: boolean;
}

const FreeAttributionTable = ({ context, session, bottomHeight, ready }: TableProps): JSX.Element => {
    const { t } = useTranslation('timeline', { keyPrefix: 'freeAttribution' });
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(summarySort);
    const [searchName, setSearchName] = useState('');
    const [groups, setGroups] = useState<FreeAttributionGroup[]>([]);
    const [expandedKeys, setExpandedKeys] = useState<React.Key[]>([]);
    const [loading, setLoading] = useState(false);
    const [hasError, setHasError] = useState(false);
    const [retry, setRetry] = useState(0);

    useEffect(() => {
        if (!ready || !context.rankId) return;
        let stale = false;
        setLoading(true);
        setHasError(false);
        setGroups([]);
        setExpandedKeys([]);
        void queryFreeAttributionSummary({
            ...context, searchName, current: page.current, pageSize: page.pageSize, orderBy: sorter.field, order: sorter.order,
        }).then(response => {
            if (stale) return;
            setGroups(response.groups);
            setPage(previous => ({ ...previous, total: response.totalGroups }));
        }).catch(() => {
            if (stale) return;
            setHasError(true);
            setPage(previous => ({ ...previous, total: 0 }));
        }).finally(() => { if (!stale) setLoading(false); });
        return () => { stale = true; };
    }, [context.rankId, context.dbPath, context.rangeMode, context.startTime, context.endTime, ready,
        page.current, page.pageSize, sorter.field, sorter.order, searchName, retry]);

    const data: SummaryRow[] = groups.map(group => ({
        ...group,
        displayName: group.groupType === 'matched' ? group.apiName : t(`status.${group.groupType}`),
    }));
    const copyColumns: ColumnsType<SummaryRow> = [
        { title: t('Host API'), dataIndex: 'displayName' },
        { title: t('Total Free Duration'), dataIndex: 'totalFreeDuration', render: freeTimeUs },
        { title: t('Free Operator Count'), dataIndex: 'freeCount' },
    ];
    const columns: ColumnsType<SummaryRow> = [
        {
            ...copyColumns[0],
            dataIndex: 'apiName',
            ...fetchColumnFilterProps('apiName', 'Name'),
            width: 300,
            sorter: {},
            sortOrder: sorter.field === 'apiName' ? (sorter.order === 'asc' ? 'ascend' : 'descend') : null,
            render: (_value: string, record): JSX.Element => longText(record.displayName),
        },
        {
            ...copyColumns[1],
            width: 180,
            sorter: {},
            sortOrder: sorter.field === 'totalFreeDuration' ? (sorter.order === 'asc' ? 'ascend' : 'descend') : null,
            title: <Tooltip title={t('Total Free Duration Tip')}><span>{t('Total Free Duration')}</span></Tooltip>,
        },
        {
            ...copyColumns[2],
            width: 180,
            sorter: {},
            sortOrder: sorter.field === 'freeCount' ? (sorter.order === 'asc' ? 'ascend' : 'descend') : null,
            title: <Tooltip title={t('Free Operator Count Tip')}><span>{t('Free Operator Count')}</span></Tooltip>,
        },
    ];
    return <div style={{ height: '100%', minHeight: 0 }}>
        {hasError && <Retry onRetry={(): void => setRetry(value => value + 1)} />}
        <ResizeTable<SummaryRow> size="small" rowKey="groupId" columns={columns} copyColumns={copyColumns}
            dataSource={data} loading={loading} pagination={getPageData(page, setPage)}
            sortDirections={['ascend', 'descend', 'ascend']}
            scroll={{ x: 700, y: Math.max(100, bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX - 40) }}
            expandable={{
                expandedRowKeys: expandedKeys,
                onExpandedRowsChange: keys => setExpandedKeys([...keys]),
                expandedRowRender: record => <FreeAttributionDetails context={{ ...context, searchName }} groupId={record.groupId} session={session} />,
            }}
            onChange={(_pagination, filters, nextSorter, extra): void => {
                if (extra.action === 'filter') {
                    setSearchName(String(filters.apiName?.[0] ?? '').trim());
                    setExpandedKeys([]);
                } else if (extra.action === 'sort') {
                    setSorter(sortValue(nextSorter, summarySort));
                } else {
                    return;
                }
                setPage(previous => ({ ...previous, current: 1 }));
            }} />
    </div>;
};

export const FreeAttribution = observer((props: SelectContentViewProps): JSX.Element => {
    const { session, card } = props;
    const deviceOffset = getTimeOffset(session, { cardId: card.cardId, metaType: 'OVERLAP_ANALYSIS' });
    const range = freeAttributionTimeRange(session.timeAnalysisRange, deviceOffset);
    const selectedUnit = session.units.find(unit => (unit.metadata as CardMetaData)?.cardId === card.cardId);
    const sourceDbPaths = selectedUnit ? getCardFlowSourceDbPaths(selectedUnit, card.cardId) : [];
    const overlapState = [...new Set([card.cardId, card.dbPath, ...sourceDbPaths])].sort().map(dbId => {
        const status = session.asyncDataLoadingList[dbId];
        return [dbId, status?.OVERLAP_ANALYSIS === true, status?.OVERLAP_ANALYSIS_VERSION ?? 0];
    });
    const registered = Array.from(session.rankCardInfoMap.values()).some(info =>
        info.rankInfo.rankId === card.cardId && info.dbPath === card.dbPath);
    // RankFilter uses the parsed-card map; cached download units may survive a
    // project switch after their database registrations have been removed.
    const ready = selectedUnit !== undefined && selectedUnit.projectType !== ProjectType.IE &&
        registered && selectedUnit.phase === 'download' && sourceDbPaths.includes(card.dbPath);
    // Overlap can finish after download and after an early query has returned empty data.
    // Remount both levels on a new query context, invalidating pending responses and detail state.
    const contextKey = JSON.stringify([card.cardId, card.dbPath, range.rangeMode, range.startTime, range.endTime,
        session.projectName, session.startRecordTime,
        session.unitsConfig.offsetConfig.timestampOffset, ready, overlapState]);
    return <FreeAttributionTable {...props} key={contextKey} ready={ready}
        context={{ rankId: card.cardId, dbPath: card.dbPath, ...range }} />;
});
