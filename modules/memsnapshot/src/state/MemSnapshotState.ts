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
import { STATE_WORKER_INFO_DEFAULT } from './defaults';
import { makeAutoObservable, observable } from 'mobx';
import type { AllocationData } from '../common/shared/api/types';
import type { EvenItem } from '../api/types';

export interface MemSnapshotSliceInfo {
    index: number;
    startEventId: number;
    endEventId: number;
    ready: boolean;
}

export interface MemSnapshotDeviceSliceInfo {
    eventCount: number;
    sliceCount: number;
    readySlices: number[];
    slices: MemSnapshotSliceInfo[];
}

export class MemSnapshotState {
    stateWorkerInfo: {
        renderOptions: RenderOptions;
        hoverItem: StateDataHoverResult | null;
        clickItem: StateDataHoverResult | null;
        eventId: number;
    } = {
            ...STATE_WORKER_INFO_DEFAULT,
            renderOptions: {
                transform: { ...STATE_WORKER_INFO_DEFAULT.renderOptions.transform },
                viewport: { ...STATE_WORKER_INFO_DEFAULT.renderOptions.viewport },
                zoom: { ...STATE_WORKER_INFO_DEFAULT.renderOptions.zoom },
            },
        };

    clickEventItem: EvenItem | null = null;
    pendingEventLocate: { eventId: number; deviceId: string } | null = null;
    leakStats: { totalSize: number; maxSize: number; minSize: number; loading: boolean; error: boolean; requestId: number } = {
        totalSize: 0,
        maxSize: 0,
        minSize: 0,
        loading: false,
        error: false,
        requestId: 0,
    };

    autoFilterPotentialLeaks: boolean = false;
    loadingState: boolean = false;
    memSnapshotParseLoading: boolean = false;
    memSnapshotParseProgress: number = 0;
    memSnapshotParseFileId: string = '';
    snapshotParsingComplete: boolean = true;
    memSnapshotCacheRefreshPending: boolean = false;
    snapshotSlices: Record<string, MemSnapshotDeviceSliceInfo> = {};
    selectedSliceIndex: number = -1;
    sliceOverviewData: Record<string, Record<number, AllocationData>> = {};
    snapshotGlobalMaxSizes: Record<string, Record<string, number>> = {};

    constructor() {
        // Overview payloads stay immutable and retain their original array identity.
        makeAutoObservable(this, { sliceOverviewData: observable.ref });
    }
}
