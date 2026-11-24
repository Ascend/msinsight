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
import { ResizeTable as Table } from '@insight/lib/resize';
import type { CommunicationApi } from '../../api';
import type { Scope, Transport } from '../../domain/types';
import { useQuery } from '../../state/useQuery';
import { Result } from '../../components/Section';
import { useTranslation } from 'react-i18next';
import { Subtitle } from '../../components/layout';
import { DrawerPanel } from './styles';
import { PacketAnalysis } from './PacketAnalysis';

const formatMeasurement = (value: number | null): string => value?.toFixed(4) ?? '/';

export function BandwidthDrawer({ api, scope, rankId, dark, onClose }: { api: CommunicationApi; scope: Scope; rankId: string; dark: boolean; onClose: () => void }): JSX.Element {
    const { t } = useTranslation('communication');
    const query = useQuery(JSON.stringify([scope, rankId]), true, signal => api.transports(scope, rankId, signal));
    const columns = [
        { title: t('searchCriteria.TransportType'), dataIndex: 'transportType' },
        { title: `${t('searchCriteria.TransitSize')}(MB)`, dataIndex: 'transitSize', render: formatMeasurement },
        { title: `${t('searchCriteria.TransitTime')}(ms)`, dataIndex: 'transitTime', render: formatMeasurement },
        { title: `${t('searchCriteria.Bandwidth')}(GB/s)`, dataIndex: 'bandwidth', render: formatMeasurement },
        { title: t('tableHead.Large Packet Ratio'), dataIndex: 'largePacketRatio', render: (value: number | null, row: Transport) => row.transportType === 'SDMA' ? '/' : formatMeasurement(value) },
    ];
    return <DrawerPanel open title={`${t('sessionTitle.BandwidthAnalysis')} · Rank ${rankId}`} width="82%" placement="right" mask maskClosable
        keyboard autoFocus onClose={onClose} destroyOnClose>
        <PacketAnalysis api={api} scope={scope} rankId={rankId} dark={dark} />
        <Subtitle>{t('modern.transfer')}</Subtitle><Result query={query} empty={query.data?.length === 0}>{data => {
            const sdma = data.find(row => row.transportType === 'SDMA');
            const children = data.filter(row => ['HCCS', 'PCIE', 'SIO'].includes(row.transportType));
            const rows = ['SDMA', 'RDMA', 'UB'].flatMap(type => {
                const row = data.find(item => item.transportType === type);
                return row ? [row === sdma && children.length ? { ...row, children } : row] : [];
            });
            return <Table size="small" rowKey="transportType" columns={columns} dataSource={rows} pagination={false} defaultExpandAllRows />;
        }}</Result>
    </DrawerPanel>;
}
