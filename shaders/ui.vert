#version 450

layout(push_constant) uniform PushConsts {
    vec2 screenSize;
} pushConsts;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
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
layout(location = 12) in vec4 inStrokeThickness;
layout(location = 13) in uint inShapeType;
layout(location = 14) in uint inFillType;
layout(location = 15) in uint inTextureIndex;
layout(location = 16) in vec4 inUvBounds;
layout(location = 17) in float inBlur;
layout(location = 18) in float inScale;
layout(location = 19) in float inRotation;
layout(location = 20) in float inFontWeight; // <--- Font Weight In

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
layout(location = 12) flat out vec4 outStrokeThickness;
layout(location = 13) flat out uint outShapeType;
layout(location = 14) flat out uint outFillType;
layout(location = 15) flat out uint outTextureIndex;
layout(location = 16) flat out vec4 outUvBounds;
layout(location = 17) flat out float outBlur;
layout(location = 18) flat out float outScale;
layout(location = 19) flat out float outRotation;
layout(location = 20) flat out float outFontWeight; // <--- Font Weight Out

void main() {
    vec2 center = inRectXYWH.xy + inRectXYWH.zw * 0.5;
    vec2 localOffset = inPosition * inRectXYWH.zw;
    vec2 fromCenter = localOffset - (inRectXYWH.zw * 0.5);
    
    float c = cos(inRotation);
    float s = sin(inRotation);
    mat2 rot = mat2(c, s, -s, c);
    vec2 transformedOffset = rot * (fromCenter * inScale);
    
    vec2 finalPos = center + transformedOffset;
    
    vec2 ndc = (finalPos / pushConsts.screenSize) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);

    outPixelPos = finalPos;
    outUV = mix(inUvBounds.xy, inUvBounds.zw, inUV);
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
    outScale = inScale;
    outRotation = inRotation;
    outFontWeight = inFontWeight; // <--- Pass through
}
