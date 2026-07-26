#include "avk/atomic_ui.h"
#include "avk/avk_font.h"
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
} // namespace

namespace atomic {

static inline uint32_t findWhereCursorLanded(const std::string &textBuffer,
                                             avk::Font *font,
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

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, uint32_t fontId) {
  const auto &style = modifier.getStyle();

  Clay_ElementId textInputId = utils::layout::getNextId("TextInput");
  Clay__OpenElementWithId(textInputId);

  // --- Layout & Visual Properties ---
  glm::vec4 bg = style.backgroundColor.value_or(DEFAULT_BACKGROUND_NORMAL);
  glm::vec4 radius = style.borderRadius.value_or(DEFAULT_BORDER_RADIUS);
  glm::vec4 strokeColor = style.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  float strokeWidth = style.strokeThickness.value_or(DEFAULT_BORDER_WIDTH);
  float textboxHeight = style.height.value_or(DEFAULT_HEIGHT);

  avk::Font *font = getFont(fontId);
  float fontHeight = font ? font->getLineHeight() : 18.0f;

  uint16_t padT = style.padTop.value_or(12);
  uint16_t padB = style.padBottom.value_or(12);
  uint16_t padL = style.padLeft.value_or(12);
  uint16_t padR = style.padRight.value_or(12);

  // Main container declaration
  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = CLAY_SIZING_GROW(),
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

  // Apply styling and absolute positioning
  utils::layout::applyStyleToLayout(decl, style);

  // Safe frame allocation - zero memory leak!
  decl.userData = utils::layout::createFramePayload(style);

  Clay__ConfigureOpenElement(decl);

  auto *uiState = utils::layout::getUiState();

  // --- Z-ORDER AWARE HOVER CHECK (Fixed!) ---
  // Clay_PointerOver respects overlapping absolute floating layers and viewport
  // clipping!
  bool isHovered = Clay_PointerOver(textInputId);

  if (isHovered) {
    uiState->anyInputBoxHovered = true;
  }

  bool isFocused = (uiState->focusedElementId == textInputId.id);

  // Helper lambdas
  auto resetSelection = [&]() {
    uiState->inputStateMap[textInputId.id].selectionStart = 0;
    uiState->inputStateMap[textInputId.id].selectionEnd = 0;
    uiState->inputStateMap[textInputId.id].selectionAnchor = 0;
    uiState->doingShiftSelect = false;
    uiState->selectAll = false;
  };

  auto deleteSelection = [&]() -> bool {
    if (uiState->inputStateMap[textInputId.id].selectionStart !=
        uiState->inputStateMap[textInputId.id].selectionEnd) {
      uint32_t start = uiState->inputStateMap[textInputId.id].selectionStart;
      uint32_t len = uiState->inputStateMap[textInputId.id].selectionEnd -
                     uiState->inputStateMap[textInputId.id].selectionStart;
      textBuffer.erase(start, len);
      uiState->inputStateMap[textInputId.id].cursorPosition = start;
      resetSelection();
      return true;
    }
    return false;
  };

  // MOUSE CLICK & DRAG SELECTION LOGIC
  Clay_ElementData elementData = Clay_GetElementData(textInputId);
  if (elementData.found) {
    float relativeMouseX =
        uiState->pointerPos.x - (elementData.boundingBox.x + padL);

    // Initial Click (LOCK POINT A / ANCHOR)
    if (isHovered && uiState->pointerPressed &&
        !uiState->inputStateMap[textInputId.id].isDraggingText) {
      uint32_t landedPos =
          findWhereCursorLanded(textBuffer, font, relativeMouseX);

      uiState->focusedElementId = textInputId.id;
      uiState->inputStateMap[textInputId.id].selectionAnchor = landedPos;
      uiState->inputStateMap[textInputId.id].cursorPosition = landedPos;
      uiState->inputStateMap[textInputId.id].selectionStart = landedPos;
      uiState->inputStateMap[textInputId.id].selectionEnd = landedPos;
      uiState->inputStateMap[textInputId.id].isDraggingText = true;
      uiState->selectAll = false;
      uiState->doingShiftSelect = false;
      isFocused = true;
    }

    // Active Dragging (UPDATE POINT B / ENDPOINT)
    if (isFocused && uiState->pointerDown &&
        uiState->inputStateMap[textInputId.id].isDraggingText) {
      uint32_t currentLandedPos =
          findWhereCursorLanded(textBuffer, font, relativeMouseX);

      uiState->inputStateMap[textInputId.id].cursorPosition = currentLandedPos;
      uiState->inputStateMap[textInputId.id].selectionStart =
          std::min<uint32_t>(
              uiState->inputStateMap[textInputId.id].selectionAnchor,
              currentLandedPos);
      uiState->inputStateMap[textInputId.id].selectionEnd = std::max<uint32_t>(
          uiState->inputStateMap[textInputId.id].selectionAnchor,
          currentLandedPos);
    }
  }

  // Mouse Release
  if (!uiState->pointerDown) {
    uiState->inputStateMap[textInputId.id].isDraggingText = false;
  }

  // --- KEYBOARD OPERATIONS ---
  if (isFocused) {
    if (uiState->selectAll) {
      uiState->inputStateMap[textInputId.id].selectionStart = 0;
      uiState->inputStateMap[textInputId.id].selectionEnd =
          static_cast<uint32_t>(textBuffer.size());
      uiState->inputStateMap[textInputId.id].cursorPosition =
          static_cast<uint32_t>(textBuffer.size());
      uiState->doingShiftSelect = false;
      uiState->selectAll = false;
    }

    if (uiState->backspacePressed) {
      if (!deleteSelection() &&
          uiState->inputStateMap[textInputId.id].cursorPosition > 0) {
        uint32_t prev = getPreviousCharIndex(
            textBuffer, uiState->inputStateMap[textInputId.id].cursorPosition);
        uint32_t len =
            uiState->inputStateMap[textInputId.id].cursorPosition - prev;
        textBuffer.erase(prev, len);
        uiState->inputStateMap[textInputId.id].cursorPosition = prev;
      }
    }

    if (uiState->deletePressed) {
      if (!deleteSelection() &&
          uiState->inputStateMap[textInputId.id].cursorPosition <
              textBuffer.size()) {
        uint32_t next = getNextCharIndex(
            textBuffer, uiState->inputStateMap[textInputId.id].cursorPosition);
        textBuffer.erase(
            uiState->inputStateMap[textInputId.id].cursorPosition,
            next - uiState->inputStateMap[textInputId.id].cursorPosition);
      }
    }

    auto moveCursorWithSelection = [&](uint32_t newCursor) {
      if (uiState->shiftPressed) {
        uint32_t anchor = uiState->inputStateMap[textInputId.id].cursorPosition;
        if (uiState->doingShiftSelect) {
          anchor = (uiState->inputStateMap[textInputId.id].cursorPosition ==
                    uiState->inputStateMap[textInputId.id].selectionStart)
                       ? uiState->inputStateMap[textInputId.id].selectionEnd
                       : uiState->inputStateMap[textInputId.id].selectionStart;
        } else {
          uiState->doingShiftSelect = true;
        }
        uiState->inputStateMap[textInputId.id].cursorPosition = newCursor;
        uiState->inputStateMap[textInputId.id].selectionStart =
            std::min<uint32_t>(
                anchor, uiState->inputStateMap[textInputId.id].cursorPosition);
        uiState->inputStateMap[textInputId.id].selectionEnd =
            std::max<uint32_t>(
                anchor, uiState->inputStateMap[textInputId.id].cursorPosition);
      } else {
        if (uiState->inputStateMap[textInputId.id].selectionStart !=
                uiState->inputStateMap[textInputId.id].selectionEnd &&
            !uiState->ctrlPressed) {
          uiState->inputStateMap[textInputId.id].cursorPosition =
              (newCursor <
               uiState->inputStateMap[textInputId.id].cursorPosition)
                  ? uiState->inputStateMap[textInputId.id].selectionStart
                  : uiState->inputStateMap[textInputId.id].selectionEnd;
        } else {
          uiState->inputStateMap[textInputId.id].cursorPosition = newCursor;
        }
        resetSelection();
      }
    };

    if (uiState->leftArrowPressed) {
      uint32_t nextPos =
          uiState->ctrlPressed
              ? getPreviousWordIndex(
                    textBuffer,
                    uiState->inputStateMap[textInputId.id].cursorPosition)
              : getPreviousCharIndex(
                    textBuffer,
                    uiState->inputStateMap[textInputId.id].cursorPosition);
      moveCursorWithSelection(nextPos);
    }

    if (uiState->rightArrowPressed) {
      uint32_t nextPos =
          uiState->ctrlPressed
              ? getNextWordIndex(
                    textBuffer,
                    uiState->inputStateMap[textInputId.id].cursorPosition)
              : getNextCharIndex(
                    textBuffer,
                    uiState->inputStateMap[textInputId.id].cursorPosition);
      moveCursorWithSelection(nextPos);
    }

    if (!uiState->capturedChars.empty()) {
      deleteSelection();
      for (uint32_t codepoint : uiState->capturedChars) {
        appendUtf8(textBuffer, codepoint,
                   uiState->inputStateMap[textInputId.id].cursorPosition);
      }
    }
  }

  // --- RENDERING PASS ---
  float textOffsetValue = style.textOffset.value_or(0.0f);

  float cursorOffset = 0.0f;
  if (isFocused && font) {
    std::string leftSub = textBuffer.substr(
        0, uiState->inputStateMap[textInputId.id].cursorPosition);
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

  // Render Caret (When no range is selected)
  if (isFocused &&
      (uiState->inputStateMap[textInputId.id].selectionStart ==
       uiState->inputStateMap[textInputId.id].selectionEnd) &&
      font && elementData.found) {
    float caretH = fontHeight * 0.85f;
    float caretY = (textboxHeight - caretH) * 0.5f;

    Clay_ElementId caretId =
        Clay__HashString(Clay_String{false, 5, "Caret"}, textInputId.id);
    Clay__OpenElementWithId(caretId);

    Clay_ElementDeclaration caretDecl{};
    caretDecl.floating.offset = {static_cast<float>(padL) + cursorOffset,
                                 caretY};
    caretDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;

    caretDecl.backgroundColor = {220.0f, 220.0f, 220.0f, 255.0f};
    caretDecl.cornerRadius = {1.0f, 1.0f, 1.0f, 1.0f};
    caretDecl.layout = {.sizing = {.width = CLAY_SIZING_FIXED(2.0f),
                                   .height = CLAY_SIZING_FIXED(caretH)}};

    Clay__ConfigureOpenElement(caretDecl);
    Clay__CloseElement();
  }

  // Render Selection Highlight Box
  if (isFocused &&
      (uiState->inputStateMap[textInputId.id].selectionStart !=
       uiState->inputStateMap[textInputId.id].selectionEnd) &&
      font && elementData.found) {
    float startX =
        font
            ->measureText(textBuffer.substr(
                0, uiState->inputStateMap[textInputId.id].selectionStart))
            .x;
    float endX =
        font
            ->measureText(textBuffer.substr(
                0, uiState->inputStateMap[textInputId.id].selectionEnd))
            .x;
    float selectedTextWidth = endX - startX;

    float selectH = fontHeight * 0.85f;
    float selectY = (textboxHeight - selectH) * 0.5f;

    Clay_ElementId selectHighlightId = Clay__HashString(
        Clay_String{false, 18, "SelectionHighlight"}, textInputId.id);
    Clay__OpenElementWithId(selectHighlightId);

    Clay_ElementDeclaration selectDecl{};
    selectDecl.floating.offset = {static_cast<float>(padL) + startX, selectY};
    selectDecl.floating.attachTo = CLAY_ATTACH_TO_PARENT;

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
