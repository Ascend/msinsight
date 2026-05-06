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

layout(location = 0) in float x;
layout(location = 1) in float y;
layout(location = 2) in float width;

uniform vec2 uScale;
uniform vec2 uTranslate;
uniform vec2 uResolution;
uniform vec2 uZoom;
uniform vec4 uBorderColor;

out vec4 vColor;

const float HEIGHT = 40.0f;

void main() {
    float vx = x * uScale.x * uZoom.x + uTranslate.x;
    float vy = y * uScale.y * uZoom.y + uTranslate.y;
    float w = max(width * uScale.x * uZoom.x, 1.0f);
    float h = HEIGHT * uScale.y * uZoom.y;

    vec2 pos;
    int idx = gl_VertexID % 8; // 每个实例 8 个顶点

    switch(idx) {
        case 0: // 左下
            pos = vec2(vx, vy);
            break;
        case 1: // 左上
            pos = vec2(vx, vy + h);
            break;
        case 2: // 上边起点（与左下相同）
            pos = vec2(vx, vy);
            break;
        case 3: // 右下
            pos = vec2(vx + w, vy);
            break;
        case 4: // 右边起点（与右下相同）
            pos = vec2(vx + w, vy);
            break;
        case 5: // 右上
            pos = vec2(vx + w, vy + h);
            break;
        case 6: // 下边起点（与左上相同）
            pos = vec2(vx, vy + h);
            break;
        case 7: // 右上（与前面重复，实际应为下边终点）
            pos = vec2(vx + w, vy + h);
            break;
    }

    vec2 clipPos = (pos / uResolution) * 2.0f - 1.0f;
    gl_Position = vec4(clipPos.x, -clipPos.y, 0.0f, 1.0f);
    vColor = uBorderColor;
}
