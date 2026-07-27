#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {

/**
 * @brief Default font overload for Text.
 */
Interaction Text(const std::string &text, Modifier &&modifier) {
  return Text(text, 0, std::move(modifier));
}

/**
 * @brief Auto-generating ID overload for Text.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Modifier &&modifier) {
  Clay_ElementId textId = utils::layout::getNextId("Text");
  return Text(text, fontId, textId, std::move(modifier));
}

/**
 * @brief Core Text primitive resolving cascading style inheritance.
 */
Interaction Text(const std::string &text, uint32_t fontId,
                 Clay_ElementId textId, Modifier &&modifier) {
  const auto &style = modifier.getStyle();
  auto *uiState = utils::layout::getUiState();

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};

  uint32_t finalFontId =
      (fontId != 0)
          ? fontId
          : (inherited.fontId != 0 ? inherited.fontId : getDefaultFontId());
  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));
  glm::vec4 textColor = style.textColor.value_or(inherited.textColor);
  float textOffset = style.textOffset.value_or(inherited.textOffset);

  avk::Font *font = getFont(finalFontId);
  float fontHeight = font ? font->getLineHeight() : 18.0f;

  Clay__OpenElementWithId(textId);

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_FIT(),
                 .height = style.height.has_value()
                               ? CLAY_SIZING_FIXED(style.height.value())
                               : CLAY_SIZING_FIXED(fontHeight)},
      .padding = {0, 0, 0, 0}};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  utils::layout::applyStyleToLayout(decl, style);

  auto *payload = utils::layout::createFramePayload(style, std::nullopt,
                                                    std::nullopt, textOffset);
  decl.userData = payload;

  Clay__ConfigureOpenElement(decl);

  Clay_String allocatedString = copyStringToClayBuffer(text);

  Clay_TextAlignment clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
  if (style.alignX.has_value()) {
    switch (style.alignX.value()) {
    case AlignmentX::Center:
      clayTextAlign = CLAY_TEXT_ALIGN_CENTER;
      break;
    case AlignmentX::Right:
      clayTextAlign = CLAY_TEXT_ALIGN_RIGHT;
      break;
    default:
      clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
      break;
    }
  }

  uint32_t nativeSize = font ? font->getFontSize() : 16;

  Clay_TextElementConfig config{};
  config.fontId = static_cast<uint16_t>(finalFontId);
  config.fontSize = static_cast<uint16_t>(nativeSize);
  config.textColor = {textColor.r * 255.0f, textColor.g * 255.0f,
                      textColor.b * 255.0f, textColor.a * 255.0f};
  config.textAlignment = clayTextAlign;
  config.userData = payload;

  Clay__OpenTextElement(allocatedString, config);
  Clay__CloseElement();

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool isPressed = isHovered && uiState->pointerDown;

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressed;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  if (uiState) {
    CascadingStyle resolved = inherited;
    resolved.textColor = textColor;
    resolved.textOffset = textOffset;
    resolved.fontId = finalFontId;
    uiState->computedStyleMap[textId.id] = resolved;
  }

  return result;
}
} // namespace atomic
