#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/color.h"
#include "ui/components.h"

namespace atomic {

Interaction Button(Modifier &&modifier, const std::function<void()> &content) {
  const auto &style = modifier.getStyle();

  glm::vec4 bg = style.backgroundColor.value_or("#212121"_hex);
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(6.0f));
  glm::vec4 strokeColor = style.strokeColor.value_or("#ffffff1a"_hex);
  float strokeWidth = style.strokeThickness.value_or(1.0f);

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
  Clay__OpenElementWithId(buttonId);

  Clay_Sizing sizing{};
  sizing.width = style.width.has_value()
                     ? CLAY_SIZING_FIXED(style.width.value())
                     : CLAY_SIZING_FIT();

  sizing.height =
      style.height.has_value()
          ? CLAY_SIZING_FIXED(style.height.value())
          : CLAY_SIZING_FIXED(DEFAULT_HEIGHT); // Default button height

  Clay_ElementDeclaration decl{};
  decl.layout = {.sizing = sizing,
                 .padding = {style.padLeft.value_or(16), // Default margins
                             style.padRight.value_or(16),
                             style.padTop.value_or(10),
                             style.padBottom.value_or(10)},
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

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(buttonId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        utils::layout::getUiState()->pointerPos, elementData.boundingBox,
        radius);
  }

  bool isPressed = isHovered && utils::layout::getUiState()->pointerPressed;

  // Apply color highlights
  Clay_Color color = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                      bg.a * 255.0f};
  if (isPressed) {
    color.r *= 0.8f;
    color.g *= 0.8f;
    color.b *= 0.8f;
  } else if (isHovered) {
    color.r = std::min(color.r * 1.15f, 255.0f);
    color.g = std::min(color.g * 1.15f, 255.0f);
    color.b = std::min(color.b * 1.15f, 255.0f);
  }

  decl.backgroundColor = color;
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  Clay__ConfigureOpenElement(decl);

  if (content) {
    content();
  }

  Clay__CloseElement();

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressed;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  return result;
} // namespace atomic

} // namespace atomic
