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

import type { Session } from '../../entity/session';
import React, { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { observer } from 'mobx-react';
import { Button } from '@insight/lib/components';
import { getDefaultColumData, getPageData, queryOneKernel, searchAllSlices } from './Common';
import { ResizeTable, fetchColumnFilterProps } from '@insight/lib/resize';
import { ChartErrorBoundary } from '../error/ChartErrorBoundary';
import styled from '@emotion/styled';
import { DEFAULT_CARD_VALUE, RankFilter, SelectedCardInfo } from './SystemView';
import { getDetailTimeDisplay } from '../../insight/units/AscendUnit';
import type { InsightUnit } from '../../entity/insight';
import { getTimeOffset } from '../../insight/units/utils';
import type { ThreadMetaData } from '../../entity/data';
import { ProcessMetaData } from '../../entity/data';
import jumpToUnitOperator from '../../utils/jumpToUnitOperator';

const CONTAINER = styled.div`
    height: calc(100% - 50px);

    .ant-table-wrapper {
        height: 100%;
    }
`;

const FindDetailContainer = styled.div`
    padding: 8px 16px;

    .rank-filter {
      display: flex;
      align-items: center;
      gap: 24px;

      .content {
        flex: none;
        width: 160px;
      }
    }
`;
export interface SearchTableData {
    /**
     *
     * @type SearchAllSlicesDetails[]
     * @memberof searchAllSlicesDetails
     */
    searchAllSlicesDetails: SearchAllSlicesDetails[];
    /**
     *
     * @type {number}
     * @memberof count
     */
    count: number;
}

export interface SearchAllSlicesDetails {
    startTime: string;
    /**
     *
     * @type {string}
     * @memberof name
     */
    name: string;
    /**
     *
     * @type {number}
     * @memberof timestamp
     */
    timestamp: number;
    /**
     *
     * @type {number}
     * @memberof duration
     */
    duration: number;
    /**
     *
     * @type {string}
     * @memberof id
     */
    id: string;
    /**
     *
     * @type {string}
     * @memberof tid
     */
    tid: string;
    /**
     *
     * @type {string}
     * @memberof pid
     */
    pid: string;
    /**
     *
     * @type {string}
     * @memberof deviceId
     */
    deviceId: string;
    /**
     *
     * @type {number}
     * @memberof depth
     */
    depth: number;
    /**
     *
     * @type {string}
     * @memberof originOptimizer
     */
    originOptimizer: string;
    /**
     * 实际是 card: `{host} {rankId}`
     * @type {string}
     * @memberof rankId
     */
    rankId: string;
    /**
     * 卡的数据库路径
     * @type {string}
     * @memberof dbPath
     */
    dbPath: string;
}

export function useFindDetail(session: Session, bottomHeight: number): any {
    const { t } = useTranslation('timeline');
    return {
        label: <div className={'title'}><span>{t('Find')}</span></div>,
        key: 'Find',
        children: <FindDetailView session={session} bottomHeight={bottomHeight}/>,
    };
}

const useColumns = (onFilter: (value: string) => void): any => {
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });
    return [
        {
            title: t('Name'),
            dataIndex: 'name',
            ...getDefaultColumData('name'),
            ...fetchColumnFilterProps('name', 'Name', false, undefined, onFilter),
        },
        { title: t('Start Time'), dataIndex: 'startTime', ...getDefaultColumData('startTime') },
        { title: t('Duration(ns)'), dataIndex: 'duration', ...getDefaultColumData('duration') },
    ];
};

export const FindDetailView = observer((props: { session: Session; bottomHeight: number }) => {
    const [conditions, setConditions] = useState<SelectedCardInfo>(DEFAULT_CARD_VALUE);
    const handleChange = (card: SelectedCardInfo): void => {
        setConditions(card);
    };
    return (
        <FindDetailContainer>
            <RankFilter session={props.session} handleChange={handleChange} defaultRankId={props.session.searchData?.rankId}></RankFilter>
            <ChartErrorBoundary>
                <FindDetail card={conditions} session={props.session} bottomHeight={props.bottomHeight}></FindDetail>
            </ChartErrorBoundary>
        </FindDetailContainer>
    );
});

const defaultPage = { current: 1, pageSize: 10, total: 0 };
const defaultSorter = { field: 'duration', order: 'descend' };

export interface FindDetailProps {
    card: SelectedCardInfo;
    session: Session;
    bottomHeight: number;
}

interface AllConditionType {
    doContextSearch?: boolean;
    page: typeof defaultPage;
    sorter: typeof defaultSorter;
    selectCard: SelectedCardInfo;
    nameFilter: string;
}

// eslint-disable-next-line max-lines-per-function
const FindDetail = observer((props: FindDetailProps) => {
    const [dataSource, setDataSource] = useState<SearchAllSlicesDetails[]>([]);
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(defaultSorter);
    const [isLoading, setLoading] = useState(false);
    const [rowData, setRowData] = useState<Partial<SearchAllSlicesDetails>>({});
    const [nameFilter, setNameFilter] = useState<string>('');
    const [allCondition, setAllCondition] = useState<AllConditionType>(
        { doContextSearch: props.session.doContextSearch, page, sorter, selectCard: props.card, nameFilter: '' });
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });

    const handleNameFilter = (value: string): void => {
        setNameFilter(value);
    };

    const columns = [...useColumns(handleNameFilter), {
        title: t('Click To Timeline'),
        dataIndex: 'click',
        ellipsis: true,
        render: (_: any, record: any): React.ReactElement => (<Button type="link"
            onClick={(): void => {
                setRowData({ name: record.name ?? record.originOptimizer, ...record });
            }}>{t('Click')}</Button>),
    }];

    useEffect(() => {
        setAllCondition({ ...allCondition, page, sorter });
    }, [sorter, page.current, page.pageSize]);

    useEffect(() => {
        setAllCondition({ ...allCondition, doContextSearch: props.session.doContextSearch, page: defaultPage, selectCard: props.card });
        setPage(defaultPage);
    }, [props.session.doContextSearch, props.card.cardId]);

    useEffect(() => {
        setAllCondition({ ...allCondition, nameFilter, page: defaultPage });
        setPage(defaultPage);
    }, [nameFilter]);

    useEffect(() => {
        updateData(allCondition.page, allCondition.sorter, props);
    }, [allCondition.sorter, allCondition.selectCard.cardId, allCondition.page.current,
        allCondition.page.pageSize, allCondition.doContextSearch, allCondition.nameFilter, props.session.doReset]);

    useEffect(() => {
        if (rowData.name === null || rowData.name === undefined) {
            return;
        }
        handleFindSelected(rowData as SearchAllSlicesDetails, props);
    }, [rowData]);

    const updateData = async(pages: any, sorters: {field: string;order: string}, prop: FindDetailProps): Promise<void> => {
        if (props.card === undefined || props.card.cardId === '') {
            setDataSource([]);
            setPage(defaultPage);
            setSorter(defaultSorter);
            setAllCondition({ ...allCondition, page: defaultPage, sorter: defaultSorter });
            return;
        }
        if (prop.session.searchData === undefined || prop.session.searchData?.content === '') {
            setDataSource([]);
            setPage(defaultPage);
            setSorter(defaultSorter);
            setAllCondition({ ...allCondition, page: defaultPage, sorter: defaultSorter });
            return;
        }
        setLoading(true);
        const res = await searchData(pages, sorters, prop, allCondition.nameFilter).finally(() => setLoading(false));
        const timestampoffset = getTimeOffset(props.session, props.card);
        const data = res.searchAllSlicesDetails.map(item => {
            item.startTime = getDetailTimeDisplay(item.timestamp - timestampoffset);
            return item;
        });
        setDataSource(data);
        setPage({ ...page, total: res.count });
    };

    return <CONTAINER>
        <ResizeTable
            onChange={(pagination: unknown, filters: unknown, newsorter: unknown, extra: {action: string}): void => {
                if (extra.action === 'sort') {
                    setSorter(newsorter as typeof sorter);
                }
            }}
            rowClassName={(record: any): string => {
                return record.id === rowData.id ? 'selected-row' : 'click-able';
            }}
            pagination={getPageData(page, setPage)}
            dataSource={dataSource}
            columns={columns}
            scroll={{ y: props.bottomHeight - 180 }}
            size="small"
            loading = {isLoading}
        />
    </CONTAINER>;
});

const searchData = async(pages: any, sorters: {field: string;order: string}, prop: FindDetailProps, nameFilter?: string): Promise<SearchTableData> => {
    const metadataList = (prop.session.lockUnit as InsightUnit[]).map(selectUnit => {
        const { threadId, processId, metaType, cardId } = selectUnit?.metadata as ThreadMetaData ?? {};
        const timestampOffset = getTimeOffset(prop.session, selectUnit?.metadata as ProcessMetaData);
        const lockStartTime = prop.session.lockRange === undefined ? 0 : Math.floor(prop.session.lockRange[0] + timestampOffset);
        const lockEndTime = prop.session.lockRange === undefined ? 0 : Math.ceil(prop.session.lockRange[1] + timestampOffset);
        return {
            tid: threadId,
            pid: processId,
            metaType,
            rankId: cardId,
            lockStartTime,
            lockEndTime,
        };
    });
    const res = await searchAllSlices({
        rankId: prop.card.cardId,
        dbPath: prop.card.dbPath,
        pageSize: pages.pageSize,
        current: pages.current,
        orderBy: sorters.field === 'startTime' ? 'timestamp' : sorters.field ?? defaultSorter.field,
        order: sorters.order ?? defaultSorter.order,
        searchContent: prop.session.searchData?.content,
        isMatchCase: prop.session.searchData?.isMatchCase,
        isMatchExact: prop.session.searchData?.isMatchExact,
        metadataList,
        nameFilter,
    });
    return res;
};

const handleFindSelected = async(rowData: SearchAllSlicesDetails & { originOptimizer: string }, props: FindDetailProps): Promise<void> => {
    const queryName = rowData.name ?? rowData.originOptimizer;
    const res = await queryOneKernel({
        rankId: rowData.rankId,
        dbPath: rowData.dbPath,
        name: queryName,
        timestamp: rowData.timestamp,
        duration: rowData.duration,
    });
    const depth = rowData.depth > res.depth ? rowData.depth : res.depth;
    jumpToUnitOperator({
        ...rowData,
        name: queryName,
        depth,
        cardId: rowData.rankId,
        dbPath: rowData.dbPath,
    });
};
