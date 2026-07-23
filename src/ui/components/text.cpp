#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "glm/ext/scalar_uint_sized.hpp"
#include <bit>

namespace atomic {

Interaction Text(const std::string &text, uint32_t fontId,
                 Modifier &&modifier) {
  const auto &style = modifier.getStyle();

  Clay_ElementId textId = utils::layout::getNextId("Text");
  Clay__OpenElementWithId(textId);

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width)
                                         : CLAY_SIZING_FIT(),
                 .height = style.hasHeight ? CLAY_SIZING_FIXED(style.height)
                                           : CLAY_SIZING_FIT()},
      .padding = {style.padLeft, style.padRight, style.padTop,
                  style.padBottom}};

  decl.backgroundColor = {
      style.backgroundColor.r * 255.0f, style.backgroundColor.g * 255.0f,
      style.backgroundColor.b * 255.0f, style.backgroundColor.a * 255.0f};

  decl.cornerRadius = {style.borderRadius.x, style.borderRadius.y,
                       style.borderRadius.z, style.borderRadius.w};

  Clay__ConfigureOpenElement(decl);

  Clay_String allocatedString = copyStringToClayBuffer(text);

  Clay_TextAlignment clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
  switch (style.alignX) {
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

  // Pack the float offset directly into a union
  union {
    float f;
    void *p;
  } u;
  u.f = style.textOffset;

  Clay_TextElementConfig config{};
  config.fontId = static_cast<uint16_t>(fontId);
  auto font = getFont(fontId);
  uint32_t nativeSize = font ? font->getFontSize() : 16;
  config.fontSize = static_cast<uint16_t>(nativeSize);
  config.textColor =
      Clay_Color{style.textColor.r * 255.0f, style.textColor.g * 255.0f,
                 style.textColor.b * 255.0f, style.textColor.a * 255.0f};
  config.textAlignment = clayTextAlign;

  config.userData = u.p;

  Clay__OpenTextElement(allocatedString, config);

  Clay__CloseElement();

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        style.borderRadius);
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
