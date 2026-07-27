#pragma once

#include "avk_allocator.h"
#include <memory>
#include <string>
#include <vector>
#include <volk.h>

namespace avk {

class VulkanContext;

/**
 * @brief Managed GPU texture instance.
 */
struct Texture {
  AllocatedImage image;
  VkImageView view = VK_NULL_HANDLE;
  uint32_t index = 0;
};

/**
 * @brief Dynamic Bindless Texture Manager.
 * Loads image files from disk, uploads them to GPU memory, and registers them
 * in our global boundless array.
 */
class TextureManager {
public:
  static constexpr uint32_t MAX_BINDLESS_TEXTURES = 512;

  explicit TextureManager(VulkanContext *context);
  ~TextureManager();

  // Disable copies
  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;

  // Enable moves
  TextureManager(TextureManager &&other) noexcept;
  TextureManager &operator=(TextureManager &&other) noexcept;

  /**
   * @brief Loads an image file, uploads it to the GPU, and registers it in our
   * boundless descriptor set.
   * @param path File path to the image asset (PNG, JPG, etc.).
   * @return The unique boundless texture index, or 0 (fallback) on failure.
   */
  uint32_t loadTexture(const std::string &path);

  /**
   * @brief Registers an externally created Vulkan image and view into the
   * bindless set. Transfers ownership of the image and view to the
   * TextureManager.
   */
  uint32_t registerTexture(AllocatedImage &&image, VkImageView view,
                           VkSampler sampler);

  /**
   * @brief Unregisters a texture, releasing its GPU memory, views, and freeing
   * its slot.
   */
  void unloadTexture(uint32_t index);

  VkDescriptorSetLayout getDescriptorSetLayout() const {
    return m_descriptorSetLayout;
  }
  VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }
  VkSampler getSharedSampler() const { return m_sharedSampler; }
  VkSampler getFontSampler() const { return m_fontSampler; }

  // returns the extent of a texture
  VkExtent2D getTextureExtent(uint32_t index) const {
    if (index < m_textures.size() && m_textures[index]) {
      return {m_textures[index]->image.getExtent().width,
              m_textures[index]->image.getExtent().height};
    }
    return {100, 100}; // Fallback
  }

private:
  void release();
  void createDescriptorSet();
  void createSharedSampler();
  void createFontSampler();

  // Helpers to record and execute transient GPU upload commands
  VkCommandBuffer beginSingleTimeCommands(VkCommandPool &outPool);
  void endSingleTimeCommands(VkCommandBuffer cmd, VkCommandPool pool);
  void transitionImageLayout(VkImage image, VkFormat format,
                             VkImageLayout oldLayout, VkImageLayout newLayout);

  VulkanContext *m_context = nullptr;

  VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
  VkSampler m_sharedSampler = VK_NULL_HANDLE;
  VkSampler m_fontSampler = VK_NULL_HANDLE;

  // Managed list of active loaded textures and recycled slots
  std::vector<std::unique_ptr<Texture>> m_textures;
  std::vector<uint32_t> m_freeSlots;
};

} // namespace avk
