#version 450

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
layout(location = 15) flat in uint inTextureIndex; // Used as the number of sides for Polygons (N >= 3)
layout(location = 16) flat in vec4 inUvBounds;
layout(location = 17) flat in float inBlur;

layout(location = 0) out vec4 outColor;

// SDF for rounded boxes with variable corner radii: [topLeft, topRight, bottomLeft, bottomRight]
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

// SDF for circles
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

// SDF for line segments from 'a' to 'b'
float sdSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0);
    return length(pa - ba*h);
}

// SDF for regular N-gons (Polygons)
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
    // 1. Pixel-Shader clipping check
    if (inPixelPos.x < inClipRect.x || inPixelPos.y < inClipRect.y ||
        inPixelPos.x > inClipRect.z || inPixelPos.y > inClipRect.w) {
        discard;
    }

    // 2. Adjust coordinate space relative to center
    vec2 center = inRectXYWH.xy + inRectXYWH.zw * 0.5;
    vec2 p = inPixelPos - center;
    vec2 halfSize = inRectXYWH.zw * 0.5;

    // 3. Compute Distance Field based on active ShapeType
    float d = 0.0;
    
    if (inShapeType == 0) { // Rectangle / Rounded Rectangle
        d = sdRoundedBox(p, halfSize, inBorderRadius);
        
    } else if (inShapeType == 1) { // Circle
        d = sdCircle(p, min(halfSize.x, halfSize.y));
        
    } else if (inShapeType == 2) { // Line
        // For lines, rectXYWH holds [x1, y1, x2, y2]
        d = sdSegment(inPixelPos, inRectXYWH.xy, inRectXYWH.zw) - inStrokeThickness * 0.5;
        
    } else if (inShapeType == 3) { // Polygon (Regular N-gon)
        // Ensure N is clamped to at least 3 sides (triangle)
        int numSides = int(max(inTextureIndex, 3));
        d = sdRegularPolygon(p, min(halfSize.x, halfSize.y), numSides);
    }

    // 4. Evaluate color fills
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
        float dist = distance(inPixelPos, center) / length(halfSize);
        fillColor = mix(inFillColorA, inFillColorB, clamp(dist, 0.0, 1.0));
    }

    // Antialiasing calculation
    float edge = fwidth(d);
    float alpha = smoothstep(edge + inBlur, -edge, d);

    // 5. Output final fragments
    // For Lines, thickness is evaluated in the SDF directly.
    if (inShapeType == 2) {
        outColor = fillColor * alpha;
    } else {
        // For Rectangles, Circles, and Polygons, support standard outline borders
        if (inStrokeThickness > 0.0) {
            float strokeD = abs(d + inStrokeThickness * 0.5) - inStrokeThickness * 0.5;
            float strokeAlpha = smoothstep(edge + inBlur, -edge, strokeD);

            vec4 strokeColor = inStrokeColor;
            outColor = mix(vec4(0.0), strokeColor, strokeAlpha);
            outColor = mix(outColor, fillColor, alpha);
        } else {
            outColor = fillColor * alpha;
        }
    }

    if (outColor.a < 0.001) {
        discard;
    }
}