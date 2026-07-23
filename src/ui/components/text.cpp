#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {

Interaction Text(const std::string &text, Modifier &&modifier) {
  return Text(text, getDefaultFontId(), std::move(modifier));
}

Interaction Text(const std::string &text, uint32_t fontId,
                 Modifier &&modifier) {
  const auto &style = modifier.getStyle();

  Clay_ElementId textId = utils::layout::getNextId("Text");
  Clay__OpenElementWithId(textId);

  // 1. Resolve safe optional values
  glm::vec4 bg =
      style.backgroundColor.value_or(glm::vec4(0.0f)); // Default transparent
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));
  glm::vec4 textColor =
      style.textColor.value_or(glm::vec4(1.0f)); // Default solid white
  float textOffset = style.textOffset.value_or(-5.0f);

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_FIT(), // Default FIT
                 .height = style.height.has_value()
                               ? CLAY_SIZING_FIXED(style.height.value())
                               : CLAY_SIZING_FIT()},
      .padding = {style.padLeft.value_or(0), style.padRight.value_or(0),
                  style.padTop.value_or(0), style.padBottom.value_or(0)}};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // Pack the text offset directly into the userData pointer
  union {
    float f;
    void *p;
  } u;
  u.f = textOffset;
  decl.userData = u.p;

  Clay__ConfigureOpenElement(decl);

  Clay_String allocatedString = copyStringToClayBuffer(text);

  // Resolve text alignment
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

  avk::Font *font = getFont(fontId);
  uint32_t nativeSize = font ? font->getFontSize() : 16;

  Clay_TextElementConfig config{};
  config.fontId = static_cast<uint16_t>(fontId);
  config.fontSize = static_cast<uint16_t>(nativeSize);
  config.textColor = {textColor.r * 255.0f, textColor.g * 255.0f,
                      textColor.b * 255.0f, textColor.a * 255.0f};
  config.textAlignment = clayTextAlign;
  config.userData = u.p; // Pass offset through to the text render command

  Clay__OpenTextElement(allocatedString, config);
  Clay__CloseElement();

  // 2. Evaluate hover states
  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        radius);
  }

  bool isPressed = isHovered && utils::layout::getUiState()->pointerPressed;

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressed;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  return result;
}

} // namespace atomic
