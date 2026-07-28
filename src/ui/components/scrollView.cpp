#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"
#include <algorithm>

namespace atomic {

/**
 * @brief Scrollable container viewport with pixel-snapped rendering and custom
 * scrollbars.
 */
void ScrollView(Modifier &&modifier, ScrollViewConfig config,
                std::function<void()> contentCallback) {
  const auto &style = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId scrollId = utils::layout::getNextId("ScrollView");
  Clay__OpenElementWithId(scrollId);

  auto &scrollState = uiState->scrollViewStates[scrollId.id];

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
      .padding = {0, 0, 0, 0},
      .layoutDirection = CLAY_TOP_TO_BOTTOM};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  float renderScrollX = std::round(scrollState.scrollOffsetX);
  float renderScrollY = std::round(scrollState.scrollOffsetY);

  decl.clip = {.horizontal = config.showHorizontalBar,
               .vertical = config.showVerticalBar,
               .childOffset = Clay_Vector2{-renderScrollX, -renderScrollY}};

  utils::layout::applyStyleToLayout(decl, style);

  auto pos = style.position.value_or(Position::Normal);
  utils::layout::PositioningContextGuard posGuard(scrollId.id, pos);
  utils::layout::StyleCascadeGuard styleGuard(style);

  decl.userData = utils::layout::createFramePayload(style);

  Clay__ConfigureOpenElement(decl);

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(scrollId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

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

  if (isHovered && !scrollState.isDraggingY) {
    if (config.showVerticalBar && uiState->mouseWheelDeltaY != 0.0f) {
      scrollState.targetScrollOffsetY -= uiState->mouseWheelDeltaY * 35.0f;
    }
    if (config.showHorizontalBar && uiState->mouseWheelDeltaX != 0.0f) {
      scrollState.targetScrollOffsetX -= uiState->mouseWheelDeltaX * 35.0f;
    }
  }

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

    float scrollPercent =
        (maxScrollY > 0.0f) ? (scrollState.scrollOffsetY / maxScrollY) : 0.0f;
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

  if (config.showVerticalBar && scrollData.found &&
      scrollData.contentDimensions.height >
          scrollData.scrollContainerDimensions.height) {
    glm::vec4 barColor = config.scrollbarColor;
    if (scrollState.isDraggingY) {
      barColor = config.scrollbarColorPressed;
    } else if (isBarHovered) {
      barColor = config.scrollbarColorHover;
    }

    Div(DefaultModifier()
            .absolute()
            .parentId(scrollId.id)
            .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
            .offset(scrollData.scrollContainerDimensions.width -
                        config.scrollbarWidth - config.scrollbarMarginRight,
                    barY)
            .size(config.scrollbarWidth, barH)
            .background(barColor)
            .rounded(config.scrollbarRadius));
  }

  Clay__CloseElement();
}

} // namespace atomic
