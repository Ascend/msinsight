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

export class NativeRenderer {
    readonly canvas: HTMLCanvasElement;
    readonly devicePixelRatio: number;
    private transform: RenderOptions['transform'] = { x: 0, y: 0, scale: 1, scaleX: 1, scaleY: 1 };
    readonly painter: Painter;
    private rafPending: boolean = false;
    private zoom: RenderOptions['zoom'] = { x: 0, y: 0, offset: 0 };

    constructor(canvas: HTMLCanvasElement, devicePixelRatio: number) {
        this.canvas = canvas;
        this.devicePixelRatio = devicePixelRatio;
        this.painter = new Painter(this.canvas, this.devicePixelRatio);
    }

    async initialize(): Promise<void> {
        await this.painter.initialize();
    }

    setZoom(zoom: RenderOptions['zoom']): this {
        this.zoom = zoom;
        this.renderFrame();
        return this;
    }

    setData(data: RenderData['blocks'] = []): this {
        this.painter.processData(data);
        this.renderFrame();
        return this;
    }

    setHighlightData(highlightData: RenderData['blocks'] = []): this {
        this.painter.processHighlightData(highlightData);
        this.renderFrame();
        return this;
    }

    setBaseDimmed(dimBase: boolean): this {
        this.painter.setBaseDimmed(dimBase);
        this.renderFrame();
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
