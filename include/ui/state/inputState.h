#pragma once
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
  bool isDraggingText = false;
};

} // namespace atomic
