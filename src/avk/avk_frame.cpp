#include "avk/avk_frame.h"
#include "avk/avk_core.h"
#include "utils/log.h"
#include <utility>

namespace avk {

FrameContext::FrameContext(VulkanContext *context) : m_context(context) {
  if (!m_context || !m_context->isValid()) {
    atomic::log_error(
        "avk: Cannot initialize FrameContext with an invalid VulkanContext.");
    return;
  }

  VkDevice device = m_context->getDevice();
  uint32_t graphicsFamily = m_context->getQueueFamilies().graphicsFamily;

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = graphicsFamily;

  if (vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPool) !=
      VK_SUCCESS) {
    atomic::log_error("avk: Failed to create command pool for FrameContext.");
    return;
  }

  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  if (vkAllocateCommandBuffers(device, &allocInfo, &m_commandBuffer) !=
      VK_SUCCESS) {
    atomic::log_error(
        "avk: Failed to allocate command buffer for FrameContext.");

    release();
    return;
  }

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                        &m_imageAvailableSemaphore) != VK_SUCCESS) {
    atomic::log_error("avk: Failed to create synchronization semaphore.");
    release();
    return;
  }

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  if (vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFence) !=
      VK_SUCCESS) {
    atomic::log_error("avk: Failed to create synchronization fence.");
    release();
    return;
  }
}

FrameContext::~FrameContext() { release(); }

FrameContext::FrameContext(FrameContext &&other) noexcept {
  *this = std::move(other);
}

FrameContext &FrameContext::operator=(FrameContext &&other) noexcept {
  if (this != &other) {
    release();

    m_context = other.m_context;
    m_commandPool = other.m_commandPool;
    m_commandBuffer = other.m_commandBuffer;
    m_imageAvailableSemaphore = other.m_imageAvailableSemaphore;
    m_inFlightFence = other.m_inFlightFence;

    other.m_context = nullptr;
    other.m_commandPool = VK_NULL_HANDLE;
    other.m_commandBuffer = VK_NULL_HANDLE;
    other.m_imageAvailableSemaphore = VK_NULL_HANDLE;
    other.m_inFlightFence = VK_NULL_HANDLE;
  }
  return *this;
}

bool FrameContext::reset() {
  if (vkResetCommandPool(m_context->getDevice(), m_commandPool, 0) !=
      VK_SUCCESS) {
    atomic::log_error("avk: Failed to bulk reset command pool.");
    return false;
  }
  return true;
}

void FrameContext::release() {
  if (m_context == nullptr)
    return;
  VkDevice device = m_context->getDevice();

  if (m_inFlightFence != VK_NULL_HANDLE) {
    vkDestroyFence(device, m_inFlightFence, nullptr);
    m_inFlightFence = VK_NULL_HANDLE;
  }

  if (m_imageAvailableSemaphore != VK_NULL_HANDLE) {
    vkDestroySemaphore(device, m_imageAvailableSemaphore, nullptr);
    m_imageAvailableSemaphore = VK_NULL_HANDLE;
  }

  if (m_commandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, m_commandPool, nullptr);
    m_commandPool = VK_NULL_HANDLE;
    m_commandBuffer = VK_NULL_HANDLE;
  }
}

} // namespace avk
