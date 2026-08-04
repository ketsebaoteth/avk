#pragma once

#include "avk_bidi.h"
#include <cstdint>

#include <glm/glm.hpp>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace avk {

enum class TextWrapMode : uint8_t { Word, Anywhere, Disabled };

enum class TextAlignMode : uint8_t { Left, Center, Right, Justify };

struct ShapedGlyph {
  uint32_t glyphIndex{0};
  uint32_t clusterIndex{0};
  uint32_t textureIndex{0};

  glm::vec4 rectXYWH{0.0f};
  glm::vec4 uvBounds{0.0f};
  glm::vec4 color{1.0f};

  float xAdvance{0.0f};
  float yAdvance{0.0f};
  float fontWeight{400.0f};
};

struct TextLayoutOptions {
  float fontSize{16.0f};
  float baseFontSize{32.0f};
  float fontWeight{400.0f};
  float lineHeight{0.0f};
  float letterSpacing{0.0f};
  float maxWidth{0.0f};
  uint32_t textureIndex{0};
  glm::vec4 color{1.0f};
  TextWrapMode wrapMode{TextWrapMode::Word};
  TextAlignMode alignMode{TextAlignMode::Left};

  void setWordWrap(bool enable) {
    wrapMode = enable ? TextWrapMode::Word : TextWrapMode::Disabled;
  }
};

class TextLayout {
public:
  static std::vector<ShapedGlyph> ShapeString(hb_font_t *hbFont,
                                              std::string_view utf8Text,
                                              const TextLayoutOptions &options,
                                              float startX = 0.0f,
                                              float startY = 0.0f) {
    if (utf8Text.empty() || !hbFont) {
      return {};
    }

    // ------------------------------------------------------------------------
    // ⚡ SAFE O(1) SHAPING CACHE: Exact string keys & automatic capacity cap
    // ------------------------------------------------------------------------
    struct CacheKey {
      hb_font_t *font;
      std::string
          text; // ✅ Store actual string to prevent hash collisions & UAF
      float fontSize;
      float fontWeight;
      float letterSpacing;
      float lineHeight;
      float maxWidth;
      uint8_t wrapMode;
      uint8_t alignMode;

      bool operator==(const CacheKey &other) const {
        return font == other.font && text == other.text &&
               fontSize == other.fontSize && fontWeight == other.fontWeight &&
               letterSpacing == other.letterSpacing &&
               lineHeight == other.lineHeight && maxWidth == other.maxWidth &&
               wrapMode == other.wrapMode && alignMode == other.alignMode;
      }
    };

    struct CacheHash {
      size_t operator()(const CacheKey &k) const noexcept {
        size_t h = std::hash<std::string>{}(k.text);
        h ^= std::hash<void *>()(k.font) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<float>()(k.fontSize) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<float>()(k.maxWidth) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
      }
    };

    thread_local std::unordered_map<CacheKey, std::vector<ShapedGlyph>,
                                    CacheHash>
        s_shapeCache;

    // Clear cache if it gets too large to prevent holding stale font
    if (s_shapeCache.size() > 4096) {
      s_shapeCache.clear();
    }

    CacheKey key{hbFont,
                 std::string(utf8Text),
                 options.fontSize,
                 options.fontWeight,
                 options.letterSpacing,
                 options.lineHeight,
                 options.maxWidth,
                 static_cast<uint8_t>(options.wrapMode),
                 static_cast<uint8_t>(options.alignMode)};

    auto cacheIt = s_shapeCache.find(key);
    if (cacheIt != s_shapeCache.end()) {
      auto cachedGlyphs = cacheIt->second;
      for (auto &g : cachedGlyphs) {
        g.rectXYWH.x += startX;
        g.rectXYWH.y += startY;
        g.color = options.color;
        g.textureIndex = options.textureIndex;
      }
      return cachedGlyphs;
    }
    //
    // ------------------------------------------------------------------------
    // HarfBuzz Shaping Pass (Only runs ONCE per unique text label)
    // ------------------------------------------------------------------------
    std::vector<ShapedGlyph> unshiftedGlyphs;
    std::vector<BidiRun> bidiRuns = BidiEngine::ProcessText(utf8Text);

    thread_local hb_buffer_t *hbBuffer = nullptr;
    if (!hbBuffer) {
      hbBuffer = hb_buffer_create();
    }

    constexpr float fontScale = 1.0f / 64.0f;
    const float baseSize =
        (options.baseFontSize > 0.0f) ? options.baseFontSize : 32.0f;
    const float scaleFactor = options.fontSize / baseSize;

    const float effectiveLineHeight = (options.lineHeight > 0.0f)
                                          ? options.lineHeight
                                          : (options.fontSize * 1.2f);

    hb_codepoint_t spaceGlyph = 0;
    hb_font_get_nominal_glyph(hbFont, ' ', &spaceGlyph);

    static const hb_feature_t noLigatures[] = {
        {HB_TAG('l', 'i', 'g', 'a'), 0, 0, (unsigned int)-1},
        {HB_TAG('c', 'l', 'i', 'g'), 0, 0, (unsigned int)-1}};

    struct RawGlyph {
      uint32_t glyphIndex;
      uint32_t clusterIndex;
      float xOffset, yOffset, xAdvance, yAdvance;
      bool isSpace;
    };

    thread_local std::vector<RawGlyph> rawGlyphs;
    rawGlyphs.clear();
    rawGlyphs.reserve(utf8Text.size());

    for (const auto &run : bidiRuns) {
      hb_buffer_clear_contents(hbBuffer);
      hb_buffer_add_utf8(
          hbBuffer, utf8Text.data(), static_cast<int>(utf8Text.size()),
          static_cast<unsigned int>(run.offset), static_cast<int>(run.length));

      hb_direction_t hbDir = (run.direction == TextDirection::RightToLeft)
                                 ? HB_DIRECTION_RTL
                                 : HB_DIRECTION_LTR;

      hb_buffer_set_direction(hbBuffer, hbDir);
      hb_buffer_guess_segment_properties(hbBuffer);
      hb_shape(hbFont, hbBuffer, noLigatures, 2);

      unsigned int glyphCount = 0;
      hb_glyph_info_t *glyphInfos =
          hb_buffer_get_glyph_infos(hbBuffer, &glyphCount);
      hb_glyph_position_t *glyphPositions =
          hb_buffer_get_glyph_positions(hbBuffer, &glyphCount);

      for (unsigned int i = 0; i < glyphCount; ++i) {
        float xOff = (glyphPositions[i].x_offset * fontScale) * scaleFactor;
        float yOff = (glyphPositions[i].y_offset * fontScale) * scaleFactor;
        float xAdv = ((glyphPositions[i].x_advance * fontScale) * scaleFactor) +
                     options.letterSpacing;
        float yAdv = (glyphPositions[i].y_advance * fontScale) * scaleFactor;

        if (glyphInfos[i].codepoint == 0 || xAdv <= 0.001f) {
          xAdv = options.fontSize;
        }

        bool isWhitespace = (glyphInfos[i].codepoint == spaceGlyph) ||
                            (glyphInfos[i].codepoint == 3) ||
                            (glyphInfos[i].codepoint == 32);

        rawGlyphs.push_back({glyphInfos[i].codepoint, glyphInfos[i].cluster,
                             xOff, yOff, xAdv, yAdv, isWhitespace});
      }
    }

    struct Line {
      std::vector<RawGlyph> glyphs;
      float width{0.0f};
      uint32_t spaceCount{0};
    };

    std::vector<Line> lines;
    lines.reserve(16);
    lines.push_back({});

    float currentWidth = 0.0f;
    size_t lastSpaceIndexInLine = std::string::npos;
    const float minWrapWidth = options.fontSize * 1.5f;

    for (const auto &g : rawGlyphs) {
      bool causeWrap = false;
      if (options.wrapMode != TextWrapMode::Disabled &&
          options.maxWidth >= minWrapWidth &&
          (currentWidth + g.xAdvance) > options.maxWidth &&
          !lines.back().glyphs.empty()) {
        causeWrap = true;
      }

      if (causeWrap) {
        if (options.wrapMode == TextWrapMode::Word &&
            lastSpaceIndexInLine != std::string::npos) {
          float newLineWidth = 0.0f;
          std::vector<RawGlyph> trailingGlyphs;
          size_t splitIdx = lastSpaceIndexInLine + 1;

          for (size_t k = splitIdx; k < lines.back().glyphs.size(); ++k) {
            trailingGlyphs.push_back(lines.back().glyphs[k]);
            newLineWidth += lines.back().glyphs[k].xAdvance;
            lines.back().width -= lines.back().glyphs[k].xAdvance;
          }

          lines.back().glyphs.resize(splitIdx);
          lines.push_back({});

          for (const auto &tg : trailingGlyphs) {
            lines.back().glyphs.push_back(tg);
          }
          lines.back().width = newLineWidth;
          currentWidth = newLineWidth;
        } else {
          lines.push_back({});
          currentWidth = 0.0f;
        }

        lastSpaceIndexInLine = std::string::npos;
      }

      if (g.isSpace) {
        lastSpaceIndexInLine = lines.back().glyphs.size();
        lines.back().spaceCount++;
      }

      lines.back().glyphs.push_back(g);
      lines.back().width += g.xAdvance;
      currentWidth += g.xAdvance;
    }

    unshiftedGlyphs.reserve(rawGlyphs.size());
    float cursorY = 0.0f;

    for (size_t lIdx = 0; lIdx < lines.size(); ++lIdx) {
      auto &line = lines[lIdx];
      float lineX = 0.0f;

      if (options.maxWidth > 0.0f && options.maxWidth > line.width) {
        float remainingSpace = options.maxWidth - line.width;

        if (options.alignMode == TextAlignMode::Center) {
          lineX += remainingSpace * 0.5f;
        } else if (options.alignMode == TextAlignMode::Right) {
          lineX += remainingSpace;
        }
      }

      float cursorX = lineX;

      for (const auto &g : line.glyphs) {
        ShapedGlyph glyph{};
        glyph.glyphIndex = g.glyphIndex;
        glyph.clusterIndex = g.clusterIndex;
        glyph.textureIndex = options.textureIndex;
        glyph.color = options.color;
        glyph.fontWeight = options.fontWeight;
        glyph.xAdvance = g.xAdvance;
        glyph.yAdvance = g.yAdvance;

        glyph.rectXYWH = glm::vec4(cursorX + g.xOffset, cursorY - g.yOffset,
                                   g.xAdvance, options.fontSize);

        unshiftedGlyphs.push_back(glyph);
        cursorX += g.xAdvance;
      }

      cursorY += effectiveLineHeight;
    }

    s_shapeCache[key] = unshiftedGlyphs;

    for (auto &g : unshiftedGlyphs) {
      g.rectXYWH.x += startX;
      g.rectXYWH.y += startY;
    }

    return unshiftedGlyphs;
  }
};

} // namespace avk
