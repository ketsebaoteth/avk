
#pragma once

#include "avk/avk_core.h"
#include "avk/avk_font.h"
#include "avk/avk_renderer.h"
#include "avk/window/session.h"

#include "ui/internal/cascadingStyle.h"
#include "ui/internal/payload.h"
#include "ui/state/inputState.h"
#include "ui/state/scrollViewState.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace atomic {
/**
 * @brief Master runtime UI engine context state.
 */
struct UIState {
  std::unique_ptr<avk::VulkanContext> context;
  std::unique_ptr<avk::Renderer> renderer;
  void *clayArenaMemory = nullptr;
  std::vector<window::WindowSession> sessions;
  std::vector<std::unique_ptr<RenderPayload>> framePayloads;
  std::vector<uint32_t> positioningContextStack;
  std::vector<CascadingStyle> cascadingStyleStack;
  std::unordered_map<uint32_t, CascadingStyle> computedStyleMap;

  glm::vec2 pointerPos = glm::vec2(0.0f);
  bool pointerPressed = false;
  bool pointerDown = false;
  std::vector<std::unique_ptr<avk::Font>> fonts;
  std::unordered_map<uint32_t, InputState> inputStateMap;
  std::unordered_map<uint32_t, ScrollViewState> scrollViewStates;

  uint32_t defaultFontId = 0;
  std::array<uint32_t, 5> defaultIconFontIds;
  std::unordered_map<std::string, uint32_t> iconMap;

  uint32_t focusedElementId = 0;
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
void initialize(std::optional<VeraNativeHandle> nativeDisplay,
                bool enableValidation = false);

/** @brief Shuts down the engine and releases all Vulkan resources. */
void shutdown();

/** @brief Maps a native Vera window session into the UI context. */
void registerWindow(VeraWindow *window);

/** @brief Unregisters and destroys a window session context. */
void unregisterWindow(VeraWindow *window);

void resetGlobalIdCounter();
UIState *getUiState();
uint32_t &getElementIdCounter();
} // namespace atomic
