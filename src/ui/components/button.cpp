#include "avk/utils/ui/layout.h"
#include "ui/animation/animation.h"
#include "ui/components.h"
#include <unordered_map>

namespace atomic {

/**
 * @brief Clean, non-intrusive Button primitive delegating layout to Div.
 */
Interaction Button(Modifier &&modifier, const std::function<void()> &content) {
  uint32_t btnId = utils::layout::getNextId("BtnAnim").id;

  static std::unordered_map<uint32_t, bool> pressMap;
  bool wasPressed = pressMap[btnId];

  Style style = modifier.getStyle();

  glm::vec4 baseBg = style.backgroundColor.value_or(DEFAULT_BACKGROUND_NORMAL);

  float targetScale = wasPressed ? 0.98f : 1.0f;
  float animatedScale =
      AnimateFloat(btnId, targetScale, 0.10f, Curves::AppleEaseOut);

  Modifier btnStyle = std::move(modifier)
                          .background(baseBg)
                          .scale(animatedScale * style.scale.value_or(1.0f))
                          .row();

  if (!style.padLeft.has_value())
    btnStyle = std::move(btnStyle).padding(16, 8);
  if (!style.borderRadius.has_value())
    btnStyle = std::move(btnStyle).rounded(6.0f);

  Interaction result = Div(std::move(btnStyle), content);

  pressMap[btnId] = result.pressed;

  return result;
}

} // namespace atomic
