/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

import type { Session } from '../entity/session';
import { getFlowPointIdentity } from '../entity/data';
import jumpToUnitOperator from '../utils/jumpToUnitOperator';
import { actionJumpToLinkSlice } from './actionJumpToLinkSlice';

jest.mock('./register', () => ({ register: (action: unknown) => action }));
jest.mock('../components/ContextMenu', () => ({ CONTEXT_MENU_SEPARATOR: 'separator' }));
jest.mock('../utils/jumpToUnitOperator', () => ({ __esModule: true, default: jest.fn() }));

const cpu = {
    rankId: 'rank0',
    dbPath: 'model.db',
    pid: '16659465180491698',
    tid: '20000',
    depth: 2,
    timestamp: 10,
    id: '212268',
    name: 'aclmdlRIExecuteAsync',
    metaType: 'CANN_API',
    duration: 36,
};
const npu = {
    rankId: 'rank0',
    dbPath: 'model.db',
    pid: 'Ascend Hardware',
    tid: '46',
    depth: 0,
    timestamp: 110,
    id: '2',
    name: 'MODEL_EXECUTE',
    metaType: 'Ascend Hardware',
    duration: 3,
};

describe.each(['ModelExecute', 'MODEL_EXECUTE', 'Model Execute', 'HostToDevice(Graph)'])('preserves category %s', category => {
    it.each([[cpu, npu, 'to'], [npu, cpu, 'from']] as const)('jumps without changing the endpoint (%s)', (selected, target, direction) => {
        const session = {
            selectedData: { ...selected, cardId: selected.rankId, processId: selected.pid, threadId: selected.tid },
            mapOfLinkLines: new Map([[getFlowPointIdentity(selected), {
                cat: category, from: direction === 'from' ? [target] : [], to: direction === 'to' ? [target] : [],
            }]]),
        } as unknown as Session;
        expect(actionJumpToLinkSlice.visible?.(session)).toBe(true);
        const menu = actionJumpToLinkSlice.subMenus?.(session).find(
            item => typeof item === 'object' && item.label === `${target.name}(${category})`);
        expect(menu).toBeDefined();
        if (typeof menu === 'object') {
            menu.perform?.(session);
        }
        expect(npu.name).toBe('MODEL_EXECUTE');
        expect(session.mapOfLinkLines.get(getFlowPointIdentity(selected))?.cat).toBe(category);
        expect(jumpToUnitOperator).toHaveBeenLastCalledWith({ ...target, cardId: target.rankId });
    });
});
