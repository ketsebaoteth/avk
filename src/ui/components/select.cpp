#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "glm/ext/vector_float4.hpp"
#include "ui/components.h"
#include "ui/core/resources.h"
#include "ui/utils/color.h"

#include <algorithm>
#include <string>

namespace atomic {

/**
 * @brief Interactive select dropdown menu with animated placement, hover
 * states, and style cascading.
 */
Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options,
                   DropdownPlacement placement) {
  Interaction result{};
  if (options.empty())
    return result;
  if (selectedIndex >= options.size())
    selectedIndex = 0;

  static const uint32_t chevronTex = loadTexture("icons/chevron-down.png");

  const auto &style = modifier.getStyle();
  auto *uiState = getUiState();
  bool initialIsOpen = isOpen;

  Clay_ElementId selectHeaderId =
      style.elementLabel.has_value()
          ? utils::layout::getNextId(style.elementLabel.value().c_str())
          : utils::layout::getNextId("SelectHeader");

  bool isHovered = false;
  Clay_ElementData headerData = Clay_GetElementData(selectHeaderId);
  glm::vec4 radius = style.borderRadius.value_or(DEFAULT_BORDER_RADIUS);

  if (headerData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, headerData.boundingBox, radius);
  }

  bool headerClicked = isHovered && uiState->pointerPressed;
  if (headerClicked) {
    isOpen = !initialIsOpen;
    result.clicked = true;
  }

  bool isPressed = isHovered && uiState->pointerDown;

  glm::vec4 baseBg = style.backgroundColor.value_or(DEFAULT_BACKGROUND_NORMAL);
  glm::vec4 strokeColor = style.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  glm::vec4 strokeWidth =
      style.strokeThickness.value_or(glm::vec4(DEFAULT_BORDER_WIDTH));

  glm::vec4 targetHeaderBg = baseBg;
  if (isPressed) {
    targetHeaderBg = glm::vec4(baseBg.r * 0.85f, baseBg.g * 0.85f,
                               baseBg.b * 0.85f, baseBg.a);
  } else if (isHovered || isOpen) {
    targetHeaderBg = glm::vec4(std::min(baseBg.r * 1.12f, 1.0f),
                               std::min(baseBg.g * 1.12f, 1.0f),
                               std::min(baseBg.b * 1.12f, 1.0f), baseBg.a);
  }

  using motion::MotionHandle;
  auto &motionMgr = uiState->motionManager;

  glm::vec4 animatedHeaderBg = motionMgr.animate<glm::vec4>(
      MotionHandle{selectHeaderId.id + 0x1000}, targetHeaderBg, 0.15f,
      motion::AnimationCurve::EaseOut());

  float chevronAlpha =
      motionMgr.animate<float>(MotionHandle{selectHeaderId.id + 0x2000},
                               isOpen ? 1.0f : (isHovered ? 0.9f : 0.5f), 0.15f,
                               motion::AnimationCurve::EaseOut());

  Modifier headerStyle = std::move(modifier)
                             .background(animatedHeaderBg)
                             .border(strokeColor, strokeWidth)
                             .rounded(radius.x)
                             .padding(14, 8)
                             .row();

  Div(std::move(headerStyle), [&]() {
    Div(Modifier().row().gap(10).center(), [&]() {
      Text(options[selectedIndex], Modifier().color(Colors::white));
      Image(Modifier().size(12.0f, 12.0f), chevronTex,
            glm::vec4(1.0f, 1.0f, 1.0f, chevronAlpha));
    });
  });

  bool selectionChanged = false;
  bool anyOptionHovered = false;

  float animProgress = motionMgr.animate<float>(
      MotionHandle{selectHeaderId.id + 0x50000000}, isOpen ? 1.0f : 0.0f, 0.22f,
      motion::AnimationCurve::EaseOut());

  if (animProgress > 0.001f) {
    constexpr float optionHeight = 32.0f;
    constexpr float optionGap = 2.0f;
    constexpr float padVert = 4.0f;

    float dropdownWidth =
        style.width.has_value()
            ? style.width.value()
            : ((headerData.found && headerData.boundingBox.width > 0.0f)
                   ? headerData.boundingBox.width
                   : 160.0f);

    float headerHeight =
        style.height.has_value()
            ? style.height.value()
            : ((headerData.found && headerData.boundingBox.height > 0.0f)
                   ? headerData.boundingBox.height
                   : DEFAULT_HEIGHT);

    float fullHeight =
        (options.size() * optionHeight) +
        ((options.size() > 0 ? options.size() - 1 : 0) * optionGap) +
        (padVert * 2.0f);

    float animatedContainerHeight = fullHeight * animProgress;
    float offsetX = 0.0f;
    float offsetY = headerHeight + 4.0f;

    if (placement == DropdownPlacement::Top) {
      offsetY = -animatedContainerHeight - 4.0f;
    }

    float contentAlpha = animProgress;

    Modifier popoverStyle =
        Modifier()
            .absolute()
            .parentId(selectHeaderId.id)
            .offset(offsetX, offsetY)
            .width(dropdownWidth)
            .height(animatedContainerHeight)
            .background(glm::vec4(baseBg.r, baseBg.g, baseBg.b,
                                  baseBg.a * contentAlpha))
            .border(strokeColor, strokeWidth)
            .rounded(radius.x)
            .padding(4, static_cast<uint16_t>(padVert))
            .column()
            .gap(static_cast<uint16_t>(optionGap));

    Div(std::move(popoverStyle), [&]() {
      for (size_t i = 0; i < options.size(); ++i) {
        Clay_ElementId optionId = Clay__HashStringWithOffset(
            Clay_String{false, 12, "SelectOption"}, static_cast<uint32_t>(i),
            selectHeaderId.id);

        bool isSelected = (i == selectedIndex);
        bool isOptHovered = false;
        Clay_ElementData optData = Clay_GetElementData(optionId);

        if (optData.found && isOpen && animProgress >= 0.95f) {
          isOptHovered = utils::ui::isPointerOverRoundedBox(
              uiState->pointerPos, optData.boundingBox, glm::vec4(4.0f));
        }

        if (isOptHovered) {
          anyOptionHovered = true;
        }

        glm::vec4 targetOptBg = glm::vec4(0.0f);
        if (isSelected) {
          targetOptBg = glm::vec4(1.0f, 1.0f, 1.0f, 0.12f * contentAlpha);
        } else if (isOptHovered) {
          targetOptBg = glm::vec4(1.0f, 1.0f, 1.0f, 0.07f * contentAlpha);
        }

        glm::vec4 animatedOptBg = motionMgr.animate<glm::vec4>(
            MotionHandle{optionId.id + 0x3000}, targetOptBg, 0.12f,
            motion::AnimationCurve::EaseOut());

        Modifier optStyle = Modifier()
                                .background(animatedOptBg)
                                .rounded(4.0f)
                                .width(dropdownWidth - 8.0f)
                                .height(optionHeight)
                                .padding(12, 0)
                                .row();

        Div(std::move(optStyle), [&]() {
          Text(options[i],
               Modifier().color(
                   isSelected ? glm::vec4(1.0f, 1.0f, 1.0f, contentAlpha)
                              : glm::vec4(0.80f, 0.80f, 0.82f, contentAlpha)));
        });

        if (initialIsOpen && isOptHovered && uiState->pointerPressed) {
          selectedIndex = i;
          isOpen = false;
          selectionChanged = true;
        }
      }
    });

    if (initialIsOpen && animProgress >= 0.95f && uiState->pointerPressed &&
        !isHovered && !anyOptionHovered) {
      isOpen = false;
    }
  }

  result.hovered = isHovered;
  result.pressed = isOpen;
  result.changed = selectionChanged;

  return result;
}

} // namespace atomic
