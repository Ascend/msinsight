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

import { Program } from './Program';

export class MemoryBlockBorderProgram extends Program {
    protected glInstanceData: Float32Array = new Float32Array();
    protected glInstanceDataSize: number = 0;
    hasBuffer = false;

    bindBuffer(): void {
        const gl = this.gl;
        if (this.instanceBuffer) {
            gl.deleteBuffer(this.instanceBuffer);
        }
        this.instanceBuffer = this.createBuffer(4 * Math.max(this.glInstanceDataSize, 1));
        gl.bindVertexArray(this.vao);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
        const stride = 5 * 4;
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, stride, 0);
        gl.vertexAttribDivisor(0, 1);
        gl.enableVertexAttribArray(1);
        gl.vertexAttribPointer(1, 2, gl.FLOAT, false, stride, 8);
        gl.vertexAttribDivisor(1, 1);
        gl.enableVertexAttribArray(2);
        gl.vertexAttribPointer(2, 1, gl.FLOAT, false, stride, 16);
        gl.vertexAttribDivisor(2, 1);
        this.cleanupGL();
    }

    processData(data: RenderData['blocks']): void {
        let totalLength = 0;
        for (let i = 0; i < data.length; i++) {
            totalLength += Math.max(data[i].path.length + 1, 0) * 5;
        }

        const needRealloc = !this.glInstanceData || this.glInstanceData.length < totalLength;
        const instanceData = needRealloc ? new Float32Array(totalLength) : this.glInstanceData;

        let offset = 0;
        for (let i = 0; i < data.length; i++) {
            const { path, size } = data[i];
            if (path.length < 1) {
                continue;
            }
            for (let j = 0; j < path.length - 1; j++) {
                instanceData[offset++] = path[j][0];
                instanceData[offset++] = path[j][1];
                instanceData[offset++] = path[j + 1][0];
                instanceData[offset++] = path[j + 1][1];
                instanceData[offset++] = size;
            }
            instanceData[offset++] = path[0][0];
            instanceData[offset++] = path[0][1];
            instanceData[offset++] = path[0][0];
            instanceData[offset++] = path[0][1] + size;
            instanceData[offset++] = 0;
            const lastPoint = path[path.length - 1];
            instanceData[offset++] = lastPoint[0];
            instanceData[offset++] = lastPoint[1];
            instanceData[offset++] = lastPoint[0];
            instanceData[offset++] = lastPoint[1] + size;
            instanceData[offset++] = 0;
        }

        this.glInstanceData = instanceData;
        this.glInstanceDataSize = offset;
        if (needRealloc || this.instanceBuffer === null) {
            this.bindBuffer();
        } else {
            this.updateSubBuffer(this.glInstanceData, this.glInstanceDataSize);
        }
        this.hasBuffer = true;
    }

    render(options: RenderOptions): void {
        if (!this.hasBuffer || this.instanceBuffer === null) {
            return;
        }
        const gl = this.gl;
        this.updateSubBuffer(this.glInstanceData, this.glInstanceDataSize);
        const instanceCount = this.glInstanceDataSize / 5;
        gl.useProgram(this.program);
        this.setBaseUniforms();
        gl.uniform1f(this.uniformLoc.uOffset, this.uniformData[8]);
        gl.bindVertexArray(this.vao);
        gl.drawArraysInstanced(gl.LINES, 0, 4, instanceCount);
        this.cleanupGL();
    }
}
