#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/animation/animation.h" // Access AnimateFloat and AnimateVec4
#include "ui/components.h"

namespace atomic {

/**
 * @brief Animated sliding toggle switch component built on top of Div.
 */
Interaction Switch(Modifier &&modifier, bool &checked) {
  const auto &style = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId switchId = utils::layout::getNextId("Switch");

  float width = style.width.value_or(48.0f);
  float height = style.height.value_or(26.0f);
  float pad = 3.0f;

  float pillRadius = height * 0.5f;
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(pillRadius));

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(switchId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool clicked = false;
  if (isHovered && uiState->pointerPressed) {
    checked = !checked;
    clicked = true;
  }

  glm::vec4 inactiveColor = glm::vec4(0.15f, 0.15f, 0.16f, 1.0f);
  glm::vec4 activeColor = style.backgroundColor.value_or(Colors::orange);
  glm::vec4 targetColor = checked ? activeColor : inactiveColor;

  glm::vec4 animatedColor = AnimateVec4(switchId.id + 0x1000, targetColor,
                                        0.18f, Curves::AppleEaseOut);

  float thumbSize = height - (pad * 2.0f);
  float minX = pad;
  float maxX = width - pad - thumbSize;
  float targetX = checked ? maxX : minX;

  float animatedX =
      AnimateFloat(switchId.id + 0x2000, targetX, 0.18f, Curves::AppleEaseOut);

  Modifier switchStyle = std::move(modifier)
                             .background(animatedColor)
                             .size(width, height)
                             .rounded(pillRadius)
                             .relative();

  Interaction result = Div(std::move(switchStyle), [&]() {
    Div(DefaultModifier()
            .absolute()
            .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
            .offset(animatedX, pad)
            .size(thumbSize, thumbSize)
            .background(Colors::white)
            .rounded(thumbSize * 0.5f));
  });

  result.clicked = clicked;
  result.pressed = checked;

  return result;
}

} // namespace atomic
