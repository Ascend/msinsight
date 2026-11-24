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
import React from 'react';
import { useTheme } from '@emotion/react';
import { Empty } from '@insight/lib/components';
import { useTranslation } from 'react-i18next';
import type { CommunicationApi } from '../../api';
import type { Scope, Packet } from '../../domain/types';
import { useQuery, type Query } from '../../state/useQuery';
import { Result } from '../../components/Section';
import { Chart } from '../../components/Chart';
import { Subtitle } from '../../components/layout';
import { PacketGrid, PacketCell, PacketEmpty } from './styles';

interface Props { api: CommunicationApi; scope: Scope; rankId: string; dark: boolean }
interface PacketQuery extends Query<Packet[] | null> { type: string }

function usePackets({ api, scope, rankId }: Props, type: string): PacketQuery {
    const query = useQuery(JSON.stringify([scope, rankId, type]), true, signal => api.packets(scope, rankId, type, signal));
    return { ...query, type };
}

export function PacketAnalysis(props: Props): JSX.Element {
    const { t } = useTranslation('communication');
    const queries = [usePackets(props, 'HCCS'), usePackets(props, 'PCIE'), usePackets(props, 'RDMA'), usePackets(props, 'SIO'), usePackets(props, 'UB')];
    const empty = queries.every(query => !query.loading && !query.error && query.data !== undefined && !query.data?.length);
    return <section aria-label={t('sessionTitle.PacketDistribution')}>
        <Subtitle>{t('sessionTitle.PacketDistribution')}</Subtitle>
        <PacketGrid>{empty
            ? <PacketEmpty image={Empty.PRESENTED_IMAGE_SIMPLE} description={t('No Data', { ns: 'translation' })} />
            : queries.map(query => <PacketPanel key={query.type} query={query} dark={props.dark} />)}
        </PacketGrid>
    </section>;
}

function PacketPanel({ query, dark }: { query: PacketQuery; dark: boolean }): JSX.Element {
    const { t } = useTranslation('communication');
    if (query.data === null) { return <></>; }
    return <PacketCell><Result query={query}>{data => data?.length
        ? <Packets data={data} type={query.type} dark={dark} />
        : <div><Subtitle>{query.type}</Subtitle><Empty description={t('No Data', { ns: 'translation' })} /></div>}
    </Result></PacketCell>;
}

function Packets({ data, type, dark }: { data: Packet[]; type: string; dark: boolean }): JSX.Element {
    const { t } = useTranslation('communication'); const theme = useTheme();
    return <Chart dark={dark} label={`${type} ${t('sessionTitle.PacketDistribution')}`} option={{
        title: { text: type, left: 'center', textStyle: { fontSize: Number.parseFloat(theme.fontSizeSmall), fontWeight: 600 } },
        tooltip: { trigger: 'axis', renderMode: 'richText' },
        legend: { top: 30 },
        grid: { left: 65, right: 65, bottom: 65, top: 85 },
        xAxis: { type: 'category', name: t('Packet Size(MB)'), nameLocation: 'middle', nameGap: 35, data: data.map(item => String(Number(item.size.toFixed(4)))) },
        yAxis: [{ type: 'value', name: t('Packet Number') }, { type: 'value', name: 'GB/s', splitLine: { show: false } }],
        series: [{ name: t('Packet Number'), type: 'bar', data: data.map(item => item.count) },
            { name: `${t('searchCriteria.Bandwidth')}(GB/s)`, type: 'line', yAxisIndex: 1, data: data.map(item => item.bandwidth) }],
    }} />;
}
