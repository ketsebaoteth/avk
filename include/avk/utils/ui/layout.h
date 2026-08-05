#pragma once

#include "clay.h"
#include "ui/layout/computedLayout.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>

namespace atomic {
struct Style;
struct RenderPayload;
struct CascadingStyle;
enum class Position : uint8_t;
enum class AttachPoint : uint8_t;
} // namespace atomic

namespace utils::layout {

/**
 * @brief Generates a Clay element ID. Anonymous primitives receive sequential
 * frame counters.
 */
Clay_ElementId getNextId(const char *label);
Clay_ElementId getNextId(const char *name, uint32_t seed);

/**
 * @brief Returns computed layout metrics (position, size) for a Clay element.
 */
atomic::ComputedLayout getComputedLayout(Clay_ElementId id);
atomic::ComputedLayout getComputedLayout(const char *label);
atomic::ComputedLayout getComputedLayout(uint32_t elementId);

glm::vec2 getComputedSize(const char *label);
glm::vec2 getComputedPosition(const char *label);

/**
 * @brief Handles Clay error logging output.
 */
void handleClayError(Clay_ErrorData error);

/**
 * @brief Guard managing relative and absolute positioning context stacks.
 */
struct PositioningContextGuard {
  bool active = false;

  PositioningContextGuard(uint32_t elementId, atomic::Position positionType);
  ~PositioningContextGuard();

  PositioningContextGuard(const PositioningContextGuard &) = delete;
  PositioningContextGuard &operator=(const PositioningContextGuard &) = delete;
  PositioningContextGuard(PositioningContextGuard &&other) noexcept;
  PositioningContextGuard &operator=(PositioningContextGuard &&other) noexcept;
};

/**
 * @brief Guard managing cascading style property stacks.
 */
struct StyleCascadeGuard {
  bool active = false;

  explicit StyleCascadeGuard(const atomic::Style &style);
  ~StyleCascadeGuard();

  StyleCascadeGuard(const StyleCascadeGuard &) = delete;
  StyleCascadeGuard &operator=(const StyleCascadeGuard &) = delete;
  StyleCascadeGuard(StyleCascadeGuard &&other) noexcept;
  StyleCascadeGuard &operator=(StyleCascadeGuard &&other) noexcept;
};

/**
 * @brief Applies high-level atomic::Style properties to Clay element
 * declarations.
 */
void applyStyleToLayout(Clay_ElementDeclaration &decl,
                        const atomic::Style &style);

/**
 * @brief Constructs a RenderPayload frame descriptor for hardware rendering.
 */
atomic::RenderPayload *createFramePayload(
    const atomic::Style &style, std::optional<float> scale = std::nullopt,
    std::optional<float> rotation = std::nullopt, float textOffset = 0.0f,
    uint32_t textureIndex = 0, const glm::vec4 &tintColor = glm::vec4(1.0f));

atomic::CascadingStyle getCurrentStyle();
atomic::CascadingStyle getComputedStyle(uint32_t elementId);
atomic::CascadingStyle getComputedStyle(const char *label);

/**
 * @brief Evaluates dynamic immediate-mode transitions for element style
 * properties.
 */
atomic::Style resolveTransitions(uint32_t elementId,
                                 const atomic::Style &targetStyle);

} // namespace utils::layout
