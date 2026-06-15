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
export type TimeStamp = number;

export const level = Symbol('unitLevel');
export const pinnedLevel = Symbol('pinnedUnitLevel');

export type TreeNode<T> = T & {
    children?: Array<TreeNode<T>>;
    [level]?: number;
    [pinnedLevel]?: number;
};

export interface PreOrderFlattenOptions<T> {
    // nodes that should always be flattened, but not appearing in the result, and not counted as one level
    bypass?: (node: TreeNode<T>) => boolean;

    // nodes that are to be flattened normally
    when?: (node: TreeNode<T>) => boolean;

    // nodes that are to be flattened but should not appear in the result
    exclude?: (node: TreeNode<T>) => boolean;
    excludeEx?: (node: TreeNode<T>) => boolean;
}

type LevelKey = typeof level | typeof pinnedLevel;

function preOrderFlattenWithLevel<T>(tree: Array<TreeNode<T>>, currentLevel: number, levelKey: LevelKey, options?: PreOrderFlattenOptions<T>): T[] {
    tree.forEach(node => {
        (node as any)[levelKey] = currentLevel;
    });
    return tree.flatMap(node => {
        if (options?.bypass?.(node) ?? false) {
            return node.children ? [...preOrderFlattenWithLevel(node.children, currentLevel, levelKey, options)] : [];
        } else {
            const self = (options?.exclude?.(node) ?? false) ? [] : [node];
            const isFlat = (self.length > 0 && (options?.when?.(node) ?? true));
            if (isFlat && node.children) {
                const nextLevel = ((node as any)[levelKey] as number) + 1;
                return [...self, ...preOrderFlattenWithLevel(node.children, nextLevel, levelKey, options)];
            } else {
                return self;
            }
        }
    });
}

export function preOrderFlatten<T>(tree: Array<TreeNode<T>>, currentLevel: number, options?: PreOrderFlattenOptions<T>): T[] {
    return preOrderFlattenWithLevel(tree, currentLevel, level, options);
}

export function preOrderPinnedFlatten<T>(tree: Array<TreeNode<T>>, currentLevel: number, options?: PreOrderFlattenOptions<T>): T[] {
    const newTree = tree.filter(item => !options?.excludeEx?.(item));
    return preOrderFlattenWithLevel(newTree, currentLevel, pinnedLevel, options);
}

export type ElementType<T> = T extends Array<infer U> ? U : never;

export type AtomicObjectElementType<T> = T extends Array<infer U> ? U extends number | string | boolean ? T : AtomicObjectElementType<U> : T;

export type KeysMatching<T extends object, U> = {
    [K in keyof T]-?: T[K] extends U ? K : never
}[keyof T];
