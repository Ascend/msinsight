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
import { runInAction } from 'mobx';
import type { Session } from '../entity/session';
import type { CardMetaData, ThreadMetaData } from '../entity/data';
import { ProjectType } from '../entity/insight';

const isEventMenuVisible = (session: Session): boolean => {
    // 必须只选中一个才能显示“跳转数据窗口事件视图”菜单项
    if (session.selectedUnits.length !== 1) {
        return false;
    }
    const selectUnit = session.selectedUnits[0];
    if (selectUnit === undefined || session.isSimulation) {
        return false;
    }
    if ((selectUnit.metadata as Partial<ThreadMetaData>).metaType === 'DPU') {
        return false;
    }
    if (['Empty', 'Card', 'Counter', 'Root'].includes(selectUnit.name)) {
        return false;
    }
    if (selectUnit.children) {
        for (const child of selectUnit.children) {
            if (child.name === 'Counter') {
                return false;
            }
        }
    }
    if ((selectUnit.metadata as ThreadMetaData).threadName?.includes('Plane')) {
        return false;
    }
    const targetInfo = session.units.find(unitItem => (unitItem.metadata as CardMetaData)?.cardId === (selectUnit.metadata as CardMetaData)?.cardId);
    if (targetInfo?.projectType === ProjectType.IE) {
        return false;
    }
    return true;
};

export const actionShowInEventsView = register({
    name: 'showInEventsView',
    label: 'timeline:contextMenu.Show in Events View',
    visible: (session) => isEventMenuVisible(session),
    perform: (session): void => {
        runInAction(() => {
            session.showEvent = !session.showEvent;
            session.eventUnits = session.selectedUnits;
        });
    },
});
