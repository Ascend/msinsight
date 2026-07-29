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

import { render } from '@testing-library/react';
import React from 'react';
import type { CommunicationBandwidthInfo } from '../../entity/data';
import { CommunicationBandwidthInfoTable } from './CommunicationBandwidthInfoTable';

jest.mock('react-i18next', () => ({
    useTranslation: (): { t: (key: string) => string } => ({
        t: (key: string): string => key,
    }),
}));

jest.mock('@insight/lib/resize', () => {
    const ReactForMock = require('react');
    return {
        ResizeTable: ({ columns, dataSource }: { columns: any[]; dataSource: any[] }): JSX.Element => ReactForMock.createElement(
            'table',
            {},
            ReactForMock.createElement('thead', {}, ReactForMock.createElement(
                'tr',
                {},
                columns.map(column => ReactForMock.createElement('th', { key: column.dataIndex }, column.title)),
            )),
            ReactForMock.createElement(
                'tbody',
                {},
                dataSource.map((record, rowIndex) => ReactForMock.createElement(
                    'tr',
                    { key: `${record.transportType}-${rowIndex}` },
                    columns.map(column => ReactForMock.createElement(
                        'td',
                        { key: column.dataIndex },
                        record[column.dataIndex],
                    )),
                )),
            ),
        ),
    };
}, { virtual: true });

describe('CommunicationBandwidthInfoTable', () => {
    it('does not render when communication bandwidth data is unavailable', () => {
        const { container, rerender } = render(<CommunicationBandwidthInfoTable />);
        expect(container).toBeEmptyDOMElement();

        rerender(<CommunicationBandwidthInfoTable details={[]} />);
        expect(container).toBeEmptyDOMElement();
    });

    it('renders only the requested metrics in the fixed transport order without adding missing rows', () => {
        const details: CommunicationBandwidthInfo[] = [
            { transportType: 'SIO', transitSize: 0, transitTime: 0, bandwidth: 0 },
            { transportType: 'HCCS', transitSize: 1907.497088, transitTime: 193.3286005, bandwidth: 9.8666 },
        ];

        const { getAllByRole, getByText, queryByText } = render(<CommunicationBandwidthInfoTable details={details} />);

        expect(getByText('Communication Bandwidth Info')).toBeInTheDocument();
        const rows = getAllByRole('row');
        expect(rows).toHaveLength(3);
        expect(rows[0]).toHaveTextContent('Transport Type');
        expect(rows[0]).toHaveTextContent('Transit Size(MB)');
        expect(rows[0]).toHaveTextContent('Transit Time(ms)');
        expect(rows[0]).toHaveTextContent('Bandwidth(GB/s)');
        expect(rows[1]).toHaveTextContent('HCCS');
        expect(rows[1]).toHaveTextContent('1907.497088');
        expect(rows[1]).toHaveTextContent('193.3286005');
        expect(rows[1]).toHaveTextContent('9.8666');
        expect(rows[2]).toHaveTextContent('SIO');
        expect(rows[2].textContent?.match(/0/g)).toHaveLength(3);
        expect(queryByText('RDMA')).not.toBeInTheDocument();
    });

    it('orders all supported transport types consistently', () => {
        const details: CommunicationBandwidthInfo[] = [
            { transportType: 'SDMA', transitSize: 4, transitTime: 4, bandwidth: 4 },
            { transportType: 'PCIE', transitSize: 3, transitTime: 3, bandwidth: 3 },
            { transportType: 'RDMA', transitSize: 1, transitTime: 1, bandwidth: 1 },
            { transportType: 'SIO', transitSize: 5, transitTime: 5, bandwidth: 5 },
            { transportType: 'HCCS', transitSize: 2, transitTime: 2, bandwidth: 2 },
        ];

        const { getAllByRole } = render(<CommunicationBandwidthInfoTable details={details} />);
        expect(getAllByRole('row').slice(1).map(row => row.firstChild?.textContent))
            .toEqual(['RDMA', 'HCCS', 'PCIE', 'SDMA', 'SIO']);
    });
});
