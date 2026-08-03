#include "ui/components.h"
#include "ui/style/themeManager.h"
#include "ui/utils/color.h"

#include <algorithm>

namespace atomic {

Interaction Button(Modifier &&modifier, const std::function<void()> &content) {
  auto *uiState = getUiState();
  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};

  const auto &rawStyle = modifier.getStyle();
  auto &tm = ThemeManager::getInstance();

  std::string labelId = rawStyle.elementLabel.value_or("BtnAnim");
  uint32_t btnId = hashLabel(labelId);

  bool isDisabled = rawStyle.disabled.value_or(false) || inherited.disabled;
  bool wasHovered = !isDisabled && isHovered(btnId);
  bool wasPressed = !isDisabled && isPressed(btnId);

  // --------------------------------------------------------------------------
  // ⚡ ENUM O(1) VECTOR LOOKUPS
  // --------------------------------------------------------------------------
  float scaleMult = tm.getVariable<float>(ThemeVarId::ScaleMultiplier, 1.0f);
  float spacingMult =
      tm.getVariable<float>(ThemeVarId::SpacingMultiplier, 1.0f);
  float fontSizeMult =
      tm.getVariable<float>(ThemeVarId::FontSizeMultiplier, 1.0f);
  float borderRadiusMult =
      tm.getVariable<float>(ThemeVarId::BorderRadiusMultiplier, 1.0f);
  float borderWidthMult =
      tm.getVariable<float>(ThemeVarId::BorderWidthMultiplier, 1.0f);

  glm::vec4 themeBaseBg =
      tm.getVariable<glm::vec4>(ThemeVarId::ColorBgSurface, "#ffffff"_hex);
  glm::vec4 baseBg = rawStyle.backgroundColor.value_or(themeBaseBg);

  glm::vec4 defaultHoverBg = glm::vec4(
      std::min(baseBg.r * 0.93f, 1.0f), std::min(baseBg.g * 0.93f, 1.0f),
      std::min(baseBg.b * 0.93f, 1.0f), baseBg.a);
  glm::vec4 hoverBg = tm.getVariable<glm::vec4>(ThemeVarId::ColorBgSurfaceHover,
                                                defaultHoverBg);

  glm::vec4 defaultActiveBg =
      glm::vec4(baseBg.r * 0.86f, baseBg.g * 0.86f, baseBg.b * 0.86f, baseBg.a);
  glm::vec4 activeBg = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorBgSurfaceActive, defaultActiveBg);

  glm::vec4 targetBg = wasPressed ? activeBg : (wasHovered ? hoverBg : baseBg);

  float themeFontSize =
      tm.getVariable<float>(ThemeVarId::ButtonFontSize, 14.0f);
  float finalFontSize =
      rawStyle.fontSize.value_or(themeFontSize) * fontSizeMult;

  float themeFontWeight =
      tm.getVariable<float>(ThemeVarId::ButtonFontWeight, 500.0f);
  float finalFontWeight = rawStyle.fontWeight.value_or(themeFontWeight);

  float themePadX = tm.getVariable<float>(ThemeVarId::ButtonPadX, 16.0f);
  float themePadY = tm.getVariable<float>(ThemeVarId::ButtonPadY, 14.0f);
  float finalPadX =
      (rawStyle.padLeft.has_value() ? rawStyle.padLeft.value() : themePadX) *
      spacingMult;
  float finalPadY =
      (rawStyle.padTop.has_value() ? rawStyle.padTop.value() : themePadY) *
      spacingMult;

  glm::vec4 themeBorderColor = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorBorderNormal, Colors::transparent);
  glm::vec4 strokeColor = rawStyle.strokeColor.value_or(themeBorderColor);

  glm::vec4 themeBorderWidth =
      glm::vec4(tm.getVariable<float>(ThemeVarId::BorderWidthThin, 1.0f));
  glm::vec4 strokeThickness =
      (rawStyle.strokeThickness.value_or(themeBorderWidth)) * borderWidthMult;

  float themeRadius = tm.getVariable<float>(ThemeVarId::BorderRadiusLg, 10.0f);
  glm::vec4 borderRadius =
      (rawStyle.borderRadius.value_or(glm::vec4(themeRadius))) *
      borderRadiusMult;

  glm::vec4 themeTextColor = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorTextPrimary, Colors::black[900]);
  glm::vec4 textColor = rawStyle.textColor.value_or(themeTextColor);

  float baseScale = rawStyle.scale.value_or(1.0f) * scaleMult;

  Modifier btnStyle = std::move(modifier)
                          .id(labelId)
                          .background(targetBg)
                          .scale(baseScale)
                          .fontSize(finalFontSize)
                          .fontWeight(finalFontWeight)
                          .padding(finalPadX, finalPadY)
                          .border(strokeColor, strokeThickness)
                          .rounded(borderRadius)
                          .color(textColor)
                          .disabled(isDisabled)
                          .row()
                          .center();

  if (!rawStyle.transitionSpec.has_value()) {
    float transDuration =
        tm.getVariable<float>(ThemeVarId::TransitionDurationNormal, 0.15f);
    btnStyle = std::move(btnStyle).transition(
        transDuration, motion::AnimationCurve::EaseOut());
  }

  Interaction result = Div(std::move(btnStyle), content);

  if (isDisabled) {
    result.hovered = false;
    result.pressed = false;
    result.clicked = false;
  }

  return result;
}

} // namespace atomic
