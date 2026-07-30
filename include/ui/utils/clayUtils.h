#pragma once
#include "Vera/src/vera_windowing/core/app/Types.h"
#include "clay.h"
#include "glm/glm.hpp"
#include "ui/internal/context.h"
#include <string>

namespace atomic {
/** @breif tells clay about the state of the cursor */
inline void setClayCursorState(glm::vec2 pos, bool downState) {
  Clay_SetPointerState(Clay_Vector2{pos.x, pos.y}, downState);
}

/** @breif sets Clays Window Dimension Knowledge */
inline void setClayDimensions(VeraWindowState &state) {
  auto clayDimensions = Clay_Dimensions{};
  clayDimensions.width = static_cast<float>(state.width);
  clayDimensions.height = static_cast<float>(state.height);
  Clay_SetLayoutDimensions(clayDimensions);
}

/** @brief measures text size*/

inline static Clay_Dimensions
measureTextCallback(Clay_StringSlice text, Clay_TextElementConfig *config,
                    void *userData) {
  auto uiState = getUiState();
  (void)userData;

  if (!uiState || !config || config->fontId >= uiState->fonts.size() ||
      text.length <= 0) {
    return Clay_Dimensions{0.0f, 0.0f};
  }

  const auto &font = *uiState->fonts[config->fontId];
  std::string safeStr(text.chars, static_cast<size_t>(text.length));

  // Fallback to 14.0f if config->fontSize is 0
  float fontSize =
      (config->fontSize > 0) ? static_cast<float>(config->fontSize) : 14.0f;
  glm::vec2 size = font.measureText(safeStr, fontSize);

  // Letter spacing extra width calculation
  if (config->letterSpacing > 0 && text.length > 1) {
    size.x += static_cast<float>(config->letterSpacing) *
              static_cast<float>(text.length - 1);
  }

  return Clay_Dimensions{size.x, size.y};
}

/** @brief Returns true if a text input box currently holds keyboard focus. */
bool isKeyboardCaptured();

/** @brief Clears global keyboard input focus. */
void clearKeyboardFocus();

/** @brief Safely copies a C++ string into Clay's frame scratchpad. */
Clay_String copyStringToClayBuffer(const std::string &text);
} // namespace atomic
