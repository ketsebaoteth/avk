#pragma once

#include "avk/avk_texture.h"
#include "core/app/Types.h"
#include <memory>
#include <volk.h>

#ifdef TRACY_ENABLE
#include <tracy/TracyVulkan.hpp>
#endif

namespace avk {

class GpuAllocator;

struct QueueFamilyIndices {
public:
  uint32_t graphicsFamily = 0xFFFFFFFF;
  uint32_t presentFamily = 0xFFFFFFFF;

  [[nodiscard]] bool isComplete() const {
    return graphicsFamily != 0xFFFFFFFF && presentFamily != 0xFFFFFFFF;
  }
};

/**
 * @brief Vulkan 1.3 Core Context.
 */
class VulkanContext {
public:
  explicit VulkanContext(std::optional<VeraNativeHandle> nativeDisplay,
                         bool enableValidation = false);
  ~VulkanContext();

  VulkanContext(const VulkanContext &) = delete;
  VulkanContext &operator=(const VulkanContext &) = delete;

  VulkanContext(VulkanContext &&other) noexcept;
  VulkanContext &operator=(VulkanContext &&other) noexcept;

  [[nodiscard]] bool isValid() const { return m_isValid; }

  // Generic, window-agnostic surface creation methods
#if defined(VERA_PLATFORM_WIN32)
  VkSurfaceKHR createWin32Surface(void *hwnd, void *hinstance) const;
#elif defined(VERA_PLATFORM_LINUX)
  VkSurfaceKHR createWaylandSurface(void *display, void *surface) const;
  VkSurfaceKHR createX11Surface(void *display, uint64_t window) const;
#endif

  [[nodiscard]] VkInstance getInstance() const { return m_instance; }
  [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const {
    return m_physicalDevice;
  }
  [[nodiscard]] VkDevice getDevice() const { return m_device; }
  [[nodiscard]] VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
  [[nodiscard]] VkQueue getPresentQueue() const { return m_presentQueue; }
  [[nodiscard]] QueueFamilyIndices getQueueFamilies() const {
    return m_queueFamilies;
  }
  [[nodiscard]] GpuAllocator *getAllocator() const { return m_allocator.get(); }
  [[nodiscard]] TextureManager *getTextureManager() const {
    return m_textureManager.get();
  }

#ifdef TRACY_ENABLE
  [[nodiscard]] TracyVkCtx getTracyVkCtx() const { return m_tracyVkCtx; }
#endif

private:
  bool createInstance(bool enableValidation);
  bool setupDebugMessenger(bool enableValidation);
  bool selectPhysicalDevice();
  bool createLogicalDevice();
  static bool
  checkPresentationSupport(VkPhysicalDevice physicalDevice,
                           uint32_t queueFamilyIndex,
                           std::optional<VeraNativeHandle> nativeHandle);
  void releaseResources();
  mutable std::optional<VeraNativeHandle> m_nativeDisplay;

  VkInstance m_instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
  VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkQueue m_graphicsQueue = VK_NULL_HANDLE;
  VkQueue m_presentQueue = VK_NULL_HANDLE;
  QueueFamilyIndices m_queueFamilies;

  std::unique_ptr<GpuAllocator> m_allocator;
  std::unique_ptr<TextureManager> m_textureManager;

#ifdef TRACY_ENABLE
  TracyVkCtx m_tracyVkCtx = nullptr;
  VkCommandPool m_tracyCommandPool = VK_NULL_HANDLE;
  VkCommandBuffer m_tracyCommandBuffer = VK_NULL_HANDLE;
#endif
  bool m_isValid = false;
};

} // namespace avk
