#pragma once

#include <volk.h>

// Force VMA to compile in pure dynamic loading mode, integrating cleanly with
// Volk
#ifndef VMA_STATIC_VULKAN_FUNCTIONS
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#endif
#ifndef VMA_DYNAMIC_VULKAN_FUNCTIONS
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#endif

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <vk_mem_alloc.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include <functional>

namespace avk {

class VulkanContext;

/**
 * @brief RAII container for a Vulkan Buffer allocation.
 */
class AllocatedBuffer {
public:
  AllocatedBuffer() = default;
  AllocatedBuffer(VmaAllocator allocator, VkBuffer buffer,
                  VmaAllocation allocation, const VmaAllocationInfo &allocInfo);
  ~AllocatedBuffer();

  AllocatedBuffer(const AllocatedBuffer &) = delete;
  AllocatedBuffer &operator=(const AllocatedBuffer &) = delete;

  AllocatedBuffer(AllocatedBuffer &&other) noexcept;
  AllocatedBuffer &operator=(AllocatedBuffer &&other) noexcept;

  void destroy();

  VkBuffer getBuffer() const { return m_buffer; }
  VmaAllocation getAllocation() const { return m_allocation; }
  void *getMappedData() const { return m_allocInfo.pMappedData; }
  VkDeviceSize getSize() const { return m_allocInfo.size; }

private:
  VmaAllocator m_allocator = nullptr;
  VkBuffer m_buffer = VK_NULL_HANDLE;
  VmaAllocation m_allocation = nullptr;
  VmaAllocationInfo m_allocInfo{};
};

/**
 * @brief RAII container for a Vulkan Image allocation.
 */
class AllocatedImage {
public:
  AllocatedImage() = default;
  AllocatedImage(VmaAllocator allocator, VkImage image,
                 VmaAllocation allocation, VkExtent3D extent, VkFormat format);
  ~AllocatedImage();

  AllocatedImage(const AllocatedImage &) = delete;
  AllocatedImage &operator=(const AllocatedImage &) = delete;

  AllocatedImage(AllocatedImage &&other) noexcept;
  AllocatedImage &operator=(AllocatedImage &&other) noexcept;

  void destroy();

  VkImage getImage() const { return m_image; }
  VmaAllocation getAllocation() const { return m_allocation; }
  VkExtent3D getExtent() const { return m_extent; }
  VkFormat getFormat() const { return m_format; }

private:
  VmaAllocator m_allocator = nullptr;
  VkImage m_image = VK_NULL_HANDLE;
  VmaAllocation m_allocation = nullptr;
  VkExtent3D m_extent{};
  VkFormat m_format = VK_FORMAT_UNDEFINED;
};

/**
 * @brief Master memory allocator wrapper managing the VmaAllocator instance.
 */
class GpuAllocator {
public:
  explicit GpuAllocator(VulkanContext *context);
  ~GpuAllocator();

  GpuAllocator(const GpuAllocator &) = delete;
  GpuAllocator &operator=(const GpuAllocator &) = delete;

  GpuAllocator(GpuAllocator &&other) noexcept;
  GpuAllocator &operator=(GpuAllocator &&other) noexcept;

  bool isValid() const { return m_allocator != nullptr; }

  AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               VmaMemoryUsage memoryUsage,
                               VmaAllocationCreateFlags flags = 0);
  AllocatedImage createImage(const VkImageCreateInfo &imageInfo,
                             VmaMemoryUsage memoryUsage,
                             VmaAllocationCreateFlags flags = 0);

  VmaAllocator getVmaAllocator() const { return m_allocator; }
  void immediateSubmit(std::function<void(VkCommandBuffer)> &&function);

  VulkanContext *getContext() const { return m_context; }

private:
  void release();

  VulkanContext *m_context = nullptr;
  VmaAllocator m_allocator = nullptr;
};

} // namespace avk
