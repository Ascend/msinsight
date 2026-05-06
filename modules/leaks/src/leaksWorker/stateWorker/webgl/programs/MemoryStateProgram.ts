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

import { GL_COLORS } from '@/leaksWorker/tools/color';
import { Program } from './Program';

export class MemoryStateProgram extends Program {
    protected glInstanceData: Float32Array = new Float32Array();
    protected glInstanceDataSize: number = 0;
    hasBuffer = false;
    protected stride = 4;
    private dimBase: boolean = false;

    bindBuffer(): void {
        const gl = this.gl;
        if (this.instanceBuffer) {
            gl.deleteBuffer(this.instanceBuffer);
        }
        this.instanceBuffer = this.createBuffer(4 * Math.max(this.glInstanceDataSize, 1));
        gl.bindVertexArray(this.vao);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
        const strideBytes = this.stride * 4;
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 1, gl.FLOAT, false, strideBytes, 0);
        gl.vertexAttribDivisor(0, 1);
        gl.enableVertexAttribArray(1);
        gl.vertexAttribPointer(1, 1, gl.FLOAT, false, strideBytes, 4);
        gl.vertexAttribDivisor(1, 1);
        gl.enableVertexAttribArray(2);
        gl.vertexAttribPointer(2, 1, gl.FLOAT, false, strideBytes, 8);
        gl.vertexAttribDivisor(2, 1);
        gl.enableVertexAttribArray(3);
        gl.vertexAttribPointer(3, 1, gl.FLOAT, false, strideBytes, 12);
        gl.vertexAttribDivisor(3, 1);
        this.cleanupGL();
    }

    processData(data: Segment[], dimBase: boolean = false): void {
        this.dimBase = dimBase;
        let totalLength = 0;
        for (let i = 0; i < data.length; i++) {
            totalLength += data[i].blocks.length * this.stride;
        }

        const needRealloc = !this.glInstanceData || this.glInstanceData.length < totalLength;
        const instanceData = needRealloc ? new Float32Array(totalLength) : this.glInstanceData;

        let offset = 0;
        for (let i = 0; i < data.length; i++) {
            const segment = data[i];
            for (let j = 0; j < segment.blocks.length; j++) {
                const block = segment.blocks[j];
                const colorIndex = block.colorIndex ?? j;
                instanceData[offset++] = segment.offsetX + block.offset;
                instanceData[offset++] = segment.offsetY;
                instanceData[offset++] = block.size;
                instanceData[offset++] = colorIndex % GL_COLORS.length;
            }
        }

        this.glInstanceData = instanceData;
        this.glInstanceDataSize = totalLength;
        if (needRealloc || this.instanceBuffer === null) {
            this.bindBuffer();
        } else {
            this.updateSubBuffer(instanceData, totalLength);
        }
        this.hasBuffer = true;
    }

    render(options: RenderOptions): void {
        if (!this.hasBuffer || this.instanceBuffer === null) {
            return;
        }
        const gl = this.gl;
        this.updateSubBuffer(this.glInstanceData, this.glInstanceDataSize);
        gl.useProgram(this.program);
        this.setBaseUniforms();
        this.setColorUniforms(this.dimBase ? 'dimmed' : 'normal');
        gl.bindVertexArray(this.vao);
        gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, this.glInstanceDataSize / this.stride);
        this.cleanupGL();
    }
}
