#pragma once

#include "avk/avk_textLayout.h"
#include <ft2build.h>
#include FT_FREETYPE_H

#include "avk/avk_allocator.h"
#include "avk/avk_renderer.h"
#include "clay.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb.h>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace avk {

struct GlyphMetrics {
  uint32_t codepoint{0};
  float advance{0.0f};
  float planeLeft{0.0f}, planeBottom{0.0f}, planeRight{0.0f}, planeTop{0.0f};
  float atlasLeft{0.0f}, atlasBottom{0.0f}, atlasRight{0.0f}, atlasTop{0.0f};
};

struct loadFontConfig {
  const char *ttfPath;
  const char *csvPath;
  GpuAllocator *allocator;
  uint32_t fontTextureSlot;

  float pixelSize = 32.0f;
  uint32_t fontAtlasWidth = 512;
  uint32_t fontAtlasHeight = 512;
};

class Font {
public:
  Font() = default;
  ~Font();

  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;
  Font(Font &&) noexcept = default;
  Font &operator=(Font &&) noexcept = default;

  bool loadFromFile(loadFontConfig &config);

  bool loadEmojiGlyph(uint32_t glyphIndex, std::vector<uint8_t> &outPixels,
                      uint32_t &outWidth, uint32_t &outHeight);

  std::vector<avk::InstanceData>
  layoutText(std::string_view text, glm::vec2 position,
             const Clay_BoundingBox &box, const glm::vec4 &color,
             float fontSize, float letterSpacing, float fontWeight,
             const glm::vec4 &clipRect, float scale, float rotation,
             const glm::vec2 &transformOrigin, const glm::vec2 &translate,
             float lineHeight, avk::TextWrapMode wrapMode,
             avk::TextAlignMode alignMode);

  [[nodiscard]] glm::vec2
  measureText(std::string_view text, float fontSize = 0.0f,
              float maxWidth = 0.0f,
              avk::TextWrapMode wrapMode = avk::TextWrapMode::Word,
              float lineHeight = 0.0f,
              avk::TextAlignMode alignMode = avk::TextAlignMode::Left) const;

  [[nodiscard]] float getLineHeight(float fontSize = 0.0f) const;
  [[nodiscard]] float getAscent(float fontSize = 0.0f) const;

  static std::string resolveSystemFontPath(std::string_view fontName);

  [[nodiscard]] hb_font_t *getHbFont() const { return m_hbFont; }
  [[nodiscard]] uint32_t getEmojiTextureSlot() const {
    return m_emojiTextureSlot;
  }
  [[nodiscard]] uint32_t getFontTextureSlot() const {
    return m_fontTextureSlot;
  }
  [[nodiscard]] float getFontSize() const { return m_pixelSize; }

private:
  bool loadMetricsCsv(const char *csvPath);
  bool isEmojiGlyph(uint32_t glyphIndex);
  glm::vec4 allocateAndUploadEmoji(uint32_t glyphIndex,
                                   const std::vector<uint8_t> &pixels,
                                   uint32_t width, uint32_t height);

  FT_Face m_ftFace{nullptr};
  FT_Face m_ftEmojiFace;
  hb_font_t *m_hbFont{nullptr};
  float m_pixelSize{32.0f};

  // ✅ SEPARATE MAPS TO PREVENT KEY COLLISION CLOBBERING
  std::unordered_map<uint32_t, GlyphMetrics>
      m_glyphIndexMetricsMap; // Keyed by FreeType Glyph Index
  std::unordered_map<uint32_t, GlyphMetrics>
      m_codepointMetricsMap; // Keyed by Unicode Codepoint

  std::unordered_map<uint32_t, glm::vec4> m_emojiUvMap;

  GpuAllocator *m_allocator{nullptr};
  AllocatedImage m_emojiAtlasImage{};
  VkImageLayout m_emojiAtlasLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageView m_emojiImageView{VK_NULL_HANDLE};

  uint32_t m_fontTextureSlot{0};
  uint32_t m_emojiTextureSlot{0};

  uint32_t m_fontAtlasWidth{1024};
  uint32_t m_fontAtlasHeight{1024};

  uint32_t m_emojiAtlasWidth{1024};
  uint32_t m_emojiAtlasHeight{1024};
  uint32_t m_shelfX{0};
  uint32_t m_shelfY{0};
  uint32_t m_rowHeight{0};

  bool m_isIconFont;
  static constexpr uint32_t m_atlasPadding{2};
};

} // namespace avk
