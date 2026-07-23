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

  // 1. Resolve safe optionals
  glm::vec4 bg = style.backgroundColor.value_or(
      glm::vec4(0.0f)); // Default transparent image backdrop
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));

  // 2. Evaluate hover states
  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(imageId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        radius);
  }

  bool isPressed = isHovered && utils::layout::getUiState()->pointerPressed;

  auto *payload = new ImagePayload{textureIndex, tint};

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_GROW(), // Default GROW
                 .height = style.height.has_value()
                               ? CLAY_SIZING_FIXED(style.height.value())
                               : CLAY_SIZING_GROW()}};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

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
