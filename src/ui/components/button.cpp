#include "avk/utils/ui/layout.h"
#include "ui/animation/animation.h"
#include "ui/components.h"
#include <unordered_map>

namespace atomic {

/**
 * @brief Composable Button component fully integrated with Style Cascading.
 */
Interaction Button(Modifier &&modifier, const std::function<void()> &content) {
  auto *uiState = getUiState();

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};

  uint32_t btnId = utils::layout::getNextId("Btn").id;

  static std::unordered_map<uint32_t, bool> pressMap;

  Style style = modifier.getStyle();

  bool isDisabled = style.disabled.value_or(false) || inherited.disabled;
  bool wasPressed = !isDisabled && pressMap[btnId];

  glm::vec4 baseBg = style.backgroundColor.value_or(DEFAULT_BACKGROUND_NORMAL);

  float targetScale = wasPressed ? 0.98f : 1.0f;
  float animatedScale =
      AnimateFloat(btnId, targetScale, 0.10f, Curves::AppleEaseOut);

  Modifier btnStyle = std::move(modifier)
                          .background(baseBg)
                          .scale(animatedScale * style.scale.value_or(1.0f))
                          .disabled(isDisabled) // Propagate disabled state down
                          .row();

  if (!style.padLeft.has_value())
    btnStyle = std::move(btnStyle).padding(16, 8);
  if (!style.borderRadius.has_value())
    btnStyle = std::move(btnStyle).rounded(6.0f);

  Interaction result = Div(std::move(btnStyle), content);

  if (isDisabled) {
    result.hovered = false;
    result.pressed = false;
    result.clicked = false;
  }

  pressMap[btnId] = result.pressed;

  return result;
}

} // namespace atomic
