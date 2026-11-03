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
import { LEAKS_WORKER_INFO_DEFAULT, MARK_LINE_POSITION_DEFAULT } from '../shared/state/defaults';
import { type BlocksTableData, EventsTableData } from '../shared/api/types';
import { type MenuItemModel } from '../components/ContextMenu';
import {
    addLifecycleMemoryMarker,
    deleteLifecycleMemoryMarker,
    getLifecycleMemoryMarkerContextKey,
    updateLifecycleMemoryMarkerColor,
    updateLifecycleMemoryMarkerPresentation,
    type LifecycleMemoryMarker,
    type LifecycleMemoryMarkerSource,
} from './lifecycleMemoryMarkers';
export { LEAKS_WORKER_INFO_DEFAULT, MARK_LINE_POSITION_DEFAULT } from '../shared/state/defaults';
interface TypeOption {
    label: string | number;
    value: string | number;
}
interface ContextMenu {
    visible: boolean;
    xPos: number;
    yPos: number;
}
interface MarkStamps {
    first: number;
    last: number;
}

export class MemorySession {
    module: 'memsnapshot' | 'leaks' = 'leaks';
    language: 'zhCN' | 'enUS' = 'enUS';
    deviceIds: any = {};
    blockData: any = { blocks: [], minSize: 0, maxSize: 0, minTimestamp: 0, maxTimestamp: 0 }; // 内存块图数据源
    allocationData: any = { allocations: [], maxTimestamp: 0, minTimestamp: 0 };// 内存折线图数据源
    deviceId: string = '';
    eventType: string = '';
    maxTime: number = 0; // x轴最大值
    minTime: number = 0; // x轴最小值
    deviceIdOpts: TypeOption[] = [];
    typeOpts: TypeOption[] = [];
    legendSelect: any = {}; // 选择的图例
    synStartTime: number = 0;
    synEndTime: number = 0;
    tableType: 'blocks' | 'events' = 'blocks';
    blocksTableData: BlocksTableData['blocks'] = []; // 内存详情表blocks数据源
    eventsTableData: EventsTableData['events'] = []; // 内存详情表events数据源
    blocksTableHeader: BlocksTableData['headers'] = [];
    eventsTableHeader: EventsTableData['headers'] = [];
    blocksOrder: boolean | string = '';
    eventsOrder: boolean | string = '';
    blocksOrderBy: string = '';
    eventsOrderBy: string = '';
    blocksFilters: { [key: string]: string } = {};
    blocksRangeFilters: { [key: string]: number[] } = {};
    eventsFilters: { [key: string]: string } = {};
    eventsRangeFilters: { [key: string]: number[] } = {};
    //  底部表格分页相关变量
    blocksCurrentPage: number = 1;
    eventsCurrentPage: number = 1;
    blocksPageSize: number = 10;
    eventsPageSize: number = 10;
    blocksTotal: number = 0;
    eventsTotal: number = 0;
    tableKey: number = 0;
    firstOffset: number = 0;
    lastOffset: number = 0;
    firstLastStamps: MarkStamps = { first: 0, last: 0 };
    markLineshow: 'none' | 'block' = 'none';
    allowMark: boolean = true;
    contextMenu: ContextMenu = { visible: false, xPos: 0, yPos: 0 };
    menuItems: MenuItemModel[] = [];

    leaksWorkerInfo: {
        sizeInfo: Omit<RenderData, 'blocks'>;
        renderOptions: RenderOptions;
        hoverItem: Block | null;
        clickItem: Block | null;
    } = {
            ...LEAKS_WORKER_INFO_DEFAULT,
            sizeInfo: { ...LEAKS_WORKER_INFO_DEFAULT.sizeInfo },
            renderOptions: {
                transform: { ...LEAKS_WORKER_INFO_DEFAULT.renderOptions.transform },
                viewport: { ...LEAKS_WORKER_INFO_DEFAULT.renderOptions.viewport },
                zoom: { ...LEAKS_WORKER_INFO_DEFAULT.renderOptions.zoom },
            },
        };

    markLineInfo: {
        block: { x: number; y: number };
        stack: { x: number; y: number };
        currentTimestamp: number;
    } = { ...MARK_LINE_POSITION_DEFAULT, block: { ...MARK_LINE_POSITION_DEFAULT.block }, stack: { ...MARK_LINE_POSITION_DEFAULT.stack } };

    pendingBlockLocateId: number | null = null;
    selectionVersion: number = 0;

    loadingBlocks: boolean = false;
    loadedMemoryBlockContextKey: string = '';
    loadingOverview: boolean = false;
    progressiveBlocksVisible: boolean = false;
    progressiveRenderedBatchCount: number = 0;
    progressiveRenderedInstanceCount: number = 0;
    progressiveRenderedEventCount: number = 0;
    progressiveTotalEventCount: number = 0;
    progressiveFirstRenderedBatchCount: number = 0;
    progressiveFirstRenderedInstanceCount: number = 0;
    dbPath: string = '';
    fileHash: string = '';
    lifecycleMemoryMarkers: Map<string, LifecycleMemoryMarker[]> = new Map();
    lifecycleMemoryMarkerOrdinals: Map<string, number> = new Map();

    // Computed compatibility fields are not enumerable; retain the notification field contract.
    getUpdateKeys(): string[] { return Object.keys(this); }

    getLifecycleMemoryMarkerContextKey(): string {
        return getLifecycleMemoryMarkerContextKey({
            fileHash: this.fileHash,
            module: this.module,
            deviceId: this.deviceId,
            eventType: this.eventType,
        });
    }

    getLifecycleMemoryMarkers(): LifecycleMemoryMarker[] {
        return this.lifecycleMemoryMarkers.get(this.getLifecycleMemoryMarkerContextKey()) ?? [];
    }

    addLifecycleMemoryMarker(
        memoryBytes: number,
        id: string,
        source: LifecycleMemoryMarkerSource = 'custom',
        color?: string,
        blockId?: number,
    ): boolean {
        const contextKey = this.getLifecycleMemoryMarkerContextKey();
        const currentMarkers = this.lifecycleMemoryMarkers.get(contextKey) ?? [];
        const nextOrdinal = Math.max(
            this.lifecycleMemoryMarkerOrdinals.get(contextKey) ?? 0,
            ...currentMarkers.map(marker => marker.ordinal ?? 0),
        ) + 1;
        const nextMarkers = addLifecycleMemoryMarker(
            currentMarkers,
            memoryBytes,
            id,
            color,
            nextOrdinal,
            source,
            blockId,
        );
        if (nextMarkers === currentMarkers) {
            return false;
        }
        this.lifecycleMemoryMarkers.set(contextKey, nextMarkers);
        this.lifecycleMemoryMarkerOrdinals.set(contextKey, nextOrdinal);
        return true;
    }

    deleteLifecycleMemoryMarker(id: string): void {
        const contextKey = this.getLifecycleMemoryMarkerContextKey();
        const currentMarkers = this.lifecycleMemoryMarkers.get(contextKey) ?? [];
        this.lifecycleMemoryMarkers.set(contextKey, deleteLifecycleMemoryMarker(currentMarkers, id));
    }

    updateLifecycleMemoryMarkerColor(id: string, color: string): boolean {
        const contextKey = this.getLifecycleMemoryMarkerContextKey();
        const currentMarkers = this.lifecycleMemoryMarkers.get(contextKey) ?? [];
        const nextMarkers = updateLifecycleMemoryMarkerColor(currentMarkers, id, color);
        if (nextMarkers === currentMarkers) {
            return false;
        }
        this.lifecycleMemoryMarkers.set(contextKey, nextMarkers);
        return true;
    }

    updateLifecycleMemoryMarkerPresentation(
        id: string,
        updates: { name?: string; hidden?: boolean },
    ): boolean {
        const contextKey = this.getLifecycleMemoryMarkerContextKey();
        const currentMarkers = this.lifecycleMemoryMarkers.get(contextKey) ?? [];
        const nextMarkers = updateLifecycleMemoryMarkerPresentation(currentMarkers, id, updates);
        if (nextMarkers === currentMarkers) return false;
        this.lifecycleMemoryMarkers.set(contextKey, nextMarkers);
        return true;
    }

    clearCurrentLifecycleMemoryMarkers(): void {
        const contextKey = this.getLifecycleMemoryMarkerContextKey();
        this.lifecycleMemoryMarkers.delete(contextKey);
        this.lifecycleMemoryMarkerOrdinals.delete(contextKey);
    }

    clearLifecycleMemoryMarkers(): void {
        this.lifecycleMemoryMarkers.clear();
        this.lifecycleMemoryMarkerOrdinals.clear();
    }
}
