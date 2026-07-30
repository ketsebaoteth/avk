#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/internal/cascadingStyle.h"
#include <unordered_map>

namespace atomic {

/**
 * @brief Core universal layout primitive managing flex direction, custom string
 * IDs, transitions, and style cascading.
 */
Interaction Div(Modifier &&modifier, const std::function<void()> &content) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId divId =
      rawStyle.elementLabel.has_value()
          ? utils::layout::getNextId(rawStyle.elementLabel.value().c_str())
          : utils::layout::getNextId("Div");

  Style style = utils::layout::resolveTransitions(divId.id, rawStyle);

  bool hasMargin =
      style.marginLeft.has_value() || style.marginRight.has_value() ||
      style.marginTop.has_value() || style.marginBottom.has_value();

  // Deterministic outer ID for margins
  Clay_ElementId outerId = divId;
  outerId.id += 0x6D417267; // "MArg" hex tag

  if (hasMargin) {
    Clay__OpenElementWithId(outerId);

    float ml = style.marginLeft.value_or(0.0f);
    float mr = style.marginRight.value_or(0.0f);
    float mt = style.marginTop.value_or(0.0f);
    float mb = style.marginBottom.value_or(0.0f);

    Clay_ElementDeclaration outerDecl{};

    // ✅ FIX: Use applyStyleToLayout so widthGrow(), heightGrow(), and sizing
    // work correctly on the outer wrapper!
    utils::layout::applyStyleToLayout(outerDecl, style);

    // Override padding on the outer container to act as margins
    outerDecl.layout.padding = {static_cast<uint16_t>(std::round(ml)),
                                static_cast<uint16_t>(std::round(mr)),
                                static_cast<uint16_t>(std::round(mt)),
                                static_cast<uint16_t>(std::round(mb))};

    // Outer margin container must be completely transparent (no
    // background/border)
    outerDecl.backgroundColor = {0, 0, 0, 0};

    Clay__ConfigureOpenElement(outerDecl);
  }

  Clay__OpenElementWithId(divId);

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  glm::vec4 bg = style.backgroundColor.value_or(
      style.gradient.has_value() ? glm::vec4(1.0f) : glm::vec4(0.0f));
  bg.a *= effectiveOpacity;

  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));
  glm::vec4 strokeColor = style.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  strokeColor.a *= effectiveOpacity;
  glm::vec4 strokeWidth = style.strokeThickness.value_or(glm::vec4(0.0f));

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

  // If there's a margin, the inner element should fill the outer container's
  // space completely
  Style innerStyle = style;
  if (hasMargin) {
    // Force inner element to grow to fill the margin wrapper
    innerStyle.width = 0;
    innerStyle.height = 0;
  }

  utils::layout::applyStyleToLayout(decl, innerStyle);

  // Apply internal padding to the inner element
  decl.layout.padding = {
      static_cast<uint16_t>(std::round(style.padLeft.value_or(0.0f))),
      static_cast<uint16_t>(std::round(style.padRight.value_or(0.0f))),
      static_cast<uint16_t>(std::round(style.padTop.value_or(0.0f))),
      static_cast<uint16_t>(std::round(style.padBottom.value_or(0.0f)))};
  decl.layout.childGap = static_cast<uint16_t>(style.childGap.value_or(0.0f));
  decl.layout.childAlignment = {.x = clayAlignX, .y = clayAlignY};
  decl.layout.layoutDirection = clayDir;

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  if (strokeWidth.x > 0.0f || strokeWidth.y > 0.0f || strokeWidth.z > 0.0f ||
      strokeWidth.w > 0.0f) {
    decl.border = {.color = {strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                             strokeColor.b * 255.0f, strokeColor.a * 255.0f},
                   .width = {static_cast<uint16_t>(strokeWidth.x),
                             static_cast<uint16_t>(strokeWidth.y),
                             static_cast<uint16_t>(strokeWidth.z),
                             static_cast<uint16_t>(strokeWidth.w)}};
  }

  auto pos = style.position.value_or(Position::Normal);
  utils::layout::PositioningContextGuard posGuard(divId.id, pos);
  utils::layout::StyleCascadeGuard styleGuard(style);

  decl.userData = utils::layout::createFramePayload(style);

  Clay__ConfigureOpenElement(decl);

  bool clayHovered = Clay_Hovered();

  if (content) {
    content();
  }

  // Close inner element
  Clay__CloseElement();

  // Close outer margin wrapper if present
  if (hasMargin) {
    Clay__CloseElement();
  }

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
    ElementLifecycleState lifecycle{};
    lifecycle.isMounted = true;
    lifecycle.isHovered = isHovered;
    lifecycle.isPressed = result.pressed;
    uiState->currentLifecycleMap[divId.id] = lifecycle;

    uiState->computedStyleMap[divId.id] = uiState->getActiveCascadingStyle();
  }

  return result;
}

} // namespace atomic
