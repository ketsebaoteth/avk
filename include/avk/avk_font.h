#pragma once

#include "avk/avk_allocator.h"
#include "avk/avk_types.h"
#include "avk_bidi.h"
#include "avk_textLayout.h"

#include <ft2build.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include FT_FREETYPE_H
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb.h>

#include <clay.h>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace avk {

struct loadFontConfig {
  const char *ttfPath{nullptr};
  const char *csvPath{nullptr};
  float pixelSize{32.0f};
  class GpuAllocator *allocator{nullptr};
  uint32_t fontTextureSlot{0};
  uint32_t fontAtlasWidth{2048};
  uint32_t fontAtlasHeight{2048};
};

struct GlyphMetrics {
  uint32_t codepoint{0};
  float advance{0.0f};
  float planeLeft{0.0f};
  float planeBottom{0.0f};
  float planeRight{0.0f};
  float planeTop{0.0f};
  float atlasLeft{0.0f};
  float atlasBottom{0.0f};
  float atlasRight{0.0f};
  float atlasTop{0.0f};
};

class Font {
public:
  Font() = default;
  ~Font();
  // ⚡ DISABLE COPYING to prevent shallow copies from freeing FT_Face
  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

  // Enable Move semantics
  Font(Font &&other) noexcept
      : m_pixelSize(other.m_pixelSize), m_allocator(other.m_allocator),
        m_fontTextureSlot(other.m_fontTextureSlot),
        m_fontAtlasWidth(other.m_fontAtlasWidth),
        m_fontAtlasHeight(other.m_fontAtlasHeight),
        m_isIconFont(other.m_isIconFont), m_ftFace(other.m_ftFace),
        m_ftEmojiFace(other.m_ftEmojiFace), m_hbFont(other.m_hbFont),
        m_codepointMetricsMap(std::move(other.m_codepointMetricsMap)),
        m_glyphIndexMetricsMap(std::move(other.m_glyphIndexMetricsMap)),
        m_emojiCache(std::move(other.m_emojiCache)),
        m_emojiAtlasImage(std::move(other.m_emojiAtlasImage)),
        m_emojiImageView(other.m_emojiImageView),
        m_emojiAtlasLayout(other.m_emojiAtlasLayout),
        m_emojiTextureSlot(other.m_emojiTextureSlot),
        m_emojiAtlasWidth(other.m_emojiAtlasWidth),
        m_emojiAtlasHeight(other.m_emojiAtlasHeight), m_shelfX(other.m_shelfX),
        m_shelfY(other.m_shelfY), m_rowHeight(other.m_rowHeight),
        m_atlasPadding(other.m_atlasPadding),
        m_emojiUvMap(std::move(other.m_emojiUvMap)) {
    // Critical: prevent the source from destroying the resources
    other.m_ftFace = nullptr;
    other.m_ftEmojiFace = nullptr;
    other.m_hbFont = nullptr;
    other.m_emojiImageView = VK_NULL_HANDLE;
  }

  Font &operator=(Font &&other) noexcept {
    if (this != &other) {
      // Destroy current resources
      this->~Font();

      // Placement-new move construct
      new (this) Font(std::move(other));
    }
    return *this;
  }
  bool loadFromFile(loadFontConfig &config);

  glm::vec2
  measureText(std::string_view text, float fontSize = 0.0f,
              float maxWidth = 0.0f,
              avk::TextWrapMode wrapMode = avk::TextWrapMode::Word,
              float lineHeight = 0.0f,
              avk::TextAlignMode alignMode = avk::TextAlignMode::Left) const;

  std::vector<avk::InstanceData> layoutText(
      std::string_view text, glm::vec2 position, const Clay_BoundingBox &box,
      const glm::vec4 &color, float fontSize = 0.0f, float letterSpacing = 0.0f,
      float fontWeight = 400.0f, const glm::vec4 &clipRect = glm::vec4(0.0f),
      float scale = 1.0f, float rotation = 0.0f,
      const glm::vec2 &transformOrigin = glm::vec2(0.5f),
      const glm::vec2 &translate = glm::vec2(0.0f), float lineHeight = 0.0f,
      avk::TextWrapMode wrapMode = avk::TextWrapMode::Word,
      avk::TextAlignMode alignMode = avk::TextAlignMode::Left);

  float getLineHeight(float fontSize = 0.0f) const;
  float getAscent(float fontSize = 0.0f) const;
  float getPixelSize() const { return m_pixelSize; }
  float getFontSize() const {
    return m_pixelSize;
  } // ⚡ Added getter alias for components

  static std::string resolveSystemFontPath(std::string_view fontName);

private:
  bool loadMetricsCsv(const char *csvPath);
  bool isEmojiGlyph(uint32_t glyphIndex) const;
  bool loadEmojiGlyph(uint32_t glyphIndex, std::vector<uint8_t> &outPixels,
                      uint32_t &outWidth, uint32_t &outHeight);
  glm::vec4 allocateAndUploadEmoji(uint32_t glyphIndex,
                                   const std::vector<uint8_t> &pixels,
                                   uint32_t width, uint32_t height);

  float m_pixelSize{32.0f};
  class GpuAllocator *m_allocator{nullptr};
  uint32_t m_fontTextureSlot{0};
  uint32_t m_fontAtlasWidth{2048};
  uint32_t m_fontAtlasHeight{2048};
  bool m_isIconFont{false};

  FT_Face m_ftFace{nullptr};
  FT_Face m_ftEmojiFace{nullptr};
  hb_font_t *m_hbFont{nullptr};

  std::unordered_map<uint32_t, GlyphMetrics> m_codepointMetricsMap;
  std::unordered_map<uint32_t, GlyphMetrics> m_glyphIndexMetricsMap;

  mutable std::unordered_map<uint32_t, bool> m_emojiCache;

  AllocatedImage m_emojiAtlasImage;
  VkImageView m_emojiImageView{VK_NULL_HANDLE};
  VkImageLayout m_emojiAtlasLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  uint32_t m_emojiTextureSlot{0};
  uint32_t m_emojiAtlasWidth{2048};
  uint32_t m_emojiAtlasHeight{2048};
  uint32_t m_shelfX{0};
  uint32_t m_shelfY{0};
  uint32_t m_rowHeight{0};
  uint32_t m_atlasPadding{2};
  std::unordered_map<uint32_t, glm::vec4> m_emojiUvMap;
};

} // namespace avk
