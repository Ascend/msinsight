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

import { runInAction } from 'mobx';
import { message } from 'antd';
import { transformCardIdInfo } from '@insight/lib/utils';
import { register } from './register';
import { parseCards } from '../api/request';
import type { InsightUnit } from '../entity/insight';
import type { CardMetaData, ThreadMetaData } from '../entity/data';
import type { Session } from '../entity/session';
import { preOrderFlatten } from '../entity/common';
import { getRootUnit } from '../utils';
import { CardUnit } from '../insight/units/AscendUnit';

function getRankIdByCardId(cardId: string): string {
    const cardIdInfo = transformCardIdInfo(cardId);
    return cardIdInfo.rankName !== '' ? cardIdInfo.rankName : cardIdInfo.deviceId;
}

function getSelectedRankList(session: Session): string[] {
    return session.selectedUnits.map((item) => (item.metadata as ThreadMetaData).rankList ?? []).flat();
}

function getUnparsedCards(session: Session, rankIds: string[]): InsightUnit[] {
    return preOrderFlatten(getRootUnit(session.units), 0, {
        when: (node) => !(node instanceof CardUnit && node.metadata?.cardName !== 'Host'),
    })
        .filter((item) => item instanceof CardUnit && item.metadata?.cardName !== 'Host' && item.shouldParse &&
            !item.isParseLoading && !item.isWaitingParseSuccessEvent)
        .filter((item) => rankIds.includes(getRankIdByCardId((item.metadata as CardMetaData).cardId)));
}

function getUnparsedCardIds(session: Session): string[] {
    let unparsedCards: InsightUnit[] = [];

    // 必须只选中一个
    if (session.selectedUnits.length === 1) {
        const rankList = getSelectedRankList(session);
        if (Array.isArray(rankList) && rankList.length !== 0) {
            unparsedCards = getUnparsedCards(session, rankList);
        }
    }

    return unparsedCards.map((item) => (item.metadata as CardMetaData).cardId);
}

export const actionParseCardsOfRelatedGroup = register({
    name: 'parseCardsOfRelatedGroup',
    label: (session, t) => {
        const unparsedCardIds = getUnparsedCardIds(session);
        return unparsedCardIds.length === 0
            ? t('timeline:contextMenu.Parsed Cards of Related Group')
            : t('timeline:contextMenu.Parse Cards of Related Group', { ranks: unparsedCardIds.map(getRankIdByCardId).join(',') });
    },
    disabled: (session) => {
        const unparsedCardIds = getUnparsedCardIds(session);
        return unparsedCardIds.length === 0;
    },
    visible: (session) => {
        if (session.selectedUnits.length !== 1) {
            return false;
        }
        const rankList = getSelectedRankList(session);
        return Array.isArray(rankList) && rankList.length !== 0;
    },
    perform: (session): void => {
        const rankList = getSelectedRankList(session);
        const unparsedCards = getUnparsedCards(session, rankList);
        const unparsedCardIds = unparsedCards.map((item) => (item.metadata as CardMetaData).cardId);
        const unparsedCardFilePaths = unparsedCards.map((item) => (item.metadata as CardMetaData).dbPath);

        parseCards({ cards: unparsedCardIds, dbPaths: unparsedCardFilePaths }).then((): void => {
            runInAction((): void => {
                unparsedCards.forEach((item): void => {
                    item.isParseLoading = true;
                });
            });
        }).catch(err => {
            message.error(err);
        });
    },
});
