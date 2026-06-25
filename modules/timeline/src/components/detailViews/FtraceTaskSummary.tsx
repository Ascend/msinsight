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

import { observer } from 'mobx-react';
import React, { useEffect, useState } from 'react';
import { useTranslation } from 'react-i18next';
import { ResizeTable } from '@insight/lib/resize';
import { StyledEmpty } from '@insight/lib/utils';
import { ftraceUnifiedColumns, queryFtraceStat, getPageData } from './Common';
import type { CardMetaData } from '../../entity/data';
import { getTimeOffset } from '../../insight/units/utils';
import { DETAIL_HEADER_HEIGHT_ETC_PX, SelectContentViewProps } from './SystemView';

const defaultPage = { current: 1, pageSize: 10, total: 0 };
const defaultSorter = { field: 'running', order: 'descend' };

/*
 * Ftrace 任务汇总表格
 * 前 3 列（运行CPU、进程名、线程名）支持筛选 + 排序，后 8 列只支持排序
 */
export const FtraceTaskSummary = observer((props: SelectContentViewProps) => {
    const { t } = useTranslation('timeline', { keyPrefix: 'tableHead' });

    const [dataSource, setDataSource] = useState<any[]>([]);
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(defaultSorter);
    const [filters, setFilters] = useState<Record<string, string[]>>({});
    const [loading, setLoading] = useState(false);

    const status = props.session.units.find((unit: any) =>
        (unit.metadata as CardMetaData)?.cardId === props.card?.cardId,
    )?.phase;

    /**
     * 检查 card 类型是否匹配（非 Ftrace 类型不应请求 Ftrace 数据）
     */
    const isCardTypeInvalid = (): boolean => {
        const cardRankInfo = Array.from(props.session.rankCardInfoMap.values()).find(
            (info) => info.rankInfo.rankId === props.card?.cardId && info.dbPath === props.card?.dbPath,
        );
        return cardRankInfo?.isFtrace !== true;
    };

    /**
     * 请求 Ftrace 数据（参考 BaseSummary 的 updateData 逻辑，传递排序/过滤/时间范围参数）
     */
    const fetchData = async (
        pages: typeof defaultPage,
        sorters: typeof defaultSorter,
        filtersConditions: typeof filters,
    ): Promise<void> => {
        if (props.card === undefined || props.card.cardId === '' || isCardTypeInvalid()) {
            setDataSource([]);
            setPage(defaultPage);
            return;
        }

        setLoading(true);
        // 计算时间范围
        let startTime = props.session.timeAnalysisRange?.[0] ?? 0;
        startTime = startTime < 0 ? 0 : startTime;
        let endTime = props.session.timeAnalysisRange?.[1] ?? 0;
        endTime = endTime < 0 ? 0 : endTime;
        const timestampOffset = getTimeOffset(props.session, props.card);

        // 构建过滤条件，参考 KernelDetails 的 filterTypes 构建方式
        const filterConditions: string[] = [];
        Object.entries(filtersConditions).forEach(([key, values]) => {
            if (values && values.length > 0) {
                filterConditions.push(JSON.stringify({ columnName: key, value: values[0] }));
            }
        });

        try {
            const res = await queryFtraceStat({
                rankId: props.card.cardId,
                dbPath: props.card.dbPath,
                current: pages.current,
                pageSize: pages.pageSize,
                orderBy: sorters.order ? sorters.field : defaultSorter.field,
                order: sorters.order ?? defaultSorter.order,
                startTime: Math.floor(startTime + timestampOffset),
                endTime: Math.ceil(endTime + timestampOffset),
                filterCondition: filterConditions,
            });

            // 确保数值字段为数字类型（后端返回字符串，前端需转为数字以支持表格排序）
            const numKeys = ['runnable', 'running', 'sleeping', 'context_switch_count',
                'soft_irq_count', 'soft_irq_duration', 'hard_irq_count', 'hard_irq_duration'];
            const data = (res.data ?? []).map(row => ({
                ...row,
                ...Object.fromEntries(numKeys.map(f => [f, Number(row[f]) || 0])),
            }));

            setDataSource(data);
            setPage(prev => ({ ...prev, total: res.count }));
        } finally {
            setLoading(false);
        }
    };

    useEffect(() => {
        if (status === 'download') {
            fetchData(page, sorter, filters);
        }
    }, [status, sorter, filters, page.current, page.pageSize, props.card?.cardId, props.card?.dbPath, props.session.timeAnalysisRange]);

    // 列定义：将 title 映射为 i18n 翻译文本
    const columns = ftraceUnifiedColumns.map(col => ({ ...col, title: t(col.title as string) }));

    return (
        (props.card !== undefined && props.card.cardId !== '')
            ? <ResizeTable
                onChange={(_pagination: any, newFilters: any, newSorter: any): void => {
                    const field = typeof newSorter?.field === 'string' ? newSorter.field : '';
                    const order = newSorter?.order == null ? '' : String(newSorter.order);
                    setSorter({ field, order });
                    setFilters(newFilters);
                }}
                loading={loading}
                pagination={getPageData(page, setPage)}
                dataSource={dataSource}
                columns={columns}
                scroll={{ y: props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX }}
                size="small" />
            : <div style={{ display: 'flex', height: '100%' }}>
                <StyledEmpty style={{ margin: 'auto' }} />
            </div>
    );
});
