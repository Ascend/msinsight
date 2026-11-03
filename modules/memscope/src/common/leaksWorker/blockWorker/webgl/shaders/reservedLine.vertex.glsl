#version 300 es
precision highp float;
layout(location = 0) in vec2 aPosition;
uniform vec2 uScale;
uniform vec2 uTranslate;
uniform vec2 uResolution;
uniform vec2 uZoom;
uniform float uOffset;

void main() {
    vec2 position = vec2((aPosition.x - uOffset) * uZoom.x, aPosition.y * uZoom.y);
    vec2 screenPosition = position * uScale + uTranslate;
    vec2 clipPos = (screenPosition / uResolution) * 2.0 - 1.0;
    gl_Position = vec4(clipPos.x, clipPos.y, 0.0, 1.0);
}
