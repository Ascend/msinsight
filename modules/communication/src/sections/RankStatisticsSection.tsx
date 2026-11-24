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
import React, { useEffect, useRef, useState } from 'react';
import { observer } from 'mobx-react';
import { Button } from '@insight/lib/components';
import { ResizeTable as Table } from '@insight/lib/resize';
import type { TableProps } from 'antd/es/table';
import type { CommunicationApi } from '../api';
import type { RankQuery, RankResult, RankSort, RankStatistics, Scope } from '../domain/types';
import { TRANSPORTS } from '../domain/types';
import { Result, Section } from '../components/Section';
import { BandwidthDrawer } from '../features/bandwidthDrawer';
import { useCommunicationStore } from '../store/context';
import { useQuery } from '../state/useQuery';
import { useTranslation } from 'react-i18next';

export const RankStatisticsSection = observer(function RankStatisticsSection({ scope, api }: {
    scope: Scope; api: CommunicationApi;
}): JSX.Element {
    const { dark } = useCommunicationStore();
    const { t } = useTranslation('communication');
    const key = JSON.stringify(scope);
    const [rankId, setRankId] = useState<string>();
    const [query, setQuery] = useState<RankQuery>({ currentPage: 1, pageSize: 20, orderBy: 'rankId', order: 'ascend' });
    const content = useRef<HTMLDivElement>(null);
    const ranks = useQuery(JSON.stringify([key, query]), true, signal => api.ranks(scope, query, signal));
    const [lastPage, setLastPage] = useState<{ scope: string; data: RankResult }>();
    useEffect(() => {
        if (ranks.data) { setLastPage({ scope: key, data: ranks.data }); }
    }, [key, ranks.data]);
    // Retain one successful page only within this scope; keep the table mounted during refreshes.
    const visibleData = ranks.data ?? (lastPage?.scope === key ? lastPage.data : undefined);
    const tableQuery = visibleData ? { data: visibleData, loading: false, retry: ranks.retry } : ranks;
    const onChange: TableProps<RankStatistics>['onChange'] = (pagination, _filters, sorter, extra) => {
        const selected = Array.isArray(sorter) ? sorter[0] : sorter;
        const sort: RankSort = selected.order && selected.columnKey !== 'rankId'
            ? { orderBy: 'bandwidth', bandType: String(selected.columnKey), order: selected.order }
            : { orderBy: 'rankId', order: selected.order ?? 'ascend' };
        const pageSize = pagination.pageSize ?? query.pageSize;
        setQuery({ ...sort, pageSize, currentPage: extra.action === 'sort' || pageSize !== query.pageSize ? 1 : pagination.current ?? 1 });
    };
    useEffect(() => {
        if (rankId === undefined || !content.current) { return; }
        const element = content.current.closest('main') ?? content.current; const focused = document.activeElement as HTMLElement | null;
        element.setAttribute('inert', ''); element.setAttribute('aria-hidden', 'true');
        return () => { element.removeAttribute('inert'); element.removeAttribute('aria-hidden'); if (focused?.isConnected) { focused.focus({ preventScroll: true }); } };
    }, [rankId]);
    return <><div ref={content} aria-busy={ranks.loading}>
        <Section title={t('modern.ranks')}>
            {visibleData && ranks.error && <Result query={ranks}>{() => null}</Result>}
            <Result query={tableQuery}>{data => <Table rowKey="rankId" size="small" dataSource={data.items} loading={ranks.loading ? { spinning: true, delay: 150 } : false}
                pagination={{ current: data.currentPage, pageSize: data.pageSize, total: data.total, showSizeChanger: true, pageSizeOptions: ['10', '20', '50', '100'] }}
                onChange={onChange} scroll={{ x: 900 }} columns={[
                // Empty sorter objects enable server sorting and preserve our controlled sortOrder in ResizeTable.
                    { title: t('slowRankList.Rank ID'), dataIndex: 'rankId', key: 'rankId', sorter: {}, sortOrder: query.orderBy === 'rankId' ? query.order : null },
                    ...TRANSPORTS.map(type => ({
                        title: `${type} ${t('searchCriteria.Bandwidth')} (GB)`,
                        key: type,
                        render: (_: unknown, row: RankStatistics) => row.bandwidth[type]?.toFixed(4) ?? '-',
                        sorter: {},
                        sortOrder: query.orderBy === 'bandwidth' && query.bandType === type ? query.order : null,
                    })),
                    { title: t('tableHead.Operation'), render: (_, row) => <Button type="link" onClick={() => setRankId(row.rankId)}>{t('sessionTitle.BandwidthAnalysis')}</Button> },
                ]} />}</Result></Section>
    </div>{rankId !== undefined && <BandwidthDrawer api={api} scope={scope} rankId={rankId}
        dark={dark} onClose={() => setRankId(undefined)} />}</>;
});
