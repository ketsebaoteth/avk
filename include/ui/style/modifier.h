#pragma once

#include "glm/glm.hpp"
#include "ui/style/style.h"
#include <algorithm>
#include <optional>
#include <utility>

namespace atomic {

/**
 * @brief chaining modifier interface holding layout and
 * visual styles.
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

  /** @brief Sets explicit line height in pixels. */
  Modifier lineHeight(float height) && {
    m_style.lineHeight = height;
    return std::move(*this);
  }
  /** @brief Make width expand to fill all available parent space (flex-grow:
   * 1). */
  Modifier widthGrow() && {
    m_style.width = 0.0f;
    return std::move(*this);
  }

  /** @brief Make height expand to fill all available parent space. */
  Modifier heightGrow() && {
    m_style.height = 0.0f;
    return std::move(*this);
  }

  /** @brief Make both width and height expand to fill parent space. */
  Modifier grow() && {
    m_style.width = 0.0f;
    m_style.height = 0.0f;
    return std::move(*this);
  }
  /** @brief Append a custom Box Shadow (Outset or Inset). */
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

  /** @brief Append a pre-configured BoxShadow struct. */
  Modifier shadow(const BoxShadow &shadow) && {
    m_style.boxShadows.push_back(shadow);
    return std::move(*this);
  }

  /** @brief Append an Inset Inner Shadow. */
  Modifier insetShadow(const glm::vec4 &color, float blurRadius,
                       float offsetX = 0.0f, float offsetY = 2.0f,
                       float spreadRadius = 0.0f) && {
    return std::move(*this).shadow(color, blurRadius, offsetX, offsetY,
                                   spreadRadius, true);
  }

  /** @brief Subtle web-style drop shadow preset (Levels 1 to 5). */
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

  /** @brief Enables implicit CSS-style transition interpolation for changing
   * properties. */
  Modifier transition(float duration = 0.15f,
                      AnimationCurve curve = AnimationCurve::EaseOut()) && {
    m_style.transitionSpec =
        TransitionSpec{.duration = duration, .curve = curve, .enabled = true};
    return std::move(*this);
  }

  /** @brief Set element background color. */
  Modifier background(const glm::vec4 &color) && {
    m_style.backgroundColor = color;
    return std::move(*this);
  }

  /** @brief Set text color. */
  Modifier color(const glm::vec4 &textColor) && {
    m_style.textColor = textColor;
    return std::move(*this);
  }

  /** @brief Alias for text color setting. */
  Modifier textColor(const glm::vec4 &textColor) && {
    m_style.textColor = textColor;
    return std::move(*this);
  }

  /** @brief Set vertical text alignment offset in pixels. */
  Modifier textOffset(float y) && {
    m_style.textOffset = y;
    return std::move(*this);
  }

  /** @brief Set opacity multiplier (0.0 to 1.0). */
  Modifier opacity(float alpha) && {
    m_style.opacity = alpha;
    return std::move(*this);
  }

  /** @brief Set component disabled state. */
  Modifier disabled(bool isDisabled = true) && {
    m_style.disabled = isDisabled;
    return std::move(*this);
  }

  /** @brief Set container layout direction. */
  Modifier direction(LayoutDirection dir) && {
    m_style.direction = dir;
    return std::move(*this);
  }

  /** @brief Set layout direction to horizontal Row. */
  Modifier row() && {
    m_style.direction = LayoutDirection::Row;
    return std::move(*this);
  }

  /** @brief Set layout direction to vertical Column. */
  Modifier column() && {
    m_style.direction = LayoutDirection::Column;
    return std::move(*this);
  }

  /** @brief Set uniform 2D transform scale factor. */
  Modifier scale(float s) && {
    m_style.scale = s;
    return std::move(*this);
  }

  /** @brief Set 2D rotation angle in radians. */
  Modifier rotation(float r) && {
    m_style.rotation = r;
    return std::move(*this);
  }

  /** @brief Set edge blur radius in pixels. */
  Modifier blur(float b) && {
    m_style.blur = b;
    return std::move(*this);
  }

  /** @brief Enable or disable pointer event capture. */
  Modifier pointerEvents(bool capture) && {
    m_style.pointerEvents = capture;
    return std::move(*this);
  }

  /** @brief Set positioning mode to Relative. */
  Modifier relative() && {
    m_style.position = Position::Relative;
    return std::move(*this);
  }

  /** @brief Set positioning mode to Absolute. */
  Modifier absolute() && {
    m_style.position = Position::Absolute;
    return std::move(*this);
  }

  /** @brief Set positioning mode to Fixed (Viewport root). */
  Modifier fixed() && {
    m_style.position = Position::Fixed;
    return std::move(*this);
  }

  /** @brief Explicitly anchor floating element to a target element ID. */
  Modifier parentId(uint32_t id) && {
    m_style.parentId = id;
    return std::move(*this);
  }

  /** @brief Specify floating anchor points (Element attach point -> Parent
   * attach point). */
  Modifier attach(AttachPoint elementPt, AttachPoint parentPt) && {
    m_style.elementAttach = elementPt;
    m_style.parentAttach = parentPt;
    return std::move(*this);
  }

  /** @brief Set post-layout offset (X, Y) in pixels. */
  Modifier offset(float x, float y) && {
    m_style.offset = glm::vec2(x, y);
    return std::move(*this);
  }

  /** @brief Set post-layout offset vector. */
  Modifier offset(const glm::vec2 &off) && {
    m_style.offset = off;
    return std::move(*this);
  }

  /** @brief Set left inset coordinate for absolute positioning. */
  Modifier left(float val) && {
    m_style.left = val;
    return std::move(*this);
  }

  /** @brief Set right inset coordinate for absolute positioning. */
  Modifier right(float val) && {
    m_style.right = val;
    return std::move(*this);
  }

  /** @brief Set top inset coordinate for absolute positioning. */
  Modifier top(float val) && {
    m_style.top = val;
    return std::move(*this);
  }

  /** @brief Set bottom inset coordinate for absolute positioning. */
  Modifier bottom(float val) && {
    m_style.bottom = val;
    return std::move(*this);
  }

  /** @brief Set 2D transform origin pivot point (0.0 to 1.0 normalized). */
  Modifier transformOrigin(float x, float y) && {
    m_style.transformOrigin = glm::vec2(x, y);
    return std::move(*this);
  }

  /** @brief Set post-layout translate offset. */
  Modifier translate(float x, float y) && {
    m_style.translate = glm::vec2(x, y);
    return std::move(*this);
  }

  /** @brief Set width and height dimensions simultaneously. Pass -1 for
   * FIT/GROW fallback. */
  Modifier size(float width, float height) && {
    m_style.width =
        (width != -1.0f) ? std::optional<float>(width) : std::nullopt;
    m_style.height =
        (height != -1.0f) ? std::optional<float>(height) : std::nullopt;
    return std::move(*this);
  }

  /** @brief Set uniform square size for both width and height. */
  Modifier size(float squareSize) && {
    m_style.width =
        (squareSize != -1.0f) ? std::optional<float>(squareSize) : std::nullopt;
    m_style.height =
        (squareSize != -1.0f) ? std::optional<float>(squareSize) : std::nullopt;
    return std::move(*this);
  }

  /** @brief Set explicit width in pixels. Pass -1 for default layout fallback.
   */
  Modifier width(float width) && {
    m_style.width =
        (width != -1.0f) ? std::optional<float>(width) : std::nullopt;
    return std::move(*this);
  }

  /** @brief Set explicit height in pixels. Pass -1 for default layout fallback.
   */
  Modifier height(float height) && {
    m_style.height =
        (height != -1.0f) ? std::optional<float>(height) : std::nullopt;
    return std::move(*this);
  }

  /** @brief Set uniform corner radius on all 4 corners. */
  Modifier rounded(float radius) && {
    m_style.borderRadius = glm::vec4(radius);
    return std::move(*this);
  }

  /** @brief Set individual corner radii (TopLeft, TopRight, BottomLeft,
   * BottomRight). */
  Modifier rounded(float topLeft, float topRight, float bottomLeft,
                   float bottomRight) && {
    m_style.borderRadius =
        glm::vec4(topLeft, topRight, bottomLeft, bottomRight);
    return std::move(*this);
  }

  /** @brief Set corner radius vector directly (x=TL, y=TR, z=BL, w=BR). */
  Modifier rounded(const glm::vec4 &radii) && {
    m_style.borderRadius = radii;
    return std::move(*this);
  }

  /** @brief Set border stroke color and thickness. */
  Modifier border(const glm::vec4 &color, float thickness) && {
    m_style.strokeColor = color;
    m_style.strokeThickness = glm::vec4(thickness);
    return std::move(*this);
  }
  /** @brief Set border stroke color and thickness. */
  Modifier border(const glm::vec4 &color, glm::vec4 thickness) && {
    m_style.strokeColor = color;
    m_style.strokeThickness = thickness;
    return std::move(*this);
  }

  /** @brief overloaded helper to 0 out border quickly */
  Modifier borderless() {
    m_style.strokeColor = glm::vec4(0.0f);
    m_style.strokeThickness = glm::vec4(0.0f);
    return std::move(*this);
  }

  /** @brief Set uniform padding on all 4 sides. */
  Modifier padding(uint16_t all) && {
    m_style.padLeft = all;
    m_style.padRight = all;
    m_style.padTop = all;
    m_style.padBottom = all;
    return std::move(*this);
  }

  /** @brief Set horizontal and vertical padding. */
  Modifier padding(uint16_t horizontal, uint16_t vertical) && {
    m_style.padLeft = horizontal;
    m_style.padRight = horizontal;
    m_style.padTop = vertical;
    m_style.padBottom = vertical;
    return std::move(*this);
  }

  /** @brief Set individual padding for Left, Right, Top, and Bottom. */
  Modifier padding(uint16_t left, uint16_t right, uint16_t top,
                   uint16_t bottom) && {
    m_style.padLeft = left;
    m_style.padRight = right;
    m_style.padTop = top;
    m_style.padBottom = bottom;
    return std::move(*this);
  }

  /** @brief Set gap spacing between child elements. */
  Modifier gap(uint16_t spacing) && {
    m_style.childGap = spacing;
    return std::move(*this);
  }

  /** @brief Set horizontal child alignment. */
  Modifier alignX(AlignmentX alignment) && {
    m_style.alignX = alignment;
    return std::move(*this);
  }

  /** @brief Set vertical child alignment. */
  Modifier alignY(AlignmentY alignment) && {
    m_style.alignY = alignment;
    return std::move(*this);
  }

  /** @brief Center children along both horizontal and vertical axes. */
  Modifier center() && {
    m_style.alignX = AlignmentX::Center;
    m_style.alignY = AlignmentY::Center;
    return std::move(*this);
  }

  /** @brief Set 2D child alignment struct. */
  Modifier childAlignment(Alignment alignment) && {
    m_style.alignX = alignment.x;
    m_style.alignY = alignment.y;
    return std::move(*this);
  }

  /** @brief Set CSS object-fit layout mode. */
  Modifier objectFit(ObjectFit fit) && {
    m_style.objectFit = fit;
    return std::move(*this);
  }

  /** @brief Set object-fit to Cover. */
  Modifier cover() && {
    m_style.objectFit = ObjectFit::Cover;
    return std::move(*this);
  }

  /** @brief Set object-fit to Contain. */
  Modifier contain() && {
    m_style.objectFit = ObjectFit::Contain;
    return std::move(*this);
  }

  /** @brief Custom UV texture bounds (uMin, vMin, uMax, vMax). */
  Modifier uv(float minU, float minV, float maxU, float maxV) && {
    m_style.uvBounds = glm::vec4(minU, minV, maxU, maxV);
    return std::move(*this);
  }

  /** @brief Custom UV texture bounds vector. */
  Modifier uv(const glm::vec4 &bounds) && {
    m_style.uvBounds = bounds;
    return std::move(*this);
  }

  /** @brief Apply a custom Gradient struct. */
  Modifier gradient(const Gradient &grad) && {
    m_style.gradient = grad;
    return std::move(*this);
  }

  /**
   * @brief 2-Color Linear Gradient with Angle in degrees.
   * @param angleDegrees 180.0f = to bottom, 90.0f = to right, 45.0f = top-left
   * to bottom-right.
   */
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

  /** @brief 2-Color Linear Gradient defaulting to top-to-bottom (180deg). */
  Modifier linearGradient(const glm::vec4 &startColor,
                          const glm::vec4 &endColor,
                          float angleDegrees = 180.0f,
                          ColorSpace space = ColorSpace::OKLab) && {
    return std::move(*this).linearGradient(angleDegrees, startColor, endColor,
                                           space);
  }

  /**
   * @brief Multi-stop Linear Gradient with Angle in degrees.
   */
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

  /**
   * @brief 2-Color Radial Gradient with custom focal center (0.5, 0.5 =
   * center).
   */
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

  /**
   * @brief Multi-stop Radial Gradient.
   */
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
