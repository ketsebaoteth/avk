#pragma once

#include "avk_frame.h"
#include "avk_swapchain.h"
#include <memory>
#include <vector>

namespace avk {

class VulkanContext;
class Renderer;

struct DeferredSwapchainTrash {
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImageView> imageViews;
  uint32_t framesRemaining = 2;
};

class WindowCanvas {
public:
  static constexpr uint32_t MAX_FRAMES_IN_FLIGHT =
      2; // Back to standard double-buffering

  WindowCanvas(VulkanContext *context, VkSurfaceKHR surface, uint32_t width,
               uint32_t height);
  ~WindowCanvas();

  WindowCanvas(const WindowCanvas &) = delete;
  WindowCanvas &operator=(const WindowCanvas &) = delete;

  bool beginFrame();
  void endFrame(Renderer &renderer);
  void resize(uint32_t width, uint32_t height);

  uint32_t getWidth() const { return m_width; }
  uint32_t getHeight() const { return m_height; }
  bool isActive() const;

private:
  void processDeletionQueue();
  void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                             VkImageLayout oldLayout, VkImageLayout newLayout);

  VulkanContext *m_context = nullptr;
  std::unique_ptr<VulkanSwapchain> m_swapchain;
  std::vector<FrameContext> m_frames;

  // Maps each swapchain image index to the fence currently synchronizing it
  std::vector<VkFence> m_imagesInFlight;
  std::vector<VkSemaphore> m_renderFinishedSemaphores;

  uint32_t m_width = 0;
  uint32_t m_height = 0;
  uint32_t m_currentFrameIndex = 0;
  uint32_t m_acquiredImageIndex = 0;

  bool m_resizePending = false;
  uint32_t m_pendingWidth = 0;
  uint32_t m_pendingHeight = 0;

  std::vector<DeferredSwapchainTrash> m_deletionQueue;
};

} // namespace avk
