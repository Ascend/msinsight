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
import type { CompareFn, FilterConfirmProps, Key } from 'antd/es/table/interface';
import React from 'react';
import { useTranslation } from 'react-i18next';
import { Button, Input } from '@insight/lib/components';
import { Space } from 'antd/lib/index';
import { SearchOutlined } from '@ant-design/icons';
import type { ColumnType } from 'antd/es/table';
import { limitInput } from '@insight/lib/utils';
import { fetchColumnFilterProps } from '@insight/lib/resize';
import i18n from '@insight/lib/i18n';
import type { TableColumnsType } from 'antd';
import { ColumnFilterIcon } from '@insight/lib/icon';
import type { EventViewParams, BaseSummaryRowItemType, KernelE2ETimeResponse } from '../../api/interface';

interface ColumData {
    title: string;
    dataIndex: string;
    key: string;
    sorter?: boolean | CompareFn<any>;
    filter?: ColumnType<any>;
    showSorterTooltip?: boolean;
}

interface SearchData {
    dataIndex: string;
    setSearchText: (text: string) => void;
    searchText: string;
    setSearchedColumn: (text: string) => void;
    searchedColumn: string;
}

export interface IQueryCondition {
    rankId: string;
    dbPath: string;
    pageSize: number;
    current: number;
    orderBy: string;
    order: string;
    isQueryTotal?: boolean;
    layer?: string;
    searchName?: string;
    startTime?: number;
    endTime?: number;
}

export const getDefaultColumData = (key: string, sorter = true): {
    sorter: boolean;
    showSorterTooltip: boolean;
    ellipsis: boolean;
    key: string;
} => {
    return {
        sorter,
        showSorterTooltip: false,
        ellipsis: true,
        key,
    };
};

export interface VoidFunction {
    (...rest: any[]): void;
}
export function isNull(val: any): boolean {
    return val === undefined || val === null || val === '';
}

export const Label = (props: { name: string; style?: object }): JSX.Element => {
    return <span style={{ margin: '0 10px', ...(props.style ?? {}) }}>{props.name ? `${props.name} :` : ''} </span>;
};

export const pythonApiSummaryColumns: ColumData[] = [
    { title: 'Time(%)', dataIndex: 'time', ...getDefaultColumData('time') },
    { title: 'Total Time(us)', dataIndex: 'totalTime', ...getDefaultColumData('totalTime') },
    { title: 'Num Calls', dataIndex: 'numberCalls', ...getDefaultColumData('numberCalls') },
    { title: 'Avg(us)', dataIndex: 'avg', ...getDefaultColumData('avg') },
    { title: 'Min(us)', dataIndex: 'min', ...getDefaultColumData('min') },
    { title: 'Max(us)', dataIndex: 'max', ...getDefaultColumData('max') },
];

export const traceColumns: ColumData[] = [
    { title: 'Start Time', dataIndex: 'startTimeLabel', ...getDefaultColumData('startTimeLabel') },
    { title: 'Duration(us)', dataIndex: 'duration', ...getDefaultColumData('duration') },
];

export const ftraceTimeSummaryColumns: ColumData[] = [
    { title: 'process', dataIndex: 'process', ...getDefaultColumData('process', false) },
    { title: 'thread', dataIndex: 'thread', ...getDefaultColumData('thread', false) },
    { title: 'runnable', dataIndex: 'runnable', ...getDefaultColumData('runnable', false) },
    { title: 'running', dataIndex: 'running', ...getDefaultColumData('running', false) },
    { title: 'sleeping', dataIndex: 'sleeping', ...getDefaultColumData('sleeping', false) },
];

export const ftraceIrqSummaryColumns: ColumData[] = [
    { title: 'process', dataIndex: 'process', ...getDefaultColumData('process', false) },
    { title: 'thread', dataIndex: 'thread', ...getDefaultColumData('thread', false) },
    { title: 'soft_irq_count', dataIndex: 'soft_irq_count', ...getDefaultColumData('soft_irq_count', false) },
    { title: 'soft_irq_duration', dataIndex: 'soft_irq_duration', ...getDefaultColumData('soft_irq_duration', false) },
    { title: 'hard_irq_count', dataIndex: 'hard_irq_count', ...getDefaultColumData('hard_irq_count', false) },
    { title: 'hard_irq_duration', dataIndex: 'hard_irq_duration', ...getDefaultColumData('hard_irq_duration', false) },
];

export const ftraceSchedSummaryColumns: ColumData[] = [
    { title: 'process', dataIndex: 'process', ...getDefaultColumData('process', false) },
    { title: 'thread', dataIndex: 'thread', ...getDefaultColumData('thread', false) },
    { title: 'context_switch_count', dataIndex: 'context_switch_count', ...getDefaultColumData('context_switch_count', false) },
];

export const useKernelDetails = (): TableColumnsType<any> => {
    const { t } = useTranslation('operator', { keyPrefix: 'tableHead' });
    return [
        { title: t('Name'), dataIndex: 'name', ...getDefaultColumData('name'), ...fetchColumnFilterProps('name', 'Name') },
        { title: t('Type'), dataIndex: 'type', ...getDefaultColumData('type'), ...fetchColumnFilterProps('type', 'Type') },
        {
            title: t('AcceleratorCore'),
            dataIndex: 'acceleratorCore',
            ...getDefaultColumData('acceleratorCore'),
            ...fetchColumnFilterProps('acceleratorCore', 'AcceleratorCore'),
        },
        { title: t('StartTime'), dataIndex: 'startTimeLabel', ...getDefaultColumData('startTimeLabel') },
        { title: `${t('Duration')}(us)`, dataIndex: 'duration', ...getDefaultColumData('duration') },
        { title: `${t('WaitTime')}(us)`, dataIndex: 'waitTime', ...getDefaultColumData('waitTime') },
        { title: t('TaskId'), dataIndex: 'taskId', ...getDefaultColumData('taskId'), ...fetchColumnFilterProps('taskId', 'TaskId') },
        { title: t('BlockNum'), dataIndex: 'blockNum', ...getDefaultColumData('blockNum') },
        { title: t('InputShapes'), dataIndex: 'inputShapes', ...getDefaultColumData('inputShapes'), ...fetchColumnFilterProps('inputShapes', 'InputShapes') },
        {
            title: t('InputDataTypes'),
            dataIndex: 'inputDataTypes',
            ...getDefaultColumData('inputDataTypes'),
            ...fetchColumnFilterProps('inputDataTypes', 'InputDataTypes'),
        },
        {
            title: t('InputFormats'),
            dataIndex: 'inputFormats',
            ...getDefaultColumData('inputFormats'),
            ...fetchColumnFilterProps('inputFormats', 'InputFormats'),
        },
        {
            title: t('OutputShapes'),
            dataIndex: 'outputShapes',
            ...getDefaultColumData('outputShapes'),
            ...fetchColumnFilterProps('outputShapes', 'OutputShapes'),
        },
        {
            title: t('OutputDataTypes'),
            dataIndex: 'outputDataTypes',
            ...getDefaultColumData('outputDataTypes'),
            ...fetchColumnFilterProps('outputDataTypes', 'OutputDataTypes'),
        },
        {
            title: t('OutputFormats'),
            dataIndex: 'outputFormats',
            ...getDefaultColumData('outputFormats'),
            ...fetchColumnFilterProps('outputFormats', 'OutputFormats'),
        },
    ];
};

const commonExpertColums: ColumData[] = [
    { title: 'Start Time', dataIndex: 'startTimeLabel', ...getDefaultColumData('startTimeLabel') },
    { title: 'Duration(ns)', dataIndex: 'duration', ...getDefaultColumData('duration') },
    { title: 'Process Id', dataIndex: 'pid', ...getDefaultColumData('pid') },
    { title: 'Thread Id', dataIndex: 'tid', ...getDefaultColumData('tid') },
];

export const affinityAPIColumns: ColumData[] = [
    { title: 'Name', dataIndex: 'name', ...getDefaultColumData('name') },
    { title: 'Origin API', dataIndex: 'originAPI', ...getDefaultColumData('originAPI') },
    { title: 'Replacement API', dataIndex: 'replaceAPI', ...getDefaultColumData('replaceAPI', false) },
    ...commonExpertColums,
    { title: 'Notes', dataIndex: 'note', ...getDefaultColumData('note', false) },
];

export const affinityOptimizerColumns: ColumData[] = [
    { title: 'Origin Optimizer', dataIndex: 'originOptimizer', ...getDefaultColumData('originOptimizer') },
    { title: 'Replacement Optimizer', dataIndex: 'replaceOptimizer', ...getDefaultColumData('replaceOptimizer', false) },
    ...commonExpertColums,
];

export const aicpuOperatorColumns: ColumData[] = [
    { title: 'Name', dataIndex: 'name', ...getDefaultColumData('name') },
    ...commonExpertColums,
    { title: 'Notes', dataIndex: 'note', ...getDefaultColumData('note', false) },
];

export const aclnnOperatorColumns: ColumData[] = [
    { title: 'Name', dataIndex: 'name', ...getDefaultColumData('name') },
    ...commonExpertColums,
    { title: 'Notes', dataIndex: 'note', ...getDefaultColumData('note', false) },
];

export const fusionOperatorColumns: ColumData[] = [
    { title: 'Name', dataIndex: 'name', ...getDefaultColumData('name') },
    { title: 'Origin Operators', dataIndex: 'originOpList', ...getDefaultColumData('originOpList', false) },
    { title: 'Fused Operator', dataIndex: 'fusedOp', ...getDefaultColumData('fusedOp', false) },
    ...commonExpertColums,
    { title: 'Notes', dataIndex: 'note', ...getDefaultColumData('note', false) },
];

export const operatorDispatchColumns: ColumData[] = [
    { title: 'Name', dataIndex: 'name', ...getDefaultColumData('name') },
    ...commonExpertColums,
    { title: 'Notes', dataIndex: 'note', ...getDefaultColumData('note', false) },
];

export interface SystemViewItem {
    name: string;
    tips?: string;
    description?: string;
}
export const statsSystemViewItems: SystemViewItem[] = [
    { name: 'Overall Metrics', tips: 'OverallMetricsTips' },
    { name: 'Python API Summary', tips: 'PythonAPISummaryTips' },
    { name: 'Python API Trace', tips: 'PythonAPITraceTips' },
    { name: 'CANN API Summary', tips: 'CANNAPISummaryTips' },
    { name: 'CANN API Trace', tips: 'CANNAPITraceTips' },
    { name: 'Ascend HardWare Task Summary', tips: 'AscendHardWareTaskSummaryTips' },
    { name: 'Ascend HardWare Task Trace', tips: 'AscendHardWareTaskTraceTips' },
    { name: 'Communication Summary', tips: 'CommunicationSummaryTips' },
    { name: 'Communication Trace', tips: 'CommunicationTraceTips' },
    { name: 'Overlap Analysis', tips: 'OverlapAnalysisTips' },
    { name: 'Kernel E2E Time', tips: 'KernelE2ETimeTips' },
    { name: 'Kernel Details', tips: 'KernelDetailsTips' },
    { name: 'Operator Details', tips: 'OperatorDetailsTips' },
    { name: 'Memcpy Overall' },
    { name: 'Ftrace Time Consuming' },
    { name: 'Ftrace IRQ' },
    { name: 'Ftrace Sched' },
];

export const ftraceTypes: string[] = ['Ftrace Time Consuming', 'Ftrace IRQ', 'Ftrace Sched'];
export const summaryAndTraceTypes: Array<{type: string; isStats: boolean; isTrace: boolean;columns: ColumData[] }> = [
    { type: 'Python', isStats: true, isTrace: false, columns: pythonApiSummaryColumns },
    { type: 'Python', isStats: false, isTrace: true, columns: traceColumns },
    { type: 'CANN', isStats: true, isTrace: false, columns: pythonApiSummaryColumns },
    { type: 'CANN', isStats: false, isTrace: true, columns: traceColumns },
    { type: 'Ascend Hardware', isStats: true, isTrace: false, columns: pythonApiSummaryColumns },
    { type: 'Ascend Hardware', isStats: false, isTrace: true, columns: traceColumns },
    { type: 'HCCL', isStats: true, isTrace: false, columns: pythonApiSummaryColumns },
    { type: 'HCCL', isStats: false, isTrace: true, columns: traceColumns },
    { type: 'Overlap Analysis', isStats: true, isTrace: false, columns: pythonApiSummaryColumns },
];

export interface IndexedSystemViewItem extends SystemViewItem {
    originIndex: number;
}

export const getVisibleStatsSystemViewItems = (
    items: SystemViewItem[],
    hasFtraceData: boolean,
    hasNonFtraceData: boolean,
): IndexedSystemViewItem[] => {
    return items.map((item, index) => ({ ...item, originIndex: index })).filter((item) => {
        const isFtraceItem = ftraceTypes.includes(item.name);
        if (hasFtraceData && !hasNonFtraceData) {
            return isFtraceItem;
        }
        if (!hasFtraceData && hasNonFtraceData) {
            return !isFtraceItem;
        }
        return true;
    });
};

export const expertSystemViewItems: SystemViewItem[] = [
    { name: 'Expert Analysis', tips: 'ExpertAnalysisTips' },
    { name: 'Affinity API', tips: 'AffinityAPITips' },
    { name: 'Affinity Optimizer', tips: 'AffinityOptimizerTips' },
    { name: 'AICPU Operators', tips: 'AICPUOperatorsTips' },
    { name: 'ACLNN Operators', tips: 'ACLNNOperatorsTips' },
    { name: 'Operators Fusion', tips: 'OperatorsFusionTips' },
    { name: 'Operators Dispatch', tips: 'OperatorsDispatchTips' },
];

export interface PageType {
    current: number;
    pageSize: number;
    total: number;
}
export const getPageData = (page: PageType, setPage: VoidFunction): object => {
    return {
        ...page,
        showSizeChanger: page.total > 10,
        pageSizeOptions: [10, 20, 50, 100],
        showTotal: (total: number) => (<div style={{ marginRight: '10px' }}>{i18n.t('PaginationTotal', { total })}</div>),
        hideOnSinglePage: false,
        onChange: (current: number, pageSize: number): void => { setPage({ ...page, current, pageSize }); },
        showQuickJumper: page.total / (page.pageSize === 0 ? 1 : page.pageSize) > 5,
    };
};

export const querySystemViewTrace = async (param: {
    rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    startTime: number; endTime: number; layer: string; searchName: string;
}): Promise<{ systemViewDetails: BaseSummaryRowItemType[] }> => {
    return window.requestData('unit/systemViewTrace', param, 'timeline');
};

export const querySystemViewDetails = async (param: {
    isQueryTotal: boolean; rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    startTime: number; endTime: number; layer: string; searchName: string;
}): Promise<{ systemViewDetails: BaseSummaryRowItemType[] }> => {
    return window.requestData('unit/systemView', param, 'timeline');
};

export const queryFtraceStat = async (param: {
    rankId: string; dbPath: string; dataType: number; current: number; pageSize: number;
}): Promise<{ headers: string[]; data: Array<Record<string, string>>; pageParam: { current: number; pageSize: number; total: number } }> => {
    return window.requestData('systemView/systemViewFtraceStat', param, 'timeline');
};

export const queryTableDataNameList = async (param: { rankId: string; dbPath: string }): Promise<any> => {
    return window.requestData('tableData/nameList', param, 'timeline', { silent: true });
};

export const queryTableDataDetails = async (param: {
    rankId: string; pageSize: number; currentPage: number; orderBy?: string; order?: string;
    selectKey: number;
}): Promise<any> => {
    return window.requestData('tableData/detail', param, 'timeline');
};

export const queryKernelDetails = async (param: {
    rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    startTime: number; endTime: number; coreType: string; filterCondition: string[];
}): Promise<any> => {
    return window.requestData('unit/kernelDetails', param, 'timeline');
};

export const queryOneKernel = async (param: {
    rankId: string; dbPath: string; name: string; timestamp: number; duration: number;
}): Promise<any> => {
    return window.requestData('unit/one/kernelDetail', param, 'timeline');
};

export const searchAllSlices = async (param: {
    rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    searchContent?: string; isMatchCase?: boolean; isMatchExact?: boolean; metadataList: any;
    nameFilter?: string;
}): Promise<any> => {
    return window.requestData('search/all/slices', param, 'timeline');
};

export const queryAffinityOptimizer = async (param: {
    rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    startTime: number; endTime: number;
}): Promise<{ data: BaseSummaryRowItemType[] }> => {
    return window.requestData('advisor/affinity_optimizer', param);
};

export const queryAffinityAPI = async (param: {
    rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    startTime: number; endTime: number;
}): Promise<{ data: BaseSummaryRowItemType[] }> => {
    return window.requestData('advisor/affinity_api', param);
};

export const queryOperatorFusion = async (param: {
    rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    startTime: number; endTime: number;
}): Promise<{ data: BaseSummaryRowItemType[] }> => {
    return window.requestData('advisor/operator_fusion', param);
};

export const queryAICPUOperators = async (param: {
    rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    startTime: number; endTime: number;
}): Promise<{ data: BaseSummaryRowItemType[] }> => {
    return window.requestData('advisor/aicpu_operator', param);
};

export const queryACLNNOperators = async (param: {
    rankId: string; dbPath: string; pageSize: number; current: number; orderBy: string; order: string;
    startTime: number; endTime: number;
}): Promise<{ data: BaseSummaryRowItemType[] }> => {
    return window.requestData('advisor/aclnn_operator', param);
};

export const eventViewData = async (params: EventViewParams): Promise<any> => {
    return window.requestData('unit/eventView', params, 'timeline');
};

export const queryKernelE2ETime = async (param: {
    rankId: string;
    dbPath: string;
    startTime: number;
    endTime: number;
    current: number;
    pageSize: number;
    pathType: string;
    opName?: string;
    sortField?: string;
    sortOrder?: string;
}): Promise<KernelE2ETimeResponse> => {
    return window.requestData('unit/kernelE2ETime', param, 'timeline');
};

export const Loading = ({ size = 20, style = {} }: { size?: number; style?: object }): JSX.Element => {
    return (<div className={'loading'}
        style={{ width: `${size}px`, height: `${size}px`, ...style }}></div>);
};

const filterDropdownCom = ({ setSelectedKeys, selectedKeys, handleSearch, confirm, dataIndex, clearFilters, handleReset }: {
    setSelectedKeys: (selectedKeys: React.Key[]) => void;
    selectedKeys: React.Key[];
    handleSearch: (selectedKeys: string[], confirm: (param?: FilterConfirmProps) => void, index: string, setSelectedKeys: (selectedKeys: Key[]) => void) => void;
    confirm: (param?: (FilterConfirmProps | undefined)) => void;
    dataIndex: string;
    clearFilters?: () => void;
    handleReset: (confirm: (param?: FilterConfirmProps) => void, index: string, clearFilters?: () => void,) => void;
}): JSX.Element => {
    return (
        <div style={{ padding: 8 }} onKeyDown={(e: React.KeyboardEvent<HTMLDivElement>): void => { e.stopPropagation(); }}>
            <Input
                placeholder={`${i18n.t('buttonText:Search')} ${i18n.t('filterColumnName:Name')}`}
                value={selectedKeys[0] !== null && selectedKeys[0] !== undefined ? selectedKeys[0] : ''}
                onChange={(e: React.ChangeEvent<HTMLInputElement>): void =>
                    setSelectedKeys((e.target.value !== null && e.target.value !== undefined) ? [e.target.value] : [])}
                onPressEnter={(): void => handleSearch(selectedKeys as string[], confirm, dataIndex, setSelectedKeys)}
                style={{ width: 168, marginBottom: 8, display: 'block' }}
            />
            <Space>
                <Button
                    type="primary"
                    onClick={(): void => handleSearch(selectedKeys as string[], confirm, dataIndex, setSelectedKeys)}
                    icon={<SearchOutlined />}
                    size="small"
                    style={{ width: 80 }}
                >
                    {i18n.t('buttonText:Search')}
                </Button>
                <Button
                    onClick={(): void => { if (clearFilters) { handleReset(confirm, dataIndex, clearFilters); } }}
                    size="small"
                    style={{ width: 80 }}
                >
                    {i18n.t('buttonText:Reset')}
                </Button>
            </Space>
        </div>
    );
};

export const getColumnSearchProps = ({ dataIndex, setSearchText, searchText, setSearchedColumn, searchedColumn }: SearchData): ColumnType<any> => {
    const handleSearch = (
        selectedKeys: string[],
        confirm: (param?: FilterConfirmProps) => void,
        index: string,
        setSelectedKeys: (selectedKeys: Key[]) => void,
    ): void => {
        const trimmed = selectedKeys[0]?.trim() ?? '';

        setSelectedKeys([trimmed]);
        confirm();
        setSearchText(trimmed);
        setSearchedColumn(index);
    };

    const handleReset = (confirm: (param?: FilterConfirmProps) => void, index: string, clearFilters?: () => void): void => {
        setSearchText('');
        clearFilters?.();
        setSearchedColumn(index);
        confirm();
    };
    return ({
        filterDropdown: ({ setSelectedKeys, selectedKeys, confirm, clearFilters }) =>
            filterDropdownCom({ setSelectedKeys, selectedKeys, handleSearch, confirm, dataIndex, clearFilters, handleReset }),
        filterIcon: (filtered: boolean) => (
            <ColumnFilterIcon/>
        ),
        onFilter: (value, record) =>
            record[dataIndex]
                .toString()
                .toLowerCase()
                .includes((value as string).toLowerCase()),
        render: (text: string) => (text),
        onFilterDropdownOpenChange: (open: boolean): void => {
            if (open) {
                limitInput();
            }
        },
    });
};
