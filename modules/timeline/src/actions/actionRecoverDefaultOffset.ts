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

import { runInAction } from 'mobx';
import { register } from './register';
import type { Session } from '../entity/session';
import type { CardMetaData } from '../entity/data';
import { getCardOffsetKey } from '../insight/units/offset';

export function recoverCardCategoryOffsets(session: Session): void {
    const nextOffsets: Record<string, number> = {};
    session.units.forEach((unit) => {
        const cardId = (unit.metadata as CardMetaData)?.cardId;
        if (cardId === undefined) {
            return;
        }
        nextOffsets[getCardOffsetKey(session, { cardId, side: 'host' })] = 0;
        nextOffsets[getCardOffsetKey(session, { cardId, side: 'device' })] = 0;
    });
    session.replaceTimestampOffsets(nextOffsets);
    session.setDomainWithoutHistory({ domainStart: 0, domainEnd: session.endTimeAll ?? session.domain.defaultDuration });
    runInAction(() => {
        session.benchMarkData = undefined;
        session.alignSliceData = [];
        session.alignRender = !session.alignRender;
    });
}

export const actionRecoverDefaultOffset = register({
    name: 'recoverDefaultOffset',
    label: 'timeline:contextMenu.Recover cards default offset',
    perform: recoverCardCategoryOffsets,
});
