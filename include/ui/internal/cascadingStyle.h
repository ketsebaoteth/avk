#pragma once
#include "glm/glm.hpp"
#include "ui/utils/color.h"
#include <cstdint>

namespace atomic {
/**
 * @brief Styles that cascade and propagate down the layout
 * hierarchy.
 */
struct CascadingStyle {
  glm::vec4 textColor = Colors::black[900];
  glm::vec2 inheritedTranslate = {0.0f, 0.0f};
  uint32_t fontId = 0;
  float textOffset = 0.0f;
  float inheritedOpacity = 1.0f;
  bool pointerEvents = true;
  bool disabled = false;

  // NEW: Typography Cascade
  float fontSize = 16.0f;
  float letterSpacing = 0.0f;
  float fontWeight = 400.0f;
  float lineHeight = 0.0f;
};
} // namespace atomic
