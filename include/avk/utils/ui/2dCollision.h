#pragma once
#include <clay.h>
#include <glm/glm.hpp>

namespace utils::ui {
inline float sdRoundedBox(glm::vec2 p, glm::vec2 b, glm::vec4 r) {
  float radius = r.x; // Default topLeft
  if (p.x > 0.0f && p.y < 0.0f) {
    radius = r.y; // topRight
  } else if (p.x < 0.0f && p.y > 0.0f) {
    radius = r.z; // bottomLeft
  } else if (p.x > 0.0f && p.y > 0.0f) {
    radius = r.w; // bottomRight
  }

  glm::vec2 q = glm::abs(p) - b + glm::vec2(radius);
  return glm::min(glm::max(q.x, q.y), 0.0f) +
         glm::length(glm::max(q, glm::vec2(0.0f))) - radius;
}

inline bool isPointerOverRoundedBox(glm::vec2 pointerPos, Clay_BoundingBox box,
                                    glm::vec4 r) {
  // rule out with basic aabb first
  if (pointerPos.x < box.x || pointerPos.y < box.y ||
      pointerPos.x > box.x + box.width || pointerPos.y > box.y + box.height) {
    return false;
  }

  glm::vec2 center =
      glm::vec2(box.x + box.width * 0.5f, box.y + box.height * 0.5f);
  glm::vec2 p = pointerPos - center;
  glm::vec2 halfSize = glm::vec2(box.width * 0.5f, box.height * 0.5f);

  float d = sdRoundedBox(p, halfSize, r);
  return d <= 0.0f;
}
} // namespace utils::ui
