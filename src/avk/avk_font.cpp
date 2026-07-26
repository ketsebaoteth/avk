#include "avk/avk_font.h"
#include "avk/avk_core.h"
#include "avk/avk_texture.h"

#include <cstring>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <iostream>
#include <utility>

namespace avk {

Font::Font(VulkanContext *context, const std::string &filePath,
           uint32_t fontSize, const std::vector<uint32_t> &codepoints)
    : m_context(context) {
  if (!m_context || !m_context->isValid()) {
    std::cerr << "avk: Cannot load Font with an invalid VulkanContext."
              << std::endl;
    return;
  }

  if (!buildAtlas(filePath, fontSize, codepoints)) {
    std::cerr << "avk: Failed to construct Font Atlas for: " << filePath
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

    other.m_context = nullptr;
    other.m_atlasView = VK_NULL_HANDLE;
    other.m_textureIndex = 0;
    other.m_lineHeight = 0.0f;
  }
  return *this;
}

void Font::release() {
  if (m_context == nullptr)
    return;

  VkDevice device = m_context->getDevice();
  if (device == VK_NULL_HANDLE)
    return;

  if (m_textureIndex != 0) {
    m_context->getTextureManager()->unloadTexture(m_textureIndex);
    m_textureIndex = 0;
  }
}

static uint32_t decodeNextUtf8(const std::string &str, uint32_t &index) {
  if (index >= str.size())
    return 0;
  unsigned char c = str[index++];
  if (c < 0x80)
    return c;
  if ((c & 0xE0) == 0xC0) {
    uint32_t res = (c & 0x1F) << 6;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F);
    return res;
  }
  if ((c & 0xF0) == 0xE0) {
    uint32_t res = (c & 0x0F) << 12;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F) << 6;
    res |= (static_cast<unsigned char>(str[index++]) & 0x3F);
    return res;
  }
  return c;
}

glm::vec2 Font::measureText(const std::string &text) const {
  float width = 0.0f;
  float maxHeight = 0.0f;

  uint32_t index = 0;
  while (index < text.size()) {
    uint32_t codepoint = decodeNextUtf8(text, index);
    const Glyph &glyph = getGlyph(codepoint);
    width += glyph.advance;
    maxHeight = std::max(maxHeight, glyph.size.y);
  }

  return glm::vec2(width, maxHeight);
}

bool Font::buildAtlas(const std::string &filePath, uint32_t fontSize,
                      const std::vector<uint32_t> &codepoints) {
  FT_Library ft = nullptr;
  if (FT_Init_FreeType(&ft)) {
    return false;
  }

  FT_Face face = nullptr;
  if (FT_New_Face(ft, filePath.c_str(), 0, &face)) {
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

  uint32_t glyphPadding = 2; // Tighter padding for standard bitmap layouts
  uint32_t atlasWidth = 512;
  uint32_t currentX = 0;
  uint32_t currentY = 0;
  uint32_t rowHeight = 0;

  for (uint32_t cp : targets) {
    // Load and render using standard anti-aliased FT_RENDER_MODE_NORMAL
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

  // Registers the font atlas utilizing our sharp VK_FILTER_NEAREST
  // point-sampler!
  m_textureIndex = m_context->getTextureManager()->registerTexture(
      std::move(m_atlasImage), m_atlasView,
      m_context->getTextureManager()->getFontSampler());

  return true;
}

} // namespace avk
