#pragma once

#include "avk/avk_core.h"
#include "avk/avk_font.h"
#include "avk/avk_renderer.h"
#include "avk/window/session.h"
#include "clay.h"

#include "core/app/App.h"
#include "ui/internal/cascadingStyle.h"
#include "ui/internal/payload.h"
#include "ui/motion/AtomicMotion.h"
#include "ui/state/inputState.h"
#include "ui/state/scrollViewState.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class VeraWindow;

namespace atomic {

/**
 * @brief Stable label hashing helper using Clay's ElementId resolver.
 */
inline uint32_t hashLabel(std::string_view label) {
  Clay_String str{.isStaticallyAllocated = false,
                  .length = static_cast<int32_t>(label.size()),
                  .chars = label.data()};
  return Clay_GetElementId(str).id;
}

/**
 * @brief Complete runtime lifecycle and focus state for a single UI element.
 */
struct ElementLifecycleState {
  bool isMounted = false;
  float mountAge = 0.0f;

  bool isHovered = false;
  bool isPressed = false;

  bool isFocused = false;
  bool focusGained = false;
  bool focusLost = false;

  bool isUnmounting = false;
  float unmountProgress = 0.0f;
};

/**
 * @brief Master runtime UI engine context state.
 */
struct UIState {
  std::unique_ptr<avk::VulkanContext> context;
  std::unique_ptr<avk::Renderer> renderer;
  bool injectDevTools = true;

  // Frame Data & Performance Metrics
  float frameTimeMs = 0.0f;
  float fps = 0.0f;
  uint32_t drawCalls = 0;

  void *clayArenaMemory = nullptr;
  std::vector<window::WindowSession> sessions;
  std::vector<std::unique_ptr<RenderPayload>> framePayloads;
  std::vector<uint32_t> positioningContextStack;
  std::vector<CascadingStyle> cascadingStyleStack;
  std::unordered_map<uint32_t, CascadingStyle> computedStyleMap;
  atomic::motion::MotionManager motionManager;

  std::vector<float> textConstraintWidthStack;
  // Lifecycle & interaction state stores (previous frame vs current frame)
  std::unordered_map<uint32_t, ElementLifecycleState> previousLifecycleMap;
  std::unordered_map<uint32_t, ElementLifecycleState> currentLifecycleMap;

  glm::vec2 pointerPos = glm::vec2(0.0f);
  bool pointerPressed = false;
  bool pointerDown = false;
  std::vector<std::unique_ptr<avk::Font>> fonts;
  std::unordered_map<uint32_t, InputState> inputStateMap;
  std::unordered_map<uint32_t, ScrollViewState> scrollViewStates;
  std::string activeDragText = "";
  uint32_t dragSourceElementId = 0;

  uint32_t defaultFontId = 0;
  std::array<uint32_t, 5> defaultIconFontIds;
  std::unordered_map<std::string, uint32_t> iconMap;

  uint32_t focusedElementId = 0;
  uint32_t previousFocusedElementId = 0;

  std::vector<uint32_t> capturedChars;
  bool backspacePressed = false;
  bool enterPressed = false;
  bool anyInputBoxHovered = false;
  float mouseWheelDeltaX = 0.0f;
  float mouseWheelDeltaY = 0.0f;

  bool selectAll = false;
  bool doingShiftSelect = false;

  bool deletePressed = false;
  bool leftArrowPressed = false;
  bool rightArrowPressed = false;
  bool ctrlPressed = false;
  bool shiftPressed = false;
  bool copyTriggered = false;
  bool cutTriggered = false;
  bool pasteTriggered = false;

  [[nodiscard]] CascadingStyle getActiveCascadingStyle() const {
    if (!cascadingStyleStack.empty()) {
      return cascadingStyleStack.back();
    }
    return CascadingStyle{};
  }

  window::WindowSession *findSession(VeraWindow *window) {
    auto result = std::find_if(sessions.begin(), sessions.end(),
                               [window](const window::WindowSession &session) {
                                 return session.window == window;
                               });
    return (result != sessions.end()) ? &(*result) : nullptr;
  }
};

/** @brief Initializes the atomicUI engine context and renderer. */
void initialize(VeraApp &veraAppPtr,
                std::optional<VeraNativeHandle> nativeDisplay,
                bool enableValidation);

/** @brief Shuts down the engine and releases all Vulkan resources. */
void shutdown();

/** @brief Maps a native Vera window session into the UI context. */
void registerWindow(VeraWindow *window);

/** @brief Unregisters and destroys a window session context. */
void unregisterWindow(VeraWindow *window);

void resetGlobalIdCounter();
UIState *getUiState();
VeraApp *getVeraApp();
uint32_t &getElementIdCounter();

/*
 * @breif disable atomic dev tools
 * */
inline void disableDevTools() { getUiState()->injectDevTools = false; }

/** @brief Returns true if an element was rendered in previous frame. */
inline bool isMounted(uint32_t elementId) {
  auto *uiState = getUiState();
  if (uiState) {
    auto it = uiState->previousLifecycleMap.find(elementId);
    return (it != uiState->previousLifecycleMap.end()) && it->second.isMounted;
  }
  return false;
}
inline bool isMounted(const std::string &label) {
  return isMounted(hashLabel(label));
}

/** @brief Returns true if an element was hovered in previous frame. */
inline bool isHovered(uint32_t elementId) {
  auto *uiState = getUiState();
  if (uiState) {
    auto it = uiState->previousLifecycleMap.find(elementId);
    return (it != uiState->previousLifecycleMap.end()) && it->second.isHovered;
  }
  return false;
}
inline bool isHovered(const std::string &label) {
  return isHovered(hashLabel(label));
}

/** @brief Returns true if an element was pressed in previous frame. */
inline bool isPressed(uint32_t elementId) {
  auto *uiState = getUiState();
  if (uiState) {
    auto it = uiState->previousLifecycleMap.find(elementId);
    return (it != uiState->previousLifecycleMap.end()) && it->second.isPressed;
  }
  return false;
}
inline bool isPressed(const std::string &label) {
  return isPressed(hashLabel(label));
}

/** @brief Returns true if an element currently holds keyboard focus. */
inline bool isFocused(uint32_t elementId) {
  auto *uiState = getUiState();
  return uiState && (uiState->focusedElementId == elementId);
}
inline bool isFocused(const std::string &label) {
  return isFocused(hashLabel(label));
}

/** @brief Returns true for exactly 1 frame when an element gains focus. */
inline bool focusGained(uint32_t elementId) {
  auto *uiState = getUiState();
  return uiState && (uiState->focusedElementId == elementId) &&
         (uiState->previousFocusedElementId != elementId);
}
inline bool focusGained(const std::string &label) {
  return focusGained(hashLabel(label));
}

/** @brief Returns true for exactly 1 frame when an element loses focus (blur).
 */
inline bool focusLost(uint32_t elementId) {
  auto *uiState = getUiState();
  return uiState && (uiState->previousFocusedElementId == elementId) &&
         (uiState->focusedElementId != elementId);
}
inline bool focusLost(const std::string &label) {
  return focusLost(hashLabel(label));
}

} // namespace atomic
