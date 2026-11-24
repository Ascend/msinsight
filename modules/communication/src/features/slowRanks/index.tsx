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
import { Trans, useTranslation } from 'react-i18next';
import { Tooltip } from '@insight/lib/components';
import { ResizeTable as Table } from '@insight/lib/resize';
import type { SlowRanks as Data } from '../../domain/types';
import { SlowRankPanel, SlowRankTitle, SlowRankDescription, SlowRankTable } from './styles';

const milliseconds = (value: number): string => (value / 1000).toFixed(6);
export function SlowRanks({ data }: { data: Data }): JSX.Element {
    const { t } = useTranslation('communication');
    const title = (label: string, help: string): JSX.Element => <Tooltip title={t(`slowRankList.RankTable.tooltip.${help}`)}><span>{t(`slowRankList.${label}`)} ⓘ</span></Tooltip>;
    if (!data.hasAdvice) { return <></>; }
    return <SlowRankPanel><SlowRankTitle>{t('Advice')}</SlowRankTitle>
        <SlowRankDescription>{data.data.length
            ? <Trans t={t} i18nKey="modern.slowRankAdvice" values={{ fastRankId: data.fastRankId, fastTotalElapseTime: milliseconds(data.fastTotalElapseTime) }}
                components={{ strong: <strong /> }} />
            : t('slowRankList.NoSlowRankDesc')}</SlowRankDescription>
        {data.data.length > 0 && <SlowRankTable><Table size="small" rowKey="rankId" dataSource={data.data} pagination={false} columns={[
            { title: t('slowRankList.Rank ID'), dataIndex: 'rankId', width: '20%' },
            { title: title('Elapse Time Difference(ms)', 'ElapseTimeDifference'), dataIndex: 'totalDiffTime', render: milliseconds },
            { title: title('Elapse Time(ms)', 'ElapseTime'), dataIndex: 'totalElapseTime', render: milliseconds },
        ]} /></SlowRankTable>}
    </SlowRankPanel>;
}
