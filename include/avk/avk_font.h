#pragma once

#include "avk_allocator.h"
#include "glm/ext/vector_float2.hpp"
#include <glm/glm.hpp>
#include <string>
#include <volk.h>
#include <vulkan/vulkan_core.h>

typedef struct FT_LibraryRec_ *FT_Library;
typedef struct FT_FaceRec_ *FT_Face;

namespace avk {
class VulkanContext;

/**
 * @brief Represents a single character's layout metrics and UV atlas
 * boundaries.
 */
struct Glyph {
  glm::vec2 size;
  glm::vec2 bearing;
  float advance;
  glm::vec4 uvBounds;
};

/**
 * @brief RAII Font Loader and Rasterizer using FreeType.
 * Packs printable ASCII characters into an optimal R8_UNORM GPU texture atlas.
 */
class Font {
public:
  Font(VulkanContext *context, const std::string &filepath, uint32_t fontSize);
  ~Font();

  // Disable copies for strict resource safety (RAII)
  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

  // Enable move semantics
  Font(Font &&other) noexcept;
  Font &operator=(Font &&other) noexcept;

  /**
   * @brief Measures the bounding width and height of a string in pixels.
   * Calculated instantly on the CPU, zero heap allocations.
   */
  glm::vec2 measureText(const std::string &text) const;

  uint32_t getTextureIndex() const { return m_textureIndex; };
  float getLineHeight() const { return m_lineHeight; };
  float getAscent() const { return m_ascent; }
  uint32_t getFontSize() const { return m_fontSize; }

  const Glyph getGlyph(char c) const {
    auto index = static_cast<uint8_t>(c);
    if (index < 128) {
      return m_glyphs[index];
    }
    return m_glyphs[63]; // fallbacking to ? of ascii value 63
  };

private:
  void release();
  bool buildAtlas(const std::string &path, uint32_t fontSize);

  VulkanContext *m_context = nullptr;

  AllocatedImage m_atlasImage;
  VkImageView m_atlasView = VK_NULL_HANDLE;
  uint32_t m_textureIndex = 0;

  float m_lineHeight = 0.0f;
  float m_ascent = 0.0f;
  uint32_t m_fontSize = 0;

  // flat array ascii mapping
  Glyph m_glyphs[128]{};
};
} // namespace avk
