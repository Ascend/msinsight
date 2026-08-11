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

const CHILD_TRANSPORT_TYPE_ORDER = ['HCCS', 'PCIE', 'SIO'];
const KNOWN_TRANSPORT_TYPES = new Set(['SDMA', 'RDMA', ...CHILD_TRANSPORT_TYPE_ORDER]);

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

interface CommunicationBandwidthInfoTableRow extends CommunicationBandwidthInfo {
    children?: CommunicationBandwidthInfoTableRow[];
}

const buildTableDataSource = (details: CommunicationBandwidthInfo[]): CommunicationBandwidthInfoTableRow[] => {
    const rowsByTransportType = (transportType: string): CommunicationBandwidthInfoTableRow[] => details
        .filter(detail => detail.transportType.toUpperCase() === transportType);
    const children = CHILD_TRANSPORT_TYPE_ORDER.flatMap(rowsByTransportType);
    const sdma = rowsByTransportType('SDMA')[0];
    const roots: CommunicationBandwidthInfoTableRow[] = [];

    if (sdma !== undefined) {
        roots.push(children.length > 0 ? { ...sdma, children } : sdma);
    } else {
        roots.push(...children);
    }

    roots.push(...rowsByTransportType('RDMA'));
    roots.push(...details
        .filter(detail => !KNOWN_TRANSPORT_TYPES.has(detail.transportType.toUpperCase()))
        .sort((left, right) => left.transportType.localeCompare(right.transportType)));
    return roots;
};

interface CommunicationBandwidthInfoTableProps {
    details?: CommunicationBandwidthInfo[];
}

export const CommunicationBandwidthInfoTable = ({ details }: CommunicationBandwidthInfoTableProps): JSX.Element | null => {
    const { t } = useTranslation('timeline', { keyPrefix: 'sliceDetail' });
    const dataSource = useMemo(() => buildTableDataSource(details ?? []), [details]);
    const columns = useMemo<ColumnsType<CommunicationBandwidthInfoTableRow>>(() => [
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
        <ResizeTable<CommunicationBandwidthInfoTableRow>
            className="communicationBandwidthInfoTable"
            columns={columns}
            dataSource={dataSource}
            expandable={{ defaultExpandedRowKeys: ['SDMA'] }}
            pagination={false}
            rowKey="transportType"
            size="small"
        />
    </StyledBandwidthInfo>;
};
