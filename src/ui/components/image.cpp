#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {

/**
 * @brief Renders a GPU texture image with CSS transforms, Object-Fit, and
 * cascading opacity.
 */
Interaction Image(Modifier &&modifier, uint32_t textureIndex,
                  const glm::vec4 &tint) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId imageId =
      rawStyle.elementLabel.has_value()
          ? utils::layout::getNextId(rawStyle.elementLabel.value().c_str())
          : utils::layout::getNextId("Image");

  Style style = utils::layout::resolveTransitions(imageId.id, rawStyle);

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  glm::vec4 finalTint = tint;
  finalTint.a *= effectiveOpacity;

  Clay__OpenElementWithId(imageId);

  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  bg.a *= effectiveOpacity;
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));

  auto *payload = utils::layout::createFramePayload(
      style, std::nullopt, std::nullopt, 0.0f, textureIndex, finalTint);

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_GROW(),
                 .height = style.height.has_value()
                               ? CLAY_SIZING_FIXED(style.height.value())
                               : CLAY_SIZING_GROW()}};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  utils::layout::applyStyleToLayout(decl, style);

  decl.userData = payload;
  decl.image = {.imageData = payload};

  Clay__ConfigureOpenElement(decl);

  bool clayHovered = Clay_Hovered();

  Clay__CloseElement();

  bool isHovered = false;
  if (clayHovered) {
    Clay_ElementData elementData = Clay_GetElementData(imageId);
    if (elementData.found) {
      isHovered = utils::ui::isPointerOverRoundedBox(
          uiState->pointerPos, elementData.boundingBox, radius);
    }
  }

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isHovered && uiState->pointerDown;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  if (uiState) {
    ElementLifecycleState lifecycle{};
    lifecycle.isMounted = true;
    lifecycle.isHovered = isHovered;
    lifecycle.isPressed = result.pressed;
    uiState->currentLifecycleMap[imageId.id] = lifecycle;

    uiState->computedStyleMap[imageId.id] = uiState->getActiveCascadingStyle();
  }

  return result;
}

} // namespace atomic
