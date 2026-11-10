/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
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
