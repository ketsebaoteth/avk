#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D globalTextures[];

// ⚡ LOCATIONS 0 THROUGH 20 MATCH ui.vert EXACTLY
layout(location = 0) in vec2 inPixelPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) flat in vec4 inRectXYWH;
layout(location = 3) flat in vec4 inBorderRadius;
layout(location = 4) flat in vec4 inFillColorA;
layout(location = 5) flat in vec4 inFillColorB;
layout(location = 6) flat in vec4 inStrokeColor;
layout(location = 7) flat in vec2 inGradientStart;
layout(location = 8) flat in vec2 inGradientEnd;
layout(location = 9) flat in vec4 inStrokeFillColorA;
layout(location = 10) flat in vec4 inStrokeFillColorB;
layout(location = 11) flat in vec4 inClipRect;
layout(location = 12) flat in vec4 inStrokeThickness;
layout(location = 13) flat in uint inShapeType;
layout(location = 14) flat in uint inFillType;
layout(location = 15) flat in uint inTextureIndex; 
layout(location = 16) flat in vec4 inUvBounds;
layout(location = 17) flat in float inBlur;
layout(location = 18) flat in float inScale;
layout(location = 19) flat in float inRotation;
layout(location = 20) flat in float inFontWeight;

layout(location = 0) out vec4 outColor;

void main() {
    // 1. Scissor Clip Check
    if (inPixelPos.x < inClipRect.x || inPixelPos.y < inClipRect.y ||
        inPixelPos.x > inClipRect.z || inPixelPos.y > inClipRect.w) {
        discard;
    }

    vec2 safeUV = clamp(inUV, inUvBounds.xy, inUvBounds.zw);
    vec4 msd = texture(globalTextures[nonuniformEXT(inTextureIndex)], safeUV);

    float sd = max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
    float trueSDF = msd.a;

    if (abs(sd - 0.5) > abs(trueSDF - 0.5) + 0.05) {
        sd = trueSDF;
    }

    float actualWeight = (inFontWeight > 10.0) ? inFontWeight : 400.0;
    sd += (actualWeight - 400.0) * 0.0003;

    vec2 dx = dFdx(safeUV);
    vec2 dy = dFdy(safeUV);
    vec2 screenTexSize = inversesqrt(dx * dx + dy * dy);
    float screenPxRange = max(0.5 * dot(vec2(8.0) / vec2(textureSize(globalTextures[nonuniformEXT(inTextureIndex)], 0)), screenTexSize), 1.0);

    float screenPxDistance = screenPxRange * (sd - 0.5);
    float opacity = clamp(screenPxDistance * 1.25 + 0.5, 0.0, 1.0);

    float finalAlpha = (inFillColorA.a > 0.01) ? inFillColorA.a : 1.0;
    outColor = vec4(inFillColorA.rgb, finalAlpha * opacity);
    
    if (outColor.a < 0.001) discard;
}
