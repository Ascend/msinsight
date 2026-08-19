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
import type { Session, MapValueOfLinkLines } from '../entity/session';
import jumpToUnitOperator from '../utils/jumpToUnitOperator';
import { CONTEXT_MENU_SEPARATOR, type ContextMenuItem } from '../components/ContextMenu';
import type { ActionName } from './types';

function getLinkSlices(session: Session): MapValueOfLinkLines | undefined {
    if (!session.selectedData) {
        return undefined;
    }
    const { depth, timestamp, threadId, processId } = session.selectedData;
    const mapKey = `${processId}_${threadId}_${depth}_${timestamp}`;
    return session.mapOfLinkLines.get(mapKey);
}

function getMenuBySlices(mapValue: MapValueOfLinkLines, linkType: 'from' | 'to'): ContextMenuItem[] {
    const slices = mapValue[linkType];
    if (mapValue[linkType].length === 0) {
        return [];
    }
    const menuItems: ContextMenuItem[] = [];
    const label = `timeline:contextMenu.${linkType === 'to' ? 'To' : 'From'}`;
    menuItems.push(register({ name: '' as ActionName, label, disabled: () => true, perform: () => {} }), CONTEXT_MENU_SEPARATOR);
    for (let i = 0; i < slices.length; i++) {
        const slice = slices[i];
        const isValidLabel = mapValue.cat && slice.name;
        const menuItem = register({
            name: '' as ActionName,
            label: isValidLabel ? `${slice.name}(${mapValue.cat})` : slice.name || mapValue.cat || '--',
            perform: (session: Session) => {
                if (!session.selectedData) {
                    return;
                }
                const mapValue = getLinkSlices(session);
                if (!mapValue) {
                    return;
                }
                jumpToUnitOperator({ ...slice, cardId: slice.rankId });
            },
        });
        menuItems.push(menuItem, CONTEXT_MENU_SEPARATOR);
    }
    return menuItems;
}

function getMenuItems(mapValue: MapValueOfLinkLines): ContextMenuItem[] {
    const subMenus: ContextMenuItem[] = [...getMenuBySlices(mapValue, 'from'), ...getMenuBySlices(mapValue, 'to')];
    if (subMenus[subMenus.length - 1] === CONTEXT_MENU_SEPARATOR) {
        subMenus.pop();
    }
    return subMenus;
}

export const actionJumpToLinkSlice = register({
    name: 'jumpToLinkSlice',
    label: 'timeline:contextMenu.JumpToLinkSlice',
    subMode: true,
    subMenus: (session: Session): ContextMenuItem[] => {
        if (!session.selectedData || session.mapOfLinkLines.size === 0) {
            return [];
        }
        const mapValue = getLinkSlices(session);
        if (!mapValue) {
            return [];
        }
        return getMenuItems(mapValue);
    },
    visible: (session: Session): boolean => {
        if (!session.selectedData) {
            return false;
        }
        const mapValue = getLinkSlices(session);
        if (!mapValue) {
            return false;
        }
        return [...mapValue.from, ...mapValue.to].length > 0;
    },
    perform: (session: Session): void => {},
});
