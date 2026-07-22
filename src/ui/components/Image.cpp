#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

namespace atomic {

void Image(Modifier &&modifier, uint32_t textureIndex, const glm::vec4 &tint) {
  const auto &style = modifier.getStyle();

  Clay__OpenElementWithId(utils::layout::getNextId("Image"));

  // 1. Allocate our layout tracking payload block on the heap
  auto *payload = new ImagePayload{textureIndex, tint};

  Clay_ElementDeclaration decl{};

  // Standard dimension configurations
  decl.layout = {
      .sizing = {.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width)
                                         : CLAY_SIZING_GROW(),
                 .height = style.hasHeight ? CLAY_SIZING_FIXED(style.height)
                                           : CLAY_SIZING_GROW()}};

  decl.backgroundColor = {
      style.backgroundColor.r * 255.0f, style.backgroundColor.g * 255.0f,
      style.backgroundColor.b * 255.0f, style.backgroundColor.a * 255.0f};

  decl.cornerRadius = {style.borderRadius.x, style.borderRadius.y,
                       style.borderRadius.z, style.borderRadius.w};

  decl.userData = payload;

  decl.image = {.imageData = payload};

  Clay__ConfigureOpenElement(decl);
  Clay__CloseElement();
}

} // namespace atomic
