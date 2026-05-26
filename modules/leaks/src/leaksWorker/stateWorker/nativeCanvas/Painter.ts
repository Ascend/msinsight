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

import { getColorStringByIndex, getDimmedColorStringByIndex } from '@/leaksWorker/tools/color';

export class Painter {
    readonly canvas: HTMLCanvasElement;
    readonly devicePixelRatio: number;
    readonly itemHeight = 40;
    private context: CanvasRenderingContext2D | null = null;
    private data: Segment[] = [];
    private highlightData: StateDataHoverResult[] = [];
    private dimBase: boolean = false;

    constructor(canvas: HTMLCanvasElement, devicePixelRatio: number) {
        this.canvas = canvas;
        this.devicePixelRatio = devicePixelRatio;
    }

    async initialize(): Promise<void> {
        this.context = this.canvas.getContext('2d');
    }

    processData(data: Segment[] = []): void {
        this.data = data;
    }

    processHighlightData(highlightData: StateDataHoverResult | StateDataHoverResult[] | null): void {
        this.highlightData = highlightData === null ? [] : Array.isArray(highlightData) ? highlightData : [highlightData];
    }

    setBaseDimmed(dimBase: boolean): void {
        this.dimBase = dimBase;
    }

    private getScaleX(transform: RenderOptions['transform']): number {
        return transform.scaleX;
    }

    private getScaleY(transform: RenderOptions['transform']): number {
        return transform.scaleY;
    }

    render(options: RenderOptions): void {
        if (this.context === null) {
            return;
        }
        const { transform, viewport } = options;
        this.context.resetTransform();
        this.context.clearRect(0, 0, viewport.width, viewport.height);
        this.context.translate(transform.x, transform.y);
        this.context.scale(this.getScaleX(transform), this.getScaleY(transform));
        this.context.save();
        this.renderData(this.data, options, this.dimBase);
        this.renderHighlightFillData(this.highlightData, options);
        this.renderHighlightData(this.highlightData, options);
    }

    renderData(data: Segment[], options: RenderOptions, dimBase: boolean = false): void {
        for (let i = 0; i < data.length; i++) {
            const { size, offsetX, offsetY, blocks } = data[i];
            for (let j = 0; j < blocks.length; j++) {
                const colorIndex = blocks[j].colorIndex ?? j;
                const color = dimBase ? getDimmedColorStringByIndex(colorIndex) : getColorStringByIndex(colorIndex);
                this.drawShape(offsetX + blocks[j].offset, offsetY, blocks[j].size, options, color);
            }
            this.drawShape(offsetX, offsetY, size, options, '#000000', true);
        }
    }

    renderHighlightFillData(highlightData: StateDataHoverResult[], options: RenderOptions): void {
        for (let i = 0; i < highlightData.length; i++) {
            this.renderSingleHighlightFillData(highlightData[i], options);
        }
    }

    renderSingleHighlightFillData(highlightData: StateDataHoverResult, options: RenderOptions): void {
        const { type, data } = highlightData;
        const { offsetX, offsetY, blocks } = data;
        if (type === 'segment') {
            for (let j = 0; j < blocks.length; j++) {
                this.drawShape(offsetX + blocks[j].offset, offsetY, blocks[j].size, options, getColorStringByIndex(j));
            }
            return;
        }
        this.drawShape(offsetX + blocks[0].offset, offsetY, blocks[0].size, options, getColorStringByIndex(blocks[0].colorIndex ?? 0));
    }

    renderHighlightData(highlightData: StateDataHoverResult[], options: RenderOptions): void {
        for (let i = 0; i < highlightData.length; i++) {
            this.renderSingleHighlightData(highlightData[i], options);
        }
    }

    renderSingleHighlightData(highlightData: StateDataHoverResult, options: RenderOptions): void {
        const { type, data } = highlightData;
        const { size, offsetX, offsetY, blocks } = data;
        if (type === 'segment') {
            this.drawShape(offsetX, offsetY, size, options, '#ff0000', true);
        } else {
            this.drawShape(offsetX + blocks[0].offset, offsetY, blocks[0].size, options, '#ff0000', true);
        }
    }

    drawShape(x: number, y: number, size: number, options: RenderOptions, color: string, isBorder: boolean = false): void {
        if (this.context === null) {
            return;
        }
        const { zoom, transform } = options;

        const lx = x * zoom.x;
        const ly = y * zoom.y;
        const w = Math.max(size * zoom.x, 1 / this.getScaleX(transform));
        const h = this.itemHeight * zoom.y;

        if (isBorder) {
            this.context.strokeStyle = color;
            this.context.lineWidth = 2 / Math.max(this.getScaleX(transform), this.getScaleY(transform));
            this.context.strokeRect(lx, ly, w, h);
            return;
        }

        // 绘制四边形
        this.context.beginPath();
        this.context.moveTo(lx, ly);
        this.context.lineTo(lx, ly + h);
        this.context.lineTo(lx + w, ly + h);
        this.context.lineTo(lx + w, ly);
        this.context.closePath();
        this.context.fillStyle = color; // 填充颜色
        this.context.fill(); // 填充图形
    }
}
