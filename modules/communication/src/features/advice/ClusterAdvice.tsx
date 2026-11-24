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
import React, { useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { MITooltipHelp } from '@insight/lib/components';
import { ResizeTable } from '@insight/lib/resize';
import type { resources } from '@insight/lib/i18n';
import type { Advice } from '../../domain/types';
import { AdviceParagraph } from '../../components/AdvicePanel';
import { AdviceList, DiagnosisCard, DiagnosisTitle, DiagnosisTable, Recommendations } from './styles';

type Strings = typeof resources.enUS.communication;
type RecommendationKey = keyof Strings['index'] & keyof Strings['title'];
const recommendations: Record<string, RecommendationKey[]> = {
    'Bandwidth Contention Analysis': ['Bandwidth Contention'],
    'Byte Alignment Analysis': ['Byte Alignment Analysis'],
    'Communication Retransmission Analysis': ['RDMA Transmission Time', 'Network Configuration'],
    'Packet Analysis': ['Data Parallelism', 'Memory Optimization', 'Adopt'],
};

/** Render the original column-oriented diagnostic data without dropping columns or rows. */
function AdviceCard({ item }: { item: Advice }): JSX.Element {
    const { t } = useTranslation('communication');
    const hasRecommendation = Object.hasOwn(recommendations, item.name);
    const title = hasRecommendation ? t(item.name) : item.name;
    const tooltip = t(`tooltip.${item.name}`, { defaultValue: '' });
    const { columns, rows } = useMemo(() => {
        const entries = Object.entries(item.statistics);
        const length = Math.max(0, ...entries.map(([, values]) => values.length));
        return {
            columns: entries.map(([key], index) => ({ title: t(`tableHead.${key}`, { defaultValue: key }), dataIndex: `column${index}`, key, width: 180 })),
            rows: Array.from({ length }, (_, index) => ({
                key: index,
                ...Object.fromEntries(entries.map(([, values], column) => [`column${column}`, values[index] ?? '-'])),
            })),
        };
    }, [item.statistics, t]);
    const adviceKeys = hasRecommendation ? recommendations[item.name] : [];
    return <DiagnosisCard>
        <DiagnosisTitle>{title}{tooltip && <> <MITooltipHelp title={tooltip} /></>}</DiagnosisTitle>
        <AdviceParagraph><strong>{t('modern.dataAnalysis')}</strong>{rows.length === 0 && t('index.No problematic operators')}</AdviceParagraph>
        {rows.length > 0 && <DiagnosisTable><ResizeTable size="small" rowKey="key" columns={columns} dataSource={rows}
            scroll={{ x: 'max-content' }} pagination={rows.length > 10 ? { defaultPageSize: 10, showSizeChanger: true, pageSizeOptions: ['10', '20', '50', '100'] } : false} /></DiagnosisTable>}
        {adviceKeys.length > 0 && <Recommendations>
            {adviceKeys.map((key, index) => <AdviceParagraph key={key}>
                {index === 0 && <strong>{t('modern.recommendation')}</strong>}{t(`title.${key}`)} {t(`index.${key}`)}
            </AdviceParagraph>)}
        </Recommendations>}
    </DiagnosisCard>;
}

export function ClusterAdvice({ data }: { data: Advice[] }): JSX.Element {
    return <AdviceList>{data.map(item => <AdviceCard key={item.name} item={item} />)}</AdviceList>;
}
