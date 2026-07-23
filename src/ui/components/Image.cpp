#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {

Interaction Image(Modifier &&modifier, uint32_t textureIndex,
                  const glm::vec4 &tint) {
  const auto &style = modifier.getStyle();

  Clay_ElementId imageId = utils::layout::getNextId("Image");
  Clay__OpenElementWithId(imageId);

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(imageId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        style.borderRadius);
  }

  bool isPressed = isHovered && utils::layout::getUiState()->pointerPressed;

  auto *payload = new ImagePayload{textureIndex, tint};

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width)
                                         : CLAY_SIZING_GROW(),
                 .height = style.hasHeight ? CLAY_SIZING_FIXED(style.height)
                                           : CLAY_SIZING_GROW()}};

  decl.backgroundColor = {
      style.backgroundColor.r * 255.0f, style.backgroundColor.g * 255.0f,
      style.backgroundColor.b * 255.0f, style.backgroundColor.a * 255.0f};

  decl.cornerRadius = {style.borderRadius.x, style.borderRadius.y,
                       style.borderRadius.z, style.borderRadius.w};

  decl.userData = payload;
  decl.image = {.imageData = payload};

  Clay__ConfigureOpenElement(decl);
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
