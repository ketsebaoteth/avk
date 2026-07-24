#define NOMINMAX
#include "avk/atomic_ui.h"
#include "avk/avk_canvas.h"
#include "avk/avk_core.h"
#include "avk/avk_font.h"
#include "avk/avk_renderer.h"
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

uint32_t loadFont(const std::string &path, uint32_t fontSize) {
  if (!g_uiState)
    return 0;

  auto font =
      std::make_unique<avk::Font>(g_uiState->context.get(), path, fontSize);
  g_uiState->fonts.push_back(std::move(font));

  return static_cast<uint32_t>(g_uiState->fonts.size() - 1);
}

static Clay_Dimensions measureTextCallback(Clay_StringSlice text,
                                           Clay_TextElementConfig *config,
                                           void *userData) {
  (void)userData;
  if (!g_uiState || config->fontId >= g_uiState->fonts.size()) {
    return Clay_Dimensions{0.0f, 0.0f};
  }

  std::string str(text.chars, text.length);
  glm::vec2 size = g_uiState->fonts[config->fontId]->measureText(str);

  return Clay_Dimensions{size.x, size.y};
}

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
  Clay_SetMeasureTextFunction(measureTextCallback, nullptr);

  // autoload built in font inter right now
  g_uiState->defaultFontId =
      loadFont("assets/fonts/Inter_24pt-Regular.ttf", 19);
}

uint32_t getDefaultFontId() { return g_uiState ? g_uiState->defaultFontId : 0; }

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

    // Track Ctrl modifier globally
    if (key == VeraKey::LeftCtrl || key == VeraKey::RightCtrl) {
      g_uiState->ctrlPressed = pressed;
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
        g_uiState->selectAll = true; // Ctrl+A triggered!
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

  if (g_uiState->pointerPressed && !g_uiState->anyInputBoxHovered) {
    g_uiState->focusedElementId = 0;
  }
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
    } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_BORDER) {
      Clay_BorderRenderData *borderData = &cmd->renderData.border;

      avk::InstanceData instance{};
      instance.rectXYWH =
          glm::vec4(cmd->boundingBox.x, cmd->boundingBox.y,
                    cmd->boundingBox.width, cmd->boundingBox.height);

      // Clay border width has top/bottom/left/right; using width.top for
      // uniform SDF stroke
      instance.strokeThickness = static_cast<float>(borderData->width.top);

      instance.strokeColor =
          glm::vec4(borderData->color.r / 255.0f, borderData->color.g / 255.0f,
                    borderData->color.b / 255.0f, borderData->color.a / 255.0f);

      instance.borderRadius = glm::vec4(borderData->cornerRadius.topLeft,
                                        borderData->cornerRadius.topRight,
                                        borderData->cornerRadius.bottomLeft,
                                        borderData->cornerRadius.bottomRight);

      // Transparent background fill so only the border stroke renders
      instance.fillColorA = glm::vec4(0.0f);
      instance.shapeType = 0; // Rectangle
      instance.fillType = 0;  // Solid
      instance.blur = 0.0f;

      g_uiState->renderer->submit(instance);
    } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE) {
      Clay_ImageRenderData *imageData = &cmd->renderData.image;
      auto *payload = static_cast<ImagePayload *>(cmd->userData);

      uint32_t textureIndex = 0;
      glm::vec4 tintColor = glm::vec4(1.0f);

      if (payload != nullptr) {
        textureIndex = payload->textureIndex;
        tintColor = payload->tintColor;

        delete payload;
      }

      avk::InstanceData instance{};
      instance.rectXYWH =
          glm::vec4(cmd->boundingBox.x, cmd->boundingBox.y,
                    cmd->boundingBox.width, cmd->boundingBox.height);

      instance.fillColorA = glm::vec4(imageData->backgroundColor.r / 255.0f,
                                      imageData->backgroundColor.g / 255.0f,
                                      imageData->backgroundColor.b / 255.0f,
                                      imageData->backgroundColor.a / 255.0f);

      instance.fillColorB = tintColor;

      instance.borderRadius = glm::vec4(imageData->cornerRadius.topLeft,
                                        imageData->cornerRadius.topRight,
                                        imageData->cornerRadius.bottomLeft,
                                        imageData->cornerRadius.bottomRight);

      instance.shapeType = 0;
      instance.fillType = 4;
      instance.textureIndex = textureIndex;
      instance.strokeThickness = 0.0f;
      instance.blur = 0.0f;

      g_uiState->renderer->submit(instance);
    } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
      Clay_TextRenderData *textData = &cmd->renderData.text;

      if (textData->fontId >= g_uiState->fonts.size()) {
        continue;
      }

      union {
        void *p;
        float f;
      } u;
      u.p = cmd->userData;
      float textOffset = u.f;

      const avk::Font &font = *g_uiState->fonts[textData->fontId];
      float cursorX = cmd->boundingBox.x;
      float cursorY = cmd->boundingBox.y;

      glm::vec4 textColor = glm::vec4(
          textData->textColor.r / 255.0f, textData->textColor.g / 255.0f,
          textData->textColor.b / 255.0f, textData->textColor.a / 255.0f);

      for (int32_t charIndex = 0; charIndex < textData->stringContents.length;
           ++charIndex) {
        char c = textData->stringContents.chars[charIndex];
        const avk::Glyph &glyph = font.getGlyph(c);

        float posX = std::round(cursorX + glyph.bearing.x);
        float posY = std::round(cursorY + (font.getAscent() - glyph.bearing.y) +
                                textOffset);
        float posW = std::round(glyph.size.x);
        float posH = std::round(glyph.size.y);

        avk::InstanceData instance{};
        instance.rectXYWH = glm::vec4(posX, posY, posW, posH);
        instance.borderRadius = glm::vec4(0.0f);
        instance.fillColorA = textColor;
        instance.uvBounds = glyph.uvBounds;

        instance.shapeType = 0;
        instance.fillType = 3;
        instance.textureIndex = font.getTextureIndex();
        instance.strokeThickness = 0.0f;
        instance.blur = 0.0f;

        g_uiState->renderer->submit(instance);

        cursorX += glyph.advance;
      }
    }
  }
  session->canvas->endFrame(*g_uiState->renderer);

  // reset
  g_uiState->capturedChars.clear();
  g_uiState->backspacePressed = false;
  g_uiState->enterPressed = false;
  g_uiState->anyInputBoxHovered = false;
  g_uiState->deletePressed = false;
  g_uiState->leftArrowPressed = false;
  g_uiState->rightArrowPressed = false;
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

uint32_t loadTexture(const std::string &path) {
  if (!g_uiState)
    return 0;
  return g_uiState->context->getTextureManager()->loadTexture(path);
}

void unloadTexture(uint32_t textureIndex) {
  if (!g_uiState)
    return;
  g_uiState->context->getTextureManager()->unloadTexture(textureIndex);
}
avk::Font *getFont(uint32_t fontId) {
  if (!g_uiState || fontId >= g_uiState->fonts.size()) {
    return nullptr;
  }
  return g_uiState->fonts[fontId].get();
}

bool isKeyboardCaptured() {
  return g_uiState && g_uiState->focusedElementId != 0;
}

void clearKeyboardFocus() {
  if (g_uiState) {
    g_uiState->focusedElementId = 0;
  }
}

} // namespace atomic

namespace utils::layout {
atomic::UIState *getUiState() { return atomic::g_uiState.get(); };
uint32_t &getElementIdCounter() { return atomic::g_elementIdCounter; };
} // namespace utils::layout
