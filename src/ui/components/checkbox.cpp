#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/animation/animation.h"
#include "ui/components.h"
#include "ui/generated/lucideIcons.generated.h"

namespace atomic {

/**
 * @brief Animated toggle checkbox composed on top of Div and vector Icon.
 */
Interaction Checkbox(Modifier &&modifier, bool &checked) {
  const auto &style = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId checkboxId = utils::layout::getNextId("Checkbox");

  float baseSize = style.width.value_or(20.0f);
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(4.0f));

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(checkboxId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool clicked = false;
  if (isHovered && uiState->pointerPressed) {
    checked = !checked;
    clicked = true;
  }

  bool isPressed = isHovered && uiState->pointerDown;

  float targetScale = isPressed ? 0.9f : 1.0f;
  float animatedScale = AnimateFloat(checkboxId.id + 0x4000, targetScale, 0.15f,
                                     Curves::AppleEaseOut);

  float finalScale = animatedScale * style.scale.value_or(1.0f);

  glm::vec4 inactiveBg = glm::vec4(0.06f, 0.06f, 0.06f, 1.0f);
  glm::vec4 activeBg = style.backgroundColor.value_or(Colors::orange);

  glm::vec4 targetBg = checked ? activeBg : inactiveBg;
  if (isPressed && !checked) {
    targetBg = inactiveBg * 1.15f;
  }

  glm::vec4 animatedBg = AnimateVec4(checkboxId.id + 0x1000, targetBg, 0.15f,
                                     Curves::AppleEaseOut);

  glm::vec4 inactiveBorder = glm::vec4(0.24f, 0.24f, 0.27f, 1.0f);
  glm::vec4 targetBorder = checked ? activeBg : inactiveBorder;
  glm::vec4 animatedBorder = AnimateVec4(checkboxId.id + 0x2000, targetBorder,
                                         0.15f, Curves::AppleEaseOut);

  Modifier boxStyle = std::move(modifier)
                          .background(animatedBg)
                          .border(animatedBorder, 1.0f)
                          .size(baseSize, baseSize)
                          .rounded(radius.x)
                          .scale(finalScale)
                          .center();

  Interaction result = Div(std::move(boxStyle), [&]() {
    float targetAlpha = checked ? 1.0f : 0.0f;
    float animatedAlpha = AnimateFloat(checkboxId.id + 0x3000, targetAlpha,
                                       0.15f, Curves::AppleEaseOut);

    if (animatedAlpha > 0.01f) {
      glm::vec4 checkColor = Colors::white;
      checkColor.a = animatedAlpha;

      Icon(LucideIcon::Check,
           DefaultModifier().color(checkColor).size(15.0f, 15.0f));
    }
  });

  result.clicked = clicked;
  result.pressed = checked;

  return result;
}

} // namespace atomic
