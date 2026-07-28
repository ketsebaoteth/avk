#pragma once
#include "glm/glm.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace atomic {
/**
 * @brief Specifies CSS object-fit layout behaviors for images and textures.
 */
enum class ObjectFit : uint8_t { Fill, Contain, Cover, Custom };

/**
 * @brief Layout direction for generic container elements (Div).
 */
enum class LayoutDirection : uint8_t { Row, Column };

/**
 * @brief Horizontal alignment modes for container child layout.
 */
enum class AlignmentX : uint8_t { Left, Center, Right, SpaceBetween };

/**
 * @brief Vertical alignment modes for container child layout.
 */
enum class AlignmentY : uint8_t { Top, Center, Bottom, SpaceBetween };

/**
 * @brief Combined 2D alignment configuration block.
 */
struct Alignment {
  AlignmentX x = AlignmentX::Left;
  AlignmentY y = AlignmentY::Top;
};

/**
 * @brief Box Shadow parameters (Offset, Blur, Spread, Color).
 */
struct BoxShadow {
  glm::vec2 offset = glm::vec2(0.0f, 4.0f); // Offset (X, Y)
  float blur = 12.0f;                       // Blur radius
  float spread = 0.0f;                      // Spread expansion
  glm::vec4 color = glm::vec4(0.0f, 0.0f, 0.0f, 0.15f);
  bool inset = false; // True for inner inset shadows!
};

/**
 * @brief Position context models for layout elements.
 */
enum class Position : uint8_t { Normal, Relative, Absolute, Fixed };

/**
 * @brief Attachment anchor points for floating and overlay elements.
 */
enum class AttachPoint : uint8_t {
  TopLeft,
  TopCenter,
  TopRight,
  BottomLeft,
  BottomCenter,
  BottomRight,
  CenterLeft,
  Center,
  CenterRight
};

/**
 * @brief Comprehensive styling definition block backing fluent modifiers.
 */
struct Style {
  std::vector<BoxShadow> boxShadows;
  std::optional<glm::vec4> backgroundColor;
  std::optional<glm::vec4> borderRadius;
  std::optional<glm::vec4> strokeColor;
  std::optional<float> strokeThickness;

  std::optional<float> width;
  std::optional<float> height;

  std::optional<glm::vec4> textColor;
  std::optional<float> textOffset;

  std::optional<uint16_t> padLeft;
  std::optional<uint16_t> padRight;
  std::optional<uint16_t> padTop;
  std::optional<uint16_t> padBottom;

  std::optional<bool> pointerEvents;
  std::optional<bool> disabled;
  std::optional<float> opacity;

  std::optional<uint16_t> childGap = 5.0f;
  std::optional<AlignmentX> alignX;
  std::optional<AlignmentY> alignY;
  std::optional<LayoutDirection> direction;

  std::optional<float> scale;
  std::optional<float> rotation;
  std::optional<float> blur;

  std::optional<Position> position;
  std::optional<float> left;
  std::optional<float> right;
  std::optional<float> top;
  std::optional<float> bottom;

  std::optional<glm::vec2> transformOrigin;
  std::optional<glm::vec2> translate;
  std::optional<uint32_t> parentId;

  std::optional<AttachPoint> elementAttach;
  std::optional<AttachPoint> parentAttach;
  std::optional<glm::vec2> offset;
  std::optional<ObjectFit> objectFit;
  std::optional<glm::vec4> uvBounds;
};

} // namespace atomic
