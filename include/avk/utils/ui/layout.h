#pragma once
#include "avk/atomic_ui.h"
#include "clay.h"
#include <cstring>
#include <print>

namespace utils::layout {

inline void handleClayError(Clay_ErrorData error) {
  std::println("[Clay Layout]: {}", error.errorText.chars);
}

atomic::UIState *getUiState();
uint32_t &getElementIdCounter();

inline Clay_ElementId getNextId(const char *label) {
  char buffer[64];

  // Fetch the centralized global reference and post-increment it
  uint32_t currentId = getElementIdCounter()++;
  std::snprintf(buffer, sizeof(buffer), "%s_%u", label, currentId);

  return Clay_GetElementId(
      Clay_String{.isStaticallyAllocated = false,
                  .length = static_cast<int32_t>(std::strlen(buffer)),
                  .chars = buffer});
}

// C++ RAII Scope Guard: Auto-manages active positioning coordinate contexts
struct PositioningContextGuard {
  bool active = false;

  PositioningContextGuard(uint32_t elementId, atomic::Position positionType) {
    if (positionType == atomic::Position::Relative ||
        positionType == atomic::Position::Absolute) {
      auto *uiState = utils::layout::getUiState();
      if (uiState) {
        uiState->positioningContextStack.push_back(elementId);
        active = true;
      }
    }
  }

  ~PositioningContextGuard() {
    if (active) {
      auto *uiState = utils::layout::getUiState();
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

// 2-Argument API: Cleans up component layout setups!
inline void applyStyleToLayout(Clay_ElementDeclaration &decl,
                               const atomic::Style &style) {
  // Apply Sizing overrides if present
  if (style.width.has_value()) {
    decl.layout.sizing.width = CLAY_SIZING_FIXED(style.width.value());
  }
  if (style.height.has_value()) {
    decl.layout.sizing.height = CLAY_SIZING_FIXED(style.height.value());
  }

  // Apply Padding overrides if present
  if (style.padLeft.has_value())
    decl.layout.padding.left = style.padLeft.value();
  if (style.padRight.has_value())
    decl.layout.padding.right = style.padRight.value();
  if (style.padTop.has_value())
    decl.layout.padding.top = style.padTop.value();
  if (style.padBottom.has_value())
    decl.layout.padding.bottom = style.padBottom.value();

  // Child gap
  if (style.childGap.has_value()) {
    decl.layout.childGap = style.childGap.value();
  }

  // Resolve proper CSS positioning
  atomic::Position pos = style.position.value_or(atomic::Position::Normal);

  if (pos == atomic::Position::Absolute || pos == atomic::Position::Fixed) {
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    // CORRECTED TYPE: Added "Type" suffix
    Clay_FloatingAttachPointType mainAttach = CLAY_ATTACH_POINT_LEFT_TOP;
    Clay_FloatingAttachPointType parentAttach = CLAY_ATTACH_POINT_LEFT_TOP;

    // 1. Resolve X Attachment Points and Offset
    if (style.right.has_value()) {
      mainAttach = CLAY_ATTACH_POINT_RIGHT_TOP;
      parentAttach = CLAY_ATTACH_POINT_RIGHT_TOP;
      offsetX = -style.right.value(); // Inward offset from the right edge
    } else {
      mainAttach = CLAY_ATTACH_POINT_LEFT_TOP;
      parentAttach = CLAY_ATTACH_POINT_LEFT_TOP;
      offsetX = style.left.value_or(0.0f);
    }

    // 2. Resolve Y Attachment Points and Offset
    if (style.bottom.has_value()) {
      // Adjust horizontal alignment anchors if already right-aligned
      if (style.right.has_value()) {
        mainAttach = CLAY_ATTACH_POINT_RIGHT_BOTTOM;
        parentAttach = CLAY_ATTACH_POINT_RIGHT_BOTTOM;
      } else {
        mainAttach = CLAY_ATTACH_POINT_LEFT_BOTTOM;
        parentAttach = CLAY_ATTACH_POINT_LEFT_BOTTOM;
      }
      offsetY = -style.bottom.value(); // Inward offset from the bottom edge
    } else {
      offsetY = style.top.value_or(0.0f);
    }

    decl.floating.offset = {offsetX, offsetY};
    decl.floating.pointerCaptureMode =
        style.pointerEvents.value_or(true)
            ? CLAY_POINTER_CAPTURE_MODE_CAPTURE
            : CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
    decl.floating.attachPoints = {mainAttach, parentAttach};

    if (pos == atomic::Position::Absolute) {
      auto *uiState = getUiState();
      if (uiState) {
        auto &stack = uiState->positioningContextStack;
        if (!stack.empty()) {
          // Absolute based on nearest Relative Parent container
          decl.floating.parentId = stack.back();
          decl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;
        } else {
          // Absolute based on Viewport Window
          decl.floating.parentId = 0;
          decl.floating.attachTo = CLAY_ATTACH_TO_ROOT;
        }
      }
    } else {
      // Fixed Position: Anchors strictly relative to viewport root
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

  // Use animated value if provided, otherwise fall back to static style
  payload->scale = scale.value_or(style.scale.value_or(1.0f));
  payload->rotation = rotation.value_or(style.rotation.value_or(0.0f));

  payload->blur = style.blur.value_or(0.0f);
  payload->transformOrigin =
      style.transformOrigin.value_or(glm::vec2(0.5f, 0.5f));
  payload->translate = style.translate.value_or(glm::vec2(0.0f, 0.0f));
  payload->textOffset = textOffset;

  // Image properties
  payload->textureIndex = textureIndex;
  payload->tintColor = tintColor;

  auto *ptr = payload.get();
  auto *uiState = utils::layout::getUiState();
  if (uiState) {
    uiState->framePayloads.push_back(std::move(payload));
  }
  return ptr;
}

} // namespace utils::layout
