#include "avk/atomic_ui.h"
#include "avk/avk_font.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/color.h"
#include "ui/components.h"
#include <algorithm>

namespace {

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

uint32_t getPreviousCharIndex(const std::string &str, uint32_t index) {
  if (index == 0)
    return 0;
  uint32_t idx = index;
  do {
    idx--;
  } while (idx > 0 && (static_cast<unsigned char>(str[idx]) & 0xC0) == 0x80);
  return idx;
}

uint32_t getNextCharIndex(const std::string &str, uint32_t index) {
  if (index >= str.size())
    return static_cast<uint32_t>(str.size());
  return index + getUtf8CharLength(static_cast<unsigned char>(str[index]));
}

uint32_t getPreviousWordIndex(const std::string &str, uint32_t index) {
  if (index == 0)
    return 0;
  uint32_t idx = index;

  while (idx > 0) {
    uint32_t prev = getPreviousCharIndex(str, idx);
    char c = str[prev];
    if (c != ' ' && c != '\t' && c != ',' && c != '.') {
      break;
    }
    idx = prev;
  }

  while (idx > 0) {
    uint32_t prev = getPreviousCharIndex(str, idx);
    char c = str[prev];
    if (c == ' ' || c == '\t' || c == ',' || c == '.') {
      return idx;
    }
    idx = prev;
  }

  return 0;
}

uint32_t getNextWordIndex(const std::string &str, uint32_t index) {
  uint32_t len = static_cast<uint32_t>(str.size());
  if (index >= len)
    return len;
  uint32_t idx = index;

  while (idx < len) {
    char c = str[idx];
    if (c == ' ' || c == '\t' || c == ',' || c == '.') {
      break;
    }
    idx = getNextCharIndex(str, idx);
  }

  while (idx < len) {
    char c = str[idx];
    if (c != ' ' && c != '\t' && c != ',' && c != '.') {
      return idx;
    }
    idx = getNextCharIndex(str, idx);
  }

  return len;
}

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

// void popBackUtf8(std::string &str) {
//   if (str.empty())
//     return;
//   while (!str.empty() &&
//          (static_cast<unsigned char>(str.back()) & 0xC0) == 0x80) {
//     str.pop_back();
//   }
//   if (!str.empty()) {
//     str.pop_back();
//   }
// }

} // namespace

namespace atomic {

static inline uint32_t findWhereCursorLanded(std::string &textBuffer,
                                             avk::Font *font,
                                             float relativeMouseX) {
  float currentWidth = 0.0f;
  uint32_t clickedIndex = 0;
  float accumulatedWidth = 0;

  for (uint32_t i = 0; i < textBuffer.size();
       i = getNextCharIndex(textBuffer, i)) {
    std::string sub = textBuffer.substr(getPreviousCharIndex(textBuffer, i), 1);
    currentWidth = font->measureText(sub).x;
    accumulatedWidth += currentWidth;
    if (relativeMouseX < accumulatedWidth) {
      auto lastCharMiddleLine = accumulatedWidth - (currentWidth / 2);
      if (relativeMouseX < lastCharMiddleLine) {
        clickedIndex = i;
      } else {
        clickedIndex = getNextCharIndex(textBuffer, i);
      }
      break;
    }
    clickedIndex = static_cast<uint32_t>(textBuffer.size());
  }
  return clickedIndex;
}

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, uint32_t fontId) {
  const auto &style = modifier.getStyle();

  Clay_ElementId textInputId = utils::layout::getNextId("TextInput");
  Clay__OpenElementWithId(textInputId);

  // --- Layout & Visual Properties ---
  glm::vec4 bg = style.backgroundColor.value_or("#212121"_hex);
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(6.0f));
  glm::vec4 strokeColor = style.strokeColor.value_or("#ffffff1a"_hex);
  float strokeWidth = style.strokeThickness.value_or(1.0f);

  float textboxHeight = style.height.value_or(DEFAULT_HEIGHT);
  avk::Font *font = getFont(fontId);
  float fontHeight = font ? font->getLineHeight() : 18.0f;

  uint16_t padT = style.padTop.value_or(12);
  uint16_t padB = style.padBottom.value_or(12);
  uint16_t padL = style.padLeft.value_or(12);
  uint16_t padR = style.padRight.value_or(12);

  // Main container decl
  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_GROW(),
                 .height = CLAY_SIZING_FIXED(textboxHeight)},
      .padding = {padL, padR, padT, padB},
      .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
      .layoutDirection = CLAY_LEFT_TO_RIGHT};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  decl.border = {.color =
                     Clay_Color{strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                                strokeColor.b * 255.0f, strokeColor.a * 255.0f},
                 .width = {.left = static_cast<uint16_t>(strokeWidth),
                           .right = static_cast<uint16_t>(strokeWidth),
                           .top = static_cast<uint16_t>(strokeWidth),
                           .bottom = static_cast<uint16_t>(strokeWidth)}};

  Clay__ConfigureOpenElement(decl);

  auto *uiState = utils::layout::getUiState();

  // --- Hover & Focus State Evaluation ---
  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textInputId);
  if (elementData.found) {
    // Reads HOST INPUT: uiState->pointerPos
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  if (isHovered) {
    uiState->anyInputBoxHovered =
        true; // Signals main loop to set I-beam cursor
  }

  bool isFocused = (uiState->focusedElementId == textInputId.id);

  // Helper lambdas
  auto resetSelection = [&]() {
    uiState->selectionStart = 0;
    uiState->selectionEnd = 0;
    uiState->selectionAnchor = 0;
    uiState->doingShiftSelect = false;
    uiState->selectAll = false;
  };

  auto deleteSelection = [&]() -> bool {
    if (uiState->selectionStart != uiState->selectionEnd) {
      uint32_t start = uiState->selectionStart;
      uint32_t len = uiState->selectionEnd - uiState->selectionStart;
      textBuffer.erase(start, len);
      uiState->cursorPosition = start;
      resetSelection();
      return true;
    }
    return false;
  };

  // --- MOUSE CLICK & DRAG SELECTION LOGIC ---
  if (elementData.found) {
    float relativeMouseX =
        uiState->pointerPos.x - (elementData.boundingBox.x + padL);

    // 1. Initial Press (Single Frame Impulse from Host: pointerPressed)
    if (isHovered && uiState->pointerPressed) {
      uint32_t landedPos =
          font ? findWhereCursorLanded(textBuffer, font, relativeMouseX)
               : static_cast<uint32_t>(textBuffer.size());

      uiState->focusedElementId = textInputId.id;
      uiState->cursorPosition = landedPos;
      uiState->selectionAnchor = landedPos;
      uiState->selectionStart = landedPos;
      uiState->selectionEnd = landedPos;
      uiState->isDraggingText = true; // Lock drag state to this component
      uiState->selectAll = false;
      uiState->doingShiftSelect = false;
      isFocused = true;
    }

    // 2. Active Drag (Continuous State from Host: pointerDown)
    if (isFocused && uiState->pointerDown && uiState->isDraggingText) {
      uint32_t landedPos =
          font ? findWhereCursorLanded(textBuffer, font, relativeMouseX)
               : static_cast<uint32_t>(textBuffer.size());

      uiState->cursorPosition = landedPos;

      // Explicit template types prevent type mismatch errors (uint32_t vs
      // size_t)
      uiState->selectionStart =
          std::min<uint32_t>(uiState->selectionAnchor, landedPos);
      uiState->selectionEnd =
          std::max<uint32_t>(uiState->selectionAnchor, landedPos);
    }
  }

  // 3. Clear active drag lock when mouse is released globally
  if (!uiState->pointerDown) {
    uiState->isDraggingText = false;
  }

  // --- KEYBOARD OPERATIONS (Requires Active Focus) ---
  if (isFocused) {
    // Select All (Ctrl+A command)
    if (uiState->selectAll) {
      uiState->selectionStart = 0;
      uiState->selectionEnd = static_cast<uint32_t>(textBuffer.size());
      uiState->cursorPosition = static_cast<uint32_t>(textBuffer.size());
      uiState->doingShiftSelect = false;
      uiState->selectAll = false;
    }

    // Backspace
    if (uiState->backspacePressed) {
      if (!deleteSelection() && uiState->cursorPosition > 0) {
        uint32_t prev =
            getPreviousCharIndex(textBuffer, uiState->cursorPosition);
        uint32_t len = uiState->cursorPosition - prev;
        textBuffer.erase(prev, len);
        uiState->cursorPosition = prev;
      }
    }

    // Delete
    if (uiState->deletePressed) {
      if (!deleteSelection() && uiState->cursorPosition < textBuffer.size()) {
        uint32_t next = getNextCharIndex(textBuffer, uiState->cursorPosition);
        textBuffer.erase(uiState->cursorPosition,
                         next - uiState->cursorPosition);
      }
    }

    // Arrow Navigation & Shift Selection
    auto moveCursorWithSelection = [&](uint32_t newCursor) {
      if (uiState->shiftPressed) {
        uint32_t anchor = uiState->cursorPosition;
        if (uiState->doingShiftSelect) {
          anchor = (uiState->cursorPosition == uiState->selectionStart)
                       ? uiState->selectionEnd
                       : uiState->selectionStart;
        } else {
          uiState->doingShiftSelect = true;
        }

        uiState->cursorPosition = newCursor;
        uiState->selectionStart =
            std::min<uint32_t>(anchor, uiState->cursorPosition);
        uiState->selectionEnd =
            std::max<uint32_t>(anchor, uiState->cursorPosition);
      } else {
        if (uiState->selectionStart != uiState->selectionEnd &&
            !uiState->ctrlPressed) {
          uiState->cursorPosition = (newCursor < uiState->cursorPosition)
                                        ? uiState->selectionStart
                                        : uiState->selectionEnd;
        } else {
          uiState->cursorPosition = newCursor;
        }
        resetSelection();
      }
    };

    if (uiState->leftArrowPressed) {
      uint32_t nextPos =
          uiState->ctrlPressed
              ? getPreviousWordIndex(textBuffer, uiState->cursorPosition)
              : getPreviousCharIndex(textBuffer, uiState->cursorPosition);
      moveCursorWithSelection(nextPos);
    }

    if (uiState->rightArrowPressed) {
      uint32_t nextPos =
          uiState->ctrlPressed
              ? getNextWordIndex(textBuffer, uiState->cursorPosition)
              : getNextCharIndex(textBuffer, uiState->cursorPosition);
      moveCursorWithSelection(nextPos);
    }

    // Character Input Capture
    if (!uiState->capturedChars.empty()) {
      deleteSelection();
      for (uint32_t codepoint : uiState->capturedChars) {
        appendUtf8(textBuffer, codepoint, uiState->cursorPosition);
      }
    }
  }

  // --- RENDERING PASS ---
  float textOffsetValue = style.textOffset.value_or(0.0f);

  // Compute cursor X offset
  float cursorOffset = 0.0f;
  if (isFocused && font) {
    std::string leftSub = textBuffer.substr(0, uiState->cursorPosition);
    cursorOffset = font->measureText(leftSub).x;
  }

  // Text / Placeholder
  if (textBuffer.empty() && !isFocused) {
    Text(placeholder, fontId,
         DefaultModifier()
             .color(glm::vec4(Colors::gray[300]))
             .textOffset(textOffsetValue));
  } else {
    Text(textBuffer, fontId,
         DefaultModifier().color(Colors::white).textOffset(textOffsetValue));
  }

  // Render Caret (Only when no range is selected)
  if (isFocused && (uiState->selectionStart == uiState->selectionEnd) && font &&
      elementData.found) {
    float caretH = fontHeight * 0.85f;
    float caretY = (textboxHeight - caretH) * 0.5f;

    Clay__OpenElementWithId(utils::layout::getNextId("Caret"));

    Clay_ElementDeclaration caretDecl{};
    caretDecl.floating = {
        .offset = {static_cast<float>(padL) + cursorOffset, caretY},
        .attachTo = CLAY_ATTACH_TO_PARENT};
    caretDecl.backgroundColor = {220.0f, 220.0f, 220.0f, 255.0f};
    caretDecl.cornerRadius = {1.0f, 1.0f, 1.0f, 1.0f};
    caretDecl.layout = {.sizing = {.width = CLAY_SIZING_FIXED(2.0f),
                                   .height = CLAY_SIZING_FIXED(caretH)}};

    Clay__ConfigureOpenElement(caretDecl);
    Clay__CloseElement();
  }

  // Render Selection Highlight Box
  if (isFocused && (uiState->selectionStart != uiState->selectionEnd) && font &&
      elementData.found) {
    std::string selectedText =
        textBuffer.substr(uiState->selectionStart,
                          uiState->selectionEnd - uiState->selectionStart);
    std::string beforeSelection = textBuffer.substr(0, uiState->selectionStart);

    float selectedTextWidth = font->measureText(selectedText).x;
    float beforeSelectionWidth = font->measureText(beforeSelection).x;
    float selectH = fontHeight * 0.85f;
    float selectY = (textboxHeight - selectH) * 0.5f;

    Clay__OpenElementWithId(utils::layout::getNextId("SelectionHighlight"));

    Clay_ElementDeclaration selectDecl{};
    selectDecl.floating = {
        .offset = {static_cast<float>(padL) + beforeSelectionWidth, selectY},
        .attachTo = CLAY_ATTACH_TO_PARENT};
    selectDecl.backgroundColor = {0.0f, 77.0f, 170.0f, 171.0f};
    selectDecl.cornerRadius = {3.0f, 3.0f, 3.0f, 3.0f};
    selectDecl.layout = {
        .sizing = {.width = CLAY_SIZING_FIXED(selectedTextWidth),
                   .height = CLAY_SIZING_FIXED(selectH)}};

    Clay__ConfigureOpenElement(selectDecl);
    Clay__CloseElement();
  }

  Clay__CloseElement();

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isFocused;

  if (isFocused && uiState->enterPressed) {
    result.clicked = true;
    uiState->focusedElementId = 0; // Unfocus on Enter submission
  }

  return result;
}

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder) {
  return TextInput(std::move(modifier), textBuffer, placeholder,
                   getDefaultFontId());
}

} // namespace atomic
