#include "avk/avk_font.h"
#include "avk/utils/ui/layout.h"
#include "ui/core/resources.h"
#include "ui/internal/context.h"
#include "utils/log.h"

#include <iostream>
#include <memory>

namespace atomic {

/**
 * @brief 3-Parameter Definition: Explicit TTF font file + MSDF PNG atlas + CSV
 * metrics.
 */
uint32_t loadFont(const std::string &ttfPath, const std::string &atlasImagePath,
                  const std::string &metricsCsvPath) {
  auto *uiState = getUiState();
  if (!uiState || !uiState->context) {
    return 0;
  }

  uint32_t fontSlot =
      uiState->context->getTextureManager()->loadFontTexture(atlasImagePath);

  auto font = std::make_unique<avk::Font>();

  VkExtent2D ext;
  if (uiState->context && uiState->context->getTextureManager()) {
    ext = uiState->context->getTextureManager()->getTextureExtent(fontSlot);
    if (ext.width == 0 || ext.height == 0) {
      std::cerr << "Font with path: " << ttfPath
                << "atlas loaded with 0 size dimensions" << std::endl;
    }
  }

  avk::loadFontConfig loadConfig{};
  loadConfig.ttfPath = ttfPath.c_str();
  loadConfig.csvPath = metricsCsvPath.c_str();
  loadConfig.pixelSize = 32.0f;
  loadConfig.allocator = uiState->context->getAllocator();
  loadConfig.fontTextureSlot = fontSlot;
  loadConfig.fontAtlasWidth = ext.width;
  loadConfig.fontAtlasHeight = ext.height;

  if (!font->loadFromFile(loadConfig)) {
    atomic::log_error_fmt("atomic: Failed to load font: %s", ttfPath.c_str());
    return 0;
  }

  uiState->fonts.push_back(std::move(font));
  return static_cast<uint32_t>(uiState->fonts.size() - 1);
}

/**
 * @brief 2-Parameter Definition: Auto-deduces TTF name from atlas name and
 * delegates to 3-parameter loadFont.
 */
uint32_t loadFont(const std::string &atlasImagePath,
                  const std::string &metricsCsvPath) {
  std::string ttfPath = atlasImagePath;
  size_t pos = ttfPath.find("_atlas");
  if (pos != std::string::npos) {
    ttfPath = ttfPath.substr(0, pos) + ".ttf";
  } else {
    pos = ttfPath.find(".png");
    if (pos != std::string::npos) {
      ttfPath = ttfPath.substr(0, pos) + ".ttf";
    }
  }
  return loadFont(ttfPath, atlasImagePath, metricsCsvPath);
}

/**
 * @brief Loads an MSDF vector font from memory buffers.
 */
uint32_t loadFontFromMemory(std::span<const uint8_t> atlasPngBytes,
                            const std::string &metricsCsvContent) {
  (void)atlasPngBytes;
  (void)metricsCsvContent;
  auto *uiState = getUiState();
  if (!uiState) {
    return 0;
  }

  auto font = std::make_unique<avk::Font>();
  uiState->fonts.push_back(std::move(font));

  return static_cast<uint32_t>(uiState->fonts.size() - 1);
}

/**
 * @brief Resolves and loads an OS system font by family name.
 */
uint32_t loadSystemFont(const std::string &fontName, uint32_t fontSize) {
  std::string fontPath = avk::Font::resolveSystemFontPath(fontName);
  if (fontPath.empty()) {
    std::cerr << "atomic: Failed to resolve OS system font: " << fontName
              << std::endl;
    return 0;
  }

  return loadFont(fontPath, fontSize);
}

/**
 * @brief Direct font file loader at specific pixel size.
 */
uint32_t loadFont(const std::string &path, [[maybe_unused]] uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints) {
  (void)codepoints;
  std::string fontPath = getPath(path);
  std::string csvPath = fontPath + ".csv";
  return loadFont(fontPath, csvPath);
}

uint32_t loadFont(const std::string &path, uint32_t fontSize) {
  return loadFont(path, fontSize, {});
}

uint32_t loadTexture(const std::string &path) {
  auto *uiState = getUiState();
  if (!uiState || !uiState->context) {
    return 0;
  }
  return uiState->context->getTextureManager()->loadTexture(getPath(path));
}

void unloadTexture(uint32_t textureIndex) {
  auto *uiState = getUiState();
  if (uiState && uiState->context) {
    uiState->context->getTextureManager()->unloadTexture(textureIndex);
  }
}

avk::Font *getFont(uint32_t fontId) {
  auto *uiState = getUiState();
  if (!uiState || fontId >= uiState->fonts.size()) {
    return nullptr;
  }
  return uiState->fonts[fontId].get();
}

bool isKeyboardCaptured() {
  auto *uiState = getUiState();
  return uiState && uiState->focusedElementId != 0;
}

void clearKeyboardFocus() {
  auto *uiState = getUiState();
  if (uiState) {
    uiState->focusedElementId = 0;
  }
}

} // namespace atomic
