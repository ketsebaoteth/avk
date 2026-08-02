#version 450
#extension GL_EXT_nonuniform_qualifier : enable

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
layout(binding = 0) uniform sampler2D globalTextures[];

// OKLab Perceptual Color Mixing
vec3 srgb_to_oklab(vec3 c) {
    mat3 m1 = mat3(
        0.4122214708, 0.5363325363, 0.0514459929,
        0.2119034982, 0.6806995451, 0.1073969566,
        0.0883024619, 0.2817188376, 0.6299787005
    );
    vec3 lms = pow(max(m1 * c, vec3(0.0)), vec3(1.0/3.0));
    mat3 m2 = mat3(
        0.2104542553, 0.7936177850, -0.0040720468,
        1.9779984951, -2.4285922050, 0.4505937099,
        0.0259040371, 0.7827717662, -0.8086757660
    );
    return m2 * lms;
}

vec3 oklab_to_srgb(vec3 c) {
    mat3 m1 = mat3(
        1.0, 0.3963377774, 0.2158037573,
        1.0, -0.1055613458, -0.0638541728,
        1.0, -0.0894841775, -1.2914855480
    );
    vec3 lms = m1 * c;
    lms = lms * lms * lms;
    mat3 m2 = mat3(
        4.0767416621, -3.3077115913, 0.2309699292,
        -1.2684380046, 2.6097574011, -0.3413193965,
        -0.0041960863, -0.7034186147, 1.7076147010
    );
    return m2 * lms;
}

float sdRoundedBox(vec2 p, vec2 b, vec4 r) {
    float radius = (p.x > 0.0) ? ((p.y < 0.0) ? r.y : r.w) : ((p.y < 0.0) ? r.x : r.z);
    vec2 q = abs(p) - b + vec2(radius);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

void main() {
    // 1. Scissor Clip Check
    if (inPixelPos.x < inClipRect.x || inPixelPos.y < inClipRect.y ||
        inPixelPos.x > inClipRect.z || inPixelPos.y > inClipRect.w) {
        discard;
    }

    // ------------------------------------------------------------------------
    // FAST-PATH 1: MTSDF Vector Text Rendering (>80% of all submitted quads)
    // Bypasses all SDF rounded box math, rotations, and gradient calculations!
    // ------------------------------------------------------------------------
    if (inFillType == 3) {
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
        return;
    }

    // ------------------------------------------------------------------------
    // FAST-PATH 2: Standard Image / Emoji Texture Sampling
    // ------------------------------------------------------------------------
    if (inFillType == 7) {
        vec2 safeUV = clamp(inUV, inUvBounds.xy, inUvBounds.zw);
        outColor = texture(globalTextures[nonuniformEXT(inTextureIndex)], safeUV) * inFillColorA;
        if (outColor.a < 0.001) discard;
        return;
    }

    // ------------------------------------------------------------------------
    // Standard Shape & Box Rendering Path
    // ------------------------------------------------------------------------
    vec2 exactPixelPos = floor(inPixelPos) + 0.5;
    vec2 center = inRectXYWH.xy + inRectXYWH.zw * 0.5;
    vec2 halfSize = inRectXYWH.zw * 0.5;
    vec2 pScreen = exactPixelPos - center;
    
    vec2 p = pScreen;
    if (inRotation != 0.0) {
        float c = cos(inRotation);
        float s = sin(inRotation);
        p = mat2(c, s, -s, c) * pScreen;
    }
    if (inScale != 1.0) {
        p /= max(inScale, 0.0001);
    }

    float d = sdRoundedBox(p, halfSize, inBorderRadius);

    vec4 fillColor = inFillColorA;
    if (inFillType == 1) { 
        // OKLab 2-Stop Linear Gradient
        vec2 dir = inGradientEnd - inGradientStart;
        float lenSq = dot(dir, dir);
        if (lenSq > 0.0001) {
            vec2 normCoord = (p + halfSize) / inRectXYWH.zw;
            float t = clamp(dot(normCoord - inGradientStart, dir) / lenSq, 0.0, 1.0);
            vec3 labMix = mix(srgb_to_oklab(inFillColorA.rgb), srgb_to_oklab(inFillColorB.rgb), t);
            fillColor.rgb = oklab_to_srgb(labMix);
            fillColor.a = mix(inFillColorA.a, inFillColorB.a, t);
        }
    } else if (inFillType == 8) { 
        // Multi-Stop 1D Texture Atlas Gradient
        vec2 dir = inGradientEnd - inGradientStart;
        float lenSq = dot(dir, dir);
        if (lenSq > 0.0001) {
            vec2 normCoord = (p + halfSize) / inRectXYWH.zw;
            float t = clamp(dot(normCoord - inGradientStart, dir) / lenSq, 0.0, 1.0);
            fillColor = texture(globalTextures[nonuniformEXT(inTextureIndex)], vec2(t, 0.5));
        }
    } else if (inFillType == 5) {
        // Outset Box-Shadow
        float spread = inFillColorB.z;
        float expand = inFillColorB.w;
        vec2 cardHalfSize = halfSize - vec2(expand) + vec2(spread);
        float shadowD = sdRoundedBox(p, cardHalfSize, inBorderRadius + vec4(spread));
        float blurRadius = max(inBlur, 0.001);
        float shadowAlpha = shadowD <= 0.0 ? 1.0 : exp(-max(shadowD, 0.0) * max(shadowD, 0.0) / (0.5 * blurRadius * blurRadius));
        outColor = vec4(inFillColorA.rgb, inFillColorA.a * shadowAlpha);
        if (outColor.a < 0.001) discard;
        return;
    }

    vec2 dGrad = vec2(dFdx(d), dFdy(d));
    float gradientLen = max(length(dGrad), 0.0001);
    float aaWidth = (inBlur > 0.0) ? (0.75 + inBlur) * max(inScale, 0.0001) : (gradientLen * 0.45); 

    bool isAxisAligned = (abs(dGrad.x) < 0.001 || abs(dGrad.y) < 0.001) && (inRotation == 0.0);
    float alpha = (isAxisAligned && inBlur == 0.0) ? ((d <= 0.001) ? 1.0 : 0.0) : smoothstep(aaWidth, -aaWidth, d);

    float maxStroke = max(max(inStrokeThickness.x, inStrokeThickness.y), max(inStrokeThickness.z, inStrokeThickness.w));
    if (maxStroke > 0.0) {
        float strokeThick = round(inStrokeThickness.x);
        float innerAlpha = smoothstep(aaWidth, -aaWidth, d + strokeThick);
        vec4 bg = vec4(fillColor.rgb * fillColor.a, fillColor.a) * alpha;
        float strokeMask = clamp(alpha - innerAlpha, 0.0, 1.0);
        vec4 fg = vec4(inStrokeColor.rgb * inStrokeColor.a, inStrokeColor.a) * strokeMask;
        vec4 finalColor = fg + bg * (1.0 - fg.a);
        outColor = (finalColor.a > 0.0001) ? vec4(finalColor.rgb / finalColor.a, finalColor.a) : vec4(0.0);
    } else {
        outColor = vec4(fillColor.rgb, fillColor.a * alpha);
    }

    if (outColor.a < 0.001) discard;
}
