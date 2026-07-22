#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {

Interaction Button(Modifier &&modifier, const std::function<void()> &content) {
  const auto &style = modifier.getStyle();

  Clay_ElementId buttonId = utils::layout::getNextId("Button");
  Clay__OpenElementWithId(buttonId);

  // 1. Convert your style enums to Clay layout options
  Clay_LayoutAlignmentX clayAlignX = CLAY_ALIGN_X_LEFT;
  switch (style.alignX) {
  case AlignmentX::Center:
    clayAlignX = CLAY_ALIGN_X_CENTER;
    break;
  case AlignmentX::Right:
    clayAlignX = CLAY_ALIGN_X_RIGHT;
    break;
  default:
    clayAlignX = CLAY_ALIGN_X_LEFT;
    break;
  }

  Clay_LayoutAlignmentY clayAlignY = CLAY_ALIGN_Y_TOP;
  switch (style.alignY) {
  case AlignmentY::Center:
    clayAlignY = CLAY_ALIGN_Y_CENTER;
    break;
  case AlignmentY::Bottom:
    clayAlignY = CLAY_ALIGN_Y_BOTTOM;
    break;
  default:
    clayAlignY = CLAY_ALIGN_Y_TOP;
    break;
  }

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width)
                                         : CLAY_SIZING_GROW(),
                 .height = style.hasHeight ? CLAY_SIZING_FIXED(style.height)
                                           : CLAY_SIZING_GROW()},
      .padding = {style.padLeft, style.padRight, style.padTop, style.padBottom},
      .childGap = style.childGap,
      .childAlignment = {.x = clayAlignX, .y = clayAlignY},
      .layoutDirection = CLAY_LEFT_TO_RIGHT

  };

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(buttonId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        style.borderRadius);
  }

  bool isPressed = isHovered && utils::layout::getUiState()->pointerPressed;

  Clay_Color color = {
      style.backgroundColor.r * 255.0f, style.backgroundColor.g * 255.0f,
      style.backgroundColor.b * 255.0f, style.backgroundColor.a * 255.0f};

  if (isPressed) {
    color.r *= 0.8f;
    color.g *= 0.8f;
    color.b *= 0.8f;
  } else if (isHovered) {
    color.r = std::min(color.r * 1.15f, 255.0f);
    color.g = std::min(color.g * 1.15f, 255.0f);
    color.b = std::min(color.b * 1.15f, 255.0f);
  }

  decl.backgroundColor = color;
  decl.cornerRadius = {style.borderRadius.x, style.borderRadius.y,
                       style.borderRadius.z, style.borderRadius.w};

  Clay__ConfigureOpenElement(decl);

  // If the user nested any child elements inside this button, execute them
  // here!
  if (content) {
    content();
  }

  Clay__CloseElement();

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
