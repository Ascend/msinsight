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
import type { MemoryContext } from './MemoryContext';
import { MemorySession } from '../../entity/MemorySession';
const commonRestore = (session: MemoryContext): void => {
    session.maxTime = 0;
    session.minTime = 0;
    session.legendSelect = {};
    session.synStartTime = 0;
    session.synEndTime = 0;
};

const barRestore = (session: MemoryContext): void => {
    session.deviceId = '';
    session.eventType = '';
    session.deviceIdOpts = [];
    session.typeOpts = [];
    session.allocationData = { allocations: [], maxTimestamp: 0, minTimestamp: 0 };
    session.blockData = { blocks: [], minSize: 0, maxSize: 0, minTimestamp: 0, maxTimestamp: 0 };
    session.firstOffset = 0;
    session.lastOffset = 0;
    session.markLineshow = 'none';
    session.contextMenu = { visible: false, xPos: 0, yPos: 0 };
    session.allowMark = true;
    session.menuItems = [];
    session.firstLastStamps = { first: 0, last: 0 };
};

const blocksDetailsRestore = (session: MemoryContext): void => {
    session.blocksTableData = [];
    session.blocksTableHeader = [];
    session.blocksCurrentPage = 1;
    session.blocksPageSize = 10;
    session.blocksTotal = 0;
    session.blocksOrder = '';
    session.blocksOrderBy = '';
    session.blocksFilters = {};
    session.blocksRangeFilters = {};
};

const eventsDetailsRestore = (session: MemoryContext): void => {
    session.eventsRangeFilters = {};
    session.eventsTableData = [];
    session.eventsTableHeader = [];
    session.eventsCurrentPage = 1;
    session.eventsPageSize = 10;
    session.eventsTotal = 0;
    session.eventsOrder = '';
    session.eventsOrderBy = '';
    session.eventsFilters = {};
};

const detailsRestore = (session: MemoryContext): void => {
    session.tableType = 'blocks';
    session.tableKey = (session.tableKey + 1) % 10;
};
export const restoreMemoryCommon = (session: MemoryContext): void => {
    session.clearLifecycleMemoryMarkers();
    commonRestore(session);
    barRestore(session);
    blocksDetailsRestore(session);
    eventsDetailsRestore(session);
    detailsRestore(session);
};

// Removing a dataset also clears renderer/loading metadata beyond the normal view reset.
export const resetMemoryApplication = (session: MemoryContext): void => {
    const defaults = new MemorySession();
    session.deviceIds = {};
    session.dbPath = '';
    session.fileHash = '';
    session.leaksWorkerInfo = defaults.leaksWorkerInfo;
    session.markLineInfo = defaults.markLineInfo;
    session.loadingBlocks = false;
    session.loadingOverview = false;
    restoreMemoryCommon(session);
};
