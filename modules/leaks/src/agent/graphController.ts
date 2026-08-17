/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */
import { runInAction } from 'mobx';
import { COMMAND_ERROR_CODES, CommandError, type JsonObject } from '@insight/lib/FrontendAgentCommand';
import type { ModuleAgentCommandClient } from '@insight/lib/ModuleAgentCommandClient';
import type { Session } from '@/entity/session';
import {
    workerHoverItem as workerHoverBlock,
    workerSelectItem as workerSelectBlock,
    workerQueryBlocks,
    workerTransform as workerTransformBlock,
} from '@/leaksWorker/blockWorker/worker';
import {
    workerHoverItem as workerHoverState,
    workerSelectItem as workerSelectState,
    workerTransform as workerTransformState,
} from '@/leaksWorker/stateWorker/worker';
import { summarizeMemoryPool, type MemoryPoolSummary } from './memoryPoolSummary';
import { getSnapshotDetail } from '@/utils/RequestUtils';
import { toBoundedDetail } from './boundedDetail';

const DEFAULT_TRANSFORM: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };

let activeSession: Session | undefined;
let memoryPoolSummary: MemoryPoolSummary | undefined;

export const setMemScopeAgentSession = (session: Session | undefined): void => {
    if (activeSession !== session) memoryPoolSummary = undefined;
    activeSession = session;
};

export const updateMemoryPoolSummary = (eventId: number, segments: Segment[]): void => {
    memoryPoolSummary = summarizeMemoryPool(eventId, segments);
};

export const clearMemoryPoolSummary = (): void => {
    memoryPoolSummary = undefined;
};

export const registerMemScopeGraphCommands = (client: ModuleAgentCommandClient): (() => void) => {
    const unregister = [
        client.registerCommand({
            name: 'MemScope.lifecycleGraph.getSummary',
            title: 'Get memory lifecycle graph summary',
            description: 'Returns bounded lifecycle graph metadata and the selected block without returning block paths or full graph data.',
            inputSchema: emptySchema(),
        }, lifecycleSummary),
        client.registerCommand({
            name: 'MemScope.lifecycleGraph.selectBlock',
            title: 'Select a block in the lifecycle graph',
            description: 'Locates and selects one memory block by its numeric block ID.',
            inputSchema: objectSchema({ blockId: { type: 'integer', minimum: 0 } }, ['blockId']),
        }, (args) => selectBlock(args)),
        client.registerCommand({
            name: 'MemScope.lifecycleGraph.getBlockDetail',
            title: 'Get bounded memory block detail',
            description: 'Returns bounded snapshot detail for one block ID, including limited related events or call stack fields when available.',
            inputSchema: detailSchema('blockId'),
        }, async (args) => getBlockDetail(args)),
        client.registerCommand({
            name: 'MemScope.lifecycleGraph.queryBlocks',
            title: 'Query lifecycle block candidates',
            description: 'Scans lifecycle metadata inside the graph worker and returns at most 20 block summaries ranked by size or lifetime. Block paths are never returned.',
            inputSchema: objectSchema({
                sortBy: { type: 'string', enum: ['size', 'lifetime'] },
                limit: { type: 'integer', minimum: 1, maximum: 20, default: 10 },
                address: { type: 'string' },
                minSize: { type: 'number', minimum: 0 },
                minLifetime: { type: 'number', minimum: 0 },
            }, ['sortBy']),
        }, async (args) => queryBlocks(args)),
        client.registerCommand({
            name: 'MemScope.lifecycleGraph.resetView',
            title: 'Reset memory lifecycle graph view',
            description: 'Resets lifecycle graph pan and zoom to the default view.',
            inputSchema: emptySchema(),
        }, resetLifecycleView),
        client.registerCommand({
            name: 'MemScope.memoryPool.getSummary',
            title: 'Get memory pool state summary',
            description: 'Returns aggregate memory pool statistics and at most five largest segments without returning full segment or block arrays.',
            inputSchema: emptySchema(),
        }, poolSummary),
        client.registerCommand({
            name: 'MemScope.memoryPool.selectEvent',
            title: 'Select a memory pool event',
            description: 'Locates an event in the memory pool event list and updates the state diagram.',
            inputSchema: objectSchema({ eventId: { type: 'integer', minimum: 0 } }, ['eventId']),
        }, (args) => selectEvent(args)),
        client.registerCommand({
            name: 'MemScope.memoryPool.getEventDetail',
            title: 'Get bounded memory event detail',
            description: 'Returns bounded snapshot detail for one memory event ID without returning unrestricted nested arrays.',
            inputSchema: detailSchema('eventId'),
        }, async (args) => getEventDetail(args)),
        client.registerCommand({
            name: 'MemScope.memoryPool.getSegmentDetail',
            title: 'Get bounded memory segment detail',
            description: 'Returns bounded detail for one segment at a selected memory state event.',
            inputSchema: objectSchema({
                eventId: { type: 'integer', minimum: 0 },
                address: { type: 'string', minLength: 1 },
                stream: { type: 'integer' },
                maxItems: detailLimitSchema(),
            }, ['eventId', 'address', 'stream']),
        }, async (args) => getSegmentDetail(args)),
        client.registerCommand({
            name: 'MemScope.memoryPool.clearSelection',
            title: 'Clear graph selection',
            description: 'Clears selected lifecycle blocks, memory pool items, and events.',
            inputSchema: emptySchema(),
        }, clearSelection),
        client.registerCommand({
            name: 'MemScope.memoryPool.resetView',
            title: 'Reset memory pool state view',
            description: 'Resets memory pool state graph pan and zoom to the default view.',
            inputSchema: emptySchema(),
        }, resetPoolView),
    ];
    return () => unregister.forEach(stop => stop());
};

const lifecycleSummary = (): JsonObject => {
    if (!activeSession) return { available: false };
    const session = activeSession;
    const info = session.leaksWorkerInfo;
    const selected = info.clickItem;
    const metrics = (globalThis as { __LEAKS_MEMORY_METRICS__?: BlockGraphBuildMetrics }).__LEAKS_MEMORY_METRICS__;
    return {
        available: true,
        ready: info.sizeInfo.maxTimestamp > info.sizeInfo.minTimestamp || session.progressiveTotalEventCount > 0,
        timestampRange: { min: finite(info.sizeInfo.minTimestamp), max: finite(info.sizeInfo.maxTimestamp) },
        sizeRangeBytes: { min: finite(info.sizeInfo.minSize), max: finite(info.sizeInfo.maxSize) },
        reservedSizeMaxBytes: finite(info.sizeInfo.reservedSizeMax),
        build: metrics
            ? {
                blockCount: metrics.blockCount,
                eventCount: metrics.eventCount,
                pathPoints: metrics.pathPoints,
                droppedPathPoints: metrics.droppedPathPoints,
                buildDurationMs: metrics.buildDurationMs,
                lodApplied: metrics.lodApplied,
            }
            : null,
        selectedBlock: selected ? blockSummary(selected) : null,
        view: { ...info.renderOptions.transform },
    };
};

const poolSummary = (): JsonObject => {
    if (!activeSession) return { available: false };
    const session = activeSession;
    const selected = session.stateWorkerInfo.clickItem;
    return {
        available: true,
        selectedEventId: session.stateWorkerInfo.eventId >= 0 ? session.stateWorkerInfo.eventId : null,
        aggregate: memoryPoolSummary ?? null,
        selectedItem: selected
            ? {
                type: selected.type,
                segment: segmentSummary(selected.data),
            }
            : null,
        selectedEvent: session.clickEventItem
            ? {
                id: finite(session.clickEventItem.id),
                action: String(session.clickEventItem.action ?? ''),
                address: String(session.clickEventItem.address ?? ''),
                stream: finite(session.clickEventItem.stream),
                sizeBytes: finite(session.clickEventItem.size),
            }
            : null,
        view: { ...session.stateWorkerInfo.renderOptions.transform },
    };
};

const selectBlock = (args: JsonObject): JsonObject => {
    const session = requireSession();
    const blockId = integerArg(args.blockId, 'blockId');
    runInAction(() => { session.pendingBlockLocateId = blockId; });
    return { accepted: true, blockId };
};

const selectEvent = (args: JsonObject): JsonObject => {
    const session = requireSession();
    const eventId = integerArg(args.eventId, 'eventId');
    if (!session.deviceId) throw unavailable('No memory device is currently selected.');
    runInAction(() => { session.pendingEventLocate = { eventId, deviceId: session.deviceId }; });
    return { accepted: true, eventId, deviceId: session.deviceId };
};

const getBlockDetail = async (args: JsonObject): Promise<JsonObject> => {
    const session = requireSnapshotSession();
    const blockId = integerArg(args.blockId, 'blockId');
    const detail = await getSnapshotDetail({ type: 'block', id: blockId, deviceId: session.deviceId });
    return boundedDetailResult(detail, maxItemsArg(args.maxItems));
};

const queryBlocks = async (args: JsonObject): Promise<JsonObject> => {
    requireSession();
    const sortBy = args.sortBy;
    if (sortBy !== 'size' && sortBy !== 'lifetime') throw invalid('sortBy must be size or lifetime.');
    const limit = optionalIntegerArg(args.limit, 'limit', 10, 1, 20);
    const address = args.address === undefined ? undefined : stringArg(args.address, 'address');
    const minSize = optionalNonNegativeNumber(args.minSize, 'minSize');
    const minLifetime = optionalNonNegativeNumber(args.minLifetime, 'minLifetime');
    const result = await workerQueryBlocks({ sortBy, limit, address, minSize, minLifetime });
    return result as unknown as JsonObject;
};

const getEventDetail = async (args: JsonObject): Promise<JsonObject> => {
    const session = requireSnapshotSession();
    const eventId = integerArg(args.eventId, 'eventId');
    const detail = await getSnapshotDetail({ type: 'event', id: eventId, deviceId: session.deviceId });
    return boundedDetailResult(detail, maxItemsArg(args.maxItems));
};

const getSegmentDetail = async (args: JsonObject): Promise<JsonObject> => {
    const session = requireSnapshotSession();
    const eventId = integerArg(args.eventId, 'eventId');
    const address = stringArg(args.address, 'address');
    const stream = signedIntegerArg(args.stream, 'stream');
    const detail = await getSnapshotDetail({
        type: 'segment',
        id: -1,
        deviceId: session.deviceId,
        eventId,
        segmentAddress: address,
        stream,
    });
    return boundedDetailResult(detail, maxItemsArg(args.maxItems));
};

const clearSelection = (): JsonObject => {
    const session = requireSession();
    const selectionVersion = session.selectionVersion + 1;
    workerSelectBlock({ item: null, selectionVersion });
    workerSelectState({ item: null, selectionVersion });
    runInAction(() => {
        session.selectionVersion = selectionVersion;
        session.leaksWorkerInfo.clickItem = null;
        session.stateWorkerInfo.clickItem = null;
        session.clickEventItem = null;
    });
    return { cleared: true };
};

const resetLifecycleView = (): JsonObject => {
    const session = requireSession();
    runInAction(() => { session.leaksWorkerInfo.renderOptions.transform = { ...DEFAULT_TRANSFORM }; });
    workerTransformBlock({ transform: { ...DEFAULT_TRANSFORM } });
    workerHoverBlock({ clientX: -1, clientY: -1 });
    document.querySelector('[data-testid="blockDiagramPanel"]')?.scrollIntoView({ behavior: 'smooth', block: 'start' });
    return { reset: true };
};

const resetPoolView = (): JsonObject => {
    const session = requireSession();
    runInAction(() => { session.stateWorkerInfo.renderOptions.transform = { ...DEFAULT_TRANSFORM }; });
    workerTransformState({ transform: { ...DEFAULT_TRANSFORM } });
    workerHoverState({ clientX: -1, clientY: -1 });
    document.querySelector('[data-testid="stateDiagramPanel"]')?.scrollIntoView({ behavior: 'smooth', block: 'start' });
    return { reset: true };
};

const blockSummary = (block: Block): JsonObject => ({
    id: block.id,
    address: block.addr,
    startTimestamp: block._startTimestamp,
    endTimestamp: block._endTimestamp,
    lifetime: Math.max(0, block._endTimestamp - block._startTimestamp),
    sizeBytes: block.size,
});

const segmentSummary = (segment: Segment): JsonObject => ({
    address: segment.address,
    stream: segment.stream,
    capacityBytes: segment.size,
    blockCount: segment.blocks.length,
    usedBytes: segment.blocks.reduce((total, block) => total + finite(block.size), 0),
    allocOrMapEventId: segment.allocOrMapEventId,
    eventId: segment.eventId ?? null,
});

const requireSession = (): Session => {
    if (!activeSession) throw unavailable('MemScope has no active analysis session.');
    return activeSession;
};
const requireSnapshotSession = (): Session => {
    const session = requireSession();
    if (session.module === 'leaks') throw unavailable('Snapshot detail is unavailable for the current live leaks session.');
    if (!session.deviceId) throw unavailable('No memory device is currently selected.');
    return session;
};
const integerArg = (value: unknown, name: string): number => {
    const number = Number(value);
    if (!Number.isInteger(number) || number < 0) throw invalid(`${name} must be a non-negative integer.`);
    return number;
};
const signedIntegerArg = (value: unknown, name: string): number => {
    const number = Number(value);
    if (!Number.isInteger(number)) throw invalid(`${name} must be an integer.`);
    return number;
};
const stringArg = (value: unknown, name: string): string => {
    if (typeof value !== 'string' || !value.trim()) throw invalid(`${name} must be a non-empty string.`);
    return value.trim();
};
const maxItemsArg = (value: unknown): number => optionalIntegerArg(value, 'maxItems', 20, 1, 50);
const optionalIntegerArg = (
    value: unknown,
    name: string,
    fallback: number,
    minimum: number,
    maximum: number,
): number => {
    if (value === undefined) return fallback;
    const number = Number(value);
    if (!Number.isInteger(number) || number < minimum || number > maximum) {
        throw invalid(`${name} must be an integer between ${minimum} and ${maximum}.`);
    }
    return number;
};
const optionalNonNegativeNumber = (value: unknown, name: string): number | undefined => {
    if (value === undefined) return undefined;
    const number = Number(value);
    if (!Number.isFinite(number) || number < 0) throw invalid(`${name} must be a non-negative number.`);
    return number;
};
const boundedDetailResult = (detail: unknown, maxItems: number): JsonObject => ({
    detail: toBoundedDetail(detail, { maxArrayItems: maxItems }),
    limits: { maxArrayItems: maxItems, maxDepth: 4, maxObjectKeys: 50, maxStringLength: 2000 },
});
const finite = (value: unknown): number => Number.isFinite(Number(value)) ? Number(value) : 0;
const emptySchema = (): JsonObject => objectSchema({}, []);
const detailSchema = (idName: string): JsonObject => objectSchema({
    [idName]: { type: 'integer', minimum: 0 },
    maxItems: detailLimitSchema(),
}, [idName]);
const detailLimitSchema = (): JsonObject => ({ type: 'integer', minimum: 1, maximum: 50, default: 20 });
const objectSchema = (properties: JsonObject, required: string[]): JsonObject => ({
    type: 'object', properties, required, additionalProperties: false,
});
const invalid = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.INVALID, message, retryable: false,
});
const unavailable = (message: string): CommandError => new CommandError({
    code: COMMAND_ERROR_CODES.UNAVAILABLE, message, retryable: true,
});
