#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {
Interaction Button(Modifier &&modifier) {
  const auto &style = modifier.getStyle();
  Clay_ElementId buttonId = utils::layout::getNextId("Button");

  Clay__OpenElementWithId(buttonId);

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width)
                                         : CLAY_SIZING_GROW(),
                 .height = style.hasHeight ? CLAY_SIZING_FIXED(style.height)
                                           : CLAY_SIZING_GROW()}};

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(buttonId);
  if (elementData.found) {
    // an sdf based check to work with rounded things
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        style.borderRadius);
  }

  bool isPressed = isHovered && utils::layout::getUiState()->pointerPressed;

  // todo: should we tho ?
  // Perform automatic visual feedback
  Clay_Color color = {
      style.backgroundColor.r * 255.0f, style.backgroundColor.g * 255.0f,
      style.backgroundColor.b * 255.0f, style.backgroundColor.a * 255.0f};

  if (isPressed) {
    color.r *= 0.8f;
    color.g *= 0.8f;
    color.b *= 0.8f; // Darken on press
  } else if (isHovered) {
    color.r = std::min(color.r * 1.15f, 255.0f); // Brighten on hover
    color.g = std::min(color.g * 1.15f, 255.0f);
    color.b = std::min(color.b * 1.15f, 255.0f);
  }

  decl.backgroundColor = color;
  decl.cornerRadius = {style.borderRadius.x, style.borderRadius.y,
                       style.borderRadius.z, style.borderRadius.w};

  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();

  // 3. Populate and return Interaction State block
  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressed;

  // Click is registered if released this frame while hovered on the active
  // rounded shape
  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  return result;
}

} // namespace atomic
