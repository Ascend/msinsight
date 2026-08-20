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
import { BlockDataOPFS, getPointFromPathData } from '../../tools/BlockDataOPFS';

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
    private blockDataOPFS: BlockDataOPFS | null = null;
    private batchCount: number = 0;
    private sourceReadBuffer: Float32Array = new Float32Array(0);

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
        this.blockDataOPFS = null;
        this.batchCount = 0;
    }

    async processDataFromOPFS(
        blockDataOPFS: BlockDataOPFS | null,
        batchCount: number,
        reservedLine: Array<[number, number]> = [],
    ): Promise<void> {
        this.blockDataOPFS = blockDataOPFS;
        this.batchCount = batchCount;
        this.data = [];
        this.reservedLine = reservedLine;
        if (blockDataOPFS && this.sourceReadBuffer.length < blockDataOPFS.getMaxBatchPathFloats()) {
            this.sourceReadBuffer = new Float32Array(blockDataOPFS.getMaxBatchPathFloats());
        }
    }

    processHighlightData(highlightData: RenderData['blocks'] = []): void {
        this.highlightData = highlightData;
    }

    setReservedLine(reservedLine: Array<[number, number]> = []): void {
        this.reservedLine = reservedLine;
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

    async render(
        options: RenderOptions,
        shouldCancel: () => boolean = () => false,
        visibility: BlockGraphLayerVisibility = { blocks: true, overview: true },
    ): Promise<void> {
        if (this.context === null) {
            return;
        }
        const { transform, viewport } = options;
        this.context.resetTransform();
        this.context.clearRect(0, 0, viewport.width, viewport.height);
        this.context.translate(transform.x, viewport.height - transform.y);
        this.context.scale(this.getScaleX(transform), -this.getScaleY(transform));
        this.context.save();
        try {
            if (visibility.blocks) {
                if (this.blockDataOPFS && this.batchCount > 0) {
                    await this.renderDataFromOPFS(this.blockDataOPFS, this.batchCount, options, false, this.dimBase, shouldCancel);
                    if (shouldCancel()) return;
                } else {
                    this.renderData(this.data, options, false, this.dimBase);
                }
            }
            if (shouldCancel()) {
                return;
            }
            if (visibility.overview) this.renderReservedLine(options);
            if (visibility.blocks) {
                this.renderData(this.highlightData, options);
                this.renderData(this.highlightData, options, true);
            }
        } finally {
            this.context.restore();
        }
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

    async renderDataFromOPFS(
        blockDataOPFS: BlockDataOPFS,
        batchCount: number,
        options: RenderOptions,
        isHighlight: boolean = false,
        dimBase: boolean = false,
        shouldCancel: () => boolean = () => false,
    ): Promise<void> {
        const { transform, viewport, zoom } = options;
        const xScale = transform.scaleX * zoom.x;
        const visibleStart = xScale === 0
            ? Number.NEGATIVE_INFINITY
            : zoom.offset - transform.x / xScale;
        const visibleEnd = xScale === 0
            ? Number.POSITIVE_INFINITY
            : zoom.offset + (viewport.width - transform.x) / xScale;
        const minTimestamp = Math.min(visibleStart, visibleEnd);
        const maxTimestamp = Math.max(visibleStart, visibleEnd);
        const batchIndices = blockDataOPFS.findBatchesOverlappingRange(minTimestamp, maxTimestamp);
        for (const batchIndex of batchIndices) {
            if (shouldCancel()) {
                return;
            }
            if (batchIndex >= batchCount) {
                continue;
            }
            const batchData = await blockDataOPFS.readBatchAsync(batchIndex, this.sourceReadBuffer);
            if (!batchData) {
                continue;
            }
            this.sourceReadBuffer = batchData.pathData;
            const { metas, pathData } = batchData;
            for (const meta of metas) {
                if (meta.pathEndTimestamp < minTimestamp || meta.pathStartTimestamp > maxTimestamp || meta.pathLength < 2) {
                    continue;
                }
                let left = 0;
                let right = meta.pathLength - 1;
                while (left < right) {
                    const middle = Math.floor((left + right) / 2);
                    const timestamp = pathData[(meta.pathOffset + middle) * 2];
                    if (timestamp < minTimestamp) {
                        left = middle + 1;
                    } else {
                        right = middle;
                    }
                }
                const firstSegment = Math.max(0, left - 1);
                for (let pathIndex = firstSegment; pathIndex < meta.pathLength - 1; pathIndex++) {
                    const p0 = getPointFromPathData(pathData, meta.pathOffset, pathIndex);
                    const p1 = getPointFromPathData(pathData, meta.pathOffset, pathIndex + 1);
                    if (p0[0] > maxTimestamp) {
                        break;
                    }
                    if (p1[0] < minTimestamp) {
                        continue;
                    }
                    const startPixel = Math.floor((p0[0] - zoom.offset) * xScale + transform.x);
                    const endPixel = Math.floor((p1[0] - zoom.offset) * xScale + transform.x);
                    if (xScale > 0 && startPixel === endPixel) {
                        let pixelLeft = pathIndex + 2;
                        let pixelRight = meta.pathLength;
                        while (pixelLeft < pixelRight) {
                            const middle = Math.floor((pixelLeft + pixelRight) / 2);
                            const middleTimestamp = pathData[(meta.pathOffset + middle) * 2];
                            const middlePixel = Math.floor((middleTimestamp - zoom.offset) * xScale + transform.x);
                            if (middlePixel <= startPixel) {
                                pixelLeft = middle + 1;
                            } else {
                                pixelRight = middle;
                            }
                        }
                        const collapsedEndIndex = Math.max(pathIndex + 1, pixelLeft - 1);
                        const collapsedEnd = getPointFromPathData(pathData, meta.pathOffset, collapsedEndIndex);
                        this.drawShape(p0, collapsedEnd, meta.size, meta.addr, options, dimBase);
                        pathIndex = collapsedEndIndex - 1;
                        continue;
                    }
                    this.drawShape(p0, p1, meta.size, meta.addr, options, dimBase);
                }
            }
        }
    }

    renderDataFromBatchData(
        metas: BlockMeta[],
        pathData: Float32Array,
        options: RenderOptions,
        isHighlight: boolean = false,
        dimBase: boolean = false,
    ): void {
        for (let i = 0; i < metas.length; i++) {
            const meta = metas[i];
            if (isHighlight) {
                this.drawBlockOutlineFromMeta(meta, pathData, options);
                continue;
            }
            for (let j = 0; j < meta.pathLength - 1; j++) {
                const p0 = getPointFromPathData(pathData, meta.pathOffset, j);
                const p1 = getPointFromPathData(pathData, meta.pathOffset, j + 1);
                this.drawShape(p0, p1, meta.size, meta.addr, options, dimBase);
            }
        }
    }

    drawBlockOutlineFromMeta(meta: BlockMeta, pathData: Float32Array, options: RenderOptions): void {
        if (this.context === null || meta.pathLength < 1) {
            return;
        }
        const { zoom } = options;
        const toX = (point: [number, number]): number => (point[0] - zoom.offset) * zoom.x;
        const toY = (point: [number, number]): number => point[1] * zoom.y;

        this.context.beginPath();
        const firstPt = getPointFromPathData(pathData, meta.pathOffset, 0);
        this.context.moveTo(toX(firstPt), toY(firstPt));
        for (let i = 1; i < meta.pathLength; i++) {
            const pt = getPointFromPathData(pathData, meta.pathOffset, i);
            this.context.lineTo(toX(pt), toY(pt));
        }
        for (let i = meta.pathLength - 1; i >= 0; i--) {
            const pt = getPointFromPathData(pathData, meta.pathOffset, i);
            this.context.lineTo(toX(pt), (pt[1] + meta.size) * zoom.y);
        }
        this.context.closePath();
        this.context.strokeStyle = getColorStringByAddr(meta.addr, true);
        this.context.lineWidth = 2 / Math.max(this.getScaleX(options.transform), this.getScaleY(options.transform));
        this.context.stroke();
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
