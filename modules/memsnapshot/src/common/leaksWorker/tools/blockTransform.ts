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

const clamp = (value: number, min: number, max: number): number => Math.min(Math.max(value, min), max);

const getBlockBounds = (block: Block): { minX: number; maxX: number; minY: number; maxY: number } | null => {
    if (block.path.length < 1) {
        return null;
    }
    const startPoint = block.path[0];
    const endPoint = block.path[block.path.length - 1];
    const minY = Math.min(startPoint[1], endPoint[1]);
    const maxY = Math.max(startPoint[1], endPoint[1]) + block.size;
    return {
        minX: startPoint[0],
        maxX: endPoint[0],
        minY,
        maxY,
    };
};

export const getCenteredBlockTransform = (
    block: Block,
    viewport: RenderOptions['viewport'],
    zoom: RenderOptions['zoom'],
): RenderOptions['transform'] | null => {
    const bounds = getBlockBounds(block);
    if (bounds === null) {
        return null;
    }
    const blockWidth = Math.max((bounds.maxX - bounds.minX) * zoom.x, 1);
    const blockHeight = Math.max((bounds.maxY - bounds.minY) * zoom.y, 1);
    const targetWidthScale = viewport.width * 0.35 / blockWidth;
    const targetHeightScale = viewport.height * 0.35 / blockHeight;
    const scale = clamp(Math.min(targetWidthScale, targetHeightScale), 1, 80);
    const centerX = ((bounds.minX + bounds.maxX) / 2 - zoom.offset) * zoom.x;
    const centerY = ((bounds.minY + bounds.maxY) / 2) * zoom.y;
    return {
        x: viewport.width / 2 - centerX * scale,
        y: viewport.height / 2 - centerY * scale,
        scaleX: scale,
        scaleY: scale,
    };
};
