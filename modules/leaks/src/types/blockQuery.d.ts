/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

interface BlockQuery {
    sortBy: 'size' | 'lifetime';
    limit: number;
    address?: string;
    minSize?: number;
    minLifetime?: number;
}

interface BlockQueryItem {
    id: number;
    address: string;
    startTimestamp: number;
    endTimestamp: number;
    lifetime: number;
    sizeBytes: number;
}

interface BlockQueryResult {
    blocks: BlockQueryItem[];
    scannedEntries: number;
    matchedEntries: number;
    limit: number;
    sortBy: BlockQuery['sortBy'];
}
