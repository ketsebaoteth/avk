#if defined(VERA_PLATFORM_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(VERA_PLATFORM_LINUX)
#ifndef VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif
#ifndef VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#ifndef VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XCB_KHR
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <wayland-client.h>
#include <xcb/xcb.h>
#endif

#include "avk/avk_allocator.h"
#include "avk/avk_core.h"
#include "avk/utils/vkdebug.h"
#include "core/app/Types.h"

#include <cmath>
#include <cstring>
#include <utility>
#include <vector>

namespace avk {

// Global static flag to prefer Integrated GPUs during physical device selection
static bool g_preferIntegrated = true;

VulkanContext::VulkanContext(std::optional<VeraNativeHandle> nativeDisplay,
                             bool enableValidation) {
  m_nativeDisplay = nativeDisplay;
  if (volkInitialize() != VK_SUCCESS) {
    std::cerr << "avk: Failed to load system Vulkan runtime loader.\n";
    return;
  }

  if (!createInstance(enableValidation))
    return;
  if (!setupDebugMessenger(enableValidation))
    return;
  if (!selectPhysicalDevice())
    return;
  if (!createLogicalDevice())
    return;

  m_allocator = std::make_unique<GpuAllocator>(this);

  if (!m_allocator->isValid()) {
    releaseResources();
    return;
  }

  m_isValid = true;
  m_textureManager = std::make_unique<TextureManager>(this);
#ifdef TRACY_ENABLE
  // Initialize Tracy Vulkan GPU Context with a persistent command pool & buffer
  VkCommandPoolCreateInfo tracyPoolInfo{};
  tracyPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  tracyPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  tracyPoolInfo.queueFamilyIndex = m_queueFamilies.graphicsFamily;

  if (vkCreateCommandPool(m_device, &tracyPoolInfo, nullptr,
                          &m_tracyCommandPool) == VK_SUCCESS) {
    VkCommandBufferAllocateInfo tracyAllocInfo{};
    tracyAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    tracyAllocInfo.commandPool = m_tracyCommandPool;
    tracyAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    tracyAllocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(m_device, &tracyAllocInfo,
                                 &m_tracyCommandBuffer) == VK_SUCCESS) {
      m_tracyVkCtx = TracyVkContext(m_physicalDevice, m_device, m_graphicsQueue,
                                    m_tracyCommandBuffer);
    }
  }
#endif
}

VulkanContext::~VulkanContext() { releaseResources(); }

VulkanContext::VulkanContext(VulkanContext &&other) noexcept {
  *this = std::move(other);
}

VulkanContext &VulkanContext::operator=(VulkanContext &&other) noexcept {
  if (this != &other) {
    releaseResources();

    m_instance = other.m_instance;
    m_debugMessenger = other.m_debugMessenger;
    m_physicalDevice = other.m_physicalDevice;
    m_device = other.m_device;
    m_graphicsQueue = other.m_graphicsQueue;
    m_presentQueue = other.m_presentQueue;
    m_queueFamilies = other.m_queueFamilies;
    m_allocator = std::move(other.m_allocator);
    m_isValid = other.m_isValid;

    other.m_instance = VK_NULL_HANDLE;
    other.m_debugMessenger = VK_NULL_HANDLE;
    other.m_physicalDevice = VK_NULL_HANDLE;
    other.m_device = VK_NULL_HANDLE;
    other.m_graphicsQueue = VK_NULL_HANDLE;
    other.m_presentQueue = VK_NULL_HANDLE;
    other.m_queueFamilies = QueueFamilyIndices{};
    other.m_isValid = false;
  }
  return *this;
}

void VulkanContext::releaseResources() {
#ifdef TRACY_ENABLE
  if (m_tracyVkCtx != nullptr) {
    TracyVkDestroy(m_tracyVkCtx);
    m_tracyVkCtx = nullptr;
  }

  if (m_tracyCommandPool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(m_device, m_tracyCommandPool, nullptr);
    m_tracyCommandPool = VK_NULL_HANDLE;
    m_tracyCommandBuffer = VK_NULL_HANDLE;
  }
#endif
  m_textureManager.reset();
  m_allocator.reset();

  if (m_device != VK_NULL_HANDLE) {
    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;
  }

  if (m_debugMessenger != VK_NULL_HANDLE) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        m_instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
      func(m_instance, m_debugMessenger, nullptr);
    }
    m_debugMessenger = VK_NULL_HANDLE;
  }

  if (m_instance != VK_NULL_HANDLE) {
    vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
  }

  m_isValid = false;
}

#if defined(VERA_PLATFORM_WIN32)
VkSurfaceKHR VulkanContext::createWin32Surface(void *hwnd,
                                               void *hinstance) const {
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkWin32SurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hwnd = static_cast<HWND>(hwnd);
  createInfo.hinstance = static_cast<HINSTANCE>(hinstance);

  if (vkCreateWin32SurfaceKHR(m_instance, &createInfo, nullptr, &surface) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to generate Win32 surface." << std::endl;
  }
  return surface;
}
#elif defined(VERA_PLATFORM_LINUX)
VkSurfaceKHR VulkanContext::createWaylandSurface(void *display,
                                                 void *surface) const {
  VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
  VkWaylandSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
  createInfo.display = static_cast<wl_display *>(display);
  createInfo.surface = static_cast<wl_surface *>(surface);

  if (vkCreateWaylandSurfaceKHR(m_instance, &createInfo, nullptr, &vkSurface) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to generate Wayland surface." << std::endl;
  }
  return vkSurface;
}

VkSurfaceKHR VulkanContext::createX11Surface(void *display,
                                             uint64_t window) const {
  VkSurfaceKHR vkSurface = VK_NULL_HANDLE;
  VkXlibSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
  createInfo.dpy = static_cast<Display *>(display);
  createInfo.window = static_cast<Window>(window);

  if (vkCreateXlibSurfaceKHR(m_instance, &createInfo, nullptr, &vkSurface) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to generate X11 surface." << std::endl;
  }
  return vkSurface;
}
#endif

bool VulkanContext::createInstance(bool enableValidation) {
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "atomic_vk Application";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "atomic_vk";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                         availableExtensions.data());

  auto isExtensionSupported = [&](const char *extName) {
    for (const auto &ext : availableExtensions) {
      if (std::strcmp(ext.extensionName, extName) == 0)
        return true;
    }
    return false;
  };

  std::vector<const char *> extensions;
  if (isExtensionSupported(VK_KHR_SURFACE_EXTENSION_NAME)) {
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
  }

#if defined(VERA_PLATFORM_WIN32)
  if (isExtensionSupported("VK_KHR_win32_surface")) {
    extensions.push_back("VK_KHR_win32_surface");
  }
#elif defined(VERA_PLATFORM_LINUX)
  if (isExtensionSupported("VK_KHR_wayland_surface")) {
    extensions.push_back("VK_KHR_wayland_surface");
  }
  if (isExtensionSupported("VK_KHR_xlib_surface")) {
    extensions.push_back("VK_KHR_xlib_surface");
  }
  if (isExtensionSupported("VK_KHR_xcb_surface")) {
    extensions.push_back("VK_KHR_xcb_surface");
  }
#endif

  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  bool validationSupported = false;
  if (enableValidation) {
    for (const auto &layer : availableLayers) {
      if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
        validationSupported = true;
        break;
      }
    }
  }

  std::vector<const char *> validationLayers;
  if (enableValidation && validationSupported) {
    validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    if (isExtensionSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
  }

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();
  createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
  createInfo.ppEnabledLayerNames = validationLayers.data();

  VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
  if (result != VK_SUCCESS) {
    std::cerr << "avk: vkCreateInstance failed with error code: " << result
              << std::endl;
    return false;
  }

  volkLoadInstance(m_instance);
  return true;
}

bool VulkanContext::setupDebugMessenger(bool enableValidation) {
  if (!enableValidation)
    return true;

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = utils::vk::debugCallback;

  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      m_instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(m_instance, &createInfo, nullptr, &m_debugMessenger) ==
           VK_SUCCESS;
  }
  return false;
}

bool VulkanContext::selectPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    std::cerr << "avk: No physical devices found with Vulkan support."
              << std::endl;
    return false;
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

  VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
  QueueFamilyIndices bestIndices;
  int highestScore = -1;

  for (const auto &device : devices) {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    if (deviceProperties.apiVersion < VK_API_VERSION_1_3) {
      continue;
    }

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             queueFamilies.data());

    QueueFamilyIndices indices;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
      if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily = i;
      }

      if (checkPresentationSupport(device, i, m_nativeDisplay)) {
        indices.presentFamily = i;
      }

      if (indices.isComplete()) {
        break;
      }
    }

    if (!indices.isComplete()) {
      continue;
    }

    int score = 0;

    if (g_preferIntegrated) {
      if (deviceProperties.deviceType ==
          VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 10000;
      } else if (deviceProperties.deviceType ==
                 VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
      }
    } else {
      if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 10000;
      } else if (deviceProperties.deviceType ==
                 VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 1000;
      }
    }

    if (deviceProperties.vendorID == 0x10DE) { // NVIDIA
      score += 5000;
    } else if (deviceProperties.vendorID == 0x1002) { // AMD
      score += 4000;
    }

    if (score > highestScore) {
      highestScore = score;
      bestDevice = device;
      bestIndices = indices;
    }
  }

  if (bestDevice == VK_NULL_HANDLE) {
    std::cerr << "avk: Failed to find a suitable GPU with presentation support."
              << std::endl;
    return false;
  }

  m_physicalDevice = bestDevice;
  m_queueFamilies = bestIndices;

  return true;
}

bool VulkanContext::createLogicalDevice() {
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  float queuePriority = 1.0f;

  VkDeviceQueueCreateInfo graphicsQueueCreateInfo{};
  graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  graphicsQueueCreateInfo.queueFamilyIndex = m_queueFamilies.graphicsFamily;
  graphicsQueueCreateInfo.queueCount = 1;
  graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;
  queueCreateInfos.push_back(graphicsQueueCreateInfo);

  if (m_queueFamilies.graphicsFamily != m_queueFamilies.presentFamily) {
    VkDeviceQueueCreateInfo presentQueueCreateInfo{};
    presentQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    presentQueueCreateInfo.queueFamilyIndex = m_queueFamilies.presentFamily;
    presentQueueCreateInfo.queueCount = 1;
    presentQueueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(presentQueueCreateInfo);
  }

  VkPhysicalDeviceVulkan12Features features12{};
  features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  features12.descriptorBindingPartiallyBound = VK_TRUE;
  features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  features12.runtimeDescriptorArray = VK_TRUE;
  features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.dynamicRendering = VK_TRUE;
  features13.pNext = &features12;

  VkPhysicalDeviceFeatures2 deviceFeatures2{};
  deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  deviceFeatures2.pNext = &features13;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pNext = &deviceFeatures2;
  createInfo.queueCreateInfoCount =
      static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();

  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to construct Vulkan logical device." << std::endl;
    return false;
  }

  volkLoadDevice(m_device);

  vkGetDeviceQueue(m_device, m_queueFamilies.graphicsFamily, 0,
                   &m_graphicsQueue);
  vkGetDeviceQueue(m_device, m_queueFamilies.presentFamily, 0, &m_presentQueue);

  return true;
}

bool VulkanContext::checkPresentationSupport(
    [[maybe_unused]] VkPhysicalDevice physicalDevice,
    [[maybe_unused]] uint32_t queueFamilyIndex,
    [[maybe_unused]] std::optional<VeraNativeHandle> nativeHandle) {
#if defined(VERA_PLATFORM_WIN32)
  return vkGetPhysicalDeviceWin32PresentationSupportKHR(
             physicalDevice, queueFamilyIndex) == VK_TRUE;

#elif defined(VERA_PLATFORM_LINUX)

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
  if (vkGetPhysicalDeviceWaylandPresentationSupportKHR != nullptr &&
      nativeHandle.has_value()) {
    if (vkGetPhysicalDeviceWaylandPresentationSupportKHR(
            physicalDevice, queueFamilyIndex,
            reinterpret_cast<struct ::wl_display *>(nativeHandle->display)) ==
        VK_TRUE) {
      return true;
    }
  }
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
  if (vkGetPhysicalDeviceXlibPresentationSupportKHR != nullptr &&
      nativeHandle.has_value()) {
    auto *xlibDisplay = static_cast<Display *>(nativeHandle->display);
    if (xlibDisplay != nullptr) {
      VisualID visualId = XVisualIDFromVisual(
          XDefaultVisual(xlibDisplay, XDefaultScreen(xlibDisplay)));
      if (vkGetPhysicalDeviceXlibPresentationSupportKHR(
              physicalDevice, queueFamilyIndex, xlibDisplay, visualId) ==
          VK_TRUE) {
        return true;
      }
    }
  }
#endif

#if defined(VK_USE_PLATFORM_XCB_KHR)
  if (vkGetPhysicalDeviceXcbPresentationSupportKHR != nullptr &&
      nativeHandle.has_value()) {
    auto *xcbConnection =
        static_cast<xcb_connection_t *>(nativeHandle->display);
    xcb_visualid_t xcbVisualId = 0;
    if (vkGetPhysicalDeviceXcbPresentationSupportKHR(
            physicalDevice, queueFamilyIndex, xcbConnection, xcbVisualId) ==
        VK_TRUE) {
      return true;
    }
  }
#endif

  return false;
#else
  return false;
#endif
}

} // namespace avk
