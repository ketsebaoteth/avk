#include "ui/core/frame.h"
#include "Vera/src/vera_windowing/core/app/Types.h"
#include "avk/utils/ui/layout.h"
#include "ui/animation/animation.h"
#include "ui/internal/context.h"
#include "ui/utils/clayUtils.h"
#include "ui/utils/coreUtils.h"
#include <cstdint>

namespace atomic {

/** @brief Begins a new UI frame pass for a window. */
bool beginFrame(VeraWindow *window) {
  auto uiState = getUiState();
  if (!uiState || window == nullptr)
    return false;

  window::WindowSession *session = uiState->findSession(window);
  if (session == nullptr || !session->canvas->isActive())
    return false;

  uiState->computedStyleMap.clear();
  uiState->positioningContextStack.clear();
  uiState->cascadingStyleStack.clear();

  setClayCursorState(uiState->pointerPos, uiState->pointerDown);
  calcFrameDeltaTime(session);
  atomic::AnimationManager::instance().update(session->lastDeltaTime);
  resetGlobalIdCounter();

  auto state = window->getState();
  setClayDimensions(state);
  Clay_BeginLayout();

  auto val = session->canvas->beginFrame();
  return val;
}

/** @brief Ends layout evaluation and submits rendering instances to GPU. */
void endFrame(VeraWindow *window) {
  auto uiState = getUiState();
  if (!uiState)
    return;

  window::WindowSession *session = uiState->findSession(window);
  if (!session)
    return;

  if (uiState->pointerPressed && !uiState->anyInputBoxHovered) {
    uiState->focusedElementId = 0;
  }

  Clay_RenderCommandArray renderCommands =
      Clay_EndLayout(session->lastDeltaTime);

  uiState->renderer->begin();

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
      std::vector<BoxShadow> shadows;

      if (cmd->userData != nullptr) {
        auto *payload = static_cast<RenderPayload *>(cmd->userData);
        elementScale = payload->scale;
        elementRotation = payload->rotation;
        elementBlur = payload->blur;
        transformOrigin = payload->transformOrigin;
        translate = payload->translate;
        shadows = payload->boxShadows;
      }

      auto submitShadow = [&](const BoxShadow &s) {
        // 1. Calculate required quad expansion so the soft blur doesn't get cut
        // off
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
        // Pass offset, spread, AND quad expansion amount in fillColorB!
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
      };

      for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
        if (!it->inset) {
          submitShadow(*it);
        }
      }

      glm::vec4 transformedRect = getPivotTransformedCoords(
          cmd->boundingBox, elementScale, elementRotation, transformOrigin,
          translate);

      avk::InstanceData mainInstance{};
      mainInstance.rectXYWH = glm::vec4(
          std::round(transformedRect.x), std::round(transformedRect.y),
          std::round(transformedRect.z), std::round(transformedRect.w));

      mainInstance.fillColorA = glm::vec4(rectData->backgroundColor.r / 255.0f,
                                          rectData->backgroundColor.g / 255.0f,
                                          rectData->backgroundColor.b / 255.0f,
                                          rectData->backgroundColor.a / 255.0f);
      mainInstance.borderRadius = glm::vec4(rectData->cornerRadius.topLeft,
                                            rectData->cornerRadius.topRight,
                                            rectData->cornerRadius.bottomLeft,
                                            rectData->cornerRadius.bottomRight);
      mainInstance.clipRect = activeClipRect;
      mainInstance.shapeType = 0;
      mainInstance.fillType = 0;
      mainInstance.blur = elementBlur;
      mainInstance.scale = elementScale;
      mainInstance.rotation = elementRotation;

      uiState->renderer->submit(mainInstance);

      for (const auto &s : shadows) {
        if (s.inset) {
          submitShadow(s);
        }
      }
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

      uiState->renderer->submit(instance);
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

      for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
        if (!it->inset) {
          glm::vec4 shadowRect = getPivotTransformedCoords(
              cmd->boundingBox, elementScale, elementRotation, transformOrigin,
              translate + it->offset);

          avk::InstanceData shadowInstance{};
          shadowInstance.rectXYWH =
              glm::vec4(std::round(shadowRect.x), std::round(shadowRect.y),
                        std::round(shadowRect.z), std::round(shadowRect.w));

          shadowInstance.fillColorA = it->color;
          shadowInstance.fillColorB =
              glm::vec4(it->offset.x, it->offset.y, it->spread, 0.0f);
          shadowInstance.borderRadius = glm::vec4(
              imageData->cornerRadius.topLeft, imageData->cornerRadius.topRight,
              imageData->cornerRadius.bottomLeft,
              imageData->cornerRadius.bottomRight);
          shadowInstance.clipRect = activeClipRect;
          shadowInstance.shapeType = 0;
          shadowInstance.fillType = 5;
          shadowInstance.blur = it->blur;
          shadowInstance.scale = elementScale;
          shadowInstance.rotation = elementRotation;

          uiState->renderer->submit(shadowInstance);
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
      instance.strokeThickness = 0.0f;
      instance.blur = elementBlur;
      instance.scale = elementScale;
      instance.rotation = elementRotation;

      uiState->renderer->submit(instance);

      for (const auto &s : shadows) {
        if (s.inset) {
          glm::vec4 shadowRect = getPivotTransformedCoords(
              cmd->boundingBox, elementScale, elementRotation, transformOrigin,
              translate + s.offset);

          avk::InstanceData shadowInstance{};
          shadowInstance.rectXYWH =
              glm::vec4(std::round(shadowRect.x), std::round(shadowRect.y),
                        std::round(shadowRect.z), std::round(shadowRect.w));

          shadowInstance.fillColorA = s.color;
          shadowInstance.fillColorB =
              glm::vec4(s.offset.x, s.offset.y, s.spread, 0.0f);
          shadowInstance.borderRadius = glm::vec4(
              imageData->cornerRadius.topLeft, imageData->cornerRadius.topRight,
              imageData->cornerRadius.bottomLeft,
              imageData->cornerRadius.bottomRight);
          shadowInstance.clipRect = activeClipRect;
          shadowInstance.shapeType = 0;
          shadowInstance.fillType = 6;
          shadowInstance.blur = s.blur;
          shadowInstance.scale = elementScale;
          shadowInstance.rotation = elementRotation;

          uiState->renderer->submit(shadowInstance);
        }
      }
    } else if (cmd->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
      Clay_TextRenderData *textData = &cmd->renderData.text;

      if (textData->fontId >= uiState->fonts.size()) {
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

      const avk::Font &font = *uiState->fonts[textData->fontId];
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

        uiState->renderer->submit(instance);

        cursorX += glyph.advance;
      }
    }
  }

  session->canvas->endFrame(*uiState->renderer);

  uiState->framePayloads.clear();

  uiState->capturedChars.clear();
  uiState->backspacePressed = false;
  uiState->enterPressed = false;
  uiState->anyInputBoxHovered = false;
  uiState->deletePressed = false;
  uiState->leftArrowPressed = false;
  uiState->rightArrowPressed = false;
  uiState->pointerPressed = false;
  uiState->mouseWheelDeltaX = 0.0f;
  uiState->mouseWheelDeltaY = 0.0f;
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
