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

import { Painter } from './Painter';
import { BlockDataOPFS } from '../../tools/BlockDataOPFS';

export class WebGLRenderer {
    readonly canvas: OffscreenCanvas;
    readonly devicePixelRatio: number;
    private transform: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
    readonly painter: Painter;
    private rafPending: boolean = false;
    private zoom: RenderOptions['zoom'] = { x: 0, y: 0, offset: 0 };
    private dimBase: boolean = false;

    constructor(canvas: OffscreenCanvas, devicePixelRatio: number, opfsRuntimeId: string) {
        this.canvas = canvas;
        this.devicePixelRatio = devicePixelRatio;
        this.painter = new Painter(this.canvas, opfsRuntimeId);
    }

    async initialize(): Promise<void> {
        await this.painter.initialize();
    }

    setZoom(zoom: RenderOptions['zoom'], render: boolean = true): this {
        this.zoom = zoom;
        if (render) {
            this.renderFrame();
        }
        return this;
    }

    setReservedLine(reservedLine: Array<[number, number]> = []): this {
        this.painter.setReservedLine(reservedLine);
        return this;
    }

    async setData(data: RenderData['blocks'] = [], reservedLine: Array<[number, number]> = []): Promise<this> {
        this.painter.setReservedLine(reservedLine);
        await this.painter.memoryBlockProgram?.processData(data, this.dimBase);
        this.renderFrame();
        return this;
    }

    async setDataFromOPFS(
        blockDataOPFS: BlockDataOPFS | null,
        batchCount: number,
        reservedLine: Array<[number, number]> = [],
        render: boolean = true,
    ): Promise<this> {
        this.painter.setReservedLine(reservedLine);
        await this.painter.memoryBlockProgram?.processDataFromOPFS(blockDataOPFS, batchCount, this.dimBase);
        if (render) {
            this.renderFrame();
        }
        return this;
    }

    async beginProgressiveDataFromOPFS(
        blockDataOPFS: BlockDataOPFS,
        reservedLine: Array<[number, number]> = [],
    ): Promise<this> {
        this.painter.setReservedLine(reservedLine);
        await this.painter.memoryBlockProgram?.processDataFromOPFS(blockDataOPFS, 0, this.dimBase);
        this.renderFrame();
        return this;
    }

    appendDataFromOPFS(startBatch: number, endBatch: number): number {
        const viewport = { width: this.canvas.width, height: this.canvas.height };
        return this.painter.renderMemoryBlockBatchRange(
            startBatch,
            endBatch,
            { transform: this.transform, viewport, zoom: this.zoom },
        );
    }

    async setHighlightData(highlightData: RenderData['blocks'] = [], render: boolean = true): Promise<this> {
        await this.painter.memoryBlockHighlightProgram?.processData(highlightData);
        this.painter.memoryBlockBorderHightlightProgram?.processData(highlightData);
        if (render) {
            this.renderFrame();
        }
        return this;
    }

    setBaseDimmed(dimBase: boolean, render: boolean = true): this {
        if (this.dimBase === dimBase) {
            return this;
        }
        this.dimBase = dimBase;
        this.painter.memoryBlockProgram?.setDimBase(this.dimBase);
        if (render) {
            this.renderFrame();
        }
        return this;
    }

    setTransform(transform: RenderOptions['transform']): this {
        this.transform = transform;
        this.requestRender();
        return this;
    }

    updateCanvasSize(viewport: RenderOptions['viewport']): this {
        this.canvas.width = Math.max(1, Math.floor(viewport.width));
        this.canvas.height = Math.max(1, Math.floor(viewport.height));
        this.requestRender();
        return this;
    }

    requestRender(): void {
        if (this.rafPending) {
            return;
        }
        this.rafPending = true;
        requestAnimationFrame(() => {
            this.rafPending = false;
            this.renderFrame();
        });
    }

    renderFrame(): void {
        const viewport = { width: this.canvas.width, height: this.canvas.height };
        this.painter.render({ transform: this.transform, viewport, zoom: this.zoom });
    }

    destroy(): void {
    }
}
