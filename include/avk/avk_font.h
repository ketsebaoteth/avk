#pragma once

#include "avk_allocator.h"
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <volk.h>

typedef struct hb_font_t hb_font_t;

namespace avk {

class VulkanContext;

/**
 * @brief Glyph layout metrics and UV coordinates inside the font atlas.
 */
struct Glyph {
  glm::vec2 size;     // Pixel dimensions of the glyph
  glm::vec2 bearing;  // Offset from baseline to top-left of the glyph
  float advance;      // Horizontal advance to the next character
  glm::vec4 uvBounds; // [uMin, vMin, uMax, vMax] inside the atlas
};

/**
 * @brief RAII Font Loader supporting MSDF Vector Atlases, Memory Byte Buffers,
 * and OS System Fonts.
 */
class Font {
public:
  /// MSDF Vector Font Constructor (Atlas PNG Path + CSV Metrics Path)
  Font(VulkanContext *context, const std::string &atlasImagePath,
       const std::string &metricsCsvPath);

  /// In-Memory MSDF Vector Font Constructor (PNG Bytes + CSV String)
  Font(VulkanContext *context, std::span<const uint8_t> atlasPngBytes,
       const std::string &metricsCsvContent);

  /// Direct TTF File / Fallback Font Constructor
  Font(VulkanContext *context, const std::string &filePath, uint32_t fontSize,
       const std::vector<uint32_t> &codepoints = {});

  ~Font();

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

  Font(Font &&other) noexcept;
  Font &operator=(Font &&other) noexcept;

  /**
   * @brief Resolves an OS system font name (e.g. "Segoe UI", "San Francisco",
   * "Arial") to a disk path.
   */
  static std::string resolveSystemFontPath(const std::string &fontName);

  /**
   * @brief Measures the bounding width and height of a string in pixels.
   */
  [[nodiscard]] glm::vec2 measureText(const std::string &text,
                                      float fontSize) const;

  [[nodiscard]] uint32_t getTextureIndex() const { return m_textureIndex; }

  /// Returns scaled line height at target fontSize (or raw m_lineHeight if
  /// fontSize == 0)
  [[nodiscard]] float getLineHeight(float fontSize = 0.0f) const {
    float baseSize = (m_fontSize > 0) ? static_cast<float>(m_fontSize) : 32.0f;
    float scale = (fontSize > 0.0f) ? (fontSize / baseSize) : 1.0f;
    return m_lineHeight * scale;
  }

  /// Returns scaled cap ascent at target fontSize (or raw m_ascent if fontSize
  /// == 0)
  [[nodiscard]] float getAscent(float fontSize = 0.0f) const {
    float baseSize = (m_fontSize > 0) ? static_cast<float>(m_fontSize) : 32.0f;
    float scale = (fontSize > 0.0f) ? (fontSize / baseSize) : 1.0f;
    return m_ascent * scale;
  }

  [[nodiscard]] uint32_t getFontSize() const { return m_fontSize; }

  [[nodiscard]] const Glyph &getGlyph(uint32_t codepoint) const {
    auto it = m_glyphs.find(codepoint);
    if (it != m_glyphs.end()) {
      return it->second;
    }
    return m_glyphs.empty() ? m_fallbackGlyph : m_glyphs.begin()->second;
  }

private:
  void release();
  bool buildAtlasFromMemory(std::span<const uint8_t> fontBytes,
                            uint32_t fontSize,
                            const std::vector<uint32_t> &codepoints);
  bool parseMetricsCsv(const std::string &csvContent, uint32_t atlasWidth,
                       uint32_t atlasHeight);

  VulkanContext *m_context = nullptr;
  AllocatedImage m_atlasImage;
  VkImageView m_atlasView = VK_NULL_HANDLE;
  uint32_t m_textureIndex = 0;

  float m_lineHeight = 24.0f;
  float m_ascent = 18.0f;
  uint32_t m_fontSize = 16;

  std::unordered_map<uint32_t, Glyph> m_glyphs;
  Glyph m_fallbackGlyph{};
  hb_font_t *m_hbFont = nullptr;
  std::vector<uint8_t> m_retainedFontBuffer;
};

} // namespace avk
