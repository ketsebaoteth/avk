#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/components.h"

#include <algorithm>
#include <cmath>

namespace atomic {

/**
 * @brief Scrollable container viewport with physical spring scrolling,
 * pixel-snapped rendering, smooth dynamic scrollbars, and outer margin wrapping
 * support.
 */
void ScrollView(Modifier &&modifier, ScrollViewConfig config,
                std::function<void()> contentCallback) {
  const auto &style = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId scrollId =
      style.elementLabel.has_value()
          ? utils::layout::getNextId(style.elementLabel.value().c_str())
          : utils::layout::getNextId("ScrollView");

  Style resolvedStyle = utils::layout::resolveTransitions(scrollId.id, style);

  bool hasMargin = resolvedStyle.marginLeft.has_value() ||
                   resolvedStyle.marginRight.has_value() ||
                   resolvedStyle.marginTop.has_value() ||
                   resolvedStyle.marginBottom.has_value();

  /**
   * @brief Outer margin padding container ID.
   */
  Clay_ElementId outerId = scrollId;
  outerId.id += 0x6D417267;

  if (hasMargin) {
    Clay__OpenElementWithId(outerId);

    float ml = resolvedStyle.marginLeft.value_or(0.0f);
    float mr = resolvedStyle.marginRight.value_or(0.0f);
    float mt = resolvedStyle.marginTop.value_or(0.0f);
    float mb = resolvedStyle.marginBottom.value_or(0.0f);

    Clay_ElementDeclaration outerDecl{};
    utils::layout::applyStyleToLayout(outerDecl, resolvedStyle);

    outerDecl.layout.padding = {static_cast<uint16_t>(std::round(ml)),
                                static_cast<uint16_t>(std::round(mr)),
                                static_cast<uint16_t>(std::round(mt)),
                                static_cast<uint16_t>(std::round(mb))};

    outerDecl.backgroundColor = {0, 0, 0, 0};
    Clay__ConfigureOpenElement(outerDecl);
  }

  Clay__OpenElementWithId(scrollId);

  auto &scrollState = uiState->scrollViewStates[scrollId.id];

  glm::vec4 bg = resolvedStyle.backgroundColor.value_or(glm::vec4(0.0f));
  glm::vec4 radius = resolvedStyle.borderRadius.value_or(glm::vec4(0.0f));

  Style innerStyle = resolvedStyle;
  if (hasMargin) {
    innerStyle.width = 0;
    innerStyle.height = 0;
  }

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = innerStyle.width.has_value()
                              ? CLAY_SIZING_FIXED(innerStyle.width.value())
                              : CLAY_SIZING_GROW(),
                 .height = innerStyle.height.has_value()
                               ? CLAY_SIZING_FIXED(innerStyle.height.value())
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

  utils::layout::applyStyleToLayout(decl, innerStyle);

  auto pos = resolvedStyle.position.value_or(Position::Normal);
  utils::layout::PositioningContextGuard posGuard(scrollId.id, pos);
  utils::layout::StyleCascadeGuard styleGuard(resolvedStyle);

  decl.userData = utils::layout::createFramePayload(resolvedStyle);

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

  /**
   * @brief Process input scroll wheel events.
   */
  if (isHovered && !scrollState.isDraggingY) {
    if (config.showVerticalBar && uiState->mouseWheelDeltaY != 0.0f) {
      scrollState.targetScrollOffsetY -=
          uiState->mouseWheelDeltaY * config.scrollSpeed;
    }
    if (config.showHorizontalBar && uiState->mouseWheelDeltaX != 0.0f) {
      scrollState.targetScrollOffsetX -=
          uiState->mouseWheelDeltaX * config.scrollSpeed;
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

  /**
   * @brief Smooth scrolling dampening backed by Atomic.Motion physics.
   */
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

  /**
   * @brief Render interactive custom scrollbar thumb.
   */
  if (config.showVerticalBar && scrollData.found &&
      scrollData.contentDimensions.height >
          scrollData.scrollContainerDimensions.height) {
    glm::vec4 targetBarColor = config.scrollbarColor;
    if (scrollState.isDraggingY) {
      targetBarColor = config.scrollbarColorPressed;
    } else if (isBarHovered) {
      targetBarColor = config.scrollbarColorHover;
    }

    /**
     * @brief Smoothly animate scrollbar thumb color using MotionManager.
     */
    using motion::MotionHandle;
    glm::vec4 animatedBarColor = uiState->motionManager.animate<glm::vec4>(
        MotionHandle{scrollId.id + 0x5C524F4C}, targetBarColor, 0.15f,
        motion::AnimationCurve::EaseOut());

    std::string thumbLabel = "ScrollbarThumb_" + std::to_string(scrollId.id);

    Div(Modifier()
            .id(thumbLabel)
            .absolute()
            .parentId(scrollId.id)
            .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
            .offset(scrollData.scrollContainerDimensions.width -
                        config.scrollbarWidth - config.scrollbarMarginRight,
                    barY)
            .size(config.scrollbarWidth, barH)
            .background(animatedBarColor)
            .rounded(config.scrollbarRadius));
  }

  Clay__CloseElement();

  if (hasMargin) {
    Clay__CloseElement();
  }
}

} // namespace atomic
