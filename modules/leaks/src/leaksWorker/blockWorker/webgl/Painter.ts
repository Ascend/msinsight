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

import { MemoryBlockProgram } from './programs/MemoryBlockProgram';
import { MemoryBlockBorderProgram } from './programs/MemoryBlockBorderProgram';
import { ReservedLineProgram } from './programs/ReservedLineProgram';
import shaders from './shaders';

export class Painter {
    private gl: WebGL2RenderingContext | null = null;
    readonly canvas: OffscreenCanvas;
    memoryBlockProgram: MemoryBlockProgram | null = null;
    memoryBlockHighlightProgram: MemoryBlockProgram | null = null;
    memoryBlockBorderHightlightProgram: MemoryBlockBorderProgram | null = null;
    reservedLineProgram: ReservedLineProgram | null = null;
    private uniformData: Float32Array;

    constructor(canvas: OffscreenCanvas, private readonly opfsRuntimeId: string) {
        this.canvas = canvas;
        this.uniformData = new Float32Array(9);
    }

    async initialize(): Promise<void> {
        const gl = this.canvas.getContext('webgl2', {
            alpha: true,
            depth: true,
            stencil: false,
            antialias: true,
            premultipliedAlpha: true,
            // Progressive rendering appends committed OPFS ranges across worker tasks.
            // Retaining the default framebuffer keeps earlier ranges visible between frames.
            preserveDrawingBuffer: true,
            powerPreference: 'high-performance',
        });
        if (gl === null) { throw new Error('WebGL2 not supported'); }
        this.gl = gl;
        this.memoryBlockProgram = new MemoryBlockProgram(
            this.gl,
            this.uniformData,
            shaders.memoryBlock,
            false,
            this.opfsRuntimeId,
        );
        this.memoryBlockHighlightProgram = new MemoryBlockProgram(
            this.gl,
            this.uniformData,
            shaders.memoryBlock,
            true,
            this.opfsRuntimeId,
        );
        this.memoryBlockBorderHightlightProgram = new MemoryBlockBorderProgram(this.gl, this.uniformData, shaders.memoryBlockBorder);
        this.reservedLineProgram = new ReservedLineProgram(this.gl, this.uniformData, shaders.reservedLine);
        await this.memoryBlockProgram.initOPFS();
        await this.memoryBlockHighlightProgram.initOPFS();
    }

    setReservedLine(reservedLine: Array<[number, number]> = []): void {
        this.reservedLineProgram?.processData(reservedLine);
    }

    private updateUniformData(options: RenderOptions): void {
        const { transform, viewport, zoom } = options;
        this.uniformData[0] = transform.scaleX;
        this.uniformData[1] = transform.scaleY;
        this.uniformData[2] = transform.x;
        this.uniformData[3] = transform.y;
        this.uniformData[4] = viewport.width;
        this.uniformData[5] = viewport.height;
        this.uniformData[6] = zoom.x;
        this.uniformData[7] = zoom.y;
        this.uniformData[8] = zoom.offset;
    }

    clear(viewport: RenderOptions['viewport']): void {
        const gl = this.gl;
        if (gl === null) {
            return;
        }
        gl.viewport(0, 0, viewport.width, viewport.height);
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    }

    renderMemoryBlockBatchRange(startBatch: number, endBatch: number, options: RenderOptions): number {
        const gl = this.gl;
        if (gl === null || endBatch <= startBatch) {
            return 0;
        }
        gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        gl.enable(gl.BLEND);
        gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        this.updateUniformData(options);
        const renderedInstanceCount = this.memoryBlockProgram?.renderBlockDataRangeFromOPFS(
            startBatch,
            endBatch,
            options,
        ) ?? 0;
        gl.disable(gl.BLEND);
        return renderedInstanceCount;
    }

    render(options: RenderOptions): void {
        const gl = this.gl;
        if (gl === null) {
            return;
        }
        this.clear(options.viewport);
        gl.enable(gl.BLEND);
        gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        this.updateUniformData(options);
        this.memoryBlockProgram?.render(options);
        this.reservedLineProgram?.render(options);
        this.memoryBlockHighlightProgram?.render(options);
        this.memoryBlockBorderHightlightProgram?.render(options);
        gl.disable(gl.BLEND);
    }
}
