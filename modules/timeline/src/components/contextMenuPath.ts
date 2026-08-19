/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

export function activateMenuAtDepth(path: string[], depth: number, menuName: string): string[] {
    return [...path.slice(0, depth), menuName];
}

export function truncateMenuPath(path: string[], depth: number): string[] {
    return path.slice(0, depth);
}
