#pragma once

#include <volk.h>
#include <vector>
#include <memory>

namespace avk {

class GpuAllocator;

struct QueueFamilyIndices {
    uint32_t graphicsFamily = 0xFFFFFFFF;
    uint32_t presentFamily  = 0xFFFFFFFF;

    bool isComplete() const {
        return graphicsFamily != 0xFFFFFFFF && presentFamily != 0xFFFFFFFF;
    }
};

/**
 * @brief Pure RAII Vulkan 1.3 Core Context (Zero Windowing dependencies).
 */
class VulkanContext {
public:
    explicit VulkanContext(bool enableValidation = false);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VulkanContext(VulkanContext&& other) noexcept;
    VulkanContext& operator=(VulkanContext&& other) noexcept;

    bool isValid() const { return m_isValid; }

    // Generic, window-agnostic surface creation methods
#if defined(VERA_PLATFORM_WIN32)
    VkSurfaceKHR createWin32Surface(void* hwnd, void* hinstance) const;
#elif defined(VERA_PLATFORM_LINUX)
    VkSurfaceKHR createWaylandSurface(void* display, void* surface) const;
    VkSurfaceKHR createX11Surface(void* display, uint64_t window) const;
#endif

    VkInstance getInstance() const { return m_instance; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice getDevice() const { return m_device; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }
    QueueFamilyIndices getQueueFamilies() const { return m_queueFamilies; }
    GpuAllocator* getAllocator() const { return m_allocator.get(); }

private:
    bool createInstance(bool enableValidation);
    bool setupDebugMessenger(bool enableValidation);
    bool selectPhysicalDevice();
    bool createLogicalDevice();
    bool checkPresentationSupport(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex);
    void releaseResources();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilies;

    std::unique_ptr<GpuAllocator> m_allocator;

    bool m_isValid = false;
};

} // namespace avk