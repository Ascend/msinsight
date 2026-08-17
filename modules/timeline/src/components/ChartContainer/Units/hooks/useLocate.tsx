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

import { autorun, runInAction } from 'mobx';
import React from 'react';
import { message } from 'antd';
import { t } from 'i18next';
import type { PreOrderFlattenOptions, TreeNode } from '../../../../entity/common';
import { InsightUnit, UnitHeight, UnitMatcher } from '../../../../entity/insight';
import type { Session } from '../../../../entity/session';
import { getRootUnit } from '../../../../utils';
/**
 * Searches a list of given @param units recursively in pre-order, comparing them with @param matcher, and save the result path in @path
 *
 * @param units the root units to start searching from
 * @param matcher matcher
 * @param path if a matching unit is found, keeps the path from root to the matching unit.
 * @returns true if found a matching unit
 */
const searchUnit = (units: InsightUnit[], matcher: UnitMatcher['target'], path: InsightUnit[]): boolean => {
    for (const unit of units) {
        path.push(unit);
        if (unit.isUnitVisible && !unit.isMerged && matcher(unit)) {
            return true;
        }
        if (unit.children && searchUnit(unit.children, matcher, path)) {
            return true;
        }
        path.pop();
    }
    return false;
};

const getTargetUnit = (units: InsightUnit[], matcher: UnitMatcher['target']): InsightUnit | undefined => {
    const path: InsightUnit[] = [];
    if (!searchUnit(units, matcher, path)) {
        return undefined;
    }
    runInAction(() => {
        // expand all parent units
        for (const unit of path) {
            if (unit.collapsible && !unit.isExpanded) {
                unit.collapseAction?.(unit);
            }
            unit.isExpanded = true;
        }
    });
    return path[path.length - 1];
};

const getNormalUnitHeight = (unitsArea: InsightUnit[], orderOptions: OrderOptions, targetUnit: InsightUnit): number | undefined => {
    const flattenUnits = orderOptions.preOrderFlatten(getRootUnit(unitsArea), 0, orderOptions.options).filter(unit => unit.isUnitVisible && !unit.isMerged);
    let findResult = false;
    let height = 0;
    for (const unit of flattenUnits) {
        if (unit === targetUnit) {
            findResult = true;
            break;
        }
        height += unit.height() + 1;
    }
    height -= 10;
    return findResult ? height : undefined;
};

export interface OrderOptions {
    preOrderFlatten: <T>(tree: Array<TreeNode<T>>, currentLevel: number, options?: PreOrderFlattenOptions<T> | undefined) => T[];
    options: PreOrderFlattenOptions<InsightUnit>;
};

export const useJumpTarget = (session: Session, unitsArea: InsightUnit[], supportJump: boolean,
    orderOptions: OrderOptions, dom: HTMLDivElement | null): void => {
    const scrollToResult = (scrollHResult: number, callback?: (scrolled: number) => void): void => {
        requestAnimationFrame(() => {
            dom?.scrollTo(0, scrollHResult);
            callback?.(scrollHResult);
        });
    };

    const handleUnitSelection = (targetUnit: InsightUnit): void => {
        session.selectedUnits = [targetUnit];
    };

    // 绘制完成泳道后再微调滚动条
    const tuningScroller = React.useCallback((scrolled: number): void => {
        if (dom === null || !supportJump || session.selectedData === undefined) { return; }
        // UnitHeight.STANDARD 是展开的 Slice 标准高度；1 是 Slice 之间的间隔
        const relativeSliceY: number = Number.isInteger(session.selectedData.depth)
            ? (UnitHeight.STANDARD + 1) * Math.max(session.selectedData.depth as number - 1, 0)
            : 0;
        const halfScrollerHeight = dom.clientHeight / 2;
        const offset = Math.max(relativeSliceY - halfScrollerHeight, 0);
        scrollToResult(scrolled + offset);
    }, [session, dom]);

    React.useEffect(() => autorun(
        () => {
            if (dom === null || !supportJump) { return; }
            if (session.locateUnit === undefined) { return; }
            const targetUnit = getTargetUnit(getRootUnit(session.units), session.locateUnit.target);
            if (targetUnit === undefined) {
                message.warn(t('NotFoundJumpTargetWarn'));
            } else {
                handleUnitSelection(targetUnit);
                session.locateUnit?.onSuccess(targetUnit);
                const scrollHResult = getNormalUnitHeight(unitsArea, orderOptions, targetUnit);
                if (scrollHResult !== undefined) {
                    // 第一次 scrollToResult 到 scrollHResult，会请求后端重新绘制泳道
                    const tuningCallback = session.locateUnit?.tuneToSelectedSlice === false ? undefined : tuningScroller;
                    scrollToResult(scrollHResult, tuningCallback);
                }
            }
            runInAction(() => {
                session.locateUnit = undefined;
            });
        },
    ), [session, dom, unitsArea, tuningScroller]);
};
