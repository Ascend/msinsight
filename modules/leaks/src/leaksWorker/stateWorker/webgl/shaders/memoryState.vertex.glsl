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

layout(location = 0) in float x;
layout(location = 1) in float y;
layout(location = 2) in float width;
layout(location = 3) in float colorIndex;

uniform vec2 uScale;
uniform vec2 uTranslate;
uniform vec2 uResolution;
uniform vec2 uZoom;
uniform vec4 uColors[COLOR_COUNT];

out vec4 vColor;

const float HEIGHT = 40.0f;

void main() {
    float lx = x * uScale.x * uZoom.x + uTranslate.x;
    float ly = y * uScale.y * uZoom.y + uTranslate.y;
    float rx = max((x + width) * uScale.x * uZoom.x + uTranslate.x, lx + 1.0f);
    float ry = y * uScale.y * uZoom.y + uTranslate.y;
    float h = HEIGHT * uScale.y * uZoom.y;

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
    gl_Position = vec4(clipPos.x, -clipPos.y, 0.0f, 1.0f);
    int index = min(int(colorIndex), COLOR_COUNT - 1);
    vColor = uColors[index];
}
