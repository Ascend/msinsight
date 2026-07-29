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

import { hashHexAddressToIndex } from '@/leaksWorker/tools/color';
import { BatchOPFS } from '@/leaksWorker/tools/BatchOPFS';
import { BlockDataOPFS } from '@/leaksWorker/tools/BlockDataOPFS';
import { Program } from './Program';

export class MemoryBlockProgram extends Program {
    readonly isHighlight: boolean;
    private dimBase: boolean = false;
    protected batchOPFS: BatchOPFS;
    protected batchCount: number = 0;
    protected maxInstanceDataSize: number = 600000;
    private readonly renderReadBuffer: Float32Array;
    private sourceReadBuffer: Float32Array;
    private blockDataOPFS: BlockDataOPFS | null = null;
    private blockBatchCount: number = 0;
    hasBuffer = false;

    constructor(
        gl: WebGL2RenderingContext,
        uniformData: Float32Array,
        shader: Shader,
        isHighlight: boolean = false,
        opfsRuntimeId: string = 'default',
    ) {
        super(gl, uniformData, shader);
        this.isHighlight = isHighlight;
        this.batchOPFS = new BatchOPFS(`${isHighlight ? 'highlight' : 'normal'}-${opfsRuntimeId}`);
        this.renderReadBuffer = new Float32Array(this.maxInstanceDataSize);
        this.sourceReadBuffer = new Float32Array(this.maxInstanceDataSize);
    }

    async initOPFS(): Promise<void> {
        await this.batchOPFS.init();
    }

    bindBuffer(): void {
        const gl = this.gl;
        if (this.instanceBuffer) {
            return;
        }
        this.instanceBuffer = this.createBuffer(4 * this.maxInstanceDataSize);
        gl.bindVertexArray(this.vao);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
        const stride = 6 * 4;
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, stride, 0);
        gl.vertexAttribDivisor(0, 1);
        gl.enableVertexAttribArray(1);
        gl.vertexAttribPointer(1, 2, gl.FLOAT, false, stride, 8);
        gl.vertexAttribDivisor(1, 1);
        gl.enableVertexAttribArray(2);
        gl.vertexAttribPointer(2, 1, gl.FLOAT, false, stride, 16);
        gl.vertexAttribDivisor(2, 1);
        gl.enableVertexAttribArray(3);
        gl.vertexAttribPointer(3, 1, gl.FLOAT, false, stride, 20);
        gl.vertexAttribDivisor(3, 1);
        this.cleanupGL();
    }

    async processData(data: RenderData['blocks'], dimBase: boolean = false): Promise<void> {
        await this.batchOPFS.clear();
        this.blockDataOPFS = null;
        this.blockBatchCount = 0;
        this.dimBase = dimBase;
        const batch = new Float32Array(this.maxInstanceDataSize);
        let batchLength = 0;
        let batchIndex = 0;
        for (let i = 0; i < data.length; i++) {
            const { path, size, addr } = data[i];
            const colorIndex = hashHexAddressToIndex(addr);
            for (let j = 0; j < path.length - 1; j++) {
                if (batchLength + 6 > this.maxInstanceDataSize) {
                    await this.batchOPFS.write(batchIndex++, batch.subarray(0, batchLength));
                    batchLength = 0;
                }
                batch[batchLength] = path[j][0];
                batch[batchLength + 1] = path[j][1];
                batch[batchLength + 2] = path[j + 1][0];
                batch[batchLength + 3] = path[j + 1][1];
                batch[batchLength + 4] = size;
                batch[batchLength + 5] = colorIndex;
                batchLength += 6;
            }
        }
        if (batchLength > 0) {
            await this.batchOPFS.write(batchIndex++, batch.subarray(0, batchLength));
        }
        this.batchCount = batchIndex;
        this.bindBuffer();
        this.hasBuffer = true;
    }

    async processDataFromOPFS(blockDataOPFS: BlockDataOPFS | null, batchCount: number, dimBase: boolean = false): Promise<void> {
        await this.batchOPFS.clear();
        this.dimBase = dimBase;
        if (!blockDataOPFS) {
            this.blockDataOPFS = null;
            this.blockBatchCount = 0;
            this.batchCount = 0;
            this.hasBuffer = false;
            return;
        }
        this.blockDataOPFS = blockDataOPFS;
        this.blockBatchCount = batchCount;
        this.batchCount = 0;
        if (this.sourceReadBuffer.length < blockDataOPFS.getMaxBatchPathFloats()) {
            this.sourceReadBuffer = new Float32Array(blockDataOPFS.getMaxBatchPathFloats());
        }
        this.bindBuffer();
        this.hasBuffer = true;
    }

    renderBlockDataRangeFromOPFS(startBatch: number, endBatch: number, options: RenderOptions): number {
        if (!this.blockDataOPFS || this.instanceBuffer === null || endBatch <= startBatch) {
            return 0;
        }
        this.blockBatchCount = Math.max(this.blockBatchCount, endBatch);
        this.hasBuffer = true;
        const gl = this.gl;
        gl.useProgram(this.program);
        this.setBaseUniforms();
        gl.uniform1f(this.uniformLoc.uOffset, this.uniformData[8]);
        this.setColorUniforms(this.dimBase ? 'dimmed' : 'normal');
        gl.bindVertexArray(this.vao);
        const renderedInstanceCount = this.renderBlockDataFromOPFS(
            this.blockDataOPFS,
            options,
            startBatch,
            endBatch,
        );
        this.cleanupGL();
        return renderedInstanceCount;
    }

    setDimBase(dimBase: boolean): void {
        this.dimBase = dimBase;
    }

    render(options: RenderOptions): void {
        if (!this.hasBuffer || this.instanceBuffer === null) {
            return;
        }
        const gl = this.gl;
        gl.useProgram(this.program);
        this.setBaseUniforms();
        gl.uniform1f(this.uniformLoc.uOffset, this.uniformData[8]);
        this.setColorUniforms(this.isHighlight ? 'highlight' : this.dimBase ? 'dimmed' : 'normal');
        gl.bindVertexArray(this.vao);
        if (this.blockDataOPFS && this.blockBatchCount > 0) {
            this.renderBlockDataFromOPFS(this.blockDataOPFS, options);
            this.cleanupGL();
            return;
        }
        for (let i = 0; i < this.batchCount; i++) {
            const batch = this.batchOPFS.read(i, this.renderReadBuffer);
            if (batch) {
                gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
                gl.bufferSubData(gl.ARRAY_BUFFER, 0, batch, 0);
                const instanceCount = batch.length / 6;
                gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, instanceCount);
            }
        }
        this.cleanupGL();
    }

    private renderBlockDataFromOPFS(
        blockDataOPFS: BlockDataOPFS,
        options: RenderOptions,
        startBatch: number = 0,
        endBatch: number = this.blockBatchCount,
    ): number {
        const { transform, viewport, zoom } = options;
        const xScale = transform.scaleX * zoom.x;
        const yScale = transform.scaleY * zoom.y;
        const visibleStart = xScale === 0 ? Number.NEGATIVE_INFINITY : zoom.offset - transform.x / xScale;
        const visibleEnd = xScale === 0 ? Number.POSITIVE_INFINITY : zoom.offset + (viewport.width - transform.x) / xScale;
        const minTimestamp = Math.min(visibleStart, visibleEnd);
        const maxTimestamp = Math.max(visibleStart, visibleEnd);
        const batchIndices = blockDataOPFS.findBatchesOverlappingRange(
            minTimestamp,
            maxTimestamp,
            startBatch,
            endBatch,
        );
        let renderBatchLength = 0;
        let renderedInstanceCount = 0;

        const flushRenderBatch = (): void => {
            if (renderBatchLength === 0) {
                return;
            }
            const gl = this.gl;
            gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
            gl.bufferSubData(gl.ARRAY_BUFFER, 0, this.renderReadBuffer.subarray(0, renderBatchLength));
            const instanceCount = renderBatchLength / 6;
            gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, instanceCount);
            renderedInstanceCount += instanceCount;
            renderBatchLength = 0;
        };
        const appendRenderSegment = (
            x1: number,
            y1: number,
            x2: number,
            y2: number,
            size: number,
            colorIndex: number,
        ): void => {
            const screenY1 = y1 * yScale + transform.y;
            const screenY2 = y2 * yScale + transform.y;
            const screenY3 = (y1 + size) * yScale + transform.y;
            const screenY4 = (y2 + size) * yScale + transform.y;
            const minScreenY = Math.min(screenY1, screenY2, screenY3, screenY4);
            const maxScreenY = Math.max(screenY1, screenY2, screenY3, screenY4);
            if (maxScreenY < 0 || minScreenY > viewport.height) {
                return;
            }
            if (renderBatchLength + 6 > this.maxInstanceDataSize) {
                flushRenderBatch();
            }
            this.renderReadBuffer[renderBatchLength] = x1;
            this.renderReadBuffer[renderBatchLength + 1] = y1;
            this.renderReadBuffer[renderBatchLength + 2] = x2;
            this.renderReadBuffer[renderBatchLength + 3] = y2;
            this.renderReadBuffer[renderBatchLength + 4] = size;
            this.renderReadBuffer[renderBatchLength + 5] = colorIndex;
            renderBatchLength += 6;
        };

        for (const batchIndex of batchIndices) {
            const batchData = blockDataOPFS.readBatch(batchIndex, this.sourceReadBuffer);
            if (!batchData) {
                continue;
            }
            this.sourceReadBuffer = batchData.pathData;
            const { metas, pathData } = batchData;
            for (const meta of metas) {
                if (meta.pathEndTimestamp < minTimestamp || meta.pathStartTimestamp > maxTimestamp || meta.pathLength < 2) {
                    continue;
                }
                const colorIndex = hashHexAddressToIndex(meta.addr);
                let left = 0;
                let right = meta.pathLength - 1;
                while (left < right) {
                    const middle = Math.floor((left + right) / 2);
                    const middleTimestamp = pathData[(meta.pathOffset + middle) * 2];
                    if (middleTimestamp < minTimestamp) {
                        left = middle + 1;
                    } else {
                        right = middle;
                    }
                }
                const firstSegment = Math.max(0, left - 1);
                for (let pathIndex = firstSegment; pathIndex < meta.pathLength - 1; pathIndex++) {
                    const firstOffset = (meta.pathOffset + pathIndex) * 2;
                    const secondOffset = firstOffset + 2;
                    const x1 = pathData[firstOffset];
                    const y1 = pathData[firstOffset + 1];
                    const x2 = pathData[secondOffset];
                    const y2 = pathData[secondOffset + 1];
                    if (x1 > maxTimestamp) {
                        break;
                    }
                    if (x2 < minTimestamp) {
                        continue;
                    }
                    const startPixel = Math.floor((x1 - zoom.offset) * xScale + transform.x);
                    const endPixel = Math.floor((x2 - zoom.offset) * xScale + transform.x);
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
                        const collapsedEndOffset = (meta.pathOffset + collapsedEndIndex) * 2;
                        appendRenderSegment(
                            x1,
                            y1,
                            pathData[collapsedEndOffset],
                            pathData[collapsedEndOffset + 1],
                            meta.size,
                            colorIndex,
                        );
                        pathIndex = collapsedEndIndex - 1;
                        continue;
                    }
                    appendRenderSegment(x1, y1, x2, y2, meta.size, colorIndex);
                }
            }
        }
        flushRenderBatch();
        return renderedInstanceCount;
    }
}
