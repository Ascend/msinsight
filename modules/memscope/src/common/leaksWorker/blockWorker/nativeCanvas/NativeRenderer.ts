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

export class NativeRenderer {
    readonly canvas: HTMLCanvasElement;
    readonly devicePixelRatio: number;
    private transform: RenderOptions['transform'] = { x: 0, y: 0, scaleX: 1, scaleY: 1 };
    readonly painter: Painter;
    private rafPending: boolean = false;
    private zoom: RenderOptions['zoom'] = { x: 0, y: 0, offset: 0 };
    private renderRequested: boolean = false;
    private renderRunning: boolean = false;
    private renderVersion: number = 0;
    private renderPromise: Promise<void> = Promise.resolve();
    private layerVisibility: BlockGraphLayerVisibility = { blocks: true, overview: true };

    constructor(canvas: HTMLCanvasElement, devicePixelRatio: number) {
        this.canvas = canvas;
        this.devicePixelRatio = devicePixelRatio;
        this.painter = new Painter(this.canvas, this.devicePixelRatio);
    }

    async initialize(): Promise<void> {
        await this.painter.initialize();
    }

    setZoom(zoom: RenderOptions['zoom'], render: boolean = true): this {
        this.zoom = zoom;
        if (render) {
            void this.renderFrame();
        }
        return this;
    }

    setAllocationLines(lines: AllocationLineData): this {
        this.painter.setAllocationLines(lines);
        return this;
    }

    setData(data: RenderData['blocks'], lines: AllocationLineData): this {
        this.painter.processData(data, lines);
        void this.renderFrame();
        return this;
    }

    async setDataFromOPFS(
        blockDataOPFS: BlockDataOPFS | null,
        batchCount: number,
        lines: AllocationLineData,
        render: boolean = true,
    ): Promise<this> {
        await this.painter.processDataFromOPFS(blockDataOPFS, batchCount, lines);
        if (render) {
            await this.renderFrame();
        }
        return this;
    }

    setHighlightData(highlightData: RenderData['blocks'] = [], render: boolean = true): this {
        this.painter.processHighlightData(highlightData);
        if (render) {
            void this.renderFrame();
        }
        return this;
    }

    setBaseDimmed(dimBase: boolean, render: boolean = true): this {
        this.painter.setBaseDimmed(dimBase);
        if (render) {
            void this.renderFrame();
        }
        return this;
    }

    setLayerVisibility(visibility: BlockGraphLayerVisibility): this {
        this.layerVisibility = visibility;
        this.requestRender();
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
            void this.renderFrame();
        });
    }

    renderFrame(): Promise<void> {
        this.renderRequested = true;
        this.renderVersion++;
        if (!this.renderRunning) {
            this.renderRunning = true;
            this.renderPromise = this.renderLoop();
        }
        return this.renderPromise;
    }

    private async renderLoop(): Promise<void> {
        while (this.renderRequested) {
            this.renderRequested = false;
            const renderVersion = this.renderVersion;
            const viewport = { width: this.canvas.width, height: this.canvas.height };
            await this.painter.render(
                { transform: this.transform, viewport, zoom: this.zoom },
                () => renderVersion !== this.renderVersion,
                this.layerVisibility,
            );
        }
        this.renderRunning = false;
    }

    destroy(): void {
    }
}
