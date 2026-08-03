#pragma once
#include "glm/glm.hpp"
#include "ui/utils/color.h"
#include <cstdint>
#include <optional>

namespace atomic {
/**
 * @brief Styles that cascade and propagate down the layout
 * hierarchy.
 */
struct CascadingStyle {
  std::optional<glm::vec4> textColor;
  std::optional<uint32_t> fontId;
  std::optional<float> fontSize;
  std::optional<float> letterSpacing;
  std::optional<float> fontWeight;
  std::optional<float> lineHeight;

  // Structural properties can safely hold absolute fallback tracking states
  glm::vec2 inheritedTranslate = {0.0f, 0.0f};
  std::optional<float> textOffset = 0.0f;
  float inheritedOpacity = 1.0f;
  bool pointerEvents = true;
  bool disabled = false;
};
} // namespace atomic
