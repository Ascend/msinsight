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
