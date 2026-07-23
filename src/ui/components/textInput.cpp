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

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, uint32_t fontId) {
  const auto &style = modifier.getStyle();

  // 1. Generate stable element ID
  Clay_ElementId textInputId = utils::layout::getNextId("TextInput");
  Clay__OpenElementWithId(textInputId);

  // Baseline visual style overrides: black[800] and 6px rounded corners
  glm::vec4 bg = style.backgroundColor.value_or(
      glm::vec4(0.06f, 0.06f, 0.06f, 1.0f)); // Colors::black[800]
  glm::vec4 radius =
      style.borderRadius.value_or(glm::vec4(6.0f)); // 6px rounded

  float textboxHeight = style.height.value_or(40.0f);
  avk::Font *font = getFont(fontId);
  float fontHeight = font ? font->getLineHeight() : 18.0f;

  // Anchor text vertically using Top-Alignment, keeping the baseline static
  uint16_t verticalPadding =
      static_cast<uint16_t>((textboxHeight - fontHeight) * 0.5f);
  uint16_t padL = style.padLeft.value_or(12);
  uint16_t padR = style.padRight.value_or(12);

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_GROW(),
                 .height = CLAY_SIZING_FIXED(textboxHeight)},
      .padding = {padL, padR, verticalPadding, verticalPadding},
      .childAlignment = {.x = CLAY_ALIGN_X_LEFT,
                         .y = CLAY_ALIGN_Y_TOP}, // Anchor to TOP
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

  // Click-to-Move Cursor: project cursor position dynamically
  if (isFocused && isHovered && uiState->pointerPressed && !justFocused &&
      font) {
    float relativeMouseX =
        uiState->pointerPos.x - (elementData.boundingBox.x + padL);
    float currentWidth = 0.0f;
    uint32_t clickedIndex = 0;

    for (uint32_t i = 0; i < textBuffer.size();
         i = getNextCharIndex(textBuffer, i)) {
      std::string sub = textBuffer.substr(0, i);
      currentWidth = font->measureText(sub).x;
      if (relativeMouseX < currentWidth) {
        clickedIndex = getPreviousCharIndex(textBuffer, i);
        break;
      }
      clickedIndex = static_cast<uint32_t>(textBuffer.size());
    }
    uiState->cursorPosition = clickedIndex;
    uiState->selectAll = false;
  }

  // Process keyboard input operations
  if (isFocused) {
    if (uiState->selectAll) {
      uiState->cursorPosition = static_cast<uint32_t>(textBuffer.size());
    }

    if (uiState->backspacePressed) {
      if (uiState->selectAll) {
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
      if (uiState->selectAll) {
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
      uiState->cursorPosition =
          getPreviousCharIndex(textBuffer, uiState->cursorPosition);
      uiState->selectAll = false;
    }
    if (uiState->rightArrowPressed) {
      uiState->cursorPosition =
          getNextCharIndex(textBuffer, uiState->cursorPosition);
      uiState->selectAll = false;
    }

    if (!uiState->capturedChars.empty()) {
      if (uiState->selectAll) {
        textBuffer.clear();
        uiState->cursorPosition = 0;
        uiState->selectAll = false;
      }
      for (uint32_t codepoint : uiState->capturedChars) {
        appendUtf8(textBuffer, codepoint, uiState->cursorPosition);
      }
    }
  }

  float textOffsetValue = style.textOffset.value_or(-3.0f);

  // Calculate cursor offset for floating rendering
  float cursorOffset = 0.0f;
  if (isFocused && font) {
    std::string leftSub = textBuffer.substr(0, uiState->cursorPosition);
    cursorOffset = font->measureText(leftSub).x;
  }

  // 4. THE FLOATING SELECTION FIX (Aligns perfectly with text margins!)
  if (isFocused && uiState->selectAll && font && elementData.found) {
    float textW = font->measureText(textBuffer).x;

    Clay__OpenElementWithId(utils::layout::getNextId("SelectionHighlight"));

    Clay_ElementDeclaration selectDecl{};
    selectDecl.floating = {// Offset exactly by left padding and vertical
                           // padding to match baseline!
                           .offset = {static_cast<float>(padL),
                                      static_cast<float>(verticalPadding)},
                           .attachTo = CLAY_ATTACH_TO_PARENT};
    selectDecl.backgroundColor = {46.0f, 117.0f, 209.0f,
                                  255.0f}; // Blue highlight
    selectDecl.cornerRadius = {3.0f, 3.0f, 3.0f, 3.0f};
    selectDecl.layout = {.sizing = {.width = CLAY_SIZING_FIXED(textW),
                                    .height = CLAY_SIZING_FIXED(fontHeight)}};

    Clay__ConfigureOpenElement(selectDecl);
    Clay__CloseElement();
  }

  // Render text or placeholder
  if (textBuffer.empty() && !isFocused) {
    Text(placeholder, fontId,
         DefaultModifier()
             .color(glm::vec4(0.40f, 0.40f, 0.40f, 1.0f))
             .textOffset(textOffsetValue));
  } else {
    Text(textBuffer, fontId,
         DefaultModifier().color(Colors::white).textOffset(textOffsetValue));
  }

  // 5. THE FLOATING CURSOR FIX (Aligns perfectly with text margins!)
  if (isFocused && !uiState->selectAll && font && elementData.found) {
    float caretH = fontHeight * 0.85f;
    float caretY = (fontHeight - caretH) * 0.5f;

    Clay__OpenElementWithId(utils::layout::getNextId("Caret"));

    Clay_ElementDeclaration caretDecl{};
    caretDecl.floating = {
        // Offset exactly by (padL + cursorOffset) and (verticalPadding +
        // caretY)
        .offset = {static_cast<float>(padL) + cursorOffset,
                   static_cast<float>(verticalPadding) + caretY},
        .attachTo = CLAY_ATTACH_TO_PARENT};
    caretDecl.backgroundColor = {220.0f, 220.0f, 220.0f, 255.0f};
    caretDecl.cornerRadius = {1.0f, 1.0f, 1.0f, 1.0f};
    caretDecl.layout = {.sizing = {.width = CLAY_SIZING_FIXED(1.5f),
                                   .height = CLAY_SIZING_FIXED(caretH)}};

    Clay__ConfigureOpenElement(caretDecl);
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
