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

import styled from '@emotion/styled';
import type { ColumnsType } from 'antd/es/table';
import React, { useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { ResizeTable } from '@insight/lib/resize';
import type { CommunicationBandwidthInfo } from '../../entity/data';

const TRANSPORT_TYPE_ORDER = ['RDMA', 'HCCS', 'PCIE', 'SDMA', 'SIO'];
const TRANSPORT_TYPE_INDEX = new Map(TRANSPORT_TYPE_ORDER.map((type, index) => [type, index]));

const StyledBandwidthInfo = styled.div`
    width: 100%;
    color: ${(props): string => props.theme.tableTextColor};
    font-size: 12px;

    .communicationBandwidthInfoTitle {
        font-weight: bold;
        margin: 8px 24px;
    }

    .communicationBandwidthInfoTable {
        margin: 0 24px 8px;
    }
`;

const sortByTransportType = (details: CommunicationBandwidthInfo[]): CommunicationBandwidthInfo[] => {
    return [...details].sort((left, right) => {
        const leftIndex = TRANSPORT_TYPE_INDEX.get(left.transportType.toUpperCase()) ?? Number.MAX_SAFE_INTEGER;
        const rightIndex = TRANSPORT_TYPE_INDEX.get(right.transportType.toUpperCase()) ?? Number.MAX_SAFE_INTEGER;
        return leftIndex - rightIndex || left.transportType.localeCompare(right.transportType);
    });
};

interface CommunicationBandwidthInfoTableProps {
    details?: CommunicationBandwidthInfo[];
}

export const CommunicationBandwidthInfoTable = ({ details }: CommunicationBandwidthInfoTableProps): JSX.Element | null => {
    const { t } = useTranslation('timeline', { keyPrefix: 'sliceDetail' });
    const dataSource = useMemo(() => sortByTransportType(details ?? []), [details]);
    const columns = useMemo<ColumnsType<CommunicationBandwidthInfo>>(() => [
        {
            title: t('Transport Type'),
            dataIndex: 'transportType',
            ellipsis: true,
        },
        {
            title: t('Transit Size(MB)'),
            dataIndex: 'transitSize',
            ellipsis: true,
        },
        {
            title: t('Transit Time(ms)'),
            dataIndex: 'transitTime',
            ellipsis: true,
        },
        {
            title: t('Bandwidth(GB/s)'),
            dataIndex: 'bandwidth',
            ellipsis: true,
        },
    ], [t]);

    if (dataSource.length === 0) {
        return null;
    }

    return <StyledBandwidthInfo>
        <div className="communicationBandwidthInfoTitle">{t('Communication Bandwidth Info')}</div>
        <ResizeTable<CommunicationBandwidthInfo>
            allowCopy={false}
            className="communicationBandwidthInfoTable"
            columns={columns}
            dataSource={dataSource}
            pagination={false}
            rowKey={(record): string => record.transportType}
            size="small"
        />
    </StyledBandwidthInfo>;
};
