#pragma once

#include <volk.h>

namespace avk {

    class VulkanContext;

    /**
     * @brief Pure RAII container for a single frame-in-flight's execution and synchronization primitives.
     */
    class FrameContext {
    public:
        explicit FrameContext(VulkanContext* context);
        ~FrameContext();

        FrameContext(const FrameContext&) = delete;
        FrameContext& operator=(const FrameContext&) = delete;

        FrameContext(FrameContext&& other) noexcept;
        FrameContext& operator=(FrameContext&& other) noexcept;

        /**
         * @brief Bulk-resets the command pool and prepares the command buffer for recording.
         */
        bool reset();

        VkCommandPool getCommandPool() const { return m_commandPool; }
        VkCommandBuffer getCommandBuffer() const { return m_commandBuffer; }
        VkSemaphore getImageAvailableSemaphore() const { return m_imageAvailableSemaphore; }
        VkSemaphore getRenderFinishedSemaphore() const { return m_renderFinishedSemaphore; }
        VkFence getInFlightFence() const { return m_inFlightFence; }

    private:
        void release();

        VulkanContext* m_context = nullptr;

        VkCommandPool m_commandPool = VK_NULL_HANDLE;
        VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;

        VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
        VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
        VkFence m_inFlightFence = VK_NULL_HANDLE;
    };

} // namespace avk