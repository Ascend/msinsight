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

layout(location = 0) in vec2 p0;
layout(location = 1) in vec2 p1;
layout(location = 2) in float height;

uniform vec2 uScale;
uniform vec2 uTranslate;
uniform vec2 uResolution;
uniform vec2 uZoom;
uniform float uOffset;

void main() {
    vec2 start = (p0 - vec2(uOffset, 0.0)) * uScale * uZoom + uTranslate;
    vec2 end   = (p1 - vec2(uOffset, 0.0)) * uScale * uZoom + uTranslate;
    float h = height * uScale.y * uZoom.y;

    vec2 pos;
    int idx = gl_VertexID % 4;
    switch(idx) {
        case 0: {
            pos = start;
            break;
        }
        case 1: {
            pos = end;
            break;
        }
        case 2: {
            pos = start + vec2(0.0, h);
            break;
        }
        default: {
            pos = end + vec2(0.0, h);
            break;
        }
    }

    vec2 clipPos = (pos / uResolution) * 2.0f - 1.0f;
    gl_Position = vec4(clipPos.x, clipPos.y, 0.0f, 1.0f);
}
