#version 450

// Enable Vulkan Descriptor Indexing extension for dynamic array indexing
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
layout(location = 12) flat in float inStrokeThickness;
layout(location = 13) flat in uint inShapeType;
layout(location = 14) flat in uint inFillType;
layout(location = 15) flat in uint inTextureIndex; // Dynamic index pointing to globalTextures
layout(location = 16) flat in vec4 inUvBounds;
layout(location = 17) flat in float inBlur;

layout(location = 0) out vec4 outColor;

// Declare our boundless array of samplers. 
layout(binding = 0) uniform sampler2D globalTextures[];

float sdRoundedBox(vec2 p, vec2 b, vec4 r) {
    float radius = r.x; // default topLeft
    if (p.x > 0.0 && p.y < 0.0) {
        radius = r.y; // topRight
    } else if (p.x < 0.0 && p.y > 0.0) {
        radius = r.z; // bottomLeft
    } else if (p.x > 0.0 && p.y > 0.0) {
        radius = r.w; // bottomRight
    }
    
    vec2 q = abs(p) - b + vec2(radius);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

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

    float d = 0.0;
    
    if (inShapeType == 0) { // Rectangle / Rounded Rectangle
        vec2 center = inRectXYWH.xy + inRectXYWH.zw * 0.5;
        vec2 p = inPixelPos - center;
        vec2 halfSize = inRectXYWH.zw * 0.5;
        d = sdRoundedBox(p, halfSize, inBorderRadius);
        
    } else if (inShapeType == 1) { // Circle
        vec2 center = inRectXYWH.xy + inRectXYWH.zw * 0.5;
        vec2 p = inPixelPos - center;
        vec2 halfSize = inRectXYWH.zw * 0.5;
        d = sdCircle(p, min(halfSize.x, halfSize.y));
        
    } else if (inShapeType == 2) { // Line
        d = sdSegment(inPixelPos, inRectXYWH.xy, inRectXYWH.zw) - inStrokeThickness * 0.5;
        
    } else if (inShapeType == 3) { // Polygon (Regular N-gon)
        vec2 center = inRectXYWH.xy + inRectXYWH.zw * 0.5;
        vec2 p = inPixelPos - center;
        vec2 halfSize = inRectXYWH.zw * 0.5;
        int numSides = int(max(inTextureIndex, 3));
        d = sdRegularPolygon(p, min(halfSize.x, halfSize.y), numSides);
    }

    vec4 fillColor = inFillColorA;
    if (inFillType == 1) { // Linear gradient
        vec2 dir = inGradientEnd - inGradientStart;
        float lenSq = dot(dir, dir);
        if (lenSq > 0.0001) {
            vec2 normCoord = (inPixelPos - inRectXYWH.xy) / inRectXYWH.zw;
            float t = clamp(dot(normCoord - inGradientStart, dir) / lenSq, 0.0, 1.0);
            fillColor = mix(inFillColorA, inFillColorB, t);
        }
    } else if (inFillType == 2) { // Radial gradient
        vec2 center = inRectXYWH.xy + inRectXYWH.zw * 0.5;
        vec2 halfSize = inRectXYWH.zw * 0.5;
        float dist = distance(inPixelPos, center) / length(halfSize);
        fillColor = mix(inFillColorA, inFillColorB, clamp(dist, 0.0, 1.0));
    } else if (inFillType == 3) { // Text
        float textAlpha = texture(globalTextures[nonuniformEXT(inTextureIndex)], inUV).r;
        outColor = vec4(inFillColorA.rgb, inFillColorA.a * textAlpha);
        if (outColor.a < 0.001) {
            discard;
        }
        return;  
    } else if (inFillType == 4) { // ImageTexture
        vec4 texColor = texture(globalTextures[nonuniformEXT(inTextureIndex)], inUV);
        vec4 tintedTex = texColor * inFillColorB;
        fillColor = mix(inFillColorA, tintedTex, tintedTex.a);
    }

    // -----------------------------------------------------------------
    // LINEAR COVERAGE ANTI-ALIASING (CSS / SKIA TRICK)
    // -----------------------------------------------------------------
    // fwidth(d) measures distance change per pixel (~1.0).
    float aaWidth = max(fwidth(d), 0.0001) + inBlur;
    
    // Exact 1-pixel linear alpha ramp:
    // d = -0.5 * aaWidth -> alpha = 1.0 (Fully inside)
    // d =  0.0          -> alpha = 0.5 (Boundary)
    // d = +0.5 * aaWidth -> alpha = 0.0 (Fully outside)
    float alpha = clamp(0.5 - d / aaWidth, 0.0, 1.0);

    if (inShapeType == 2) { // Line
        outColor = vec4(fillColor.rgb, fillColor.a * alpha);
    } else {
        if (inStrokeThickness > 0.0) {
            float innerD = d + inStrokeThickness;
            float innerAlpha = clamp(0.5 - innerD / aaWidth, 0.0, 1.0);

            // Isolate stroke ring
            float strokeMask = clamp(alpha - innerAlpha, 0.0, 1.0);

            vec4 stroke = vec4(inStrokeColor.rgb, inStrokeColor.a * strokeMask);
            vec4 fill = vec4(fillColor.rgb, fillColor.a * innerAlpha);

            // Standard source-over blending
            float finalAlpha = fill.a + stroke.a * (1.0 - fill.a);
            vec3 finalRgb = (finalAlpha > 0.0001)
                ? (fill.rgb * fill.a + stroke.rgb * stroke.a * (1.0 - fill.a)) / finalAlpha
                : vec3(0.0);

            outColor = vec4(finalRgb, finalAlpha);
        } else {
            outColor = vec4(fillColor.rgb, fillColor.a * alpha);
        }
    }

    if (outColor.a < 0.001) {
        discard;
    }
}
