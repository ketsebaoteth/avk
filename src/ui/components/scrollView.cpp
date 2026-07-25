#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include <algorithm>
#include <iostream>

namespace atomic {

void ScrollView(Modifier &&modifier, ScrollViewConfig config,
                std::function<void()> contentCallback) {
  const auto &style = modifier.getStyle();
  auto *uiState = utils::layout::getUiState();

  Clay_ElementId scrollId = utils::layout::getNextId("ScrollView");
  Clay__OpenElementWithId(scrollId);

  // 1. Access scroll state from the central UIState cache
  auto &scrollState = uiState->scrollViewStates[scrollId.id];

  // 2. Configure Outer Container Sizing & Background
  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.width.has_value()
                              ? CLAY_SIZING_FIXED(style.width.value())
                              : CLAY_SIZING_GROW(),
                 .height = style.height.has_value()
                               ? CLAY_SIZING_FIXED(style.height.value())
                               : CLAY_SIZING_GROW()},
      .padding = {style.padLeft.value_or(0), style.padRight.value_or(0),
                  style.padTop.value_or(0), style.padBottom.value_or(0)},
      .layoutDirection = CLAY_TOP_TO_BOTTOM};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // Bind negative float offsets to Clay's clip.childOffset
  decl.clip = {.horizontal = config.showHorizontalBar,
               .vertical = config.showVerticalBar,
               .childOffset = Clay_Vector2{-scrollState.scrollOffsetX,
                                           -scrollState.scrollOffsetY}};

  Clay__ConfigureOpenElement(decl);

  // 3. Evaluate mouse hover for scroll events
  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(scrollId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  // Query active scroll metrics from Clay's previous frame
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

  // Process mouse wheel scroll deltas (only if we aren't dragging the bar)
  if (isHovered && !scrollState.isDraggingY) {
    if (config.showVerticalBar && uiState->mouseWheelDeltaY != 0.0f) {
      scrollState.targetScrollOffsetY -= uiState->mouseWheelDeltaY * 35.0f;
    }
    if (config.showHorizontalBar && uiState->mouseWheelDeltaX != 0.0f) {
      scrollState.targetScrollOffsetX -= uiState->mouseWheelDeltaX * 35.0f;
    }
  }

  // 4. THE INTERACTIVE DRAG-TO-SCROLL ALGORITHM (Vertical Scrollbar)
  float barH = 0.0f;
  float barY = 0.0f;
  bool isBarHovered = false;

  if (config.showVerticalBar && scrollData.found &&
      scrollData.contentDimensions.height >
          scrollData.scrollContainerDimensions.height) {
    float containerH = scrollData.scrollContainerDimensions.height;
    float contentH = scrollData.contentDimensions.height;

    // Proportional height of the handle
    barH = containerH * (containerH / contentH);
    barH = std::max(config.scrollbarMinThumbSize,
                    barH); // Enforce custom minimum size

    // Proportional vertical position of the handle
    float scrollPercent = scrollState.scrollOffsetY / maxScrollY;
    barY = scrollPercent * (containerH - barH);

    // Define the handle's absolute bounding box on screen
    float absoluteBarX = elementData.boundingBox.x +
                         scrollData.scrollContainerDimensions.width -
                         config.scrollbarWidth - config.scrollbarMarginRight;
    float absoluteBarY = elementData.boundingBox.y + barY;

    Clay_BoundingBox barBox = {absoluteBarX, absoluteBarY,
                               config.scrollbarWidth, barH};

    // Check if cursor is hovering over the actual scrollbar handle
    isBarHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, barBox, glm::vec4(config.scrollbarRadius));

    // Initiate dragging on mouse click down (using your pointerPressed!)
    if (isBarHovered && uiState->pointerPressed) {
      scrollState.isDraggingY = true;
      scrollState.dragStartY = uiState->pointerPos.y;
      scrollState.dragStartScrollY = scrollState.scrollOffsetY;
    }

    // Release dragging on mouse button up (using your pointerDown!)
    if (scrollState.isDraggingY && !uiState->pointerDown) {
      scrollState.isDraggingY = false;
    }

    // If actively dragging, calculate exact ratio translation
    if (scrollState.isDraggingY) {
      float deltaMouseY = uiState->pointerPos.y - scrollState.dragStartY;

      // Mathematically exact ratio: scrollable pixels / track travel pixels
      float trackTravel = containerH - barH;
      float ratio = (trackTravel > 0.0f) ? (maxScrollY / trackTravel) : 0.0f;

      scrollState.targetScrollOffsetY =
          scrollState.dragStartScrollY + (deltaMouseY * ratio);
    }
  }

  // Clamp scroll targets
  scrollState.targetScrollOffsetY =
      std::clamp(scrollState.targetScrollOffsetY, 0.0f, maxScrollY);
  scrollState.targetScrollOffsetX =
      std::clamp(scrollState.targetScrollOffsetX, 0.0f, maxScrollX);

  // 5. Smooth Scrolling Interpolation (Lerp)
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

  // Final safety clamps
  scrollState.scrollOffsetY =
      std::clamp(scrollState.scrollOffsetY, 0.0f, maxScrollY);
  scrollState.scrollOffsetX =
      std::clamp(scrollState.scrollOffsetX, 0.0f, maxScrollX);

  // Execute user nested elements
  if (contentCallback) {
    contentCallback();
  }

  // 6. Draw the Interactive Scrollbar handle
  if (config.showVerticalBar && scrollData.found &&
      scrollData.contentDimensions.height >
          scrollData.scrollContainerDimensions.height) {
    Clay_ElementId scrollbarId = utils::layout::getNextId("ScrollbarThumb");
    Clay__OpenElementWithId(scrollbarId);

    Clay_ElementDeclaration scrollbarDecl{};
    scrollbarDecl.floating = {
        .offset = {scrollData.scrollContainerDimensions.width -
                       config.scrollbarWidth - config.scrollbarMarginRight,
                   barY},
        .parentId = scrollId.id,
        .zIndex = 500,
        .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
    };

    // Resolve interactive handle color (brightens on hover, glows while
    // dragged)
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

  Clay__CloseElement(); // Close ScrollView Container
}

} // namespace atomic
