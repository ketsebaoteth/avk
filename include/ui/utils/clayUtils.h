#pragma once
#include "Vera/src/vera_windowing/core/app/Types.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "glm/glm.hpp"
#include "ui/core/resources.h"
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

inline Clay_Dimensions measureTextCallback(Clay_StringSlice text,
                                           Clay_TextElementConfig *config,
                                           void *userData) {
  (void)userData;
  if (text.length == 0 || config == nullptr) {
    return Clay_Dimensions{0.0f, 0.0f};
  }

  uint32_t fontId = config->fontId;
  avk::Font *font = atomic::getFont(fontId);
  float fontSize = static_cast<float>(config->fontSize);

  avk::TextWrapMode wrapMode = avk::TextWrapMode::Disabled;
  float lineHeight = 0.0f;
  avk::TextAlignMode alignMode = avk::TextAlignMode::Left;
  float maxWidth = 0.0f;
  bool wrapExplicit = false;

  auto *uiState = atomic::getUiState();

  if (config->userData != nullptr) {
    auto *payload = static_cast<atomic::RenderPayload *>(config->userData);
    lineHeight = payload->lineHeight;

    if (payload->textWrap.has_value()) {
      wrapExplicit = true;
      switch (payload->textWrap.value()) {
      case atomic::TextWrap::Anywhere:
        wrapMode = avk::TextWrapMode::Anywhere;
        break;
      case atomic::TextWrap::Disabled:
        wrapMode = avk::TextWrapMode::Disabled;
        break;
      default:
        wrapMode = avk::TextWrapMode::Word;
        break;
      }
    }

    if (payload->textAlign.has_value()) {
      switch (payload->textAlign.value()) {
      case atomic::TextAlign::Center:
        alignMode = avk::TextAlignMode::Center;
        break;
      case atomic::TextAlign::Right:
        alignMode = avk::TextAlignMode::Right;
        break;
      // case atomic::TextAlign::Justify:
      //   alignMode = avk::TextAlignMode::Justify;
      //   break;
      default:
        alignMode = avk::TextAlignMode::Left;
        break;
      }
    }

    if (payload->textMaxWidth > 0.0f) {
      maxWidth = payload->textMaxWidth;
    }
  }

  const bool needsConstraint =
      (wrapExplicit && wrapMode != avk::TextWrapMode::Disabled) ||
      (alignMode != avk::TextAlignMode::Left);

  if (needsConstraint && maxWidth <= 20.0f && uiState) {
    if (!uiState->textConstraintWidthStack.empty()) {
      float stackW = uiState->textConstraintWidthStack.back();
      if (stackW > 20.0f) {
        maxWidth = stackW;
      }
    } else if (!uiState->positioningContextStack.empty()) {
      uint32_t parentId = uiState->positioningContextStack.back();
      auto parentLayout = utils::layout::getComputedLayout(parentId);
      if (parentLayout.found && parentLayout.width() > 20.0f) {
        maxWidth = parentLayout.width();
      }
    }
  }

  std::string safeStr(text.chars, text.length);

  float measureMaxW = (needsConstraint && maxWidth > 20.0f) ? maxWidth : 0.0f;

  if (font) {
    glm::vec2 size = font->measureText(safeStr, fontSize, measureMaxW, wrapMode,
                                       lineHeight, avk::TextAlignMode::Left);

    if (size.x > 0.0f && size.y > 0.0f) {
      if (needsConstraint && maxWidth > 20.0f) {
        size.x = std::min(size.x, maxWidth);
      }
      return Clay_Dimensions{size.x, size.y};
    }
  }

  float estimatedW = static_cast<float>(text.length) * (fontSize * 0.55f);
  float estimatedH = (fontSize > 0.0f) ? fontSize : 16.0f;
  if (needsConstraint && maxWidth > 20.0f) {
    estimatedW = maxWidth;
  }
  return Clay_Dimensions{estimatedW, estimatedH};
}

/** @brief Returns true if a text input box currently holds keyboard focus. */
bool isKeyboardCaptured();

/** @brief Clears global keyboard input focus. */
void clearKeyboardFocus();

/** @brief Safely copies a C++ string into Clay's frame scratchpad. */
Clay_String copyStringToClayBuffer(const std::string &text);
} // namespace atomic
