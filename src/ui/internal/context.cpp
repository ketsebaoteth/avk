#include "ui/internal/context.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "ui/core/resources.h"
#include "ui/utils/clayUtils.h"
#include <iostream>

namespace atomic {

namespace { // internal linkage
std::unique_ptr<UIState> g_uiState = nullptr;
[[maybe_unused]] static void *g_clayArenaMemory = nullptr;
uint32_t g_elementIdCounter = 0;
} // namespace

void resetGlobalIdCounter() { g_elementIdCounter = 0; };
UIState *getUiState() { return g_uiState.get(); };
uint32_t &getElementIdCounter() { return g_elementIdCounter; };

void initialize(std::optional<VeraNativeHandle> nativeDisplay,
                bool enableValidation) {
  g_uiState = std::make_unique<UIState>();

  g_uiState->context =
      std::make_unique<avk::VulkanContext>(nativeDisplay, enableValidation);

  g_uiState->renderer =
      std::make_unique<avk::Renderer>(g_uiState->context.get());

  uint64_t totalMemorySize = Clay_MinMemorySize();
  g_clayArenaMemory = std::malloc(totalMemorySize);
  Clay_Arena arena =
      Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, g_clayArenaMemory);
  Clay_Initialize(arena, Clay_Dimensions{800, 600},
                  Clay_ErrorHandler{utils::layout::handleClayError, nullptr});
  Clay_SetMeasureTextFunction(measureTextCallback, nullptr);

  g_uiState->defaultFontId = loadFont("fonts/Inter_24pt-Regular.ttf", 19);

  std::vector<uint32_t> iconCodepoints;
  iconCodepoints.reserve(6400);
  for (uint32_t i = 0xE000; i <= 0xF8FF; ++i) {
    iconCodepoints.push_back(i);
  }

  g_uiState->defaultIconFontIds[0] =
      loadFont("fonts/lucide.ttf", 12, iconCodepoints);
  g_uiState->defaultIconFontIds[1] =
      loadFont("fonts/lucide.ttf", 16, iconCodepoints);
  g_uiState->defaultIconFontIds[2] =
      loadFont("fonts/lucide.ttf", 24, iconCodepoints);
  g_uiState->defaultIconFontIds[3] =
      loadFont("fonts/lucide.ttf", 32, iconCodepoints);
  g_uiState->defaultIconFontIds[4] =
      loadFont("fonts/lucide.ttf", 48, iconCodepoints);
}

void shutdown() {
  if (!g_uiState)
    return;

  if (g_uiState->context && g_uiState->context->isValid()) {
    vkDeviceWaitIdle(g_uiState->context->getDevice());
  }

  g_uiState->sessions.clear();
  g_uiState->renderer.reset();
  g_uiState->context.reset();

  if (g_uiState->clayArenaMemory) {
    std::free(g_uiState->clayArenaMemory);
  }

  g_uiState.reset();
}

void registerWindow(VeraWindow *window) {
  if (!g_uiState)
    return;
  VeraNativeHandle native = window->getNativeHandle();
  VkSurfaceKHR surface = VK_NULL_HANDLE;

#if defined(VERA_PLATFORM_WIN32)
  surface = g_uiState->context->createWin32Surface(native.hwnd,
                                                   GetModuleHandle(nullptr));
#elif defined(VERA_PLATFORM_LINUX)
  if (native.waylandSurface != nullptr) {
    surface = g_uiState->context->createWaylandSurface(native.display,
                                                       native.waylandSurface);
  } else {
    surface =
        g_uiState->context->createX11Surface(native.display, native.x11Window);
  }
#endif

  if (surface == VK_NULL_HANDLE) {
    std::cerr << "avk: Failed to map native surface inside atomicUI."
              << std::endl;
    return;
  }

  float scale = window->getCurrentMonitor().dpiScale;
  int32_t intScale = static_cast<int32_t>(std::ceil(scale));
  if (intScale < 1) {
    intScale = 1;
  }

  auto state = window->getState();
  uint32_t physicalWidth = state.width * intScale;
  uint32_t physicalHeight = state.height * intScale;
  auto canvas = std::make_unique<avk::WindowCanvas>(
      g_uiState->context.get(), surface, physicalWidth, physicalHeight);

  g_uiState->sessions.push_back(
      window::WindowSession{window, surface, std::move(canvas),
                            std::chrono::high_resolution_clock::now(), 0.016f});

  window->setScrollCallback([&](double xOffset, double yOffset) {
    g_uiState->mouseWheelDeltaX += static_cast<float>(xOffset);
    g_uiState->mouseWheelDeltaY += static_cast<float>(yOffset);
  });

  window->setMouseMoveCallback([](double x, double y) {
    if (!g_uiState)
      return;
    g_uiState->pointerPos =
        glm::vec2(static_cast<float>(x), static_cast<float>(y));
  });

  window->setMouseButtonCallback([](VeraMouseButton button, bool pressed) {
    if (!g_uiState)
      return;
    if (button == VeraMouseButton::Left) {
      if (pressed) {
        g_uiState->pointerPressed = true;
        g_uiState->pointerDown = true;
      } else {
        g_uiState->pointerPressed = false;
        g_uiState->pointerDown = false;
      }
    }
  });

  window->setCharCallback([](uint32_t codepoint) {
    if (!g_uiState)
      return;

    if (g_uiState->focusedElementId != 0 && codepoint >= 32) {
      g_uiState->capturedChars.push_back(codepoint);
    }
  });

  window->setKeyCallback([](VeraKey key, bool pressed, bool repeat) {
    if (!g_uiState)
      return;
    (void)repeat;

    if (key == VeraKey::LeftCtrl || key == VeraKey::RightCtrl) {
      g_uiState->ctrlPressed = pressed;
    }
    if (key == VeraKey::LeftShift || key == VeraKey::RightShift) {
      g_uiState->shiftPressed = pressed;
    }

    if (g_uiState->focusedElementId != 0 && pressed) {
      if (key == VeraKey::Backspace) {
        g_uiState->backspacePressed = true;
      } else if (key == VeraKey::Enter) {
        g_uiState->enterPressed = true;
      } else if (key == VeraKey::Delete) {
        g_uiState->deletePressed = true;
      } else if (key == VeraKey::Left) {
        g_uiState->leftArrowPressed = true;
      } else if (key == VeraKey::Right) {
        g_uiState->rightArrowPressed = true;
      } else if (key == VeraKey::ALower && g_uiState->ctrlPressed) {
        g_uiState->selectAll = true;
      }
    }
  });
}

void unregisterWindow(VeraWindow *window) {
  if (!g_uiState)
    return;

  auto it = std::remove_if(
      g_uiState->sessions.begin(), g_uiState->sessions.end(),
      [window](const window::WindowSession &s) { return s.window == window; });
  if (it != g_uiState->sessions.end()) {
    vkDeviceWaitIdle(g_uiState->context->getDevice());
    g_uiState->sessions.erase(it, g_uiState->sessions.end());
  }
}

uint32_t getDefaultFontId() { return g_uiState ? g_uiState->defaultFontId : 0; }

} // namespace atomic
