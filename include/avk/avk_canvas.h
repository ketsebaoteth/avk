#pragma once

#include <volk.h>
#include <vector>
#include <memory>

namespace avk {

class VulkanContext;
class VulkanSwapchain;
class FrameContext;
class Renderer;

/**
 * @brief Trash structure for old swapchain assets awaiting safe GPU completion.
 */
struct DeferredSwapchainTrash {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImageView> imageViews;
    uint32_t framesRemaining = 2; // Safely survives double-buffer flight
};

/**
 * @brief High-level canvas interface. Encapsulates synchronization, swapchain, 
 * layout transitions, and frame-state details.
 */
class WindowCanvas {
public:
    WindowCanvas(VulkanContext* context, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    ~WindowCanvas();

    WindowCanvas(const WindowCanvas&) = delete;
    WindowCanvas& operator=(const WindowCanvas&) = delete;

    bool beginFrame();

    void endFrame(Renderer& renderer);

    void resize(uint32_t width, uint32_t height);

    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }
    bool isActive() const;

private:
    void processDeletionQueue();
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

    VulkanContext* m_context = nullptr;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    std::vector<FrameContext> m_frames;

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