#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace avk {

    enum class ShapeType : uint32_t {
        Rectangle = 0,
        Circle = 1,
        Line = 2,
        Polygon = 3
    };

    enum class FillType : uint32_t {
        Solid = 0,
        LinearGradient = 1,
        RadialGradient = 2,
        TextureGlyph = 3,
        ImageTexture = 4
    };

    /**
     * @brief High-performance, 16-byte aligned per-instance data block with safe C++20 defaults.
     */
    struct InstanceData {
        glm::vec4 rectXYWH = glm::vec4(0.0f);
        glm::vec4 borderRadius = glm::vec4(0.0f);
        glm::vec4 fillColorA = glm::vec4(1.0f); // Default solid white
        glm::vec4 fillColorB = glm::vec4(1.0f);
        glm::vec4 strokeColor = glm::vec4(0.0f);

        glm::vec2 gradientStart = glm::vec2(0.0f);
        glm::vec2 gradientEnd = glm::vec2(0.0f);

        glm::vec4 strokeFillColorA = glm::vec4(0.0f);
        glm::vec4 strokeFillColorB = glm::vec4(0.0f);

        // Default to a massive bounding box to prevent accidental pixel discards
        glm::vec4 clipRect = glm::vec4(0.0f, 0.0f, 16384.0f, 16384.0f);

        float strokeThickness = 0.0f;
        uint32_t shapeType = 0;
        uint32_t fillType = 0;
        uint32_t textureIndex = 0;

        glm::vec4 uvBounds = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); // Default to full UV bounds

        float blur = 0.0f;
        float pad0 = 0.0f;
        float pad1 = 0.0f;
        float pad2 = 0.0f;
    };

    struct Vertex {
        glm::vec2 pos;
        glm::vec2 uv;
    };

} // namespace avk