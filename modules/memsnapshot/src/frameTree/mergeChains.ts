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
import { childIds, nodeAt } from './model';
import type { FrameTree, FrameTreeNode } from './types';

const shortName = (name = ''): string => {
    const symbol = name.split('(')[0].split('::').pop()?.trim() ?? '';
    return symbol.length > 32 ? symbol.slice(0, 31) + '\u2026' : symbol;
};

export function mergeSingleChildChains(tree: FrameTree, boundaries: ReadonlySet<string> = new Set()): {
    tree: FrameTree; owners: Map<string, string>;
} {
    const view: FrameTree = { rootId: tree.rootId, nodes: new Map(), children: new Map() };
    const owners = new Map<string, string>();
    const pending = [{ id: tree.rootId, parent: null as string | null, depth: 0 }];
    while (pending.length) {
        const entry = pending.pop();
        if (!entry) break;
        const { id, parent, depth } = entry;
        const head = nodeAt(tree, id);
        const metrics = { ...head.metrics };
        const ids = [id];
        let tail = head;
        let children = childIds(tree, id);
        // Focus boundaries remain addressable; synthetic sources never merge with frames.
        while (tail.dto.kind === 'frame' && children.length === 1 && !boundaries.has(children[0])) {
            const child = nodeAt(tree, children[0]);
            if (child.dto.kind !== 'frame') break;
            ids.push(child.dto.nodeId);
            metrics.selfBytes += child.metrics.selfBytes;
            metrics.selfRequestedBytes += child.metrics.selfRequestedBytes;
            metrics.selfBlockCount += child.metrics.selfBlockCount;
            tail = child;
            children = childIds(tree, child.dto.nodeId);
        }
        view.nodes.set(id, {
            ...head,
            metrics,
            dto: {
                ...head.dto,
                parentId: parent,
                depth,
                childCount: children.length,
                selfBytes: metrics.selfBytes.toString(),
                selfRequestedBytes: metrics.selfRequestedBytes.toString(),
                selfBlockCount: metrics.selfBlockCount.toString(),
            },
            ...(ids.length > 1 ? { merged: { ids, name: shortName(tail.dto.frame?.name) } } : {}),
        });
        view.children.set(id, [...children]);
        for (const member of ids) owners.set(member, id);
        for (let index = children.length - 1; index >= 0; index--) {
            pending.push({ id: children[index], parent: id, depth: depth + 1 });
        }
    }
    return { tree: view, owners };
}

export function mergeChainPath(tree: FrameTree, path: readonly string[]): FrameTreeNode[] {
    const result: FrameTreeNode[] = [];
    for (let start = 0; start < path.length;) {
        let end = start;
        while (end + 1 < path.length) {
            const node = nodeAt(tree, path[end]);
            const children = childIds(tree, path[end]);
            if (node.dto.kind !== 'frame' || children.length !== 1 || children[0] !== path[end + 1] ||
                nodeAt(tree, path[end + 1]).dto.kind !== 'frame') break;
            end++;
        }
        const tail = nodeAt(tree, path[end]);
        result.push(end === start
            ? tail
            : {
                ...tail, merged: { ids: path.slice(start, end + 1), name: shortName(tail.dto.frame?.name) },
            });
        start = end + 1;
    }
    return result;
}
