/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
export const queryBlockSummaries = (
    blocks: Iterable<Block | BlockMeta>,
    query: BlockQuery,
): BlockQueryResult => {
    const top: BlockQueryItem[] = [];
    const topById = new Map<number, BlockQueryItem>();
    const address = query.address?.trim().toLowerCase();
    let scanned = 0;
    let matched = 0;
    for (const block of blocks) {
        scanned++;
        const item = summarize(block);
        if (address && !item.address.toLowerCase().includes(address)) continue;
        if (item.sizeBytes < (query.minSize ?? 0) || item.lifetime < (query.minLifetime ?? 0)) continue;
        matched++;
        const existing = topById.get(item.id);
        if (existing) {
            existing.startTimestamp = Math.min(existing.startTimestamp, item.startTimestamp);
            existing.endTimestamp = Math.max(existing.endTimestamp, item.endTimestamp);
            existing.lifetime = Math.max(0, existing.endTimestamp - existing.startTimestamp);
            sortTop(top, query.sortBy);
            continue;
        }
        top.push(item);
        topById.set(item.id, item);
        sortTop(top, query.sortBy);
        if (top.length > query.limit) {
            const removed = top.pop();
            if (removed) topById.delete(removed.id);
        }
    }
    return { blocks: top, scannedEntries: scanned, matchedEntries: matched, limit: query.limit, sortBy: query.sortBy };
};

const summarize = (block: Block | BlockMeta): BlockQueryItem => {
    const startTimestamp = finite(block._startTimestamp);
    const endTimestamp = finite(block._endTimestamp);
    return {
        id: block.id,
        address: block.addr,
        startTimestamp,
        endTimestamp,
        lifetime: Math.max(0, endTimestamp - startTimestamp),
        sizeBytes: finite(block.size),
    };
};

const sortTop = (items: BlockQueryItem[], sortBy: BlockQuery['sortBy']): void => {
    items.sort((left, right) => right[sortBy === 'size' ? 'sizeBytes' : 'lifetime'] -
        left[sortBy === 'size' ? 'sizeBytes' : 'lifetime'] || left.id - right.id);
};

const finite = (value: unknown): number => Number.isFinite(Number(value)) ? Number(value) : 0;
