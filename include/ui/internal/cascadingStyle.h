#pragma once
#include "glm/glm.hpp"
#include <cstdint>

namespace atomic {
/**
 * @brief Styles that cascade and propagate down the layout
 * hierarchy.
 */
struct CascadingStyle {
  glm::vec4 textColor = glm::vec4(1.0f);
  // inherited font id for the used font
  uint32_t fontId = 0;
  // inherited text offset verticall offset
  float textOffset = 0.0f;
  float inheritedOpacity = 1.0f;
  bool pointerEvents = true;
  bool disabled = false;
};
} // namespace atomic
