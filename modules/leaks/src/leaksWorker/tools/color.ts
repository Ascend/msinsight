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

export const colors = ['#4e79a7', '#f28e2c', '#e15759', '#76b7b2', '#59a14f', '#edc949', '#af7aa1', '#ff9da7', '#9c755f', '#bab0ab'];
export const highlightColors = ['#8cb3d9', '#f7bc77', '#f08a8c', '#b3dce0', '#9dd68a', '#f5e082', '#e0b3d9', '#ffcdd2', '#d4bfa1', '#e0dbd7'];

const hexToRgba = (hex: string, opacity: number = 1): [number, number, number, number] => {
    if (!hex) { return [0.5, 0.5, 0.5, opacity]; }
    const h = hex.replace('#', '');
    if (h.length !== 6) { return [0.5, 0.5, 0.5, opacity]; }
    const r = Number.parseInt(h.slice(0, 2), 16) / 255;
    const g = Number.parseInt(h.slice(2, 4), 16) / 255;
    const b = Number.parseInt(h.slice(4, 6), 16) / 255;
    if (Number.isNaN(r) || Number.isNaN(g) || Number.isNaN(b)) { return [0.5, 0.5, 0.5, opacity]; }
    return [r, g, b, opacity];
};

const dimHexColor = (hex: string): string => {
    const [r, g, b] = hexToRgba(hex);
    const dimRatio = 0.58;
    const floor = 0.42;
    const dim = (value: number): number => Math.max(floor, value * dimRatio + (1 - dimRatio));
    const toHex = (value: number): string => Math.round(value * 255).toString(16).padStart(2, '0');
    return `#${toHex(dim(r))}${toHex(dim(g))}${toHex(dim(b))}`;
};

export const GL_COLORS: Array<[number, number, number, number]> = colors.map(color => hexToRgba(color));
export const GL_HIGHLIGHT_COLORS: Array<[number, number, number, number]> = highlightColors.map(color => hexToRgba(color));
export const GL_DIMMED_COLORS: Array<[number, number, number, number]> = colors.map(color => hexToRgba(dimHexColor(color)));

const hashString = (str: string): number => {
    let hash = 5381;
    for (let i = 0; i < str.length; i++) {
        hash = ((hash << 5) + hash + str.charCodeAt(i)) | 0;
    }
    return hash >>> 0;
};

export const hashHexAddressToIndex = (addr: string): number => {
    const clean = addr.toLowerCase().replace(/^0x/, '');
    return hashString(clean) % GL_COLORS.length;
};

const hexToRgbaString = (hex: string, opacity: number = 1): string => {
    const [r, g, b, a] = hexToRgba(hex, opacity);
    return `rgba(${Math.round(r * 255)}, ${Math.round(g * 255)}, ${Math.round(b * 255)}, ${a})`;
};

export const getColorStringByAddr = (addr: string, isHighlight: boolean = false, opacity: number = 1): string => {
    if (isHighlight) {
        return opacity >= 1 ? '#000000' : hexToRgbaString('#000000', opacity);
    }
    const index = hashHexAddressToIndex(addr);
    const color = colors[index];
    return opacity >= 1 ? color : hexToRgbaString(color, opacity);
};

export const getDimmedColorStringByAddr = (addr: string, opacity: number = 1): string => {
    const index = hashHexAddressToIndex(addr);
    const color = dimHexColor(colors[index]);
    return opacity >= 1 ? color : hexToRgbaString(color, opacity);
};

export const getColorStringByIndex = (index: number, isHighlight: boolean = false): string => {
    return isHighlight ? highlightColors[index % GL_COLORS.length] : colors[index % GL_COLORS.length];
};

export const getDimmedColorStringByIndex = (index: number): string => {
    return dimHexColor(colors[index % colors.length]);
};
