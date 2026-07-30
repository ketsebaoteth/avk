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

/**
 * @brief Generates an element ID. Anonymous primitives receive sequential frame
 * counters, while custom string labels automatically resolve to 100% stable
 * static IDs.
 */
inline Clay_ElementId getNextId(const char *label) {
  if (std::strcmp(label, "Div") == 0 || std::strcmp(label, "Text") == 0 ||
      std::strcmp(label, "Image") == 0 || std::strcmp(label, "BtnAnim") == 0 ||
      std::strcmp(label, "Checkbox") == 0 ||
      std::strcmp(label, "Switch") == 0 ||
      std::strcmp(label, "ScrollView") == 0 ||
      std::strcmp(label, "ToastTrigger") == 0) {

    char buffer[64];
    uint32_t currentId = atomic::getElementIdCounter()++;
    std::snprintf(buffer, sizeof(buffer), "%s_%u", label, currentId);

    return Clay_GetElementId(
        Clay_String{.isStaticallyAllocated = false,
                    .length = static_cast<int32_t>(std::strlen(buffer)),
                    .chars = buffer});
  }

  return Clay_GetElementId(
      Clay_String{.isStaticallyAllocated = false,
                  .length = static_cast<int32_t>(std::strlen(label)),
                  .chars = label});
}

inline Clay_ElementId getNextId(const char *name, uint32_t seed) {
  Clay_String str{false, static_cast<int32_t>(strlen(name)), name};
  return Clay__HashString(str, seed);
}

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

inline void applyStyleToLayout(Clay_ElementDeclaration &decl,
                               const atomic::Style &style) {
  if (style.width.has_value()) {
    float w = style.width.value();
    if (w == 0.0f) {
      decl.layout.sizing.width = CLAY_SIZING_GROW();
    } else {
      decl.layout.sizing.width = CLAY_SIZING_FIXED(w);
    }
  }

  if (style.height.has_value()) {
    float h = style.height.value();
    if (h == 0.0f) {
      decl.layout.sizing.height = CLAY_SIZING_GROW();
    } else {
      decl.layout.sizing.height = CLAY_SIZING_FIXED(h);
    }
  }

  if (style.padLeft.has_value())
    decl.layout.padding.left = style.padLeft.value();
  if (style.padRight.has_value())
    decl.layout.padding.right = style.padRight.value();
  if (style.padTop.has_value())
    decl.layout.padding.top = style.padTop.value();
  if (style.padBottom.has_value())
    decl.layout.padding.bottom = style.padBottom.value();

  if (style.childGap.has_value()) {
    decl.layout.childGap = style.childGap.value();
  }

  atomic::Position pos = style.position.value_or(atomic::Position::Normal);

  if (pos == atomic::Position::Absolute || pos == atomic::Position::Fixed) {
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    Clay_FloatingAttachPointType mainAttach = CLAY_ATTACH_POINT_LEFT_TOP;
    Clay_FloatingAttachPointType parentAttach = CLAY_ATTACH_POINT_LEFT_TOP;

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
    decl.floating.attachPoints = {mainAttach, parentAttach};

    if (pos == atomic::Position::Absolute) {
      if (style.parentId.has_value()) {
        decl.floating.parentId = style.parentId.value();
        decl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;
      } else {
        auto *uiState = atomic::getUiState();
        if (uiState && !uiState->positioningContextStack.empty()) {
          decl.floating.parentId = uiState->positioningContextStack.back();
          decl.floating.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID;
        } else {
          decl.floating.parentId = 0;
          decl.floating.attachTo = CLAY_ATTACH_TO_ROOT;
        }
      }
    } else {
      decl.floating.parentId = 0;
      decl.floating.attachTo = CLAY_ATTACH_TO_ROOT;
    }
  }
}

inline atomic::RenderPayload *createFramePayload(
    const atomic::Style &style, std::optional<float> scale = std::nullopt,
    std::optional<float> rotation = std::nullopt, float textOffset = 0.0f,
    uint32_t textureIndex = 0, const glm::vec4 &tintColor = glm::vec4(1.0f)) {
  auto payload = std::make_unique<atomic::RenderPayload>();

  payload->scale = scale.value_or(style.scale.value_or(1.0f));
  payload->rotation = rotation.value_or(style.rotation.value_or(0.0f));
  payload->blur = style.blur.value_or(0.0f);
  payload->transformOrigin =
      style.transformOrigin.value_or(glm::vec2(0.5f, 0.5f));
  payload->translate = style.translate.value_or(glm::vec2(0.0f, 0.0f));
  payload->textOffset = textOffset;
  payload->boxShadows = style.boxShadows;
  payload->gradient = style.gradient; // <--- Map Gradient Payload!

  payload->textureIndex = textureIndex;
  payload->tintColor = tintColor;
  payload->uvBounds =
      style.uvBounds.value_or(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
  payload->objectFit = style.objectFit.value_or(atomic::ObjectFit::Fill);

  // Map Typography
  payload->fontSize = style.fontSize.value_or(16.0f);
  payload->letterSpacing = style.letterSpacing.value_or(0.0f);
  payload->fontWeight = style.fontWeight.value_or(400.0f);

  auto *ptr = payload.get();
  auto *uiState = atomic::getUiState();
  if (uiState) {
    uiState->framePayloads.push_back(std::move(payload));
  }
  return ptr;
}

struct StyleCascadeGuard {
  bool active = false;

  explicit StyleCascadeGuard(const atomic::Style &style) {
    auto *uiState = atomic::getUiState();
    if (uiState) {
      atomic::CascadingStyle current = uiState->getActiveCascadingStyle();
      bool modified = false;

      if (style.opacity.has_value()) {
        current.inheritedOpacity *= style.opacity.value();
        modified = true;
      }
      if (style.fontSize.has_value()) {
        current.fontSize = style.fontSize.value();
        modified = true;
      }
      if (style.letterSpacing.has_value()) {
        current.letterSpacing = style.letterSpacing.value();
        modified = true;
      }
      if (style.fontWeight.has_value()) {
        current.fontWeight = style.fontWeight.value();
        modified = true;
      }
      if (style.lineHeight.has_value()) {
        current.lineHeight = style.lineHeight.value();
        modified = true;
      }
      if (style.textColor.has_value()) {
        current.textColor = style.textColor.value();
        modified = true;
      }
      if (style.textOffset.has_value()) {
        current.textOffset = style.textOffset.value();
        modified = true;
      }
      if (style.pointerEvents.has_value()) {
        current.pointerEvents =
            current.pointerEvents && style.pointerEvents.value();
        modified = true;
      }

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

inline atomic::ComputedLayout getComputedLayout(const char *label) {
  return getComputedLayout(getNextId(label));
}

inline atomic::ComputedLayout getComputedLayout(uint32_t elementId) {
  return getComputedLayout(Clay_ElementId{.id = elementId});
}

inline glm::vec2 getComputedSize(const char *label) {
  return getComputedLayout(label).size;
}

inline glm::vec2 getComputedPosition(const char *label) {
  return getComputedLayout(label).position;
}

inline atomic::CascadingStyle getCurrentStyle() {
  auto *uiState = atomic::getUiState();
  if (uiState) {
    return uiState->getActiveCascadingStyle();
  }
  return atomic::CascadingStyle{};
}

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

inline atomic::CascadingStyle getComputedStyle(const char *label) {
  return getComputedStyle(getNextId(label).id);
}

inline atomic::Style resolveTransitions(uint32_t elementId,
                                        const atomic::Style &targetStyle) {
  if (!targetStyle.transitionSpec.has_value() ||
      !targetStyle.transitionSpec->enabled) {
    return targetStyle;
  }

  const auto &spec = targetStyle.transitionSpec.value();
  atomic::Style resolved = targetStyle;

  if (targetStyle.backgroundColor.has_value()) {
    resolved.backgroundColor = atomic::AnimateVec4(
        elementId + 0x10000, targetStyle.backgroundColor.value(), spec.duration,
        spec.curve);
  }
  if (targetStyle.textColor.has_value()) {
    resolved.textColor =
        atomic::AnimateVec4(elementId + 0x20000, targetStyle.textColor.value(),
                            spec.duration, spec.curve);
  }
  if (targetStyle.strokeColor.has_value()) {
    resolved.strokeColor = atomic::AnimateVec4(elementId + 0x30000,
                                               targetStyle.strokeColor.value(),
                                               spec.duration, spec.curve);
  }
  if (targetStyle.scale.has_value()) {
    resolved.scale =
        atomic::AnimateFloat(elementId + 0x40000, targetStyle.scale.value(),
                             spec.duration, spec.curve);
  }
  if (targetStyle.opacity.has_value()) {
    resolved.opacity =
        atomic::AnimateFloat(elementId + 0x50000, targetStyle.opacity.value(),
                             spec.duration, spec.curve);
  }
  if (targetStyle.rotation.has_value()) {
    resolved.rotation =
        atomic::AnimateFloat(elementId + 0x60000, targetStyle.rotation.value(),
                             spec.duration, spec.curve);
  }
  if (targetStyle.strokeThickness.has_value()) {
    resolved.strokeThickness = atomic::AnimateVec4(
        elementId + 0x70000, targetStyle.strokeThickness.value(), spec.duration,
        spec.curve);
  }
  if (targetStyle.textOffset.has_value()) {
    resolved.textOffset = atomic::AnimateFloat(elementId + 0x80000,
                                               targetStyle.textOffset.value(),
                                               spec.duration, spec.curve);
  }
  if (targetStyle.blur.has_value()) {
    resolved.blur =
        atomic::AnimateFloat(elementId + 0x90000, targetStyle.blur.value(),
                             spec.duration, spec.curve);
  }
  if (targetStyle.translate.has_value()) {
    float tx =
        atomic::AnimateFloat(elementId + 0xA0000, targetStyle.translate->x,
                             spec.duration, spec.curve);
    float ty =
        atomic::AnimateFloat(elementId + 0xB0000, targetStyle.translate->y,
                             spec.duration, spec.curve);
    resolved.translate = glm::vec2(tx, ty);
  }
  if (targetStyle.offset.has_value()) {
    float ox = atomic::AnimateFloat(elementId + 0xC0000, targetStyle.offset->x,
                                    spec.duration, spec.curve);
    float oy = atomic::AnimateFloat(elementId + 0xD0000, targetStyle.offset->y,
                                    spec.duration, spec.curve);
    resolved.offset = glm::vec2(ox, oy);
  }
  if (targetStyle.width.has_value()) {
    resolved.width =
        atomic::AnimateFloat(elementId + 0xE0000, targetStyle.width.value(),
                             spec.duration, spec.curve);
  }
  if (targetStyle.height.has_value()) {
    resolved.height =
        atomic::AnimateFloat(elementId + 0xF0000, targetStyle.height.value(),
                             spec.duration, spec.curve);
  }
  if (targetStyle.fontSize.has_value()) {
    resolved.fontSize =
        atomic::AnimateFloat(elementId + 0x100000, targetStyle.fontSize.value(),
                             spec.duration, spec.curve);
  }
  if (targetStyle.letterSpacing.has_value()) {
    resolved.letterSpacing = atomic::AnimateFloat(
        elementId + 0x110000, targetStyle.letterSpacing.value(), spec.duration,
        spec.curve);
  }
  if (targetStyle.fontWeight.has_value()) {
    resolved.fontWeight = atomic::AnimateFloat(elementId + 0x120000,
                                               targetStyle.fontWeight.value(),
                                               spec.duration, spec.curve);
  }
  if (targetStyle.lineHeight.has_value()) {
    resolved.lineHeight = atomic::AnimateFloat(elementId + 0x130000,
                                               targetStyle.lineHeight.value(),
                                               spec.duration, spec.curve);
  }

  if (targetStyle.padLeft.has_value()) {
    resolved.padLeft = atomic::AnimateFloat(
        elementId + 0x140000, static_cast<float>(targetStyle.padLeft.value()),
        spec.duration, spec.curve);
  }
  if (targetStyle.padRight.has_value()) {
    resolved.padRight = atomic::AnimateFloat(
        elementId + 0x150000, static_cast<float>(targetStyle.padRight.value()),
        spec.duration, spec.curve);
  }
  if (targetStyle.padTop.has_value()) {
    resolved.padTop = atomic::AnimateFloat(
        elementId + 0x160000, static_cast<float>(targetStyle.padTop.value()),
        spec.duration, spec.curve);
  }
  if (targetStyle.padBottom.has_value()) {
    resolved.padBottom = atomic::AnimateFloat(
        elementId + 0x170000, static_cast<float>(targetStyle.padBottom.value()),
        spec.duration, spec.curve);
  }

  if (!targetStyle.boxShadows.empty()) {
    std::vector<atomic::BoxShadow> animatedShadows;
    for (size_t i = 0; i < targetStyle.boxShadows.size(); ++i) {
      atomic::BoxShadow s = targetStyle.boxShadows[i];

      uint32_t shadowId =
          elementId + 0x200000 + (static_cast<uint32_t>(i) * 0x1000);

      s.color =
          atomic::AnimateVec4(shadowId + 1, s.color, spec.duration, spec.curve);
      s.offset.x = atomic::AnimateFloat(shadowId + 2, s.offset.x, spec.duration,
                                        spec.curve);
      s.offset.y = atomic::AnimateFloat(shadowId + 3, s.offset.y, spec.duration,
                                        spec.curve);
      s.blur =
          atomic::AnimateFloat(shadowId + 4, s.blur, spec.duration, spec.curve);
      s.spread = atomic::AnimateFloat(shadowId + 5, s.spread, spec.duration,
                                      spec.curve);

      animatedShadows.push_back(s);
    }
    resolved.boxShadows = animatedShadows;
  }

  return resolved;
}

} // namespace utils::layout
