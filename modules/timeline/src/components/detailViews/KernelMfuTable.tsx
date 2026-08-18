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
import React, { useEffect, useMemo, useRef, useState } from 'react';
import { useTranslation } from 'react-i18next';
import styled from '@emotion/styled';
import type { TableColumnsType } from 'antd';
import type { FilterValue } from 'antd/es/table/interface';
import { createSmartDebounceRequestFunc } from '@insight/lib';
import { Tooltip } from '@insight/lib/components';
import { fetchColumnFilterProps, ResizeTable } from '@insight/lib/resize';
import { StyledEmpty } from '@insight/lib/utils';
import { queryKernelMfuList } from '../../api/request';
import type { KernelMfuListParams, KernelMfuRow } from '../../api/interface';
import type { SelectContentViewProps } from './SystemView';
import { DETAIL_HEADER_HEIGHT_ETC_PX } from './SystemView';
import { getPageData, getDefaultColumData } from './Common';

const TableContainer = styled.div`
    display: flex;
    flex-direction: column;
    height: 100%;
    min-height: 0;

    .ant-table-wrapper {
        flex: 1;
        min-height: 0;
    }
`;

const ErrorState = styled.div`
    display: flex;
    align-items: center;
    justify-content: center;
    height: 100%;
    color: ${(props): string => props.theme.textColorSecondary};
`;

interface KernelMfuPage {
    current: number;
    pageSize: number;
    total: number;
}

interface KernelMfuSorter {
    field?: string;
    order?: 'ascend' | 'descend';
}

const createDefaultPage = (): KernelMfuPage => ({ current: 1, pageSize: 10, total: 0 });
const DEFAULT_SORTER: KernelMfuSorter = {};

const debouncedFetch = createSmartDebounceRequestFunc(
    (params: KernelMfuListParams) => queryKernelMfuList(params),
    { delay: 100 },
);

const kernelMfuOrderByFields: string[] = [
    'rankId',
    'opName',
    'kernelName',
    'kernelStartNs',
    'kernelEndNs',
    'kernelDurationNs',
    'mfu',
    'actualTflops',
    'chipPeakTflops',
    'flops',
    'flopsOpName',
    'inputShapes',
    'outputShapes',
];

const isKernelMfuOrderBy = (value: unknown): value is string =>
    typeof value === 'string' && kernelMfuOrderByFields.includes(value);

const renderLongText = (value: string): JSX.Element => (
    <Tooltip title={value} placement="topLeft">
        <span>{value}</span>
    </Tooltip>
);

export const KernelMfuTable = observer((props: SelectContentViewProps) => {
    const { t } = useTranslation('timeline', { keyPrefix: 'systemView' });
    const { t: tableHeadT } = useTranslation('timeline', { keyPrefix: 'tableHead' });
    const clusterPath = props.session.selectedClusterPath;
    const projectGeneration = props.session.kernelMfuProjectGeneration;
    const previousProjectGeneration = useRef(projectGeneration);
    const listRequestSequence = useRef(0);
    const [dataSource, setDataSource] = useState<KernelMfuRow[]>([]);
    const [opName, setOpName] = useState('');
    const [kernelName, setKernelName] = useState('');
    const [rankIds, setRankIds] = useState<string[]>([]);
    const [rankOptions, setRankOptions] = useState<string[]>([]);
    const [page, setPage] = useState<KernelMfuPage>(createDefaultPage);
    const [sorter, setSorter] = useState<KernelMfuSorter>(DEFAULT_SORTER);
    const [loading, setLoading] = useState(false);
    const [hasError, setHasError] = useState(false);

    const sortKey = `${sorter.field ?? ''}:${sorter.order ?? ''}`;
    const projectGenerationChanged = previousProjectGeneration.current !== projectGeneration;
    useEffect(() => {
        setDataSource([]);
        setOpName('');
        setKernelName('');
        setRankIds([]);
        setRankOptions([]);
        setPage(createDefaultPage());
        setSorter(DEFAULT_SORTER);
        setHasError(false);
        previousProjectGeneration.current = projectGeneration;
    }, [projectGeneration]);

    useEffect(() => {
        if (projectGenerationChanged || clusterPath === '') {
            return;
        }
        const requestSequence = ++listRequestSequence.current;
        const requestProjectGeneration = projectGeneration;
        const params: KernelMfuListParams = {
            clusterPath,
            current: page.current,
            pageSize: page.pageSize,
            ...(rankIds.length > 0 ? { rankIds } : {}),
            ...(opName.length > 0 ? { opName } : {}),
            ...(kernelName.length > 0 ? { kernelName } : {}),
            ...(sorter.field !== undefined && sorter.order !== undefined
                ? { orderBy: sorter.field, order: sorter.order }
                : {}),
        };
        let cancelled = false;
        const applyResponse = (response: Awaited<ReturnType<typeof queryKernelMfuList>>): void => {
            if (cancelled || requestSequence !== listRequestSequence.current ||
                requestProjectGeneration !== props.session.kernelMfuProjectGeneration ||
                clusterPath !== props.session.selectedClusterPath) {
                return;
            }
            if (!response.available) {
                setDataSource([]);
                setRankOptions([]);
                setPage((currentPage) => ({ ...currentPage, total: 0 }));
                props.session.updateKernelMfuAvailability({ available: false });
                return;
            }
            setDataSource(response.data);
            setRankOptions(response.rankOptions);
            setPage((currentPage) => ({
                ...currentPage,
                current: response.current,
                pageSize: response.pageSize,
                total: response.count,
            }));
            setHasError(false);
        };

        setLoading(true);
        setHasError(false);
        void debouncedFetch(params).then(applyResponse).catch(() => {
            if (cancelled || requestSequence !== listRequestSequence.current ||
                requestProjectGeneration !== props.session.kernelMfuProjectGeneration ||
                clusterPath !== props.session.selectedClusterPath) {
                return;
            }
            setDataSource([]);
            setRankOptions([]);
            setPage((currentPage) => ({ ...currentPage, total: 0 }));
            setHasError(true);
        }).finally(() => {
            if (!cancelled) {
                setLoading(false);
            }
        });

        return () => {
            cancelled = true;
            debouncedFetch.cancel();
        };
    }, [
        opName,
        kernelName,
        page.current,
        page.pageSize,
        projectGeneration,
        projectGenerationChanged,
        props.session,
        rankIds,
        sortKey,
        clusterPath,
    ]);

    const columns = useMemo<TableColumnsType<KernelMfuRow>>(() => [
        {
            title: tableHeadT('Rank ID'),
            dataIndex: 'rankId',
            ...getDefaultColumData('rankId'),
            filters: rankOptions.map((option) => ({ text: option, value: option })),
            width: 100,
        },
        {
            title: tableHeadT('Operator Name'),
            dataIndex: 'opName',
            ...getDefaultColumData('opName'),
            ...fetchColumnFilterProps('opName', 'Operator Name', false, undefined, (value: string): void => {
                setOpName(value);
                setPage((currentPage) => ({ ...currentPage, current: 1 }));
            }),
            width: 180,
            render: renderLongText,
        },
        {
            title: tableHeadT('Kernel Name'),
            dataIndex: 'kernelName',
            ...getDefaultColumData('kernelName'),
            ...fetchColumnFilterProps('kernelName', 'Kernel Name', false, undefined, (value: string): void => {
                setKernelName(value);
                setPage((currentPage) => ({ ...currentPage, current: 1 }));
            }),
            width: 180,
            render: renderLongText,
        },
        { title: tableHeadT('Kernel Start Time (ns)'), dataIndex: 'kernelStartNs', ...getDefaultColumData('kernelStartNs'), width: 160 },
        { title: tableHeadT('Kernel End Time (ns)'), dataIndex: 'kernelEndNs', ...getDefaultColumData('kernelEndNs'), width: 160 },
        { title: tableHeadT('Kernel Duration (ns)'), dataIndex: 'kernelDurationNs', ...getDefaultColumData('kernelDurationNs'), width: 180 },
        { title: tableHeadT('MFU'), dataIndex: 'mfu', ...getDefaultColumData('mfu'), width: 100 },
        { title: tableHeadT('Actual TFLOPS'), dataIndex: 'actualTflops', ...getDefaultColumData('actualTflops'), width: 140 },
        { title: tableHeadT('Chip Peak TFLOPS'), dataIndex: 'chipPeakTflops', ...getDefaultColumData('chipPeakTflops'), width: 160 },
        { title: tableHeadT('FLOPs'), dataIndex: 'flops', ...getDefaultColumData('flops'), width: 120 },
        { title: tableHeadT('FLOPs Operator Name'), dataIndex: 'flopsOpName', ...getDefaultColumData('flopsOpName'), width: 180, render: renderLongText },
        { title: tableHeadT('Input Shape'), dataIndex: 'inputShapes', ...getDefaultColumData('inputShapes'), width: 180, render: renderLongText },
        { title: tableHeadT('Output Shape'), dataIndex: 'outputShapes', ...getDefaultColumData('outputShapes'), width: 180, render: renderLongText },
    ], [rankOptions, tableHeadT]);

    const scrollHeight = Math.max(100, props.bottomHeight - DETAIL_HEADER_HEIGHT_ETC_PX - 40);
    if (hasError) {
        return <ErrorState>{t('Kernel MFU Query Failed')}</ErrorState>;
    }
    if (clusterPath === '') {
        return <StyledEmpty style={{ margin: 'auto' }} />;
    }
    return (
        <TableContainer>
            <ResizeTable
                key={`kernelMfu-${clusterPath}-${projectGeneration}`}
                rowKey={(record, index): string => `${record.rankId}_${record.kernelStartNs}_${record.kernelEndNs}_${index}`}
                onChange={(_pagination: unknown, filters: Record<string, FilterValue | null>, newSorter: unknown, extra: { action: string }): void => {
                    if (extra.action === 'filter') {
                        const rankFilter = filters.rankId;
                        const nextRankIds = Array.isArray(rankFilter)
                            ? rankFilter.filter((value): value is string => typeof value === 'string')
                            : [];
                        setRankIds(nextRankIds);
                        setPage((currentPage) => ({ ...currentPage, current: 1 }));
                    }
                    if (extra.action !== 'sort') {
                        return;
                    }
                    const sorterValue = Array.isArray(newSorter) ? newSorter[0] : newSorter;
                    const field = (sorterValue as { field?: unknown })?.field;
                    const order = (sorterValue as { order?: unknown })?.order;
                    if (!isKernelMfuOrderBy(field) || (order !== 'ascend' && order !== 'descend')) {
                        setSorter(DEFAULT_SORTER);
                    } else {
                        setSorter({ field, order });
                    }
                    setPage((currentPage) => ({ ...currentPage, current: 1 }));
                }}
                loading={loading}
                pagination={getPageData(page, setPage)}
                dataSource={dataSource}
                columns={columns}
                scroll={{ x: 'max-content', y: scrollHeight }}
                size="small"
            />
        </TableContainer>
    );
});
