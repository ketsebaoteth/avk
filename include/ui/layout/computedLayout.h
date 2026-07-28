#pragma once
#include "glm/glm.hpp"

namespace atomic {
/**
 * @brief Resolved pixel geometry computed post-layout by the layout engine.
 */
struct ComputedLayout {
  glm::vec2 position = glm::vec2(0.0f);
  glm::vec2 size = glm::vec2(0.0f);
  glm::vec4 padding = glm::vec4(0.0f);
  bool found = false;

  // returns width
  [[nodiscard]] float width() const { return size.x; }
  // returns height
  [[nodiscard]] float height() const { return size.y; }
  // returns the x position
  [[nodiscard]] float x() const { return position.x; }
  // returns the y position
  [[nodiscard]] float y() const { return position.y; }
  // returns center of element basically position + 1/2 of size
  [[nodiscard]] glm::vec2 center() const { return position + (size * 0.5f); }
  // returns weather this computed style contains actuall information
  [[nodiscard]] bool hasValue() const { return found; }
  // returns sum of left and right padding
  [[nodiscard]] float horizontalPadding() const {
    return padding.x + padding.z;
  };
  // returns sum of top and bottom padding
  [[nodiscard]] float verticalPadding() const { return padding.y + padding.w; }
};
} // namespace atomic
