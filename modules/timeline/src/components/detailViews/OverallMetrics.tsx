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

import { observer } from 'mobx-react';
import { fetchColumnFilterProps, ResizeTable } from '@insight/lib/resize';
import type { ColumnsType } from 'antd/es/table';
import { DragDirection, useDraggableContainer } from '@insight/lib';
import React, { useEffect, useRef, useState } from 'react';
import { ChartErrorBoundary } from '../error/ChartErrorBoundary';
import { MoreContainer, StyledMoreCard } from '../BottomPanel';
import { store } from '../../store';
import { getOverallMetrics, getOverallMetricsMoreList } from '../../api/request';
import type {
    GetOverallMetricsMoreListResultItem,
    GetOverallMetricsResultItem,
} from '../../api/interface';
import type { FilterValue, SorterResult } from 'antd/lib/table/interface';
import { Session } from '../../entity/session';
import { getDefaultColumData, getPageData, PageType, queryOneKernel } from './Common';
import type { CardMetaData } from '../../entity/data';
import jumpToUnitOperator from '../../utils/jumpToUnitOperator';
import { getDetailTimeDisplay } from '../../insight/units/AscendUnit';
import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import { StyledEmpty } from '@insight/lib/utils';
import type { SelectContentViewProps } from './SystemView';
import { getTimeOffset } from '../../insight/units/utils';
import { ResponseValidator } from '../../utils/response-validator';

export const overallMetricsColumns = (t: TFunction): ColumnsType<GetOverallMetricsResultItem> => [
    { title: t('Category'), dataIndex: 'name', ellipsis: true },
    { title: t('Total Time(us)'), dataIndex: 'totalTime' },
    {
        title: t('Time Ratio'),
        dataIndex: 'ratio',
        render: (text) => text !== null ? `${text}%` : text,
    },
    { title: t('Number'), dataIndex: 'nums' },
    { title: t('Avg(us)'), dataIndex: 'avg' },
    { title: t('Min(us)'), dataIndex: 'min' },
    { title: t('Max(us)'), dataIndex: 'max' },
];

const overallMetricsMoreColumns = (t: TFunction): ColumnsType<GetOverallMetricsMoreListResultItem> => [
    {
        title: t('Name'),
        dataIndex: 'name',
        ellipsis: true,
        ...fetchColumnFilterProps('name', 'Name'),
        onFilter: undefined,
    },
    {
        title: t('Start Time'),
        dataIndex: 'startTime',
        ...getDefaultColumData('startTime'),
    },
    {
        title: t('Duration(ns)'),
        dataIndex: 'duration',
        ...getDefaultColumData('duration'),
    },
];

interface OverallMetricsTableProps extends SelectContentViewProps {
    session: Session;
    selectedRow?: GetOverallMetricsResultItem | null;
    setSelectedRow: (row: GetOverallMetricsResultItem | null) => void;
}

const OverallMetricsTable = observer(({ bottomHeight, card, session, selectedRow, setSelectedRow }: OverallMetricsTableProps) => {
    const defaultPage = { current: 1, pageSize: 10, total: 0 };
    const [tableData, setTableData] = useState<GetOverallMetricsResultItem[]>([]);
    const [loading, setLoading] = useState(false);
    const [page, setPage] = useState(defaultPage);
    const cardPhase = session.units.find((unit) => (unit.metadata as CardMetaData).cardId === card.cardId)?.phase;
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });

    async function getOverallMetricsData(): Promise<void> {
        if (!card || card.cardId === '') {
            return;
        }
        setLoading(true);
        try {
            const rankId = card.cardId;
            const dbPath = card.dbPath;
            let startTime = session.timeAnalysisRange?.[0] ?? 0;
            startTime = startTime < 0 ? 0 : startTime;
            let endTime = session.timeAnalysisRange?.[1] ?? 0;
            endTime = endTime < 0 ? 0 : endTime;
            const timestampoffset = getTimeOffset(session, card);
            const { data, count: total } = await getOverallMetrics({
                rankId,
                dbPath,
                pageSize: page.pageSize,
                current: page.current,
                startTime: Math.floor(startTime + timestampoffset),
                endTime: Math.ceil(endTime + timestampoffset),
            });
            setPage({ ...page, total });
            setTableData(data ?? []);
            setLoading(false);
        } catch (e) {
            setLoading(false);
            setTableData([]);
            setPage(defaultPage);
        }
    }

    useEffect(() => {
        getOverallMetricsData();
        setSelectedRow(null);
    }, [card.cardId, session.timeAnalysisRange]);

    useEffect(() => {
        if (cardPhase === 'download') {
            getOverallMetricsData();
        }
    }, [cardPhase, session.timeAnalysisRange]);

    return <ResizeTable
        rowKey={'id'}
        dataSource={tableData}
        columns={overallMetricsColumns(t)}
        loading={loading}
        scroll={{ y: bottomHeight - 146 }}
        pagination={getPageData(page, setPage)}
        rowHoverable={false}
        expandable={{}}
        onRow={(record): { onClick: () => void } => ({
            onClick: (): void => {
                if (record.level === 1) {
                    return;
                }
                setSelectedRow(record);
            },
        })}
        // 给表格添加类名
        rowClassName={(record): string => {
            // 检查selectedRow的id是否与当前记录的id相同
            return selectedRow?.id === record.id ? 'selected-row' : '';
        }}
    ></ResizeTable>;
});

interface OverallMetricsMoreProps extends SelectContentViewProps {
    selectedRow?: GetOverallMetricsResultItem | null;
}
const OverallMetricsMoreTable = observer(({ card, session, selectedRow, bottomHeight }: OverallMetricsMoreProps) => {
    const [selectedRowId, setSelectedRowId] = useState<string>();
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });

    const { page, setPage, setSorter, setFilters, loading, tableData } = useMetricsMoreUpdater({
        session,
        card,
        selectedRow,
    });

    const rowEvents = (record: GetOverallMetricsMoreListResultItem): React.HTMLAttributes<any> => {
        const { id, name, duration, timestamp } = record;
        return {
            onClick: async (): Promise<void> => {
                const res = await queryOneKernel({ rankId: card.cardId, dbPath: card.dbPath, name, timestamp, duration });
                jumpToUnitOperator({
                    ...record,
                    ...res,
                    duration,
                    cardId: card.cardId,
                    dbPath: card.dbPath,
                    tid: res.threadId,
                    pid: res.pid,
                    id: res.id,
                    metaType: res.metaType ?? '',
                });
                setSelectedRowId(id);
            },
        };
    };

    return <ResizeTable
        rowKey={'id'}
        dataSource={tableData}
        columns={overallMetricsMoreColumns(t)}
        scroll={{ y: bottomHeight !== undefined ? bottomHeight - 200 : undefined }}
        loading={loading}
        pagination={getPageData(page, setPage)}
        onRow={(record): React.HTMLAttributes<any> => rowEvents(record)}
        rowClassName={(record): string => {
            return record.id === selectedRowId ? 'selected-row' : 'click-able';
        }}
        onChange={(pagination, filters, newSorter, extra): void => {
            if (extra.action === 'sort') {
                setSorter(newSorter as SorterResult<GetOverallMetricsMoreListResultItem>);
            }
            if (extra.action === 'filter') {
                setFilters(filters);
            }
        }}
    ></ResizeTable>;
});

export type MetricsMoreUpdaterType = ({ session, card, selectedRow }: Pick<OverallMetricsMoreProps, 'session' | 'card' | 'selectedRow'>) => ({
    page: PageType;
    setPage: (args: PageType) => void;
    sorter: SorterResult<GetOverallMetricsMoreListResultItem>;
    setSorter: React.Dispatch<React.SetStateAction<SorterResult<GetOverallMetricsMoreListResultItem>>>;
    setFilters: React.Dispatch<React.SetStateAction<Record<string, FilterValue | null>>>;
    loading: boolean;
    tableData: GetOverallMetricsMoreListResultItem[];
});

const useMetricsMoreUpdater: MetricsMoreUpdaterType = ({ session, card, selectedRow }) => {
    const defaultPage = { current: 1, pageSize: 10, total: 0 };
    const defaultSorter: SorterResult<GetOverallMetricsMoreListResultItem> = { field: 'duration', order: 'descend' };
    const [page, setPage] = useState<PageType>(defaultPage);
    const [sorter, setSorter] = useState(defaultSorter);
    const [filters, setFilters] = useState<Record<string, FilterValue | null>>({});
    const [loading, setLoading] = useState(false);
    const [tableData, setTableData] = useState<GetOverallMetricsMoreListResultItem[]>([]);
    const validatorRef = useRef(new ResponseValidator());
    const isMountedRef = useRef(true); // 防止卸载后 setState

    const getMoreListData = React.useCallback(async (selectedRow: GetOverallMetricsResultItem | null): Promise<{
        page: PageType; data: GetOverallMetricsMoreListResultItem[];
    } | null> => {
        if (!card || card.cardId === '') {
            return null;
        }
        setLoading(true);
        let startTime = session.timeAnalysisRange?.[0] ?? 0;
        startTime = startTime < 0 ? 0 : startTime;
        let endTime = session.timeAnalysisRange?.[1] ?? 0;
        endTime = endTime < 0 ? 0 : endTime;
        const timestampoffset = getTimeOffset(session, card);
        const { sameOperatorsDetails, count: total, pageSize, currentPage: current } = await getOverallMetricsMoreList({
            rankId: card.cardId,
            dbPath: card.dbPath,
            name: filters.name?.[0] as string | undefined,
            categoryList: selectedRow?.categoryList ?? [],
            order: sorter.order,
            orderBy: sorter.field as keyof GetOverallMetricsMoreListResultItem,
            pageSize: page.pageSize,
            current: page.current,
            startTime: Math.floor(startTime + timestampoffset),
            endTime: Math.ceil(endTime + timestampoffset),
        }).finally(() => {
            setLoading(false);
        });
        return {
            page: { ...page, pageSize, current, total },
            data: sameOperatorsDetails.map(item => ({ ...item, startTime: getDetailTimeDisplay(item.timestamp) })) ?? [],
        };
    }, [card?.cardId, card?.dbPath, filters.name, sorter.order, sorter.field, page.pageSize, page.current, session.timeAnalysisRange]);

    useEffect(() => {
        isMountedRef.current = true;
        const validator = validatorRef.current;

        // 更新版本号
        const requestVersion = validator.markUpdate();
        setTableData([]);
        // 情况1：selectedRow 为空 -> 立即清空数据
        if (!selectedRow) {
            return () => {
                isMountedRef.current = false;
            };
        }
        getMoreListData(selectedRow).then(res => {
            if (res && isMountedRef.current && validator.isValid(requestVersion)) {
                setPage(res.page);
                setTableData(res.data);
            }
        });
        return () => {
            isMountedRef.current = false;
        };
    }, [getMoreListData, selectedRow]);

    useEffect(() => {
        setPage(defaultPage);
    }, [card?.cardId, selectedRow?.id]);

    useEffect(() => () => {
        validatorRef.current.reset(); // 组件卸载时重置版本号，确保后续请求无效
    }, []);

    return { page, setPage, sorter, setSorter, setFilters, loading, tableData };
};

/**
 * OverallMetrics组件，用于展示总体指标信息。
 *
 * @param {SelectContentViewProps} props - 组件的属性，包含卡片信息等
 * @returns {JSX.Element} 返回一个React元素，根据条件渲染总体指标表格或空状态
 */
export const OverallMetrics = observer((props: SelectContentViewProps) => {
    const { sessionStore } = store;
    // 获取当前活跃的session
    const session = sessionStore.activeSession;
    // 使用拖拽容器，设置拖拽方向为向右，宽度为400
    const [view] = useDraggableContainer({ dragDirection: DragDirection.RIGHT, draggableWH: 400 });
    // 定义并初始化selectedRow状态，用于存储选中的行数据
    const [selectedRow, setSelectedRow] = useState<GetOverallMetricsResultItem | null>();
    const { t } = useTranslation();

    return props.card !== undefined && props.card.cardId !== ''
        ? view({
            mainContainer: session !== undefined
                ? <OverallMetricsTable {...props} selectedRow={selectedRow} setSelectedRow={setSelectedRow}
                    session={session}/>
                : <></>,
            draggableContainer: <StyledMoreCard
                className="moreContainer"
                title={t('Details')}
                bordered={false}>
                <ChartErrorBoundary className={'more-error'}>
                    <MoreContainer>
                        {session && <OverallMetricsMoreTable {...props} session={session} selectedRow={selectedRow}/>}
                    </MoreContainer>
                </ChartErrorBoundary>
            </StyledMoreCard>,
            id: 'overallMetrics',
        })
        : <div style={{ display: 'flex', height: '100%' }}>
            <StyledEmpty style={{ margin: 'auto' }}/>
        </div>;
});
