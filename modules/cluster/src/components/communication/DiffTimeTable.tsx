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

import React, { ReactNode } from 'react';
import { type TableColumnsType } from 'antd';
import type { TFunction } from 'i18next';
import styled from '@emotion/styled';
import { Button, MITooltipHelp } from '@insight/lib/components';
import { ResizeTable } from '@insight/lib/resize';
import { Hit } from '@insight/lib/utils';
import { SlowRankListItem, SlowRankOpListItem } from '../../utils/interface';
import { useTranslation } from 'react-i18next';
import eventBus from '../../utils/eventBus';

interface DiffTimeTableProps {
    fastTotalElapseTime: number;
    fastRankId: string;
    data: SlowRankListItem[];
    loading: boolean;
}

interface ExpandedDataType extends SlowRankOpListItem {
    topK: string;
    rankId: string;
}

const usToMs = (value: number): string => {
    return (value / 1000).toFixed(6);
};

const StyledAlert = styled.div`
    color: ${({ theme }) => theme.textColorTertiary}
`;

const ColumnTitle = (props: { name: string; tooltip: string }): JSX.Element => {
    const { name, tooltip } = props;

    return <div className={'flex align-center'}>
        <div className={'mr-6'}>{name}</div>
        <MITooltipHelp title={tooltip}></MITooltipHelp>
    </div>;
};

// 外层表格列配置
const useRankColumns = (t: TFunction<'communication', 'slowRankList'>): TableColumnsType<SlowRankListItem> => {
    return [
        {
            title: t('Rank ID'),
            dataIndex: 'rankId',
            key: 'rankId',
        },
        {
            title: <ColumnTitle name={t('Elapse Time Difference(ms)')} tooltip={t('RankTable.tooltip.ElapseTimeDifference')}></ColumnTitle>,
            dataIndex: 'totalDiffTime',
            key: 'totalDiffTime',
            render: usToMs,
        },
        {
            title: <ColumnTitle name={t('Elapse Time(ms)')} tooltip={t('RankTable.tooltip.ElapseTime')}></ColumnTitle>,
            dataIndex: 'totalElapseTime',
            key: 'totalElapseTime',
            render: usToMs,
        },
    ];
};

// 子表格列配置
const useOpColumns = (t: TFunction<'communication', 'slowRankList'>): TableColumnsType<ExpandedDataType> => {
    return [
        {
            title: t('Index'),
            dataIndex: 'topK',
            key: 'topK',
            width: 100,
        },
        {
            title: t('Operator Name'),
            dataIndex: 'name',
            key: 'name',
        },
        {
            title: <ColumnTitle name={t('Elapse Time Difference(ms)')} tooltip={t('ExpandedTable.tooltip.ElapseTimeDifference')}></ColumnTitle>,
            dataIndex: 'diffTime',
            key: 'diffTime',
            render: usToMs,
        },
        {
            title: <ColumnTitle name={t('Elapse Time on Current Rank(ms)')} tooltip={t('ExpandedTable.tooltip.ElapseTimeOnCurrentRank')}></ColumnTitle>,
            dataIndex: 'elapseTime',
            key: 'elapseTime',
            render: usToMs,
        },
        {
            title: <ColumnTitle name={t('Elapse Time on Fastest Rank(ms)')} tooltip={t('ExpandedTable.tooltip.ElapseTimeOnFastestRank')}></ColumnTitle>,
            dataIndex: 'maxTime',
            key: 'maxTime',
            render: usToMs,
        },
        {
            title: t('Action'),
            key: 'action',
            width: 160,
            render: (_, record) => (
                <Button type="link" onClick={(): void => {
                    eventBus.emit('onClickSlowRankOp', {
                        rankId: record.rankId,
                        name: record.name,
                        startValue: Math.floor(record.maxStartTime / 1000),
                        endValue: Math.ceil((record.maxStartTime + record.maxTime) / 1000),
                    });
                }}>
                    {t('Find in Communication')}
                </Button>
            ),
        },
    ];
};

const DiffTimeTable: React.FC<DiffTimeTableProps> = ({ data, fastTotalElapseTime, fastRankId, loading }) => {
    const { t } = useTranslation('communication', { keyPrefix: 'slowRankList' });
    const rankColumns = useRankColumns(t);
    const opColumns = useOpColumns(t);

    // 展开行渲染子表格
    const expandedRowRender = (record: SlowRankListItem): ReactNode => {
        const opData = record.opList.map((op, idx) => ({
            topK: `Top ${idx + 1}`,
            rankId: record.rankId,
            ...op,
        }));

        return (
            <ResizeTable
                columns={opColumns}
                dataSource={opData}
                rowKey={(row) => row.topK}
                pagination={false}
                size="small"
            />
        );
    };
    return (
        data?.length > 0
            ? <>

                <div className={'mb-16'}>
                    <Hit type={ 'advice' }
                        title={ `${t('Advice')}:` }
                        text={ t('SlowRankDesc', { fastTotalElapseTime: usToMs(fastTotalElapseTime), fastRankId }) }/>
                </div>

                <ResizeTable
                    loading={loading}
                    columns={rankColumns}
                    dataSource={data}
                    expandable={{
                        expandedRowRender,
                        rowExpandable: (record) => record.opList && record.opList.length > 0,
                    }}
                    rowKey={(row) => row.rankId}
                    pagination={false}
                />
            </>
            : <StyledAlert>{t('NoSlowRankDesc')}</StyledAlert>
    );
};

export default DiffTimeTable;
