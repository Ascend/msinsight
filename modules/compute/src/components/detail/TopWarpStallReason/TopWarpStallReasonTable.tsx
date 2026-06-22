/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
import React from 'react';
import { useTranslation } from 'react-i18next';
import { ResizeTable } from '@insight/lib/resize';
import type { TFunction } from 'i18next';
import { getContextElement, renderExpandColumn } from '../../Common';
import { type Theme, useTheme } from '@emotion/react';
import { CompareData } from '../../../utils/interface';
import { type IStallReasonData } from './Index';

interface Iprops {
    data: Array<CompareData<IStallReasonData>>;
    isCompared: boolean;
}

interface Iobj {
    key?: string;
    name: string;
    value: number;
    source?: string;
    children?: Iobj[];
}

function getFullCols({ isCompared, setExpandedKeys, theme, t }: {isCompared: boolean;
    setExpandedKeys: React.Dispatch<React.SetStateAction<string[]>>; theme: Theme; t: TFunction;}): any[] {
    const cols = [
        {
            title: t('Stall Reason'),
            dataIndex: 'name',
            ellipsis: true,
        },
        {
            title: t('Stall Count'),
            dataIndex: 'value',
            ellipsis: true,
            render: (text: string, record: any): JSX.Element => getContextElement(text, record, theme, t),
        },
    ];
    if (isCompared) {
        cols.push({
            title: t('Details'),
            dataIndex: 'action',
            ellipsis: true,
            render: (text: string, record: any): JSX.Element => renderExpandColumn(record, setExpandedKeys, t),
        });
        cols.splice(1, 0, {
            title: t('Source'),
            dataIndex: 'source',
            ellipsis: true,
            render: (text: string): JSX.Element => <div>{text}</div>,
        });
    }
    return cols;
}

function TopWarpStallReasonTable({ data, isCompared }: Iprops): JSX.Element {
    const { t } = useTranslation('details');
    const theme = useTheme();
    const [expandedRowKeys, setExpandedKeys] = React.useState<string[]>([]);
    const columns = getFullCols({ isCompared, setExpandedKeys, theme, t });
    const dataSource = data.map((item, index) => {
        const compare: Iobj = { name: item.compare.name, value: item.compare.value, source: t('Comparison') };
        if (!isCompared) {
            return { ...compare, key: `${item.compare.name}_${index}` };
        }
        const diff: Iobj = { name: item.diff.name, value: item.diff.value, source: t('Difference') };
        const baseline: Iobj = { name: item.baseline.name, value: item.baseline.value, source: t('Baseline') };
        diff.children = [compare, baseline];
        return { ...diff, key: `${item.diff.name}_${index}` };
    });

    return (
        <div data-testId="topWarpStallReasonTable">
            <ResizeTable
                size="small"
                columns={columns}
                dataSource={dataSource}
                scroll={dataSource.length > 10 ? { y: 500 } : undefined}
                pagination={false}
                expandable={{
                    expandIcon: () => <></>,
                    expandedRowKeys,
                }}
            />
        </div>
    );
}

export default TopWarpStallReasonTable;
