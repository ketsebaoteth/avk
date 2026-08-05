#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "glm/ext/vector_float4.hpp"
#include "ui/components.h"
#include "ui/core/resources.h"
#include "ui/internal/context.h"
#include "ui/style/style.h"
#include "ui/style/themeManager.h"
#include "ui/utils/clayUtils.h"
#include "ui/utils/color.h"

#include <cmath>
#include <cstdint>
#include <string_view>

namespace atomic {

// button implementaiton :)
Interaction Button(Modifier &&modifier, const std::function<void()> &content) {
  auto *uiState = getUiState();
  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};

  const auto &rawStyle = modifier.getStyle();
  auto &tm = ThemeManager::getInstance();

  std::string labelId = rawStyle.elementLabel.value_or("BtnAnim");
  uint32_t btnId = hashLabel(labelId);

  bool isDisabled = rawStyle.disabled.value_or(false) || inherited.disabled;
  bool wasHovered = !isDisabled && isHovered(btnId);
  bool wasPressed = !isDisabled && isPressed(btnId);

  float scaleMult = tm.getVariable<float>(ThemeVarId::ScaleMultiplier, 1.0f);
  float spacingMult =
      tm.getVariable<float>(ThemeVarId::SpacingMultiplier, 1.0f);
  float fontSizeMult =
      tm.getVariable<float>(ThemeVarId::FontSizeMultiplier, 1.0f);
  float borderRadiusMult =
      tm.getVariable<float>(ThemeVarId::BorderRadiusMultiplier, 1.0f);
  float borderWidthMult =
      tm.getVariable<float>(ThemeVarId::BorderWidthMultiplier, 1.0f);

  glm::vec4 themeBaseBg =
      tm.getVariable<glm::vec4>(ThemeVarId::ColorBgSurface, "#ffffff"_hex);
  glm::vec4 baseBg = rawStyle.backgroundColor.value_or(themeBaseBg);

  glm::vec4 defaultHoverBg = glm::vec4(
      std::min(baseBg.r * 0.93f, 1.0f), std::min(baseBg.g * 0.93f, 1.0f),
      std::min(baseBg.b * 0.93f, 1.0f), baseBg.a);

  glm::vec4 hoverBg = tm.getVariable<glm::vec4>(ThemeVarId::ColorBgSurfaceHover,
                                                defaultHoverBg);

  if (rawStyle.hoveredStyle.has_value() &&
      rawStyle.hoveredStyle.value()->backgroundColor.has_value()) {
    hoverBg = rawStyle.hoveredStyle.value()->backgroundColor.value();
  }

  glm::vec4 defaultActiveBg =
      glm::vec4(baseBg.r * 0.86f, baseBg.g * 0.86f, baseBg.b * 0.86f, baseBg.a);

  glm::vec4 activeBg = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorBgSurfaceActive, defaultActiveBg);

  if (rawStyle.activeStyle.has_value() &&
      rawStyle.activeStyle.value()->backgroundColor.has_value()) {
    activeBg = rawStyle.activeStyle.value()->backgroundColor.value();
  }

  glm::vec4 targetBg = wasPressed ? activeBg : (wasHovered ? hoverBg : baseBg);
  float themeFontSize =
      tm.getVariable<float>(ThemeVarId::ButtonFontSize, 14.0f);
  float finalFontSize =
      rawStyle.fontSize.value_or(themeFontSize) * fontSizeMult;

  float themeFontWeight =
      tm.getVariable<float>(ThemeVarId::ButtonFontWeight, 500.0f);
  float finalFontWeight = rawStyle.fontWeight.value_or(themeFontWeight);

  float themePadX = tm.getVariable<float>(ThemeVarId::ButtonPadX, 16.0f);
  float themePadY = tm.getVariable<float>(ThemeVarId::ButtonPadY, 14.0f);
  float finalPadX =
      (rawStyle.padLeft.has_value() ? rawStyle.padLeft.value() : themePadX) *
      spacingMult;
  float finalPadY =
      (rawStyle.padTop.has_value() ? rawStyle.padTop.value() : themePadY) *
      spacingMult;

  glm::vec4 themeBorderColor = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorBorderNormal, Colors::transparent);
  glm::vec4 strokeColor = rawStyle.strokeColor.value_or(themeBorderColor);

  glm::vec4 themeBorderWidth =
      glm::vec4(tm.getVariable<float>(ThemeVarId::BorderWidthThin, 1.0f));
  glm::vec4 strokeThickness =
      (rawStyle.strokeThickness.value_or(themeBorderWidth)) * borderWidthMult;

  float themeRadius = tm.getVariable<float>(ThemeVarId::BorderRadiusLg, 10.0f);
  glm::vec4 borderRadius =
      (rawStyle.borderRadius.value_or(glm::vec4(themeRadius))) *
      borderRadiusMult;

  glm::vec4 themeTextColor = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorTextPrimary, Colors::black[900]);
  glm::vec4 textColor = rawStyle.textColor.value_or(themeTextColor);

  float baseScale = rawStyle.scale.value_or(1.0f) * scaleMult;

  Modifier btnStyle = std::move(modifier)
                          .id(labelId)
                          .background(targetBg)
                          .scale(baseScale)
                          .fontSize(finalFontSize)
                          .fontWeight(finalFontWeight)
                          .padding(finalPadX, finalPadY)
                          .border(strokeColor, strokeThickness)
                          .rounded(borderRadius)
                          .color(textColor)
                          .disabled(isDisabled)
                          .row()
                          .center();

  if (!rawStyle.transitionSpec.has_value()) {
    float transDuration =
        tm.getVariable<float>(ThemeVarId::TransitionDurationNormal, 0.15f);
    btnStyle = std::move(btnStyle).transition(
        transDuration, motion::AnimationCurve::EaseOut());
  }

  Interaction result = Div(std::move(btnStyle), content);

  if (isDisabled) {
    result.hovered = false;
    result.pressed = false;
    result.clicked = false;
  }

  return result;
}

// checkbox implementation :)
Interaction Checkbox(Modifier &&modifier, bool &checked) {
  const auto &style = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId checkboxId =
      style.elementLabel.has_value()
          ? utils::layout::getNextId(style.elementLabel.value().c_str())
          : utils::layout::getNextId("Checkbox");

  float baseSize = style.width.value_or(20.0f);
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(4.0f));

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(checkboxId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool clicked = false;
  if (isHovered && uiState->pointerPressed) {
    checked = !checked;
    clicked = true;
  }

  bool isPressed = isHovered && uiState->pointerDown;

  using motion::MotionHandle;
  auto &motionMgr = uiState->motionManager;

  float targetScale = isPressed ? 0.9f : 1.0f;
  float animatedScale = motionMgr.animate<float>(
      MotionHandle{checkboxId.id + 0x4000}, targetScale, 0.15f,
      motion::AnimationCurve::EaseOut());

  float finalScale = animatedScale * style.scale.value_or(1.0f);

  glm::vec4 inactiveBg = glm::vec4(0.06f, 0.06f, 0.06f, 1.0f);
  glm::vec4 activeBg = style.backgroundColor.value_or(Colors::orange);

  glm::vec4 targetBg = checked ? activeBg : inactiveBg;
  if (isPressed && !checked) {
    targetBg = inactiveBg * 1.15f;
  }

  glm::vec4 animatedBg = motionMgr.animate<glm::vec4>(
      MotionHandle{checkboxId.id + 0x1000}, targetBg, 0.15f,
      motion::AnimationCurve::EaseOut());

  glm::vec4 inactiveBorder = glm::vec4(0.24f, 0.24f, 0.27f, 1.0f);
  glm::vec4 targetBorder = checked ? activeBg : inactiveBorder;
  glm::vec4 animatedBorder = motionMgr.animate<glm::vec4>(
      MotionHandle{checkboxId.id + 0x2000}, targetBorder, 0.15f,
      motion::AnimationCurve::EaseOut());

  Modifier boxStyle = std::move(modifier)
                          .background(animatedBg)
                          .border(animatedBorder, 1.0f)
                          .size(baseSize, baseSize)
                          .rounded(radius.x)
                          .scale(finalScale)
                          .center();

  Interaction result = Div(std::move(boxStyle), [&]() {
    float targetAlpha = checked ? 1.0f : 0.0f;
    float animatedAlpha = motionMgr.animate<float>(
        MotionHandle{checkboxId.id + 0x3000}, targetAlpha, 0.15f,
        motion::AnimationCurve::EaseOut());

    if (animatedAlpha > 0.01f) {
      glm::vec4 checkColor = Colors::white;
      checkColor.a = animatedAlpha;

      Icon(LucideIcon::Check, Modifier().color(checkColor).size(15.0f, 15.0f));
    }
  });

  result.clicked = clicked;
  result.pressed = checked;

  return result;
}

static ResizeEdge detectHoveredResizeEdge(const glm::vec2 &mousePos,
                                          const Clay_BoundingBox &box,
                                          const ResizeConfig &cfg) {
  float l = box.x, r = box.x + box.width;
  float t = box.y, b = box.y + box.height;

  float sThresh = cfg.sideProximity;
  float cThresh = cfg.cornerProximity;

  // ⚡ MASK: If absolute positioning is disabled, lock to Right, Bottom,
  // BottomRight
  ResizeEdge effectiveEdges = cfg.allowedEdges;
  if (!cfg.allowAbsolutePositioning) {
    effectiveEdges = static_cast<ResizeEdge>(
        static_cast<uint8_t>(effectiveEdges) &
        static_cast<uint8_t>(ResizeEdge::Right | ResizeEdge::Bottom |
                             ResizeEdge::BottomRight));
  }

  // 1. Check 4 Corners (Higher Priority)
  if (std::abs(mousePos.x - l) <= cThresh &&
      std::abs(mousePos.y - t) <= cThresh &&
      hasEdge(effectiveEdges, ResizeEdge::TopLeft))
    return ResizeEdge::TopLeft;

  if (std::abs(mousePos.x - r) <= cThresh &&
      std::abs(mousePos.y - t) <= cThresh &&
      hasEdge(effectiveEdges, ResizeEdge::TopRight))
    return ResizeEdge::TopRight;

  if (std::abs(mousePos.x - l) <= cThresh &&
      std::abs(mousePos.y - b) <= cThresh &&
      hasEdge(effectiveEdges, ResizeEdge::BottomLeft))
    return ResizeEdge::BottomLeft;

  if (std::abs(mousePos.x - r) <= cThresh &&
      std::abs(mousePos.y - b) <= cThresh &&
      hasEdge(effectiveEdges, ResizeEdge::BottomRight))
    return ResizeEdge::BottomRight;

  // 2. Check 4 Sides
  bool nearLeft = (std::abs(mousePos.x - l) <= sThresh) &&
                  (mousePos.y >= t && mousePos.y <= b);
  bool nearRight = (std::abs(mousePos.x - r) <= sThresh) &&
                   (mousePos.y >= t && mousePos.y <= b);
  bool nearTop = (std::abs(mousePos.y - t) <= sThresh) &&
                 (mousePos.x >= l && mousePos.x <= r);
  bool nearBottom = (std::abs(mousePos.y - b) <= sThresh) &&
                    (mousePos.x >= l && mousePos.x <= r);

  if (nearTop && hasEdge(effectiveEdges, ResizeEdge::Top))
    return ResizeEdge::Top;
  if (nearBottom && hasEdge(effectiveEdges, ResizeEdge::Bottom))
    return ResizeEdge::Bottom;
  if (nearLeft && hasEdge(effectiveEdges, ResizeEdge::Left))
    return ResizeEdge::Left;
  if (nearRight && hasEdge(effectiveEdges, ResizeEdge::Right))
    return ResizeEdge::Right;

  return ResizeEdge::NoEdge;
}

// div implementation :)
Interaction Div(Modifier &&modifier, const std::function<void()> &content) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();
  auto &tm = ThemeManager::getInstance();

  float spacingMult =
      tm.getVariable<float>(ThemeVarId::SpacingMultiplier, 1.0f);
  float borderRadiusMult =
      tm.getVariable<float>(ThemeVarId::BorderRadiusMultiplier, 1.0f);
  float borderWidthMult =
      tm.getVariable<float>(ThemeVarId::BorderWidthMultiplier, 1.0f);

  Clay_ElementId divId =
      rawStyle.elementLabel.has_value()
          ? utils::layout::getNextId(rawStyle.elementLabel.value().c_str())
          : utils::layout::getNextId("Div");

  Style style = utils::layout::resolveTransitions(divId.id, rawStyle);

  // --------------------------------------------------------------------------
  // RESIZE ENGINE: Apply Persistent Size & Position Overrides
  // --------------------------------------------------------------------------
  bool isResizable =
      style.resizeConfig.has_value() && style.resizeConfig->enabled;
  const ResizeConfig *resizeCfg =
      isResizable ? &style.resizeConfig.value() : nullptr;

  if (isResizable && uiState) {
    // 1. Always apply width & height override for flex layout calculation
    auto sizeIt = uiState->persistentDivSizes.find(divId.id);
    if (sizeIt != uiState->persistentDivSizes.end()) {
      style.width = sizeIt->second.x;
      style.height = sizeIt->second.y;
    }

    // 2. Only convert to Position::Absolute if allowAbsolutePositioning == true
    // or if the Div was originally declared as Absolute/Fixed
    auto originalPos = style.position.value_or(Position::Normal);
    bool canBeAbsolute = resizeCfg->allowAbsolutePositioning ||
                         originalPos == Position::Absolute ||
                         originalPos == Position::Fixed;

    if (canBeAbsolute) {
      auto posIt = uiState->persistentDivPositions.find(divId.id);
      if (posIt != uiState->persistentDivPositions.end()) {
        style.left = posIt->second.x;
        style.top = posIt->second.y;
        style.position = Position::Absolute;
      }
    }
  }

  bool hasMargin =
      style.marginLeft.has_value() || style.marginRight.has_value() ||
      style.marginTop.has_value() || style.marginBottom.has_value();

  Clay_ElementId outerId = divId;
  outerId.id += 0x6D417267;
  if (hasMargin) {
    Clay__OpenElementWithId(outerId);
    float ml = style.marginLeft.value_or(0.0f) * spacingMult;
    float mr = style.marginRight.value_or(0.0f) * spacingMult;
    float mt = style.marginTop.value_or(0.0f) * spacingMult;
    float mb = style.marginBottom.value_or(0.0f) * spacingMult;
    Clay_ElementDeclaration outerDecl{};
    utils::layout::applyStyleToLayout(outerDecl, style);
    outerDecl.layout.padding = {static_cast<uint16_t>(std::round(ml)),
                                static_cast<uint16_t>(std::round(mr)),
                                static_cast<uint16_t>(std::round(mt)),
                                static_cast<uint16_t>(std::round(mb))};
    outerDecl.backgroundColor = {0, 0, 0, 0};
    Clay__ConfigureOpenElement(outerDecl);
  }

  Clay__OpenElementWithId(divId);

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  glm::vec4 themeBg =
      tm.getVariable<glm::vec4>(ThemeVarId::ColorBgSurface, glm::vec4(1.0f));
  glm::vec4 bg = style.backgroundColor.value_or(
      style.gradient.has_value() ? glm::vec4(1.0f) : themeBg);
  bg.a *= effectiveOpacity;

  glm::vec4 themeRadius =
      glm::vec4(tm.getVariable<float>(ThemeVarId::BorderRadiusLg, 0.0f));
  glm::vec4 radius =
      (style.borderRadius.value_or(themeRadius)) * borderRadiusMult;

  glm::vec4 themeBorderColor = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorBorderNormal, DEFAULT_BORDER_NORMAL);
  glm::vec4 strokeColor = style.strokeColor.value_or(themeBorderColor);
  strokeColor.a *= effectiveOpacity;

  glm::vec4 themeBorderWidth =
      glm::vec4(tm.getVariable<float>(ThemeVarId::BorderWidthNone, 0.0f));
  glm::vec4 strokeWidth =
      (style.strokeThickness.value_or(themeBorderWidth)) * borderWidthMult;

  LayoutDirection dir = style.direction.value_or(LayoutDirection::Row);
  Clay_LayoutDirection clayDir = (dir == LayoutDirection::Column)
                                     ? CLAY_TOP_TO_BOTTOM
                                     : CLAY_LEFT_TO_RIGHT;
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
  Style innerStyle = style;
  if (hasMargin) {
    innerStyle.width = 0;
    innerStyle.height = 0;
  }
  utils::layout::applyStyleToLayout(decl, innerStyle);

  const float padL = style.padLeft.value_or(0.0f) * spacingMult;
  const float padR = style.padRight.value_or(0.0f) * spacingMult;
  const float padT = style.padTop.value_or(0.0f) * spacingMult;
  const float padB = style.padBottom.value_or(0.0f) * spacingMult;

  decl.layout.padding = {static_cast<uint16_t>(std::round(padL)),
                         static_cast<uint16_t>(std::round(padR)),
                         static_cast<uint16_t>(std::round(padT)),
                         static_cast<uint16_t>(std::round(padB))};
  decl.layout.childGap = static_cast<uint16_t>(
      std::round(style.childGap.value_or(0.0f) * spacingMult));
  decl.layout.childAlignment = {.x = clayAlignX, .y = clayAlignY};
  decl.layout.layoutDirection = clayDir;
  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};
  if (strokeWidth.x > 0.0f || strokeWidth.y > 0.0f || strokeWidth.z > 0.0f ||
      strokeWidth.w > 0.0f) {
    decl.border = {.color = {strokeColor.r * 255.0f, strokeColor.g * 255.0f,
                             strokeColor.b * 255.0f, strokeColor.a * 255.0f},
                   .width = {static_cast<uint16_t>(strokeWidth.x),
                             static_cast<uint16_t>(strokeWidth.y),
                             static_cast<uint16_t>(strokeWidth.z),
                             static_cast<uint16_t>(strokeWidth.w)}};
  }

  auto pos = style.position.value_or(Position::Normal);
  utils::layout::PositioningContextGuard posGuard(divId.id, pos);
  utils::layout::StyleCascadeGuard styleGuard(style);
  decl.userData = utils::layout::createFramePayload(style);
  Clay__ConfigureOpenElement(decl);

  bool pushedTextConstraint = false;
  if (uiState) {
    float constraintW = 0.0f;
    const bool hasWidth = style.width.has_value();
    const float rawW = hasWidth ? style.width.value() : -1.0f;
    const bool isGrow = hasWidth && rawW == 0.0f;
    const bool isFixed = hasWidth && rawW > 0.0f;

    if (isFixed) {
      constraintW = (rawW * spacingMult) - padL - padR;
    } else if (isGrow || !hasWidth) {
      if (!uiState->textConstraintWidthStack.empty()) {
        constraintW = uiState->textConstraintWidthStack.back() - padL - padR;
      }
    }

    if (constraintW > 0.5f) {
      uiState->textConstraintWidthStack.push_back(constraintW);
      pushedTextConstraint = true;
    }
  }

  bool clayHovered = Clay_Hovered();

  // 1. Render User Content
  if (content) {
    content();
  }

  if (uiState && pushedTextConstraint) {
    uiState->textConstraintWidthStack.pop_back();
  }

  // --------------------------------------------------------------------------
  // ⚡ 2. RENDER 8 HANDLE OVERLAYS BEFORE CLOSING PARENT ELEMENT
  // --------------------------------------------------------------------------
  ResizeEdge hoveredEdge = ResizeEdge::NoEdge;
  ResizeEdge activeEdge = ResizeEdge::NoEdge;
  bool isActivelyResizingThis = false;

  Clay_ElementData elementData = Clay_GetElementData(divId);
  if (isResizable && uiState && elementData.found) {
    const auto &box = elementData.boundingBox;

    // Detect mouse hover edge/corner
    hoveredEdge = detectHoveredResizeEdge(uiState->pointerPos, box, *resizeCfg);

    // Anchor-Locked Resize Drag Calculations
    if (uiState->activeResize.has_value() &&
        uiState->activeResize->elementId == divId.id) {
      isActivelyResizingThis = true;
      activeEdge = uiState->activeResize->activeEdge;

      if (uiState->pointerDown) {
        glm::vec2 mouseDelta =
            uiState->pointerPos - uiState->activeResize->dragStartMousePos;
        glm::vec2 newSize = uiState->activeResize->startSize;
        glm::vec2 newPos = uiState->activeResize->startPosition;

        // Resize Right / Left (Lock opposite side)
        if (hasEdge(activeEdge, ResizeEdge::Right)) {
          newSize.x =
              std::clamp(uiState->activeResize->startSize.x + mouseDelta.x,
                         resizeCfg->minSize.x, resizeCfg->maxSize.x);
        } else if (hasEdge(activeEdge, ResizeEdge::Left) &&
                   resizeCfg->allowAbsolutePositioning) {
          float unclampedWidth =
              uiState->activeResize->startSize.x - mouseDelta.x;
          newSize.x = std::clamp(unclampedWidth, resizeCfg->minSize.x,
                                 resizeCfg->maxSize.x);
          float actualDeltaX = uiState->activeResize->startSize.x - newSize.x;
          newPos.x = uiState->activeResize->startPosition.x + actualDeltaX;
        }

        // Resize Bottom / Top (Lock opposite side)
        if (hasEdge(activeEdge, ResizeEdge::Bottom)) {
          newSize.y =
              std::clamp(uiState->activeResize->startSize.y + mouseDelta.y,
                         resizeCfg->minSize.y, resizeCfg->maxSize.y);
        } else if (hasEdge(activeEdge, ResizeEdge::Top) &&
                   resizeCfg->allowAbsolutePositioning) {
          float unclampedHeight =
              uiState->activeResize->startSize.y - mouseDelta.y;
          newSize.y = std::clamp(unclampedHeight, resizeCfg->minSize.y,
                                 resizeCfg->maxSize.y);
          float actualDeltaY = uiState->activeResize->startSize.y - newSize.y;
          newPos.y = uiState->activeResize->startPosition.y + actualDeltaY;
        }

        // Store persistent size and position
        uiState->persistentDivSizes[divId.id] = newSize;
        if (resizeCfg->allowAbsolutePositioning) {
          uiState->persistentDivPositions[divId.id] = newPos;
        }

        if (resizeCfg->onResize) {
          resizeCfg->onResize(newSize, newPos);
        }
      } else {
        // Release resize drag
        uiState->activeResize.reset();
      }
    } else if (hoveredEdge != ResizeEdge::NoEdge && uiState->pointerPressed) {
      // Start resize drag on click
      uiState->activeResize =
          ActiveResizeState{.elementId = divId.id,
                            .activeEdge = hoveredEdge,
                            .dragStartMousePos = uiState->pointerPos,
                            .startPosition = glm::vec2(box.x, box.y),
                            .startSize = glm::vec2(box.width, box.height),
                            .currentSize = glm::vec2(box.width, box.height),
                            .currentPosition = glm::vec2(box.x, box.y)};
      isActivelyResizingThis = true;
      activeEdge = hoveredEdge;
    }

    // Helper lambda to render 8 handle callbacks INSIDE divId's open element
    // scope!
    auto renderHandleOverlay = [&](ResizeEdge flag, HandleRenderCallback cb,
                                   float hLeft, float hTop, float hW,
                                   float hH) {
      ResizeEdge allowed = resizeCfg->allowedEdges;
      if (!resizeCfg->allowAbsolutePositioning) {
        allowed = static_cast<ResizeEdge>(
            static_cast<uint8_t>(allowed) &
            static_cast<uint8_t>(ResizeEdge::Right | ResizeEdge::Bottom |
                                 ResizeEdge::BottomRight));
      }

      if (!hasEdge(allowed, flag))
        return;

      bool hHovered = (hoveredEdge == flag);
      bool hDragging = (activeEdge == flag && isActivelyResizingThis);

      if (cb) {
        HandleState hs{.edge = flag,
                       .isHovered = hHovered,
                       .isDragging = hDragging,
                       .currentSize = glm::vec2(box.width, box.height),
                       .currentPosition = glm::vec2(box.x, box.y)};

        // ⚡ FIX: Removed .relative() so .absolute() floats element attached to
        // divId!
        Div(Modifier()
                .absolute()
                .parentId(divId.id)
                .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
                .left(hLeft)
                .top(hTop)
                .width(hW)
                .height(hH)
                .pointerEvents(false)
                .zIndex(100),
            [&]() { cb(hs); });
      }
    };

    // Render 4 Side Handles
    renderHandleOverlay(ResizeEdge::Top, resizeCfg->onRenderTop, 0, 0,
                        box.width, 10);
    renderHandleOverlay(ResizeEdge::Bottom, resizeCfg->onRenderBottom, 0,
                        box.height, box.width, 10);
    renderHandleOverlay(ResizeEdge::Left, resizeCfg->onRenderLeft, 0, 0, 10,
                        box.height);
    renderHandleOverlay(ResizeEdge::Right, resizeCfg->onRenderRight, box.width,
                        0, 10, box.height);

    // Render 4 Corner Handles
    renderHandleOverlay(ResizeEdge::TopLeft, resizeCfg->onRenderTopLeft, 0, 0,
                        10, 10);
    renderHandleOverlay(ResizeEdge::TopRight, resizeCfg->onRenderTopRight,
                        box.width, 0, 10, 10);
    renderHandleOverlay(ResizeEdge::BottomLeft, resizeCfg->onRenderBottomLeft,
                        0, box.height, 10, 10);
    renderHandleOverlay(ResizeEdge::BottomRight, resizeCfg->onRenderBottomRight,
                        box.width, box.height, 10, 10);
  }

  // ⚡ 3. NOW CLOSE PARENT ELEMENTS IN CLAY
  Clay__CloseElement();
  if (hasMargin) {
    Clay__CloseElement();
  }

  bool isHovered = false;
  if (clayHovered && elementData.found) {
    glm::vec2 translation =
        style.translate.value_or(glm::vec2(0.0f)) * spacingMult;
    Clay_BoundingBox hitBox = elementData.boundingBox;
    hitBox.x += translation.x;
    hitBox.y += translation.y;
    isHovered =
        utils::ui::isPointerOverRoundedBox(uiState->pointerPos, hitBox, radius);
  }

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isHovered && uiState->pointerDown;
  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  if (uiState) {
    ElementLifecycleState lifecycle{};
    lifecycle.isMounted = true;
    lifecycle.isHovered = isHovered;
    lifecycle.isPressed = result.pressed;
    uiState->currentLifecycleMap[divId.id] = lifecycle;
    uiState->computedStyleMap[divId.id] = uiState->getActiveCascadingStyle();
  }

  return result;
}

/**
 * @brief Converts a 32-bit Unicode codepoint to a standard UTF-8 string.
 */
std::string codepointToUtf8(char32_t codepoint) {
  std::string out;
  if (codepoint <= 0x7F) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0x10FFFF) {
    out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
  return out;
}

// icon implementaiton :)
Interaction Icon(LucideIcon icon, Modifier &&modifier) {
  static std::unordered_map<LucideIcon, std::string> iconStringCache;

  if (iconStringCache.find(icon) == iconStringCache.end()) {
    const auto codepoint = static_cast<char32_t>(icon);
    iconStringCache[icon] = codepointToUtf8(codepoint);
  }

  const std::string &iconString = iconStringCache[icon];
  const auto &style = modifier.getStyle();

  const float requestedSize = style.fontSize.value_or(
      style.height.value_or(style.width.value_or(16.0f)));

  auto savedFontId = getActiveFont();
  SetActiveFont(getUiState()->defaultIconFontIds[0]);
  auto interaction = Text(iconString, std::move(modifier)
                                          .fontSize(requestedSize)
                                          .size(requestedSize, requestedSize));
  // reset font
  SetActiveFont(savedFontId);
  return interaction;
}

// scrollview implementaiton :)
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

// select implementation
Interaction Select(Modifier &&modifier, bool &isOpen, size_t &selectedIndex,
                   const std::vector<std::string> &options,
                   DropdownPlacement placement) {
  Interaction result{};
  if (options.empty())
    return result;
  if (selectedIndex >= options.size())
    selectedIndex = 0;
  // update this to not use a texture and to use lucide icons icon
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

// switch implementation
Interaction Switch(Modifier &&modifier, bool &checked) {
  const auto &style = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId switchId =
      style.elementLabel.has_value()
          ? utils::layout::getNextId(style.elementLabel.value().c_str())
          : utils::layout::getNextId("Switch");

  float width = style.width.value_or(48.0f);
  float height = style.height.value_or(26.0f);
  float pad = 3.0f;

  float pillRadius = height * 0.5f;
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(pillRadius));

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(switchId);
  if (elementData.found) {
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool clicked = false;
  if (isHovered && uiState->pointerPressed) {
    checked = !checked;
    clicked = true;
  }

  glm::vec4 inactiveColor = glm::vec4(0.15f, 0.15f, 0.16f, 1.0f);
  glm::vec4 activeColor = style.backgroundColor.value_or(Colors::orange);
  glm::vec4 targetColor = checked ? activeColor : inactiveColor;

  using motion::MotionHandle;
  auto &motionMgr = uiState->motionManager;

  glm::vec4 animatedColor = motionMgr.animate<glm::vec4>(
      MotionHandle{switchId.id + 0x1000}, targetColor, 0.18f,
      motion::AnimationCurve::EaseOut());

  float thumbSize = height - (pad * 2.0f);
  float minX = pad;
  float maxX = width - pad - thumbSize;
  float targetX = checked ? maxX : minX;

  float animatedX =
      motionMgr.animate<float>(MotionHandle{switchId.id + 0x2000}, targetX,
                               0.18f, motion::AnimationCurve::EaseOut());

  Modifier switchStyle = std::move(modifier)
                             .background(animatedColor)
                             .size(width, height)
                             .rounded(pillRadius)
                             .relative();

  Interaction result = Div(std::move(switchStyle), [&]() {
    Div(Modifier()
            .absolute()
            .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
            .offset(animatedX, pad)
            .size(thumbSize, thumbSize)
            .background(Colors::white)
            .rounded(thumbSize * 0.5f));
  });

  result.clicked = clicked;
  result.pressed = checked;

  return result;
}

// text implementation :)
Interaction Text(std::string_view text, Modifier &&modifier) {
  const auto &rawStyle = modifier.getStyle();
  Clay_ElementId textId =
      rawStyle.elementLabel.has_value()
          ? utils::layout::getNextId(rawStyle.elementLabel.value().c_str())
          : utils::layout::getNextId("Text");
  return Text(text, textId, std::move(modifier));
}

Interaction Text(std::string_view text, Clay_ElementId textId,
                 Modifier &&modifier) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();
  auto &tm = ThemeManager::getInstance();

  // ⚡ ENUM O(1) LOOKUPS
  float fontSizeMult =
      tm.getVariable<float>(ThemeVarId::FontSizeMultiplier, 1.0f);
  float spacingMult =
      tm.getVariable<float>(ThemeVarId::SpacingMultiplier, 1.0f);

  Style style = utils::layout::resolveTransitions(textId.id, rawStyle);

  bool hasMargin =
      style.marginLeft.has_value() || style.marginRight.has_value() ||
      style.marginTop.has_value() || style.marginBottom.has_value();

  Clay_ElementId outerId = textId;
  outerId.id += 0x6D417267;

  constexpr float BASE_UI_SCALE = 2.0f;

  float monitorDpi =
      (getVeraApp() && getVeraApp()->getPrimaryMonitor().dpiScale > 0.0f)
          ? getVeraApp()->getPrimaryMonitor().dpiScale
          : 1.0f;

  float effectiveScale = monitorDpi * BASE_UI_SCALE;

  if (hasMargin) {
    Clay__OpenElementWithId(outerId);

    float ml = style.marginLeft.value_or(0.0f) * spacingMult * effectiveScale;
    float mr = style.marginRight.value_or(0.0f) * spacingMult * effectiveScale;
    float mt = style.marginTop.value_or(0.0f) * spacingMult * effectiveScale;
    float mb = style.marginBottom.value_or(0.0f) * spacingMult * effectiveScale;

    Clay_ElementDeclaration outerDecl{};
    utils::layout::applyStyleToLayout(outerDecl, style);

    outerDecl.layout.padding = {static_cast<uint16_t>(std::round(ml)),
                                static_cast<uint16_t>(std::round(mr)),
                                static_cast<uint16_t>(std::round(mt)),
                                static_cast<uint16_t>(std::round(mb))};

    outerDecl.backgroundColor = {0, 0, 0, 0};
    Clay__ConfigureOpenElement(outerDecl);
  }

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  uint32_t fontId = getActiveFont();
  uint32_t finalFontId = (fontId != INVALID_FONT_ID)
                             ? fontId
                             : (inherited.fontId.value_or(getDefaultFontId()));

  avk::Font *font = getFont(finalFontId);

  float defaultLogicalFontSize = (font && font->getFontSize() > 0)
                                     ? static_cast<float>(font->getFontSize())
                                     : 16.0f;

  float unscaledLogicalFontSize =
      (style.fontSize.value_or(
          inherited.fontSize.value_or(defaultLogicalFontSize))) *
      fontSizeMult;

  float finalPhysicalFontSize = unscaledLogicalFontSize * effectiveScale;

  glm::vec4 themeTextColor = tm.getVariable<glm::vec4>(
      ThemeVarId::ColorTextPrimary, Colors::black[900]);
  glm::vec4 textColor =
      style.textColor.value_or(inherited.textColor.value_or(themeTextColor));

  textColor.a *= effectiveOpacity;
  float textOffset =
      style.textOffset.value_or(inherited.textOffset.value_or(0.0f));

  // ⚡ Fix: Explicitly construct std::string for copyStringToClayBuffer
  Clay_String allocatedString = copyStringToClayBuffer(std::string(text));

  Clay_TextAlignment clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
  if (style.textAlign.has_value()) {
    switch (style.textAlign.value()) {
    case TextAlign::Center:
      clayTextAlign = CLAY_TEXT_ALIGN_CENTER;
      break;
    case TextAlign::Right:
      clayTextAlign = CLAY_TEXT_ALIGN_RIGHT;
      break;
    default:
      clayTextAlign = CLAY_TEXT_ALIGN_LEFT;
      break;
    }
  }

  auto *payload = utils::layout::createFramePayload(style, std::nullopt,
                                                    std::nullopt, textOffset);

  Clay_TextElementConfig config{};
  config.fontId = static_cast<uint16_t>(finalFontId);
  config.fontSize = static_cast<uint16_t>(std::round(finalPhysicalFontSize));
  config.textColor = {textColor.r * 255.0f, textColor.g * 255.0f,
                      textColor.b * 255.0f, textColor.a * 255.0f};
  config.textAlignment = clayTextAlign;
  config.userData = payload;

  Clay__OpenTextElement(allocatedString, config);

  if (hasMargin) {
    Clay__CloseElement();
  }

  bool isHovered = false;
  Clay_ElementData elementData = Clay_GetElementData(textId);
  if (elementData.found) {
    glm::vec4 themeRadius =
        glm::vec4(tm.getVariable<float>(ThemeVarId::BorderRadiusLg, 0.0f));
    glm::vec4 radius = style.borderRadius.value_or(themeRadius);
    isHovered = utils::ui::isPointerOverRoundedBox(
        uiState->pointerPos, elementData.boundingBox, radius);
  }

  bool isPressed = isHovered && uiState->pointerDown;

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isPressed;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  if (uiState) {
    ElementLifecycleState lifecycle{};
    lifecycle.isMounted = true;
    lifecycle.isHovered = isHovered;
    lifecycle.isPressed = isPressed;
    uiState->currentLifecycleMap[textId.id] = lifecycle;

    CascadingStyle resolved = inherited;
    resolved.textColor = textColor;
    resolved.textOffset = textOffset;
    resolved.fontId = finalFontId;
    resolved.fontSize = unscaledLogicalFontSize;
    resolved.fontWeight =
        style.fontWeight.value_or(inherited.fontWeight.value_or(400.0f));
    resolved.letterSpacing =
        style.letterSpacing.value_or(inherited.letterSpacing.value_or(0.0f));
    resolved.lineHeight =
        style.lineHeight.value_or(inherited.lineHeight.value_or(0.0f));

    uiState->computedStyleMap[textId.id] = resolved;
  }

  return result;
}

namespace {

int getUtf8CharLength(unsigned char c) {
  if (c < 0x80)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

uint32_t getPreviousCharIndex(const std::string &str, uint32_t index) {
  if (index == 0)
    return 0;
  uint32_t idx = index;
  do {
    idx--;
  } while (idx > 0 && (static_cast<unsigned char>(str[idx]) & 0xC0) == 0x80);
  return idx;
}

uint32_t getNextCharIndex(const std::string &str, uint32_t index) {
  if (index >= str.size())
    return static_cast<uint32_t>(str.size());
  return index + getUtf8CharLength(static_cast<unsigned char>(str[index]));
}

uint32_t getPreviousWordIndex(const std::string &str, uint32_t index) {
  if (index == 0)
    return 0;
  uint32_t idx = index;
  while (idx > 0) {
    uint32_t prev = getPreviousCharIndex(str, idx);
    char c = str[prev];
    if (c != ' ' && c != '\t' && c != ',' && c != '.')
      break;
    idx = prev;
  }
  while (idx > 0) {
    uint32_t prev = getPreviousCharIndex(str, idx);
    char c = str[prev];
    if (c == ' ' || c == '\t' || c == ',' || c == '.')
      return idx;
    idx = prev;
  }
  return 0;
}

uint32_t getNextWordIndex(const std::string &str, uint32_t index) {
  uint32_t len = static_cast<uint32_t>(str.size());
  if (index >= len)
    return len;
  uint32_t idx = index;
  while (idx < len) {
    char c = str[idx];
    if (c == ' ' || c == '\t' || c == ',' || c == '.')
      break;
    idx = getNextCharIndex(str, idx);
  }
  while (idx < len) {
    char c = str[idx];
    if (c != ' ' && c != '\t' && c != ',' && c != '.')
      return idx;
    idx = getNextCharIndex(str, idx);
  }
  return len;
}

uint32_t getWordEndIndex(const std::string &str, uint32_t index) {
  uint32_t len = static_cast<uint32_t>(str.size());
  if (index >= len)
    return len;

  uint32_t idx = index;
  while (idx < len) {
    char c = str[idx];
    if (c == ' ' || c == '\t' || c == ',' || c == '.')
      break;
    idx = getNextCharIndex(str, idx);
  }
  return idx;
}

size_t getCodepointCount(const std::string &str) {
  size_t count = 0;
  uint32_t idx = 0;
  while (idx < str.size()) {
    idx = getNextCharIndex(str, idx);
    count++;
  }
  return count;
}

void appendUtf8(std::string &str, uint32_t codepoint, uint32_t &cursorBytePos) {
  std::string temp;
  if (codepoint < 0x80) {
    temp.push_back(static_cast<char>(codepoint));
  } else if (codepoint < 0x800) {
    temp.push_back(static_cast<char>((codepoint >> 6) | 0xC0));
    temp.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
  } else if (codepoint < 0x10000) {
    temp.push_back(static_cast<char>((codepoint >> 12) | 0xE0));
    temp.push_back(static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80));
    temp.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
  } else if (codepoint < 0x110000) {
    temp.push_back(static_cast<char>((codepoint >> 18) | 0xF0));
    temp.push_back(static_cast<char>(((codepoint >> 12) & 0x3F) | 0x80));
    temp.push_back(static_cast<char>(((codepoint >> 6) & 0x3F) | 0x80));
    temp.push_back(static_cast<char>((codepoint & 0x3F) | 0x80));
  }
  str.insert(cursorBytePos, temp);
  cursorBytePos += static_cast<uint32_t>(temp.size());
}

std::vector<uint32_t> decodeUtf8String(VeraStringView str) {
  std::vector<uint32_t> codepoints;

  // Early exit if the view is empty using your new helper
  if (str.isEmpty()) {
    return codepoints;
  }

  uint32_t idx = 0;
  while (idx < str.length) {
    uint32_t cp = 0;
    // Read from the raw character pointer array safely
    unsigned char c = static_cast<unsigned char>(str.data[idx]);
    int len = getUtf8CharLength(c);

    // Safeguard the parsing index bounds against the view's length limit
    if (len == 1) {
      cp = c;
    } else if (len == 2 && idx + 1 < str.length) {
      cp = ((c & 0x1F) << 6) |
           (static_cast<unsigned char>(str.data[idx + 1]) & 0x3F);
    } else if (len == 3 && idx + 2 < str.length) {
      cp = ((c & 0x0F) << 12) |
           ((static_cast<unsigned char>(str.data[idx + 1]) & 0x3F) << 6) |
           (static_cast<unsigned char>(str.data[idx + 2]) & 0x3F);
    } else if (len == 4 && idx + 3 < str.length) {
      cp = ((c & 0x07) << 18) |
           ((static_cast<unsigned char>(str.data[idx + 1]) & 0x3F) << 12) |
           ((static_cast<unsigned char>(str.data[idx + 2]) & 0x3F) << 6) |
           (static_cast<unsigned char>(str.data[idx + 3]) & 0x3F);
    }

    if (cp != 0) {
      codepoints.push_back(cp);
    }

    // Prevent infinite loops if getUtf8CharLength returns <= 0 on malformed
    // input
    idx += (len > 0) ? len : 1;
  }
  return codepoints;
}

bool validateChar(uint32_t codepoint, const std::string &text, uint32_t pos,
                  const atomic::TextConfig &config) {
  using namespace atomic;
  if (config.maxLength > 0 && getCodepointCount(text) >= config.maxLength) {
    return false;
  }

  switch (config.type) {
  case TextInputType::NumberOnly:
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '.' ||
           codepoint == '-';
  case TextInputType::AlphaOnly:
    return (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= 'a' && codepoint <= 'z');
  case TextInputType::Alphanumeric:
    return (codepoint >= '0' && codepoint <= '9') ||
           (codepoint >= 'A' && codepoint <= 'Z') ||
           (codepoint >= 'a' && codepoint <= 'z');
  case TextInputType::Custom:
    return config.customFilter ? config.customFilter(codepoint, text, pos)
                               : true;
  case TextInputType::Text:
  default:
    return true;
  }
}

float getSubstringAdvance(const std::string &str, avk::Font *font,
                          float physicalFontSize, float customGap) {
  if (str.empty() || !font)
    return 0.0f;
  float baseW = font->measureText(str, physicalFontSize).x;
  size_t count = getCodepointCount(str);
  return baseW + (static_cast<float>(count) * customGap);
}

uint32_t findWhereCursorLanded(const std::string &displayString,
                               avk::Font *font, float relativeMouseX,
                               float physicalFontSize, float customGap) {
  if (displayString.empty() || !font || relativeMouseX <= 0.0f) {
    return 0;
  }
  float prevWidth = 0.0f;
  for (uint32_t i = 0; i < displayString.size();
       i = getNextCharIndex(displayString, i)) {
    uint32_t nextI = getNextCharIndex(displayString, i);

    float currentWidth = getSubstringAdvance(displayString.substr(0, nextI),
                                             font, physicalFontSize, customGap);

    float midPoint = (prevWidth + currentWidth) * 0.5f;
    if (relativeMouseX < midPoint) {
      return i;
    }
    prevWidth = currentWidth;
  }
  return static_cast<uint32_t>(displayString.size());
}

} // namespace
// textinput implementation
Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder,
                      const TextConfig &config) {
  auto *uiState = getUiState();
  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};

  const auto &rawStyle = modifier.getStyle();

  std::string labelId = rawStyle.elementLabel.value_or("TextInput");
  Clay_ElementId textInputId = utils::layout::getNextId(labelId.c_str());
  uint32_t elementId = textInputId.id;

  std::string selectBoxId = labelId + "_select";
  std::string caretLineId = labelId + "_caret";
  std::string dropCaretId = labelId + "_dropCaret";

  bool isDisabled = rawStyle.disabled.value_or(false) || inherited.disabled;
  bool isFocused = (!isDisabled && uiState->focusedElementId == elementId);
  bool wasHovered = !isDisabled && isHovered(elementId);

  glm::vec4 baseBg = rawStyle.backgroundColor.value_or("#ffffff"_hex);
  glm::vec4 baseStrokeColor =
      rawStyle.strokeColor.value_or(DEFAULT_BORDER_NORMAL);
  glm::vec4 hoverBg =
      glm::vec4(baseBg.r * 0.98f, baseBg.g * 0.98f, baseBg.b * 0.98f, baseBg.a);
  glm::vec4 focusBg =
      glm::vec4(baseBg.r * 0.95f, baseBg.g * 0.95f, baseBg.b * 0.95f, baseBg.a);
  glm::vec4 computedBg = isFocused ? focusBg : (wasHovered ? hoverBg : baseBg);
  glm::vec4 baseStrokeWidth =
      rawStyle.strokeThickness.value_or(glm::vec4(DEFAULT_BORDER_WIDTH));

  glm::vec4 activeBorderWidth = isFocused ? glm::vec4(1.5f) : baseStrokeWidth;

  constexpr float BASE_UI_SCALE = 2.0f;
  float monitorDpi =
      (getVeraApp() && getVeraApp()->getPrimaryMonitor().dpiScale > 0.0f)
          ? getVeraApp()->getPrimaryMonitor().dpiScale
          : 1.0f;
  float effectiveScale = monitorDpi * BASE_UI_SCALE;

  float customGapPhysical = config.customCharAdvance * effectiveScale;

  // Set default height to 42px to prevent container collapse when using
  // absolute custom renderers
  float defaultContainerHeight = rawStyle.height.value_or(42.0f);

  Modifier containerStyle = std::move(modifier)
                                .id(labelId)
                                .height(defaultContainerHeight) // Lock height
                                .background(computedBg)
                                .border(baseStrokeColor, activeBorderWidth)
                                .disabled(isDisabled)
                                .relative()
                                .row();

  if (!rawStyle.fontWeight.has_value())
    containerStyle = std::move(containerStyle).fontWeight(400.0f);
  if (!rawStyle.padLeft.has_value())
    containerStyle = std::move(containerStyle).padding(10, 8);
  if (!rawStyle.borderRadius.has_value())
    containerStyle = std::move(containerStyle).rounded(10.0f);
  if (!rawStyle.textColor.has_value())
    containerStyle = std::move(containerStyle).color(Colors::black[900]);

  if (!rawStyle.transitionSpec.has_value()) {
    containerStyle = std::move(containerStyle)
                         .transition(0.2f, motion::AnimationCurve::EaseOut());
  }

  const auto &finalStyle = containerStyle.getStyle();

  float logicalFontSize = finalStyle.fontSize.value_or(14.0f);
  float physicalFontSize = logicalFontSize * effectiveScale;

  float fontWeight = finalStyle.fontWeight.value_or(300.0f);
  glm::vec4 textColor = finalStyle.textColor.value_or(Colors::black[900]);

  uint16_t padL = finalStyle.padLeft.value_or(12);
  uint16_t padR = finalStyle.padRight.value_or(12);

  auto fontId = getActiveFont();
  avk::Font *font = getFont(fontId != 0 ? fontId : getDefaultFontId());

  auto &inputState = uiState->inputStateMap[elementId];

  if (!isFocused) {
    inputState.selectionStart = 0;
    inputState.selectionEnd = 0;
    inputState.selectionAnchor = 0;
    inputState.isDraggingText = false;
    inputState.isPotentialTextDrag = false;
    inputState.wasArmedByDoubleClick = false;
    inputState.isDraggingSelectedText = false;
  }

  std::string displayString = textBuffer;
  if (config.isPassword && !textBuffer.empty()) {
    displayString.clear();
    size_t count = getCodepointCount(textBuffer);
    for (size_t i = 0; i < count; ++i) {
      displayString += "•";
    }
  }

  auto resetSelection = [&]() {
    inputState.selectionStart = 0;
    inputState.selectionEnd = 0;
    uiState->doingShiftSelect = false;
    uiState->selectAll = false;
  };

  auto deleteSelection = [&]() -> bool {
    if (inputState.selectionStart != inputState.selectionEnd) {
      uint32_t start = inputState.selectionStart;
      uint32_t len = inputState.selectionEnd - inputState.selectionStart;
      textBuffer.erase(start, len);
      inputState.cursorPosition = start;
      resetSelection();
      return true;
    }
    return false;
  };

  Interaction result = Div(std::move(containerStyle), [&]() {
    bool isBoxHovered = Clay_Hovered();
    if (isBoxHovered && !isDisabled) {
      uiState->anyInputBoxHovered = true;
    }

    ComputedLayout bounds = utils::layout::getComputedLayout(textInputId);
    float visibleWidth =
        bounds.found ? std::max(0.0f, bounds.width() - padL - padR) : 200.0f;

    // Single-line text input scroll tracking with custom gap support
    if (font) {
      float totalTextW = getSubstringAdvance(
          displayString, font, physicalFontSize, customGapPhysical);
      float rawCaretX = getSubstringAdvance(
          displayString.substr(0, inputState.cursorPosition), font,
          physicalFontSize, customGapPhysical);

      if (rawCaretX - inputState.scrollX > visibleWidth) {
        inputState.scrollX = rawCaretX - visibleWidth;
      } else if (rawCaretX - inputState.scrollX < 0.0f) {
        inputState.scrollX = rawCaretX;
      }

      float maxScroll = std::max(0.0f, totalTextW - visibleWidth);
      inputState.scrollX = std::clamp(inputState.scrollX, 0.0f, maxScroll);
    }

    if (bounds.found && !isDisabled) {
      float relativeMouseX =
          (uiState->pointerPos.x - (bounds.x() + padL)) + inputState.scrollX;

      if (isBoxHovered && uiState->pointerPressed) {
        uint32_t landedPos =
            findWhereCursorLanded(displayString, font, relativeMouseX,
                                  physicalFontSize, customGapPhysical);

        uiState->focusedElementId = elementId;
        isFocused = true;

        auto now = std::chrono::high_resolution_clock::now();
        float timeSinceLastClick =
            std::chrono::duration<float>(now - inputState.lastClickTime)
                .count();
        float clickDist =
            glm::length(uiState->pointerPos - inputState.lastClickPos);

        bool isDoubleClick =
            (timeSinceLastClick < 0.48f) && (clickDist < 22.0f);
        inputState.lastClickTime = now;
        inputState.lastClickPos = uiState->pointerPos;

        if (isDoubleClick) {
          uint32_t wStart = getPreviousWordIndex(displayString, landedPos);
          uint32_t wEnd = getWordEndIndex(displayString, landedPos);

          inputState.selectionStart = wStart;
          inputState.selectionEnd = wEnd;
          inputState.selectionAnchor = wStart;
          inputState.cursorPosition = wEnd;
        } else if (inputState.selectionStart != inputState.selectionEnd &&
                   landedPos >= inputState.selectionStart &&
                   landedPos <= inputState.selectionEnd) {
          inputState.isPotentialTextDrag = true;
          inputState.wasArmedByDoubleClick = false;
          inputState.dragStartMousePos = uiState->pointerPos;
        } else {
          inputState.selectionAnchor = landedPos;
          inputState.cursorPosition = landedPos;
          inputState.selectionStart = landedPos;
          inputState.selectionEnd = landedPos;
          inputState.isDraggingText = true;
          inputState.dragStartMousePos = uiState->pointerPos;
          uiState->selectAll = false;
          uiState->doingShiftSelect = false;
        }
      }

      if (isFocused && uiState->pointerDown) {
        float totalTextWidth =
            font ? getSubstringAdvance(displayString, font, physicalFontSize,
                                       customGapPhysical)
                 : 0.0f;
        float clampedMouseX = std::clamp(relativeMouseX, 0.0f, totalTextWidth);

        if (inputState.isPotentialTextDrag) {
          float mouseDist =
              glm::length(uiState->pointerPos - inputState.dragStartMousePos);
          if (mouseDist > 4.0f) {
            inputState.isDraggingSelectedText = true;
            inputState.isPotentialTextDrag = false;

            if (uiState) {
              uiState->activeDragText = textBuffer.substr(
                  inputState.selectionStart,
                  inputState.selectionEnd - inputState.selectionStart);
              uiState->dragSourceElementId = elementId;
            }
          }
        }

        if (inputState.isDraggingText) {
          uint32_t currentLandedPos =
              findWhereCursorLanded(displayString, font, clampedMouseX,
                                    physicalFontSize, customGapPhysical);
          inputState.cursorPosition = currentLandedPos;
          inputState.selectionStart =
              std::min(inputState.selectionAnchor, currentLandedPos);
          inputState.selectionEnd =
              std::max(inputState.selectionAnchor, currentLandedPos);
        }
      }
    }

    if (!uiState->pointerDown) {
      if (inputState.isPotentialTextDrag) {
        inputState.isPotentialTextDrag = false;

        float relativeMouseX =
            bounds.found ? ((uiState->pointerPos.x - (bounds.x() + padL)) +
                            inputState.scrollX)
                         : 0.0f;
        uint32_t landedPos =
            findWhereCursorLanded(displayString, font, relativeMouseX,
                                  physicalFontSize, customGapPhysical);

        uiState->focusedElementId = elementId;
        isFocused = true;
        inputState.cursorPosition = landedPos;
        inputState.selectionAnchor = landedPos;
        inputState.selectionStart = landedPos;
        inputState.selectionEnd = landedPos;
        resetSelection();
      }

      if (uiState && !uiState->activeDragText.empty() && bounds.found) {
        bool isOverThisInput =
            (uiState->pointerPos.x >= bounds.x() &&
             uiState->pointerPos.x <= bounds.x() + bounds.width() &&
             uiState->pointerPos.y >= bounds.y() &&
             uiState->pointerPos.y <= bounds.y() + bounds.height());

        if (isOverThisInput) {
          float relativeMouseX = (uiState->pointerPos.x - (bounds.x() + padL)) +
                                 inputState.scrollX;
          float totalTextWidth =
              font ? getSubstringAdvance(displayString, font, physicalFontSize,
                                         customGapPhysical)
                   : 0.0f;
          float clampedMouseX =
              std::clamp(relativeMouseX, 0.0f, totalTextWidth);
          uint32_t dropPos =
              findWhereCursorLanded(displayString, font, clampedMouseX,
                                    physicalFontSize, customGapPhysical);

          std::string draggedSlice = uiState->activeDragText;

          if (uiState->dragSourceElementId == elementId) {
            if (dropPos < inputState.selectionStart ||
                dropPos > inputState.selectionEnd) {
              textBuffer.erase(inputState.selectionStart, draggedSlice.size());
              uint32_t targetIdx =
                  (dropPos > inputState.selectionEnd)
                      ? (dropPos - static_cast<uint32_t>(draggedSlice.size()))
                      : dropPos;
              textBuffer.insert(targetIdx, draggedSlice);
              inputState.cursorPosition =
                  targetIdx + static_cast<uint32_t>(draggedSlice.size());
              inputState.selectionStart = targetIdx;
              inputState.selectionEnd = inputState.cursorPosition;
            }
          } else {
            textBuffer.insert(dropPos, draggedSlice);
            inputState.cursorPosition =
                dropPos + static_cast<uint32_t>(draggedSlice.size());
            uiState->focusedElementId = elementId;
            isFocused = true;
          }

          uiState->activeDragText.clear();
          uiState->dragSourceElementId = 0;
        }
      }

      inputState.isDraggingText = false;
      inputState.isDraggingSelectedText = false;
    }

    if (isFocused) {
      auto *app = getVeraApp();

      if (uiState->ctrlPressed && app) {
        if (uiState->copyTriggered &&
            inputState.selectionStart != inputState.selectionEnd) {
          std::string selectedText = textBuffer.substr(
              inputState.selectionStart,
              inputState.selectionEnd - inputState.selectionStart);
          app->setClipboardText(selectedText);
        }

        if (uiState->cutTriggered &&
            inputState.selectionStart != inputState.selectionEnd) {
          std::string selectedText = textBuffer.substr(
              inputState.selectionStart,
              inputState.selectionEnd - inputState.selectionStart);
          app->setClipboardText(selectedText);
          deleteSelection();
        }

        bool pasteTriggered = false;
        for (auto it = uiState->capturedChars.begin();
             it != uiState->capturedChars.end();) {
          if (*it == 'v' || *it == 'V' || *it == 22) {
            pasteTriggered = true;
            it = uiState->capturedChars.erase(it);
          } else {
            ++it;
          }
        }

        if (pasteTriggered) {
          deleteSelection();
          VeraStringView clipboardText = app->getClipboardText();
          std::vector<uint32_t> codepoints = decodeUtf8String(clipboardText);
          for (uint32_t cp : codepoints) {
            if (validateChar(cp, textBuffer, inputState.cursorPosition,
                             config)) {
              appendUtf8(textBuffer, cp, inputState.cursorPosition);
            }
          }
        }
      }

      if (uiState->selectAll) {
        inputState.selectionStart = 0;
        inputState.selectionEnd = static_cast<uint32_t>(textBuffer.size());
        inputState.cursorPosition = static_cast<uint32_t>(textBuffer.size());
        uiState->doingShiftSelect = false;
        uiState->selectAll = false;
      }

      if (uiState->backspacePressed) {
        if (!deleteSelection() && inputState.cursorPosition > 0) {
          uint32_t prev =
              uiState->ctrlPressed
                  ? getPreviousWordIndex(textBuffer, inputState.cursorPosition)
                  : getPreviousCharIndex(textBuffer, inputState.cursorPosition);
          uint32_t len = inputState.cursorPosition - prev;
          textBuffer.erase(prev, len);
          inputState.cursorPosition = prev;
        }
      }

      if (uiState->deletePressed) {
        if (!deleteSelection() &&
            inputState.cursorPosition < textBuffer.size()) {
          uint32_t next =
              getNextCharIndex(textBuffer, inputState.cursorPosition);
          textBuffer.erase(inputState.cursorPosition,
                           next - inputState.cursorPosition);
        }
      }

      auto moveCursorWithSelection = [&](uint32_t newCursor) {
        if (uiState->shiftPressed) {
          uint32_t anchor = inputState.cursorPosition;
          if (uiState->doingShiftSelect) {
            anchor = (inputState.cursorPosition == inputState.selectionStart)
                         ? inputState.selectionEnd
                         : inputState.selectionStart;
          } else {
            uiState->doingShiftSelect = true;
          }
          inputState.cursorPosition = newCursor;
          inputState.selectionStart =
              std::min(anchor, inputState.cursorPosition);
          inputState.selectionEnd = std::max(anchor, inputState.cursorPosition);
        } else {
          if (inputState.selectionStart != inputState.selectionEnd &&
              !uiState->ctrlPressed) {
            inputState.cursorPosition = (newCursor < inputState.cursorPosition)
                                            ? inputState.selectionStart
                                            : inputState.selectionEnd;
          } else {
            inputState.cursorPosition = newCursor;
          }
          resetSelection();
        }
      };

      if (uiState->leftArrowPressed) {
        uint32_t nextPos =
            uiState->ctrlPressed
                ? getPreviousWordIndex(textBuffer, inputState.cursorPosition)
                : getPreviousCharIndex(textBuffer, inputState.cursorPosition);
        moveCursorWithSelection(nextPos);
      }

      if (uiState->rightArrowPressed) {
        uint32_t nextPos =
            uiState->ctrlPressed
                ? getNextWordIndex(textBuffer, inputState.cursorPosition)
                : getNextCharIndex(textBuffer, inputState.cursorPosition);
        moveCursorWithSelection(nextPos);
      }

      if (!uiState->capturedChars.empty()) {
        deleteSelection();
        for (uint32_t codepoint : uiState->capturedChars) {
          if (validateChar(codepoint, textBuffer, inputState.cursorPosition,
                           config)) {
            appendUtf8(textBuffer, codepoint, inputState.cursorPosition);
          }
        }
      }
    }

    float textboxHeight = bounds.found
                              ? bounds.height()
                              : finalStyle.height.value_or(DEFAULT_HEIGHT);

    displayString = textBuffer;
    if (config.isPassword && !textBuffer.empty()) {
      displayString.clear();
      size_t count = getCodepointCount(textBuffer);
      for (size_t i = 0; i < count; ++i) {
        displayString += "•";
      }
    }

    float measuredLineH =
        font ? font->getLineHeight(physicalFontSize) : physicalFontSize;

    // 1. PRESENTATION / CUSTOM RENDER HOOK OVERRIDE (Rendered FIRST)
    if (config.customRenderer && font && !displayString.empty()) {
      float localX = padL - inputState.scrollX;
      float localY = (textboxHeight - (measuredLineH / effectiveScale)) * 0.5f;
      config.customRenderer(displayString, localX, localY, logicalFontSize,
                            font, textColor);
    } else {
      std::string textToRender =
          textBuffer.empty() ? placeholder : displayString;
      glm::vec4 finalTextColor = textBuffer.empty() ? "#737373"_hex : textColor;

      Text(textToRender, Modifier()
                             .color(finalTextColor)
                             .fontSize(logicalFontSize)
                             .fontWeight(fontWeight)
                             .translate(-inputState.scrollX, 0.0f));
    }

    // 2. RENDER SELECTION BOX SECOND (On Top with Opacity)
    if (isFocused && (inputState.selectionStart != inputState.selectionEnd) &&
        font) {
      float startX = (getSubstringAdvance(
                          displayString.substr(0, inputState.selectionStart),
                          font, physicalFontSize, customGapPhysical) +
                      padL) -
                     inputState.scrollX;
      float endX =
          (getSubstringAdvance(displayString.substr(0, inputState.selectionEnd),
                               font, physicalFontSize, customGapPhysical) +
           padL) -
          inputState.scrollX;
      float selectW = endX - startX;
      float selectH = measuredLineH / 1.5f;
      float selectY = (textboxHeight - selectH) * 0.5f;

      Div(Modifier()
              .id(selectBoxId)
              .absolute()
              .pointerEvents(false)
              .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
              .offset(startX, selectY)
              .size(selectW, selectH)
              .background(glm::vec4(0.2f, 0.5f, 1.0f, 1.0f))
              .opacity(0.35f)); // Rendered on top of text with 35% opacity
    }

    // Render Drop Target Caret when dragging selected text
    if (uiState && !uiState->activeDragText.empty() && bounds.found && font) {
      bool isOverThisInput =
          (uiState->pointerPos.x >= bounds.x() &&
           uiState->pointerPos.x <= bounds.x() + bounds.width() &&
           uiState->pointerPos.y >= bounds.y() &&
           uiState->pointerPos.y <= bounds.y() + bounds.height());
      if (isOverThisInput) {
        float relativeMouseX =
            (uiState->pointerPos.x - (bounds.x() + padL)) + inputState.scrollX;
        float totalTextWidth =
            font ? getSubstringAdvance(displayString, font, physicalFontSize,
                                       customGapPhysical)
                 : 0.0f;
        float clampedMouseX = std::clamp(relativeMouseX, 0.0f, totalTextWidth);
        uint32_t dropPos =
            findWhereCursorLanded(displayString, font, clampedMouseX,
                                  physicalFontSize, customGapPhysical);

        float dropOffset =
            (getSubstringAdvance(displayString.substr(0, dropPos), font,
                                 physicalFontSize, customGapPhysical) +
             padL) -
            inputState.scrollX;
        float dropCaretH = measuredLineH / 1.5f;
        float dropCaretY = (textboxHeight - dropCaretH) * 0.5f;

        Div(Modifier()
                .id(dropCaretId)
                .absolute()
                .pointerEvents(false)
                .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
                .offset(dropOffset, dropCaretY)
                .size(2.0f, dropCaretH)
                .background(textColor));
      }
    }

    // Render Caret Line (Unique ID _caret)
    if (isFocused && font) {
      float cursorOffset = padL - inputState.scrollX;
      if (config.isPassword) {
        size_t numCodepoints =
            getCodepointCount(textBuffer.substr(0, inputState.cursorPosition));
        std::string maskedSub;
        for (size_t i = 0; i < numCodepoints; ++i) {
          maskedSub += "•";
        }
        cursorOffset += getSubstringAdvance(maskedSub, font, physicalFontSize,
                                            customGapPhysical);
      } else {
        cursorOffset += getSubstringAdvance(
            displayString.substr(0, inputState.cursorPosition), font,
            physicalFontSize, customGapPhysical);
      }
      float caretH = measuredLineH / 1.5f;
      float caretY = (textboxHeight - caretH) * 0.5f;

      Div(Modifier()
              .id(caretLineId)
              .absolute()
              .attach(AttachPoint::TopLeft, AttachPoint::TopLeft)
              .offset(cursorOffset, caretY)
              .size(2.0f, caretH)
              .background(textColor));
    }
  });

  // Floating Drag-and-Drop Selection Ghost
  if (uiState && !uiState->activeDragText.empty() && uiState->pointerDown) {
    std::string draggedSlice = uiState->activeDragText;
    std::string ghostId = labelId + "_ghost";

    Div(Modifier()
            .id(ghostId)
            .fixed()
            .left(uiState->pointerPos.x + 25.0f)
            .top(uiState->pointerPos.y + 25.0f),
        [&]() {
          Text(draggedSlice, Modifier().color(textColor).opacity(0.5).fontSize(
                                 logicalFontSize));
        });
  }

  if (isDisabled) {
    result.hovered = false;
    result.pressed = false;
    result.clicked = false;
  }

  if (isFocused && uiState->enterPressed) {
    result.clicked = true;
    uiState->focusedElementId = 0;
  }

  return result;
}

Interaction TextInput(Modifier &&modifier, std::string &textBuffer,
                      const std::string &placeholder) {
  return TextInput(std::move(modifier), textBuffer, placeholder, TextConfig{});
}

// image implementation
Interaction Image(Modifier &&modifier, uint32_t textureIndex,
                  const glm::vec4 &tint) {
  const auto &rawStyle = modifier.getStyle();
  auto *uiState = getUiState();

  Clay_ElementId imageId =
      rawStyle.elementLabel.has_value()
          ? utils::layout::getNextId(rawStyle.elementLabel.value().c_str())
          : utils::layout::getNextId("Image");

  Style style = utils::layout::resolveTransitions(imageId.id, rawStyle);

  bool hasMargin =
      style.marginLeft.has_value() || style.marginRight.has_value() ||
      style.marginTop.has_value() || style.marginBottom.has_value();

  /**
   * @brief Outer margin padding container ID.
   */
  Clay_ElementId outerId = imageId;
  outerId.id += 0x6D417267;

  if (hasMargin) {
    Clay__OpenElementWithId(outerId);

    float ml = style.marginLeft.value_or(0.0f);
    float mr = style.marginRight.value_or(0.0f);
    float mt = style.marginTop.value_or(0.0f);
    float mb = style.marginBottom.value_or(0.0f);

    Clay_ElementDeclaration outerDecl{};

    utils::layout::applyStyleToLayout(outerDecl, style);

    outerDecl.layout.padding = {static_cast<uint16_t>(std::round(ml)),
                                static_cast<uint16_t>(std::round(mr)),
                                static_cast<uint16_t>(std::round(mt)),
                                static_cast<uint16_t>(std::round(mb))};

    outerDecl.backgroundColor = {0, 0, 0, 0};

    Clay__ConfigureOpenElement(outerDecl);
  }

  CascadingStyle inherited =
      uiState ? uiState->getActiveCascadingStyle() : CascadingStyle{};
  float effectiveOpacity =
      inherited.inheritedOpacity * style.opacity.value_or(1.0f);

  glm::vec4 finalTint = tint;
  finalTint.a *= effectiveOpacity;

  Clay__OpenElementWithId(imageId);

  glm::vec4 bg = style.backgroundColor.value_or(glm::vec4(0.0f));
  bg.a *= effectiveOpacity;
  glm::vec4 radius = style.borderRadius.value_or(glm::vec4(0.0f));

  auto *payload = utils::layout::createFramePayload(
      style, std::nullopt, std::nullopt, 0.0f, textureIndex, finalTint);

  Style innerStyle = style;
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
                               : CLAY_SIZING_GROW()}};

  decl.backgroundColor = {bg.r * 255.0f, bg.g * 255.0f, bg.b * 255.0f,
                          bg.a * 255.0f};
  decl.cornerRadius = {radius.x, radius.y, radius.z, radius.w};

  utils::layout::applyStyleToLayout(decl, innerStyle);

  decl.userData = payload;
  decl.image = {.imageData = payload};

  Clay__ConfigureOpenElement(decl);

  bool clayHovered = Clay_Hovered();

  Clay__CloseElement();

  if (hasMargin) {
    Clay__CloseElement();
  }

  bool isHovered = false;
  if (clayHovered) {
    Clay_ElementData elementData = Clay_GetElementData(imageId);
    if (elementData.found) {
      isHovered = utils::ui::isPointerOverRoundedBox(
          uiState->pointerPos, elementData.boundingBox, radius);
    }
  }

  Interaction result{};
  result.hovered = isHovered;
  result.pressed = isHovered && uiState->pointerDown;

  auto pointerState = Clay_GetPointerState();
  if (isHovered &&
      pointerState.state == CLAY_POINTER_DATA_RELEASED_THIS_FRAME) {
    result.clicked = true;
  }

  if (uiState) {
    ElementLifecycleState lifecycle{};
    lifecycle.isMounted = true;
    lifecycle.isHovered = isHovered;
    lifecycle.isPressed = result.pressed;
    uiState->currentLifecycleMap[imageId.id] = lifecycle;

    uiState->computedStyleMap[imageId.id] = uiState->getActiveCascadingStyle();
  }

  return result;
}

} // namespace atomic
