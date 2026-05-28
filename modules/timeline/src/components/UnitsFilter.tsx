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
import { useTheme } from '@emotion/react';
import styled from '@emotion/styled';
import { Tooltip, Select } from '@insight/lib/components';
import { runInAction } from 'mobx';
import { observer } from 'mobx-react';
import React, { useEffect, useRef, useState } from 'react';
import { FilterIcon, HelpIcon } from '@insight/lib/icon';
import type { Session } from '../entity/session';
import { CustomButton } from './base/StyledButton';
import type { InsightUnit } from '../entity/insight';
import type { CardMetaData, MetaDataInnerBase } from '../entity/data';
import { useTranslation } from 'react-i18next';

const CustomDiv = styled.div`
    display: flex;
    align-items: center;
    border-radius: 2px;
    padding: 0 8px;
    .chooseResult {
        display: flex;
        margin-bottom: 0;
        list-style: none;
        padding: 0;
        li {
            display: flex;
            align-items: center;
            height: 24px;
            padding-right: 8px;
            font-size: 14px;
            white-space: nowrap;
            color: ${(props): string => props.theme.deviceProcessContentFontColor};
            background: ${(props): string => props.theme.multiSelectBgColor};
            border-radius: 12px;
            margin: 0 5px;
            .icon {
                width: 12px;
                height: 12px;
                margin-left: 5px;
                fill: ${(props): string => props.theme.buttonColor.enableClickColor};
            }
            & > div {
                padding-left: 8px;
            }
            .ant-select {
                color: ${(props): string => props.theme.deviceProcessContentFontColor};
                .ant-select-selector {
                    height: 24px;
                    border: none;
                    padding: 0 5px;
                    background-color: transparent;
                    .ant-select-selection-search input {
                        height: 24px;
                    }
                    .ant-select-selection-item {
                        display: flex;
                        align-items: center;
                        height: 24px;
                        padding-right: 0;
                    }
                }
                // select禁用的时候颜色不变
                &.ant-select-disabled .ant-select-selector {
                    color: ${(props): string => props.theme.deviceProcessContentFontColor};
                }
                // 去掉下拉三角按钮
                .ant-select-arrow {
                    display: none;
                }
                .ant-select-dropdown {
                    width: 200px!important;
                    border-radius: 16px;
                    left: -7px !important;
                    background: ${(props): string => props.theme.deviceProcessBackgroundColor};
                    .rc-virtual-list-holder-inner {
                        & > div {
                            border-radius: 10px;
                            color: ${(props): string => props.theme.filterColor};
                        }
                        .ant-select-item-option-active {
                            color: ${(props): string => props.theme.filterColor} !important;
                            background-color: ${(props): string => props.theme.filterSelectActiveBgColor} !important;
                        }
                    }
                }
            }
        }
    }
    .searchResult {
        color: ${(props): string => props.theme.svgBackgroundColor};
        font-size: 12px;
        white-space: nowrap;
    }
`;

const FixedTooltipWrapper = styled.div`
    position: absolute;
    top: 15px;
    right: 24px;
`;

interface CompleteOptionProps {
    label: string;
    value: string;
}

export const normalizeUnitFilterName = (name = ''): string => name.replace(/\(\d{0,12}\)/, '').trim();

export const getUnitFilterName = (unit: InsightUnit): string => {
    const metadata = unit.metadata as MetaDataInnerBase;
    if (unit.name === 'Thread' || unit.name === 'Counter') {
        return normalizeUnitFilterName(metadata.threadName);
    }
    return normalizeUnitFilterName(metadata.processName);
};

const expandUnit = (unit: InsightUnit): void => {
    if (!unit.children?.length) {
        return;
    }
    if (unit.collapsible && !unit.isExpanded) {
        unit.collapseAction?.(unit);
    }
    unit.isExpanded = true;
};

const displayUnitTree = (unit: InsightUnit, loopIndex = 0): void => {
    const MaxLoop = 100;
    if (loopIndex > MaxLoop) {
        return;
    }
    unit.isDisplay = true;
    unit.children?.forEach(child => {
        displayUnitTree(child, loopIndex + 1);
    });
};

/**
 * 开始过滤会话中的单位和卡片。
 * @param session - 当前会话对象。
 * @param cardSelection - 选中的卡片名称数组。
 * @param unitSelection - 选中的单位名称数组。
 */
const startFilter = (session: Session, cardSelection: string[], unitSelection: string[]): void => {
    // 设置所有单位为显示状态
    setAllUnitsDisplay(session);
    // 如果没有选择任何卡片或单位，则直接返回
    if (cardSelection.length === 0 && unitSelection.length === 0) {
        return;
    }

    // 如果有选中的卡片，则执行卡片过滤
    if (cardSelection.length !== 0) {
        doCardFilter(session.units, cardSelection);
    }
    // 如果有选中的单位，则执行单位过滤
    if (unitSelection.length !== 0) {
        doUnitsFilter(session.units, unitSelection);
    }

    // 重置单链接线
    session.singleLinkLine = {};
};

/**
 * 执行卡片过滤，根据选中的卡片名称显示或隐藏单位。
 * @param flattenUnits - 扁平化的单位数组。
 * @param selectValues - 选中的卡片名称数组。
 */
const doCardFilter = (flattenUnits: InsightUnit[], selectValues: string[]): void => {
    runInAction(() => {
        flattenUnits.forEach(unit => {
            // 检查当前单位是否为目标单位
            const isTargetUnit = selectValues.includes((unit.metadata as CardMetaData).cardName as string);
            unit.isDisplay = isTargetUnit;
            if (isTargetUnit) {
                // 如果是目标单位，则显示其子单位和子进程
                unit.children?.forEach(unitChild => {
                    unitChild.isDisplay = true;
                    unitChild.children?.forEach(processUnit => {
                        processUnit.isDisplay = true;
                    });
                });
            }
        });
    });
};

/**
 * 执行单位过滤，根据选中的单位名称显示或隐藏单位。
 * @param flattenUnits - 扁平化的单位数组。
 * @param selectValues - 选中的单位名称数组。
 */
export const doUnitsFilter = (flattenUnits: InsightUnit[], selectValues: string[]): void => {
    runInAction(() => {
        const normalizedSelectValues = selectValues.map(normalizeUnitFilterName);
        flattenUnits.forEach(unit => {
            if (!unit.children) {
                // 如果没有子单位，则隐藏该单位
                unit.isDisplay = false;
                return;
            }
            let hasMatchUnit = false;
            unit.children.forEach(childUnit => {
                const isUnitMatch = normalizedSelectValues.includes(getUnitFilterName(childUnit));
                const hasMatchedChild = isUnitMatch ? false : findMatchUnit(childUnit, normalizedSelectValues);
                if (isUnitMatch) {
                    displayUnitTree(childUnit);
                } else {
                    childUnit.isDisplay = hasMatchedChild;
                }
                if (childUnit.isDisplay) {
                    expandUnit(childUnit);
                }
                hasMatchUnit = hasMatchUnit || childUnit.isDisplay;
            });
            if (hasMatchUnit) {
                expandUnit(unit);
            } else {
                // 如果没有匹配的子单位，则隐藏该单位
                unit.isDisplay = false;
            }
        });

        flattenUnits.forEach(unit => {
            if (!unit.children) {
                return;
            }
            if (unit.name === 'Thread') {
                // 如果是线程单位，则根据线程名称设置显示状态
                if (normalizedSelectValues.includes(getUnitFilterName(unit))) {
                    displayUnitTree(unit);
                    expandUnit(unit);
                }
            } else {
                // 如果是进程单位，则根据进程名称设置显示状态
                if (normalizedSelectValues.includes(getUnitFilterName(unit))) {
                    displayUnitTree(unit);
                    expandUnit(unit);
                }
            }
        });
    });
};

/**
 * 检查单元是否与给定的选择值匹配。
 * @param unit - 要检查的单元。
 * @param selectValues - 选择值数组。
 * @returns 如果单元匹配则返回true，否则返回false。
 */
export const findMatchUnit = (unit: InsightUnit, selectValues: string[]): boolean => {
    let hasMatchUnit = false;
    unit.children?.forEach(childUnit => {
        const isUnitMatch = selectValues.includes(getUnitFilterName(childUnit));
        const hasMatchedChild = isUnitMatch ? false : findMatchUnit(childUnit, selectValues);
        if (isUnitMatch) {
            displayUnitTree(childUnit);
        } else {
            childUnit.isDisplay = hasMatchedChild;
        }
        if (childUnit.isDisplay) {
            expandUnit(childUnit);
        }
        hasMatchUnit = hasMatchUnit || childUnit.isDisplay;
    });
    if (hasMatchUnit) {
        expandUnit(unit);
    }
    return hasMatchUnit;
};

/**
 * 设置所有单元为显示状态。
 * @param session - 包含单元的会话对象。
 */
const setAllUnitsDisplay = (session: Session): void => {
    const setUnitDisplay = (unit: InsightUnit, loopIndex = 0): void => {
        const MaxLoop = 100;
        if (loopIndex > MaxLoop) {
            return;
        }
        runInAction(() => {
            unit.isDisplay = true;
        });
        if (unit.children) {
            for (const child of unit.children) {
                setUnitDisplay(child, loopIndex++); // 递归调用，增加循环索引
            }
        }
    };
    for (const unit of session.units) {
        setUnitDisplay(unit);
    }
};

/**
 * 获取单元名称集合。
 * @param session - 包含单元的会话对象。
 * @returns 包含卡片名称和单元名称的集合对象。
 */
export const useUnitsNameSet = (session: Session): { cardNames: Set<string>; unitNames: Set<string> } => {
    const cardNames = new Set<string>();
    const unitNames = new Set<string>();

    const visitUnit = (unit: InsightUnit, loopIndex = 0): void => {
        const MaxLoop = 100;
        if (loopIndex > MaxLoop) {
            return;
        }
        if (unit.name === 'Card' && !unit.isMultiDeviceHidden) {
            cardNames.add((unit.metadata as CardMetaData).cardName ?? '');
        }
        if (unit.name === 'Process' || unit.name === 'Label') {
            const metaDataName = getUnitFilterName(unit);
            if (metaDataName === 'Process Scheduling' || (!metaDataName.toLowerCase().includes('process') && !metaDataName.toLowerCase().includes('thread'))) {
                unitNames.add(metaDataName);
            }
        }
        if (unit.name === 'Thread') {
            const metaDataName = getUnitFilterName(unit);
            if (metaDataName && metaDataName !== '' && ['pytorch', 'mstx', 'python gc', 'cann', 'os runtime api'].includes(metaDataName.toLowerCase())) {
                unitNames.add(metaDataName);
            }
        }
        if (unit.children) {
            for (const child of unit.children) {
                visitUnit(child, loopIndex++); // 递归调用，增加循环索引
            }
        }
    };

    session.units.forEach(unit => {
        visitUnit(unit);
    });
    return { cardNames, unitNames };
};

/**
 * 渲染分类搜索内容组件。
 * @param session - 包含单元的会话对象。
 * @returns JSX元素
 */
const CategorySearchContent = (session: Session): JSX.Element => {
    const theme = useTheme();
    const { cardNames, unitNames } = useUnitsNameSet(session);
    const [completeCardOptions, setCompleteCardOptions] = useState<CompleteOptionProps[]>([]);
    const [completeUnitOptions, setCompleteUnitOptions] = useState<CompleteOptionProps[]>([]);
    const [cardSelection, setCardSelection] = useState<string[]>([]);
    const [unitSelection, setUnitSelection] = useState<string[]>([]);
    const { t } = useTranslation();

    /**
     * 处理搜索操作。
     * @param value - 搜索值。
     * @param key - 搜索类型，可以是'Card'或'Unit'
     */
    const handleSearch = (value: string, key = 'Card'): void => {
        const result: CompleteOptionProps[] = [];
        let targetSet = new Set<string>();
        if (key === 'Unit') {
            targetSet = unitNames;
        }
        if (key === 'Card') {
            targetSet = cardNames;
        }
        targetSet.forEach((cardName) => {
            if (cardName.toLowerCase().includes(value.toLowerCase())) {
                result.push({ label: cardName, value: cardName });
            }
        });
        if (key === 'Unit') {
            setCompleteUnitOptions(result);
        }
        if (key === 'Card') {
            setCompleteCardOptions(result);
        }
    };

    // 切换项目时 session.projectName 改变，触发 selectValue 重置
    useEffect(() => {
        setCompleteUnitOptions([]);
        setCompleteCardOptions([]);
        setCardSelection([]);
        setUnitSelection([]);
    }, [session.projectName]);

    useEffect(() => {
        startFilter(session, cardSelection, unitSelection);
    }, [cardSelection, unitSelection]);

    useEffect(() => {
        startFilter(session, cardSelection, unitSelection);
    }, [cardSelection, unitSelection]);

    if (completeCardOptions.length !== cardNames.size && cardNames.size > 0) {
        handleSearch('', 'Card');
    }
    if (completeUnitOptions.length !== unitNames.size && unitNames.size > 0) {
        handleSearch('', 'Unit');
    }

    useEffect(() => {
        // 检查 session.units 数组是否为空
        if (session.units.length === 0) {
            // 如果为空，则将 completeUnitOptions 设置为空数组
            setCompleteUnitOptions([]);
            // 将 completeCardOptions 设置为空数组
            setCompleteCardOptions([]);
            // 将 cardSelection 设置为空数组
            setCardSelection([]);
            // 将 unitSelection 设置为空数组
            setUnitSelection([]);
        }
    }, [session.units]); // 依赖数组，当 session.units 发生变化时，执行该 effect

    return (
        <CustomDiv theme={theme}>
            <div style={ { margin: '8px' } }>
                <div style={ { marginBottom: '8px' } }>{ t('Card Filter', { ns: 'timeline' }) }</div>
                <Select
                    id={ 'select-card-filter-content' }
                    mode="multiple"
                    allowClear
                    placeholder={ t('Please select card', { ns: 'timeline' }) }
                    options={ completeCardOptions }
                    width={ 280 }
                    value={ cardSelection }
                    onChange={ (val: string[]) => setCardSelection(val) }
                >
                </Select>
                <div
                    style={ { marginBottom: '8px', marginTop: '16px' } }>{ t('Units Filter', { ns: 'timeline' }) }</div>
                <Select
                    id={ 'select-unit-filter-content' }
                    mode="multiple"
                    allowClear
                    placeholder={ t('Please select unit', { ns: 'timeline' }) }
                    options={ completeUnitOptions }
                    width={ 280 }
                    value={ unitSelection }
                    onChange={ (val: string[]) => setUnitSelection(val) }
                >
                </Select>
            </div>
            <FixedTooltipWrapper>
                <Tooltip placement="bottom" title={t('Filter ToolTip', { ns: 'timeline' })}>
                    <HelpIcon style={{ cursor: 'pointer' }} height={20} width={20} />
                </Tooltip>
            </FixedTooltipWrapper>
        </CustomDiv>
    );
};

/**
 * UnitsFilter组件是一个用于过滤的按钮组件，它使用了observer来观察状态变化。
 * @param {Object} props - 组件的属性
 * @param {Session} props.session - 会话信息
 * @returns {JSX.Element | null} 返回一个JSX元素或null
 */
export const UnitsFilter = observer(({ session }: { session: Session}): JSX.Element | null => {
    // 定义一个状态来控制按钮的强调和暂停效果
    const [customButtonProps, updateCustomButtonProps] = useState({
        isEmphasize: false,
        isSuspend: false,
    });

    /**
     * tooltip显隐控制悬浮效果的函数
     * @param {boolean} visible - tooltip是否可见
     */
    const onTooltipVisibleChange = (visible: boolean): void => {
        // 更新按钮的暂停状态
        updateCustomButtonProps({ ...customButtonProps, isSuspend: visible });

        // 设置一个定时器，用于更新所有输入框的maxlength属性
        setTimeout(() => {
            const inputs = document.querySelectorAll('input');
            inputs.forEach(input => {
                input.setAttribute('maxlength', '200');
            });
        });
    };

    // 创建一个引用，用于指向HTMLButtonElement
    const ref = useRef<HTMLButtonElement>(null);

    // 使用useTranslation钩子来获取翻译函数
    const { t } = useTranslation();

    // 返回一个带有tooltip的自定义按钮组件
    return (
        <Tooltip overlayStyle={{ maxWidth: 1000 }}
            overlayInnerStyle={{ borderRadius: 2 }}
            title={CategorySearchContent(session)}
            trigger="click"
            onOpenChange={onTooltipVisibleChange}
            overlayClassName="insight-category-search-overlay"
            align={{ offset: [-8, 3] }}
            zIndex={1040}
        >
            <CustomButton data-testid={'tool-filter'} tooltip={t('tooltip:filter')} icon={FilterIcon as any} { ...customButtonProps } ref={ref}/>
        </Tooltip>
    );
});
