#include "avk/atomic_ui.h"
#include "avk/utils/ui/layout.h"
#include "ui/color.h"
#include "ui/components.h"

namespace atomic {

void Column(Modifier &&modifier, const std::function<void()> &content) {
  const auto &style = modifier.getStyle();

  Clay__OpenElementWithId(utils::layout::getNextId("Column"));

  // 1. Resolve safe optionals
  glm::vec4 bg =
      style.backgroundColor.value_or(glm::vec4(0.0f)); // Default transparent
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));

  glm::vec4 strokeColor = style.strokeColor.value_or("#ffffff1a"_hex);
  float strokeWidth = style.strokeThickness.value_or(1.0f);

  Clay_LayoutAlignmentX clayAlignX = CLAY_ALIGN_X_LEFT; // Default Left
  if (style.alignX.has_value()) {
    switch (style.alignX.value()) {
    case AlignmentX::Center:
      clayAlignX = CLAY_ALIGN_X_CENTER;
      break;
    case AlignmentX::Right:
      clayAlignX = CLAY_ALIGN_X_RIGHT;
      break;
    default:
      clayAlignX = CLAY_ALIGN_X_LEFT;
      break;
    }
  }

  Clay_LayoutAlignmentY clayAlignY = CLAY_ALIGN_Y_TOP; // Default Top
  if (style.alignY.has_value()) {
    switch (style.alignY.value()) {
    case AlignmentY::Center:
      clayAlignY = CLAY_ALIGN_Y_CENTER;
      break;
    case AlignmentY::Bottom:
      clayAlignY = CLAY_ALIGN_Y_BOTTOM;
      break;
    default:
      clayAlignY = CLAY_ALIGN_Y_TOP;
      break;
    }
  }

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_GROW(), // Default GROW
                 .height = style.height.has_value()
                               ? CLAY_SIZING_FIXED(style.height.value())
                               : CLAY_SIZING_GROW()},
      .padding = {style.padLeft.value_or(0), style.padRight.value_or(0),
                  style.padTop.value_or(0), style.padBottom.value_or(0)},
      .childGap = style.childGap.value_or(0),
      .childAlignment = {.x = clayAlignX, .y = clayAlignY},
      .layoutDirection = CLAY_TOP_TO_BOTTOM};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  if (strokeWidth > 0.0f) {
    uint16_t w = static_cast<uint16_t>(strokeWidth);
    decl.border = {.color = {strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                             strokeColor.b * 255.0f, strokeColor.a * 255.0f},
                   .width = {w, w, w, w}};
  }

  Clay__ConfigureOpenElement(decl);

  content();

  Clay__CloseElement();
}

} // namespace atomic
