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
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

import type { CommunicationWebGLOperator } from './communicationTimeWebglData';
import { CommunicationOperatorSource } from './communicationTimeWebglData';

const FLOATS_PER_INSTANCE = 8;
const INITIAL_INSTANCE_CAPACITY = 16384;
const HIGHLIGHT_STROKE = '#ffd666';
const HIGHLIGHT_SHADOW = 'rgba(255, 214, 102, 0.9)';
const HIGHLIGHT_LINE_WIDTH = 4;
const HIGHLIGHT_SHADOW_BLUR = 12;

const VERTEX_SHADER = `#version 300 es
in vec2 aCorner;
in vec4 aRect;
in vec4 aColor;

uniform vec2 uViewport;

out vec4 vColor;

void main() {
    vec2 position = aRect.xy + aCorner * aRect.zw;
    vec2 clip = position / uViewport * 2.0 - 1.0;
    gl_Position = vec4(clip.x, -clip.y, 0.0, 1.0);
    vColor = aColor;
}`;

const FRAGMENT_SHADER = `#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 outputColor;

void main() {
    outputColor = vColor;
}`;

export interface CommunicationWebGLLayout {
    canvasWidth: number;
    canvasHeight: number;
    gridLeft: number;
    gridTop: number;
    gridWidth: number;
    gridHeight: number;
    xMin: number;
    xMax: number;
    rankY0: number;
    rankStep: number;
    rowHeight: number;
    isCompare: boolean;
}

export interface CommunicationWebGLRectangle {
    x: number;
    y: number;
    width: number;
    height: number;
    coverage: number;
}

export interface CommunicationWebGLRankGeometry {
    rankY0: number;
    rankStep: number;
    rowHeight: number;
}

interface CommunicationWebGLRankGeometryParams {
    rankCount: number;
    visibleExtent: number[];
    axisExtent: number[];
    bandWidth: number;
    getRankY: (rankIndex: number) => number;
}

export const getCommunicationWebGLRankGeometry = ({
    rankCount,
    visibleExtent,
    axisExtent,
    bandWidth,
    getRankY,
}: CommunicationWebGLRankGeometryParams): CommunicationWebGLRankGeometry | null => {
    if (rankCount <= 0 || visibleExtent.length < 2 || axisExtent.length < 2) {
        return null;
    }
    const lastRankIndex = rankCount - 1;
    const visibleStart = Math.max(0, Math.min(lastRankIndex,
        Math.round(Math.min(visibleExtent[0], visibleExtent[1]))));
    const visibleEnd = Math.max(visibleStart, Math.min(lastRankIndex,
        Math.round(Math.max(visibleExtent[0], visibleExtent[1]))));
    const firstVisibleY = getRankY(visibleStart);
    if (!Number.isFinite(firstVisibleY)) {
        return null;
    }
    const axisDirection = Math.sign(axisExtent[1] - axisExtent[0]) || -1;
    const rowHeight = Math.max(Number.EPSILON, Math.abs(bandWidth));
    let rankStep = axisDirection * rowHeight;
    if (visibleEnd > visibleStart) {
        const secondVisibleY = getRankY(visibleStart + 1);
        if (Number.isFinite(secondVisibleY) && Math.abs(secondVisibleY - firstVisibleY) > Number.EPSILON) {
            rankStep = secondVisibleY - firstVisibleY;
        }
    }
    return {
        rankY0: firstVisibleY - visibleStart * rankStep,
        rankStep,
        rowHeight,
    };
};

export const getCommunicationWebGLOperatorRect = (
    operator: CommunicationWebGLOperator,
    layout: CommunicationWebGLLayout,
    devicePixelRatio = 1,
): CommunicationWebGLRectangle => {
    const xDuration = Math.max(Number.EPSILON, layout.xMax - layout.xMin);
    const safeDevicePixelRatio = Number.isFinite(devicePixelRatio) && devicePixelRatio > 0
        ? devicePixelRatio
        : 1;
    const minimumSize = 1 / safeDevicePixelRatio;
    const actualHeight = Math.max(0, layout.rowHeight) * 0.6 * (layout.isCompare ? 0.5 : 1);
    const operatorHeight = Math.max(minimumSize, actualHeight);
    const centerY = layout.rankY0 + operator.rankIndex * layout.rankStep;
    const y = layout.isCompare
        ? operator.source === CommunicationOperatorSource.COMPARISON
            ? centerY - operatorHeight
            : centerY + operatorHeight / 3
        : centerY - operatorHeight / 2;
    const x = layout.gridLeft + (operator.startTime - layout.xMin) / xDuration * layout.gridWidth;
    const endX = layout.gridLeft + (operator.endTime - layout.xMin) / xDuration * layout.gridWidth;
    const actualWidth = Math.max(0, endX - x);
    const width = Math.max(minimumSize, actualWidth);
    return {
        x,
        y,
        width,
        height: operatorHeight,
        coverage: Math.min(1, actualWidth / width) * Math.min(1, actualHeight / operatorHeight),
    };
};

const parseHexColor = (value: string): [number, number, number, number] => {
    const normalized = value.trim().replace('#', '');
    if (normalized.length !== 6) {
        return [0.32, 0.57, 1, 1];
    }
    return [
        Number.parseInt(normalized.slice(0, 2), 16) / 255,
        Number.parseInt(normalized.slice(2, 4), 16) / 255,
        Number.parseInt(normalized.slice(4, 6), 16) / 255,
        1,
    ];
};

const compileShader = (gl: WebGL2RenderingContext, type: number, source: string): WebGLShader => {
    const shader = gl.createShader(type);
    if (!shader) {
        throw new Error('Unable to create WebGL shader');
    }
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        const message = gl.getShaderInfoLog(shader) ?? 'Unknown WebGL shader error';
        gl.deleteShader(shader);
        throw new Error(message);
    }
    return shader;
};

const createProgram = (gl: WebGL2RenderingContext): WebGLProgram => {
    const vertexShader = compileShader(gl, gl.VERTEX_SHADER, VERTEX_SHADER);
    const fragmentShader = compileShader(gl, gl.FRAGMENT_SHADER, FRAGMENT_SHADER);
    const program = gl.createProgram();
    if (!program) {
        throw new Error('Unable to create WebGL program');
    }
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);
    gl.linkProgram(program);
    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
        const message = gl.getProgramInfoLog(program) ?? 'Unknown WebGL link error';
        gl.deleteProgram(program);
        throw new Error(message);
    }
    return program;
};

export class CommunicationTimeWebGLRenderer {
    readonly canvas: HTMLCanvasElement;
    readonly highlightCanvas: HTMLCanvasElement;
    private readonly gl: WebGL2RenderingContext;
    private readonly highlightContext: CanvasRenderingContext2D;
    private readonly program: WebGLProgram;
    private readonly vertexArray: WebGLVertexArrayObject;
    private readonly cornerBuffer: WebGLBuffer;
    private readonly instanceBuffer: WebGLBuffer;
    private readonly viewportLocation: WebGLUniformLocation;
    private instanceData = new Float32Array(INITIAL_INSTANCE_CAPACITY * FLOATS_PER_INSTANCE);
    private instanceCount = 0;
    private hoveredId = -1;
    private selectedId = -1;
    private readonly visibleRectangles = new Map<number, CommunicationWebGLRectangle>();
    private layout: CommunicationWebGLLayout | null = null;
    private colors: Array<[number, number, number, number]> = [];
    private disposed = false;
    private readonly contextLostHandler: (event: Event) => void;

    constructor(container: HTMLElement, onContextLost: () => void) {
        this.canvas = document.createElement('canvas');
        this.canvas.className = 'communication-time-webgl-layer';
        Object.assign(this.canvas.style, {
            position: 'absolute',
            inset: '0',
            width: '100%',
            height: '100%',
            pointerEvents: 'none',
            zIndex: '1',
        });
        this.highlightCanvas = document.createElement('canvas');
        this.highlightCanvas.className = 'communication-time-webgl-highlight-layer';
        Object.assign(this.highlightCanvas.style, {
            position: 'absolute',
            inset: '0',
            width: '100%',
            height: '100%',
            pointerEvents: 'none',
            zIndex: '2',
        });
        const highlightContext = this.highlightCanvas.getContext('2d');
        if (!highlightContext) {
            throw new Error('Canvas2D is not supported');
        }
        this.highlightContext = highlightContext;
        const gl = this.canvas.getContext('webgl2', {
            alpha: true,
            antialias: false,
            depth: false,
            premultipliedAlpha: true,
            preserveDrawingBuffer: false,
        });
        if (!gl) {
            throw new Error('WebGL2 is not supported');
        }
        this.gl = gl;
        this.program = createProgram(gl);
        const vertexArray = gl.createVertexArray();
        const cornerBuffer = gl.createBuffer();
        const instanceBuffer = gl.createBuffer();
        const viewportLocation = gl.getUniformLocation(this.program, 'uViewport');
        if (!vertexArray || !cornerBuffer || !instanceBuffer || !viewportLocation) {
            throw new Error('Unable to allocate WebGL resources');
        }
        this.vertexArray = vertexArray;
        this.cornerBuffer = cornerBuffer;
        this.instanceBuffer = instanceBuffer;
        this.viewportLocation = viewportLocation;
        this.bindAttributes();
        this.contextLostHandler = (event: Event): void => {
            event.preventDefault();
            onContextLost();
        };
        this.canvas.addEventListener('webglcontextlost', this.contextLostHandler);
        if (getComputedStyle(container).position === 'static') {
            container.style.position = 'relative';
        }
        container.appendChild(this.canvas);
        container.appendChild(this.highlightCanvas);
    }

    private bindAttributes(): void {
        const gl = this.gl;
        gl.bindVertexArray(this.vertexArray);
        gl.bindBuffer(gl.ARRAY_BUFFER, this.cornerBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
            0, 0, 1, 0, 0, 1,
            0, 1, 1, 0, 1, 1,
        ]), gl.STATIC_DRAW);
        const cornerLocation = gl.getAttribLocation(this.program, 'aCorner');
        gl.enableVertexAttribArray(cornerLocation);
        gl.vertexAttribPointer(cornerLocation, 2, gl.FLOAT, false, 0, 0);

        gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
        const stride = FLOATS_PER_INSTANCE * Float32Array.BYTES_PER_ELEMENT;
        const attributes = [
            { name: 'aRect', size: 4, offset: 0 },
            { name: 'aColor', size: 4, offset: 4 * Float32Array.BYTES_PER_ELEMENT },
        ];
        attributes.forEach(attribute => {
            const location = gl.getAttribLocation(this.program, attribute.name);
            gl.enableVertexAttribArray(location);
            gl.vertexAttribPointer(location, attribute.size, gl.FLOAT, false, stride, attribute.offset);
            gl.vertexAttribDivisor(location, 1);
        });
        gl.bindVertexArray(null);
        gl.bindBuffer(gl.ARRAY_BUFFER, null);
    }

    setColors(colors: string[]): void {
        this.colors = colors.map(parseHexColor);
    }

    setData(operators: CommunicationWebGLOperator[], layout: CommunicationWebGLLayout): void {
        this.layout = layout;
        this.ensureCapacity(operators.length);
        this.visibleRectangles.clear();
        const devicePixelRatio = window.devicePixelRatio || 1;
        operators.forEach((operator, index) => {
            const rect = getCommunicationWebGLOperatorRect(operator, layout, devicePixelRatio);
            this.visibleRectangles.set(operator.id, rect);
            const color = this.colors[operator.colorIndex] ?? [0.32, 0.57, 1, 1];
            const offset = index * FLOATS_PER_INSTANCE;
            this.instanceData[offset] = rect.x;
            this.instanceData[offset + 1] = rect.y;
            this.instanceData[offset + 2] = rect.width;
            this.instanceData[offset + 3] = rect.height;
            this.instanceData[offset + 4] = color[0];
            this.instanceData[offset + 5] = color[1];
            this.instanceData[offset + 6] = color[2];
            this.instanceData[offset + 7] = color[3] * rect.coverage;
        });
        this.instanceCount = operators.length;
        const gl = this.gl;
        gl.bindBuffer(gl.ARRAY_BUFFER, this.instanceBuffer);
        gl.bufferData(gl.ARRAY_BUFFER, this.instanceData.subarray(0, operators.length * FLOATS_PER_INSTANCE), gl.DYNAMIC_DRAW);
        gl.bindBuffer(gl.ARRAY_BUFFER, null);
        this.draw();
    }

    setHoveredOperator(operatorId: number | null): void {
        const nextId = operatorId ?? -1;
        if (nextId === this.hoveredId) {
            return;
        }
        this.hoveredId = nextId;
        this.drawHighlights();
    }

    setSelectedOperator(operatorId: number | null): void {
        const nextId = operatorId ?? -1;
        if (nextId === this.selectedId) {
            return;
        }
        this.selectedId = nextId;
        this.drawHighlights();
    }

    clear(): void {
        this.instanceCount = 0;
        this.visibleRectangles.clear();
        this.draw();
    }

    private ensureCapacity(instanceCount: number): void {
        const requiredLength = instanceCount * FLOATS_PER_INSTANCE;
        if (requiredLength <= this.instanceData.length) {
            return;
        }
        let nextLength = this.instanceData.length;
        while (nextLength < requiredLength) {
            nextLength *= 2;
        }
        this.instanceData = new Float32Array(nextLength);
    }

    private resizeCanvas(layout: CommunicationWebGLLayout): number {
        const devicePixelRatio = window.devicePixelRatio || 1;
        const width = Math.max(1, Math.round(layout.canvasWidth * devicePixelRatio));
        const height = Math.max(1, Math.round(layout.canvasHeight * devicePixelRatio));
        if (this.canvas.width !== width || this.canvas.height !== height) {
            this.canvas.width = width;
            this.canvas.height = height;
        }
        if (this.highlightCanvas.width !== width || this.highlightCanvas.height !== height) {
            this.highlightCanvas.width = width;
            this.highlightCanvas.height = height;
        }
        return devicePixelRatio;
    }

    private draw(): void {
        if (this.disposed || !this.layout || this.gl.isContextLost()) {
            return;
        }
        const gl = this.gl;
        const layout = this.layout;
        const devicePixelRatio = this.resizeCanvas(layout);
        gl.viewport(0, 0, this.canvas.width, this.canvas.height);
        gl.clearColor(0, 0, 0, 0);
        gl.clear(gl.COLOR_BUFFER_BIT);
        if (this.instanceCount === 0) {
            this.drawHighlights(devicePixelRatio);
            return;
        }
        gl.enable(gl.SCISSOR_TEST);
        gl.scissor(
            Math.round(layout.gridLeft * devicePixelRatio),
            Math.round((layout.canvasHeight - layout.gridTop - layout.gridHeight) * devicePixelRatio),
            Math.round(layout.gridWidth * devicePixelRatio),
            Math.round(layout.gridHeight * devicePixelRatio),
        );
        gl.enable(gl.BLEND);
        gl.blendFuncSeparate(
            gl.SRC_ALPHA,
            gl.ONE_MINUS_SRC_ALPHA,
            gl.ONE,
            gl.ONE_MINUS_SRC_ALPHA,
        );
        gl.useProgram(this.program);
        gl.uniform2f(this.viewportLocation, layout.canvasWidth, layout.canvasHeight);
        gl.bindVertexArray(this.vertexArray);
        gl.drawArraysInstanced(gl.TRIANGLES, 0, 6, this.instanceCount);
        gl.bindVertexArray(null);
        gl.disable(gl.BLEND);
        gl.disable(gl.SCISSOR_TEST);
        this.drawHighlights(devicePixelRatio);
    }

    private drawHighlights(devicePixelRatio = window.devicePixelRatio || 1): void {
        if (this.disposed || !this.layout) {
            return;
        }
        const context = this.highlightContext;
        const layout = this.layout;
        context.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
        context.clearRect(0, 0, layout.canvasWidth, layout.canvasHeight);
        const operatorIds = this.hoveredId === this.selectedId
            ? [this.selectedId]
            : [this.selectedId, this.hoveredId];
        if (operatorIds.every(operatorId => !this.visibleRectangles.has(operatorId))) {
            return;
        }
        context.save();
        context.beginPath();
        context.rect(layout.gridLeft, layout.gridTop, layout.gridWidth, layout.gridHeight);
        context.clip();
        context.strokeStyle = HIGHLIGHT_STROKE;
        context.lineWidth = HIGHLIGHT_LINE_WIDTH;
        context.shadowBlur = HIGHLIGHT_SHADOW_BLUR;
        context.shadowColor = HIGHLIGHT_SHADOW;
        operatorIds.forEach(operatorId => {
            const rect = this.visibleRectangles.get(operatorId);
            if (rect) {
                context.strokeRect(rect.x, rect.y, rect.width, rect.height);
            }
        });
        context.restore();
    }

    dispose(): void {
        if (this.disposed) {
            return;
        }
        this.disposed = true;
        this.canvas.removeEventListener('webglcontextlost', this.contextLostHandler);
        this.gl.deleteBuffer(this.cornerBuffer);
        this.gl.deleteBuffer(this.instanceBuffer);
        this.gl.deleteVertexArray(this.vertexArray);
        this.gl.deleteProgram(this.program);
        this.canvas.remove();
        this.highlightCanvas.remove();
    }
}

export const createCommunicationTimeWebGLRenderer = (
    container: HTMLElement,
    onContextLost: () => void,
): CommunicationTimeWebGLRenderer | null => {
    try {
        return new CommunicationTimeWebGLRenderer(container, onContextLost);
    } catch {
        container.querySelector('.communication-time-webgl-layer')?.remove();
        container.querySelector('.communication-time-webgl-highlight-layer')?.remove();
        return null;
    }
};
