#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#ifndef AVK_ASSETS_DIR
#define AVK_ASSETS_DIR "assets"
#endif

namespace avk {
class Font;
}

namespace atomic {

/**
 * @brief Resolves a path relative to assets/ folder unless already absolute.
 */
inline std::string getPath(const std::string &relativePath) {
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

/**
 * @brief 3-Parameter Overload: Explicit TTF font file + MSDF atlas PNG + CSV
 * metrics.
 */
uint32_t loadFont(const std::string &ttfPath, const std::string &atlasImagePath,
                  const std::string &metricsCsvPath);

/**
 * @brief 2-Parameter Overload: Auto-deduces TTF font file from MSDF atlas name.
 */
uint32_t loadFont(const std::string &atlasImagePath,
                  const std::string &metricsCsvPath);

/**
 * @brief Loads an MSDF vector font directly from in-memory PNG bytes and
 * metrics string.
 */
uint32_t loadFontFromMemory(std::span<const uint8_t> atlasPngBytes,
                            const std::string &metricsCsvContent);

/**
 * @brief Resolves and loads an OS system font by name.
 */
uint32_t loadSystemFont(const std::string &fontName, uint32_t fontSize = 16);

/**
 * @brief Loads a TrueType font file at a specific pixel size.
 */
uint32_t loadFont(const std::string &path, uint32_t fontSize);

/**
 * @brief Loads a TrueType font file with explicit unicode codepoint targets.
 */
uint32_t loadFont(const std::string &path, uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints);

/**
 * @brief Loads an image texture into bindless descriptor slot memory.
 */
uint32_t loadTexture(const std::string &path);

/**
 * @brief Unloads a texture slot from descriptor memory.
 */
void unloadTexture(uint32_t textureIndex);

/**
 * @brief Fetches raw font instance by font ID.
 */
avk::Font *getFont(uint32_t fontId);

/**
 * @brief Returns the default system font ID.
 */
uint32_t getDefaultFontId();

/**
 * @brief Finds closest icon font size tier for vector atlas rendering.
 */
uint32_t getClosestIconFontId(float requestedSize);

/**
 * @brief Returns true if any UI element currently holds keyboard focus.
 */
bool isKeyboardCaptured();

/**
 * @brief Clears global keyboard focus.
 */
void clearKeyboardFocus();

} // namespace atomic
