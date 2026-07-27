#include "avk/atomic_ui.h"
#include "avk/avk_font.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/color.h"
#include "ui/components.h"
#include <algorithm>

namespace {

/**
 * @brief Returns byte length of a UTF-8 leading character.
 */
int getUtf8CharLength(unsigned char c) {
  if (c < 0x80)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

/**
 * @brief Decrements byte index to start of previous UTF-8 codepoint.
 */
uint32_t getPreviousCharIndex(const std::string &str, uint32_t index) {
  if (index == 0)
    return 0;
  uint32_t idx = index;
  do {
    idx--;
  } while (idx > 0 && (static_cast<unsigned char>(str[idx]) & 0xC0) == 0x80);
  return idx;
}

/**
 * @brief Increments byte index to start of next UTF-8 codepoint.
 */
uint32_t getNextCharIndex(const std::string &str, uint32_t index) {
  if (index >= str.size())
    return static_cast<uint32_t>(str.size());
  return index + getUtf8CharLength(static_cast<unsigned char>(str[index]));
}

/**
 * @brief Returns byte index of previous word boundary.
 */
uint32_t getPreviousWordIndex(const std::string &str, uint32_t index) {
  if (index == 0)
    return 0;
  uint32_t idx = index;
  while (idx > 0) {
    uint32_t prev = getPreviousCharIndex(str, idx);
    char c = str[prev];
    if (c != ' ' && c != '\t' && c != ',' && c != '.')
      break;
    idx = prev;
  }
  while (idx > 0) {
    uint32_t prev = getPreviousCharIndex(str, idx);
    char c = str[prev];
    if (c == ' ' || c == '\t' || c == ',' || c == '.')
      return idx;
    idx = prev;
  }
  return 0;
}

/**
 * @brief Returns byte index of next word boundary.
 */
uint32_t getNextWordIndex(const std::string &str, uint32_t index) {
  uint32_t len = static_cast<uint32_t>(str.size());
  if (index >= len)
    return len;
  uint32_t idx = index;
  while (idx < len) {
    char c = str[idx];
    if (c == ' ' || c == '\t' || c == ',' || c == '.')
      break;
    idx = getNextCharIndex(str, idx);
  }
  while (idx < len) {
    char c = str[idx];
    if (c != ' ' && c != '\t' && c != ',' && c != '.')
      return idx;
    idx = getNextCharIndex(str, idx);
  }
  return len;
}

/**
 * @brief Inserts a Unicode codepoint at cursor byte offset.
 */
void appendUtf8(std::string &str, uint32_t codepoint, uint32_t &cursorBytePos) {
  std::string temp;
  if (codepoint < 0x80) {
    temp.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800) {
    temp.push_back(static_cast<char>((codepoint >> 6) | 0xC0));
    temp.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
  } else if (codepoint < 0x10000) {
    temp.push_back(static_cast<char>((codepoint >> 12) | 0xE0));
    temp.push_back(static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80));
    temp.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
  } else if (codepoint < 0x110000) {
    temp.push_back(static_cast<char>((codepoint >> 18) | 0xF0));
    temp.push_back(static_cast<char>(((codepoint >> 12) & 0x3F) | 0x80));
    temp.push_back(static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80));
    temp.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
  }
  str.insert(cursorBytePos, temp);
  cursorBytePos += static_cast<uint32_t>(temp.size());
}

/**
 * @brief Determines text character index corresponding to relative mouse pixel
 * position.
 */
uint32_t findWhereCursorLanded(const std::string &textBuffer, avk::Font *font,
                               float relativeMouseX) {
  if (textBuffer.empty() || !font || relativeMouseX <= 0.0f) {
    return 0;
  }
  float prevWidth = 0.0f;
  for (uint32_t i = 0; i < textBuffer.size();
       i = getNextCharIndex(textBuffer, i)) {
    uint32_t nextI = getNextCharIndex(textBuffer, i);
    float currentWidth = font->measureText(textBuffer.substr(0, nextI)).x;
    float midPoint = (prevWidth + currentWidth) * 0.5f;
    if (relativeMouseX < midPoint) {
      return i;
    }
    prevWidth = currentWidth;
  }
  return static_cast<uint32_t>(textBuffer.size());
}

} // namespace

namespace atomic {

/**
 * @brief Interactive text input box component with text selection and cursor
 * controls.
 */
Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, uint32_t fontId) {
  const auto &style = modifier.getStyle();
  auto *uiState = utils::layout::getUiState();

  Clay_ElementId textInputId = utils::layout::getNextId("TextInput");
  uint32_t elementId = textInputId.id;

  float textboxHeight = style.height.value_or(DEFAULT_HEIGHT);
  glm::vec4 bg = style.backgroundColor.value_or(DEFAULT_BACKGROUND_NORMAL);
  glm::vec4 strokeColor = style.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  float strokeWidth = style.strokeThickness.value_or(DEFAULT_BORDER_WIDTH);
  glm::vec4 radius = style.borderRadius.value_or(DEFAULT_BORDER_RADIUS);

  uint16_t padL = style.padLeft.value_or(12);
  uint16_t padR = style.padRight.value_or(12);
  uint16_t padT = style.padTop.value_or(10);
  uint16_t padB = style.padBottom.value_or(10);

  avk::Font *font = getFont(fontId != 0 ? fontId : getDefaultFontId());
  float fontHeight = font ? font->getLineHeight() : 18.0f;

  bool isFocused = (uiState->focusedElementId == elementId);

  glm::vec4 activeBorder = isFocused ? Colors::orange : strokeColor;
  float activeBorderWidth = isFocused ? 1.5f : strokeWidth;

  Modifier containerStyle = std::move(modifier)
                                .background(bg)
                                .border(activeBorder, activeBorderWidth)
                                .rounded(radius.x)
                                .height(textboxHeight)
                                .padding(padL, padR, padT, padB)
                                .relative()
                                .row();

  auto &inputState = uiState->inputStateMap[elementId];

  auto resetSelection = [&]() {
    inputState.selectionStart = 0;
    inputState.selectionEnd = 0;
    inputState.selectionAnchor = 0;
    uiState->doingShiftSelect = false;
    uiState->selectAll = false;
  };

  auto deleteSelection = [&]() -> bool {
    if (inputState.selectionStart != inputState.selectionEnd) {
      uint32_t start = inputState.selectionStart;
      uint32_t len = inputState.selectionEnd - inputState.selectionStart;
      textBuffer.erase(start, len);
      inputState.cursorPosition = start;
      resetSelection();
      return true;
    }
    return false;
  };

  Interaction result = Div(std::move(containerStyle), [&]() {
    if (result.hovered) {
      uiState->anyInputBoxHovered = true;
    }

    ComputedLayout bounds = utils::layout::getComputedLayout(textInputId);
    if (bounds.found) {
      float relativeMouseX = uiState->pointerPos.x - (bounds.x() + padL);

      if (result.hovered && uiState->pointerPressed &&
          !inputState.isDraggingText) {
        uint32_t landedPos =
            findWhereCursorLanded(textBuffer, font, relativeMouseX);
        uiState->focusedElementId = elementId;
        inputState.selectionAnchor = landedPos;
        inputState.cursorPosition = landedPos;
        inputState.selectionStart = landedPos;
        inputState.selectionEnd = landedPos;
        inputState.isDraggingText = true;
        uiState->selectAll = false;
        uiState->doingShiftSelect = false;
        isFocused = true;
      }

      if (isFocused && uiState->pointerDown && inputState.isDraggingText) {
        uint32_t currentLandedPos =
            findWhereCursorLanded(textBuffer, font, relativeMouseX);
        inputState.cursorPosition = currentLandedPos;
        inputState.selectionStart =
            std::min(inputState.selectionAnchor, currentLandedPos);
        inputState.selectionEnd =
            std::max(inputState.selectionAnchor, currentLandedPos);
      }
    }

    if (!uiState->pointerDown) {
      inputState.isDraggingText = false;
    }

    if (isFocused) {
      if (uiState->selectAll) {
        inputState.selectionStart = 0;
        inputState.selectionEnd = static_cast<uint32_t>(textBuffer.size());
        inputState.cursorPosition = static_cast<uint32_t>(textBuffer.size());
        uiState->doingShiftSelect = false;
        uiState->selectAll = false;
      }

      if (uiState->backspacePressed) {
        if (!deleteSelection() && inputState.cursorPosition > 0) {
          uint32_t prev =
              getPreviousCharIndex(textBuffer, inputState.cursorPosition);
          uint32_t len = inputState.cursorPosition - prev;
          textBuffer.erase(prev, len);
          inputState.cursorPosition = prev;
        }
      }

      if (uiState->deletePressed) {
        if (!deleteSelection() &&
            inputState.cursorPosition < textBuffer.size()) {
          uint32_t next =
              getNextCharIndex(textBuffer, inputState.cursorPosition);
          textBuffer.erase(inputState.cursorPosition,
                           next - inputState.cursorPosition);
        }
      }

      auto moveCursorWithSelection = [&](uint32_t newCursor) {
        if (uiState->shiftPressed) {
          uint32_t anchor = inputState.cursorPosition;
          if (uiState->doingShiftSelect) {
            anchor = (inputState.cursorPosition == inputState.selectionStart)
                         ? inputState.selectionEnd
                         : inputState.selectionStart;
          } else {
            uiState->doingShiftSelect = true;
          }
          inputState.cursorPosition = newCursor;
          inputState.selectionStart =
              std::min(anchor, inputState.cursorPosition);
          inputState.selectionEnd = std::max(anchor, inputState.cursorPosition);
        } else {
          if (inputState.selectionStart != inputState.selectionEnd &&
              !uiState->ctrlPressed) {
            inputState.cursorPosition = (newCursor < inputState.cursorPosition)
                                            ? inputState.selectionStart
                                            : inputState.selectionEnd;
          } else {
            inputState.cursorPosition = newCursor;
          }
          resetSelection();
        }
      };

      if (uiState->leftArrowPressed) {
        uint32_t nextPos =
            uiState->ctrlPressed
                ? getPreviousWordIndex(textBuffer, inputState.cursorPosition)
                : getPreviousCharIndex(textBuffer, inputState.cursorPosition);
        moveCursorWithSelection(nextPos);
      }

      if (uiState->rightArrowPressed) {
        uint32_t nextPos =
            uiState->ctrlPressed
                ? getNextWordIndex(textBuffer, inputState.cursorPosition)
                : getNextCharIndex(textBuffer, inputState.cursorPosition);
        moveCursorWithSelection(nextPos);
      }

      if (!uiState->capturedChars.empty()) {
        deleteSelection();
        for (uint32_t codepoint : uiState->capturedChars) {
          appendUtf8(textBuffer, codepoint, inputState.cursorPosition);
        }
      }
    }

    if (isFocused && (inputState.selectionStart != inputState.selectionEnd) &&
        font) {
      float startX =
          font->measureText(textBuffer.substr(0, inputState.selectionStart)).x;
      float endX =
          font->measureText(textBuffer.substr(0, inputState.selectionEnd)).x;
      float selectW = endX - startX;
      float selectH = fontHeight * 0.85f;
      float selectY = (textboxHeight - selectH) * 0.5f - padT;

      Div(DefaultModifier()
              .absolute()
              .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
              .offset(startX, selectY)
              .size(selectW, selectH)
              .background(glm::vec4(0.0f, 0.3f, 0.67f, 0.65f))
              .rounded(3.0f));
    }

    if (textBuffer.empty() && !isFocused) {
      Text(placeholder, fontId,
           DefaultModifier().color(glm::vec4(Colors::gray[400])));
    } else {
      Text(textBuffer, fontId, DefaultModifier().color(Colors::white));
    }

    if (isFocused && (inputState.selectionStart == inputState.selectionEnd) &&
        font) {
      float cursorOffset =
          font->measureText(textBuffer.substr(0, inputState.cursorPosition)).x;
      float caretH = fontHeight * 0.85f;
      float caretY = (textboxHeight - caretH) * 0.5f - padT;

      Div(DefaultModifier()
              .absolute()
              .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
              .offset(cursorOffset, caretY)
              .size(2.0f, caretH)
              .background(Colors::white)
              .rounded(1.0f));
    }
  });

  if (isFocused && uiState->enterPressed) {
    result.clicked = true;
    uiState->focusedElementId = 0;
  }

  return result;
}

Interaction TextInput(std::string &textBuffer, const std::string &placeholder,
                      Modifier &&modifier) {
  return TextInput(std::move(modifier), textBuffer, placeholder,
                   getDefaultFontId());
}

} // namespace atomic
