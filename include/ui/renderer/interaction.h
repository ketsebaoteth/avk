#pragma once

#include "ui/utils/color.h"
#include <glm/glm.hpp>

namespace atomic {
/**
 * @brief Interaction result payload returned by interactive primitives.
 */
struct Interaction {
  bool clicked = false;
  bool hovered = false;
  bool pressed = false;
  bool changed = false;

  explicit operator bool() const { return clicked || changed; }
};

} // namespace atomic
