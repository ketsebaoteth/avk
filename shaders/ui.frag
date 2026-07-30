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

// OKLab Perceptual Non-Muddy Color Mixing Functions
vec3 srgb_to_oklab(vec3 c) {
    mat3 m1 = mat3(
        0.4122214708, 0.5363325363, 0.0514459929,
        0.2119034982, 0.6806995451, 0.1073969566,
        0.0883024619, 0.2817188376, 0.6299787005
    );
    vec3 lms = m1 * c;
    lms = pow(max(lms, vec3(0.0)), vec3(1.0/3.0));
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
    float radius = r.x; 
    if (p.x > 0.0 && p.y < 0.0) radius = r.y; 
    else if (p.x < 0.0 && p.y > 0.0) radius = r.z; 
    else if (p.x > 0.0 && p.y > 0.0) radius = r.w; 
    
    vec2 q = abs(p) - b + vec2(radius);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float sdCircle(vec2 p, float r) { return length(p) - r; }

float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0);
    return length(pa - ba*h);
}

float sdRegularPolygon(vec2 p, float r, int n) {
    float an = 3.14159265358979323846 / float(n);
    float he = r * cos(an);
    float angle = atan(p.y, p.x) + 3.14159265358979323846;
    float sector = floor(angle / (2.0 * an));
    float a = sector * 2.0 * an + an - 3.14159265358979323846;
    vec2 p_rot = vec2(cos(a) * p.x + sin(a) * p.y, -sin(a) * p.x + cos(a) * p.y);
    return p_rot.x - he;
}

void main() {
    if (inPixelPos.x < inClipRect.x || inPixelPos.y < inClipRect.y ||
        inPixelPos.x > inClipRect.z || inPixelPos.y > inClipRect.w) {
        discard;
    }

    vec2 exactPixelPos = floor(inPixelPos) + 0.5;

    vec2 center = inRectXYWH.xy + inRectXYWH.zw * 0.5;
    vec2 halfSize = inRectXYWH.zw * 0.5;
    
    vec2 pScreen = exactPixelPos - center;
    
    float c = cos(inRotation);
    float s = sin(inRotation);
    mat2 invRot = mat2(c, s, -s, c);
    vec2 p = (invRot * pScreen) / max(inScale, 0.0001);

    float d = 0.0;
    
    if (inShapeType == 0) { 
        d = sdRoundedBox(p, halfSize, inBorderRadius);
    } else if (inShapeType == 1) { 
        d = sdCircle(p, min(halfSize.x, halfSize.y));
    } else if (inShapeType == 2) { 
        vec2 localA = invRot * (inRectXYWH.xy - center) / max(inScale, 0.0001);
        vec2 localB = invRot * (inRectXYWH.zw - center) / max(inScale, 0.0001);
        d = sdSegment(p, localA, localB) - inStrokeThickness.x * 0.5;
    } else if (inShapeType == 3) { 
        int numSides = int(max(float(inTextureIndex), 3.0));
        d = sdRegularPolygon(p, min(halfSize.x, halfSize.y), numSides);
    }

    vec4 fillColor = inFillColorA;
    if (inFillType == 1) { 
        // OKLab 2-Stop Linear Gradient
        vec2 dir = inGradientEnd - inGradientStart;
        float lenSq = dot(dir, dir);
        if (lenSq > 0.0001) {
            vec2 localPixelPos = p + halfSize;
            vec2 normCoord = localPixelPos / inRectXYWH.zw;
            float t = clamp(dot(normCoord - inGradientStart, dir) / lenSq, 0.0, 1.0);
            
            vec3 labA = srgb_to_oklab(inFillColorA.rgb);
            vec3 labB = srgb_to_oklab(inFillColorB.rgb);
            vec3 labMix = mix(labA, labB, t);
            
            fillColor.rgb = oklab_to_srgb(labMix);
            fillColor.a = mix(inFillColorA.a, inFillColorB.a, t);
        }
    } else if (inFillType == 2) { 
        // OKLab Radial Gradient
        vec2 normCenter = inGradientStart;
        float dist = distance(p / halfSize, (normCenter - vec2(0.5)) * 2.0);
        float t = clamp(dist, 0.0, 1.0);
        
        vec3 labA = srgb_to_oklab(inFillColorA.rgb);
        vec3 labB = srgb_to_oklab(inFillColorB.rgb);
        vec3 labMix = mix(labA, labB, t);
        
        fillColor.rgb = oklab_to_srgb(labMix);
        fillColor.a = mix(inFillColorA.a, inFillColorB.a, t);
    } else if (inFillType == 8) { 
        // Multi-Stop 1D Texture Atlas Gradient (Handles 3, 7, 20+ stops!)
        vec2 dir = inGradientEnd - inGradientStart;
        float lenSq = dot(dir, dir);
        if (lenSq > 0.0001) {
            vec2 localPixelPos = p + halfSize;
            vec2 normCoord = localPixelPos / inRectXYWH.zw;
            float t = clamp(dot(normCoord - inGradientStart, dir) / lenSq, 0.0, 1.0);

            // Sample 1D Gradient Atlas Texture!
            fillColor = texture(globalTextures[nonuniformEXT(inTextureIndex)], vec2(t, 0.5));
        }
    } else if (inFillType == 3) {
        // MTSDF Text Rendering
        vec2 safeUV = clamp(inUV, inUvBounds.xy, inUvBounds.zw);
        vec4 msd = texture(globalTextures[nonuniformEXT(inTextureIndex)], safeUV);

        float sd = max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
        float trueSDF = msd.a;

        if (abs(sd - 0.5) > abs(trueSDF - 0.5) + 0.05) {
            sd = trueSDF;
        }

        float weightShift = (inFontWeight - 400.0) * 0.0003;
        sd += weightShift;

        const float pxRange = 9.0;

        vec2 unitRange = vec2(pxRange) / vec2(textureSize(globalTextures[nonuniformEXT(inTextureIndex)], 0));
        vec2 dx = dFdx(safeUV);
        vec2 dy = dFdy(safeUV);
        vec2 screenTexSize = inversesqrt(dx*dx + dy*dy);
        float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

        float screenPxDistance = screenPxRange * (sd - 0.5);

        float opacity = clamp(screenPxDistance * 1.25 + 0.5, 0.0, 1.0);

        outColor = vec4(inFillColorA.rgb, inFillColorA.a * opacity);
        if (outColor.a < 0.001) discard;
        return;
    } else if (inFillType == 4) { 
        // Image Rendering
        vec2 uv = mix(inUvBounds.xy, inUvBounds.zw, inUV);
        vec4 texColor = (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) 
            ? vec4(0.0) 
            : texture(globalTextures[nonuniformEXT(inTextureIndex)], uv);
        
        vec4 finalColor = texColor * inFillColorB;
        float aaWidth = (0.75 + inBlur) * max(inScale, 0.0001);
        float alpha = 1.0 - smoothstep(-aaWidth, aaWidth, d);

        outColor = vec4(finalColor.rgb, finalColor.a * alpha);
        if (outColor.a < 0.001) discard;
        return;
    } else if (inFillType == 5) {
        // Outset Box-Shadow
        float spread = inFillColorB.z;
        float expand = inFillColorB.w;
        vec2 cardHalfSize = (inRectXYWH.zw * 0.5) - vec2(expand) + vec2(spread);
        vec4 shadowRadius = inBorderRadius + vec4(spread);

        float shadowD = sdRoundedBox(p, cardHalfSize, shadowRadius);
        float blurRadius = max(inBlur, 0.001);
        float shadowAlpha = shadowD <= 0.0 ? 1.0 : exp(-max(shadowD, 0.0) * max(shadowD, 0.0) / (0.5 * blurRadius * blurRadius));

        outColor = vec4(inFillColorA.rgb, inFillColorA.a * shadowAlpha);
        if (outColor.a < 0.001) discard;
        return;
    } else if (inFillType == 6) {
        // Inset Box-Shadow
        float spread = inFillColorB.z;
        vec2 shadowHalfSize = (inRectXYWH.zw * 0.5) - vec2(spread);
        vec4 shadowRadius = max(inBorderRadius - vec4(spread), vec4(0.0));

        float shadowD = sdRoundedBox(p, shadowHalfSize, shadowRadius);
        float blurRadius = max(inBlur, 0.001);
        float shadowAlpha = shadowD < 0.0 ? exp(-(-shadowD * -shadowD) / (0.5 * blurRadius * blurRadius)) : 0.0;

        outColor = vec4(inFillColorA.rgb, inFillColorA.a * shadowAlpha);
        if (outColor.a < 0.001) discard;
        return;
    }

    vec2 dGrad = vec2(dFdx(d), dFdy(d));
    float gradientLen = max(length(dGrad), 0.0001);

    float aaWidth = (inBlur > 0.0)
        ? (0.75 + inBlur) * max(inScale, 0.0001)
        : (gradientLen * 0.45); 

    bool isStraightX = abs(dGrad.x) < 0.001;
    bool isStraightY = abs(dGrad.y) < 0.001;
    bool isAxisAligned = (isStraightX || isStraightY) && (inRotation == 0.0);

    float alpha;
    if (isAxisAligned && inBlur == 0.0) {
        alpha = (d <= 0.001) ? 1.0 : 0.0;
    } else {
        alpha = smoothstep(aaWidth, -aaWidth, d);
    }

    if (inShapeType == 2) {
        outColor = vec4(fillColor.rgb, fillColor.a * alpha);
    } else {
        float maxStroke = max(max(inStrokeThickness.x, inStrokeThickness.y),
                              max(inStrokeThickness.z, inStrokeThickness.w));
        if (maxStroke > 0.0) {
            float strokeThick = inStrokeThickness.x;
            if (inShapeType == 0) {
                vec2 normP = p / max(halfSize, vec2(0.0001));
                if (abs(normP.x) > abs(normP.y)) {
                    strokeThick = (p.x > 0.0) ? inStrokeThickness.y : inStrokeThickness.w;
                } else {
                    strokeThick = (p.y < 0.0) ? inStrokeThickness.x : inStrokeThickness.z;
                }
            }

            strokeThick = round(strokeThick);

            float innerD = d + strokeThick;
            float innerAlpha = smoothstep(aaWidth, -aaWidth, innerD);

            vec4 bg = vec4(fillColor.rgb * fillColor.a, fillColor.a) * alpha;
            float strokeMask = clamp(alpha - innerAlpha, 0.0, 1.0);
            vec4 fg = vec4(inStrokeColor.rgb * inStrokeColor.a, inStrokeColor.a) * strokeMask;

            vec4 finalColor = fg + bg * (1.0 - fg.a);

            if (finalColor.a > 0.0001) {
                outColor = vec4(finalColor.rgb / finalColor.a, finalColor.a);
            } else {
                outColor = vec4(0.0);
            }
        } else {
            outColor = vec4(fillColor.rgb, fillColor.a * alpha);
        }
    }

    if (outColor.a < 0.001) discard;
}
