#pragma once

#include <volk.h>
#include <vector>

namespace avk {

    class VulkanContext;

    /**
     * @brief Structure returned during a hot swapchain recreation.
     */
    struct RetiredSwapchain {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkImageView> imageViews;
    };

    class VulkanSwapchain {
    public:
        VulkanSwapchain(VulkanContext* context, VkSurfaceKHR surface, uint32_t width, uint32_t height);
        ~VulkanSwapchain();

        VulkanSwapchain(const VulkanSwapchain&) = delete;
        VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

        VulkanSwapchain(VulkanSwapchain&& other) noexcept;
        VulkanSwapchain& operator=(VulkanSwapchain&& other) noexcept;

        /**
         * @brief Recreates the swapchain, linking to the old swapchain, and returns the retired handles.
         */
        RetiredSwapchain recreate(uint32_t width, uint32_t height);

        VkSwapchainKHR getSwapchain() const { return m_swapchain; }
        VkSurfaceKHR getSurface() const { return m_surface; }
        VkFormat getFormat() const { return m_format; }
        VkExtent2D getExtent() const { return m_extent; }

        const std::vector<VkImage>& getImages() const { return m_images; }
        const std::vector<VkImageView>& getImageViews() const { return m_imageViews; }
        uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }
        bool isActive() const { return m_isActive; }

    private:
        void cleanup();
        bool build(uint32_t width, uint32_t height);

        VulkanContext* m_context = nullptr;

        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{ 0, 0 };

        std::vector<VkImage> m_images;
        std::vector<VkImageView> m_imageViews;

        bool m_isActive = false;
    };

} // namespace avk