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
                    [[maybe_unused]] void *userData) {
  auto uiState = getUiState();

  if (!uiState || config->fontId >= uiState->fonts.size()) {
    return Clay_Dimensions{0.0f, 0.0f};
  }

  std::string str(text.chars, text.length);
  glm::vec2 size = uiState->fonts[config->fontId]->measureText(str);

  return Clay_Dimensions{size.x, size.y};
}

/** @brief Returns true if a text input box currently holds keyboard focus. */
bool isKeyboardCaptured();

/** @brief Clears global keyboard input focus. */
void clearKeyboardFocus();

/** @brief Safely copies a C++ string into Clay's frame scratchpad. */
Clay_String copyStringToClayBuffer(const std::string &text);
} // namespace atomic
