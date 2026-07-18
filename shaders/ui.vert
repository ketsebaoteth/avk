#version 450

// Vertex inputs
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

// Instance inputs
layout(location = 2) in vec4 inRectXYWH;
layout(location = 3) in vec4 inBorderRadius;
layout(location = 4) in vec4 inFillColorA;
layout(location = 5) in vec4 inFillColorB;
layout(location = 6) in vec4 inStrokeColor;
layout(location = 7) in vec2 inGradientStart;
layout(location = 8) in vec2 inGradientEnd;
layout(location = 9) in vec4 inStrokeFillColorA;
layout(location = 10) in vec4 inStrokeFillColorB;
layout(location = 11) in vec4 inClipRect;
layout(location = 12) in float inStrokeThickness;
layout(location = 13) in uint inShapeType;
layout(location = 14) in uint inFillType;
layout(location = 15) in uint inTextureIndex;
layout(location = 16) in vec4 inUvBounds;
layout(location = 17) in float inBlur;

// Outputs to fragment shader
layout(location = 0) out vec2 outPixelPos;
layout(location = 1) out vec2 outUV;
layout(location = 2) flat out vec4 outRectXYWH;
layout(location = 3) flat out vec4 outBorderRadius;
layout(location = 4) flat out vec4 outFillColorA;
layout(location = 5) flat out vec4 outFillColorB;
layout(location = 6) flat out vec4 outStrokeColor;
layout(location = 7) flat out vec2 outGradientStart;
layout(location = 8) flat out vec2 outGradientEnd;
layout(location = 9) flat out vec4 outStrokeFillColorA;
layout(location = 10) flat out vec4 outStrokeFillColorB;
layout(location = 11) flat out vec4 outClipRect;
layout(location = 12) flat out float outStrokeThickness;
layout(location = 13) flat out uint outShapeType;
layout(location = 14) flat out uint outFillType;
layout(location = 15) flat out uint outTextureIndex;
layout(location = 16) flat out vec4 outUvBounds;
layout(location = 17) flat out float outBlur;

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} push;

void main() {
    // 1. Calculate local coordinates mapped to instance size
    vec2 localPos = inPos * inRectXYWH.zw;
    vec2 globalPos = inRectXYWH.xy + localPos;

    // 2. Translate pixel positions to Vulkan NDC: [0, width] -> [-1, 1], [0, height] -> [-1, 1] (and flip Y)
    vec2 ndcPos = (globalPos / push.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndcPos.x, ndcPos.y, 0.0, 1.0);

    // 3. Map parameters to interpolate downstream
    outPixelPos = globalPos;
    outUV = inUvBounds.xy + inUV * (inUvBounds.zw - inUvBounds.xy);

    outRectXYWH = inRectXYWH;
    outBorderRadius = inBorderRadius;
    outFillColorA = inFillColorA;
    outFillColorB = inFillColorB;
    outStrokeColor = inStrokeColor;
    outGradientStart = inGradientStart;
    outGradientEnd = inGradientEnd;
    outStrokeFillColorA = inStrokeFillColorA;
    outStrokeFillColorB = inStrokeFillColorB;
    outClipRect = inClipRect;
    outStrokeThickness = inStrokeThickness;
    outShapeType = inShapeType;
    outFillType = inFillType;
    outTextureIndex = inTextureIndex;
    outUvBounds = inUvBounds;
    outBlur = inBlur;
}