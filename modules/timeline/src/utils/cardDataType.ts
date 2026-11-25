/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

import type { CardRankInfo } from '../api/interface';

interface CardSession {
    rankCardInfoMap: Map<string, CardRankInfo>;
    isFullDb: boolean;
}

export const isFullDbCard = (session: CardSession, card: { cardId: string; dbPath: string }): boolean => {
    const info = Array.from(session.rankCardInfoMap.values()).find(value =>
        value.rankInfo.rankId === card.cardId && value.dbPath === card.dbPath,
    );
    if (info?.isFullDb !== undefined) {
        return info.isFullDb;
    }
    return info?.isFtrace === true ? false : session.isFullDb;
};
