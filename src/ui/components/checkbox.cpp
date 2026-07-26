#include "animation/animation.h" // Access AnimateFloat and AnimateVec4
#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include "ui/lucide-icons.generated.h"

namespace atomic {

Interaction Checkbox(Modifier &&modifier, bool &checked) {
  const auto &style = modifier.getStyle();
  auto *uiState = utils::layout::getUiState();

  // 1. Generate stable ID for the Checkbox container
  Clay_ElementId checkboxId = utils::layout::getNextId("Checkbox");
  Clay__OpenElementWithId(checkboxId);

  // Standard sleek dimensions (overwritten if specified in modifier)
  float baseSize = style.width.value_or(20.0f);
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(4.0f));

  // Evaluate CPU-side rounded box hover
  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(checkboxId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  // Toggle checked state instantly on mouse click down
  bool clicked = false;
  if (isHovered && uiState->pointerPressed) {
    checked = !checked;
    clicked = true;
  }

  bool isPressed = isHovered && uiState->pointerDown;

  // -----------------------------------------------------------------
  // 2. THE GPU BUMP ANIMATION (RenderPayload scale)
  // -----------------------------------------------------------------
  // Scale down the box to 90% when pressed
  float targetScale = isPressed ? 0.9f : 1.0f;
  float animatedScale = AnimateFloat(checkboxId.id + 0x4000, targetScale, 0.15f,
                                     AnimationCurve::EaseOut());

  float finalScale = animatedScale * style.scale.value_or(1.0f);
  float finalRotation = style.rotation.value_or(0.0f);

  // 3. Animate Background Color
  glm::vec4 inactiveBg = glm::vec4(0.06f, 0.06f, 0.06f, 1.0f); // #0F0F10
  glm::vec4 activeBg = style.backgroundColor.value_or(Colors::orange);

  glm::vec4 targetBg = checked ? activeBg : inactiveBg;
  if (isPressed && !checked) {
    targetBg = inactiveBg * 1.15f;
  }

  glm::vec4 animatedBg = AnimateVec4(checkboxId.id + 0x1000, targetBg, 0.15f,
                                     AnimationCurve::EaseOut());

  // 4. Animate Border Color
  glm::vec4 inactiveBorder = glm::vec4(0.24f, 0.24f, 0.27f, 1.0f); // #3F3F46
  glm::vec4 activeBorder = activeBg;

  glm::vec4 targetBorder = checked ? activeBorder : inactiveBorder;
  glm::vec4 animatedBorder = AnimateVec4(checkboxId.id + 0x2000, targetBorder,
                                         0.15f, AnimationCurve::EaseOut());

  Clay_ElementDeclaration decl{};
  decl.layout = {
      // STATIC LAYOUT: Sizing remains completely fixed! Zero CPU layout reflow.
      .sizing = {.width = CLAY_SIZING_FIXED(baseSize),
                 .height = CLAY_SIZING_FIXED(baseSize)},
      .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}};

  decl.backgroundColor = {animatedBg.r * 255.0f, animatedBg.g * 255.0f,
                          animatedBg.b * 255.0f, animatedBg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // Standard 1px border
  decl.border = {.color = {animatedBorder.r * 255.0f, animatedBorder.g * 255.0f,
                           animatedBorder.b * 255.0f,
                           animatedBorder.a * 255.0f},
                 .width = {1, 1, 1, 1, 0}};

  // 1-LINE SYSTEM HOOK: Auto-maps custom sizing, absolute positions, and Clay
  // floating configs!
  utils::layout::applyStyleToLayout(decl, style);

  // Safe frame allocation - zero memory leak!
  decl.userData =
      utils::layout::createFramePayload(style, finalScale, finalRotation);

  Clay__ConfigureOpenElement(decl);

  // -----------------------------------------------------------------
  // 5. THE CHECKMARK FADE ANIMATION
  // -----------------------------------------------------------------
  float targetAlpha = checked ? 1.0f : 0.0f;
  float animatedAlpha = AnimateFloat(checkboxId.id + 0x3000, targetAlpha, 0.15f,
                                     AnimationCurve::EaseOut());

  if (animatedAlpha > 0.01f) {
    // Icon layout size remains entirely static!
    float iconSize = 15.0f;

    glm::vec4 checkColor = Colors::white;
    checkColor.a = animatedAlpha;

    Icon(LucideIcon::Check,
         DefaultModifier().color(checkColor).size(iconSize, iconSize));
  }

  Clay__CloseElement();

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = checked;
  result.clicked = clicked;

  return result;
}

} // namespace atomic
