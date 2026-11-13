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
import type { FrameTreeNodeDTO, FrameTreePageRequest, FrameTreeQuery, NodeKind } from '../../frameTree/types';

export const frameTreeLeaf = (nodeId: string, parentId: string | null, bytes: string | bigint, depth = 1): FrameTreeNodeDTO => ({
    nodeId,
    parentId,
    depth,
    kind: 'frame',
    frame: { filename: `${nodeId}.py`, line: 1, name: nodeId },
    sizeBytes: String(bytes),
    selfBytes: String(bytes),
    requestedBytes: String(bytes),
    selfRequestedBytes: String(bytes),
    blockCount: '1',
    selfBlockCount: '1',
    childCount: 0,
});

function buildMockNodes(query: FrameTreeQuery): FrameTreeNodeDTO[] {
    const nodes: FrameTreeNodeDTO[] = [];
    const add = (id: string, parent: FrameTreeNodeDTO | null, kind: NodeKind, name: string, self = 0n): FrameTreeNodeDTO => {
        const node: FrameTreeNodeDTO = {
            nodeId: id,
            parentId: parent?.nodeId ?? null,
            depth: parent ? parent.depth + 1 : 0,
            kind,
            frame: kind === 'frame' ? { filename: `/workspace/model/${name.split('.')[0]}.py`, line: nodes.length % 240 + 1, name } : null,
            sizeBytes: self.toString(),
            selfBytes: self.toString(),
            requestedBytes: (self * 7n / 8n).toString(),
            selfRequestedBytes: (self * 7n / 8n).toString(),
            blockCount: self > 0n ? '1' : '0',
            selfBlockCount: self > 0n ? '1' : '0',
            childCount: 0,
        };
        nodes.push(node);
        if (parent) parent.childCount++;
        return node;
    };
    const root = add('snapshot-root', null, 'root', '');
    const scale = BigInt(1 + query.eventId % 7);
    const branches = ['train_step', 'forward', 'backward', 'optimizer_step', 'data_loader', 'checkpoint'];
    branches.forEach((name, branch) => {
        const parent = add(`branch-${branch}`, root, 'frame', name, BigInt(branch + 1) * 8192n * scale);
        for (let group = 0; group < 10; group++) {
            const stage = add(`b${branch}-g${group}`, parent, 'frame', `layer_${group}.forward`, 4096n * scale);
            for (let index = 0; index < 18; index++) {
                const bytes = BigInt((branch + 2) * (group + 1) * (index + 1)) * 16384n * scale;
                add(`b${branch}-g${group}-f${index}`, stage, 'frame', index % 2 ? 'allocate_tensor' : 'linear', bytes);
            }
        }
    });
    let deep = nodes[1];
    for (let level = 0; level < 18; level++) deep = add(`deep-${level}`, deep, 'frame', `recursive_${level}`);
    deep.selfBytes = (1048576n * scale).toString();
    deep.selfRequestedBytes = (1000000n * scale).toString();
    deep.selfBlockCount = '1';
    if (query.includeStatic) {
        add('static', root, 'static', '', 33554432n);
        add('preexisting', root, 'preexisting', '', 16777216n);
    }
    add('empty', root, 'emptyStack', '', 2097152n);
    add('missing', nodes[1], 'missingFrame', '', 4194304n);
    const zero = add('zero-byte-block', root, 'frame', 'zero_byte_tensor');
    zero.selfBlockCount = '1';
    const byId = new Map(nodes.map(node => [node.nodeId, node]));
    const lookup = (id: string): FrameTreeNodeDTO => {
        const node = byId.get(id);
        if (!node) throw new Error('Invalid mock frame tree node');
        return node;
    };
    for (const node of nodes) {
        node.sizeBytes = node.selfBytes;
        node.requestedBytes = node.selfRequestedBytes;
        node.blockCount = node.selfBlockCount;
    }
    for (let index = nodes.length - 1; index >= 0; index--) {
        const node = nodes[index];
        const parent = byId.get(node.parentId ?? '');
        if (!parent) continue;
        for (const metric of ['sizeBytes', 'requestedBytes', 'blockCount'] as const) {
            parent[metric] = (BigInt(parent[metric]) + BigInt(node[metric])).toString();
        }
    }
    const children = new Map<string, string[]>();
    for (const node of nodes) {
        if (node.parentId === null) continue;
        const siblings = children.get(node.parentId) ?? [];
        siblings.push(node.nodeId);
        children.set(node.parentId, siblings);
    }
    for (const siblings of children.values()) {
        siblings.sort((a, b) => {
            const left = BigInt(lookup(a).sizeBytes);
            const right = BigInt(lookup(b).sizeBytes);
            return left === right ? (a < b ? -1 : a > b ? 1 : 0) : left > right ? -1 : 1;
        });
    }
    const result: FrameTreeNodeDTO[] = [];
    const pending = [root.nodeId];
    while (pending.length) {
        const id = pending.pop();
        if (id === undefined) break;
        result.push(lookup(id));
        const next = children.get(id) ?? [];
        for (let index = next.length - 1; index >= 0; index--) pending.push(next[index]);
    }
    return result;
}

export const createMockFrameTreePage: FrameTreePageRequest = async params => {
    if (!params.deviceId || !Number.isSafeInteger(params.eventId) || params.eventId < 0 ||
        typeof params.includeStatic !== 'boolean' || !Number.isSafeInteger(params.currentPage) || params.currentPage < 1 ||
        !Number.isSafeInteger(params.pageSize) || params.pageSize < 1 || params.pageSize > 1000) {
        throw new Error('Invalid frame tree mock request');
    }
    const nodes = buildMockNodes(params);
    const offset = (params.currentPage - 1) * params.pageSize;
    if (offset >= nodes.length) throw new Error('Frame tree page is out of range');
    return { total: nodes.length, nodes: nodes.slice(offset, offset + params.pageSize) };
};
