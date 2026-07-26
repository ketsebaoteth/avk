#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include <algorithm>

namespace atomic {

void ScrollView(Modifier &&modifier, ScrollViewConfig config,
                std::function<void()> contentCallback) {
  const auto &style = modifier.getStyle();
  auto *uiState = utils::layout::getUiState();

  Clay_ElementId scrollId = utils::layout::getNextId("ScrollView");
  Clay__OpenElementWithId(scrollId);

  auto &scrollState = uiState->scrollViewStates[scrollId.id];

  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
      .padding = {0, 0, 0, 0},
      .layoutDirection = CLAY_TOP_TO_BOTTOM};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  decl.clip = {.horizontal = config.showHorizontalBar,
               .vertical = config.showVerticalBar,
               .childOffset = Clay_Vector2{-scrollState.scrollOffsetX,
                                           -scrollState.scrollOffsetY}};

  // 1-LINE SYSTEM HOOK: Map styling, size overrides, padding, and layout
  // offsets
  utils::layout::applyStyleToLayout(decl, style);

  // CONTEXT GUARD: Establishes a relative/absolute positioning context for
  // nested scrolling children
  auto pos = style.position.value_or(Position::Normal);
  utils::layout::PositioningContextGuard guard(scrollId.id, pos);

  decl.userData = utils::layout::createFramePayload(style);

  Clay__ConfigureOpenElement(decl);

  // Evaluate mouse hover
  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(scrollId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  // Scroll Container logic
  Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(scrollId);
  float maxScrollY = 0.0f;
  float maxScrollX = 0.0f;

  if (scrollData.found) {
    maxScrollY =
        std::max(0.0f, scrollData.contentDimensions.height -
                           scrollData.scrollContainerDimensions.height);
    maxScrollX = std::max(0.0f, scrollData.contentDimensions.width -
                                    scrollData.scrollContainerDimensions.width);
  }

  // Scroll wheel events
  if (isHovered && !scrollState.isDraggingY) {
    if (config.showVerticalBar && uiState->mouseWheelDeltaY != 0.0f) {
      scrollState.targetScrollOffsetY -= uiState->mouseWheelDeltaY * 35.0f;
    }
    if (config.showHorizontalBar && uiState->mouseWheelDeltaX != 0.0f) {
      scrollState.targetScrollOffsetX -= uiState->mouseWheelDeltaX * 35.0f;
    }
  }

  // Vertical Scrollbar logic
  float barH = 0.0f;
  float barY = 0.0f;
  bool isBarHovered = false;

  if (config.showVerticalBar && scrollData.found &&
      scrollData.contentDimensions.height >
          scrollData.scrollContainerDimensions.height) {
    float containerH = scrollData.scrollContainerDimensions.height;
    float contentH = scrollData.contentDimensions.height;

    barH = containerH * (containerH / contentH);
    barH = std::max(config.scrollbarMinThumbSize, barH);

    float scrollPercent = scrollState.scrollOffsetY / maxScrollY;
    barY = scrollPercent * (containerH - barH);

    float absoluteBarX = elementData.boundingBox.x +
                         scrollData.scrollContainerDimensions.width -
                         config.scrollbarWidth - config.scrollbarMarginRight;
    float absoluteBarY = elementData.boundingBox.y + barY;

    Clay_BoundingBox barBox = {absoluteBarX, absoluteBarY,
                               config.scrollbarWidth, barH};

    isBarHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, barBox, glm::vec4(config.scrollbarRadius));

    if (isBarHovered && uiState->pointerPressed) {
      scrollState.isDraggingY = true;
      scrollState.dragStartY = uiState->pointerPos.y;
      scrollState.dragStartScrollY = scrollState.scrollOffsetY;
    }

    if (scrollState.isDraggingY && !uiState->pointerDown) {
      scrollState.isDraggingY = false;
    }

    if (scrollState.isDraggingY) {
      float deltaMouseY = uiState->pointerPos.y - scrollState.dragStartY;
      float trackTravel = containerH - barH;
      float ratio = (trackTravel > 0.0f) ? (maxScrollY / trackTravel) : 0.0f;
      scrollState.targetScrollOffsetY =
          scrollState.dragStartScrollY + (deltaMouseY * ratio);
    }
  }

  scrollState.targetScrollOffsetY =
      std::clamp(scrollState.targetScrollOffsetY, 0.0f, maxScrollY);
  scrollState.targetScrollOffsetX =
      std::clamp(scrollState.targetScrollOffsetX, 0.0f, maxScrollX);

  if (config.smoothScrolling) {
    scrollState.scrollOffsetY +=
        (scrollState.targetScrollOffsetY - scrollState.scrollOffsetY) *
        config.smoothFactor;
    scrollState.scrollOffsetX +=
        (scrollState.targetScrollOffsetX - scrollState.scrollOffsetX) *
        config.smoothFactor;
  } else {
    scrollState.scrollOffsetY = scrollState.targetScrollOffsetY;
    scrollState.scrollOffsetX = scrollState.targetScrollOffsetX;
  }

  scrollState.scrollOffsetY =
      std::clamp(scrollState.scrollOffsetY, 0.0f, maxScrollY);
  scrollState.scrollOffsetX =
      std::clamp(scrollState.scrollOffsetX, 0.0f, maxScrollX);

  if (contentCallback) {
    contentCallback();
  }

  // Draw Vertical Scrollbar thumb
  if (config.showVerticalBar && scrollData.found &&
      scrollData.contentDimensions.height >
          scrollData.scrollContainerDimensions.height) {
    Clay_ElementId scrollbarId = utils::layout::getNextId("ScrollbarThumb");
    Clay__OpenElementWithId(scrollbarId);

    Clay_ElementDeclaration scrollbarDecl{};
    // Value member configurations directly assigned to avoid compile warnings!
    scrollbarDecl.floating.offset = {
        scrollData.scrollContainerDimensions.width - config.scrollbarWidth -
            config.scrollbarMarginRight,
        barY};
    scrollbarDecl.floating.parentId = scrollId.id;
    scrollbarDecl.floating.zIndex = 500;
    scrollbarDecl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;

    glm::vec4 barColor = config.scrollbarColor;
    if (scrollState.isDraggingY) {
      barColor = config.scrollbarColorPressed;
    } else if (isBarHovered) {
      barColor = config.scrollbarColorHover;
    }

    scrollbarDecl.backgroundColor = {barColor.r * 255.0f, barColor.g * 255.0f,
                                     barColor.b * 255.0f, barColor.a * 255.0f};
    scrollbarDecl.cornerRadius = {
        config.scrollbarRadius, config.scrollbarRadius, config.scrollbarRadius,
        config.scrollbarRadius};
    scrollbarDecl.layout = {
        .sizing = {.width = CLAY_SIZING_FIXED(config.scrollbarWidth),
                   .height = CLAY_SIZING_FIXED(barH)}};

    Clay__ConfigureOpenElement(scrollbarDecl);
    Clay__CloseElement();
  }

  Clay__CloseElement();
}

} // namespace atomic
