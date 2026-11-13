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
import { childIds, compareNodes, nodeAt } from './model';
import type { FrameTree } from './types';

export function filterPythonFrames(source: FrameTree): FrameTree {
    const view: FrameTree = { rootId: source.rootId, nodes: new Map(), children: new Map() };
    const pending = [{ id: source.rootId, parent: null as string | null, depth: 0 }];
    while (pending.length) {
        const entry = pending.pop();
        if (!entry) break;
        const original = nodeAt(source, entry.id);
        const keep = original.dto.kind !== 'frame' || original.dto.frame?.filename.endsWith('.py');
        let parent = entry.parent;
        let depth = entry.depth;
        if (keep) {
            view.nodes.set(entry.id, {
                ...original,
                dto: { ...original.dto, parentId: parent, depth },
                metrics: { ...original.metrics },
            });
            view.children.set(entry.id, []);
            if (parent !== null) childIds(view, parent).push(entry.id);
            parent = entry.id;
            depth++;
        } else if (parent !== null) {
            // Only self allocations move upward: inclusive bytes would double-count retained descendants.
            const ancestor = nodeAt(view, parent);
            for (const key of ['selfBytes', 'selfRequestedBytes', 'selfBlockCount'] as const) ancestor.metrics[key] += original.metrics[key];
        }
        const children = childIds(source, entry.id);
        for (let index = children.length - 1; index >= 0; index--) pending.push({ id: children[index], parent, depth });
    }
    for (const [id, node] of view.nodes) {
        const children = childIds(view, id);
        children.sort((a, b) => Number(['static', 'unknown'].includes(nodeAt(view, a).dto.kind)) -
            Number(['static', 'unknown'].includes(nodeAt(view, b).dto.kind)) ||
            compareNodes(view, a, b));
        node.dto.childCount = children.length;
        for (const key of ['selfBytes', 'selfRequestedBytes', 'selfBlockCount'] as const) node.dto[key] = node.metrics[key].toString();
    }
    return view;
}
