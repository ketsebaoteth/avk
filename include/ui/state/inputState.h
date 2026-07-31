#pragma once
#include "glm/glm.hpp"
#include <chrono>
#include <cstdint>

namespace atomic {

/**
 * @brief Persistent state block tracking text input cursor and selection
 * metrics.
 */
struct InputState {
  uint32_t cursorPosition = 0;
  uint32_t selectionStart = 0;
  uint32_t selectionEnd = 0;
  uint32_t selectionAnchor = 0;

  glm::vec2 dragStartMousePos = glm::vec2(0.0f);
  glm::vec2 lastClickPos = glm::vec2(0.0f);
  std::chrono::high_resolution_clock::time_point lastClickTime{};

  bool isDraggingText = false;
  bool isPotentialTextDrag = false;
  bool wasArmedByDoubleClick = false;
  bool isDraggingSelectedText = false;
};

} // namespace atomic
