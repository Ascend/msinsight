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
export const FRAME_TREE_COMMAND = 'Memory/snapshot/frameTree';
export const METRIC_KEYS = ['sizeBytes', 'selfBytes', 'requestedBytes', 'selfRequestedBytes', 'blockCount', 'selfBlockCount'] as const;
export type MetricKey = typeof METRIC_KEYS[number];
export type NodeKind = 'root' | 'frame' | 'static' | 'preexisting' | 'emptyStack' | 'missingFrame';
export interface FrameTreeNodeDTO extends Record<MetricKey, string> {
    nodeId: string;
    parentId: string | null;
    depth: number;
    kind: NodeKind;
    frame: { filename: string; line: number | null; name: string } | null;
    childCount: number;
}
export interface FrameTreeQuery {
    deviceId: string;
    eventId: number;
    includeStatic: boolean;
    pageSize: number;
}
export interface FrameTreePage {
    total: number;
    nodes: FrameTreeNodeDTO[];
}
export type FrameTreePageRequest = (
    params: Readonly<FrameTreeQuery & { currentPage: number }>,
    signal?: AbortSignal,
) => Promise<FrameTreePage>;
export interface FrameTreeNode {
    // Unknown is a frontend grouping, not an additional transport node kind.
    dto: Omit<FrameTreeNodeDTO, 'kind'> & { kind: NodeKind | 'unknown' };
    metrics: Record<MetricKey, bigint>;
    searchText: string;
    merged?: { ids: readonly string[]; name: string };
}
export interface FrameTree {
    rootId: string;
    nodes: Map<string, FrameTreeNode>;
    children: Map<string, string[]>;
}
export interface FrameTreeCell {
    nodeId: string;
    x: number;
    y: number;
    width: number;
    height: number;
    level: number;
    self: boolean;
    truncated: boolean;
}
