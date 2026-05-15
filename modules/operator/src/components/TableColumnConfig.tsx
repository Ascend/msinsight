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
import React from 'react';
import type { ColumnsType } from 'antd/es/table';
import { useTranslation } from 'react-i18next';
import { fetchColumnFilterProps } from '@insight/lib/resize';
import { themeInstance } from '@insight/lib/theme';

export const enum OperatorGroup {
    OPERATOR = 'Operator',
    OPERATOR_TYPE = 'Operator Type',
    INPUT_SHAPE = 'Input Shape',
    HCCL_OPERATOR = 'Communication Operator',
    HCCL_OPERATOR_TYPE = 'Communication Operator Type',
};

export const useCompareSourceColumn = (): ColumnsType<any> => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    return [
        {
            title: t('Source'),
            dataIndex: 'source',
            ellipsis: true,
        },
    ];
};

const useTextColor = (value: string | number): JSX.Element => {
    const theme = themeInstance.themeObject;
    return <div style={{ color: Number(value) >= 0 ? theme.successColor : theme.dangerColor }}>{value}</div>;
};

const useOpl0Columns = (isCompare: boolean): ColumnsType<any> => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    return [
        {
            title: t('Name'),
            dataIndex: 'name',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('name', 'Name'),
        },
        {
            title: t('Type'),
            dataIndex: 'type',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('type', 'Type'),
        },
        {
            title: t('AcceleratorCore'),
            dataIndex: 'accCore',
            key: 'accCore',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('accCore', 'AcceleratorCore'),
        },
        {
            title: `${t('StartTime')}(ms)`,
            dataIndex: 'startTime',
            sorter: true,
            ellipsis: true,
        },
        {
            title: `${t('Duration')}(μs)`,
            dataIndex: 'duration',
            sorter: true,
            ellipsis: true,
            render: isCompare ? useTextColor : undefined,
        },
        {
            title: `${t('WaitTime')}(μs)`,
            dataIndex: 'waitTime',
            sorter: true,
            ellipsis: true,
        },
    ];
};
const useOpl2Columns = (isCompare: boolean): ColumnsType<any> => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    return [
        ...useOpl0Columns(isCompare),
        {
            title: t('BlockNum'),
            dataIndex: 'blockNum',
            sorter: true,
            ellipsis: true,
        },
        {
            title: t('InputShapes'),
            dataIndex: 'inputShape',
            sorter: true,
            ellipsis: true,
        },
        {
            title: t('InputDataTypes'),
            dataIndex: 'inputType',
            sorter: true,
            ellipsis: true,
        },
        {
            title: t('InputFormats'),
            dataIndex: 'inputFormat',
            sorter: true,
            ellipsis: true,
        },
        {
            title: t('OutputShapes'),
            dataIndex: 'outputShape',
            sorter: true,
            ellipsis: true,
        },
        {
            title: t('OutputDataTypes'),
            dataIndex: 'outputType',
            sorter: true,
            ellipsis: true,
        },
        {
            title: t('OutputFormats'),
            dataIndex: 'outputFormat',
            sorter: true,
            ellipsis: true,
        },
    ];
};
const useOpStaticColumns = (isCompare: boolean): ColumnsType<any> => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    return [
        {
            title: t('Type'),
            dataIndex: 'opType',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('opType', 'Type'),
        },
        {
            title: t('AcceleratorCore'),
            dataIndex: 'accCore',
            key: 'accCore',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('accCore', 'AcceleratorCore'),
        },
        {
            title: t('Count'),
            dataIndex: 'count',
            sorter: true,
            ellipsis: true,
            render: isCompare ? useTextColor : undefined,
        },
        {
            title: `${t('TotalTime')}(μs)`,
            dataIndex: 'totalTime',
            sorter: true,
            ellipsis: true,
            render: isCompare ? useTextColor : undefined,
        },
        {
            title: `${t('AvgTime')}(μs)`,
            dataIndex: 'avgTime',
            sorter: true,
            ellipsis: true,
        },
        {
            title: `${t('MaxTime')}(μs)`,
            dataIndex: 'maxTime',
            sorter: true,
            ellipsis: true,
        },
        {
            title: `${t('MinTime')}(μs)`,
            dataIndex: 'minTime',
            sorter: true,
            ellipsis: true,
        },
    ];
};
const useOpShapeStaticColumns = (isCompare: boolean): ColumnsType<any> => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    return [
        { title: t('Name'), dataIndex: 'opName', sorter: true, ellipsis: true, ...fetchColumnFilterProps('opName', 'Name') },
        { title: t('Shape'), dataIndex: 'inputShape', key: 'Shape', sorter: true, ellipsis: true },
        {
            title: t('AcceleratorCore'),
            dataIndex: 'accCore',
            key: 'accCore',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('accCore', 'AcceleratorCore'),
        },
        {
            title: t('Count'),
            dataIndex: 'count',
            sorter: true,
            ellipsis: true,
            render: isCompare ? useTextColor : undefined,
        },
        {
            title: `${t('TotalTime')}(μs)`,
            dataIndex: 'totalTime',
            sorter: true,
            ellipsis: true,
            render: isCompare ? useTextColor : undefined,
        },
        {
            title: `${t('AvgTime')}(μs)`,
            dataIndex: 'avgTime',
            sorter: true,
            ellipsis: true,
        },
        {
            title: `${t('MaxTime')}(μs)`,
            dataIndex: 'maxTime',
            sorter: true,
            ellipsis: true,
        },
        {
            title: `${t('MinTime')}(μs)`,
            dataIndex: 'minTime',
            sorter: true,
            ellipsis: true,
        },
    ];
};
const useHcclOpColumns = (isCompare: boolean): ColumnsType<any> => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    return [
        {
            title: t('Name'),
            dataIndex: 'name',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('name', 'Name'),
        },
        {
            title: t('Type'),
            dataIndex: 'type',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('type', 'Type'),
        },
        {
            title: `${t('StartTime')}(ms)`,
            dataIndex: 'startTime',
            sorter: true,
            ellipsis: true,
        },
        {
            title: `${t('Duration')}(μs)`,
            dataIndex: 'duration',
            sorter: true,
            ellipsis: true,
            render: isCompare ? useTextColor : undefined,
        },
        {
            title: `${t('WaitTime')}(μs)`,
            dataIndex: 'waitTime',
            sorter: true,
            ellipsis: true,
        },
    ];
};

const useHcclOpTypeColumns = (isCompare: boolean): ColumnsType<any> => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    return [
        {
            title: t('Type'),
            dataIndex: 'opType',
            sorter: true,
            ellipsis: true,
            ...fetchColumnFilterProps('opType', 'Type'),
        },
        {
            title: t('Count'),
            dataIndex: 'count',
            sorter: true,
            ellipsis: true,
            render: isCompare ? useTextColor : undefined,
        },
        {
            title: `${t('TotalTime')}(μs)`,
            dataIndex: 'totalTime',
            sorter: true,
            ellipsis: true,
            render: isCompare ? useTextColor : undefined,
        },
        {
            title: `${t('AvgTime')}(μs)`,
            dataIndex: 'avgTime',
            sorter: true,
            ellipsis: true,
        },
        {
            title: `${t('MaxTime')}(μs)`,
            dataIndex: 'maxTime',
            sorter: true,
            ellipsis: true,
        },
        {
            title: `${t('MinTime')}(μs)`,
            dataIndex: 'minTime',
            sorter: true,
            ellipsis: true,
        },
    ];
};

export const useColMap = (isCompare: boolean): any => {
    const opl0Columns = useOpl0Columns(isCompare);
    const opl2Columns = useOpl2Columns(isCompare);
    const opStaticColumns = useOpStaticColumns(isCompare);
    const opShapeStaticColumns = useOpShapeStaticColumns(isCompare);
    const hcclOpColumns = useHcclOpColumns(isCompare);
    const hcclOpTypeColumns = useHcclOpTypeColumns(isCompare);

    return {
        [OperatorGroup.OPERATOR]: {
            l0: opl0Columns,
            l1: opl2Columns,
            l2: opl2Columns,
        },
        [OperatorGroup.OPERATOR_TYPE]: opStaticColumns,
        [OperatorGroup.INPUT_SHAPE]: opShapeStaticColumns,
        [OperatorGroup.HCCL_OPERATOR]: hcclOpColumns,
        [OperatorGroup.HCCL_OPERATOR_TYPE]: {
            l0: hcclOpTypeColumns,
            l1: hcclOpColumns,
        },
    };
};
