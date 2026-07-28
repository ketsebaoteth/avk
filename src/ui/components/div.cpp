#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/internal/cascadingStyle.h"
#include <unordered_map>

namespace atomic {

/**
 * @brief Universal layout container managing flex direction, position contexts,
 * and style cascading.
 */
Interaction Div(Modifier &&modifier, const std::function<void()> &content) {
  const auto &style = modifier.getStyle();
  auto *uiState = getUiState();

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  Clay_ElementId divId = utils::layout::getNextId("Div");
  Clay__OpenElementWithId(divId);

  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  bg.a *= effectiveOpacity;
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));
  glm::vec4 strokeColor = style.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  strokeColor.a *= effectiveOpacity;
  float strokeWidth = style.strokeThickness.value_or(0.0f);

  LayoutDirection dir = style.direction.value_or(LayoutDirection::Row);
  Clay_LayoutDirection clayDir = (dir == LayoutDirection::Column)
                                     ? CLAY_TOP_TO_BOTTOM
                                     : CLAY_LEFT_TO_RIGHT;

  Clay_LayoutAlignmentX clayAlignX =
      style.alignX.has_value() && style.alignX.value() == AlignmentX::Center
          ? CLAY_ALIGN_X_CENTER
      : style.alignX.has_value() && style.alignX.value() == AlignmentX::Right
          ? CLAY_ALIGN_X_RIGHT
          : CLAY_ALIGN_X_LEFT;

  Clay_LayoutAlignmentY clayAlignY =
      style.alignY.has_value() && style.alignY.value() == AlignmentY::Center
          ? CLAY_ALIGN_Y_CENTER
      : style.alignY.has_value() && style.alignY.value() == AlignmentY::Bottom
          ? CLAY_ALIGN_Y_BOTTOM
          : CLAY_ALIGN_Y_TOP;

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_FIT(),
                 .height = style.height.has_value()
                               ? CLAY_SIZING_FIXED(style.height.value())
                               : CLAY_SIZING_FIT()},
      .padding = {style.padLeft.value_or(0), style.padRight.value_or(0),
                  style.padTop.value_or(0), style.padBottom.value_or(0)},
      .childGap = static_cast<uint16_t>(style.childGap.value_or(0.0f)),
      .childAlignment = {.x = clayAlignX, .y = clayAlignY},
      .layoutDirection = clayDir};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  if (strokeWidth > 0.0f) {
    uint16_t w = static_cast<uint16_t>(strokeWidth);
    decl.border = {.color = {strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                             strokeColor.b * 255.0f, strokeColor.a * 255.0f},
                   .width = {w, w, w, w}};
  }

  utils::layout::applyStyleToLayout(decl, style);

  auto pos = style.position.value_or(Position::Normal);
  utils::layout::PositioningContextGuard posGuard(divId.id, pos);
  utils::layout::StyleCascadeGuard styleGuard(style);

  decl.userData = utils::layout::createFramePayload(style);

  Clay__ConfigureOpenElement(decl);

  bool clayHovered = Clay_Hovered();

  if (content) {
    content();
  }

  Clay__CloseElement();

  bool isHovered = false;
  if (clayHovered) {
    Clay_ElementData elementData = Clay_GetElementData(divId);
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
    uiState->computedStyleMap[divId.id] = uiState->getActiveCascadingStyle();
  }

  return result;
}

} // namespace atomic
