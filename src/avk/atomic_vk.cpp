#define NOMINMAX
#include "avk/atomic_ui.h"
#include "avk/avk_canvas.h"
#include "avk/avk_core.h"
#include "avk/avk_renderer.h"
#include "avk/utils/ui/2dCollision.h"
#include "avk/utils/ui/layout.h"
#include "clay.h"
#include "core/app/Types.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

namespace atomic {

// --globals
std::unique_ptr<UIState> g_uiState = nullptr;
[[maybe_unused]] static void *g_clayArenaMemory = nullptr;
uint32_t g_elementIdCounter = 0;
// --globals

void initialize(std::optional<VeraNativeHandle> nativeDisplay,
                bool enableValidation) {
  g_uiState = std::make_unique<UIState>();

  g_uiState->context =
      std::make_unique<avk::VulkanContext>(nativeDisplay, enableValidation);

  g_uiState->renderer =
      std::make_unique<avk::Renderer>(g_uiState->context.get());

  uint64_t totalMemorySize = Clay_MinMemorySize();
  g_uiState->clayArenaMemory = std::malloc(totalMemorySize);
  Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(
      totalMemorySize, g_uiState->clayArenaMemory);
  Clay_Initialize(arena, Clay_Dimensions{800, 600},
                  Clay_ErrorHandler{utils::layout::handleClayError, nullptr});
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

  auto state = window->getState();
  auto canvas = std::make_unique<avk::WindowCanvas>(
      g_uiState->context.get(), surface, state.width, state.height);

  g_uiState->sessions.push_back(
      window::WindowSession{window, surface, std::move(canvas),
                            std::chrono::high_resolution_clock::now(), 0.016f});

  window->setMouseMoveCallback([](double x, double y) {
    if (!g_uiState)
      return;
    g_uiState->pointerPos =
        glm::vec2(static_cast<float>(x), static_cast<float>(y));

    Clay_SetPointerState(
        Clay_Vector2{g_uiState->pointerPos.x, g_uiState->pointerPos.y},
        g_uiState->pointerPressed);
  });

  window->setMouseButtonCallback([](VeraMouseButton button, bool pressed) {
    if (!g_uiState)
      return;
    if (button == VeraMouseButton::Left) {
      g_uiState->pointerPressed = pressed;

      Clay_SetPointerState(
          Clay_Vector2{g_uiState->pointerPos.x, g_uiState->pointerPos.y},
          g_uiState->pointerPressed);
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

bool beginFrame(VeraWindow *window) {
  if (!g_uiState || window == nullptr)
    return false;

  window::WindowSession *session = g_uiState->findSession(window);
  if (session == nullptr || !session->canvas->isActive())
    return false;

  auto currentTime = std::chrono::high_resolution_clock::now();
  session->lastDeltaTime =
      std::chrono::duration<float>(currentTime - session->lastTime).count();
  session->lastTime = currentTime;

  g_elementIdCounter = 0;
  auto clayDimensions = Clay_Dimensions{};
  clayDimensions.width = static_cast<float>(session->canvas->getWidth());
  clayDimensions.height = static_cast<float>(session->canvas->getHeight());

  Clay_SetLayoutDimensions(clayDimensions);
  Clay_BeginLayout();

  auto val = session->canvas->beginFrame();
  return val;
}

void endFrame(VeraWindow *window) {
  if (!g_uiState)
    return;

  window::WindowSession *session = g_uiState->findSession(window);
  if (session == nullptr)
    return;

  Clay_RenderCommandArray renderCommands =
      Clay_EndLayout(session->lastDeltaTime);

  g_uiState->renderer->begin();

  for (int32_t i = 0; i < renderCommands.length; ++i) {
    Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&renderCommands, i);

    if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
      Clay_RectangleRenderData *rectData = &cmd->renderData.rectangle;

      avk::InstanceData instance{};
      instance.rectXYWH =
          glm::vec4(cmd->boundingBox.x, cmd->boundingBox.y,
                    cmd->boundingBox.width, cmd->boundingBox.height);

      instance.fillColorA = glm::vec4(rectData->backgroundColor.r / 255.0f,
                                      // skip the frame.
                                      rectData->backgroundColor.g / 255.0f,
                                      rectData->backgroundColor.b / 255.0f,
                                      rectData->backgroundColor.a / 255.0f);

      instance.borderRadius = glm::vec4(rectData->cornerRadius.topLeft,
                                        rectData->cornerRadius.topRight,
                                        rectData->cornerRadius.bottomLeft,
                                        rectData->cornerRadius.bottomRight);

      instance.shapeType = 0; // Rectangle
      instance.fillType = 0;  // Solid
      instance.strokeThickness = 0.0f;
      instance.blur = 0.0f;

      g_uiState->renderer->submit(instance);
    }
  }
  session->canvas->endFrame(*g_uiState->renderer);
}

void resizeWindow(VeraWindow *window, uint32_t width, uint32_t height) {
  if (!g_uiState)
    return;
  window::WindowSession *session = g_uiState->findSession(window);
  if (session) {
    session->canvas->resize(width, height);
  }
}

uint32_t getWidth(VeraWindow *window) {
  if (!g_uiState)
    return 0;
  window::WindowSession *session = g_uiState->findSession(window);
  return session ? session->canvas->getWidth() : 0;
}

uint32_t getHeight(VeraWindow *window) {
  if (!g_uiState)
    return 0;
  window::WindowSession *session = g_uiState->findSession(window);
  return session ? session->canvas->getHeight() : 0;
}

void Column(Modifier &&modifier, const std::function<void()> &content) {
  const auto &style = modifier.getStyle();

  Clay__OpenElementWithId(utils::layout::getNextId("Column"));

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width)
                                         : CLAY_SIZING_GROW(),
                 .height = style.hasHeight ? CLAY_SIZING_FIXED(style.height)
                                           : CLAY_SIZING_GROW()},
      .padding = {style.padLeft, style.padRight, style.padTop, style.padBottom},
      .childGap = style.childGap,
      .layoutDirection = CLAY_TOP_TO_BOTTOM};
  decl.backgroundColor = {
      style.backgroundColor.r * 255.0f, style.backgroundColor.g * 255.0f,
      style.backgroundColor.b * 255.0f, style.backgroundColor.a * 255.0f};
  decl.cornerRadius = {style.borderRadius.x, style.borderRadius.y,
                       style.borderRadius.z, style.borderRadius.w};

  Clay__ConfigureOpenElement(decl);

  content();

  Clay__CloseElement();
}

void Row(Modifier &&modifier, const std::function<void()> &content) {
  const auto &style = modifier.getStyle();

  Clay__OpenElementWithId(utils::layout::getNextId("Row"));

  Clay_ElementDeclaration decl{};
  decl.layout = {
      .sizing = {.width = style.hasWidth ? CLAY_SIZING_FIXED(style.width)
                                         : CLAY_SIZING_GROW(),
                 .height = style.hasHeight ? CLAY_SIZING_FIXED(style.height)
                                           : CLAY_SIZING_GROW()},
      .padding = {style.padLeft, style.padRight, style.padTop, style.padBottom},
      .childGap = style.childGap,
      .layoutDirection = CLAY_LEFT_TO_RIGHT};
  decl.backgroundColor = {
      style.backgroundColor.r * 255.0f, style.backgroundColor.g * 255.0f,
      style.backgroundColor.b * 255.0f, style.backgroundColor.a * 255.0f};
  decl.cornerRadius = {style.borderRadius.x, style.borderRadius.y,
                       style.borderRadius.z, style.borderRadius.w};

  Clay__ConfigureOpenElement(decl);

  content();

  Clay__CloseElement();
}
} // namespace atomic
namespace utils::layout {
atomic::UIState *getUiState() { return atomic::g_uiState.get(); };
uint32_t &getElementIdCounter() { return atomic::g_elementIdCounter; };
} // namespace utils::layout
