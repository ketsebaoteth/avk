#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/internal/cascadingStyle.h"
#include "ui/motion/AtomicMotion.h"
#include "ui/style/themeManager.h"

#include <cmath>
#include <unordered_map>

namespace atomic {

/**
 * @brief Core universal layout primitive managing flex direction, margins,
 * transitions, style cascading, theme scaling, and transform/translation
 * offsets.
 */
Interaction Div(Modifier &&modifier, const std::function<void()> &content) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();
  auto &tm = ThemeManager::getInstance();

  // ⚡ ENUM O(1) LOOKUPS
  float spacingMult =
      tm.getVariable<float>(ThemeVarId::SpacingMultiplier, 1.0f);
  float borderRadiusMult =
      tm.getVariable<float>(ThemeVarId::BorderRadiusMultiplier, 1.0f);
  float borderWidthMult =
      tm.getVariable<float>(ThemeVarId::BorderWidthMultiplier, 1.0f);

  Clay_ElementId divId =
      rawStyle.elementLabel.has_value()
          ? utils::layout::getNextId(rawStyle.elementLabel.value().c_str())
          : utils::layout::getNextId("Div");
  Style style = utils::layout::resolveTransitions(divId.id, rawStyle);
  bool hasMargin =
      style.marginLeft.has_value() || style.marginRight.has_value() ||
      style.marginTop.has_value() || style.marginBottom.has_value();

  Clay_ElementId outerId = divId;
  outerId.id += 0x6D417267;
  if (hasMargin) {
    Clay__OpenElementWithId(outerId);
    float ml = style.marginLeft.value_or(0.0f) * spacingMult;
    float mr = style.marginRight.value_or(0.0f) * spacingMult;
    float mt = style.marginTop.value_or(0.0f) * spacingMult;
    float mb = style.marginBottom.value_or(0.0f) * spacingMult;
    Clay_ElementDeclaration outerDecl{};
    utils::layout::applyStyleToLayout(outerDecl, style);
    outerDecl.layout.padding = {static_cast<uint16_t>(std::round(ml)),
                                static_cast<uint16_t>(std::round(mr)),
                                static_cast<uint16_t>(std::round(mt)),
                                static_cast<uint16_t>(std::round(mb))};
    outerDecl.backgroundColor = {0, 0, 0, 0};
    Clay__ConfigureOpenElement(outerDecl);
  }

  Clay__OpenElementWithId(divId);

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  glm::vec4 themeBg =
      tm.getVariable<glm::vec4>(ThemeVarId::ColorBgSurface, glm::vec4(0.0f));
  glm::vec4 bg = style.backgroundColor.value_or(
      style.gradient.has_value() ? glm::vec4(1.0f) : themeBg);
  bg.a *= effectiveOpacity;

  glm::vec4 themeRadius =
      glm::vec4(tm.getVariable<float>(ThemeVarId::BorderRadiusLg, 0.0f));
  glm::vec4 radius =
      (style.borderRadius.value_or(themeRadius)) * borderRadiusMult;

  glm::vec4 themeBorderColor = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorBorderNormal, DEFAULT_BORDER_NORMAL);
  glm::vec4 strokeColor = style.strokeColor.value_or(themeBorderColor);
  strokeColor.a *= effectiveOpacity;

  glm::vec4 themeBorderWidth =
      glm::vec4(tm.getVariable<float>(ThemeVarId::BorderWidthNone, 0.0f));
  glm::vec4 strokeWidth =
      (style.strokeThickness.value_or(themeBorderWidth)) * borderWidthMult;

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
  Style innerStyle = style;
  if (hasMargin) {
    innerStyle.width = 0;
    innerStyle.height = 0;
  }
  utils::layout::applyStyleToLayout(decl, innerStyle);

  const float padL = style.padLeft.value_or(0.0f) * spacingMult;
  const float padR = style.padRight.value_or(0.0f) * spacingMult;
  const float padT = style.padTop.value_or(0.0f) * spacingMult;
  const float padB = style.padBottom.value_or(0.0f) * spacingMult;

  decl.layout.padding = {static_cast<uint16_t>(std::round(padL)),
                         static_cast<uint16_t>(std::round(padR)),
                         static_cast<uint16_t>(std::round(padT)),
                         static_cast<uint16_t>(std::round(padB))};
  decl.layout.childGap = static_cast<uint16_t>(
      std::round(style.childGap.value_or(0.0f) * spacingMult));
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

  bool pushedTextConstraint = false;
  if (uiState) {
    float constraintW = 0.0f;

    const bool hasWidth = style.width.has_value();
    const float rawW = hasWidth ? style.width.value() : -1.0f;
    const bool isGrow = hasWidth && rawW == 0.0f;
    const bool isFixed = hasWidth && rawW > 0.0f;

    if (isFixed) {
      constraintW = (rawW * spacingMult) - padL - padR;
    } else if (isGrow || !hasWidth) {
      if (!uiState->textConstraintWidthStack.empty()) {
        constraintW = uiState->textConstraintWidthStack.back() - padL - padR;
      }
    }

    if (constraintW > 0.5f) {
      uiState->textConstraintWidthStack.push_back(constraintW);
      pushedTextConstraint = true;
    }
  }

  bool clayHovered = Clay_Hovered();
  if (content) {
    content();
  }

  if (uiState && pushedTextConstraint) {
    uiState->textConstraintWidthStack.pop_back();
  }

  Clay__CloseElement();
  if (hasMargin) {
    Clay__CloseElement();
  }

  bool isHovered = false;
  if (clayHovered) {
    Clay_ElementData elementData = Clay_GetElementData(divId);
    if (elementData.found) {
      glm::vec2 translation =
          style.translate.value_or(glm::vec2(0.0f)) * spacingMult;
      Clay_BoundingBox hitBox = elementData.boundingBox;
      hitBox.x += translation.x;
      hitBox.y += translation.y;
      isHovered = utils::ui::isPointerOverRoundedBox(uiState->pointerPos,
                                                     hitBox, radius);
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
