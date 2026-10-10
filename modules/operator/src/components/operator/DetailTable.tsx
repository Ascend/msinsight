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
import { ResizeTable, type ResizeTableRef } from '@insight/lib/resize';
import React, { useState, useEffect, useRef, useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import type { TFunction } from 'i18next';
import { Button, Tooltip, message, CollapsiblePanel } from '@insight/lib/components';
import { DownOutlined } from '@ant-design/icons';
import { getPageConfigWithPageData } from '../Common';
import { type ConditionType, type FilterType } from './Filter';
import { queryOperators, queryOperatorsInStatic, queryOperatorStatic, exportOperatorDetail } from '../RequestUtils';
import { runInAction } from 'mobx';
import type { Session } from '../../entity/session';
import { OperatorGroup, useColMap, useCompareSourceColumn } from '../TableColumnConfig';
import { HelpIcon } from '@insight/lib/icon';
import connector from '../../connection/index';
import UpdateTableAsync from '../../utils/UpdateTableAsync';

let GdbPath = '';
const updateTableAsyne = new UpdateTableAsync();
connector.addListener('updateAllTable', (e: any) => {
    if (GdbPath === e?.data?.body?.data?.dbId) {
        updateTableAsyne.update();
    }
});

interface FullConditionType {
    isCompare: boolean;
    rankId: string;
    dbPath: string;
    group: string;
    topK: number;
    current: number;
    pageSize: number;
    field: string;
    order: string;
    type: string[];
    opType: string[];
    name: string[];
    opName: string[];
    accCore: string[];
};

interface CompInfo {
    level: string;
    detailData: any[];
    pmuHeaders: any[];
}

const OperatorTable = ({ condition, filterType, opType, accCore, opName, inputShape, compInfo, session }:
{
    condition: ConditionType; filterType: FilterType; opType?: string; accCore?: string; opName?: string;
    inputShape?: string; compInfo?: CompInfo; session: Session;
}): JSX.Element => {
    return <BaseTable
        condition={condition}
        filterType={filterType}
        opType={opType}
        accCore={accCore}
        opName={opName}
        inputShape={inputShape}
        compInfo={compInfo}
        session={session}
    />;
};

const OperatorTypeTable = ({ condition, filterType, session }: { condition: ConditionType; filterType: FilterType; session: Session }): JSX.Element => {
    return <BaseTable condition={condition} filterType={filterType} session={session} />;
};

const defaultPage = { current: 1, pageSize: 10, total: 0 };
const defaultSorter = { field: '', order: '' };
const defaultFilters = { type: [], opType: [], name: [], opName: [], accCore: [] };

// 字符串替换映射
const replaceMap = new Map([
    ['_us_', '(us)'],
    ['_GB_s_', '(GB/s)'],
    ['_PCT_', '(%)'],
    ['_KB_', '(KB)'],
]);

function modifyTitle(item: string): string {
    let modifiedItem = item;
    replaceMap.forEach((value, key) => {
        if (modifiedItem.includes(key)) {
            modifiedItem = modifiedItem.replace(new RegExp(key, 'g'), value);
        }
    });
    return modifiedItem;
}

const getCols = ({ group, columnLevel, btnCol, colMap, condition, isExpend, pmuHeaders }:
{ group: string; columnLevel: string; btnCol: any; colMap: any; condition: ConditionType; isExpend: boolean; pmuHeaders: any[] }): any[] => {
    const isCompare = condition.isCompare as boolean;
    let result = [];
    switch (group) {
        case OperatorGroup.OPERATOR: {
            // 对于operator算子，先获取基本列，再获取pmu列
            const columns = colMap[group][columnLevel] ?? colMap[group].l2;
            // pmu数据的表头，后端返回时确定,如果为空或者未定义就是没有，不做处理直接返回
            if (pmuHeaders !== null && pmuHeaders !== undefined && Array.isArray(pmuHeaders)) {
                pmuHeaders.forEach((item: any) => {
                    columns.push({
                        title: modifyTitle(item),
                        dataIndex: item,
                        sorter: false,
                        ellipsis: true,
                    });
                });
            }

            if (isCompare && !isExpend) {
                // 这里是比对的表头信息，多加了一个see more 列用于查看详细信息
                result = [...columns, btnCol];
                break;
            }
            // 这个是比对的详细的表格数据
            result = columns;
            break;
        }
        case OperatorGroup.HCCL_OPERATOR:
            if (isCompare && !isExpend) {
                result = [...colMap[group], btnCol];
                break;
            }
            result = colMap[group];
            break;
        case OperatorGroup.HCCL_OPERATOR_TYPE:
            result = getHcclOperatorTypeCols({ group, columnLevel, btnCol, colMap, isCompare, isExpend });
            break;
        default:
            if (isCompare && isExpend) {
                result = [...colMap[group] ?? []];
                break;
            }
            result = [...colMap[group] ?? [], btnCol];
            break;
    }
    return result;
};

const fetchData = async (fullCondition: FullConditionType, condition: ConditionType,
    opDetail: { opType?: string; opName?: string; accCore?: string; inputShape?: string },
    pageStatus: { isCompare: boolean; compInfo?: CompInfo }): Promise<{ res: any; isExpend: boolean }> => {
    const filterTypes = setFilterTypes(fullCondition);
    const { opType, opName, accCore, inputShape } = opDetail;
    const { isCompare, compInfo } = pageStatus;
    let isExpend = false;
    let res;

    if (opType !== undefined || opName !== undefined || accCore !== undefined) {
        isExpend = true;
        if (isCompare) {
            res = { data: compInfo?.detailData ?? [], total: 2, level: compInfo?.level, pmuHeaders: compInfo?.pmuHeaders };
        } else {
            res = await queryOperatorDetailData({ fullCondition, filterTypes, opType, opName, accCore, inputShape });
        }
    } else {
        res = await queryOperatorData({ condition, fullCondition, filterTypes });
    }

    return { res, isExpend };
};

const getHcclOperatorTypeCols = ({ group, columnLevel, btnCol, colMap, isCompare, isExpend }:
{ group: string; columnLevel: string; btnCol: any; colMap: any; isCompare: boolean; isExpend: boolean }): any[] => {
    if (columnLevel === undefined) {
        if (isCompare && isExpend) {
            return [...colMap[group].l0 ?? []];
        }
        return [...colMap[group].l0 ?? [], btnCol];
    } else {
        if (isCompare && !isExpend) {
            return [...colMap[group][columnLevel], btnCol];
        }
        return colMap[group][columnLevel];
    }
};

const setFilterTypes = (fullCondition: FullConditionType): string[] => {
    const filterColumn = ['type', 'opType', 'name', 'opName', 'accCore'];
    const filterTypes: string[] = [];
    Object.keys(fullCondition).forEach(key => {
        const filterValue = fullCondition[key as keyof FullConditionType];
        if (filterColumn.includes(key) && filterValue != null) {
            if (Array.isArray((filterValue)) && filterValue.length > 0) {
                filterTypes.push(JSON.stringify({ columnName: key, value: filterValue[0] }));
            }
        }
    });
    return filterTypes;
};

const queryOperatorData = async ({ condition, fullCondition, filterTypes }:
{ condition: ConditionType; fullCondition: FullConditionType; filterTypes: string[] }): Promise<any> => {
    let res;
    if (condition.group === OperatorGroup.OPERATOR || condition.group === OperatorGroup.HCCL_OPERATOR) {
        const param = { ...fullCondition, orderBy: fullCondition.field, filters: filterTypes, isCompare: condition.isCompare };
        res = await queryOperators(param);
    } else {
        const param = { ...fullCondition, orderBy: fullCondition.field, filters: filterTypes, isCompare: condition.isCompare };
        res = await queryOperatorStatic(param);
    }
    return res;
};

const queryOperatorDetailData = async ({ fullCondition, filterTypes, opType, opName, accCore, inputShape }:
{ fullCondition: FullConditionType; filterTypes: string[]; opType?: string; opName?: string; accCore?: string; inputShape?: string }): Promise<any> => {
    const param = { ...fullCondition, orderBy: fullCondition.field, filters: filterTypes, opType: opType ?? '', opName, shape: inputShape ?? '', accCore: accCore ?? '' };
    return await queryOperatorsInStatic(param);
};

const handleOrginData = (condition: FullConditionType, data: any[]): any[] => {
    const realData: any[] = [];
    data.forEach((item: any, index: number) => {
        const record = item.compare ?? item;
        const { opType, opName, accCore, inputShape } = record;
        realData.push({
            ...record,
            rowKey: record.rowKey ?? `${JSON.stringify({ ...condition, opType, opName, accCore, inputShape })}${index}`,
        });
    });
    return realData;
};

const handleDiffData = (condition: FullConditionType, data: any[], t: TFunction): any => {
    const realData: any[] = [];
    data.forEach((item: any, index: number) => {
        if (item.diff !== null && item.diff !== undefined) {
            item.diff.rowKey = `${JSON.stringify(condition)}${index}`;
            item.diff.source = t('operator:Difference');
            item.diff.compInfo = [item.baseline, item.compare];
            realData.push(item.diff);
        };
    });
    return realData;
};

const handleCompareData = (data: any, t: TFunction): any[] => {
    if (data.length === 0) {
        return data;
    }
    return [
        { ...data[0], rowKey: 'baseline', source: t('operator:Baseline') },
        { ...data[1], rowKey: 'comparison', source: t('operator:Comparison') },
    ];
};

// eslint-disable-next-line max-lines-per-function
const BaseTable = ({ condition, filterType, opType, accCore, opName, inputShape, compInfo, session }:
{
    condition: ConditionType; filterType: FilterType; opType?: string; accCore?: string;
    opName?: string; inputShape?: string; compInfo?: CompInfo; session: Session;
}): JSX.Element => {
    // 从condition对象中获取isCompare属性
    const isCompare = condition.isCompare as boolean;
    // 从condition对象中获取数据库路径
    GdbPath = condition.dbPath;
    const { t } = useTranslation();
    const tableRef = useRef<ResizeTableRef>(null);
    const [cols, setCols] = useState<any[]>(useColMap(isCompare)[OperatorGroup.OPERATOR].l0);
    const [page, setPage] = useState(defaultPage);
    const [sorter, setSorter] = useState(defaultSorter);
    const [filters, setFilters] = useState(defaultFilters);
    const [tableData, setTableData] = useState<any[]>([]);
    const rowKey = 'rowKey';
    const [expandedRowKeys, setExpandedKeys] = useState<string[]>([]);
    const [loading, setLoading] = useState(false);
    const [fullCondition, setFullCondition] = useState<FullConditionType>({
        isCompare: false,
        current: 1,
        pageSize: 10,
        field: '',
        order: '',
        group: '',
        rankId: '',
        dbPath: '',
        topK: 0,
        type: [],
        opType: [],
        name: [],
        opName: [],
        accCore: [],
    });
    // 使用useState钩子初始化compareColumnLevel状态，初始值为undefined
    const [compareColumnLevel, setCompareColumnLevel] = useState<string>();
    // 使用useState钩子初始化comparePmuColumns状态，初始值为空数组
    const [comparePmuColumns, setComparePmuColumns] = useState<string[]>([]);
    // 表格单独添加列按钮
    const btnCol = {
        title: t('operator:Operation'),
        width: 115,
        key: 'action',
        render: (_: any, record: any) => (<Button type="link"
            onClick={(): void => {
                setExpandedKeys((pre: any) => {
                    const list = [...pre];
                    const keyIndex = list.indexOf(record[rowKey]);
                    if (keyIndex === -1) {
                        list.push(record[rowKey]);
                    } else {
                        list.splice(keyIndex, 1);
                    }
                    return list;
                });
            }}>{t('operator:tableHead.Details', { ns: 'buttonText' })}<DownOutlined /></Button>),
    };
    const colMap = useColMap(isCompare && compInfo === undefined);
    const compareSourceCol = useCompareSourceColumn();

    const tableDataProcess = (r: any): any => {
        const { res, isExpend } = r;
        if (res === null || res === undefined) {
            return;
        }
        const { pmuHeaders, data, level } = res;
        let realData = [];
        if (isCompare) {
            if (isExpend) {
                realData = handleCompareData(data, t);
            } else {
                realData = handleDiffData(fullCondition, data, t);
                setCompareColumnLevel(level);
                // 存到缓存里面
                setComparePmuColumns(pmuHeaders);
            }
        } else {
            realData = handleOrginData(fullCondition, data);
        }
        return realData;
    };
    const updateData = async (): Promise<void> => {
        const result = await fetchData(
            fullCondition,
            condition,
            { opType, opName, accCore, inputShape },
            { isCompare, compInfo },
        );
        const { res, isExpend } = result;
        const { pmuHeaders, total, level } = res;
        const realData = tableDataProcess(result);
        const isLoading = updateTableAsyne.replaceEmptyValuesWithLoa(realData);
        if (isLoading) {
            updateTableAsyne.addUpdateList(
                JSON.stringify({
                    fullCondition,
                    condition,
                    opDetail: { opType, opName, accCore, inputShape },
                    pageStatus: { isCompare, compInfo },
                }),
                fetchData,
                (r: any) => tableDataProcess(r),
                setTableData,
            );
        }
        setTableData(realData);
        setPage({ ...page, total });
        let group = opType !== undefined && !isCompare ? OperatorGroup.OPERATOR : condition.group;
        let columnLevel = level;
        if (condition.group === OperatorGroup.HCCL_OPERATOR_TYPE && isExpend && !isCompare) {
            group = OperatorGroup.HCCL_OPERATOR_TYPE;
            columnLevel = 'l1';
        }
        let columns = getCols({ group, columnLevel, btnCol, colMap, condition, isExpend, pmuHeaders });
        if (isCompare) {
            columns.splice(1, 0, compareSourceCol[0]);
        }
        columns = updateTableAsyne.addRenderMethod(columns);
        setCols(columns);
        runInAction(() => {
            session.total = total;
        });
    };

    const handleReset = (): void => {
        GdbPath = condition.dbPath;
        setSorter(defaultSorter);
        setPage(defaultPage);
        setFilters(defaultFilters);
        updateFullCondition({ ...defaultSorter, ...defaultPage, ...defaultFilters, ...condition });
    };

    const updateTable = async (): Promise<void> => {
        setLoading(true);
        try {
            await updateData();
        } finally {
            setExpandedKeys([]);
            setLoading(false);
        }
    };

    const updateFullCondition = (obj: FullConditionType): void => {
        setTimeout(() => {
            const newFullCondition = { ...fullCondition };
            const keys = ['group', 'rankId', 'dbPath', 'topK', 'current', 'pageSize',
                'field', 'order', 'type', 'opType', 'name', 'opName', 'accCore', 'isCompare'];
            Object.keys(obj).forEach(key => {
                if (keys.includes(key)) {
                    Object.assign(newFullCondition, { [key]: obj[key as keyof FullConditionType] });
                }
            });
            setFullCondition(newFullCondition);
        });
    };

    useEffect(() => {
        GdbPath = condition.dbPath;
        // 首次渲染不更新表格
        if (fullCondition.rankId === '') {
            setTableData([]);
            setPage(defaultPage);
            runInAction(() => {
                session.total = 0;
            });
            return;
        }
        updateTable();
    }, [JSON.stringify(fullCondition), t]);

    // 触发条件是同一工程切换页签时
    useEffect(() => {
        handleReset();
    }, [condition.group]);

    // 触发条件是切换不同工程的时候，包括非比对变成比对
    useEffect(() => {
        if (page.current * page.pageSize > page.total) {
            page.current = parseInt((page.total / page.pageSize).toString()) + 1;
        }
        updateFullCondition({ ...sorter, ...page, ...filters, ...condition });
    }, [page.current, page.pageSize, page.total, sorter.field, sorter.order,
        filters.type, filters.opType, filters.name, filters.opName, filters.accCore,
        condition.rankId, condition.topK, condition.isCompare]);

    // 切换或删除项目清空表格筛选状态
    useEffect((): void => {
        handleReset();
        tableRef.current?.clearAllFilters();
        tableRef.current?.clearAllSorts();
    }, [session.projectChangedTrigger]);

    return <ResizeTable
        ref={tableRef}
        size="small"
        minThWidth={50}
        loading={loading}
        columns={cols}
        dataSource={tableData}
        scroll={{
            x: 100 * cols.length,
        }}
        pagination={isCompare && compInfo !== undefined ? false : getPageConfigWithPageData(page, setPage)}
        onChange={(pagination: any, newFilters: any, newSorter: any, extra: any): void => {
            if (extra.action === 'sort') {
                setSorter(newSorter.order === undefined ? { field: '', order: '' } : newSorter);
            }
            if (extra.action === 'filter') {
                setFilters(newFilters === undefined ? {} : newFilters);
            }
        }
        }
        rowKey={rowKey}
        expandable={isCompare || (condition.group !== OperatorGroup.OPERATOR && condition.group !== OperatorGroup.HCCL_OPERATOR)
            ? {
                expandedRowRender: (record: any) => <OperatorTable
                    condition={condition}
                    filterType={filterType}
                    opName={record.opName}
                    opType={record.opType}
                    inputShape={record.inputShape}
                    accCore={record.accCore}
                    compInfo={
                        isCompare
                            ? {
                                level: compareColumnLevel,
                                pmuHeaders: comparePmuColumns,
                                detailData: record.compInfo ?? [],
                            } as CompInfo
                            : undefined
                    }
                    session={session}
                />,
                expandedRowKeys,
                expandIcon: () => (<></>),
            }
            : undefined}
    />;
};

export const useHit = (condition: ConditionType): React.ReactElement => {
    const { t } = useTranslation('operator');
    if (condition.isCompare || condition.group !== OperatorGroup.OPERATOR) {
        return (<></>);
    }
    const hit = t('Operator/ComputeOperatorDetailDescribe',
        { returnObjects: true }) as string[];
    return (<Tooltip
        overlayClassName={'width-auto'}
        title={
            (
                <div style={{ padding: '1rem' }}>
                    {hit?.map((item: string, index: number) => <div key={index}>{item}</div>)}
                </div>
            )
        }>
        <HelpIcon style={{ cursor: 'pointer', marginLeft: '3px' }} height={20} width={20} />
    </Tooltip>);
};

const DetailTable = ({ condition, filterType, session }: { condition: ConditionType; filterType: FilterType; session: Session }): JSX.Element => {
    const { t } = useTranslation('operator');
    let table;
    switch (condition.group) {
        case OperatorGroup.OPERATOR:
        case OperatorGroup.HCCL_OPERATOR:
            table = <OperatorTable condition={condition} filterType={filterType} session={session} />;
            break;
        case OperatorGroup.OPERATOR_TYPE:
        case OperatorGroup.HCCL_OPERATOR_TYPE:
            table = <OperatorTypeTable condition={condition} filterType={filterType} session={session} />;
            break;
        default:
            table = <BaseTable condition={condition} filterType={filterType} session={session} />;
            break;
    }
    return <CollapsiblePanel
        title={<div className={'flex items-center'}>
            {t('sessionTitle.OperatorDetails')}{useHit(condition)}
        </div>}
        suffix={<ExportBtn condition={condition} />}
        secondary
    >
        {table}
    </CollapsiblePanel>;
};

const ExportBtn = ({ condition }: { condition: ConditionType }): JSX.Element => {
    const { t } = useTranslation('operator');
    const [loading, setLoading] = useState<boolean>(false);
    const disableExport = useMemo(() => {
        return (condition.rankId.length < 1) || (condition.dbPath.length < 1);
    }, [condition.rankId, condition.dbPath]);

    const exportOp = async (): Promise<void> => {
        setLoading(true);
        try {
            const res = await exportOperatorDetail({
                rankId: condition.rankId,
                dbPath: condition.dbPath,
                group: condition.group,
                topK: condition.topK,
                isCompare: condition.isCompare,
            });
            if (res.filePath.length === 0) {
                message.error(t('exportFail'));
            } else {
                const exceedingFileLimitTip = res.exceedingFileLimit ? `${t('exceedingFileLimit')}` : '';
                const filePathTip = t('exportFilePath', { filePath: res.filePath });
                message.success(`${t('exportSuccess')}${exceedingFileLimitTip}${filePathTip}`);
            }
        } finally {
            setLoading(false);
        }
    };

    return <Button
        onClick={exportOp}
        type="primary"
        size="middle"
        style={{ marginLeft: 'auto' }}
        loading={loading}
        disabled={disableExport}
    >{t('export')}</Button>;
};

export default DetailTable;
