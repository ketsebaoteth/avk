#include "ui/core/frame.h"
#include "Vera/src/vera_windowing/core/app/Types.h"
#include "avk/avk_font.h"
#include "avk/avk_textLayout.h"
#include "tracy/Tracy.hpp"
#include "ui/core/gradientAtlas.h"
#include "ui/devtools/Devtools.h"
#include "ui/internal/context.h"
#include "ui/style/style.h"
#include "ui/utils/clayUtils.h"
#include "ui/utils/coreUtils.h"

#include <cstdint>
#include <vector>

namespace atomic {

/** @brief Begins a new UI frame pass for a window. */
bool beginFrame(VeraWindow *window) {
  FrameMark;
  ZoneScopedN("UI_BeginFrame");

  auto uiState = getUiState();
  if (!uiState || window == nullptr)
    return false;

  window::WindowSession *session = nullptr;
  {
    ZoneScopedN("UI_FindSession");
    session = uiState->findSession(window);
  }

  if (session == nullptr || !session->canvas->isActive())
    return false;

  {
    ZoneScopedN("UI_State_Reset");
    uiState->anyInputBoxHovered = false;

    {
      ZoneScopedN("UI_ClearMaps");
      uiState->computedStyleMap.clear();
      uiState->positioningContextStack.clear();
      uiState->cascadingStyleStack.clear();
      uiState->stringArena.clear();

      uiState->previousLifecycleMap = std::move(uiState->currentLifecycleMap);
      uiState->currentLifecycleMap.clear();
    }

    uiState->previousFocusedElementId = uiState->focusedElementId;

    {
      ZoneScopedN("UI_CalcDeltaTime");
      setClayCursorState(uiState->pointerPos, uiState->pointerDown);
      calcFrameDeltaTime(session);

      /**
       * @brief Frame Data Metrics Calculation (FPS & Frame Time ms)
       */
      if (session->lastDeltaTime > 0.00001f) {
        float rawFrameTime = session->lastDeltaTime * 1000.0f;
        float rawFps = 1.0f / session->lastDeltaTime;

        if (uiState->fps <= 0.0f) {
          uiState->frameTimeMs = rawFrameTime;
          uiState->fps = rawFps;
        } else {
          uiState->frameTimeMs =
              uiState->frameTimeMs * 0.9f + rawFrameTime * 0.1f;
          uiState->fps = uiState->fps * 0.9f + rawFps * 0.1f;
        }
      }
    }
  }

  {
    ZoneScopedN("MotionManager_Tick");
    uiState->motionManager.tick(session->lastDeltaTime);
  }

  {
    ZoneScopedN("Clay_Layout_Setup");
    resetGlobalIdCounter();

    auto state = window->getState();
    setClayDimensions(state);
    Clay_SetMeasureTextFunction(measureTextCallback, nullptr);

    {
      ZoneScopedN("Clay_BeginLayout");
      Clay_BeginLayout();
    }

    if (uiState->injectDevTools) {
      ZoneScopedN("UI_DrawDevTools");
      drawDevToolsDock(window);
    }
  }

  // --------------------------------------------------------------------------
  // Vulkan Swapchain VSync / Fence Wait (CPU idle time)
  // --------------------------------------------------------------------------
  bool val = false;
  {
    ZoneScopedN("Vulkan_Canvas_BeginFrame");
    val = session->canvas->beginFrame();
  }

  return val;
}

/** @brief Ends layout evaluation and submits rendering instances to GPU. */
void endFrame(VeraWindow *window) {
  ZoneScopedN("UI_EndFrame");
  auto uiState = getUiState();
  if (!uiState)
    return;

  window::WindowSession *session = uiState->findSession(window);
  if (!session)
    return;

  if (uiState->pointerPressed && !uiState->anyInputBoxHovered) {
    uiState->focusedElementId = 0;
  }

  Clay_RenderCommandArray renderCommands;
  {
    ZoneScopedN("UI_ClayEndLayout");
    renderCommands = Clay_EndLayout(session->lastDeltaTime);
  }

  {
    ZoneScopedN("UI_RendererBegin");
    uiState->renderer->begin();
  }

  std::vector<Clay_BoundingBox> clipStack;
  [[maybe_unused]] Clay_BoundingBox *currentClip = nullptr;
  uint32_t currentFrameDrawCalls = 0; // Submitted instance counter

  // ⚡ Fast-path pivot transform when scale=1, rotation=0, translate=0
  auto getPivotTransformedCoords = [](const Clay_BoundingBox &box, float scale,
                                      float rotation, const glm::vec2 &origin,
                                      const glm::vec2 &translate) -> glm::vec4 {
    if (rotation == 0.0f && scale == 1.0f && translate.x == 0.0f &&
        translate.y == 0.0f) {
      return glm::vec4(box.x, box.y, box.width, box.height);
    }

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

  {
    ZoneScopedN("UI_ProcessRenderCommands");

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
      bool hasActiveClip = !clipStack.empty();

      if (hasActiveClip) {
        const auto &clip = clipStack.back();
        activeClipRect = glm::vec4(std::round(clip.x), std::round(clip.y),
                                   std::round(clip.x + clip.width),
                                   std::round(clip.y + clip.height));
      } else {
        activeClipRect = glm::vec4(-10000.0f, -10000.0f, 200000.0f, 200000.0f);
      }

      if (hasActiveClip) {
        constexpr float CULL_MARGIN = 64.0f;
        const auto &box = cmd->boundingBox;

        if (box.x + box.width + CULL_MARGIN < activeClipRect.x ||
            box.x - CULL_MARGIN > activeClipRect.z ||
            box.y + box.height + CULL_MARGIN < activeClipRect.y ||
            box.y - CULL_MARGIN > activeClipRect.w) {
          continue; // Cull off-screen element instantly!
        }
      }

      if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
        ZoneScopedN("UI_RenderCmd_Rectangle");
        Clay_RectangleRenderData *rectData = &cmd->renderData.rectangle;

        float elementScale = 1.0f;
        float elementRotation = 0.0f;
        float elementBlur = 0.0f;
        glm::vec2 transformOrigin(0.5f, 0.5f);
        glm::vec2 translate(0.0f, 0.0f);
        std::vector<BoxShadow> shadows;
        std::optional<atomic::Gradient> elementGradient;

        if (cmd->userData != nullptr) {
          auto *payload = static_cast<RenderPayload *>(cmd->userData);
          elementScale = payload->scale;
          elementRotation = payload->rotation;
          elementBlur = payload->blur;
          transformOrigin = payload->transformOrigin;
          translate = payload->translate;
          shadows = payload->boxShadows;
          elementGradient = payload->gradient;
        }

        // ⚡ CPU CULLING: Skip completely transparent rectangles without
        // gradients or shadows
        if (rectData->backgroundColor.a == 0 && !elementGradient.has_value() &&
            shadows.empty()) {
          continue;
        }

        auto submitShadow = [&](const BoxShadow &s) {
          float expand =
              s.inset ? 0.0f : ((s.blur * 2.5f) + std::max(s.spread, 0.0f));

          Clay_BoundingBox expandedBox = cmd->boundingBox;
          expandedBox.x -= expand;
          expandedBox.y -= expand;
          expandedBox.width += expand * 2.0f;
          expandedBox.height += expand * 2.0f;

          glm::vec4 shadowRect = getPivotTransformedCoords(
              expandedBox, elementScale, elementRotation, transformOrigin,
              translate + s.offset);

          avk::InstanceData instance{};
          instance.rectXYWH =
              glm::vec4(std::round(shadowRect.x), std::round(shadowRect.y),
                        std::round(shadowRect.z), std::round(shadowRect.w));

          instance.fillColorA = s.color;
          instance.fillColorB =
              glm::vec4(s.offset.x, s.offset.y, s.spread, expand);

          instance.borderRadius = glm::vec4(rectData->cornerRadius.topLeft,
                                            rectData->cornerRadius.topRight,
                                            rectData->cornerRadius.bottomLeft,
                                            rectData->cornerRadius.bottomRight);

          instance.clipRect = activeClipRect;
          instance.shapeType = 0;
          instance.fillType = s.inset ? 6 : 5;
          instance.blur = s.blur;
          instance.scale = elementScale;
          instance.rotation = elementRotation;

          uiState->renderer->submit(instance);
          currentFrameDrawCalls++;
        };

        for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
          if (!it->inset) {
            submitShadow(*it);
          }
        }

        glm::vec4 transformedRect = getPivotTransformedCoords(
            cmd->boundingBox, elementScale, elementRotation, transformOrigin,
            translate);

        float absoluteRight = transformedRect.x + transformedRect.z;
        float absoluteBottom = transformedRect.y + transformedRect.w;

        float snappedX = std::round(transformedRect.x);
        float snappedY = std::round(transformedRect.y);
        float snappedRight = std::round(absoluteRight);
        float snappedBottom = std::round(absoluteBottom);

        float snappedW = std::max(snappedRight - snappedX, 1.0f);
        float snappedH = std::max(snappedBottom - snappedY, 1.0f);

        avk::InstanceData mainInstance{};
        mainInstance.rectXYWH =
            glm::vec4(snappedX, snappedY, snappedW, snappedH);

        mainInstance.fillColorA =
            glm::vec4(rectData->backgroundColor.r / 255.0f,
                      rectData->backgroundColor.g / 255.0f,
                      rectData->backgroundColor.b / 255.0f,
                      rectData->backgroundColor.a / 255.0f);
        mainInstance.borderRadius = glm::vec4(
            rectData->cornerRadius.topLeft, rectData->cornerRadius.topRight,
            rectData->cornerRadius.bottomLeft,
            rectData->cornerRadius.bottomRight);
        mainInstance.clipRect = activeClipRect;
        mainInstance.shapeType = 0;
        mainInstance.fillType = 0;

        // ⚡ COMMAND MERGING: Merge next Clay BORDER command into mainInstance
        if (i + 1 < renderCommands.length) {
          Clay_RenderCommand *nextCmd =
              Clay_RenderCommandArray_Get(&renderCommands, i + 1);
          if (nextCmd->commandType == CLAY_RENDER_COMMAND_TYPE_BORDER) {
            Clay_BorderRenderData *bData = &nextCmd->renderData.border;
            if (bData->width.top > 0 || bData->width.right > 0 ||
                bData->width.bottom > 0 || bData->width.left > 0) {

              mainInstance.strokeThickness =
                  glm::vec4(bData->width.top, bData->width.right,
                            bData->width.bottom, bData->width.left);
              mainInstance.strokeColor =
                  glm::vec4(bData->color.r / 255.0f, bData->color.g / 255.0f,
                            bData->color.b / 255.0f, bData->color.a / 255.0f);

              // Consume nextCmd so it isn't rendered as a 2nd quad!
              i++;
            }
          }
        }

        if (elementGradient.has_value() &&
            elementGradient->type != GradientType::Disabled) {
          const auto &g = elementGradient.value();

          if (g.stops.size() > 2) {
            uint32_t gradTexIdx =
                atomic::GradientAtlasManager::instance()
                    .getOrCreateGradientTexture(uiState->context.get(), g);

            mainInstance.fillType = 8;
            mainInstance.textureIndex = gradTexIdx;

            if (g.type == atomic::GradientType::Linear) {
              float rad = glm::radians(g.angleDegrees - 90.0f);
              glm::vec2 dir(std::cos(rad), std::sin(rad));
              mainInstance.gradientStart = glm::vec2(0.5f) - dir * 0.5f;
              mainInstance.gradientEnd = glm::vec2(0.5f) + dir * 0.5f;
            } else if (g.type == atomic::GradientType::Radial) {
              mainInstance.gradientStart = g.center;
              mainInstance.gradientEnd = g.center + g.radius;
            }
          } else if (g.stops.size() == 2) {
            if (g.type == atomic::GradientType::Linear) {
              mainInstance.fillType = 1;
              float rad = glm::radians(g.angleDegrees - 90.0f);
              glm::vec2 dir(std::cos(rad), std::sin(rad));
              mainInstance.gradientStart = glm::vec2(0.5f) - dir * 0.5f;
              mainInstance.gradientEnd = glm::vec2(0.5f) + dir * 0.5f;
            } else if (g.type == atomic::GradientType::Radial) {
              mainInstance.fillType = 2;
              mainInstance.gradientStart = g.center;
              mainInstance.gradientEnd = g.center + g.radius;
            }

            mainInstance.fillColorA = g.stops[0].color;
            mainInstance.fillColorB = g.stops[1].color;
          }
        }

        mainInstance.blur = elementBlur;
        mainInstance.scale = elementScale;
        mainInstance.rotation = elementRotation;

        uiState->renderer->submit(mainInstance);
        currentFrameDrawCalls++;

        for (const auto &s : shadows) {
          if (s.inset) {
            submitShadow(s);
          }
        }
      } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_BORDER) {
        ZoneScopedN("UI_RenderCmd_Border");
        Clay_BorderRenderData *borderData = &cmd->renderData.border;

        // ⚡ CPU CULLING: Skip 0-width or 0-alpha borders
        if ((borderData->width.top == 0 && borderData->width.right == 0 &&
             borderData->width.bottom == 0 && borderData->width.left == 0) ||
            borderData->color.a == 0) {
          continue;
        }

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

        instance.strokeThickness = {
            borderData->width.top, borderData->width.right,
            borderData->width.bottom, borderData->width.left};

        instance.strokeColor = glm::vec4(
            borderData->color.r / 255.0f, borderData->color.g / 255.0f,
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

        uiState->renderer->submit(instance);
        currentFrameDrawCalls++;
      } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE) {
        ZoneScopedN("UI_RenderCmd_Image");
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
        std::vector<BoxShadow> shadows;

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
          shadows = payload->boxShadows;
        }

        auto submitImageShadow = [&](const BoxShadow &s) {
          float expand =
              s.inset ? 0.0f : ((s.blur * 2.5f) + std::max(s.spread, 0.0f));

          Clay_BoundingBox expandedBox = cmd->boundingBox;
          expandedBox.x -= expand;
          expandedBox.y -= expand;
          expandedBox.width += expand * 2.0f;
          expandedBox.height += expand * 2.0f;

          glm::vec4 shadowRect = getPivotTransformedCoords(
              expandedBox, elementScale, elementRotation, transformOrigin,
              translate + s.offset);

          avk::InstanceData shadowInstance{};
          shadowInstance.rectXYWH =
              glm::vec4(std::round(shadowRect.x), std::round(shadowRect.y),
                        std::round(shadowRect.z), std::round(shadowRect.w));

          shadowInstance.fillColorA = s.color;
          shadowInstance.fillColorB =
              glm::vec4(s.offset.x, s.offset.y, s.spread, expand);
          shadowInstance.borderRadius = glm::vec4(
              imageData->cornerRadius.topLeft, imageData->cornerRadius.topRight,
              imageData->cornerRadius.bottomLeft,
              imageData->cornerRadius.bottomRight);
          shadowInstance.clipRect = activeClipRect;
          shadowInstance.shapeType = 0;
          shadowInstance.fillType = s.inset ? 6 : 5;
          shadowInstance.blur = s.blur;
          shadowInstance.scale = elementScale;
          shadowInstance.rotation = elementRotation;

          uiState->renderer->submit(shadowInstance);
          currentFrameDrawCalls++;
        };

        for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
          if (!it->inset) {
            submitImageShadow(*it);
          }
        }

        glm::vec4 transformedRect = getPivotTransformedCoords(
            cmd->boundingBox, elementScale, elementRotation, transformOrigin,
            translate);

        glm::vec4 finalUvBounds = uvBounds;

        if (objectFit != ObjectFit::Fill && objectFit != ObjectFit::Custom) {
          VkExtent2D texExt =
              uiState->context->getTextureManager()->getTextureExtent(
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
        instance.strokeThickness = glm::vec4(0.0f);
        instance.blur = elementBlur;
        instance.scale = elementScale;
        instance.rotation = elementRotation;

        uiState->renderer->submit(instance);
        currentFrameDrawCalls++;

        for (const auto &s : shadows) {
          if (s.inset) {
            submitImageShadow(s);
          }
        }
      } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
        ZoneScopedN("UI_RenderCmd_Text");
        Clay_TextRenderData *textData = &cmd->renderData.text;

        if (textData->fontId >= uiState->fonts.size()) {
          continue;
        }

        float fontSize = static_cast<float>(textData->fontSize);
        float elementScale = 1.0f;
        float elementRotation = 0.0f;
        float letterSpacing = 0.0f;
        float fontWeight = 400.0f;
        float lineHeight = 0.0f;
        avk::TextWrapMode wrapMode = avk::TextWrapMode::Word;
        avk::TextAlignMode alignMode = avk::TextAlignMode::Left;

        glm::vec2 transformOrigin(0.5f, 0.5f);
        glm::vec2 translate(0.0f, 0.0f);
        Clay_BoundingBox renderBox = cmd->boundingBox;

        if (cmd->userData != nullptr) {
          auto *payload = static_cast<RenderPayload *>(cmd->userData);
          elementScale = payload->scale;
          elementRotation = payload->rotation;
          transformOrigin = payload->transformOrigin;
          translate = payload->translate;
          letterSpacing = payload->letterSpacing;
          fontWeight = payload->fontWeight;
          lineHeight = payload->lineHeight;

          if (payload->textWrap.has_value()) {
            switch (payload->textWrap.value()) {
            case atomic::TextWrap::Anywhere:
              wrapMode = avk::TextWrapMode::Anywhere;
              break;
            case atomic::TextWrap::Disabled:
              wrapMode = avk::TextWrapMode::Disabled;
              break;
            default:
              wrapMode = avk::TextWrapMode::Word;
              break;
            }
          }

          if (payload->textAlign.has_value()) {
            switch (payload->textAlign.value()) {
            case atomic::TextAlign::Center:
              alignMode = avk::TextAlignMode::Center;
              break;
            case atomic::TextAlign::Right:
              alignMode = avk::TextAlignMode::Right;
              break;
            default:
              alignMode = avk::TextAlignMode::Left;
              break;
            }
          }
        }

        avk::Font &font = *uiState->fonts[textData->fontId];
        std::string textStr(textData->stringContents.chars,
                            textData->stringContents.length);

        glm::vec4 textColor(
            textData->textColor.r / 255.0f, textData->textColor.g / 255.0f,
            textData->textColor.b / 255.0f, textData->textColor.a / 255.0f);

        glm::vec2 position(renderBox.x, renderBox.y);

        auto instances = font.layoutText(
            textStr, position, renderBox, textColor, fontSize, letterSpacing,
            fontWeight, activeClipRect, elementScale, elementRotation,
            transformOrigin, translate, lineHeight, wrapMode, alignMode);

        for (const auto &instance : instances) {
          uiState->renderer->submit(instance);
          currentFrameDrawCalls++;
        }
      }
    }
  }

  {
    ZoneScopedN("UI_CanvasEndFrame");
    session->canvas->endFrame(*uiState->renderer);
  }

  {
    ZoneScopedN("UI_StateCleanup");
    uiState->drawCalls = currentFrameDrawCalls;
    uiState->framePayloads.clear();
    uiState->motionManager.gc();

    uiState->capturedChars.clear();
    uiState->backspacePressed = false;
    uiState->enterPressed = false;
    uiState->cutTriggered = false;
    uiState->pasteTriggered = false;
    uiState->copyTriggered = false;
    uiState->anyInputBoxHovered = false;
    uiState->deletePressed = false;
    uiState->leftArrowPressed = false;
    uiState->rightArrowPressed = false;
    uiState->pointerPressed = false;
    uiState->mouseWheelDeltaX = 0.0f;
    uiState->mouseWheelDeltaY = 0.0f;
  }
}

void resizeWindow(VeraWindow *window, uint32_t width, uint32_t height) {
  auto uiState = getUiState();
  if (!uiState)
    return;
  window::WindowSession *session = uiState->findSession(window);
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
  auto uiState = getUiState();
  if (!uiState)
    return 0;
  window::WindowSession *session = uiState->findSession(window);
  return session ? session->canvas->getWidth() : 0;
}

uint32_t getHeight(VeraWindow *window) {
  auto uiState = getUiState();
  if (!uiState)
    return 0;
  window::WindowSession *session = uiState->findSession(window);
  return session ? session->canvas->getHeight() : 0;
}

} // namespace atomic
