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

import { getColorStringByAddr, getDimmedColorStringByAddr } from '@/leaksWorker/tools/color';

const RESERVED_LINE_COLOR = '#0052D9';
const RESERVED_LABEL_COLOR = '#003CAB';

export class Painter {
    readonly canvas: HTMLCanvasElement;
    readonly devicePixelRatio: number;
    private context: CanvasRenderingContext2D | null = null;
    private data: RenderData['blocks'] = [];
    private highlightData: RenderData['blocks'] = [];
    private dimBase: boolean = false;
    private reservedLine: Array<[number, number]> = [];

    constructor(canvas: HTMLCanvasElement, devicePixelRatio: number) {
        this.canvas = canvas;
        this.devicePixelRatio = devicePixelRatio;
    }

    async initialize(): Promise<void> {
        this.context = this.canvas.getContext('2d');
    }

    processData(data: RenderData['blocks'] = [], reservedLine: Array<[number, number]> = []): void {
        this.data = data;
        this.reservedLine = reservedLine;
    }

    processHighlightData(highlightData: RenderData['blocks'] = []): void {
        this.highlightData = highlightData;
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
        this.context.translate(transform.x, viewport.height - transform.y);
        this.context.scale(this.getScaleX(transform), -this.getScaleY(transform));
        this.context.save();
        this.renderData(this.data, options, false, this.dimBase);
        this.renderReservedLine(options);
        this.renderData(this.highlightData, options);
        this.renderData(this.highlightData, options, true);
    }

    renderReservedLine(options: RenderOptions): void {
        if (this.context === null || this.reservedLine.length < 2) {
            return;
        }
        const { zoom } = options;
        const context = this.context;
        context.beginPath();
        context.strokeStyle = RESERVED_LINE_COLOR;
        context.lineWidth = 2 / Math.max(this.getScaleX(options.transform), this.getScaleY(options.transform));
        this.reservedLine.forEach(([timestamp, reservedSize], index) => {
            const x = (timestamp - zoom.offset) * zoom.x;
            const y = reservedSize * zoom.y;
            if (index === 0) {
                context.moveTo(x, y);
            } else {
                context.lineTo(x, y);
            }
        });
        context.stroke();
        const lastPoint = this.reservedLine[this.reservedLine.length - 1];
        context.save();
        context.scale(1, -1);
        context.fillStyle = RESERVED_LABEL_COLOR;
        context.font = `${12 / this.getScaleY(options.transform)}px sans-serif`;
        context.fillText('Reserved', (lastPoint[0] - zoom.offset) * zoom.x, -lastPoint[1] * zoom.y);
        context.restore();
    }

    renderData(data: RenderData['blocks'], options: RenderOptions, isHighlight: boolean = false, dimBase: boolean = false): void {
        for (let i = 0; i < data.length; i++) {
            const { path, size, addr } = data[i];
            if (isHighlight) {
                this.drawBlockOutline(path, size, addr, options);
                continue;
            }
            for (let j = 0; j < path.length - 1; j++) {
                this.drawShape(path[j], path[j + 1], size, addr, options, dimBase);
            }
        }
    }

    drawBlockOutline(path: Array<[number, number]>, size: number, addr: string, options: RenderOptions): void {
        if (this.context === null || path.length < 1) {
            return;
        }
        const { zoom } = options;
        const toX = (point: [number, number]): number => (point[0] - zoom.offset) * zoom.x;
        const toY = (point: [number, number]): number => point[1] * zoom.y;

        this.context.beginPath();
        this.context.moveTo(toX(path[0]), toY(path[0]));
        for (let i = 1; i < path.length; i++) {
            this.context.lineTo(toX(path[i]), toY(path[i]));
        }
        for (let i = path.length - 1; i >= 0; i--) {
            this.context.lineTo(toX(path[i]), (path[i][1] + size) * zoom.y);
        }
        this.context.closePath();
        this.context.strokeStyle = getColorStringByAddr(addr, true);
        this.context.lineWidth = 2 / Math.max(this.getScaleX(options.transform), this.getScaleY(options.transform));
        this.context.stroke();
    }

    drawShape(p0: [number, number], p1: [number, number], size: number, addr: string, options: RenderOptions, dimBase: boolean): void {
        if (this.context === null) {
            return;
        }
        const { zoom } = options;

        const lx = (p0[0] - zoom.offset) * zoom.x;
        const ly = p0[1] * zoom.y;
        const minWidth = 1 / this.getScaleX(options.transform);
        const rx = Math.max((p1[0] - zoom.offset) * zoom.x, lx + minWidth);
        const ry = p1[1] * zoom.y;
        const h = size * zoom.y;

        this.context.beginPath();
        this.context.moveTo(lx, ly);
        this.context.lineTo(lx, ly + h);
        this.context.lineTo(rx, ry + h);
        this.context.lineTo(rx, ry);
        this.context.closePath();

        this.context.fillStyle = dimBase ? getDimmedColorStringByAddr(addr) : getColorStringByAddr(addr);
        this.context.fill();
    }
}
