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

import { register } from './register';
import type { Session } from '../entity/session';
import { runInAction } from 'mobx';
import { InsightUnit } from '../entity/insight';
import { EmptyMetaData } from '../entity/data';
import { EmptyUnit } from '../components/ContextMenu';
import { isPinned, switchPinned } from '../components/ChartContainer/unitPin';

const MAX_RECURSIVE_COUNT = 10;

function hideUnit(session: Session): void {
    if (session.selectedRangeIsLock || session.isTimeAnalysisMode) {
        return;
    }
    hideUnits(session, session.selectedUnits);
    runInAction(() => {
        session.renderTrigger = !session.renderTrigger;
    });
}

function unpinUnitIfPinned(session: Session, insightUnit: InsightUnit): void {
    if (isPinned(insightUnit)) {
        runInAction(() => {
            session.pinnedUnits = session.pinnedUnits.filter((item) =>
                item.metadata !== insightUnit.metadata);
            switchPinned(insightUnit);
        });
    }
}

const setChildrenUnitHide = (units: InsightUnit[]): void => {
    const hideChildrenUnit = (insightUnit: InsightUnit, count = 1): void => {
        runInAction(() => {
            insightUnit.isUnitVisible = false;
        });
        if (insightUnit.children && count <= MAX_RECURSIVE_COUNT) {
            for (const child of insightUnit.children) {
                hideChildrenUnit(child, count + 1);
            }
        }
    };
    for (const insightUnit of units) {
        hideChildrenUnit(insightUnit);
    }
};

const hideUnits = (session: Session, selectUnits: InsightUnit[]): void => {
    const hideEveryUnit = (insightUnit: InsightUnit, ancestorsIsVisible: boolean = true, count = 1): void => {
        /**
         * 对于有父子Unit被选中的情况
         * 由于代码是从顶向下找的，当发现第一个父Unit是选中的，就直接 hideSelectUnit，然后退出程序
         * 这样可以避免再次选中子Unit导致子Unit又创建一个Hidden Unit这种不希望的情形出现
         */
        if (selectUnits.some((item): boolean => item.metadata === insightUnit.metadata)) {
            unpinUnitIfPinned(session, insightUnit);
            if (ancestorsIsVisible) { // 只有祖先都是可见的情况下，才需要隐藏，否则不需要处理
                hideSelectUnit(insightUnit);
            }
        } else if (insightUnit.children && count <= MAX_RECURSIVE_COUNT) {
            for (const child of insightUnit.children) {
                // 如果当前泳道已经是不可见（隐藏）的，设为 false, 否则用本次传入的
                hideEveryUnit(child, insightUnit.isUnitVisible ? ancestorsIsVisible : false, count + 1);
            }
        }
    };

    const hideSelectUnit = (insightUnit: InsightUnit): void => {
        runInAction(() => {
            insightUnit.isUnitVisible = false;
            session.selectedUnits = [];
            updateEmptyUnits(insightUnit);
        });

        if (insightUnit.children) {
            setChildrenUnitHide(insightUnit.children);
        }
    };

    const updateEmptyUnits = (insightUnit: InsightUnit): void => {
        if (insightUnit.parent === undefined) {
            const rankEmptyUnit = session.units.find(item => item.name === 'Empty');
            if (rankEmptyUnit !== undefined) {
                (rankEmptyUnit.metadata as EmptyMetaData).count++;
            } else {
                session.units.push(new EmptyUnit({
                    count: 1,
                    dataSource: insightUnit.metadata.dataSource,
                } as EmptyMetaData));
            }
        } else {
            const emptyUnit = insightUnit.parent.children?.find(item => item.name === 'Empty');
            if (emptyUnit !== undefined) {
                (emptyUnit.metadata as EmptyMetaData).count++;
            } else {
                insightUnit.parent.children?.push(new EmptyUnit({ count: 1 } as EmptyMetaData));
            }
        }
    };

    for (const sessionUnit of session.units) {
        hideEveryUnit(sessionUnit);
    }
};

const showAllHidedUnits = (session: Session): void => {
    const setUnitNotHide = (insightUnit: InsightUnit): void => {
        runInAction(() => {
            insightUnit.isUnitVisible = true;
            if (insightUnit.name === 'Empty') {
                insightUnit.parent?.children?.pop();
            }
        });

        if (insightUnit.children) {
            insightUnit.children.forEach(child => setUnitNotHide(child));
        }
    };

    const handleEmptyUnit = (selectUnit: InsightUnit): void => {
        if (selectUnit.parent) {
            setUnitNotHide(selectUnit.parent);
        } else {
            // 顶层 EmptyUnit 的处理方式
            session.units.forEach(cardUnit => setUnitNotHide(cardUnit));
            runInAction(() => {
                session.units.pop();
            });
        }
    };

    const showChildrenUnits = (insightUnit: InsightUnit): void => {
        if (insightUnit.children) {
            insightUnit.children.forEach(child => setUnitNotHide(child));
        }
    };

    session.selectedUnits.forEach((selectUnit): void => {
        if (selectUnit !== undefined) {
            if (selectUnit.name === 'Empty') {
                handleEmptyUnit(selectUnit);
            } else {
                showChildrenUnits(selectUnit);
            }
        }
    });
};

function showHiddenUnit(session: Session): void {
    showAllHidedUnits(session);
    runInAction(() => {
        // Remove restored placeholders from the selection before further checkbox selections.
        session.selectedUnits = session.selectedUnits.filter(unit => unit.name !== 'Empty');
        session.renderTrigger = !session.renderTrigger;
    });
}

const isShowUnitsMenuVisible = (session: Session): boolean => {
    // 必须只选中一个才能显示“显示全部已隐藏泳道”菜单项
    if (session.selectedUnits.length !== 1) {
        return false;
    }
    if (session.selectedUnits[0].name === 'Empty') {
        return true;
    }
    if (session.selectedUnits[0].children) {
        for (const child of session.selectedUnits[0].children) {
            if (!child.isUnitVisible) {
                return true;
            }
        }
    }
    return false;
};

const isHideUnitMenuVisible = (session: Session): boolean => {
    if (session.selectedUnits.length === 0) {
        return false;
    }
    return session.selectedUnits.every((item): boolean => item.name !== 'Empty');
};

export const actionHideUnits = register({
    name: 'hideUnits',
    label: 'timeline:contextMenu.Hide',
    visible: (session: Session) => isHideUnitMenuVisible(session),
    disabled: (session: Session) => session.selectedRangeIsLock || session.isTimeAnalysisMode,
    perform: (session): void => {
        hideUnit(session);
    },
});

export const actionShowHiddenUnits = register({
    name: 'showHiddenUnits',
    label: 'timeline:contextMenu.Show all',
    visible: (session: Session) => isShowUnitsMenuVisible(session),
    disabled: (session: Session) => session.selectedRangeIsLock || session.isTimeAnalysisMode,
    perform: (session): void => {
        showHiddenUnit(session);
    },
});
