#pragma once
namespace atomic {
/**
 * @brief Persistent state block tracking scroll offsets and drag interactions.
 */
struct ScrollViewState {
  float scrollOffsetX = 0.0f;
  float scrollOffsetY = 0.0f;
  float targetScrollOffsetX = 0.0f;
  float targetScrollOffsetY = 0.0f;
  bool isDraggingY = false;
  float dragStartY = 0.0f;
  float dragStartScrollY = 0.0f;
};
} // namespace atomic
