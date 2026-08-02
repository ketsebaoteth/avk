#include "avk/avk_font.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/core/resources.h"
#include "ui/internal/context.h"
#include "ui/motion/AtomicMotion.h"
#include "ui/utils/color.h"

#include <algorithm>
#include <chrono>
#include <concepts>
#include <iostream>
#include <ostream>
#include <vector>

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

uint32_t getWordEndIndex(const std::string &str, uint32_t index) {
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
  return idx;
}

size_t getCodepointCount(const std::string &str) {
  size_t count = 0;
  uint32_t idx = 0;
  while (idx < str.size()) {
    idx = getNextCharIndex(str, idx);
    count++;
  }
  return count;
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

std::vector<uint32_t> decodeUtf8String(const std::string &str) {
  std::vector<uint32_t> codepoints;
  uint32_t idx = 0;
  while (idx < str.size()) {
    uint32_t cp = 0;
    unsigned char c = static_cast<unsigned char>(str[idx]);
    int len = getUtf8CharLength(c);

    if (len == 1) {
      cp = c;
    } else if (len == 2 && idx + 1 < str.size()) {
      cp =
          ((c & 0x1F) << 6) | (static_cast<unsigned char>(str[idx + 1]) & 0x3F);
    } else if (len == 3 && idx + 2 < str.size()) {
      cp = ((c & 0x0F) << 12) |
           ((static_cast<unsigned char>(str[idx + 1]) & 0x3F) << 6) |
           (static_cast<unsigned char>(str[idx + 2]) & 0x3F);
    } else if (len == 4 && idx + 3 < str.size()) {
      cp = ((c & 0x07) << 18) |
           ((static_cast<unsigned char>(str[idx + 1]) & 0x3F) << 12) |
           ((static_cast<unsigned char>(str[idx + 2]) & 0x3F) << 6) |
           (static_cast<unsigned char>(str[idx + 3]) & 0x3F);
    }

    if (cp != 0) {
      codepoints.push_back(cp);
    }
    idx += len;
  }
  return codepoints;
}

bool validateChar(uint32_t codepoint, const std::string &text, uint32_t pos,
                  const atomic::TextConfig &config) {
  using namespace atomic;
  if (config.maxLength > 0 && getCodepointCount(text) >= config.maxLength) {
    return false;
  }

  switch (config.type) {
  case TextInputType::NumberOnly:
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '.' ||
           codepoint == '-';
  case TextInputType::AlphaOnly:
    return (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= 'a' && codepoint <= 'z');
  case TextInputType::Alphanumeric:
    return (codepoint >= '0' && codepoint <= '9') ||
           (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= 'a' && codepoint <= 'z');
  case TextInputType::Custom:
    return config.customFilter ? config.customFilter(codepoint, text, pos)
                               : true;
  case TextInputType::Text:
  default:
    return true;
  }
}

float getSubstringAdvance(const std::string &str, avk::Font *font,
                          float physicalFontSize, float customGap) {
  if (str.empty() || !font)
    return 0.0f;
  float baseW = font->measureText(str, physicalFontSize).x;
  size_t count = getCodepointCount(str);
  return baseW + (static_cast<float>(count) * customGap);
}

uint32_t findWhereCursorLanded(const std::string &displayString,
                               avk::Font *font, float relativeMouseX,
                               float physicalFontSize, float customGap) {
  if (displayString.empty() || !font || relativeMouseX <= 0.0f) {
    return 0;
  }
  float prevWidth = 0.0f;
  for (uint32_t i = 0; i < displayString.size();
       i = getNextCharIndex(displayString, i)) {
    uint32_t nextI = getNextCharIndex(displayString, i);

    float currentWidth = getSubstringAdvance(displayString.substr(0, nextI),
                                             font, physicalFontSize, customGap);

    float midPoint = (prevWidth + currentWidth) * 0.5f;
    if (relativeMouseX < midPoint) {
      return i;
    }
    prevWidth = currentWidth;
  }
  return static_cast<uint32_t>(displayString.size());
}

} // namespace

namespace atomic {

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, const TextConfig &config,
                      uint32_t fontId) {
  auto *uiState = getUiState();
  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};

  const auto &rawStyle = modifier.getStyle();

  std::string labelId = rawStyle.elementLabel.value_or("TextInput");
  Clay_ElementId textInputId = utils::layout::getNextId(labelId.c_str());
  uint32_t elementId = textInputId.id;

  std::string selectBoxId = labelId + "_select";
  std::string caretLineId = labelId + "_caret";
  std::string dropCaretId = labelId + "_dropCaret";

  bool isDisabled = rawStyle.disabled.value_or(false) || inherited.disabled;
  bool isFocused = (!isDisabled && uiState->focusedElementId == elementId);
  bool wasHovered = !isDisabled && isHovered(elementId);

  glm::vec4 baseBg = rawStyle.backgroundColor.value_or("#ffffff"_hex);
  glm::vec4 baseStrokeColor =
      rawStyle.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  glm::vec4 hoverBg =
      glm::vec4(baseBg.r * 0.98f, baseBg.g * 0.98f, baseBg.b * 0.98f, baseBg.a);
  glm::vec4 focusBg =
      glm::vec4(baseBg.r * 0.95f, baseBg.g * 0.95f, baseBg.b * 0.95f, baseBg.a);
  glm::vec4 computedBg = isFocused ? focusBg : (wasHovered ? hoverBg : baseBg);
  glm::vec4 baseStrokeWidth =
      rawStyle.strokeThickness.value_or(glm::vec4(DEFAULT_BORDER_WIDTH));

  glm::vec4 activeBorderWidth = isFocused ? glm::vec4(1.5f) : baseStrokeWidth;

  constexpr float BASE_UI_SCALE = 2.0f;
  float monitorDpi =
      (getVeraApp() && getVeraApp()->getPrimaryMonitor().dpiScale > 0.0f)
          ? getVeraApp()->getPrimaryMonitor().dpiScale
          : 1.0f;
  float effectiveScale = monitorDpi * BASE_UI_SCALE;

  float customGapPhysical = config.customCharAdvance * effectiveScale;

  // Set default height to 42px to prevent container collapse when using
  // absolute custom renderers
  float defaultContainerHeight = rawStyle.height.value_or(42.0f);

  Modifier containerStyle = std::move(modifier)
                                .id(labelId)
                                .height(defaultContainerHeight) // Lock height
                                .background(computedBg)
                                .border(baseStrokeColor, activeBorderWidth)
                                .disabled(isDisabled)
                                .relative()
                                .row();

  if (!rawStyle.fontWeight.has_value())
    containerStyle = std::move(containerStyle).fontWeight(400.0f);
  if (!rawStyle.padLeft.has_value())
    containerStyle = std::move(containerStyle).padding(10, 8);
  if (!rawStyle.borderRadius.has_value())
    containerStyle = std::move(containerStyle).rounded(10.0f);
  if (!rawStyle.textColor.has_value())
    containerStyle = std::move(containerStyle).color(Colors::black[900]);

  if (!rawStyle.transitionSpec.has_value()) {
    containerStyle = std::move(containerStyle)
                         .transition(0.2f, motion::AnimationCurve::EaseOut());
  }

  const auto &finalStyle = containerStyle.getStyle();

  float logicalFontSize = finalStyle.fontSize.value_or(14.0f);
  float physicalFontSize = logicalFontSize * effectiveScale;

  float fontWeight = finalStyle.fontWeight.value_or(300.0f);
  glm::vec4 textColor = finalStyle.textColor.value_or(Colors::black[900]);

  uint16_t padL = finalStyle.padLeft.value_or(12);
  uint16_t padR = finalStyle.padRight.value_or(12);

  avk::Font *font = getFont(fontId != 0 ? fontId : getDefaultFontId());

  auto &inputState = uiState->inputStateMap[elementId];

  if (!isFocused) {
    inputState.selectionStart = 0;
    inputState.selectionEnd = 0;
    inputState.selectionAnchor = 0;
    inputState.isDraggingText = false;
    inputState.isPotentialTextDrag = false;
    inputState.wasArmedByDoubleClick = false;
    inputState.isDraggingSelectedText = false;
  }

  std::string displayString = textBuffer;
  if (config.isPassword && !textBuffer.empty()) {
    displayString.clear();
    size_t count = getCodepointCount(textBuffer);
    for (size_t i = 0; i < count; ++i) {
      displayString += "•";
    }
  }

  auto resetSelection = [&]() {
    inputState.selectionStart = 0;
    inputState.selectionEnd = 0;
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
    bool isBoxHovered = Clay_Hovered();
    if (isBoxHovered && !isDisabled) {
      uiState->anyInputBoxHovered = true;
    }

    ComputedLayout bounds = utils::layout::getComputedLayout(textInputId);
    float visibleWidth =
        bounds.found ? std::max(0.0f, bounds.width() - padL - padR) : 200.0f;

    // Single-line text input scroll tracking with custom gap support
    if (font) {
      float totalTextW = getSubstringAdvance(
          displayString, font, physicalFontSize, customGapPhysical);
      float rawCaretX = getSubstringAdvance(
          displayString.substr(0, inputState.cursorPosition), font,
          physicalFontSize, customGapPhysical);

      if (rawCaretX - inputState.scrollX > visibleWidth) {
        inputState.scrollX = rawCaretX - visibleWidth;
      } else if (rawCaretX - inputState.scrollX < 0.0f) {
        inputState.scrollX = rawCaretX;
      }

      float maxScroll = std::max(0.0f, totalTextW - visibleWidth);
      inputState.scrollX = std::clamp(inputState.scrollX, 0.0f, maxScroll);
    }

    if (bounds.found && !isDisabled) {
      float relativeMouseX =
          (uiState->pointerPos.x - (bounds.x() + padL)) + inputState.scrollX;

      if (isBoxHovered && uiState->pointerPressed) {
        uint32_t landedPos =
            findWhereCursorLanded(displayString, font, relativeMouseX,
                                  physicalFontSize, customGapPhysical);

        uiState->focusedElementId = elementId;
        isFocused = true;

        auto now = std::chrono::high_resolution_clock::now();
        float timeSinceLastClick =
            std::chrono::duration<float>(now - inputState.lastClickTime)
                .count();
        float clickDist =
            glm::length(uiState->pointerPos - inputState.lastClickPos);

        bool isDoubleClick =
            (timeSinceLastClick < 0.48f) && (clickDist < 22.0f);
        inputState.lastClickTime = now;
        inputState.lastClickPos = uiState->pointerPos;

        if (isDoubleClick) {
          uint32_t wStart = getPreviousWordIndex(displayString, landedPos);
          uint32_t wEnd = getWordEndIndex(displayString, landedPos);

          inputState.selectionStart = wStart;
          inputState.selectionEnd = wEnd;
          inputState.selectionAnchor = wStart;
          inputState.cursorPosition = wEnd;
        } else if (inputState.selectionStart != inputState.selectionEnd &&
                   landedPos >= inputState.selectionStart &&
                   landedPos <= inputState.selectionEnd) {
          inputState.isPotentialTextDrag = true;
          inputState.wasArmedByDoubleClick = false;
          inputState.dragStartMousePos = uiState->pointerPos;
        } else {
          inputState.selectionAnchor = landedPos;
          inputState.cursorPosition = landedPos;
          inputState.selectionStart = landedPos;
          inputState.selectionEnd = landedPos;
          inputState.isDraggingText = true;
          inputState.dragStartMousePos = uiState->pointerPos;
          uiState->selectAll = false;
          uiState->doingShiftSelect = false;
        }
      }

      if (isFocused && uiState->pointerDown) {
        float totalTextWidth =
            font ? getSubstringAdvance(displayString, font, physicalFontSize,
                                       customGapPhysical)
                 : 0.0f;
        float clampedMouseX = std::clamp(relativeMouseX, 0.0f, totalTextWidth);

        if (inputState.isPotentialTextDrag) {
          float mouseDist =
              glm::length(uiState->pointerPos - inputState.dragStartMousePos);
          if (mouseDist > 4.0f) {
            inputState.isDraggingSelectedText = true;
            inputState.isPotentialTextDrag = false;

            if (uiState) {
              uiState->activeDragText = textBuffer.substr(
                  inputState.selectionStart,
                  inputState.selectionEnd - inputState.selectionStart);
              uiState->dragSourceElementId = elementId;
            }
          }
        }

        if (inputState.isDraggingText) {
          uint32_t currentLandedPos =
              findWhereCursorLanded(displayString, font, clampedMouseX,
                                    physicalFontSize, customGapPhysical);
          inputState.cursorPosition = currentLandedPos;
          inputState.selectionStart =
              std::min(inputState.selectionAnchor, currentLandedPos);
          inputState.selectionEnd =
              std::max(inputState.selectionAnchor, currentLandedPos);
        }
      }
    }

    if (!uiState->pointerDown) {
      if (inputState.isPotentialTextDrag) {
        inputState.isPotentialTextDrag = false;

        float relativeMouseX =
            bounds.found ? ((uiState->pointerPos.x - (bounds.x() + padL)) +
                            inputState.scrollX)
                         : 0.0f;
        uint32_t landedPos =
            findWhereCursorLanded(displayString, font, relativeMouseX,
                                  physicalFontSize, customGapPhysical);

        uiState->focusedElementId = elementId;
        isFocused = true;
        inputState.cursorPosition = landedPos;
        inputState.selectionAnchor = landedPos;
        inputState.selectionStart = landedPos;
        inputState.selectionEnd = landedPos;
        resetSelection();
      }

      if (uiState && !uiState->activeDragText.empty() && bounds.found) {
        bool isOverThisInput =
            (uiState->pointerPos.x >= bounds.x() &&
             uiState->pointerPos.x <= bounds.x() + bounds.width() &&
             uiState->pointerPos.y >= bounds.y() &&
             uiState->pointerPos.y <= bounds.y() + bounds.height());

        if (isOverThisInput) {
          float relativeMouseX = (uiState->pointerPos.x - (bounds.x() + padL)) +
                                 inputState.scrollX;
          float totalTextWidth =
              font ? getSubstringAdvance(displayString, font, physicalFontSize,
                                         customGapPhysical)
                   : 0.0f;
          float clampedMouseX =
              std::clamp(relativeMouseX, 0.0f, totalTextWidth);
          uint32_t dropPos =
              findWhereCursorLanded(displayString, font, clampedMouseX,
                                    physicalFontSize, customGapPhysical);

          std::string draggedSlice = uiState->activeDragText;

          if (uiState->dragSourceElementId == elementId) {
            if (dropPos < inputState.selectionStart ||
                dropPos > inputState.selectionEnd) {
              textBuffer.erase(inputState.selectionStart, draggedSlice.size());
              uint32_t targetIdx =
                  (dropPos > inputState.selectionEnd)
                      ? (dropPos - static_cast<uint32_t>(draggedSlice.size()))
                      : dropPos;
              textBuffer.insert(targetIdx, draggedSlice);
              inputState.cursorPosition =
                  targetIdx + static_cast<uint32_t>(draggedSlice.size());
              inputState.selectionStart = targetIdx;
              inputState.selectionEnd = inputState.cursorPosition;
            }
          } else {
            textBuffer.insert(dropPos, draggedSlice);
            inputState.cursorPosition =
                dropPos + static_cast<uint32_t>(draggedSlice.size());
            uiState->focusedElementId = elementId;
            isFocused = true;
          }

          uiState->activeDragText.clear();
          uiState->dragSourceElementId = 0;
        }
      }

      inputState.isDraggingText = false;
      inputState.isDraggingSelectedText = false;
    }

    if (isFocused) {
      auto *app = getVeraApp();

      if (uiState->ctrlPressed && app) {
        if (uiState->copyTriggered &&
            inputState.selectionStart != inputState.selectionEnd) {
          std::string selectedText = textBuffer.substr(
              inputState.selectionStart,
              inputState.selectionEnd - inputState.selectionStart);
          app->setClipboardText(selectedText);
        }

        if (uiState->cutTriggered &&
            inputState.selectionStart != inputState.selectionEnd) {
          std::string selectedText = textBuffer.substr(
              inputState.selectionStart,
              inputState.selectionEnd - inputState.selectionStart);
          app->setClipboardText(selectedText);
          deleteSelection();
        }

        bool pasteTriggered = false;
        for (auto it = uiState->capturedChars.begin();
             it != uiState->capturedChars.end();) {
          if (*it == 'v' || *it == 'V' || *it == 22) {
            pasteTriggered = true;
            it = uiState->capturedChars.erase(it);
          } else {
            ++it;
          }
        }

        if (pasteTriggered) {
          deleteSelection();
          std::string clipboardText = app->getClipboardText();
          std::vector<uint32_t> codepoints = decodeUtf8String(clipboardText);
          for (uint32_t cp : codepoints) {
            if (validateChar(cp, textBuffer, inputState.cursorPosition,
                             config)) {
              appendUtf8(textBuffer, cp, inputState.cursorPosition);
            }
          }
        }
      }

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
              uiState->ctrlPressed
                  ? getPreviousWordIndex(textBuffer, inputState.cursorPosition)
                  : getPreviousCharIndex(textBuffer, inputState.cursorPosition);
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
          if (validateChar(codepoint, textBuffer, inputState.cursorPosition,
                           config)) {
            appendUtf8(textBuffer, codepoint, inputState.cursorPosition);
          }
        }
      }
    }

    float textboxHeight = bounds.found
                              ? bounds.height()
                              : finalStyle.height.value_or(DEFAULT_HEIGHT);

    displayString = textBuffer;
    if (config.isPassword && !textBuffer.empty()) {
      displayString.clear();
      size_t count = getCodepointCount(textBuffer);
      for (size_t i = 0; i < count; ++i) {
        displayString += "•";
      }
    }

    float measuredLineH =
        font ? font->getLineHeight(physicalFontSize) : physicalFontSize;

    // 1. PRESENTATION / CUSTOM RENDER HOOK OVERRIDE (Rendered FIRST)
    if (config.customRenderer && font && !displayString.empty()) {
      float localX = padL - inputState.scrollX;
      float localY = (textboxHeight - (measuredLineH / effectiveScale)) * 0.5f;
      config.customRenderer(displayString, localX, localY, logicalFontSize,
                            font, textColor);
    } else {
      std::string textToRender =
          textBuffer.empty() ? placeholder : displayString;
      glm::vec4 finalTextColor = textBuffer.empty() ? "#737373"_hex : textColor;

      Text(textToRender, fontId,
           Modifier()
               .color(finalTextColor)
               .fontSize(logicalFontSize)
               .fontWeight(fontWeight)
               .translate(-inputState.scrollX, 0.0f));
    }

    // 2. RENDER SELECTION BOX SECOND (On Top with Opacity)
    if (isFocused && (inputState.selectionStart != inputState.selectionEnd) &&
        font) {
      float startX = (getSubstringAdvance(
                          displayString.substr(0, inputState.selectionStart),
                          font, physicalFontSize, customGapPhysical) +
                      padL) -
                     inputState.scrollX;
      float endX =
          (getSubstringAdvance(displayString.substr(0, inputState.selectionEnd),
                               font, physicalFontSize, customGapPhysical) +
           padL) -
          inputState.scrollX;
      float selectW = endX - startX;
      float selectH = measuredLineH / 1.5f;
      float selectY = (textboxHeight - selectH) * 0.5f;

      Div(Modifier()
              .id(selectBoxId)
              .absolute()
              .pointerEvents(false)
              .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
              .offset(startX, selectY)
              .size(selectW, selectH)
              .background(glm::vec4(0.2f, 0.5f, 1.0f, 1.0f))
              .opacity(0.35f)); // Rendered on top of text with 35% opacity
    }

    // Render Drop Target Caret when dragging selected text
    if (uiState && !uiState->activeDragText.empty() && bounds.found && font) {
      bool isOverThisInput =
          (uiState->pointerPos.x >= bounds.x() &&
           uiState->pointerPos.x <= bounds.x() + bounds.width() &&
           uiState->pointerPos.y >= bounds.y() &&
           uiState->pointerPos.y <= bounds.y() + bounds.height());
      if (isOverThisInput) {
        float relativeMouseX =
            (uiState->pointerPos.x - (bounds.x() + padL)) + inputState.scrollX;
        float totalTextWidth =
            font ? getSubstringAdvance(displayString, font, physicalFontSize,
                                       customGapPhysical)
                 : 0.0f;
        float clampedMouseX = std::clamp(relativeMouseX, 0.0f, totalTextWidth);
        uint32_t dropPos =
            findWhereCursorLanded(displayString, font, clampedMouseX,
                                  physicalFontSize, customGapPhysical);

        float dropOffset =
            (getSubstringAdvance(displayString.substr(0, dropPos), font,
                                 physicalFontSize, customGapPhysical) +
             padL) -
            inputState.scrollX;
        float dropCaretH = measuredLineH / 1.5f;
        float dropCaretY = (textboxHeight - dropCaretH) * 0.5f;

        Div(Modifier()
                .id(dropCaretId)
                .absolute()
                .pointerEvents(false)
                .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
                .offset(dropOffset, dropCaretY)
                .size(2.0f, dropCaretH)
                .background(textColor));
      }
    }

    // Render Caret Line (Unique ID _caret)
    if (isFocused && font) {
      float cursorOffset = padL - inputState.scrollX;
      if (config.isPassword) {
        size_t numCodepoints =
            getCodepointCount(textBuffer.substr(0, inputState.cursorPosition));
        std::string maskedSub;
        for (size_t i = 0; i < numCodepoints; ++i) {
          maskedSub += "•";
        }
        cursorOffset += getSubstringAdvance(maskedSub, font, physicalFontSize,
                                            customGapPhysical);
      } else {
        cursorOffset += getSubstringAdvance(
            displayString.substr(0, inputState.cursorPosition), font,
            physicalFontSize, customGapPhysical);
      }
      float caretH = measuredLineH / 1.5f;
      float caretY = (textboxHeight - caretH) * 0.5f;

      Div(Modifier()
              .id(caretLineId)
              .absolute()
              .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
              .offset(cursorOffset, caretY)
              .size(2.0f, caretH)
              .background(textColor));
    }
  });

  // Floating Drag-and-Drop Selection Ghost
  if (uiState && !uiState->activeDragText.empty() && uiState->pointerDown) {
    std::string draggedSlice = uiState->activeDragText;
    std::string ghostId = labelId + "_ghost";

    Div(Modifier()
            .id(ghostId)
            .fixed()
            .left(uiState->pointerPos.x + 25.0f)
            .top(uiState->pointerPos.y + 25.0f),
        [&]() {
          Text(draggedSlice, fontId,
               Modifier().color(textColor).opacity(0.5).fontSize(
                   logicalFontSize));
        });
  }

  if (isDisabled) {
    result.hovered = false;
    result.pressed = false;
    result.clicked = false;
  }

  if (isFocused && uiState->enterPressed) {
    result.clicked = true;
    uiState->focusedElementId = 0;
  }

  return result;
}

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder, uint32_t fontId) {
  return TextInput(std::move(modifier), textBuffer, placeholder, TextConfig{},
                   fontId);
}

Interaction TextInput(std::string &textBuffer, const std::string &placeholder,
                      Modifier &&modifier) {
  return TextInput(std::move(modifier), textBuffer, placeholder, TextConfig{},
                   getDefaultFontId());
}

} // namespace atomic
