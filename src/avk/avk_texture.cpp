#include "avk/avk_texture.h"
#include "avk/avk_core.h"
#include "stb/stb_image.h"
#include <cstring>
#include <iostream>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace avk {

TextureManager::TextureManager(VulkanContext *context) : m_context(context) {
  if (!m_context || !m_context->isValid()) {
    std::cerr << "avk: Cannot initialize TextureManager with an invalid "
                 "VulkanContext."
              << std::endl;
    return;
  }

  createDescriptorSet();
  createSharedSampler();
  createFontSampler();

  // Initialize our free slot stack with available index slots
  m_textures.resize(MAX_BINDLESS_TEXTURES);
  m_freeSlots.reserve(MAX_BINDLESS_TEXTURES);
  for (int32_t i = MAX_BINDLESS_TEXTURES - 1; i >= 0; --i) {
    m_freeSlots.push_back(static_cast<uint32_t>(i));
  }
}

TextureManager::~TextureManager() { release(); }

TextureManager::TextureManager(TextureManager &&other) noexcept {
  *this = std::move(other);
}

TextureManager &TextureManager::operator=(TextureManager &&other) noexcept {
  if (this != &other) {
    release();

    m_context = other.m_context;
    m_descriptorSetLayout = other.m_descriptorSetLayout;
    m_descriptorPool = other.m_descriptorPool;
    m_descriptorSet = other.m_descriptorSet;
    m_sharedSampler = other.m_sharedSampler;
    m_textures = std::move(other.m_textures);
    m_freeSlots = std::move(other.m_freeSlots);

    other.m_context = nullptr;
    other.m_descriptorSetLayout = VK_NULL_HANDLE;
    other.m_descriptorPool = VK_NULL_HANDLE;
    other.m_descriptorSet = VK_NULL_HANDLE;
    other.m_sharedSampler = VK_NULL_HANDLE;
  }
  return *this;
}
// In src/avk/avk_texture.cpp:

void TextureManager::createFontSampler() {
  VkDevice device = m_context->getDevice();

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;

  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(device, &samplerInfo, nullptr, &m_fontSampler) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to construct Font Linear Sampler." << std::endl;
  }
}

void TextureManager::release() {
  VkDevice device = m_context->getDevice();
  if (device == VK_NULL_HANDLE)
    return;

  if (m_fontSampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_fontSampler, nullptr);
    m_fontSampler = VK_NULL_HANDLE;
  }
  // Clear all textures safely via RAII destructors
  for (auto &tex : m_textures) {
    if (tex) {
      vkDestroyImageView(device, tex->view, nullptr);
      tex->image.destroy();
      tex.reset();
    }
  }
  m_textures.clear();

  if (m_sharedSampler != VK_NULL_HANDLE) {
    vkDestroySampler(device, m_sharedSampler, nullptr);
    m_sharedSampler = VK_NULL_HANDLE;
  }

  if (m_descriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    m_descriptorPool = VK_NULL_HANDLE;
  }

  if (m_descriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
    m_descriptorSetLayout = VK_NULL_HANDLE;
  }
}
uint32_t TextureManager::loadRawPixels(const uint8_t *pixels, uint32_t width,
                                       uint32_t height) {
  if (m_freeSlots.empty() || !pixels || width == 0 || height == 0) {
    std::cerr << "avk: Cannot load raw pixels from empty buffer!" << std::endl;
    return 0;
  }

  VkDeviceSize imageSize = width * height * 4;

  // 1. Allocate staging buffer to transfer raw RGBA pixel data
  AllocatedBuffer stagingBuffer = m_context->getAllocator()->createBuffer(
      imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  std::memcpy(stagingBuffer.getMappedData(), pixels, imageSize);

  vmaFlushAllocation(m_context->getAllocator()->getVmaAllocator(),
                     stagingBuffer.getAllocation(), 0, VK_WHOLE_SIZE);

  // 2. Create target optimal GPU image
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

  AllocatedImage gpuImage = m_context->getAllocator()->createImage(
      imageInfo, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

  // 3. Record and submit copy commands to GPU
  VkDevice device = m_context->getDevice();
  VkCommandPool tempPool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = beginSingleTimeCommands(tempPool);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = gpuImage.getImage();
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
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {width, height, 1};

  vkCmdCopyBufferToImage(cmd, stagingBuffer.getBuffer(), gpuImage.getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  endSingleTimeCommands(cmd, tempPool);
  stagingBuffer.destroy();

  // 4. Create ImageView wrapper
  VkImageView view = VK_NULL_HANDLE;
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = gpuImage.getImage();
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
    gpuImage.destroy();
    return 0;
  }

  return registerTexture(std::move(gpuImage), view, m_sharedSampler);
}
uint32_t TextureManager::loadTextureFromMemory(std::span<const uint8_t> bytes) {
  if (m_freeSlots.empty() || bytes.empty()) {
    std::cerr << "avk: Cannot load texture from empty memory buffer!"
              << std::endl;
    return 0;
  }

  // 1. Load image pixels directly from memory via STB
  int texWidth = 0, texHeight = 0, texChannels = 0;
  stbi_uc *pixels = stbi_load_from_memory(
      bytes.data(), static_cast<int>(bytes.size()), &texWidth, &texHeight,
      &texChannels, STBI_rgb_alpha);

  if (!pixels) {
    std::cerr << "avk: Failed to decode texture from memory buffer!"
              << std::endl;
    return 0;
  }

  VkDeviceSize imageSize = texWidth * texHeight * 4;

  // 2. Allocate staging buffer to transfer pixel data
  AllocatedBuffer stagingBuffer = m_context->getAllocator()->createBuffer(
      imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  std::memcpy(stagingBuffer.getMappedData(), pixels, imageSize);
  stbi_image_free(pixels);

  vmaFlushAllocation(m_context->getAllocator()->getVmaAllocator(),
                     stagingBuffer.getAllocation(), 0, VK_WHOLE_SIZE);

  // 3. Create target optimal GPU image
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = static_cast<uint32_t>(texWidth);
  imageInfo.extent.height = static_cast<uint32_t>(texHeight);
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

  AllocatedImage gpuImage = m_context->getAllocator()->createImage(
      imageInfo, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

  // 4. Record and submit copy commands to GPU
  VkDevice device = m_context->getDevice();
  VkCommandPool tempPool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = beginSingleTimeCommands(tempPool);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = gpuImage.getImage();
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
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {static_cast<uint32_t>(texWidth),
                        static_cast<uint32_t>(texHeight), 1};

  vkCmdCopyBufferToImage(cmd, stagingBuffer.getBuffer(), gpuImage.getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  endSingleTimeCommands(cmd, tempPool);
  stagingBuffer.destroy();

  // 5. Create ImageView wrapper
  VkImageView view = VK_NULL_HANDLE;
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = gpuImage.getImage();
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
    std::cerr << "avk: Failed to build VkImageView for memory texture."
              << std::endl;
    gpuImage.destroy();
    return 0;
  }

  // 6. Register inside bindless set
  return registerTexture(std::move(gpuImage), view, m_sharedSampler);
}

uint32_t TextureManager::loadTexture(const std::string &path) {
  if (m_freeSlots.empty()) {
    std::cerr << "avk: Max bindless texture limit reached!" << std::endl;
    return 0;
  }

  // 1. Load image file from disk via STB
  int texWidth = 0, texHeight = 0, texChannels = 0;
  stbi_uc *pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels,
                              STBI_rgb_alpha);
  if (!pixels) {
    std::cerr << "avk: Failed to load image asset path: " << path << std::endl;
    return 0;
  }

  VkDeviceSize imageSize = texWidth * texHeight * 4;

  // 2. Allocate staging buffer to transfer pixel data
  AllocatedBuffer stagingBuffer = m_context->getAllocator()->createBuffer(
      imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
  std::memcpy(stagingBuffer.getMappedData(), pixels, imageSize);
  stbi_image_free(pixels);
  vmaFlushAllocation(m_context->getAllocator()->getVmaAllocator(),
                     stagingBuffer.getAllocation(), 0, VK_WHOLE_SIZE);

  // 3. Create target optimal GPU image (UNORM Format)
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = static_cast<uint32_t>(texWidth);
  imageInfo.extent.height = static_cast<uint32_t>(texHeight);
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // <--- FIX 1
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

  AllocatedImage gpuImage = m_context->getAllocator()->createImage(
      imageInfo, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

  // 4. Record and submit copy commands to GPU
  VkDevice device = m_context->getDevice();
  VkCommandPool tempPool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = beginSingleTimeCommands(tempPool);

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = gpuImage.getImage();
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
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {static_cast<uint32_t>(texWidth),
                        static_cast<uint32_t>(texHeight), 1};

  vkCmdCopyBufferToImage(cmd, stagingBuffer.getBuffer(), gpuImage.getImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  endSingleTimeCommands(cmd, tempPool);
  stagingBuffer.destroy();

  // 5. Create ImageView wrapper (UNORM Format)
  VkImageView view = VK_NULL_HANDLE;
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = gpuImage.getImage();
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // <--- FIX 2
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
    std::cerr << "avk: Failed to build VkImageView for texture." << std::endl;
    gpuImage.destroy();
    return 0;
  }

  // 6. Register inside descriptor set
  return registerTexture(std::move(gpuImage), view, m_sharedSampler);
}

void TextureManager::unloadTexture(uint32_t index) {
  if (index >= MAX_BINDLESS_TEXTURES || !m_textures[index]) {
    return;
  }

  VkDevice device = m_context->getDevice();
  vkDeviceWaitIdle(device); // Ensure the GPU has finished using this texture

  vkDestroyImageView(device, m_textures[index]->view, nullptr);
  m_textures[index]->image.destroy();
  m_textures[index].reset();

  m_freeSlots.push_back(index);
}

void TextureManager::createDescriptorSet() {
  VkDevice device = m_context->getDevice();

  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // Fixed
  binding.descriptorCount = MAX_BINDLESS_TEXTURES;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorBindingFlags bindingFlags =
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

  VkDescriptorSetLayoutBindingFlagsCreateInfo layoutBindingFlags{};
  layoutBindingFlags.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  layoutBindingFlags.bindingCount = 1;
  layoutBindingFlags.pBindingFlags = &bindingFlags;

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.pNext = &layoutBindingFlags;
  layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  layoutInfo.bindingCount = 1;
  layoutInfo.pBindings = &binding;

  if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                  &m_descriptorSetLayout) != VK_SUCCESS) {
    std::cerr << "avk: Failed to construct Bindless Descriptor Set Layout."
              << std::endl;
  }

  VkDescriptorPoolSize poolSize{};
  poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // Fixed
  poolSize.descriptorCount = MAX_BINDLESS_TEXTURES;

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;

  if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to construct Bindless Descriptor Pool."
              << std::endl;
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = m_descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &m_descriptorSetLayout;

  if (vkAllocateDescriptorSets(device, &allocInfo, &m_descriptorSet) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to allocate Bindless Descriptor Set."
              << std::endl;
  }
}

void TextureManager::createSharedSampler() {
  VkDevice device = m_context->getDevice();

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  if (vkCreateSampler(device, &samplerInfo, nullptr, &m_sharedSampler) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to construct shared UI Sampler." << std::endl;
  }
}

VkCommandBuffer
TextureManager::beginSingleTimeCommands(VkCommandPool &outPool) {
  VkDevice device = m_context->getDevice();

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags =
      VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // Optimizes short-lived pools
  poolInfo.queueFamilyIndex = m_context->getQueueFamilies().graphicsFamily;

  if (vkCreateCommandPool(device, &poolInfo, nullptr, &outPool) != VK_SUCCESS) {
    std::cerr << "avk: Failed to create transient command pool." << std::endl;
    return VK_NULL_HANDLE;
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = outPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  vkAllocateCommandBuffers(device, &allocInfo, &cmd);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmd, &beginInfo);
  return cmd;
}

void TextureManager::endSingleTimeCommands(VkCommandBuffer cmd,
                                           VkCommandPool pool) {
  if (cmd == VK_NULL_HANDLE || pool == VK_NULL_HANDLE)
    return;

  VkDevice device = m_context->getDevice();
  VkQueue graphicsQueue = m_context->getGraphicsQueue();

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
  vkDestroyCommandPool(device, pool,
                       nullptr); // Destroys the pool and releases the command
                                 // buffer automatically
}

void TextureManager::transitionImageLayout(VkImage image, VkFormat format,
                                           VkImageLayout oldLayout,
                                           VkImageLayout newLayout) {
  (void)format;
  VkCommandPool tempPool = VK_NULL_HANDLE;
  VkCommandBuffer cmd = beginSingleTimeCommands(tempPool);
  if (cmd == VK_NULL_HANDLE)
    return;

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
      newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    vkDestroyCommandPool(m_context->getDevice(), tempPool, nullptr);
    return;
  }

  vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
  endSingleTimeCommands(cmd, tempPool);
}
uint32_t TextureManager::registerTexture(AllocatedImage &&image,
                                         VkImageView view, VkSampler sampler) {
  if (m_freeSlots.empty()) {
    std::cerr << "avk: Max bindless texture limit reached!" << std::endl;
    return 0;
  }

  VkDevice device = m_context->getDevice();
  uint32_t slot = m_freeSlots.back();
  m_freeSlots.pop_back();

  // Write the custom image descriptor to our boundless set
  VkDescriptorImageInfo descriptorImageInfo{};
  descriptorImageInfo.sampler =
      (sampler != VK_NULL_HANDLE) ? sampler : m_sharedSampler;
  descriptorImageInfo.imageView = view;
  descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = m_descriptorSet;
  write.dstBinding = 0;
  write.dstArrayElement = slot;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.descriptorCount = 1;
  write.pImageInfo = &descriptorImageInfo;

  vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

  // Create the texture record, taking ownership of the image
  auto texture = std::make_unique<Texture>();
  texture->image = std::move(image);
  texture->view = view;
  texture->index = slot;

  m_textures[slot] = std::move(texture);

  return slot;
}

} // namespace avk
