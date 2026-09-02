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
import { observer } from 'mobx-react';
import { observable, observe } from 'mobx';
import React, { useEffect, useRef, useState } from 'react';
import type { RadioChangeEvent } from 'antd';
import { useTranslation } from 'react-i18next';
import { Select, Radio, Form } from '@insight/lib/components';
import { getUsableVal, delayExecute } from '@insight/lib/utils';
import { Label, toSortedStage, getAllNumberFromString } from '../Common';
import type { CompareData, optionDataType, optionMapDataType, VoidFunction } from '../../utils/interface';
import { queryIterations, queryMatrixOperators, queryOperators, queryStages } from '../../utils/RequestUtils';
import type { Session } from '../../entity/session';
import { ClusterSelect } from '../ClusterSelect';

export interface ConditionDataType {
    iterationId: string;
    baselineIterationId: string;
    stage: string;
    operatorName: string;
    type: AnalysisType;
    targetOperatorName?: string;
    pgName: string;
    groupIdHash: string;
    baselineGroupIdHash: string;
}
export const totalOperator = 'Total Op Info';
export enum AnalysisType { COMMUNICATION_DURATION_ANALYSIS = 'CommunicationDurationAnalysis', COMMUNICATION_MATRIX = 'CommunicationMatrix' };

export const defaultCondition = {
    iterationId: '',
    baselineIterationId: '',
    stage: '',
    operatorName: '',
    type: AnalysisType.COMMUNICATION_MATRIX,
    pgName: '',
    groupIdHash: '',
    baselineGroupIdHash: '',
};
const defaultOptionMap = {
    clusterOptions: [],
    iterationOptions: [],
    baselineIterationOptions: [],
    operatorOptions: [],
    stageOptions: [],
};

const observeCondition = observable<{ value: Partial<ConditionDataType> }>({ value: defaultCondition });
let pendingConditionUpdate: Partial<ConditionDataType> | undefined;

export function updateData(filterParams: Partial<ConditionDataType>): void {
    pendingConditionUpdate = filterParams;
    observeCondition.value = filterParams;
}

function getDefaultStageOptValue(inputArray: Array<{value: string | number}>): string | number {
    if (inputArray.length === 0) {
        return '';
    }
    // 寻找第一个通信域非单个的情况
    const result = inputArray.find(item =>
        getAllNumberFromString(item.value as string).length > 1 || !isNumberPairsFormat(item.value as string));
    if (!result) {
        return inputArray[0].value;
    }
    return result.value;
}

function getPgNameByValue(stageOptions: optionDataType[], value: string): string {
    const stageData = stageOptions.find(item => item.value === value);
    if (stageData === undefined) {
        return '';
    }
    return (stageData as unknown as {strategy: string}).strategy as string;
}

function getStageByValue(stageOptions: optionDataType[], value: string): string {
    const stageData = stageOptions.find(item => item.value === value);
    if (stageData === undefined) {
        return '';
    }
    return (stageData as unknown as {stage: string}).stage as string;
}

function generateStageKeyValue(groupIdHash: string, baselineGroupIdHash: string): string {
    return `${groupIdHash}:${baselineGroupIdHash}`;
}

function getStageKeyFromCondition(condition: ConditionDataType): string {
    if (condition.groupIdHash === '' && condition.baselineGroupIdHash === '') {
        return '';
    }
    return generateStageKeyValue(condition.groupIdHash, condition.baselineGroupIdHash);
}

function getStageValueByObserveCondition(condition: ConditionDataType, stageOptions: optionDataType[]): string {
    let res: string = '';
    for (let i = 0; i < stageOptions.length; i++) {
        const temp = (stageOptions[i] as unknown as {groupIdHash: {compare: string; baseline: string}; stage: string; strategy: string});
        // 选取stageOptions列表的第一个作为默认值，然后根据stage和pgName进行匹配 看是否有对应的
        if (i === 0) {
            res = generateStageKeyValue(temp.groupIdHash.compare, temp.groupIdHash.baseline);
            continue;
        }
        // pgName存在的时候，匹配pgName
        if (condition.pgName !== '' && temp?.strategy === condition.pgName && temp?.stage === condition.stage) {
            res = generateStageKeyValue(temp.groupIdHash.compare, temp.groupIdHash.baseline);
            break;
        }
        // 兼容老数据处理，pgName不存在的时候 根据groupId的hash值进行匹配
        if (condition.pgName === '' && condition.groupIdHash === temp.groupIdHash.compare) {
            res = generateStageKeyValue(temp.groupIdHash.compare, temp.groupIdHash.baseline);
            break;
        }
    }
    return res;
}

const getOptionsAndValue = async (session: Session, initObj: ConditionDataType, initOptionMap: optionMapDataType, key?: keyof ConditionDataType, val?: any):
Promise<{condition: ConditionDataType;optionMap: optionMapDataType}> => {
    if (key !== undefined) {
        const condition = { ...initObj, [key]: val };
        const optionMap = initOptionMap;
        if (condition.stage === '') {
            // stage === '' 时，页面不显示单选框，此时应该恢复 type 的默认值
            condition.type = defaultCondition.type;
        }
        if (['iterationId', 'baselineIterationId'].includes(key as string)) {
            const stageOptions: optionDataType[] = await getStageOptions(condition, session);
            // 切换迭代id后，group信息需要更新，这里会获取第一个选值信息
            const value = getUsableVal(getStageKeyFromCondition(initObj), stageOptions, getStageKeyFromCondition(defaultCondition), getDefaultStageOptValue);
            // 切换变量中的信息
            optionMap.stageOptions = stageOptions;
            condition.pgName = getPgNameByValue(stageOptions, value.toString());
            condition.stage = getStageByValue(stageOptions, value.toString());
        }
        if (['iterationId', 'stage', 'type'].includes(key as string)) {
            if ((key as string) === 'stage') {
                // stage切换参数更新
                condition.pgName = getPgNameByValue(optionMap.stageOptions, val);
                condition.stage = getStageByValue(optionMap.stageOptions, val);
                const groupIdHashList = val.split(':');
                condition.groupIdHash = groupIdHashList.length >= 1 ? groupIdHashList[0] : '';
                condition.baselineGroupIdHash = groupIdHashList.length >= 2 ? groupIdHashList[1] : '';
            }
            const operatorResult = await getOperatorOptionsWithFallback(session,
                { iterationId: condition.iterationId, stage: condition.stage, type: condition.type, pgName: condition.pgName, groupIdHash: condition.groupIdHash });
            condition.type = operatorResult.type;
            const operatorOptions = operatorResult.operatorOptions;
            optionMap.operatorOptions = operatorOptions;
            condition.operatorName = getUsableVal(initObj.operatorName, operatorOptions, totalOperator).toString();
        }
        return { optionMap, condition };
    }

    // iterationId（step）
    const { compare: iterationOptions, baseline: baselineIterationOptions } = await getiterationOptions(session.isCompare);
    const iterationId = getUsableVal(initObj.iterationId, iterationOptions, defaultCondition.iterationId).toString();

    // 基线step
    const baselineIterationId = getUsableVal(initObj.baselineIterationId, baselineIterationOptions, defaultCondition.baselineIterationId) as string;

    // stage
    const stageOptions: optionDataType[] = await getStageOptions({ iterationId, baselineIterationId }, session);
    const value = getStageValueByObserveCondition(initObj, stageOptions);
    const pgName = getPgNameByValue(stageOptions, value.toString());
    const stage = getStageByValue(stageOptions, value.toString());
    const groupIdHashList = (value as string).split(':');
    const groupIdHash = groupIdHashList.length >= 1 ? groupIdHashList[0] : '';
    const baselineGroupIdHash = groupIdHashList.length >= 2 ? groupIdHashList[1] : '';

    // type
    // stage === '' 时，页面不显示单选框，此时应该恢复 type 的默认值
    let type = value !== '' ? initObj.type ?? defaultCondition.type : defaultCondition.type;

    // Operator Name
    const operatorResult = await getOperatorOptionsWithFallback(session, { iterationId, stage, type, pgName, groupIdHash });
    type = operatorResult.type;
    const operatorOptions = operatorResult.operatorOptions;
    const operatorName = getUsableVal(initObj.operatorName === '' ? totalOperator : initObj.operatorName, operatorOptions, totalOperator).toString();
    return {
        optionMap: { iterationOptions, baselineIterationOptions, stageOptions, operatorOptions },
        condition: { iterationId, stage, pgName, type, operatorName, baselineIterationId, groupIdHash, baselineGroupIdHash },
    };
};

// step选项
const getiterationOptions = async(isCompare: boolean): Promise<CompareData<optionDataType[]>> => {
    const res: {iterationOrRankId: {compare: string[];baseline: string[]} } = await queryIterations({ isCompare });
    const { compare = [], baseline = [] } = res?.iterationOrRankId ?? {};
    const compareOptions: optionDataType[] = compare.map(item => ({ value: item, label: item }));
    const baselineOptions = baseline.map(item => ({ value: item, label: item }));
    return { compare: compareOptions, baseline: baselineOptions, diff: [] };
};

/**
 * 校验字符串是否为(1, 2, 3.....)格式
 * @param str
 */
function isNumberPairsFormat(str: string): boolean {
    const pattern: RegExp = /^\(\s*\d+(?:\s*,\s*\d+)*(?:\s*,)?\s*\)$/;
    return pattern.test(str);
}

// 对比stage数据，先对长度进行排序（长的排前面），再对内容排序（首数字小的排前面）
function compareStageInfo(stageA: string, stageB: string): number {
    const aRankList = getAllNumberFromString(stageA);
    const bRankList = getAllNumberFromString(stageB);
    if (aRankList.length !== bRankList.length) {
        return aRankList.length <= bRankList.length ? 1 : -1;
    }

    for (let i = 0; i < aRankList.length; ++i) {
        if (aRankList[i] < bRankList[i]) {
            return -1;
        }
        if (aRankList[i] > bRankList[i]) {
            return 1;
        }
    }
    return 0;
}

const getStageOptions = async (condition: {iterationId: string;baselineIterationId: string}, session: Session): Promise<optionDataType[]> => {
    const res: {data: [{group: string; parallelStrategy: string; groupIdHash: {compare: string; baseline: string}}] } = await queryStages({ ...condition, isCompare: session.isCompare });
    const list = res?.data ?? [];
    const isSingleItem = list.length === 1;
    let unknownCounter = 1; // 初始化计数器

    const options: optionDataType[] = list
        .map(item => {
            // 如果stage是由数字组成，则对数据进行重排序，否则不做任何处理（p2p的情况）
            const stageAfterSort = isNumberPairsFormat(item.group) ? toSortedStage(item.group) : item.group;
            // 并行策略 如果该数据后端传回来已有该数据，则直接使用，如果没有则显示group id的hash值
            const strategy = item.parallelStrategy;
            // value使用group id的hash值，考虑对比场景，使用两个hash值拼接而成，
            const value = generateStageKeyValue(item.groupIdHash.compare, item.groupIdHash.baseline);
            // label的显示格式为 并行策略：通信域，如果并行策略不存在，则在通信域后面加上groupId的hash值信息
            // 若相同RankSet通信域不唯一，则不展示哈希值，以unknown代替
            const hash = isSingleItem ? value : `unknown_${unknownCounter++}`;
            const label = strategy.length === 0 ? `${stageAfterSort}:${hash}` : `${strategy}:${stageAfterSort}`;
            return { value, strategy, label, stageAfterSort, groupIdHash: item.groupIdHash, stage: item.group };
        })
        .sort((a, b) => {
            // 存在并行策略的排在前面
            if (a.strategy && !b.strategy) {
                return -1;
            }
            if (!a.strategy && b.strategy) {
                return 1;
            }
            // 如果都存在并行策略，且策略相同，则按通信域中rank数量进行排序（从大到小），数量相同，则根据数字对位比较排序（小数字排前）
            if (a.strategy && b.strategy && a.strategy === b.strategy) {
                return compareStageInfo(a.stageAfterSort, b.stageAfterSort);
            }
            // 如果都存在并行策略，但策略不同，则按策略的字母排序
            if (a.strategy && b.strategy && a.strategy !== b.strategy) {
                return a.strategy > b.strategy ? 1 : -1;
            }
            // 如果都没有并行策略，则按通信域中rank数量进行排序（从大到小），数量相同，则根据数字对位比较排序（小数字排前）
            if (!a.strategy && !b.strategy) {
                return compareStageInfo(a.stageAfterSort, b.stageAfterSort);
            }
            // 其他情况保持原有顺序
            return 0;
        });
    return options;
};
const getOperatorOptions = async ({ iterationId, stage, type, pgName, groupIdHash }: {iterationId: string;
    stage: string;type: string; pgName: string; groupIdHash: string;}):
Promise<optionDataType[]> => {
    if (stage === '') {
        return [];
    }
    const res: {operatorName: string[] } = (type === AnalysisType.COMMUNICATION_DURATION_ANALYSIS
        ? await queryOperators({ iterationId, stage, pgName, groupIdHash })
        : await queryMatrixOperators({ iterationId, stage, pgName, groupIdHash }));
    const list = res?.operatorName ?? [];
    const options: optionDataType[] = list.map(item => ({ value: item, label: item }));
    return options;
};

const getOperatorOptionsWithFallback = async (session: Session, params: {iterationId: string;
    stage: string;type: AnalysisType; pgName: string; groupIdHash: string;}):
Promise<{type: AnalysisType;operatorOptions: optionDataType[]}> => {
    try {
        return { type: params.type, operatorOptions: await getOperatorOptions(params) };
    } catch (error) {
        if (params.type !== AnalysisType.COMMUNICATION_MATRIX || !session.durationFileCompleted) {
            throw error;
        }
        const type = AnalysisType.COMMUNICATION_DURATION_ANALYSIS;
        return { type, operatorOptions: await getOperatorOptions({ ...params, type }) };
    }
};

interface FilterProps {
    session: Session;
    handleFilterChange: VoidFunction;
    onLoadingChange?: (loading: boolean) => void;
}

const Filter = observer(({ session, handleFilterChange, onLoadingChange }: FilterProps) => {
    const [condition, setCondition] = useState<ConditionDataType>(defaultCondition);
    const [optionMap, setOptionMap] = useState<optionMapDataType>(defaultOptionMap);
    const conditionRef = useRef<ConditionDataType>(condition);
    const latestOptionRequestId = useRef(0);
    const activeCommunicator = session.activeCommunicator?.value;

    const handleChange = (key: keyof ConditionDataType, val: any): void => {
        updateCondition(conditionRef.current, key, val);
    };
    const updateCondition = async (initObj: ConditionDataType, key?: keyof ConditionDataType, val?: any): Promise<void> => {
        if (!session.clusterCompleted) {
            delayExecute(() => {
                if (!session.clusterCompleted) {
                    const resetCondition = { ...defaultCondition, type: conditionRef.current.type };
                    conditionRef.current = resetCondition;
                    setCondition(resetCondition);
                    setOptionMap(defaultOptionMap);
                }
            });
            return;
        }
        const requestId = ++latestOptionRequestId.current;
        onLoadingChange?.(true);
        try {
            const { condition: newCondition, optionMap: newOptionMap } = await getOptionsAndValue(session, initObj, optionMap, key, val);
            if (requestId !== latestOptionRequestId.current) {
                return;
            }
            newCondition.targetOperatorName = session.targetOperator?.name;
            conditionRef.current = newCondition;
            setCondition(newCondition);
            setOptionMap(newOptionMap);
        } finally {
            if (requestId === latestOptionRequestId.current) {
                onLoadingChange?.(false);
            }
        }
    };

    useEffect(() => {
        const applyPendingCondition = (): void => {
            const externalCondition = pendingConditionUpdate ?? observeCondition.value;
            pendingConditionUpdate = undefined;
            const newCondition = { ...conditionRef.current, ...externalCondition };
            conditionRef.current = newCondition;
            updateCondition(newCondition);
        };
        const disposer = observe(observeCondition, applyPendingCondition);
        if (pendingConditionUpdate !== undefined) {
            applyPendingCondition();
        }
        return () => {
            disposer();
            latestOptionRequestId.current += 1;
            onLoadingChange?.(false);
        };
    }, []);
    useEffect(() => {
        updateCondition(conditionRef.current);
    }, [session.clusterCompleted, session.durationFileCompleted, session.selectedClusterPath, session.communicatorData.partitionModes, session.isCompare]);
    useEffect(() => {
        setTimeout(() => {
            if (activeCommunicator !== undefined && activeCommunicator !== conditionRef.current.stage) {
                updateCondition(conditionRef.current, 'stage', activeCommunicator);
            }
        });
    }, [activeCommunicator]);
    useEffect(() => {
        const realCondition = { ...condition, clusterPath: session.selectedClusterPath };
        handleFilterChange(realCondition);
    }, [condition]);

    return (<FilterCom handleChange={handleChange} condition={condition} optionMap={optionMap} session={session}/>);
});

interface IcomProps {
    optionMap: optionMapDataType;
    condition: ConditionDataType;
    handleChange: (key: keyof ConditionDataType, val: string) => void;
    session: Session;
}
function FilterCom({ optionMap, condition, handleChange, session }: IcomProps): JSX.Element {
    const { t } = useTranslation('communication');
    const tooltipOperatorName = t('OperatorNameTooltip', { returnObjects: true }) as string[];

    return <Form data-testid="filters" layout="inline">
        <Form.Item label={t('searchCriteria.Cluster')}>
            <ClusterSelect width={200} session={session} />
        </Form.Item>
        <Form.Item label={t('searchCriteria.Step')}>
            <Select
                id={'step-select'}
                value={condition.iterationId}
                style={{ width: 120 }}
                onChange={(val: string): void => {
                    handleChange('iterationId', val);
                }}
                options={optionMap.iterationOptions}
            />
        </Form.Item>
        {
            session.isCompare && <Form.Item label={t('searchCriteria.BaselineStep')}>
                <Select
                    id={'baseline-step-select'}
                    value={condition.baselineIterationId}
                    style={{ width: 120 }}
                    onChange={(val: string): void => {
                        handleChange('baselineIterationId', val);
                    }}
                    options={optionMap.baselineIterationOptions}
                />
            </Form.Item>
        }
        <Form.Item label={t('searchCriteria.CommunicationGroup')}>
            <Select
                id={'communicationGroup-select'}
                value={generateStageKeyValue(condition.groupIdHash, condition.baselineGroupIdHash)}
                style={{ width: 200 }}
                onChange={(val: string): void => {
                    handleChange('stage', val);
                }}
                filterOption={(input: any, option: any): boolean => (option?.label as string).toLowerCase().includes(input.toLowerCase())}
                options={optionMap.stageOptions}
                showSearch
                dropdownMatchSelectWidth
            />
        </Form.Item>
        <Form.Item
            label={t('searchCriteria.OperatorName')}
            tooltip={
                condition.type === AnalysisType.COMMUNICATION_MATRIX
                    ? <div style={{ padding: 6 }}>
                        {tooltipOperatorName.map((item, index) => <div style={{ padding: '4px 0' }} key={index}>{item}</div>)}
                    </div>
                    : false
            }
        >
            <Select
                id={'operatorName-select'}
                value={condition.operatorName}
                style={{ width: 300 }}
                onChange={(val: string): void => {
                    handleChange('operatorName', val);
                }}
                options={optionMap.operatorOptions}
                showSearch={true}
                disabled={session.isCompare}
                dropdownMatchSelectWidth
            />
        </Form.Item>
        { condition.stage !== '' &&
            <Form.Item>
                <Radio.Group value={condition.type}
                    onChange={(e: RadioChangeEvent): void => {
                        handleChange('type', e.target.value);
                    }}>
                    <Radio value={AnalysisType.COMMUNICATION_MATRIX}>{t('searchCriteria.CommunicationMatrix')}</Radio>
                    <Radio
                        value={AnalysisType.COMMUNICATION_DURATION_ANALYSIS}>{t('searchCriteria.CommunicationDurationAnalysis')}</Radio>
                </Radio.Group>
            </Form.Item>
        }
    </Form>;
}

export function FormItem(props: any): JSX.Element {
    return (<div style={{ display: 'inline-block', height: '30px', lineHeight: '30px', margin: '0 20px 10px 0' }}>
        <Label name={props.name}/>
        {props.content}
    </div>);
};

export default Filter;
