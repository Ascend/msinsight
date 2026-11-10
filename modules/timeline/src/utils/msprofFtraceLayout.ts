/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */

interface FtraceIdentity {
    cardId?: unknown;
    rankId?: unknown;
    dbPath?: unknown;
}

export const isMsprofFtraceCard = (metadata: FtraceIdentity | Record<string, unknown>): boolean =>
    [metadata.cardId, metadata.rankId, metadata.dbPath].some(value =>
        typeof value === 'string' && /::msprof_ftrace(?:::baseline:\d+)?$/.test(value),
    );
