#pragma once
#include "glm/glm.hpp"
#include "ui/style/style.h"

namespace atomic {

/**
 * @brief Fluent chaining modifier interface holding layout and visual styles.
 */
class Modifier {
public:
  Modifier() = default;

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

  Modifier rounded(const glm::vec4 &radii) && {
    m_style.borderRadius = radii;
    return std::move(*this);
  }

  Modifier border(const glm::vec4 &color, float thickness) && {
    m_style.strokeColor = color;
    m_style.strokeThickness = thickness;
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

  [[nodiscard]] const Style &getStyle() const { return m_style; }

private:
  Style m_style;
};

inline Modifier DefaultModifier() { return Modifier{}; }

} // namespace atomic
