#pragma once

#include "glm/glm.hpp"
#include "ui/internal/context.h"
#include "ui/motion/AtomicMotion.h"
#include "ui/style/style.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

namespace atomic {

/**
 * @brief Simple 3-state value container for Idle, Hovered, and Pressed
 * interaction targets.
 */
template <typename T> struct MotionState {
  T idle{};
  T hovered{};
  T pressed{};

  [[nodiscard]] constexpr T resolve(bool isHovered,
                                    bool isPressed) const noexcept {
    return isPressed ? pressed : (isHovered ? hovered : idle);
  }
};

/**
 * @brief Fluent chaining modifier interface holding layout and visual styles.
 */
class Modifier {
public:
  Modifier() = default;

  /** @brief Sets an explicit string ID for declarative state queries
   * (isHovered, isPressed). */
  Modifier id(const std::string &label) && {
    m_style.elementLabel = label;
    return std::move(*this);
  }

  // =========================================================================
  // HIGH-LEVEL MOTION SHORTHANDS (Re-uses ID set via .id())
  // =========================================================================

  /** @brief Animates element scale using a single target value. */
  Modifier animateScale(float targetScale, float duration = 0.2f,
                        AnimationCurve curve = AnimationCurve::EaseOut()) && {
    auto *uiState = getUiState();
    if (!uiState)
      return std::move(*this).scale(targetScale);

    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);

    float animatedScale = uiState->motionManager.animate<float>(
        motion::MotionHandle(rawId, "scale"), targetScale, duration, curve);

    return std::move(*this).scale(animatedScale);
  }

  /** @brief Animates element scale across 3 states (Idle, Hovered, Pressed)
   * using stored .id(). */
  Modifier animateScale(const MotionState<float> &state, float duration = 0.2f,
                        AnimationCurve curve = AnimationCurve::EaseOut()) && {
    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);
    bool hov = isHovered(rawId);
    bool prs = isPressed(rawId);

    float target = state.resolve(hov, prs);
    return std::move(*this).animateScale(target, duration, curve);
  }

  /** @brief Animates background color using a single target value. */
  Modifier
  animateBackground(const glm::vec4 &targetBg, float duration = 0.2f,
                    AnimationCurve curve = AnimationCurve::EaseOut()) && {
    auto *uiState = getUiState();
    if (!uiState)
      return std::move(*this).background(targetBg);

    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);

    glm::vec4 animatedBg = uiState->motionManager.animate<glm::vec4>(
        motion::MotionHandle(rawId, "bg"), targetBg, duration, curve);

    return std::move(*this).background(animatedBg);
  }

  /** @brief Animates background color across 3 states (Idle, Hovered, Pressed)
   * using stored .id(). */
  Modifier
  animateBackground(const MotionState<glm::vec4> &state, float duration = 0.2f,
                    AnimationCurve curve = AnimationCurve::EaseOut()) && {
    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);
    bool hov = isHovered(rawId);
    bool prs = isPressed(rawId);

    glm::vec4 target = state.resolve(hov, prs);
    return std::move(*this).animateBackground(target, duration, curve);
  }

  /** @brief Animates border color using a single target value. */
  Modifier animateBorder(const glm::vec4 &targetColor, float thickness = 1.0f,
                         float duration = 0.2f,
                         AnimationCurve curve = AnimationCurve::EaseOut()) && {
    auto *uiState = getUiState();
    if (!uiState)
      return std::move(*this).border(targetColor, thickness);

    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);

    glm::vec4 animatedColor = uiState->motionManager.animate<glm::vec4>(
        motion::MotionHandle(rawId, "border"), targetColor, duration, curve);

    return std::move(*this).border(animatedColor, thickness);
  }

  /** @brief Animates border color across 3 states (Idle, Hovered, Pressed)
   * using stored .id(). */
  Modifier animateBorder(const MotionState<glm::vec4> &state,
                         float thickness = 1.0f, float duration = 0.2f,
                         AnimationCurve curve = AnimationCurve::EaseOut()) && {
    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);
    bool hov = isHovered(rawId);
    bool prs = isPressed(rawId);

    glm::vec4 target = state.resolve(hov, prs);
    return std::move(*this).animateBorder(target, thickness, duration, curve);
  }

  /** @brief Animates GPU translation offset using a single target vector. */
  Modifier
  animateTranslate(const glm::vec2 &targetTranslate, float duration = 0.2f,
                   AnimationCurve curve = AnimationCurve::EaseOut()) && {
    auto *uiState = getUiState();
    if (!uiState)
      return std::move(*this).translate(targetTranslate.x, targetTranslate.y);

    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);

    float tx = uiState->motionManager.animate<float>(
        motion::MotionHandle(rawId, "tx"), targetTranslate.x, duration, curve);
    float ty = uiState->motionManager.animate<float>(
        motion::MotionHandle(rawId, "ty"), targetTranslate.y, duration, curve);

    return std::move(*this).translate(tx, ty);
  }

  /** @brief Animates GPU translation offset across 3 states (Idle, Hovered,
   * Pressed). */
  Modifier
  animateTranslate(const MotionState<glm::vec2> &state, float duration = 0.2f,
                   AnimationCurve curve = AnimationCurve::EaseOut()) && {
    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);
    bool hov = isHovered(rawId);
    bool prs = isPressed(rawId);

    glm::vec2 target = state.resolve(hov, prs);
    return std::move(*this).animateTranslate(target, duration, curve);
  }

  /** @brief Animates opacity multiplier using a single target value. */
  Modifier animateOpacity(float targetOpacity, float duration = 0.2f,
                          AnimationCurve curve = AnimationCurve::EaseOut()) && {
    auto *uiState = getUiState();
    if (!uiState)
      return std::move(*this).opacity(targetOpacity);

    std::string label = m_style.elementLabel.value_or("AnonModifier");
    uint32_t rawId = hashLabel(label);

    float animatedAlpha = uiState->motionManager.animate<float>(
        motion::MotionHandle(rawId, "opacity"), targetOpacity, duration, curve);

    return std::move(*this).opacity(animatedAlpha);
  }

  // =========================================================================
  // STANDARD LAYOUT MODIFIERS
  // =========================================================================

  Modifier margin(float all) {
    m_style.marginLeft = all;
    m_style.marginRight = all;
    m_style.marginTop = all;
    m_style.marginBottom = all;
    return std::move(*this);
  }
  Modifier margin(float horizontal, float vertical) {
    m_style.marginLeft = horizontal;
    m_style.marginRight = horizontal;
    m_style.marginTop = vertical;
    m_style.marginBottom = vertical;
    return std::move(*this);
  }
  Modifier margin(glm::vec4 all) {
    m_style.marginLeft = all.x;
    m_style.marginRight = all.y;
    m_style.marginTop = all.z;
    m_style.marginBottom = all.w;
    return std::move(*this);
  }

  Modifier fontSize(float size) && {
    m_style.fontSize = size;
    return std::move(*this);
  }

  Modifier letterSpacing(float spacing) && {
    m_style.letterSpacing = spacing;
    return std::move(*this);
  }

  Modifier fontWeight(float weight) && {
    m_style.fontWeight = weight;
    return std::move(*this);
  }

  Modifier font(uint32_t fontId) && {
    m_style.fontId = fontId;
    return std::move(*this);
  }

  Modifier lineHeight(float height) && {
    m_style.lineHeight = height;
    return std::move(*this);
  }

  Modifier widthGrow() && {
    m_style.width = 0.0f;
    return std::move(*this);
  }

  Modifier heightGrow() && {
    m_style.height = 0.0f;
    return std::move(*this);
  }

  Modifier grow() && {
    m_style.width = 0.0f;
    m_style.height = 0.0f;
    return std::move(*this);
  }

  Modifier shadow(const glm::vec4 &color, float blurRadius,
                  float offsetX = 0.0f, float offsetY = 4.0f,
                  float spreadRadius = 0.0f, bool inset = false) && {
    m_style.boxShadows.push_back(
        BoxShadow{.offset = glm::vec2(offsetX, offsetY),
                  .blur = blurRadius,
                  .spread = spreadRadius,
                  .color = color,
                  .inset = inset});
    return std::move(*this);
  }

  Modifier shadow(const BoxShadow &shadow) && {
    m_style.boxShadows.push_back(shadow);
    return std::move(*this);
  }

  Modifier insetShadow(const glm::vec4 &color, float blurRadius,
                       float offsetX = 0.0f, float offsetY = 2.0f,
                       float spreadRadius = 0.0f) && {
    return std::move(*this).shadow(color, blurRadius, offsetX, offsetY,
                                   spreadRadius, true);
  }

  Modifier subtleShadow(int level = 1) && {
    level = std::clamp(level, 1, 5);

    switch (level) {
    case 1:
      return std::move(*this)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.04f), 4.0f, 0.0f, 1.0f)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.05f), 12.0f, 0.0f, 4.0f);
    case 2:
      return std::move(*this)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.05f), 6.0f, 0.0f, 2.0f)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.08f), 20.0f, 0.0f, 8.0f);
    case 3:
      return std::move(*this)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.06f), 10.0f, 0.0f, 4.0f)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.10f), 32.0f, 0.0f, 12.0f);
    case 4:
      return std::move(*this)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.08f), 12.0f, 0.0f, 6.0f)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.14f), 48.0f, 0.0f, 20.0f);
    case 5:
    default:
      return std::move(*this)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.10f), 16.0f, 0.0f, 8.0f)
          .shadow(glm::vec4(0.0f, 0.0f, 0.0f, 0.18f), 64.0f, 0.0f, 28.0f);
    }
  }

  Modifier transition(float duration = 0.15f,
                      AnimationCurve curve = AnimationCurve::EaseOut()) && {
    m_style.transitionSpec =
        TransitionSpec{.duration = duration, .curve = curve, .enabled = true};
    return std::move(*this);
  }

  Modifier background(const glm::vec4 &color) && {
    m_style.backgroundColor = color;
    return std::move(*this);
  }

  Modifier color(const glm::vec4 &textColor) && {
    m_style.textColor = textColor;
    return std::move(*this);
  }

  Modifier textColor(const glm::vec4 &textColor) && {
    m_style.textColor = textColor;
    return std::move(*this);
  }

  Modifier textOffset(float y) && {
    m_style.textOffset = y;
    return std::move(*this);
  }

  Modifier opacity(float alpha) && {
    m_style.opacity = alpha;
    return std::move(*this);
  }

  Modifier disabled(bool isDisabled = true) && {
    m_style.disabled = isDisabled;
    return std::move(*this);
  }

  Modifier direction(LayoutDirection dir) && {
    m_style.direction = dir;
    return std::move(*this);
  }

  Modifier row() && {
    m_style.direction = LayoutDirection::Row;
    return std::move(*this);
  }

  Modifier column() && {
    m_style.direction = LayoutDirection::Column;
    return std::move(*this);
  }

  Modifier scale(float s) && {
    m_style.scale = s;
    return std::move(*this);
  }

  Modifier rotation(float r) && {
    m_style.rotation = r;
    return std::move(*this);
  }

  Modifier blur(float b) && {
    m_style.blur = b;
    return std::move(*this);
  }

  Modifier pointerEvents(bool capture) && {
    m_style.pointerEvents = capture;
    return std::move(*this);
  }

  Modifier relative() && {
    m_style.position = Position::Relative;
    return std::move(*this);
  }

  Modifier absolute() && {
    m_style.position = Position::Absolute;
    return std::move(*this);
  }

  Modifier fixed() && {
    m_style.position = Position::Fixed;
    return std::move(*this);
  }

  Modifier parentId(uint32_t id) && {
    m_style.parentId = id;
    return std::move(*this);
  }

  Modifier attach(AttachPoint elementPt, AttachPoint parentPt) && {
    m_style.elementAttach = elementPt;
    m_style.parentAttach = parentPt;
    return std::move(*this);
  }

  Modifier offset(float x, float y) && {
    m_style.offset = glm::vec2(x, y);
    return std::move(*this);
  }

  Modifier offset(const glm::vec2 &off) && {
    m_style.offset = off;
    return std::move(*this);
  }

  Modifier left(float val) && {
    m_style.left = val;
    return std::move(*this);
  }

  Modifier right(float val) && {
    m_style.right = val;
    return std::move(*this);
  }

  Modifier top(float val) && {
    m_style.top = val;
    return std::move(*this);
  }

  Modifier bottom(float val) && {
    m_style.bottom = val;
    return std::move(*this);
  }

  Modifier transformOrigin(float x, float y) && {
    m_style.transformOrigin = glm::vec2(x, y);
    return std::move(*this);
  }

  Modifier translate(float x, float y) && {
    m_style.translate = glm::vec2(x, y);
    return std::move(*this);
  }

  Modifier size(float width, float height) && {
    m_style.width =
        (width != -1.0f) ? std::optional<float>(width) : std::nullopt;
    m_style.height =
        (height != -1.0f) ? std::optional<float>(height) : std::nullopt;
    return std::move(*this);
  }

  Modifier size(float squareSize) && {
    m_style.width =
        (squareSize != -1.0f) ? std::optional<float>(squareSize) : std::nullopt;
    m_style.height =
        (squareSize != -1.0f) ? std::optional<float>(squareSize) : std::nullopt;
    return std::move(*this);
  }

  Modifier width(float width) && {
    m_style.width =
        (width != -1.0f) ? std::optional<float>(width) : std::nullopt;
    return std::move(*this);
  }

  Modifier height(float height) && {
    m_style.height =
        (height != -1.0f) ? std::optional<float>(height) : std::nullopt;
    return std::move(*this);
  }

  Modifier rounded(float radius) && {
    m_style.borderRadius = glm::vec4(radius);
    return std::move(*this);
  }

  Modifier rounded(float topLeft, float topRight, float bottomLeft,
                   float bottomRight) && {
    m_style.borderRadius =
        glm::vec4(topLeft, topRight, bottomLeft, bottomRight);
    return std::move(*this);
  }

  Modifier rounded(const glm::vec4 &radii) && {
    m_style.borderRadius = radii;
    return std::move(*this);
  }

  Modifier border(const glm::vec4 &color, float thickness) && {
    m_style.strokeColor = color;
    m_style.strokeThickness = glm::vec4(thickness);
    return std::move(*this);
  }

  Modifier border(const glm::vec4 &color, glm::vec4 thickness) && {
    m_style.strokeColor = color;
    m_style.strokeThickness = thickness;
    return std::move(*this);
  }

  Modifier borderless() {
    m_style.strokeColor = glm::vec4(0.0f);
    m_style.strokeThickness = glm::vec4(0.0f);
    return std::move(*this);
  }

  Modifier padding(uint16_t all) && {
    m_style.padLeft = all;
    m_style.padRight = all;
    m_style.padTop = all;
    m_style.padBottom = all;
    return std::move(*this);
  }

  Modifier padding(uint16_t horizontal, uint16_t vertical) && {
    m_style.padLeft = horizontal;
    m_style.padRight = horizontal;
    m_style.padTop = vertical;
    m_style.padBottom = vertical;
    return std::move(*this);
  }

  Modifier padding(uint16_t left, uint16_t right, uint16_t top,
                   uint16_t bottom) && {
    m_style.padLeft = left;
    m_style.padRight = right;
    m_style.padTop = top;
    m_style.padBottom = bottom;
    return std::move(*this);
  }

  Modifier gap(uint16_t spacing) && {
    m_style.childGap = spacing;
    return std::move(*this);
  }

  Modifier alignX(AlignmentX alignment) && {
    m_style.alignX = alignment;
    return std::move(*this);
  }

  Modifier alignY(AlignmentY alignment) && {
    m_style.alignY = alignment;
    return std::move(*this);
  }

  Modifier center() && {
    m_style.alignX = AlignmentX::Center;
    m_style.alignY = AlignmentY::Center;
    return std::move(*this);
  }

  Modifier childAlignment(Alignment alignment) && {
    m_style.alignX = alignment.x;
    m_style.alignY = alignment.y;
    return std::move(*this);
  }

  Modifier objectFit(ObjectFit fit) && {
    m_style.objectFit = fit;
    return std::move(*this);
  }

  Modifier cover() && {
    m_style.objectFit = ObjectFit::Cover;
    return std::move(*this);
  }

  Modifier contain() && {
    m_style.objectFit = ObjectFit::Contain;
    return std::move(*this);
  }

  Modifier uv(float minU, float minV, float maxU, float maxV) && {
    m_style.uvBounds = glm::vec4(minU, minV, maxU, maxV);
    return std::move(*this);
  }

  Modifier uv(const glm::vec4 &bounds) && {
    m_style.uvBounds = bounds;
    return std::move(*this);
  }

  Modifier gradient(const Gradient &grad) && {
    m_style.gradient = grad;
    return std::move(*this);
  }

  Modifier linearGradient(float angleDegrees, const glm::vec4 &startColor,
                          const glm::vec4 &endColor,
                          ColorSpace space = ColorSpace::OKLab) && {
    Gradient g;
    g.type = GradientType::Linear;
    g.colorSpace = space;
    g.angleDegrees = angleDegrees;
    g.stops = {{startColor, 0.0f}, {endColor, 1.0f}};
    m_style.gradient = std::move(g);
    return std::move(*this);
  }

  Modifier linearGradient(const glm::vec4 &startColor,
                          const glm::vec4 &endColor,
                          float angleDegrees = 180.0f,
                          ColorSpace space = ColorSpace::OKLab) && {
    return std::move(*this).linearGradient(angleDegrees, startColor, endColor,
                                           space);
  }

  Modifier linearGradient(float angleDegrees,
                          const std::vector<GradientStop> &stops,
                          ColorSpace space = ColorSpace::OKLab) && {
    Gradient g;
    g.type = GradientType::Linear;
    g.colorSpace = space;
    g.angleDegrees = angleDegrees;
    g.stops = stops;
    m_style.gradient = std::move(g);
    return std::move(*this);
  }

  Modifier radialGradient(const glm::vec2 &center, const glm::vec4 &innerColor,
                          const glm::vec4 &outerColor,
                          ColorSpace space = ColorSpace::OKLab) && {
    Gradient g;
    g.type = GradientType::Radial;
    g.colorSpace = space;
    g.center = center;
    g.stops = {{innerColor, 0.0f}, {outerColor, 1.0f}};
    m_style.gradient = std::move(g);
    return std::move(*this);
  }

  Modifier radialGradient(const glm::vec2 &center,
                          const std::vector<GradientStop> &stops,
                          ColorSpace space = ColorSpace::OKLab) && {
    Gradient g;
    g.type = GradientType::Radial;
    g.colorSpace = space;
    g.center = center;
    g.stops = stops;
    m_style.gradient = std::move(g);
    return std::move(*this);
  }

  [[nodiscard]] const Style &getStyle() const { return m_style; }

private:
  Style m_style;
};

inline Modifier DefaultModifier() { return Modifier{}; }

} // namespace atomic
