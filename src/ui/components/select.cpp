#include "animation/animation.h"
#include "avk/atomic_ui.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/color.h"
#include "ui/components.h"
#include <algorithm>
#include <string>

namespace atomic {

Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options, uint32_t fontId,
                   DropdownPlacement placement) {
  Interaction result{};
  if (options.empty())
    return result;
  if (selectedIndex >= options.size())
    selectedIndex = 0;

  static const uint32_t chevronTex = loadTexture("icons/chevron-down.png");

  const auto &style = modifier.getStyle();
  auto *uiState = utils::layout::getUiState();
  bool initialIsOpen = isOpen;

  // Find the longest option to guarantee static button width
  std::string longestOption = options[0];
  for (const auto &opt : options) {
    if (opt.length() > longestOption.length()) {
      longestOption = opt;
    }
  }

  // 1. Header Declaration & Identification
  Clay_ElementId selectHeaderId = utils::layout::getNextId("SelectHeader");

  bool isHovered = false;
  Clay_ElementData headerData = Clay_GetElementData(selectHeaderId);
  glm::vec4 radius = style.borderRadius.value_or(DEFAULT_BORDER_RADIUS);

  if (headerData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, headerData.boundingBox, radius);
  }

  // SNAPPY CLICK TOGGLE: Evaluates clicks on our stable pointerPressed frame
  bool headerClicked = isHovered && uiState->pointerPressed;
  if (headerClicked) {
    isOpen = !initialIsOpen;
    result.clicked = true;
  }

  bool isPressed = isHovered && uiState->pointerDown;

  Clay__OpenElementWithId(selectHeaderId);

  glm::vec4 baseBg = style.backgroundColor.value_or(DEFAULT_BACKGROUND_NORMAL);
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

  Clay_ElementDeclaration decl{};
  // Constant layout defaults inside C++
  decl.layout = {.sizing = {.width = CLAY_SIZING_FIT(),
                            .height = CLAY_SIZING_FIXED(DEFAULT_HEIGHT)},
                 .padding = {14, 14, 8, 8},
                 .childGap = style.childGap.value_or(0),
                 .childAlignment = {.x = clayAlignX, .y = clayAlignY},
                 .layoutDirection = CLAY_LEFT_TO_RIGHT};

  decl.border = {.color =
                     Clay_Color{strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                                strokeColor.b * 255.0f, strokeColor.a * 255.0f},
                 .width = {static_cast<uint16_t>(strokeWidth),
                           static_cast<uint16_t>(strokeWidth),
                           static_cast<uint16_t>(strokeWidth),
                           static_cast<uint16_t>(strokeWidth)}};

  // 2-ARG SYSTEM HOOK: Apply width/height overrides and absolute floating
  // properties!
  utils::layout::applyStyleToLayout(decl, style);

  glm::vec4 targetHeaderBg = baseBg;
  if (isPressed) {
    targetHeaderBg = glm::vec4(baseBg.r * 0.85f, baseBg.g * 0.85f,
                               baseBg.b * 0.85f, baseBg.a);
  } else if (isHovered) {
    targetHeaderBg = glm::vec4(std::min(baseBg.r * 1.12f, 1.0f),
                               std::min(baseBg.g * 1.12f, 1.0f),
                               std::min(baseBg.b * 1.12f, 1.0f), baseBg.a);
  } else if (isOpen) {
    targetHeaderBg = glm::vec4(std::min(baseBg.r * 1.08f, 1.0f),
                               std::min(baseBg.g * 1.08f, 1.0f),
                               std::min(baseBg.b * 1.08f, 1.0f), baseBg.a);
  }

  glm::vec4 animatedHeaderBg = AnimateVec4(
      selectHeaderId.id + 0x1000, targetHeaderBg, 0.15f, Curves::AppleEaseOut);

  decl.backgroundColor = {
      animatedHeaderBg.r * 255.0f, animatedHeaderBg.g * 255.0f,
      animatedHeaderBg.b * 255.0f, animatedHeaderBg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  // Safe frame allocation - zero memory leak!
  decl.userData = utils::layout::createFramePayload(style);

  Clay__ConfigureOpenElement(decl);

  float chevronAlpha = AnimateFloat(selectHeaderId.id + 0x2000,
                                    isOpen ? 1.0f : (isHovered ? 0.9f : 0.5f),
                                    0.15f, Curves::AppleEaseOut);

  Row(DefaultModifier()
          .border(Colors::white, 0.0f)
          .gap(10)
          .childAlignment({.y = AlignmentY::Center}),
      [&]() {
        // Hash "TextContainer" with parent ID to keep layout indexes stable
        Clay_ElementId textContainerId = Clay__HashString(
            Clay_String{false, 13, "TextContainer"}, selectHeaderId.id);
        Clay__OpenElementWithId(textContainerId);

        Clay_ElementDeclaration tcDecl{};
        tcDecl.layout = {
            .sizing = {.width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT()},
            .childAlignment = {.x = CLAY_ALIGN_X_LEFT,
                               .y = CLAY_ALIGN_Y_CENTER},
            .layoutDirection = CLAY_LEFT_TO_RIGHT};
        Clay__ConfigureOpenElement(tcDecl);

        // Invisible longest text stretches layout predictably
        Text(longestOption, fontId, DefaultModifier().color(glm::vec4(0.0f)));

        // Hash "VisibleText" with parent ID to keep layout indexes stable
        Clay_ElementId visibleTextId = Clay__HashString(
            Clay_String{false, 11, "VisibleText"}, selectHeaderId.id);
        Clay__OpenElementWithId(visibleTextId);

        Clay_ElementDeclaration vtDecl{};
        // Directly modify fields of the value member!
        vtDecl.floating.parentId = textContainerId.id;
        vtDecl.floating.attachPoints = {
            .element = CLAY_ATTACH_POINT_LEFT_CENTER,
            .parent = CLAY_ATTACH_POINT_LEFT_CENTER};
        vtDecl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;
        Clay__ConfigureOpenElement(vtDecl);

        Text(options[selectedIndex], fontId,
             DefaultModifier().color(Colors::white));

        Clay__CloseElement(); // SelectVisibleText
        Clay__CloseElement(); // SelectTextContainer

        Image(DefaultModifier().size(12.0f, 12.0f), chevronTex,
              glm::vec4(1.0f, 1.0f, 1.0f, chevronAlpha));
      });

  Clay__CloseElement();

  bool selectionChanged = false;
  bool anyOptionHovered = false;

  // 2. Dropdown Popover Reveal Animation
  float animProgress =
      AnimateFloat(selectHeaderId.id + 0x50000000, isOpen ? 1.0f : 0.0f, 0.22f,
                   Curves::AppleEaseOut);

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
    float offsetY = 0.0f;

    switch (placement) {
    case DropdownPlacement::Top:
      offsetY = -animatedContainerHeight - 4.0f;
      break;
    case DropdownPlacement::Left:
      offsetX = -dropdownWidth - 4.0f;
      offsetY = 0.0f;
      break;
    case DropdownPlacement::Right:
      offsetX = dropdownWidth + 4.0f;
      offsetY = 0.0f;
      break;
    case DropdownPlacement::Bottom:
    default:
      offsetY = headerHeight + 4.0f;
      break;
    }

    float contentAlpha = animProgress;

    // Hash "SelectDropdown" with parent ID to protect global index counters!
    Clay_ElementId dropdownId = Clay__HashString(
        Clay_String{false, 14, "SelectDropdown"}, selectHeaderId.id);
    Clay__OpenElementWithId(dropdownId);

    Clay_ElementDeclaration floatDecl{};
    // Directly modify fields of the value member!
    floatDecl.floating.offset = {offsetX, offsetY};
    floatDecl.floating.parentId = selectHeaderId.id;
    floatDecl.floating.zIndex = 1000;
    floatDecl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;

    floatDecl.backgroundColor = {baseBg.r * 255.0f, baseBg.g * 255.0f,
                                 baseBg.b * 255.0f,
                                 (baseBg.a * contentAlpha) * 255.0f};

    floatDecl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};
    floatDecl.border = {
        .color = Clay_Color{strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                            strokeColor.b * 255.0f,
                            (strokeColor.a * contentAlpha) * 255.0f},
        .width = {static_cast<uint16_t>(strokeWidth),
                  static_cast<uint16_t>(strokeWidth),
                  static_cast<uint16_t>(strokeWidth),
                  static_cast<uint16_t>(strokeWidth)}};

    floatDecl.layout = {
        .sizing = {.width = CLAY_SIZING_FIXED(dropdownWidth),
                   .height = CLAY_SIZING_FIXED(animatedContainerHeight)},
        .padding = {4, 4, static_cast<uint16_t>(padVert),
                    static_cast<uint16_t>(padVert)},
        .childGap = static_cast<uint16_t>(optionGap),
        .layoutDirection = CLAY_TOP_TO_BOTTOM};

    Clay__ConfigureOpenElement(floatDecl);

    for (size_t i = 0; i < options.size(); ++i) {
      Clay_ElementId optionId =
          Clay__HashStringWithOffset(Clay_String{false, 12, "SelectOption"},
                                     static_cast<uint32_t>(i), dropdownId.id);

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
      if (isSelected && isOptHovered) {
        targetOptBg = glm::vec4(1.0f, 1.0f, 1.0f, 0.1f * contentAlpha);
      } else if (isSelected) {
        targetOptBg = glm::vec4(1.0f, 1.0f, 1.0f, 0.12f * contentAlpha);
      } else if (isOptHovered) {
        targetOptBg = glm::vec4(1.0f, 1.0f, 1.0f, 0.07f * contentAlpha);
      }

      glm::vec4 animatedOptBg = AnimateVec4(optionId.id + 0x3000, targetOptBg,
                                            0.12f, Curves::AppleEaseOut);

      Clay__OpenElementWithId(optionId);

      Clay_ElementDeclaration optDecl{};
      optDecl.layout = {
          .sizing = {.width = CLAY_SIZING_GROW(),
                     .height = CLAY_SIZING_FIXED(optionHeight)},
          .padding = {12, 12, 0, 0},
          .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER},
          .layoutDirection = CLAY_LEFT_TO_RIGHT};

      optDecl.backgroundColor = {
          animatedOptBg.r * 255.0f, animatedOptBg.g * 255.0f,
          animatedOptBg.b * 255.0f, animatedOptBg.a * 255.0f};
      optDecl.cornerRadius = {4.0f, 4.0f, 4.0f, 4.0f};

      Clay__ConfigureOpenElement(optDecl);

      if (contentAlpha > 0.001f) {
        // Hash the text ID using the optionId as the seed
        Clay_ElementId optionTextId =
            Clay__HashString(Clay_String{false, 10, "OptionText"}, optionId.id);

        Text(options[i], fontId, optionTextId,
             DefaultModifier().color(
                 isSelected ? glm::vec4(1.0f, 1.0f, 1.0f, contentAlpha)
                            : glm::vec4(0.80f, 0.80f, 0.82f, contentAlpha)));
      }

      Clay__CloseElement(); // SelectOption

      if (initialIsOpen && isOptHovered && uiState->pointerPressed) {
        selectedIndex = i;
        isOpen = false;
        selectionChanged = true;
      }
    }

    Clay__CloseElement(); // SelectDropdown

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

Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options,
                   DropdownPlacement placement) {
  return Select(std::move(modifier), isOpen, selectedIndex, options,
                getDefaultFontId(), placement);
}

} // namespace atomic
