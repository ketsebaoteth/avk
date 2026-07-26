#include "avk/atomic_ui.h"
#include "avk/utils/ui/layout.h"
#include "ui/components.h"

namespace atomic {

void Row(Modifier &&modifier, const std::function<void()> &content) {
  const auto &style = modifier.getStyle();

  Clay_ElementId rowId = utils::layout::getNextId("Row");
  Clay__OpenElementWithId(rowId);

  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  glm::vec4 radius = style.borderRadius.value_or(DEFAULT_BORDER_RADIUS);
  glm::vec4 strokeColor = style.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  float strokeWidth = style.strokeThickness.value_or(0.0f);

  Clay_LayoutAlignmentX clayAlignX =
      style.alignX.has_value() && style.alignX.value() == AlignmentX::Center
          ? CLAY_ALIGN_X_CENTER
      : style.alignX.has_value() && style.alignX.value() == AlignmentX::Right
          ? CLAY_ALIGN_X_RIGHT
          : CLAY_ALIGN_X_LEFT;
  Clay_LayoutAlignmentY clayAlignY =
      style.alignY.has_value() && style.alignY.value() == AlignmentY::Center
          ? CLAY_ALIGN_Y_CENTER
      : style.alignY.has_value() && style.alignY.value() == AlignmentY::Bottom
          ? CLAY_ALIGN_Y_BOTTOM
          : CLAY_ALIGN_Y_TOP;

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
      .padding = {0, 0, 0, 0},
      .childGap = 0,
      .childAlignment = {.x = clayAlignX, .y = clayAlignY},
      .layoutDirection = CLAY_LEFT_TO_RIGHT};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  if (strokeWidth > 0.0f) {
    uint16_t w = static_cast<uint16_t>(strokeWidth);
    decl.border = {.color = {strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                             strokeColor.b * 255.0f, strokeColor.a * 255.0f},
                   .width = {w, w, w, w}};
  }

  // 1-LINE SYSTEM HOOK: Apply styling and absolute/fixed layouts
  utils::layout::applyStyleToLayout(decl, style);

  // CONTEXT GUARD: Establishes a relative/absolute positioning context for all
  // nested children
  auto pos = style.position.value_or(Position::Normal);
  utils::layout::PositioningContextGuard guard(rowId.id, pos);

  decl.userData = utils::layout::createFramePayload(style);

  Clay__ConfigureOpenElement(decl);

  if (content) {
    content();
  }

  Clay__CloseElement();
}

} // namespace atomic
