
#pragma once
#include "glm/glm.hpp"
#include "ui/style/style.h"

namespace atomic {

/**
 * @brief Per-frame instance payload generated during layout traversal.
 */
struct RenderPayload {
  uint32_t textureIndex = 0;
  glm::vec4 tintColor{1.0f};
  glm::vec4 uvBounds{0.0f, 0.0f, 1.0f, 1.0f};
  ObjectFit objectFit = ObjectFit::Fill;

  float scale = 1.0f;
  float rotation = 0.0f;
  float blur = 0.0f;
  glm::vec2 transformOrigin{0.5f, 0.5f};
  glm::vec2 translate{0.0f, 0.0f};
  float textOffset = 0.0f;
  std::vector<BoxShadow> boxShadows;
};
} // namespace atomic
