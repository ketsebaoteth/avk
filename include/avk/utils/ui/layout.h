#pragma once
#include "clay.h"
#include "ui/internal/context.h"
#include "ui/layout/computedLayout.h"
#include <cstring>
#include <print>

namespace utils::layout {

inline void handleClayError(Clay_ErrorData error) {
  std::println("[Clay Layout]: {}", error.errorText.chars);
}

inline Clay_ElementId getNextId(const char *label) {
  char buffer[64];

  // Fetch the centralized global reference and post-increment it
  uint32_t currentId = atomic::getElementIdCounter()++;
  std::snprintf(buffer, sizeof(buffer), "%s_%u", label, currentId);

  return Clay_GetElementId(
      Clay_String{.isStaticallyAllocated = false,
                  .length = static_cast<int32_t>(std::strlen(buffer)),
                  .chars = buffer});
}

// In utils::layout (wherever getNextId lives)
inline Clay_ElementId getNextId(const char *name, uint32_t seed) {
  Clay_String str{false, static_cast<int32_t>(strlen(name)), name};
  return Clay__HashString(str, seed);
}

// C++ RAII Scope Guard: Auto-manages active positioning coordinate contexts
struct PositioningContextGuard {
  bool active = false;

  PositioningContextGuard(uint32_t elementId, atomic::Position positionType) {
    if (positionType == atomic::Position::Relative ||
        positionType == atomic::Position::Absolute) {
      auto *uiState = atomic::getUiState();
      if (uiState) {
        uiState->positioningContextStack.push_back(elementId);
        active = true;
      }
    }
  }

  ~PositioningContextGuard() {
    if (active) {
      auto *uiState = atomic::getUiState();
      if (uiState) {
        auto &stack = uiState->positioningContextStack;
        if (!stack.empty()) {
          stack.pop_back();
        }
      }
    }
  }

  // Delete copy constructors
  PositioningContextGuard(const PositioningContextGuard &) = delete;
  PositioningContextGuard &operator=(const PositioningContextGuard &) = delete;
  PositioningContextGuard(PositioningContextGuard &&other) noexcept
      : active(other.active) {
    other.active = false;
  }
};

static Clay_FloatingAttachPointType mapAttachPoint(atomic::AttachPoint pt) {
  switch (pt) {
  case atomic::AttachPoint::TopLeft:
    return CLAY_ATTACH_POINT_LEFT_TOP;
  case atomic::AttachPoint::TopCenter:
    return CLAY_ATTACH_POINT_CENTER_TOP;
  case atomic::AttachPoint::TopRight:
    return CLAY_ATTACH_POINT_RIGHT_TOP;
  case atomic::AttachPoint::BottomLeft:
    return CLAY_ATTACH_POINT_LEFT_BOTTOM;
  case atomic::AttachPoint::BottomCenter:
    return CLAY_ATTACH_POINT_CENTER_BOTTOM;
  case atomic::AttachPoint::BottomRight:
    return CLAY_ATTACH_POINT_RIGHT_BOTTOM;
  case atomic::AttachPoint::CenterLeft:
    return CLAY_ATTACH_POINT_LEFT_CENTER;
  case atomic::AttachPoint::CenterRight:
    return CLAY_ATTACH_POINT_RIGHT_CENTER;
  default:
    return CLAY_ATTACH_POINT_CENTER_CENTER;
  }
}

// 2-Argument API: Cleans up component layout setups!
inline void applyStyleToLayout(Clay_ElementDeclaration &decl,
                               const atomic::Style &style) {
  // 1. Apply Sizing overrides if present
  if (style.width.has_value()) {
    decl.layout.sizing.width = CLAY_SIZING_FIXED(style.width.value());
  }
  if (style.height.has_value()) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(style.height.value());
  }

  // 2. Apply Padding overrides if present
  if (style.padLeft.has_value())
    decl.layout.padding.left = style.padLeft.value();
  if (style.padRight.has_value())
    decl.layout.padding.right = style.padRight.value();
  if (style.padTop.has_value())
    decl.layout.padding.top = style.padTop.value();
  if (style.padBottom.has_value())
    decl.layout.padding.bottom = style.padBottom.value();

  // 3. Child gap
  if (style.childGap.has_value()) {
    decl.layout.childGap = style.childGap.value();
  }

  // 4. Resolve CSS & Floating positioning
  atomic::Position pos = style.position.value_or(atomic::Position::Normal);

  if (pos == atomic::Position::Absolute || pos == atomic::Position::Fixed) {
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    Clay_FloatingAttachPointType mainAttach = CLAY_ATTACH_POINT_LEFT_TOP;
    Clay_FloatingAttachPointType parentAttach = CLAY_ATTACH_POINT_LEFT_TOP;

    // 1. Resolve Attach Points
    if (style.elementAttach.has_value() && style.parentAttach.has_value()) {
      mainAttach = mapAttachPoint(style.elementAttach.value());
      parentAttach = mapAttachPoint(style.parentAttach.value());
    } else {
      if (style.right.has_value()) {
        mainAttach = CLAY_ATTACH_POINT_RIGHT_TOP;
        parentAttach = CLAY_ATTACH_POINT_RIGHT_TOP;
        offsetX = -style.right.value();
      } else {
        offsetX = style.left.value_or(0.0f);
      }

      if (style.bottom.has_value()) {
        if (style.right.has_value()) {
          mainAttach = CLAY_ATTACH_POINT_RIGHT_BOTTOM;
          parentAttach = CLAY_ATTACH_POINT_RIGHT_BOTTOM;
        } else {
          mainAttach = CLAY_ATTACH_POINT_LEFT_BOTTOM;
          parentAttach = CLAY_ATTACH_POINT_LEFT_BOTTOM;
        }
        offsetY = -style.bottom.value();
      } else {
        offsetY = style.top.value_or(0.0f);
      }
    }

    // =========================================================================
    // FIX: Accumulate BOTH style.offset AND style.translate here!
    // =========================================================================
    if (style.offset.has_value()) {
      offsetX += style.offset->x;
      offsetY += style.offset->y;
    }
    if (style.translate.has_value()) {
      offsetX += style.translate->x;
      offsetY += style.translate->y;
    }

    decl.floating.offset = {offsetX, offsetY};
    decl.floating.pointerCaptureMode =
        style.pointerEvents.value_or(true)
            ? CLAY_POINTER_CAPTURE_MODE_CAPTURE
            : CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
    decl.floating.attachPoints = {mainAttach,
                                  parentAttach}; // --- PARENT ID RESOLUTION ---
    if (pos == atomic::Position::Absolute) {
      // Priority 1: Explicit parentId set on Modifier (e.g.
      // .parentId(targetId))
      if (style.parentId.has_value()) {
        decl.floating.parentId = style.parentId.value();
        decl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;
      } else {
        // Priority 2: Use nearest Relative Parent from the context stack
        auto *uiState = atomic::getUiState();
        if (uiState && !uiState->positioningContextStack.empty()) {
          decl.floating.parentId = uiState->positioningContextStack.back();
          decl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;
        } else {
          // Priority 3: Fall back to Viewport Window Root
          decl.floating.parentId = 0;
          decl.floating.attachTo = CLAY_ATTACH_TO_ROOT;
        }
      }
    } else {
      // Fixed Position: Anchors strictly relative to viewport root window
      decl.floating.parentId = 0;
      decl.floating.attachTo = CLAY_ATTACH_TO_ROOT;
    }
  }
}

// Memory-safe, self-cleaning payload allocator
inline atomic::RenderPayload *createFramePayload(
    const atomic::Style &style, std::optional<float> scale = std::nullopt,
    std::optional<float> rotation = std::nullopt, float textOffset = 0.0f,
    uint32_t textureIndex = 0, const glm::vec4 &tintColor = glm::vec4(1.0f)) {
  auto payload = std::make_unique<atomic::RenderPayload>();

  // Transform properties
  payload->scale = scale.value_or(style.scale.value_or(1.0f));
  payload->rotation = rotation.value_or(style.rotation.value_or(0.0f));
  payload->blur = style.blur.value_or(0.0f);
  payload->transformOrigin =
      style.transformOrigin.value_or(glm::vec2(0.5f, 0.5f));
  payload->translate = style.translate.value_or(glm::vec2(0.0f, 0.0f));
  payload->textOffset = textOffset;

  // CSS Image & Background properties
  payload->textureIndex = textureIndex;
  payload->tintColor = tintColor;
  payload->uvBounds =
      style.uvBounds.value_or(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
  payload->objectFit = style.objectFit.value_or(atomic::ObjectFit::Fill);

  auto *ptr = payload.get();
  auto *uiState = atomic::getUiState();
  if (uiState) {
    uiState->framePayloads.push_back(std::move(payload));
  }
  return ptr;
}

// ============================================================================
// RAII Style Cascade Guard
// Automatically manages style inheritance lifecycle for containers
// ============================================================================
struct StyleCascadeGuard {
  bool active = false;

  explicit StyleCascadeGuard(const atomic::Style &style) {
    auto *uiState = atomic::getUiState();
    if (uiState) {
      atomic::CascadingStyle current = uiState->getActiveCascadingStyle();
      bool modified = false;

      // 1. Text Color Cascading
      if (style.textColor.has_value()) {
        current.textColor = style.textColor.value();
        modified = true;
      }

      // 2. Text Offset Cascading
      if (style.textOffset.has_value()) {
        current.textOffset = style.textOffset.value();
        modified = true;
      }

      // 3. Pointer Events Cascading (Children inherit false if parent disabled
      // it)
      if (style.pointerEvents.has_value()) {
        current.pointerEvents =
            current.pointerEvents && style.pointerEvents.value();
        modified = true;
      }

      // 4. Push updated cascading scope to engine stack
      if (modified) {
        uiState->cascadingStyleStack.push_back(current);
        active = true;
      }
    }
  }

  ~StyleCascadeGuard() {
    if (active) {
      auto *uiState = atomic::getUiState();
      if (uiState && !uiState->cascadingStyleStack.empty()) {
        uiState->cascadingStyleStack.pop_back();
      }
    }
  }

  StyleCascadeGuard(const StyleCascadeGuard &) = delete;
  StyleCascadeGuard &operator=(const StyleCascadeGuard &) = delete;
  StyleCascadeGuard(StyleCascadeGuard &&other) noexcept : active(other.active) {
    other.active = false;
  }
};

inline atomic::ComputedLayout getComputedLayout(Clay_ElementId id) {
  Clay_ElementData data = Clay_GetElementData(id);
  if (data.found) {
    atomic::ComputedLayout layout{};
    layout.position = glm::vec2(data.boundingBox.x, data.boundingBox.y);
    layout.size = glm::vec2(data.boundingBox.width, data.boundingBox.height);
    layout.found = true;
    return layout;
  }
  return atomic::ComputedLayout{};
}

/// Get computed layout by string label (e.g. "MainSidebar")
inline atomic::ComputedLayout getComputedLayout(const char *label) {
  return getComputedLayout(getNextId(label));
}

/// Get computed layout by raw numeric ID
inline atomic::ComputedLayout getComputedLayout(uint32_t elementId) {
  return getComputedLayout(Clay_ElementId{.id = elementId});
}

/// Helper: Get computed size (Width, Height) in pixels
inline glm::vec2 getComputedSize(const char *label) {
  return getComputedLayout(label).size;
}

/// Helper: Get computed screen position (X, Y) in pixels
inline glm::vec2 getComputedPosition(const char *label) {
  return getComputedLayout(label).position;
}

// ============================================================================
// COMPUTED STYLE API (CSS getComputedStyle)
// ============================================================================

/// Returns the active inherited style in the current layout scope
inline atomic::CascadingStyle getCurrentStyle() {
  auto *uiState = atomic::getUiState();
  if (uiState) {
    return uiState->getActiveCascadingStyle();
  }
  return atomic::CascadingStyle{};
}

/// Returns the final resolved style of any specific element ID
inline atomic::CascadingStyle getComputedStyle(uint32_t elementId) {
  auto *uiState = atomic::getUiState();
  if (uiState) {
    auto it = uiState->computedStyleMap.find(elementId);
    if (it != uiState->computedStyleMap.end()) {
      return it->second;
    }
  }
  return atomic::CascadingStyle{};
}

/// Returns the final resolved style of an element by string label
inline atomic::CascadingStyle getComputedStyle(const char *label) {
  return getComputedStyle(getNextId(label).id);
}
} // namespace utils::layout
