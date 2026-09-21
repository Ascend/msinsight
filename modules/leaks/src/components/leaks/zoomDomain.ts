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
export type InitialZoomDomainInput = {
    blockMinTimestamp: number;
    blockMaxTimestamp: number;
    allocationMinTimestamp: number;
    allocationMaxTimestamp: number;
    funcMinTimestamp: number;
    funcMaxTimestamp: number;
};

export type InitialZoomDomain = {
    minTime: number;
    maxTime: number;
};

export const getInitialZoomDomain = ({
    blockMinTimestamp,
    blockMaxTimestamp,
    allocationMinTimestamp,
    allocationMaxTimestamp,
    funcMinTimestamp,
    funcMaxTimestamp,
}: InitialZoomDomainInput): InitialZoomDomain => {
    const hasBlockRange = blockMaxTimestamp > blockMinTimestamp;
    let minTime = hasBlockRange ? blockMinTimestamp : allocationMinTimestamp;
    let maxTime = Math.max(blockMaxTimestamp, allocationMaxTimestamp);

    if (funcMaxTimestamp > 0) {
        minTime = Math.min(minTime, funcMinTimestamp);
        maxTime = Math.max(maxTime, funcMaxTimestamp);
    }

    return { minTime, maxTime };
};
