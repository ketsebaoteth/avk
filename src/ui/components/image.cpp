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
  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
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

  // Use the memory-safe frame allocator to register image payloads
  auto *payload = utils::layout::createFramePayload(
      style, std::nullopt, std::nullopt, 0.0f, textureIndex, tint);

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // 1-LINE SYSTEM HOOK: Apply layouts, sizes, margins, and Clay floating
  // absolute layouts!
  utils::layout::applyStyleToLayout(decl, style);

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
