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
import { FRAME_TREE_COMMAND } from './types';
import type { FrameTreePageRequest } from './types';
import { loadFrameTree } from './loader';

export const requestFrameTreePage: FrameTreePageRequest = params =>
    window.request({ command: FRAME_TREE_COMMAND, params: { ...params } });

// The server caches one tree and cancels older queries; keep every tree's pages together.
let pendingLoad: Promise<unknown> = Promise.resolve();
export const loadFrameTreeSerial: typeof loadFrameTree = (...args) => {
    const result = pendingLoad.then(() => loadFrameTree(...args));
    pendingLoad = result.catch(() => undefined);
    return result;
};
