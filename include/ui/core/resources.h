#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#ifndef AVK_ASSETS_DIR
#define AVK_ASSETS_DIR "assets"
#endif

// forward
namespace avk {
class Font;
}

namespace atomic {

/** @brief resolves an path to relative to assets/ folder but ignores if its
 * already global. */
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

/** @brief Loads an image texture into bindless descriptor slot memory. */
uint32_t loadTexture(const std::string &path);

/** @brief Unloads a texture slot from descriptor memory. */
void unloadTexture(uint32_t textureIndex);

/** @brief Fetches raw font instance by font ID. */
avk::Font *getFont(uint32_t fontId);

/** @brief Returns the default system font ID. */
uint32_t getDefaultFontId();

/** @brief Loads a TrueType font file at a specific pixel size. */
uint32_t loadFont(const std::string &path, uint32_t fontSize);

/** @brief Loads a TrueType font file with explicit unicode codepoint targets.
 */
uint32_t loadFont(const std::string &path, uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints);

/** @brief Finds closest icon font size tier for rasterization. */
uint32_t getClosestIconFontId(float requestedSize);

} // namespace atomic
