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

import { type BlocksTableData, EventsTableData } from '../api/types';
import { type MenuItemModel } from '../../components/ContextMenu';
import type { LifecycleMemoryMarker, LifecycleMemoryMarkerSource } from '../../entity/lifecycleMemoryMarkers';
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
export interface MemoryContext {
    module: 'memsnapshot' | 'leaks';
    language: 'zhCN' | 'enUS';
    deviceIds: any;
    blockData: any;
    allocationData: any;
    deviceId: string;
    eventType: string;
    maxTime: number;
    minTime: number;
    deviceIdOpts: TypeOption[];
    typeOpts: TypeOption[];
    legendSelect: any;
    synStartTime: number;
    synEndTime: number;
    tableType: 'blocks' | 'events';
    blocksTableData: BlocksTableData['blocks'];
    eventsTableData: EventsTableData['events'];
    blocksTableHeader: BlocksTableData['headers'];
    eventsTableHeader: EventsTableData['headers'];
    blocksOrder: boolean | string;
    eventsOrder: boolean | string;
    blocksOrderBy: string;
    eventsOrderBy: string;
    blocksFilters: { [key: string]: string };
    blocksRangeFilters: { [key: string]: number[] };
    eventsFilters: { [key: string]: string };
    eventsRangeFilters: { [key: string]: number[] };
    blocksCurrentPage: number;
    eventsCurrentPage: number;
    blocksPageSize: number;
    eventsPageSize: number;
    blocksTotal: number;
    eventsTotal: number;
    tableKey: number;
    firstOffset: number;
    lastOffset: number;
    firstLastStamps: MarkStamps;
    markLineshow: 'none' | 'block';
    allowMark: boolean;
    contextMenu: ContextMenu;
    menuItems: MenuItemModel[];
    leaksWorkerInfo: {
        sizeInfo: Omit<RenderData, 'blocks'>;
        renderOptions: RenderOptions;
        hoverItem: Block | null;
        clickItem: Block | null;
    };
    markLineInfo: {
        block: { x: number; y: number };
        stack: { x: number; y: number };
        currentTimestamp: number;
    };
    pendingBlockLocateId: number | null;
    selectionVersion: number;
    loadingBlocks: boolean;
    loadedMemoryBlockContextKey: string;
    loadingOverview: boolean;
    progressiveBlocksVisible: boolean;
    progressiveRenderedBatchCount: number;
    progressiveRenderedInstanceCount: number;
    progressiveRenderedEventCount: number;
    progressiveTotalEventCount: number;
    progressiveFirstRenderedBatchCount: number;
    progressiveFirstRenderedInstanceCount: number;
    dbPath: string;
    fileHash: string;
    lifecycleMemoryMarkers: Map<string, LifecycleMemoryMarker[]>;
    lifecycleMemoryMarkerOrdinals: Map<string, number>;
    getLifecycleMemoryMarkerContextKey: () => string;
    getLifecycleMemoryMarkers: () => LifecycleMemoryMarker[];
    addLifecycleMemoryMarker: (
        memoryBytes: number,
        id: string,
        source?: LifecycleMemoryMarkerSource,
        color?: string,
        blockId?: number,
    ) => boolean;
    deleteLifecycleMemoryMarker: (id: string) => void;
    updateLifecycleMemoryMarkerColor: (id: string, color: string) => boolean;
    updateLifecycleMemoryMarkerPresentation: (
        id: string,
        updates: { name?: string; hidden?: boolean },
    ) => boolean;
    clearCurrentLifecycleMemoryMarkers: () => void;
    clearLifecycleMemoryMarkers: () => void;
}
