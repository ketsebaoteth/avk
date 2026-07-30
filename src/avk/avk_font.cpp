#include "avk/avk_font.h"
#include "avk/avk_core.h"
#include "avk/avk_texture.h"
#include "freetype/freetype.h"
#include "hb-blob.hh"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

namespace avk {

Font::Font(VulkanContext *context, const std::string &atlasImagePath,
           const std::string &metricsCsvPath)
    : m_context(context) {
  if (!m_context || !m_context->isValid()) {
    std::cerr << "avk: Cannot load Font with an invalid VulkanContext."
              << std::endl;
    return;
  }

  // 1. Load MSDF Atlas Texture
  m_textureIndex = m_context->getTextureManager()->loadTexture(atlasImagePath);

  VkExtent2D texExtent =
      m_context->getTextureManager()->getTextureExtent(m_textureIndex);

  if (texExtent.width == 0 || texExtent.height == 0) {
    std::cerr
        << "avk: Failed to resolve valid texture dimensions for MSDF Font."
        << std::endl;
    return;
  }

  // 2. Load and Parse MSDF CSV Metrics File
  std::ifstream csvFile(metricsCsvPath);
  if (!csvFile.is_open()) {
    std::cerr << "avk: Failed to open MSDF font metrics CSV: " << metricsCsvPath
              << std::endl;
    return;
  }

  std::stringstream ss;
  ss << csvFile.rdbuf();
  csvFile.close();

  // 3. Parse metrics using EXACT physical texture dimensions!
  parseMetricsCsv(ss.str(), texExtent.width, texExtent.height);

  // 4. Initialize HarfBuzz Shaper
  hb_blob_t *blob = hb_blob_create_from_file(atlasImagePath.c_str());
  hb_face_t *face = hb_face_create(blob, 0);
  m_hbFont = hb_font_create(face);
  hb_face_destroy(face);
  hb_blob_destroy(blob);
}

Font::Font(VulkanContext *context, std::span<const uint8_t> atlasPngBytes,
           const std::string &metricsCsvContent)
    : m_context(context) {
  if (!m_context || !m_context->isValid()) {
    std::cerr << "avk: Cannot load Font with an invalid VulkanContext."
              << std::endl;
    return;
  }

  m_textureIndex =
      m_context->getTextureManager()->loadTextureFromMemory(atlasPngBytes);
  parseMetricsCsv(metricsCsvContent, 512, 512);
}

Font::Font(VulkanContext *context, const std::string &filePath,
           uint32_t fontSize, const std::vector<uint32_t> &codepoints)
    : m_context(context) {
  if (!m_context || !m_context->isValid()) {
    std::cerr << "avk: Cannot load Font with an invalid VulkanContext."
              << std::endl;
    return;
  }

  std::ifstream file(filePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "avk: Failed to open font file: " << filePath << std::endl;
    return;
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  m_retainedFontBuffer.resize(fileSize);
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char *>(m_retainedFontBuffer.data()), fileSize);
  file.close();

  if (!buildAtlasFromMemory(m_retainedFontBuffer, fontSize, codepoints)) {
    std::cerr << "avk: Failed to construct Font Atlas for file: " << filePath
              << std::endl;
  }
}

Font::~Font() { release(); }

Font::Font(Font &&other) noexcept { *this = std::move(other); }

Font &Font::operator=(Font &&other) noexcept {
  if (this != &other) {
    release();

    m_context = other.m_context;
    m_atlasImage = std::move(other.m_atlasImage);
    m_atlasView = other.m_atlasView;
    m_textureIndex = other.m_textureIndex;
    m_lineHeight = other.m_lineHeight;
    m_ascent = other.m_ascent;
    m_fontSize = other.m_fontSize;
    m_glyphs = std::move(other.m_glyphs);
    m_fallbackGlyph = other.m_fallbackGlyph;
    m_hbFont = other.m_hbFont;
    m_retainedFontBuffer = std::move(other.m_retainedFontBuffer);

    other.m_context = nullptr;
    other.m_atlasView = VK_NULL_HANDLE;
    other.m_textureIndex = 0;
    other.m_hbFont = nullptr;
  }
  return *this;
}

void Font::release() {
  if (m_context == nullptr)
    return;

  if (m_textureIndex != 0) {
    m_context->getTextureManager()->unloadTexture(m_textureIndex);
    m_textureIndex = 0;
  }
  m_retainedFontBuffer.clear();
}

std::string Font::resolveSystemFontPath(const std::string &fontName) {
  namespace fs = std::filesystem;

  std::vector<std::string> searchDirs;
#if defined(_WIN32)
  searchDirs.push_back("C:\\Windows\\Fonts");
#elif defined(__APPLE__)
  searchDirs.push_back("/System/Library/Fonts");
  searchDirs.push_back("/Library/Fonts");
#else
  searchDirs.push_back("/usr/share/fonts");
  searchDirs.push_back("/usr/local/share/fonts");
  searchDirs.push_back("~/.fonts");
#endif

  std::string lowerTarget = fontName;
  std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(),
                 ::tolower);

  for (const auto &dirPath : searchDirs) {
    if (!fs::exists(dirPath))
      continue;

    for (const auto &entry : fs::recursive_directory_iterator(dirPath)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        std::string lowerFilename = filename;
        std::transform(lowerFilename.begin(), lowerFilename.end(),
                       lowerFilename.begin(), ::tolower);

        if (lowerFilename.find(lowerTarget) != std::string::npos &&
            (entry.path().extension() == ".ttf" ||
             entry.path().extension() == ".otf")) {
          return entry.path().string();
        }
      }
    }
  }
  return "";
}

static uint32_t decodeNextUtf8(const std::string &str, uint32_t &index) {
  if (index >= str.size())
    return 0;
  unsigned char c = str[index++];
  if (c < 0x80)
    return c;
  if ((c & 0xE0) == 0xC0) {
    if (index >= str.size())
      return c;
    uint32_t res = (c & 0x1F) << 6;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F);
    return res;
  }
  if ((c & 0xF0) == 0xE0) {
    if (index + 1 >= str.size())
      return c;
    uint32_t res = (c & 0x0F) << 12;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F) << 6;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F);
    return res;
  }
  if ((c & 0xF8) == 0xF0) {
    if (index + 2 >= str.size())
      return c;
    uint32_t res = (c & 0x07) << 18;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F) << 12;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F) << 6;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F);
    return res;
  }
  return c;
}

glm::vec2 Font::measureText(const std::string &text, float fontSize) const {
  float baseSize = (m_fontSize > 0) ? static_cast<float>(m_fontSize) : 32.0f;
  float scale = (fontSize > 0.0f) ? (fontSize / baseSize) : 1.0f;

  float width = 0.0f;
  float lineHeight = m_lineHeight * scale;

  uint32_t index = 0;
  while (index < text.size()) {
    uint32_t codepoint = decodeNextUtf8(text, index);
    const Glyph &glyph = getGlyph(codepoint);
    width += glyph.advance * scale;
  }

  return glm::vec2(width, lineHeight);
}

bool Font::parseMetricsCsv(const std::string &csvContent, uint32_t atlasWidth,
                           uint32_t atlasHeight) {
  std::stringstream ss(csvContent);
  std::string line;

  float invW = 1.0f / static_cast<float>(atlasWidth);
  float invH = 1.0f / static_cast<float>(atlasHeight);

  float maxAscent = 0.0f;
  float maxDescent = 0.0f;

  while (std::getline(ss, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    std::stringstream lineStream(line);
    std::string cell;
    std::vector<float> values;

    while (std::getline(lineStream, cell, ',')) {
      try {
        values.push_back(std::stof(cell));
      } catch (...) {
        break;
      }
    }

    if (values.size() >= 10) {
      uint32_t codepoint = static_cast<uint32_t>(values[0]);
      float advance = values[1] * 32.0f;

      float pLeft = values[2] * 32.0f;
      float pBottom = values[3] * 32.0f;
      float pRight = values[4] * 32.0f;
      float pTop = values[5] * 32.0f;

      // Track maximum ascent and descent dynamically from CSV!
      maxAscent = std::max(maxAscent, pTop);
      maxDescent = std::max(maxDescent, std::abs(pBottom));

      float aLeft = values[6];
      float aBottom = values[7];
      float aRight = values[8];
      float aTop = values[9];

      float uMin = std::min(aLeft, aRight) * invW;
      float uMax = std::max(aLeft, aRight) * invW;
      float vMin = 1.0f - (std::max(aTop, aBottom) * invH);
      float vMax = 1.0f - (std::min(aTop, aBottom) * invH);

      Glyph glyph{};
      glyph.size = glm::vec2(pRight - pLeft, pTop - pBottom);
      glyph.bearing = glm::vec2(pLeft, pTop);
      glyph.advance = advance;

      glyph.uvBounds = glm::vec4(uMin, vMin, uMax, vMax);

      m_glyphs[codepoint] = glyph;
    }
  }

  // If 'H' or 'A' exist (standard text font), use them. Otherwise, compute max
  // top bearing across all glyphs.
  if (m_glyphs.find('H') != m_glyphs.end()) {
    m_ascent = m_glyphs['H'].bearing.y;
  } else if (m_glyphs.find('A') != m_glyphs.end()) {
    m_ascent = m_glyphs['A'].bearing.y;
  } else if (!m_glyphs.empty()) {
    // Fallback for Icon Fonts (like Lucide): find max top bearing
    float maxTop = 0.0f;
    for (const auto &[cp, g] : m_glyphs) {
      maxTop = std::max(maxTop, g.bearing.y);
    }
    m_ascent = (maxTop > 0.0f) ? maxTop : 22.0f;
  } else {
    m_ascent = 22.0f;
  }

  // Same for descent/line height
  float maxStandardDescent = 0.0f;
  const char descenderChars[] = {'g', 'j', 'p', 'q', 'y', 'Q',
                                 '|', '(', ')', '[', ']'};
  for (char c : descenderChars) {
    if (m_glyphs.find(c) != m_glyphs.end()) {
      float d = std::abs(m_glyphs[c].size.y - m_glyphs[c].bearing.y);
      maxStandardDescent = std::max(maxStandardDescent, d);
    }
  }

  if (maxStandardDescent == 0.0f) {
    maxStandardDescent = maxDescent > 0.0f ? maxDescent : (m_ascent * 0.3f);
  }

  m_lineHeight = m_ascent + maxStandardDescent;
  return !m_glyphs.empty();
}

bool Font::buildAtlasFromMemory(std::span<const uint8_t> fontBytes,
                                uint32_t fontSize,
                                const std::vector<uint32_t> &codepoints) {
  if (fontBytes.empty())
    return false;

  FT_Library ft = nullptr;
  if (FT_Init_FreeType(&ft)) {
    return false;
  }

  FT_Face face = nullptr;
  if (FT_New_Memory_Face(ft, fontBytes.data(),
                         static_cast<FT_Long>(fontBytes.size()), 0, &face)) {
    FT_Done_FreeType(ft);
    return false;
  }

  FT_Set_Pixel_Sizes(face, 0, fontSize);

  m_lineHeight = static_cast<float>(face->size->metrics.height >> 6);
  m_ascent = static_cast<float>(face->size->metrics.ascender >> 6);
  m_fontSize = fontSize;

  std::vector<uint32_t> targets = codepoints;
  if (targets.empty()) {
    targets.reserve(95);
    for (uint32_t i = 32; i < 127; ++i) {
      targets.push_back(i);
    }
  }

  uint32_t glyphPadding = 2;
  uint32_t atlasWidth = 512;
  uint32_t currentX = 0;
  uint32_t currentY = 0;
  uint32_t rowHeight = 0;

  for (uint32_t cp : targets) {
    if (FT_Load_Char(face, cp, FT_LOAD_RENDER)) {
      continue;
    }

    uint32_t w = face->glyph->bitmap.width;
    uint32_t h = face->glyph->bitmap.rows;

    if (currentX + w + glyphPadding >= atlasWidth) {
      currentX = 0;
      currentY += rowHeight + glyphPadding;
      rowHeight = 0;
    }

    rowHeight = std::max(rowHeight, h);
    currentX += w + glyphPadding;
  }

  uint32_t atlasHeight = currentY + rowHeight + glyphPadding;
  atlasHeight = (atlasHeight + 3) & ~3;

  std::vector<uint8_t> atlasBuffer(atlasWidth * atlasHeight, 0);

  currentX = 0;
  currentY = 0;
  rowHeight = 0;

  for (uint32_t cp : targets) {
    if (FT_Load_Char(face, cp, FT_LOAD_RENDER)) {
      continue;
    }

    FT_Bitmap &bitmap = face->glyph->bitmap;
    uint32_t w = bitmap.width;
    uint32_t h = bitmap.rows;

    if (currentX + w + glyphPadding >= atlasWidth) {
      currentX = 0;
      currentY += rowHeight + glyphPadding;
      rowHeight = 0;
    }

    for (uint32_t r = 0; r < h; ++r) {
      for (uint32_t c = 0; c < w; ++c) {
        atlasBuffer[(currentY + r) * atlasWidth + (currentX + c)] =
            bitmap.buffer[r * bitmap.pitch + c];
      }
    }

    Glyph glyph{};
    glyph.size = glm::vec2(static_cast<float>(w), static_cast<float>(h));
    glyph.bearing = glm::vec2(static_cast<float>(face->glyph->bitmap_left),
                              static_cast<float>(face->glyph->bitmap_top));
    glyph.advance = static_cast<float>(face->glyph->advance.x >> 6);

    glyph.uvBounds = glm::vec4(
        static_cast<float>(currentX) / static_cast<float>(atlasWidth),
        static_cast<float>(currentY) / static_cast<float>(atlasHeight),
        static_cast<float>(currentX + w) / static_cast<float>(atlasWidth),
        static_cast<float>(currentY + h) / static_cast<float>(atlasHeight));

    m_glyphs[cp] = glyph;

    rowHeight = std::max(rowHeight, h);
    currentX += w + glyphPadding;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  m_fallbackGlyph = m_glyphs[63];

  VkDeviceSize bufferSize = atlasWidth * atlasHeight;

  AllocatedBuffer stagingBuffer = m_context->getAllocator()->createBuffer(
      bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  std::memcpy(stagingBuffer.getMappedData(), atlasBuffer.data(), bufferSize);

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = atlasWidth;
  imageInfo.extent.height = atlasHeight;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8_UNORM;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

  m_atlasImage = m_context->getAllocator()->createImage(
      imageInfo, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

  VkDevice device = m_context->getDevice();
  VkQueue graphicsQueue = m_context->getGraphicsQueue();

  VkCommandPool tempPool = VK_NULL_HANDLE;
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  poolInfo.queueFamilyIndex = m_context->getQueueFamilies().graphicsFamily;
  vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool);

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = tempPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(device, &allocInfo, &cmd);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &beginInfo);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = m_atlasImage.getImage();
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {atlasWidth, atlasHeight, 1};

  vkCmdCopyBufferToImage(cmd, stagingBuffer.getBuffer(),
                         m_atlasImage.getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  vkEndCommandBuffer(cmd);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence = VK_NULL_HANDLE;
  vkCreateFence(device, &fenceInfo, nullptr, &fence);

  vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence);
  vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

  vkDestroyFence(device, fence, nullptr);
  vkDestroyCommandPool(device, tempPool, nullptr);
  stagingBuffer.destroy();

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = m_atlasImage.getImage();
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewInfo, nullptr, &m_atlasView) !=
      VK_SUCCESS) {
    m_atlasImage.destroy();
    return false;
  }

  m_textureIndex = m_context->getTextureManager()->registerTexture(
      std::move(m_atlasImage), m_atlasView,
      m_context->getTextureManager()->getFontSampler());

  return true;
}

} // namespace avk
