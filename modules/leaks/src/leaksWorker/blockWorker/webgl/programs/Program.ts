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

import { GL_COLORS, GL_DIMMED_COLORS, GL_HIGHLIGHT_COLORS } from '@/leaksWorker/tools/color';

type ColorMode = 'normal' | 'highlight' | 'dimmed';

export abstract class Program {
    readonly uniformLoc: Record<string, WebGLUniformLocation | null> = {};
    gl: WebGL2RenderingContext;
    program: WebGLProgram | null;
    vao: WebGLVertexArrayObject | null = null;
    instanceBuffer: WebGLBuffer | null = null;
    readonly uniformData: Float32Array;

    constructor(gl: WebGL2RenderingContext, uniformData: Float32Array, shader: Shader) {
        this.gl = gl;
        this.uniformData = uniformData;

        this.program = this.createProgram(shader.vertexShader, shader.fragmentShader);
        this.uniformLoc.uScale = gl.getUniformLocation(this.program, 'uScale');
        this.uniformLoc.uTranslate = gl.getUniformLocation(this.program, 'uTranslate');
        this.uniformLoc.uResolution = gl.getUniformLocation(this.program, 'uResolution');
        this.uniformLoc.uZoom = gl.getUniformLocation(this.program, 'uZoom');
        this.uniformLoc.uOffset = gl.getUniformLocation(this.program, 'uOffset');
        for (let i = 0; i < GL_COLORS.length; i++) {
            this.uniformLoc[`uColors[${i}]`] = gl.getUniformLocation(this.program, `uColors[${i}]`);
        }
        this.vao = gl.createVertexArray();
        gl.bindVertexArray(this.vao);
    }

    protected createShader(type: number, source: string): WebGLShader {
        const gl = this.gl;
        const shader = gl.createShader(type);
        if (shader === null) {
            throw new Error('Failed to create shader');
        }
        gl.shaderSource(shader, source);
        gl.compileShader(shader);
        if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
            const info = gl.getShaderInfoLog(shader);
            gl.deleteShader(shader);
            throw new Error(`Shader compilation failed: ${info}`);
        }
        return shader;
    }

    protected createProgram(vertexSource: string, fragmentSource: string): WebGLProgram {
        const gl = this.gl;
        const vertexShader = this.createShader(gl.VERTEX_SHADER, vertexSource);
        const fragmentShader = this.createShader(gl.FRAGMENT_SHADER, fragmentSource);
        const program = gl.createProgram();
        if (program === null) {
            throw new Error('Failed to create program');
        }
        gl.attachShader(program, vertexShader);
        gl.attachShader(program, fragmentShader);
        gl.linkProgram(program);
        if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
            const info = gl.getProgramInfoLog(program);
            gl.deleteProgram(program);
            throw new Error(`Program linking failed: ${info}`);
        }
        gl.deleteShader(vertexShader);
        gl.deleteShader(fragmentShader);
        return program;
    }

    createBuffer(size: number): WebGLBuffer {
        const gl = this.gl;
        const buffer = gl.createBuffer();
        if (buffer === null) {
            throw new Error('Failed to create buffer.');
        }
        gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
        gl.bufferData(gl.ARRAY_BUFFER, size, gl.DYNAMIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, null);
        return buffer;
    }

    setBaseUniforms(): void {
        const gl = this.gl;
        const d = this.uniformData;
        gl.uniform2f(this.uniformLoc.uScale, d[0], d[1]);
        gl.uniform2f(this.uniformLoc.uTranslate, d[2], d[3]);
        gl.uniform2f(this.uniformLoc.uResolution, d[4], d[5]);
        gl.uniform2f(this.uniformLoc.uZoom, d[6], d[7]);
    }

    setColorUniforms(mode: ColorMode = 'normal'): void {
        const gl = this.gl;
        const colors = mode === 'highlight' ? GL_HIGHLIGHT_COLORS : mode === 'dimmed' ? GL_DIMMED_COLORS : GL_COLORS;
        for (let i = 0; i < colors.length; i++) {
            gl.uniform4f(this.uniformLoc[`uColors[${i}]`], colors[i][0], colors[i][1], colors[i][2], colors[i][3]);
        }
    }

    updateSubBuffer(data: Float32Array, size: number): void {
        const gl = this.gl;
        gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
        gl.bufferSubData(gl.ARRAY_BUFFER, 0, data.subarray(0, size));
    }

    cleanupGL(): void {
        this.gl.bindVertexArray(null);
        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, null);
    }

    destroy(): void {
        if (this.instanceBuffer) {
            this.gl.deleteBuffer(this.instanceBuffer);
            this.instanceBuffer = null;
        }
        if (this.vao) {
            this.gl.deleteVertexArray(this.vao);
            this.vao = null;
        }
        if (this.program) {
            this.gl.deleteProgram(this.program);
            this.program = null;
        }
    }
}
