#include "avk/avk_font.h"
#include "avk/avk_textLayout.h"

#include "avk/avk_core.h"
#include "glm/ext/vector_float2.hpp"
#include "ui/core/resources.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>

namespace {

uint32_t getUtf8CodepointAtCluster(std::string_view text,
                                   uint32_t clusterIndex) {
  if (clusterIndex >= text.size())
    return 0;

  unsigned char c = static_cast<unsigned char>(text[clusterIndex]);
  if (c < 0x80)
    return c;
  if ((c & 0xE0) == 0xC0 && clusterIndex + 1 < text.size()) {
    return ((c & 0x1F) << 6) |
           (static_cast<unsigned char>(text[clusterIndex + 1]) & 0x3F);
  }
  if ((c & 0xF0) == 0xE0 && clusterIndex + 2 < text.size()) {
    return ((c & 0x0F) << 12) |
           ((static_cast<unsigned char>(text[clusterIndex + 1]) & 0x3F) << 6) |
           (static_cast<unsigned char>(text[clusterIndex + 2]) & 0x3F);
  }
  if ((c & 0xF8) == 0xF0 && clusterIndex + 3 < text.size()) {
    return ((c & 0x07) << 18) |
           ((static_cast<unsigned char>(text[clusterIndex + 1]) & 0x3F) << 12) |
           ((static_cast<unsigned char>(text[clusterIndex + 2]) & 0x3F) << 6) |
           (static_cast<unsigned char>(text[clusterIndex + 3]) & 0x3F);
  }
  return 0;
}

} // namespace

namespace avk {

Font::~Font() {
  if (m_hbFont) {
    hb_font_destroy(m_hbFont);
    m_hbFont = nullptr;
  }
  if (m_ftFace) {
    FT_Done_Face(m_ftFace);
    m_ftFace = nullptr;
  }
  if (m_ftEmojiFace) {
    FT_Done_Face(m_ftEmojiFace);
    m_ftEmojiFace = nullptr;
  }

  if (m_allocator && m_allocator->getContext() &&
      m_emojiImageView != VK_NULL_HANDLE) {
    VkDevice device = m_allocator->getContext()->getDevice();
    if (device != VK_NULL_HANDLE) {
      vkDestroyImageView(device, m_emojiImageView, nullptr);
    }
    m_emojiImageView = VK_NULL_HANDLE;
  }

  if (m_emojiAtlasImage.getImage() != VK_NULL_HANDLE) {
    m_emojiAtlasImage.destroy();
  }
}

bool Font::loadFromFile(const char *ttfPath, const char *csvPath,
                        float pixelSize, GpuAllocator *allocator,
                        uint32_t fontTextureSlot, uint32_t emojiTextureSlot,
                        uint32_t fontAtlasWidth, uint32_t fontAtlasHeight) {
  m_pixelSize = pixelSize;
  m_allocator = allocator;
  m_fontTextureSlot = fontTextureSlot;
  m_emojiTextureSlot = emojiTextureSlot;

  m_fontAtlasWidth = fontAtlasWidth;
  m_fontAtlasHeight = fontAtlasHeight;

  if (m_allocator && m_allocator->getContext() &&
      m_allocator->getContext()->getTextureManager()) {
    VkExtent2D ext =
        m_allocator->getContext()->getTextureManager()->getTextureExtent(
            fontTextureSlot);
    if (ext.width > 0 && ext.height > 0) {
      m_fontAtlasWidth = ext.width;
      m_fontAtlasHeight = ext.height;
    }
  }

  static FT_Library ftLibrary = []() {
    FT_Library lib;
    FT_Init_FreeType(&lib);
    return lib;
  }();

  if (FT_New_Face(ftLibrary, ttfPath, 0, &m_ftFace) != 0) {
    std::string path1 = "assets/fonts/" + std::string(ttfPath);
    if (FT_New_Face(ftLibrary, path1.c_str(), 0, &m_ftFace) != 0) {
      std::string path2 = "../assets/fonts/" + std::string(ttfPath);
      if (FT_New_Face(ftLibrary, path2.c_str(), 0, &m_ftFace) != 0) {
        std::string path3 = "assets/" + std::string(ttfPath);
        if (FT_New_Face(ftLibrary, path3.c_str(), 0, &m_ftFace) != 0) {
          std::println(
              "[atomicUI]: ERROR! FT_New_Face failed to locate TTF font: {}",
              ttfPath);
          return false;
        }
      }
    }
  }

  if (FT_Set_Pixel_Sizes(m_ftFace, 0, static_cast<FT_UInt>(pixelSize)) != 0) {
    FT_Done_Face(m_ftFace);
    m_ftFace = nullptr;
    return false;
  }

  m_hbFont = hb_ft_font_create_referenced(m_ftFace);
  if (!m_hbFont) {
    FT_Done_Face(m_ftFace);
    m_ftFace = nullptr;
    return false;
  }

  hb_ft_font_set_funcs(m_hbFont);

  if (!loadMetricsCsv(csvPath)) {
    return false;
  }

  // 1. Allocate GPU Image for dynamic Emoji shelf atlas
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = m_emojiAtlasWidth;
  imageInfo.extent.height = m_emojiAtlasHeight;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  m_emojiAtlasImage =
      m_allocator->createImage(imageInfo, VMA_MEMORY_USAGE_GPU_ONLY);
  m_emojiAtlasLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // 2. Transition m_emojiAtlasImage layout to SHADER_READ_ONLY_OPTIMAL
  m_allocator->immediateSubmit([&](VkCommandBuffer cmd) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_emojiAtlasImage.getImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
  });
  m_emojiAtlasLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  // 3. Register m_emojiAtlasImage view in Vulkan Texture Manager at
  // m_emojiTextureSlot
  if (m_allocator && m_allocator->getContext() &&
      m_allocator->getContext()->getTextureManager()) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_emojiAtlasImage.getImage();
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkDevice device = m_allocator->getContext()->getDevice();
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_emojiImageView) ==
        VK_SUCCESS) {
      m_allocator->getContext()->getTextureManager()->registerTextureAtSlot(
          m_emojiTextureSlot, m_emojiImageView);
    }
  }

  // 4. Load OS System Color Emoji Fallback Font with FT_Select_Size
  std::string emojiFontPath = atomic::getPath("fonts/NotoColorEmoji.ttf");
  if (!std::filesystem::exists(emojiFontPath)) {
    emojiFontPath = resolveSystemFontPath("NotoColorEmoji");
  }

  if (std::filesystem::exists(emojiFontPath)) {
    if (FT_New_Face(ftLibrary, emojiFontPath.c_str(), 0, &m_ftEmojiFace) == 0) {
      if (m_ftEmojiFace) {
        if (m_ftEmojiFace->num_fixed_sizes > 0) {
          FT_Select_Size(m_ftEmojiFace, 0);
        } else {
          FT_Set_Pixel_Sizes(m_ftEmojiFace, 0, static_cast<FT_UInt>(pixelSize));
        }
      }
    }
  }

  if (m_ftFace) {
    std::string pathStr(ttfPath);
    std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
    if (m_ftFace->descender == 0 ||
        pathStr.find("lucide") != std::string::npos ||
        pathStr.find("icon") != std::string::npos ||
        pathStr.find("awesome") != std::string::npos) {
      m_isIconFont = true;
    }
  }

  return m_emojiAtlasImage.getImage() != VK_NULL_HANDLE;
}

bool Font::loadMetricsCsv(const char *csvPath) {
  std::ifstream file(csvPath);
  if (!file.is_open()) {
    return false;
  }

  std::string line;
  std::getline(file, line);

  while (std::getline(file, line)) {
    if (line.empty())
      continue;

    std::stringstream ss(line);
    std::string val;
    std::vector<std::string> tokens;

    while (std::getline(ss, val, ',')) {
      tokens.push_back(val);
    }

    if (tokens.size() >= 10) {
      GlyphMetrics gm{};
      gm.codepoint = static_cast<uint32_t>(std::stoul(tokens[0]));
      gm.advance = std::stof(tokens[1]);
      gm.planeLeft = std::stof(tokens[2]);
      gm.planeBottom = std::stof(tokens[3]);
      gm.planeRight = std::stof(tokens[4]);
      gm.planeTop = std::stof(tokens[5]);
      gm.atlasLeft = std::stof(tokens[6]);
      gm.atlasBottom = std::stof(tokens[7]);
      gm.atlasRight = std::stof(tokens[8]);
      gm.atlasTop = std::stof(tokens[9]);

      m_codepointMetricsMap[gm.codepoint] = gm;

      if (m_ftFace) {
        FT_UInt glyphIdx = FT_Get_Char_Index(m_ftFace, gm.codepoint);
        if (glyphIdx != 0) {
          m_glyphIndexMetricsMap[glyphIdx] = gm;
        }
      }
    }
  }
  return !m_codepointMetricsMap.empty();
}

bool Font::loadEmojiGlyph(uint32_t glyphIndex, std::vector<uint8_t> &outPixels,
                          uint32_t &outWidth, uint32_t &outHeight) {
  FT_Face targetFace = m_ftFace;
  FT_UInt targetGlyphIdx = glyphIndex;

  if (m_ftEmojiFace && (glyphIndex >= 0x80000000)) {
    targetFace = m_ftEmojiFace;
    targetGlyphIdx = glyphIndex & 0x7FFFFFFF;
  }

  if (!targetFace) {
    return false;
  }

  FT_Int32 loadFlags = FT_LOAD_COLOR | FT_LOAD_DEFAULT;
  if (FT_Load_Glyph(targetFace, targetGlyphIdx, loadFlags) != 0) {
    return false;
  }

  if (targetFace->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
    if (FT_Render_Glyph(targetFace->glyph, FT_RENDER_MODE_NORMAL) != 0) {
      return false;
    }
  }

  FT_Bitmap &bitmap = targetFace->glyph->bitmap;
  outWidth = bitmap.width;
  outHeight = bitmap.rows;

  if (outWidth == 0 || outHeight == 0) {
    return false;
  }

  outPixels.resize(outWidth * outHeight * 4);

  if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
    for (uint32_t y = 0; y < outHeight; ++y) {
      for (uint32_t x = 0; x < outWidth; ++x) {
        uint32_t srcIdx = y * bitmap.pitch + x * 4;
        uint32_t dstIdx = (y * outWidth + x) * 4;

        outPixels[dstIdx + 0] = bitmap.buffer[srcIdx + 2];
        outPixels[dstIdx + 1] = bitmap.buffer[srcIdx + 1];
        outPixels[dstIdx + 2] = bitmap.buffer[srcIdx + 0];
        outPixels[dstIdx + 3] = bitmap.buffer[srcIdx + 3];
      }
    }
    return true;
  } else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
    for (uint32_t y = 0; y < outHeight; ++y) {
      for (uint32_t x = 0; x < outWidth; ++x) {
        uint32_t srcIdx = y * bitmap.pitch + x;
        uint32_t dstIdx = (y * outWidth + x) * 4;
        uint8_t alpha = bitmap.buffer[srcIdx];

        outPixels[dstIdx + 0] = 255;
        outPixels[dstIdx + 1] = 255;
        outPixels[dstIdx + 2] = 255;
        outPixels[dstIdx + 3] = alpha;
      }
    }
    return true;
  }

  return false;
}

bool Font::isEmojiGlyph(uint32_t glyphIndex) {
  if (FT_Load_Glyph(m_ftFace, glyphIndex, FT_LOAD_COLOR) == 0) {
    return m_ftFace->glyph->format == FT_GLYPH_FORMAT_BITMAP;
  }
  return false;
}

glm::vec4 Font::allocateAndUploadEmoji(uint32_t glyphIndex,
                                       const std::vector<uint8_t> &pixels,
                                       uint32_t width, uint32_t height) {
  if (m_shelfX + width + m_atlasPadding > m_emojiAtlasWidth) {
    m_shelfX = 0;
    m_shelfY += m_rowHeight + m_atlasPadding;
    m_rowHeight = 0;
  }

  if (m_shelfY + height + m_atlasPadding > m_emojiAtlasHeight) {
    return glm::vec4(0.0f);
  }

  uint32_t allocX = m_shelfX + m_atlasPadding;
  uint32_t allocY = m_shelfY + m_atlasPadding;

  m_shelfX += width + (m_atlasPadding * 2);
  m_rowHeight = std::max(m_rowHeight, height + (m_atlasPadding * 2));

  VkDeviceSize imageSize = width * height * 4;
  AllocatedBuffer stagingBuffer = m_allocator->createBuffer(
      imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
      VMA_ALLOCATION_CREATE_MAPPED_BIT);

  std::memcpy(stagingBuffer.getMappedData(), pixels.data(),
              static_cast<size_t>(imageSize));

  m_allocator->immediateSubmit([&](VkCommandBuffer cmd) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_emojiAtlasLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_emojiAtlasImage.getImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Match pipeline stage mask to access mask
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkAccessFlags srcAccess = 0;

    if (m_emojiAtlasLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      srcAccess = VK_ACCESS_SHADER_READ_BIT;
    }

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(allocX),
                          static_cast<int32_t>(allocY), 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, stagingBuffer.getBuffer(),
                           m_emojiAtlasImage.getImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);
  });

  m_emojiAtlasLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  stagingBuffer.destroy();

  glm::vec4 uvRect(
      static_cast<float>(allocX) / static_cast<float>(m_emojiAtlasWidth),
      static_cast<float>(allocY) / static_cast<float>(m_emojiAtlasHeight),
      static_cast<float>(allocX + width) /
          static_cast<float>(m_emojiAtlasWidth),
      static_cast<float>(allocY + height) /
          static_cast<float>(m_emojiAtlasHeight));

  m_emojiUvMap[glyphIndex] = uvRect;
  return uvRect;
}

glm::vec2 Font::measureText(std::string_view text, float fontSize,
                            float maxWidth, avk::TextWrapMode wrapMode,
                            float lineHeight,
                            avk::TextAlignMode alignMode) const {
  if (text.empty() || !m_hbFont) {
    return glm::vec2(0.0f);
  }

  float size = (fontSize > 0.0f) ? fontSize : m_pixelSize;

  TextLayoutOptions options{};
  options.fontSize = size;
  options.baseFontSize = m_pixelSize;
  options.maxWidth = maxWidth;
  options.wrapMode = wrapMode;
  options.lineHeight = lineHeight;
  options.alignMode = alignMode;

  auto glyphs = TextLayout::ShapeString(m_hbFont, text, options, 0.0f, 0.0f);
  if (glyphs.empty()) {
    return glm::vec2(0.0f, size);
  }

  float effectiveLineH = (lineHeight > 0.0f) ? lineHeight : getLineHeight(size);

  float maxLineWidth = 0.0f;
  float maxY = 0.0f;

  for (const auto &g : glyphs) {
    maxLineWidth = std::max(maxLineWidth, g.rectXYWH.x + g.xAdvance);
    // Accumulate height using the active line height for each line slot
    maxY = std::max(maxY, g.rectXYWH.y + effectiveLineH);
  }

  return glm::vec2(maxLineWidth, maxY);
}

float Font::getLineHeight(float fontSize) const {
  float size = (fontSize > 0.0f) ? fontSize : m_pixelSize;
  if (m_isIconFont)
    return size;
  return size * 1.2f;
}

float Font::getAscent(float fontSize) const {
  float size = (fontSize > 0.0f) ? fontSize : m_pixelSize;
  if (m_isIconFont) {
    return size;
  }
  return size * 0.86f;
}

std::string Font::resolveSystemFontPath(std::string_view fontName) {
#if defined(__linux__)
  std::vector<std::string> searchPaths = {
      "/usr/share/fonts/noto/" + std::string(fontName) + ".ttf",
      "/usr/share/fonts/noto-emoji/" + std::string(fontName) + ".ttf",
      "/usr/share/fonts/TTF/" + std::string(fontName) + ".ttf",
      "/usr/share/fonts/truetype/noto/" + std::string(fontName) + ".ttf",
      "/usr/share/fonts/truetype/" + std::string(fontName) + ".ttf",
      "/usr/share/fonts/TTF/NotoColorEmoji.ttf",
      "/usr/share/fonts/noto/NotoColorEmoji.ttf"};

  for (const auto &path : searchPaths) {
    if (std::filesystem::exists(path)) {
      return path;
    }
  }
#elif defined(__APPLE__)
  std::vector<std::string> searchPaths = {
      "/System/Library/Fonts/Apple Color Emoji.ttc",
      "/Library/Fonts/Apple Color Emoji.ttc",
      "/Library/Fonts/" + std::string(fontName) + ".ttf"};
  for (const auto &path : searchPaths) {
    if (std::filesystem::exists(path))
      return path;
  }
#elif defined(_WIN32)
  std::string path = "C:\\Windows\\Fonts\\seguiemj.ttf";
  if (std::filesystem::exists(path))
    return path;
  path = "C:\\Windows\\Fonts\\" + std::string(fontName) + ".ttf";
  if (std::filesystem::exists(path))
    return path;
#endif
  return std::string(fontName);
}

std::vector<avk::InstanceData>
Font::layoutText(std::string_view text, glm::vec2 position,
                 const Clay_BoundingBox &box, const glm::vec4 &color,
                 float fontSize, float letterSpacing, float fontWeight,
                 const glm::vec4 &clipRect, float scale, float rotation,
                 const glm::vec2 &transformOrigin, const glm::vec2 &translate,
                 float lineHeight, avk::TextWrapMode wrapMode,
                 avk::TextAlignMode alignMode) {

  std::vector<avk::InstanceData> instances;
  if (text.empty() || !m_hbFont) {
    return instances;
  }

  float targetSize = fontSize > 0.0f ? fontSize : m_pixelSize;

  TextLayoutOptions options{};
  options.fontSize = targetSize;
  options.baseFontSize = m_pixelSize;
  options.fontWeight = fontWeight;
  options.letterSpacing = letterSpacing;
  options.lineHeight = lineHeight;
  options.wrapMode = wrapMode;
  options.alignMode = alignMode;
  options.maxWidth = box.width;
  options.textureIndex = m_fontTextureSlot;
  options.color = color;

  std::vector<ShapedGlyph> shapedGlyphs =
      TextLayout::ShapeString(m_hbFont, text, options, position.x, position.y);

  instances.reserve(shapedGlyphs.size());

  auto getPivotTransformedCoords =
      [](const glm::vec4 &glyphRect,
         [[maybe_unused]] const Clay_BoundingBox &containerBox,
         [[maybe_unused]] float s, [[maybe_unused]] float r,
         [[maybe_unused]] const glm::vec2 &origin,
         const glm::vec2 &trans) -> glm::vec4 {
    float rawPosX = glyphRect.x + trans.x;
    float rawPosY = glyphRect.y + trans.y;
    return glm::vec4(rawPosX, rawPosY, glyphRect.z, glyphRect.w);
  };

  for (const auto &glyph : shapedGlyphs) {
    avk::InstanceData instance{};

    bool isEmoji = isEmojiGlyph(glyph.glyphIndex);
    uint32_t activeEmojiKey = glyph.glyphIndex;

    // Fallback to Noto Color Emoji only for non-ASCII characters (codepoint >=
    // 0x80)
    if ((!isEmoji || glyph.glyphIndex == 0) && m_ftEmojiFace) {
      uint32_t codepoint = getUtf8CodepointAtCluster(text, glyph.clusterIndex);
      if (codepoint >= 0x80) {
        FT_UInt fallbackIdx = FT_Get_Char_Index(m_ftEmojiFace, codepoint);
        if (fallbackIdx != 0) {
          isEmoji = true;
          activeEmojiKey = 0x80000000 | fallbackIdx;
        }
      }
    }

    if (isEmoji) {
      glm::vec4 uvRect(0.0f);
      uint32_t eWidth = 0, eHeight = 0;

      auto itUv = m_emojiUvMap.find(activeEmojiKey);
      if (itUv != m_emojiUvMap.end()) {
        uvRect = itUv->second;
      } else {
        std::vector<uint8_t> emojiPixels;
        if (loadEmojiGlyph(activeEmojiKey, emojiPixels, eWidth, eHeight)) {
          uvRect = allocateAndUploadEmoji(activeEmojiKey, emojiPixels, eWidth,
                                          eHeight);
        }
      }

      float eSize = targetSize;
      glm::vec4 emojiRect(glyph.rectXYWH.x, glyph.rectXYWH.y, eSize, eSize);
      glm::vec4 bounds = getPivotTransformedCoords(
          emojiRect, box, scale, rotation, transformOrigin, translate);

      instance.rectXYWH =
          glm::vec4(std::floor(bounds.x + 0.5f), std::floor(bounds.y + 0.5f),
                    std::floor(eSize + 0.5f), std::floor(eSize + 0.5f));

      instance.borderRadius = glm::vec4(0.0f);
      instance.fillColorA = glm::vec4(1.0f);
      instance.uvBounds = uvRect;
      instance.clipRect = clipRect;
      instance.shapeType = 0;
      instance.fillType = 7; // fillType = 7 for RGBA Color Emoji sampling
      instance.textureIndex = m_emojiTextureSlot;
      instance.strokeThickness = glm::vec4(0.0f);
      instance.blur = 0.0f;
      instance.scale = scale;
      instance.rotation = rotation;
      instance.fontWeight = fontWeight;
      instances.push_back(instance);
    } else {
      auto it = m_glyphIndexMetricsMap.find(glyph.glyphIndex);
      if (it == m_glyphIndexMetricsMap.end()) {
        continue;
      }

      const auto &gm = it->second;

      float inkWidth = (gm.planeRight - gm.planeLeft) * options.fontSize;
      float inkHeight = (gm.planeTop - gm.planeBottom) * options.fontSize;

      float lineBoxHeight =
          (options.lineHeight > 0.0f) ? options.lineHeight : glyph.rectXYWH.w;
      float verticalPadding = (lineBoxHeight > targetSize)
                                  ? (lineBoxHeight - targetSize) * 0.5f
                                  : 0.0f;

      float fontAscent = getAscent(targetSize);
      float rawBaselineY = glyph.rectXYWH.y + fontAscent + verticalPadding;
      float baselineY = std::floor(rawBaselineY + 0.5f);

      float posX = glyph.rectXYWH.x + (gm.planeLeft * options.fontSize);
      float posY = baselineY - (gm.planeTop * options.fontSize);

      glm::vec4 physicalGlyphRect(posX, posY, inkWidth, inkHeight);
      glm::vec4 bounds = getPivotTransformedCoords(
          physicalGlyphRect, box, scale, rotation, transformOrigin, translate);

      instance.rectXYWH =
          glm::vec4(std::floor(bounds.x + 0.5f), std::floor(bounds.y + 0.5f),
                    std::floor(bounds.z + 0.5f), std::floor(bounds.w + 0.5f));

      instance.borderRadius = glm::vec4(0.0f);
      instance.fillColorA = color;

      float texW = static_cast<float>(m_fontAtlasWidth);
      float texH = static_cast<float>(m_fontAtlasHeight);

      float uMin = gm.atlasLeft / texW;
      float vMin = (texH - gm.atlasTop) / texH;
      float uMax = gm.atlasRight / texW;
      float vMax = (texH - gm.atlasBottom) / texH;

      instance.uvBounds = glm::vec4(uMin, vMin, uMax, vMax);
      instance.clipRect = clipRect;
      instance.shapeType = 0;
      instance.fillType = 3;
      instance.textureIndex = m_fontTextureSlot;
      instance.strokeThickness = glm::vec4(0.0f);
      instance.blur = 0.0f;
      instance.scale = scale;
      instance.rotation = rotation;
      instance.fontWeight = fontWeight;
      instances.push_back(instance);
    }
  }

  return instances;
}

} // namespace avk
