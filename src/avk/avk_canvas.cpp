#include "avk/avk_canvas.h"
#include "avk/avk_core.h"
#include "avk/avk_frame.h"
#include "avk/avk_renderer.h"
#include "avk/avk_swapchain.h"
#include <iostream>

namespace avk {

WindowCanvas::WindowCanvas(VulkanContext *context, VkSurfaceKHR surface,
                           uint32_t width, uint32_t height)
    : m_context(context), m_width(width), m_height(height) {

  m_swapchain =
      std::make_unique<VulkanSwapchain>(m_context, surface, width, height);

  m_frames.reserve(MAX_FRAMES_IN_FLIGHT);
  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    m_frames.emplace_back(m_context);
  }

  uint32_t imageCount = m_swapchain->getImageCount();

  m_imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

  m_renderFinishedSemaphores.resize(imageCount);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkDevice device = m_context->getDevice();
  for (uint32_t i = 0; i < imageCount; ++i) {
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
      std::cerr
          << "avk: Failed to create image-indexed render finished semaphore."
          << std::endl;
    }
  }
}

WindowCanvas::~WindowCanvas() {
  VkDevice device = m_context->getDevice();
  vkDeviceWaitIdle(device);

  for (auto semaphore : m_renderFinishedSemaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, semaphore, nullptr);
    }
  }
  m_renderFinishedSemaphores.clear();

  m_deletionQueue.clear();
}

void WindowCanvas::resize(uint32_t width, uint32_t height) {
  if (m_width == width && m_height == height) {
    return;
  }
  if (width == 0 || height == 0) {
    return;
  }

  VkDevice device = m_context->getDevice();
  vkDeviceWaitIdle(device);

  m_width = width;
  m_height = height;

  auto retired = m_swapchain->recreate(m_width, m_height);

  if (retired.swapchain != VK_NULL_HANDLE) {
    DeferredSwapchainTrash trash{};
    trash.swapchain = retired.swapchain;
    trash.imageViews = retired.imageViews;
    trash.framesRemaining = MAX_FRAMES_IN_FLIGHT;
    m_deletionQueue.push_back(trash);
  }

  // 1. Clean out the old semaphores from the previous swapchain allocation
  for (auto semaphore : m_renderFinishedSemaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, semaphore, nullptr);
    }
  }
  m_renderFinishedSemaphores.clear();

  // 2. Fetch the new total image count (e.g., could change or stay at 4)
  uint32_t imageCount = m_swapchain->getImageCount();

  // 3. Re-allocate fresh semaphores to mirror the new swapchain size
  m_renderFinishedSemaphores.resize(imageCount);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  for (uint32_t i = 0; i < imageCount; ++i) {
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
      std::cerr << "avk: Failed to create fresh render semaphore during "
                   "swapchain resize."
                << std::endl;
    }
  }

  // Resize our image-to-fence tracking list to match the newly recreated
  // swapchain size
  m_imagesInFlight.clear();
  m_imagesInFlight.resize(imageCount, VK_NULL_HANDLE);
}

bool WindowCanvas::isActive() const { return m_swapchain->isActive(); }

bool WindowCanvas::beginFrame() {
  if (!m_swapchain->isActive()) {
    return false;
  }

  VkDevice device = m_context->getDevice();
  FrameContext &frame = m_frames[m_currentFrameIndex];

  // 1. Wait for the current frame-in-flight's fence to be signaled (CPU-GPU
  // sync)
  VkFence fence = frame.getInFlightFence();
  vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

  processDeletionQueue();

  // 2. Acquire the next swapchain image
  VkResult result =
      vkAcquireNextImageKHR(device, m_swapchain->getSwapchain(), UINT64_MAX,
                            frame.getImageAvailableSemaphore(), VK_NULL_HANDLE,
                            &m_acquiredImageIndex);

  // -----------------------------------------------------------------
  // THE CRITICAL FIX: Safe exit on out of date/suboptimal resize traps
  // -----------------------------------------------------------------
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    m_swapchain->recreate(m_width, m_height);
    // Do NOT touch m_imagesInFlight here since m_acquiredImageIndex might be
    // garbage or invalid!
    return false;
  } else if (result != VK_SUCCESS) {
    return false;
  }

  // 3. If the acquired swapchain image is currently being used by another
  // frame-in-flight, wait on its fence!
  if (m_imagesInFlight[m_acquiredImageIndex] != VK_NULL_HANDLE) {
    vkWaitForFences(device, 1, &m_imagesInFlight[m_acquiredImageIndex], VK_TRUE,
                    UINT64_MAX);
  }

  // Now it is 100% safe to reset the current frame's fence!
  vkResetFences(device, 1, &fence);

  frame.reset();
  VkCommandBuffer cmd = frame.getCommandBuffer();

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmd, &beginInfo);

  transitionImageLayout(cmd, m_swapchain->getImages()[m_acquiredImageIndex],
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  return true;
}

void WindowCanvas::endFrame(Renderer &renderer) {
  FrameContext &frame = m_frames[m_currentFrameIndex];
  VkCommandBuffer cmd = frame.getCommandBuffer();
  VkExtent2D extent = m_swapchain->getExtent();

  renderer.render(cmd, m_swapchain->getImageViews()[m_acquiredImageIndex],
                  m_swapchain->getFormat(), extent);

  transitionImageLayout(cmd, m_swapchain->getImages()[m_acquiredImageIndex],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  vkEndCommandBuffer(cmd);

  m_imagesInFlight[m_acquiredImageIndex] = frame.getInFlightFence();

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {frame.getImageAvailableSemaphore()};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &cmd;

  VkSemaphore signalSemaphores[] = {
      m_renderFinishedSemaphores[m_acquiredImageIndex]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo,
                frame.getInFlightFence());

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapchains[] = {m_swapchain->getSwapchain()};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &m_acquiredImageIndex;

  vkQueuePresentKHR(m_context->getPresentQueue(), &presentInfo);

  m_currentFrameIndex = (m_currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void WindowCanvas::processDeletionQueue() {
  VkDevice device = m_context->getDevice();

  for (auto it = m_deletionQueue.begin(); it != m_deletionQueue.end();) {
    if (it->framesRemaining > 0) {
      it->framesRemaining--;
      ++it;
    } else {
      for (auto view : it->imageViews) {
        vkDestroyImageView(device, view, nullptr);
      }
      if (it->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, it->swapchain, nullptr);
      }
      it = m_deletionQueue.erase(it);
    }
  }
}

void WindowCanvas::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                         VkImageLayout oldLayout,
                                         VkImageLayout newLayout) {
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
      newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = 0;
    sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  } else {
    return;
  }

  vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

} // namespace avk
