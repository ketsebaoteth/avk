#include "animation/animation.h"
#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include <algorithm>

namespace atomic {

Interaction Button(Modifier &&modifier, const std::function<void()> &content) {
  const auto &style = modifier.getStyle();
  auto *uiState = utils::layout::getUiState();

  glm::vec4 bg = style.backgroundColor.value_or(DEFAULT_BACKGROUND_NORMAL);
  glm::vec4 radius = style.borderRadius.value_or(DEFAULT_BORDER_RADIUS);
  glm::vec4 strokeColor = style.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  float strokeWidth = style.strokeThickness.value_or(DEFAULT_BORDER_WIDTH);

  Clay_LayoutAlignmentX clayAlignX = CLAY_ALIGN_X_CENTER;
  if (style.alignX.has_value()) {
    switch (style.alignX.value()) {
    case AlignmentX::Left:
      clayAlignX = CLAY_ALIGN_X_LEFT;
      break;
    case AlignmentX::Right:
      clayAlignX = CLAY_ALIGN_X_RIGHT;
      break;
    default:
      clayAlignX = CLAY_ALIGN_X_CENTER;
      break;
    }
  }

  Clay_LayoutAlignmentY clayAlignY = CLAY_ALIGN_Y_CENTER;
  if (style.alignY.has_value()) {
    switch (style.alignY.value()) {
    case AlignmentY::Top:
      clayAlignY = CLAY_ALIGN_Y_TOP;
      break;
    case AlignmentY::Bottom:
      clayAlignY = CLAY_ALIGN_Y_BOTTOM;
      break;
    default:
      clayAlignY = CLAY_ALIGN_Y_CENTER;
      break;
    }
  }

  Clay_ElementId buttonId = utils::layout::getNextId("Button");

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(buttonId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool isPressedDown = isHovered && uiState->pointerDown;

  // Scale factor tracking combined with modifier transforms
  float targetScale = isPressedDown ? 0.95f : 1.0f;
  float animatedScale = AnimateFloat(buttonId.id + 0x4000, targetScale, 0.15f,
                                     Curves::AppleEaseOut);

  float finalScale = animatedScale * style.scale.value_or(1.0f);
  float finalRotation = style.rotation.value_or(0.0f);
  float finalBlur = style.blur.value_or(0.0f);

  Clay__OpenElementWithId(buttonId);

  // STATIC LAYOUT: Sizing and padding remain completely fixed.
  // This guarantees zero layout reflow, zero layout jumps, and 60/120fps
  // performance.
  Clay_Sizing sizing{};
  if (style.width.has_value()) {
    sizing.width = CLAY_SIZING_FIXED(style.width.value());
  } else {
    sizing.width = CLAY_SIZING_FIT();
  }
  sizing.height = style.height.has_value()
                      ? CLAY_SIZING_FIXED(style.height.value())
                      : CLAY_SIZING_FIXED(DEFAULT_HEIGHT);

  uint16_t padL = style.padLeft.value_or(16);
  uint16_t padR = style.padRight.value_or(16);
  uint16_t padT = style.padTop.value_or(10);
  uint16_t padB = style.padBottom.value_or(10);

  Clay_ElementDeclaration decl{};
  decl.layout = {.sizing = sizing,
                 .padding = {padL, padR, padT, padB},
                 .childGap = style.childGap.value_or(0),
                 .childAlignment = {.x = clayAlignX, .y = clayAlignY},
                 .layoutDirection = CLAY_LEFT_TO_RIGHT};

  decl.border = {.color =
                     Clay_Color{strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                                strokeColor.b * 255.0f, strokeColor.a * 255.0f},
                 .width = {.left = static_cast<uint16_t>(strokeWidth),
                           .right = static_cast<uint16_t>(strokeWidth),
                           .top = static_cast<uint16_t>(strokeWidth),
                           .bottom = static_cast<uint16_t>(strokeWidth)}};

  glm::vec4 targetBg = bg;
  if (isPressedDown) {
    targetBg = glm::vec4(bg.r * 0.8f, bg.g * 0.8f, bg.b * 0.8f, bg.a);
  } else if (isHovered) {
    targetBg =
        glm::vec4(std::min(bg.r * 1.15f, 1.0f), std::min(bg.g * 1.15f, 1.0f),
                  std::min(bg.b * 1.15f, 1.0f), bg.a);
  }

  glm::vec4 animatedBg =
      AnimateVec4(buttonId.id + 0x1000, targetBg, 0.15f, Curves::AppleEaseOut);

  decl.backgroundColor = {animatedBg.r * 255.0f, animatedBg.g * 255.0f,
                          animatedBg.b * 255.0f, animatedBg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // Attach GPU transform payload so endFrame reads scale, rotation, and blur
  // correctly
  auto *payload = new RenderPayload{.textureIndex = 0,
                                    .tintColor = glm::vec4(1.0f),
                                    .scale = finalScale,
                                    .rotation = finalRotation,
                                    .blur = finalBlur};
  decl.userData = payload;

  Clay__ConfigureOpenElement(decl);

  if (content) {
    content();
  }

  Clay__CloseElement();

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressedDown;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  return result;
}

} // namespace atomic
