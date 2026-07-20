#include "avk/avk_canvas.h"
#include "avk/avk_core.h"
#include "avk/avk_frame.h"
#include "avk/avk_renderer.h"
#include "avk/avk_swapchain.h"

namespace avk {

WindowCanvas::WindowCanvas(VulkanContext *context, VkSurfaceKHR surface,
                           uint32_t width, uint32_t height)
    : m_context(context), m_width(width), m_height(height) {

  m_swapchain =
      std::make_unique<VulkanSwapchain>(m_context, surface, width, height);

  m_frames.reserve(2);
  for (int i = 0; i < 2; ++i) {
    m_frames.emplace_back(m_context);
  }
}

WindowCanvas::~WindowCanvas() {
  vkDeviceWaitIdle(m_context->getDevice());

  m_deletionQueue.clear();
  for (auto &trash : m_deletionQueue) {
    for (auto view : trash.imageViews) {
      vkDestroyImageView(m_context->getDevice(), view, nullptr);
    }
    if (trash.swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(m_context->getDevice(), trash.swapchain, nullptr);
    }
  }
}

void WindowCanvas::resize(uint32_t width, uint32_t height) {
  if (m_width == width && m_height == height) {
    return;
  }
  if (width == 0 || height == 0) {
    return;
  }

  m_width = width;
  m_height = height;

  // Recreate the swapchain instantly, performing the oldSwapchain link-up
  auto retired = m_swapchain->recreate(m_width, m_height);

  // If there was a valid old swapchain, push it to the deferred deletion queue
  if (retired.swapchain != VK_NULL_HANDLE) {
    DeferredSwapchainTrash trash{};
    trash.swapchain = retired.swapchain;
    trash.imageViews = retired.imageViews;
    trash.framesRemaining = 2;
    m_deletionQueue.push_back(trash);
  }
}

bool WindowCanvas::isActive() const { return m_swapchain->isActive(); }

bool WindowCanvas::beginFrame() {
  if (!m_swapchain->isActive()) {
    return false;
  }

  VkDevice device = m_context->getDevice();
  FrameContext &frame = m_frames[m_currentFrameIndex];

  // 1. Wait on and reset the frame fence (GPU completed previous loop pass)
  VkFence fence = frame.getInFlightFence();
  vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
  vkResetFences(device, 1, &fence);

  // 2. Safe cleanup of trash buffers now that the GPU fence is signaled
  processDeletionQueue();

  // 3. Acquire next image
  VkResult result =
      vkAcquireNextImageKHR(device, m_swapchain->getSwapchain(), UINT64_MAX,
                            frame.getImageAvailableSemaphore(), VK_NULL_HANDLE,
                            &m_acquiredImageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    m_swapchain->recreate(m_width, m_height);
    return false;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    return false;
  }

  // 4. Begin Command buffer recording
  frame.reset();
  VkCommandBuffer cmd = frame.getCommandBuffer();

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(cmd, &beginInfo);

  // Transition Layout to render optimal
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

  VkSemaphore signalSemaphores[] = {frame.getRenderFinishedSemaphore()};
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

  m_currentFrameIndex = (m_currentFrameIndex + 1) % 2;
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
