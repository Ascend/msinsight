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
import { KEYS } from '@insight/lib/utils';
import type { Session } from '../entity/session';
import { runInAction } from 'mobx';

const lockSelectionMenuVisible = (session: Session): boolean => {
    if (session.selectedRangeIsLock || session.isTimeAnalysisMode) {
        return false;
    }
    if (session.selectedRange === undefined || session.selectedUnits === undefined) {
        return false;
    }
    return session.selectedUnits.length !== 0;
};

const lockSelectionArea = (session: Session): void => {
    if (session.selectedRange === undefined || session.selectedUnits === undefined) {
        return;
    }
    if (session.selectedUnits.length === 0) {
        return;
    }
    runInAction(() => {
        session.selectedRangeIsLock = true;
        session.lockUnitCount = session.selectedUnits.length;
        session.lockRange = session.selectedRange;
        session.lockUnit = session.selectedUnits;
    });
};

const unlockSelectionArea = (session: Session): void => {
    runInAction(() => {
        session.clearSelectionLock();
    });
};

export const actionLockSelection = register({
    name: 'lockSelection',
    label: 'timeline:contextMenu.Lock selection area',
    visible: (session: Session): boolean => lockSelectionMenuVisible(session),
    perform: (session): void => {
        lockSelectionArea(session);
    },
    keyTest: (event) => event.key.toLowerCase() === KEYS.S,
});

export const actionUnLockSelection = register({
    name: 'unlockSelection',
    label: 'timeline:contextMenu.Unlock selection area',
    visible: (session: Session): boolean => session.selectedRangeIsLock,
    perform: (session): void => {
        unlockSelectionArea(session);
    },
    keyTest: (event) => event.key.toLowerCase() === KEYS.S,
});
