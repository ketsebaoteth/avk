#pragma once

#include "avk_allocator.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <volk.h>

typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;

namespace avk {

class VulkanContext;

struct Glyph {
  glm::vec2 size;     // Pixel dimensions of the glyph
  glm::vec2 bearing;  // Offset from baseline to top-left of the glyph
  float advance;      // Horizontal advance to the next character
  glm::vec4 uvBounds; // [uMin, vMin, uMax, vMax] inside the atlas
};

/**
 * @brief RAII Font Loader and Rasterizer using FreeType.
 * Generates a sharp, point-filtered R8_UNORM bitmap texture atlas at startup.
 */
class Font {
public:
  Font(VulkanContext *context, const std::string &filePath, uint32_t fontSize,
       const std::vector<uint32_t> &codepoints = {});
  ~Font();

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

  Font(Font &&other) noexcept;
  Font &operator=(Font &&other) noexcept;

  /**
   * @brief Measures the bounding width and height of a string in pixels.
   */
  glm::vec2 measureText(const std::string &text) const;

  uint32_t getTextureIndex() const { return m_textureIndex; }
  float getLineHeight() const { return m_lineHeight; }
  float getAscent() const { return m_ascent; }
  uint32_t getFontSize() const { return m_fontSize; }

  const Glyph &getGlyph(uint32_t codepoint) const {
    auto it = m_glyphs.find(codepoint);
    if (it != m_glyphs.end()) {
      return it->second;
    }
    return m_glyphs.empty() ? m_fallbackGlyph : m_glyphs.begin()->second;
  }

private:
  void release();
  bool buildAtlas(const std::string &filePath, uint32_t fontSize,
                  const std::vector<uint32_t> &codepoints);

  VulkanContext *m_context = nullptr;
  AllocatedImage m_atlasImage;
  VkImageView m_atlasView = VK_NULL_HANDLE;
  uint32_t m_textureIndex = 0;

  float m_lineHeight = 0.0f;
  float m_ascent = 0.0f;
  uint32_t m_fontSize = 0;

  std::unordered_map<uint32_t, Glyph> m_glyphs;
  Glyph m_fallbackGlyph{};
};

} // namespace avk
