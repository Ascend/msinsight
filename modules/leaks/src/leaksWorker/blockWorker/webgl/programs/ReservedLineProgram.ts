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

const RESERVED_LINE_COLOR = [0, 0.32, 0.85, 1];

export class ReservedLineProgram extends Program {
    private vertices = new Float32Array(0);
    private pointCount = 0;

    constructor(gl: WebGL2RenderingContext, uniformData: Float32Array, shader: Shader) {
        super(gl, uniformData, shader);
        const program = this.program;
        if (program === null) {
            throw new Error('Reserved line program unavailable');
        }
        this.uniformLoc.uColor = gl.getUniformLocation(program, 'uColor');
        this.instanceBuffer = this.createBuffer(0);
        gl.bindVertexArray(this.vao);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
        gl.enableVertexAttribArray(0);
        gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
        this.cleanupGL();
    }

    processData(reservedLine: Array<[number, number]> = []): void {
        this.pointCount = reservedLine.length;
        this.vertices = new Float32Array(this.pointCount * 2);
        for (let i = 0; i < reservedLine.length; i++) {
            this.vertices[i * 2] = reservedLine[i][0];
            this.vertices[i * 2 + 1] = reservedLine[i][1];
        }
        const gl = this.gl;
        gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, this.vertices, gl.DYNAMIC_DRAW);
        this.cleanupGL();
    }

    render(options: RenderOptions): void {
        if (this.pointCount < 2 || this.instanceBuffer === null || this.program === null) {
            return;
        }
        const gl = this.gl;
        gl.useProgram(this.program);
        this.setBaseUniforms();
        gl.uniform1f(this.uniformLoc.uOffset, this.uniformData[8]);
        gl.uniform4f(this.uniformLoc.uColor, RESERVED_LINE_COLOR[0], RESERVED_LINE_COLOR[1], RESERVED_LINE_COLOR[2], RESERVED_LINE_COLOR[3]);
        gl.bindVertexArray(this.vao);
        gl.drawArrays(gl.LINE_STRIP, 0, this.pointCount);
        this.cleanupGL();
    }
}
