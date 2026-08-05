#include "avk/avk_allocator.h"
#include "avk/avk_core.h"
#include "utils/log.h"
#include <utility>

namespace avk {

// =================================================================
// 1. AllocatedBuffer Implementation
// =================================================================

AllocatedBuffer::AllocatedBuffer(VmaAllocator allocator, VkBuffer buffer,
                                 VmaAllocation allocation,
                                 const VmaAllocationInfo &allocInfo)
    : m_allocator(allocator), m_buffer(buffer), m_allocation(allocation),
      m_allocInfo(allocInfo) {}

AllocatedBuffer::~AllocatedBuffer() { destroy(); }

AllocatedBuffer::AllocatedBuffer(AllocatedBuffer &&other) noexcept {
  *this = std::move(other);
}

AllocatedBuffer &AllocatedBuffer::operator=(AllocatedBuffer &&other) noexcept {
  if (this != &other) {
    destroy();

    m_allocator = other.m_allocator;
    m_buffer = other.m_buffer;
    m_allocation = other.m_allocation;
    m_allocInfo = other.m_allocInfo;

    other.m_allocator = nullptr;
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = nullptr;
    other.m_allocInfo = VmaAllocationInfo{};
  }
  return *this;
}

void AllocatedBuffer::destroy() {
  if (m_buffer != VK_NULL_HANDLE && m_allocation != nullptr) {
    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    m_buffer = VK_NULL_HANDLE;
    m_allocation = nullptr;
  }
}

// =================================================================
// 2. AllocatedImage Implementation
// =================================================================

AllocatedImage::AllocatedImage(VmaAllocator allocator, VkImage image,
                               VmaAllocation allocation, VkExtent3D extent,
                               VkFormat format)
    : m_allocator(allocator), m_image(image), m_allocation(allocation),
      m_extent(extent), m_format(format) {}

AllocatedImage::~AllocatedImage() { destroy(); }

AllocatedImage::AllocatedImage(AllocatedImage &&other) noexcept {
  *this = std::move(other);
}

AllocatedImage &AllocatedImage::operator=(AllocatedImage &&other) noexcept {
  if (this != &other) {
    destroy();

    m_allocator = other.m_allocator;
    m_image = other.m_image;
    m_allocation = other.m_allocation;
    m_extent = other.m_extent;
    m_format = other.m_format;

    other.m_allocator = nullptr;
    other.m_image = VK_NULL_HANDLE;
    other.m_allocation = nullptr;
    other.m_extent = VkExtent3D{};
    other.m_format = VK_FORMAT_UNDEFINED;
  }
  return *this;
}

void AllocatedImage::destroy() {
  if (m_image != VK_NULL_HANDLE && m_allocation != nullptr) {
    vmaDestroyImage(m_allocator, m_image, m_allocation);
    m_image = VK_NULL_HANDLE;
    m_allocation = nullptr;
  }
}

// =================================================================
// 3. GpuAllocator Implementation
// =================================================================

GpuAllocator::GpuAllocator(VulkanContext *context) : m_context(context) {
  if (!m_context || m_context->getDevice() == VK_NULL_HANDLE) {
    atomic::log_error(
        "avk: Cannot initialize GpuAllocator from an uninitialized "
        "Vulkan device.");

    return;
  }

  // Pass ONLY vkGetInstanceProcAddr and vkGetDeviceProcAddr.
  // In dynamic loading mode, VMA will automatically load all other Vulkan
  // pointers itself.
  VmaVulkanFunctions vulkanFunctions{};
  vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = m_context->getPhysicalDevice();
  allocatorInfo.device = m_context->getDevice();
  allocatorInfo.instance = m_context->getInstance();
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorInfo.pVulkanFunctions = &vulkanFunctions;

  if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
    atomic::log_error(
        "avk: Failed to construct VMA master allocator instance.");
    m_allocator = nullptr;
  }
}

GpuAllocator::~GpuAllocator() { release(); }

GpuAllocator::GpuAllocator(GpuAllocator &&other) noexcept {
  *this = std::move(other);
}

GpuAllocator &GpuAllocator::operator=(GpuAllocator &&other) noexcept {
  if (this != &other) {
    release();

    m_context = other.m_context;
    m_allocator = other.m_allocator;

    other.m_context = nullptr;
    other.m_allocator = nullptr;
  }
  return *this;
}

void GpuAllocator::release() {
  if (m_allocator != nullptr) {
    vmaDestroyAllocator(m_allocator);
    m_allocator = nullptr;
  }
}

AllocatedBuffer GpuAllocator::createBuffer(VkDeviceSize size,
                                           VkBufferUsageFlags usage,
                                           VmaMemoryUsage memoryUsage,
                                           VmaAllocationCreateFlags flags) {
  if (m_allocator == nullptr) {
    atomic::log_error(
        "avk: Failed to allocate buffer; allocator is not initialized.");
    return AllocatedBuffer{};
  }

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = memoryUsage;
  allocInfo.flags = flags;

  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation allocation = nullptr;
  VmaAllocationInfo allocationResultInfo{};

  if (vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &buffer,
                      &allocation, &allocationResultInfo) != VK_SUCCESS) {
    atomic::log_error_fmt("avk: Failed to allocate GPU buffer of size %lu",
                          size);
    return AllocatedBuffer{};
  }

  return AllocatedBuffer{m_allocator, buffer, allocation, allocationResultInfo};
}

AllocatedImage GpuAllocator::createImage(const VkImageCreateInfo &imageInfo,
                                         VmaMemoryUsage memoryUsage,
                                         VmaAllocationCreateFlags flags) {
  if (m_allocator == nullptr) {
    atomic::log_error(
        "avk: Failed to allocate image; allocator is not initialized.");
    return AllocatedImage{};
  }

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = memoryUsage;
  allocInfo.flags = flags;

  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = nullptr;

  if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &image, &allocation,
                     nullptr) != VK_SUCCESS) {
    atomic::log_error("avk: Failed to allocate GPU image.");
    return AllocatedImage{};
  }

  return AllocatedImage{m_allocator, image, allocation, imageInfo.extent,
                        imageInfo.format};
}

void GpuAllocator::immediateSubmit(
    std::function<void(VkCommandBuffer)> &&function) {
  VkDevice device = m_context->getDevice();
  VkQueue graphicsQueue = m_context->getGraphicsQueue();
  uint32_t graphicsFamily = m_context->getQueueFamilies().graphicsFamily;

  // 1. Create a temporary transient command pool for this upload
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  poolInfo.queueFamilyIndex = graphicsFamily;

  VkCommandPool commandPool;
  vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

  // 2. Allocate the command buffer from the temporary pool
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer cmd;
  vkAllocateCommandBuffers(device, &allocInfo, &cmd);

  // 3. Begin recording
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmd, &beginInfo);

  // 4. Execute the barrier and copy commands passed via lambda
  function(cmd);

  vkEndCommandBuffer(cmd);

  // 5. Submit to the graphics queue and wait using a fence
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence uploadFence;
  vkCreateFence(device, &fenceInfo, nullptr, &uploadFence);

  vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadFence);
  vkWaitForFences(device, 1, &uploadFence, VK_TRUE, UINT64_MAX);

  // 6. Cleanup (destroying the command pool automatically frees the allocated
  // command buffer)
  vkDestroyFence(device, uploadFence, nullptr);
  vkDestroyCommandPool(device, commandPool, nullptr);
}
} // namespace avk
