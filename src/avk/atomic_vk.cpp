#define NOMINMAX
#include "animation/animation.h"
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
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#ifndef AVK_ASSETS_DIR
#define AVK_ASSETS_DIR "assets"
#endif

namespace atomic {

std::unique_ptr<UIState> g_uiState = nullptr;
[[maybe_unused]] static void *g_clayArenaMemory = nullptr;
uint32_t g_elementIdCounter = 0;

std::string getPath(const std::string &relativePath) {
  namespace fs = std::filesystem;
  fs::path path(relativePath);

  if (path.is_absolute()) {
    return path.string();
  }

  fs::path baseAssetsDir(AVK_ASSETS_DIR);

  if (path.has_parent_path() && path.begin()->string() == "assets") {
    return path.string();
  }

  return (baseAssetsDir / path).string();
}

uint32_t loadFont(const std::string &path, uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints) {
  if (!g_uiState)
    return 0;

  auto font = std::make_unique<avk::Font>(g_uiState->context.get(),
                                          getPath(path), fontSize, codepoints);
  g_uiState->fonts.push_back(std::move(font));

  return static_cast<uint32_t>(g_uiState->fonts.size() - 1);
}

uint32_t loadFont(const std::string &path, uint32_t fontSize) {
  return loadFont(path, fontSize, {});
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

uint32_t getClosestIconFontId(float requestedSize) {
  if (!utils::layout::getUiState())
    return 0;

  static constexpr std::array<float, 5> TIERS = {12.0f, 16.0f, 24.0f, 32.0f,
                                                 48.0f};

  uint32_t closestIndex = 1;
  float minDiff = std::abs(requestedSize - TIERS[1]);

  for (size_t i = 0; i < TIERS.size(); ++i) {
    float diff = std::abs(requestedSize - TIERS[i]);
    if (diff < minDiff) {
      minDiff = diff;
      closestIndex = static_cast<uint32_t>(i);
    }
  }

  return utils::layout::getUiState()->defaultIconFontIds[closestIndex];
}

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

bool beginFrame(VeraWindow *window) {
  if (!g_uiState || window == nullptr)
    return false;

  window::WindowSession *session = g_uiState->findSession(window);
  if (session == nullptr || !session->canvas->isActive())
    return false;

  g_uiState->computedStyleMap.clear();
  g_uiState->positioningContextStack.clear();
  g_uiState->cascadingStyleStack.clear();

  Clay_SetPointerState(
      Clay_Vector2{g_uiState->pointerPos.x, g_uiState->pointerPos.y},
      g_uiState->pointerDown);

  auto currentTime = std::chrono::high_resolution_clock::now();
  session->lastDeltaTime =
      std::chrono::duration<float>(currentTime - session->lastTime).count();
  session->lastTime = currentTime;

  atomic::AnimationManager::instance().update(session->lastDeltaTime);

  g_elementIdCounter = 0;
  auto state = window->getState();
  auto clayDimensions = Clay_Dimensions{};
  clayDimensions.width = static_cast<float>(state.width);
  clayDimensions.height = static_cast<float>(state.height);

  Clay_SetLayoutDimensions(clayDimensions);
  Clay_BeginLayout();

  auto val = session->canvas->beginFrame();
  return val;
}

static uint32_t decodeNextUtf8(const char *chars, int32_t length,
                               uint32_t &index) {
  if (index >= static_cast<uint32_t>(length))
    return 0;

  unsigned char c = chars[index++];

  if (c < 0x80)
    return c;

  if ((c & 0xE0) == 0xC0) {
    if (index >= static_cast<uint32_t>(length))
      return c;
    uint32_t res = (c & 0x1F) << 6;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F);
    return res;
  }

  if ((c & 0xF0) == 0xE0) {
    if (index + 1 >= static_cast<uint32_t>(length))
      return c;
    uint32_t res = (c & 0x0F) << 12;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F) << 6;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F);
    return res;
  }

  if ((c & 0xF8) == 0xF0) {
    if (index + 2 >= static_cast<uint32_t>(length))
      return c;
    uint32_t res = (c & 0x07) << 18;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F) << 12;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F) << 6;
    res |= (static_cast<unsigned char>(chars[index++]) & 0x3F);
    return res;
  }

  return c;
}

void endFrame(VeraWindow *window) {
  if (!g_uiState)
    return;

  window::WindowSession *session = g_uiState->findSession(window);
  if (!session)
    return;

  if (g_uiState->pointerPressed && !g_uiState->anyInputBoxHovered) {
    g_uiState->focusedElementId = 0;
  }

  Clay_RenderCommandArray renderCommands =
      Clay_EndLayout(session->lastDeltaTime);

  g_uiState->renderer->begin();

  std::vector<Clay_BoundingBox> clipStack;
  [[maybe_unused]] Clay_BoundingBox *currentClip = nullptr;

  auto getPivotTransformedCoords = [](const Clay_BoundingBox &box, float scale,
                                      float rotation, const glm::vec2 &origin,
                                      const glm::vec2 &translate) -> glm::vec4 {
    glm::vec2 pos(box.x + translate.x, box.y + translate.y);
    glm::vec2 size(box.width, box.height);

    glm::vec2 pivotLocal = origin * size;
    glm::vec2 pivotWorld = pos + pivotLocal;
    glm::vec2 centerWorld = pos + size * 0.5f;

    glm::vec2 toCenter = centerWorld - pivotWorld;

    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);
    glm::vec2 rotatedToCenter((toCenter.x * cosR - toCenter.y * sinR) * scale,
                              (toCenter.x * sinR + toCenter.y * cosR) * scale);

    glm::vec2 newCenterWorld = pivotWorld + rotatedToCenter;
    return glm::vec4(newCenterWorld - size * 0.5f, size.x, size.y);
  };

  for (int32_t i = 0; i < renderCommands.length; ++i) {
    Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&renderCommands, i);

    if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
      clipStack.push_back(cmd->boundingBox);
      currentClip = &clipStack.back();
      continue;
    }
    if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
      if (!clipStack.empty()) {
        clipStack.pop_back();
        currentClip = clipStack.empty() ? nullptr : &clipStack.back();
      }
      continue;
    }

    glm::vec4 activeClipRect;
    if (!clipStack.empty()) {
      const auto &clip = clipStack.back();
      activeClipRect = glm::vec4(std::round(clip.x), std::round(clip.y),
                                 std::round(clip.x + clip.width),
                                 std::round(clip.y + clip.height));
    } else {
      activeClipRect = glm::vec4(-10000.0f, -10000.0f, 200000.0f, 200000.0f);
    }

    if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
      Clay_RectangleRenderData *rectData = &cmd->renderData.rectangle;

      float elementScale = 1.0f;
      float elementRotation = 0.0f;
      float elementBlur = 0.0f;
      glm::vec2 transformOrigin(0.5f, 0.5f);
      glm::vec2 translate(0.0f, 0.0f);

      if (cmd->userData != nullptr) {
        auto *payload = static_cast<RenderPayload *>(cmd->userData);
        elementScale = payload->scale;
        elementRotation = payload->rotation;
        elementBlur = payload->blur;
        transformOrigin = payload->transformOrigin;
        translate = payload->translate;
      }

      glm::vec4 transformedRect = getPivotTransformedCoords(
          cmd->boundingBox, elementScale, elementRotation, transformOrigin,
          translate);

      avk::InstanceData instance{};
      instance.rectXYWH = glm::vec4(
          std::round(transformedRect.x), std::round(transformedRect.y),
          std::round(transformedRect.z), std::round(transformedRect.w));

      instance.fillColorA = glm::vec4(rectData->backgroundColor.r / 255.0f,
                                      rectData->backgroundColor.g / 255.0f,
                                      rectData->backgroundColor.b / 255.0f,
                                      rectData->backgroundColor.a / 255.0f);

      instance.borderRadius = glm::vec4(rectData->cornerRadius.topLeft,
                                        rectData->cornerRadius.topRight,
                                        rectData->cornerRadius.bottomLeft,
                                        rectData->cornerRadius.bottomRight);

      instance.clipRect = activeClipRect;
      instance.shapeType = 0;
      instance.fillType = 0;
      instance.strokeThickness = 0.0f;
      instance.blur = elementBlur;
      instance.scale = elementScale;
      instance.rotation = elementRotation;

      g_uiState->renderer->submit(instance);
    } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_BORDER) {
      Clay_BorderRenderData *borderData = &cmd->renderData.border;

      float elementScale = 1.0f;
      float elementRotation = 0.0f;
      float elementBlur = 0.0f;
      glm::vec2 transformOrigin(0.5f, 0.5f);
      glm::vec2 translate(0.0f, 0.0f);

      if (cmd->userData != nullptr) {
        auto *payload = static_cast<RenderPayload *>(cmd->userData);
        elementScale = payload->scale;
        elementRotation = payload->rotation;
        elementBlur = payload->blur;
        transformOrigin = payload->transformOrigin;
        translate = payload->translate;
      }

      glm::vec4 transformedRect = getPivotTransformedCoords(
          cmd->boundingBox, elementScale, elementRotation, transformOrigin,
          translate);

      avk::InstanceData instance{};
      instance.rectXYWH = glm::vec4(
          std::round(transformedRect.x), std::round(transformedRect.y),
          std::round(transformedRect.z), std::round(transformedRect.w));

      instance.strokeThickness = static_cast<float>(borderData->width.top);

      instance.strokeColor =
          glm::vec4(borderData->color.r / 255.0f, borderData->color.g / 255.0f,
                    borderData->color.b / 255.0f, borderData->color.a / 255.0f);

      instance.borderRadius = glm::vec4(borderData->cornerRadius.topLeft,
                                        borderData->cornerRadius.topRight,
                                        borderData->cornerRadius.bottomLeft,
                                        borderData->cornerRadius.bottomRight);

      instance.fillColorA = glm::vec4(0.0f);
      instance.clipRect = activeClipRect;
      instance.shapeType = 0;
      instance.fillType = 0;
      instance.blur = elementBlur;
      instance.scale = elementScale;
      instance.rotation = elementRotation;

      g_uiState->renderer->submit(instance);
    } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE) {
      Clay_ImageRenderData *imageData = &cmd->renderData.image;

      float elementScale = 1.0f;
      float elementRotation = 0.0f;
      float elementBlur = 0.0f;
      glm::vec2 transformOrigin(0.5f, 0.5f);
      glm::vec2 translate(0.0f, 0.0f);
      uint32_t textureIndex = 0;
      glm::vec4 tintColor(1.0f);

      glm::vec4 uvBounds(0.0f, 0.0f, 1.0f, 1.0f);
      ObjectFit objectFit = ObjectFit::Fill;

      if (cmd->userData != nullptr) {
        auto *payload = static_cast<RenderPayload *>(cmd->userData);
        elementScale = payload->scale;
        elementRotation = payload->rotation;
        elementBlur = payload->blur;
        transformOrigin = payload->transformOrigin;
        translate = payload->translate;
        textureIndex = payload->textureIndex;
        tintColor = payload->tintColor;

        uvBounds = payload->uvBounds;
        objectFit = payload->objectFit;
      }

      glm::vec4 transformedRect = getPivotTransformedCoords(
          cmd->boundingBox, elementScale, elementRotation, transformOrigin,
          translate);

      glm::vec4 finalUvBounds = uvBounds;

      if (objectFit != ObjectFit::Fill && objectFit != ObjectFit::Custom) {
        VkExtent2D texExt =
            g_uiState->context->getTextureManager()->getTextureExtent(
                textureIndex);

        if (texExt.height > 0 && transformedRect.w > 0.0f) {
          float srcAspect = static_cast<float>(texExt.width) /
                            static_cast<float>(texExt.height);
          float destAspect = transformedRect.z / transformedRect.w;

          if (objectFit == ObjectFit::Cover) {
            if (srcAspect > destAspect) {
              float scale = destAspect / srcAspect;
              float offset = (1.0f - scale) * 0.5f;
              finalUvBounds = glm::vec4(offset, 0.0f, 1.0f - offset, 1.0f);
            } else {
              float scale = srcAspect / destAspect;
              float offset = (1.0f - scale) * 0.5f;
              finalUvBounds = glm::vec4(0.0f, offset, 1.0f, 1.0f - offset);
            }
          } else if (objectFit == ObjectFit::Contain) {
            if (srcAspect > destAspect) {
              float scale = srcAspect / destAspect;
              float offset = (1.0f - scale) * 0.5f;
              finalUvBounds = glm::vec4(0.0f, offset, 1.0f, 1.0f - offset);
            } else {
              float scale = destAspect / srcAspect;
              float offset = (1.0f - scale) * 0.5f;
              finalUvBounds = glm::vec4(offset, 0.0f, 1.0f - offset, 1.0f);
            }
          }
        }
      }

      avk::InstanceData instance{};
      instance.rectXYWH = glm::vec4(
          std::round(transformedRect.x), std::round(transformedRect.y),
          std::round(transformedRect.z), std::round(transformedRect.w));

      instance.fillColorA = glm::vec4(imageData->backgroundColor.r / 255.0f,
                                      imageData->backgroundColor.g / 255.0f,
                                      imageData->backgroundColor.b / 255.0f,
                                      imageData->backgroundColor.a / 255.0f);
      instance.fillColorB = tintColor;

      instance.uvBounds = finalUvBounds;

      instance.borderRadius = glm::vec4(imageData->cornerRadius.topLeft,
                                        imageData->cornerRadius.topRight,
                                        imageData->cornerRadius.bottomLeft,
                                        imageData->cornerRadius.bottomRight);

      instance.clipRect = activeClipRect;
      instance.shapeType = 0;
      instance.fillType = 4;
      instance.textureIndex = textureIndex;
      instance.strokeThickness = 0.0f;
      instance.blur = elementBlur;
      instance.scale = elementScale;
      instance.rotation = elementRotation;

      g_uiState->renderer->submit(instance);
    } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
      Clay_TextRenderData *textData = &cmd->renderData.text;

      if (textData->fontId >= g_uiState->fonts.size()) {
        continue;
      }

      float textOffset = 0.0f;
      float elementScale = 1.0f;
      float elementRotation = 0.0f;
      glm::vec2 transformOrigin(0.5f, 0.5f);
      glm::vec2 translate(0.0f, 0.0f);

      if (cmd->userData != nullptr) {
        auto *payload = static_cast<RenderPayload *>(cmd->userData);
        elementScale = payload->scale;
        elementRotation = payload->rotation;
        transformOrigin = payload->transformOrigin;
        translate = payload->translate;
        textOffset = payload->textOffset;
      }

      const avk::Font &font = *g_uiState->fonts[textData->fontId];
      float cursorX = cmd->boundingBox.x;
      float cursorY = cmd->boundingBox.y;

      glm::vec4 textColor = glm::vec4(
          textData->textColor.r / 255.0f, textData->textColor.g / 255.0f,
          textData->textColor.b / 255.0f, textData->textColor.a / 255.0f);

      uint32_t charIndex = 0;
      int32_t stringLength = textData->stringContents.length;

      while (charIndex < static_cast<uint32_t>(stringLength)) {
        uint32_t codepoint = decodeNextUtf8(textData->stringContents.chars,
                                            stringLength, charIndex);
        if (codepoint == 0)
          break;

        const avk::Glyph &glyph = font.getGlyph(codepoint);

        float posX = std::round(cursorX + glyph.bearing.x);
        float posY = std::round(cursorY + (font.getAscent() - glyph.bearing.y) +
                                textOffset);
        float posW = std::round(glyph.size.x);
        float posH = std::round(glyph.size.y);

        glm::vec4 transformedGlyph = getPivotTransformedCoords(
            Clay_BoundingBox{posX, posY, posW, posH}, elementScale,
            elementRotation, transformOrigin, translate);

        avk::InstanceData instance{};
        instance.rectXYWH = glm::vec4(
            std::round(transformedGlyph.x), std::round(transformedGlyph.y),
            std::round(transformedGlyph.z), std::round(transformedGlyph.w));
        instance.borderRadius = glm::vec4(0.0f);
        instance.fillColorA = textColor;
        instance.uvBounds = glyph.uvBounds;
        instance.clipRect = activeClipRect;

        instance.shapeType = 0;
        instance.fillType = 3;
        instance.textureIndex = font.getTextureIndex();
        instance.strokeThickness = 0.0f;
        instance.blur = 0.0f;
        instance.scale = elementScale;
        instance.rotation = elementRotation;

        g_uiState->renderer->submit(instance);

        cursorX += glyph.advance;
      }
    }
  }

  session->canvas->endFrame(*g_uiState->renderer);

  g_uiState->framePayloads.clear();

  g_uiState->capturedChars.clear();
  g_uiState->backspacePressed = false;
  g_uiState->enterPressed = false;
  g_uiState->anyInputBoxHovered = false;
  g_uiState->deletePressed = false;
  g_uiState->leftArrowPressed = false;
  g_uiState->rightArrowPressed = false;
  g_uiState->pointerPressed = false;
  g_uiState->mouseWheelDeltaX = 0.0f;
  g_uiState->mouseWheelDeltaY = 0.0f;
}

void resizeWindow(VeraWindow *window, uint32_t width, uint32_t height) {
  if (!g_uiState)
    return;
  window::WindowSession *session = g_uiState->findSession(window);
  if (session) {
    float scale = window->getCurrentMonitor().dpiScale;
    int32_t intScale = static_cast<int32_t>(std::ceil(scale));
    if (intScale < 1) {
      intScale = 1;
    }

    session->canvas->resize(width * intScale, height * intScale);
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
  return g_uiState->context->getTextureManager()->loadTexture(getPath(path));
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
