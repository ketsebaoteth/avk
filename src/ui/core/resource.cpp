#include "avk/utils/ui/layout.h"
#include "ui/core/resources.h"
#include "ui/internal/context.h"
#include <iostream>

namespace atomic {

/** @brief Loads an MSDF vector font from a generated atlas image path and
 * metrics CSV path. */
uint32_t loadFont(const std::string &atlasImagePath,
                  const std::string &metricsCsvPath) {
  auto uiState = getUiState();
  if (!uiState)
    return 0;

  auto font = std::make_unique<avk::Font>(uiState->context.get(),
                                          atlasImagePath, metricsCsvPath);
  uiState->fonts.push_back(std::move(font));

  return static_cast<uint32_t>(uiState->fonts.size() - 1);
}

/** @brief Loads an MSDF vector font directly from in-memory PNG bytes and
 * metrics string. */
uint32_t loadFontFromMemory(std::span<const uint8_t> atlasPngBytes,
                            const std::string &metricsCsvContent) {
  auto uiState = getUiState();
  if (!uiState)
    return 0;

  auto font = std::make_unique<avk::Font>(uiState->context.get(), atlasPngBytes,
                                          metricsCsvContent);
  uiState->fonts.push_back(std::move(font));

  return static_cast<uint32_t>(uiState->fonts.size() - 1);
}

/** @brief Resolves and loads an OS system font by name. */
uint32_t loadSystemFont(const std::string &fontName, uint32_t fontSize) {
  auto uiState = getUiState();
  if (!uiState)
    return 0;

  std::string fontPath = avk::Font::resolveSystemFontPath(fontName);
  if (fontPath.empty()) {
    std::cerr << "atomic: Failed to resolve OS system font: " << fontName
              << std::endl;
    return 0;
  }

  return loadFont(fontPath, fontSize);
}

/** @brief Direct TTF file loader. */
uint32_t loadFont(const std::string &path, uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints) {
  auto uiState = getUiState();
  if (!uiState)
    return 0;

  auto font = std::make_unique<avk::Font>(uiState->context.get(), getPath(path),
                                          fontSize, codepoints);
  uiState->fonts.push_back(std::move(font));

  return static_cast<uint32_t>(uiState->fonts.size() - 1);
}

uint32_t loadFont(const std::string &path, uint32_t fontSize) {
  return loadFont(path, fontSize, {});
}

uint32_t loadTexture(const std::string &path) {
  auto uiState = getUiState();
  if (!uiState)
    return 0;
  return uiState->context->getTextureManager()->loadTexture(getPath(path));
}

void unloadTexture(uint32_t textureIndex) {
  auto uiState = getUiState();
  if (!uiState)
    return;
  uiState->context->getTextureManager()->unloadTexture(textureIndex);
}

avk::Font *getFont(uint32_t fontId) {
  auto uiState = getUiState();
  if (!uiState || fontId >= uiState->fonts.size()) {
    return nullptr;
  }
  return uiState->fonts[fontId].get();
}

bool isKeyboardCaptured() {
  auto uiState = getUiState();
  return uiState && uiState->focusedElementId != 0;
}

void clearKeyboardFocus() {
  auto uiState = getUiState();
  if (uiState) {
    uiState->focusedElementId = 0;
  }
}

} // namespace atomic
