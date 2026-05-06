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

import { MemoryStateProgram } from './programs/MemoryStateProgram';
import { MemoryStateBorderProgram, RenderType } from './programs/MemoryStateBorderProgram';
import shaders from './shaders';

export class Painter {
    private gl: WebGL2RenderingContext | null = null;
    readonly canvas: OffscreenCanvas;
    memoryStateProgram: MemoryStateProgram | null = null;
    memoryStateHighlightFillProgram: MemoryStateProgram | null = null;
    memoryStateBorderProgram: MemoryStateBorderProgram | null = null;
    memoryStateHighlightProgram: MemoryStateBorderProgram | null = null;
    private uniformData: Float32Array;

    constructor(canvas: OffscreenCanvas) {
        this.canvas = canvas;
        this.uniformData = new Float32Array(8);
    }

    async initialize(): Promise<void> {
        const gl = this.canvas.getContext('webgl2', {
            alpha: true,
            depth: true,
            stencil: false,
            antialias: true,
            premultipliedAlpha: true,
            preserveDrawingBuffer: false,
            powerPreference: 'high-performance',
        });
        if (gl === null) { throw new Error('WebGL2 not supported'); }
        this.gl = gl;
        this.memoryStateProgram = new MemoryStateProgram(this.gl, this.uniformData, shaders.memoryState);
        this.memoryStateHighlightFillProgram = new MemoryStateProgram(this.gl, this.uniformData, shaders.memoryState);
        this.memoryStateBorderProgram = new MemoryStateBorderProgram(this.gl, this.uniformData, shaders.memoryStateBorder);
        this.memoryStateHighlightProgram = new MemoryStateBorderProgram(this.gl, this.uniformData, shaders.memoryStateBorder, RenderType.highlightBorder);
    }

    private updateUniformData(options: RenderOptions): void {
        const { transform, viewport, zoom } = options;
        this.uniformData[0] = transform.scaleX ?? transform.scale;
        this.uniformData[1] = transform.scaleY ?? transform.scale;
        this.uniformData[2] = transform.x;
        this.uniformData[3] = transform.y;
        this.uniformData[4] = viewport.width;
        this.uniformData[5] = viewport.height;
        this.uniformData[6] = zoom.x;
        this.uniformData[7] = zoom.y;
    }

    render(options: RenderOptions): void {
        const gl = this.gl;
        if (gl === null) {
            return;
        }
        gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.enable(gl.BLEND);
        gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        this.updateUniformData(options);
        this.memoryStateProgram?.render(options);
        this.memoryStateHighlightFillProgram?.render(options);
        this.memoryStateBorderProgram?.render(options);
        this.memoryStateHighlightProgram?.render(options);
        gl.disable(gl.BLEND);
    }
}
