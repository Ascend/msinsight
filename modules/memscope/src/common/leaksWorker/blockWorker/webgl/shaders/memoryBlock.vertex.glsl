#version 300 es
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

precision highp float;

const int COLOR_COUNT = 10;

layout(location = 0) in vec2 p0;
layout(location = 1) in vec2 p1;
layout(location = 2) in float height;
layout(location = 3) in float colorIndex;

uniform vec2 uScale;
uniform vec2 uTranslate;
uniform vec2 uResolution;
uniform vec2 uZoom;
uniform float uOffset;
uniform vec4 uColors[COLOR_COUNT];

out vec4 vColor;

void main() {
    float lx = (p0.x - uOffset) * uScale.x * uZoom.x + uTranslate.x;
    float ly = (p0.y) * uScale.y * uZoom.y + uTranslate.y;
    float rx = max((p1.x - uOffset) * uScale.x * uZoom.x + uTranslate.x, lx + 1.0f);
    float ry = (p1.y) * uScale.y * uZoom.y + uTranslate.y;
    float h = height * uScale.y * uZoom.y;

    vec2 pos;
    int idx = gl_VertexID % 6;
    switch(idx) {
        case 0: {
            pos = vec2(lx, ly);
            break;
        }
        case 1: {
            pos = vec2(lx, ly + h);
            break;
        }
        case 2: {
            pos = vec2(rx, ry);
            break;
        }
        case 3: {
            pos = vec2(lx, ly + h);
            break;
        }
        case 4: {
            pos = vec2(rx, ry + h);
            break;
        }
        default: {
            pos = vec2(rx, ry);
            break;
        }
    }

    vec2 clipPos = (pos / uResolution) * 2.0f - 1.0f;
    gl_Position = vec4(clipPos.x, clipPos.y, 0.0f, 1.0f);
    int index = min(int(colorIndex), COLOR_COUNT - 1);
    vColor = uColors[index];
}
