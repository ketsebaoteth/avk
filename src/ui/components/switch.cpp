#include "animation/animation.h" // Access AnimateFloat and AnimateVec4
#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {

Interaction Switch(Modifier &&modifier, bool &checked) {
  const auto &style = modifier.getStyle();
  auto *uiState = utils::layout::getUiState();

  Clay_ElementId switchId = utils::layout::getNextId("Switch");
  Clay__OpenElementWithId(switchId);

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
                                        0.18f, AnimationCurve::EaseOut());

  Clay_ElementDeclaration decl{};
  decl.layout = {.sizing = {.width = CLAY_SIZING_FIXED(width),
                            .height = CLAY_SIZING_FIXED(height)}};

  decl.backgroundColor = {animatedColor.r * 255.0f, animatedColor.g * 255.0f,
                          animatedColor.b * 255.0f, animatedColor.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // 1-LINE SYSTEM HOOK: Overwrites layout metrics and positioning
  utils::layout::applyStyleToLayout(decl, style);

  // Safe frame allocation for transitions
  decl.userData = utils::layout::createFramePayload(style);

  Clay__ConfigureOpenElement(decl);

  // -----------------------------------------------------------------
  // 3. THE SLIDING THUMB ANIMATION (Floating Layout)
  // -----------------------------------------------------------------
  float thumbSize = height - (pad * 2.0f);
  float minX = pad;
  float maxX = width - pad - thumbSize;
  float targetX = checked ? maxX : minX;

  float animatedX = AnimateFloat(switchId.id + 0x2000, targetX, 0.18f,
                                 AnimationCurve::EaseOut());

  Clay_ElementId thumbId =
      Clay__HashString(Clay_String{false, 11, "SwitchThumb"}, switchId.id);
  Clay__OpenElementWithId(thumbId);

  Clay_ElementDeclaration thumbDecl{};
  // Direct value assignment to avoid C++ aggregate initialization warnings
  thumbDecl.floating.offset = {animatedX, pad};
  thumbDecl.floating.parentId = switchId.id;
  thumbDecl.floating.zIndex = 500;
  thumbDecl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;

  thumbDecl.backgroundColor = {255.0f, 255.0f, 255.0f, 255.0f};
  thumbDecl.cornerRadius = {thumbSize * 0.5f, thumbSize * 0.5f,
                            thumbSize * 0.5f, thumbSize * 0.5f};
  thumbDecl.layout = {.sizing = {.width = CLAY_SIZING_FIXED(thumbSize),
                                 .height = CLAY_SIZING_FIXED(thumbSize)}};

  Clay__ConfigureOpenElement(thumbDecl);
  Clay__CloseElement(); // Close SwitchThumb

  Clay__CloseElement(); // Close Switch Container

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = checked;
  result.clicked = clicked;

  return result;
}

} // namespace atomic
