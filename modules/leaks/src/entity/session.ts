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
import { makeAutoObservable } from 'mobx';
import { type BlocksTableData, EvenItem, EventsTableData, ThreShold } from '../utils/RequestUtils';
import { type MenuItemModel } from '../components/ContextMenu';
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

export const LEAKS_WORKER_INFO_DEFAULT = {
    sizeInfo: {
        maxTimestamp: 0,
        minTimestamp: 0,
        maxSize: 0,
        minSize: 0,
    },
    renderOptions: {
        transform: {
            x: 0,
            y: 0,
            scaleX: 1,
            scaleY: 1,
        },
        viewport: {
            width: 0,
            height: 0,
        },
        zoom: {
            x: 1,
            y: 1,
            offset: 0,
        },
    },
    mousePosition: {
        x: -1,
        y: -1,
    },
    hoverItem: null,
    clickItem: null,
};

export const MARK_LINE_POSITION_DEFAULT = {
    block: { x: -1, y: -1 },
    stack: { x: -1, y: -1 },
    currentTimestamp: -1,
};

export const STATE_WORKER_INFO_DEFAULT = {
    renderOptions: {
        transform: { x: 0, y: 0, scaleX: 1, scaleY: 1 },
        viewport: { width: 0, height: 0 },
        zoom: { x: 1, y: 1, offset: 0 },
    },
    hoverItem: null,
    clickItem: null,
    eventId: -1,
};

export class Session {
    module: 'memsnapshot' | 'leaks' = 'leaks';
    language: 'zhCN' | 'enUS' = 'enUS';
    deviceIds: any = {};
    threadIds: number[] = [];
    blockData: any = { blocks: [], minSize: 0, maxSize: 0, minTimestamp: 0, maxTimestamp: 0 }; // 内存块图数据源
    allocationData: any = { allocations: [], maxTimestamp: 0, minTimestamp: 0 };// 内存折线图数据源
    memoryData: any = { size: 0, name: '', subNodes: [] };// 内存拆解图数据源
    funcData: any = { traces: [], maxTimestamp: 0, minTimestamp: 0 };// 调用栈图数据源
    memoryStamp: number = 0;
    deviceId: string = '';
    eventType: string = '';
    threadId: number | string = '';
    maxTime: number = 0; // x轴最大值
    minTime: number = 0; // x轴最小值
    deviceIdOpts: TypeOption[] = [];
    typeOpts: TypeOption[] = [];
    threadOps: TypeOption[] = [];
    legendSelect: any = {}; // 选择的图例
    synStartTime: number = 0;
    synEndTime: number = 0;
    searchFunc: string[] = [];
    funcOptions: any = [];
    threadFlag: boolean = false; // 判断是否由双击事件触发的thread的变化
    maxDepth: number = 0; // 火焰图最大深度
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
    lazyUsedThreshold: ThreShold = { perT: null, valueT: null };
    delayedFreeThreshold: ThreShold = { perT: null, valueT: null };
    longIdleThreshold: ThreShold = { perT: null, valueT: null };
    onlyInefficient: boolean = false;
    allowTrim: boolean = true;

    leaksWorkerInfo: {
        sizeInfo: Omit<RenderData, 'blocks'>;
        renderOptions: RenderOptions;
        hoverItem: Block | null;
        clickItem: Block | null;
    } = LEAKS_WORKER_INFO_DEFAULT;

    markLineInfo: {
        block: { x: number; y: number };
        stack: { x: number; y: number };
        currentTimestamp: number;
    } = MARK_LINE_POSITION_DEFAULT;

    stateWorkerInfo: {
        renderOptions: RenderOptions;
        hoverItem: StateDataHoverResult | null;
        clickItem: StateDataHoverResult | null;
        eventId: number;
    } = { ...STATE_WORKER_INFO_DEFAULT };

    clickEventItem: EvenItem | null = null;
    pendingEventLocate: { eventId: number; deviceId: string } | null = null;
    pendingBlockLocateId: number | null = null;
    selectionVersion: number = 0;
    leakStats: { totalSize: number; maxSize: number; minSize: number; loading: boolean; error: boolean; requestId: number } = {
        totalSize: 0,
        maxSize: 0,
        minSize: 0,
        loading: false,
        error: false,
        requestId: 0,
    };

    autoFilterPotentialLeaks: boolean = false;

    loadingBlocks: boolean = false;
    loadingFunc: boolean = false;
    loadingState: boolean = false;
    memSnapshotParseLoading: boolean = false;
    memSnapshotParseProgress: number = 0;
    memSnapshotParseFileId: string = '';

    constructor() {
        makeAutoObservable(this);
    }
}
