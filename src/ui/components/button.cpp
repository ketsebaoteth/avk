#include "avk/utils/ui/layout.h"
#include "ui/animation/animation.h"
#include "ui/components.h"
#include "ui/utils/color.h"
#include <algorithm>

namespace atomic {

// In src/ui/components/button.cpp:

Interaction Button(Modifier &&modifier, const std::function<void()> &content) {
  auto *uiState = getUiState();
  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};

  const auto &rawStyle = modifier.getStyle();

  std::string labelId = rawStyle.elementLabel.value_or("BtnAnim");
  uint32_t btnId = hashLabel(labelId);

  bool isDisabled = rawStyle.disabled.value_or(false) || inherited.disabled;
  bool wasHovered = !isDisabled && isHovered(btnId);
  bool wasPressed = !isDisabled && isPressed(btnId);

  // shadcn default colors: Dark Zinc (#18181b) -> Hover (#27272a) -> Active
  // (#09090b)
  glm::vec4 baseBg = rawStyle.backgroundColor.value_or("#ffffff"_hex);
  glm::vec4 hoverBg = glm::vec4(std::min(baseBg.r * 0.93f, 1.0f),
                                std::min(baseBg.g * 0.93f, 1.0f),
                                std::min(baseBg.b * 0.93f, 1.0f), baseBg.a);
  glm::vec4 activeBg =
      glm::vec4(baseBg.r * 0.86f, baseBg.g * 0.86f, baseBg.b * 0.86f, baseBg.a);

  glm::vec4 targetBg = wasPressed ? activeBg : (wasHovered ? hoverBg : baseBg);

  Modifier btnStyle = std::move(modifier)
                          .id(labelId)
                          .background(targetBg)
                          .scale(rawStyle.scale.value_or(1.0f))
                          .disabled(isDisabled)
                          .row()
                          .center();

  if (!rawStyle.fontSize.has_value())
    btnStyle = std::move(btnStyle).fontSize(14.0f);
  if (!rawStyle.fontWeight.has_value())
    btnStyle = std::move(btnStyle).fontWeight(500.0f);
  if (!rawStyle.padLeft.has_value())
    btnStyle = std::move(btnStyle).padding(16, 8);
  if (!rawStyle.strokeColor.has_value())
    btnStyle = std::move(btnStyle).border(Colors::transparent, 1);
  if (!rawStyle.strokeThickness.has_value())
    btnStyle = std::move(btnStyle).border(Colors::gray[100], 1);
  if (!rawStyle.borderRadius.has_value())
    btnStyle = std::move(btnStyle).rounded(10.0f);
  if (!rawStyle.textColor.has_value())
    btnStyle = std::move(btnStyle).color(Colors::black[900]);

  if (!rawStyle.transitionSpec.has_value()) {
    btnStyle = std::move(btnStyle).transition(0.5f, Curves::AppleEaseOut);
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
