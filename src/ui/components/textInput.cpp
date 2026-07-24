#include "avk/atomic_ui.h"
#include "avk/avk_font.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/color.h"
#include "ui/components.h"

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

  // Baseline visuals
  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(Colors::black[800]));
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(6.0f));

  float textboxHeight = style.height.value_or(40.0f);
  avk::Font *font = getFont(fontId);
  // fontsize
  float fontHeight = font ? font->getLineHeight() : 18.0f;

  uint16_t padT = style.padTop.value_or(12);
  uint16_t padB = style.padBottom.value_or(12);
  uint16_t padL = style.padLeft.value_or(12);
  uint16_t padR = style.padRight.value_or(12);

  // main rect body
  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_GROW(),
                 .height = CLAY_SIZING_FIXED(textboxHeight)},
      .padding = {padL, padR, padT, padB},
      .childAlignment = {.x = CLAY_ALIGN_X_LEFT,
                         .y = CLAY_ALIGN_Y_CENTER}, // Anchor center
      .layoutDirection = CLAY_LEFT_TO_RIGHT};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  Clay__ConfigureOpenElement(decl);

  // Evaluate mouse hover and clicks
  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textInputId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        radius);
  }

  auto *uiState = utils::layout::getUiState();

  if (isHovered) {
    uiState->anyInputBoxHovered = true;
  }

  bool justFocused = false;
  if (isHovered && uiState->pointerPressed &&
      uiState->focusedElementId != textInputId.id) {
    uiState->focusedElementId = textInputId.id;
    uiState->cursorPosition = static_cast<uint32_t>(textBuffer.size());
    uiState->selectAll = false;
    justFocused = true;
  }

  bool isFocused = (uiState->focusedElementId == textInputId.id);

  auto resetSelection = [&]() {
    uiState->selectionStart = 0;
    uiState->selectionEnd = 0;
  };

  // find where to place cursor on click
  if (isFocused && isHovered && uiState->pointerPressed && !justFocused &&
      font) {
    float relativeMouseX =
        uiState->pointerPos.x - (elementData.boundingBox.x + padL);
    uiState->cursorPosition =
        findWhereCursorLanded(textBuffer, font, relativeMouseX);
    uiState->selectAll = false;
    resetSelection();
  }

  // Process keyboard input operations
  if (isFocused) {
    if (uiState->selectAll) {
      uiState->cursorPosition = static_cast<uint32_t>(textBuffer.size());
      uiState->selectionStart = 0;
      uiState->selectionEnd = static_cast<uint32_t>(textBuffer.size());
      uiState->selectAll = false;
    }

    if (uiState->backspacePressed) {
      if (uiState->selectionStart != uiState->selectionEnd) {
        // Todo: for now only
        textBuffer.clear();
        uiState->cursorPosition = 0;
        uiState->selectAll = false;
      } else if (uiState->cursorPosition > 0) {
        uint32_t prev =
            getPreviousCharIndex(textBuffer, uiState->cursorPosition);
        uint32_t len = uiState->cursorPosition - prev;
        textBuffer.erase(prev, len);
        uiState->cursorPosition = prev;
      }
    }

    if (uiState->deletePressed) {
      if (uiState->selectionStart != uiState->selectionEnd) {
        textBuffer.clear();
        uiState->cursorPosition = 0;
        uiState->selectAll = false;
      } else if (uiState->cursorPosition < textBuffer.size()) {
        uint32_t next = getNextCharIndex(textBuffer, uiState->cursorPosition);
        textBuffer.erase(uiState->cursorPosition,
                         next - uiState->cursorPosition);
      }
    }

    if (uiState->leftArrowPressed) {
      if (uiState->ctrlPressed) {
        uiState->cursorPosition =
            getPreviousWordIndex(textBuffer, uiState->cursorPosition);
      } else {
        uiState->cursorPosition =
            getPreviousCharIndex(textBuffer, uiState->cursorPosition);
      }
      uiState->selectAll = false;
    }
    if (uiState->rightArrowPressed) {
      if (uiState->ctrlPressed) {
        uiState->cursorPosition =
            getNextWordIndex(textBuffer, uiState->cursorPosition);
      } else {
        uiState->cursorPosition =
            getNextCharIndex(textBuffer, uiState->cursorPosition);
      }
      uiState->selectAll = false;
    }

    if (!uiState->capturedChars.empty()) {
      if (uiState->selectionStart != uiState->selectionEnd) {
        textBuffer.clear();
        resetSelection();
        uiState->cursorPosition = 0;
        uiState->selectAll = false;
      }
      for (uint32_t codepoint : uiState->capturedChars) {
        appendUtf8(textBuffer, codepoint, uiState->cursorPosition);
      }
    }
  }

  float textOffsetValue = style.textOffset.value_or(-4.0f);

  // Calculate cursor offset for floating rendering
  float cursorOffset = 0.0f;
  if (isFocused && font) {
    std::string leftSub = textBuffer.substr(0, uiState->cursorPosition);
    cursorOffset = font->measureText(leftSub).x;
  }

  // Render text or placeholder
  if (textBuffer.empty() && !isFocused) {
    Text(placeholder, fontId,
         DefaultModifier()
             .color(glm::vec4(Colors::gray[700]))
             .textOffset(textOffsetValue));
  } else {
    Text(textBuffer, fontId,
         DefaultModifier().color(Colors::white).textOffset(textOffsetValue));
  }

  if (isFocused && !uiState->selectAll && font && elementData.found) {
    float caretH = fontHeight * 0.85f;
    float caretY = (textboxHeight / 2 - caretH) * 0.5f;

    Clay__OpenElementWithId(utils::layout::getNextId("Caret"));

    Clay_ElementDeclaration caretDecl{};
    caretDecl.floating = {.offset = {static_cast<float>(padL) + cursorOffset,
                                     static_cast<float>(padT) + caretY},
                          .attachTo = CLAY_ATTACH_TO_PARENT};
    caretDecl.backgroundColor = {220.0f, 220.0f, 220.0f, 255.0f};
    caretDecl.cornerRadius = {1.0f, 1.0f, 1.0f, 1.0f};
    caretDecl.layout = {.sizing = {.width = CLAY_SIZING_FIXED(2.0f),
                                   .height = CLAY_SIZING_FIXED(caretH)}};

    Clay__ConfigureOpenElement(caretDecl);
    Clay__CloseElement();
  }

  // highlight box
  if (isFocused && (uiState->selectionStart != uiState->selectionEnd) && font &&
      elementData.found) {
    std::string selectedText =
        textBuffer.substr(uiState->selectionStart,
                          uiState->selectionEnd - uiState->selectionStart);
    std::string beforeSelection = textBuffer.substr(0, uiState->selectionStart);

    float selectedTextWidth = font->measureText(selectedText).x;
    float beforeSelectionWidth = font->measureText(beforeSelection).x;
    float selectH = fontHeight * 0.85f;
    float selectY = (textboxHeight / 2 - selectH) * 0.5f;

    Clay__OpenElementWithId(utils::layout::getNextId("SelectionHighlight"));

    Clay_ElementDeclaration selectDecl{};
    selectDecl.floating = {
        .offset = {static_cast<float>(padL + beforeSelectionWidth),
                   static_cast<float>(padT) + selectY},
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
    uiState->focusedElementId = 0;
  }

  return result;
}

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder) {
  return TextInput(std::move(modifier), textBuffer, placeholder,
                   getDefaultFontId());
}

} // namespace atomic
