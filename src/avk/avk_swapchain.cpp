#include "avk/avk_swapchain.h"
#include "avk/avk_core.h"
#include <algorithm>
#include <iostream>
#include <utility>

namespace avk {

VulkanSwapchain::VulkanSwapchain(VulkanContext *context, VkSurfaceKHR surface,
                                 uint32_t width, uint32_t height)
    : m_context(context), m_surface(surface) {
  if (!m_context || !m_context->isValid() || m_surface == VK_NULL_HANDLE) {
    std::cerr << "avk: Invalid context or surface provided to swapchain."
              << std::endl;
    return;
  }

  build(width, height);
}

VulkanSwapchain::~VulkanSwapchain() {
  cleanup();

  if (m_surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(m_context->getInstance(), m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
  }
}

VulkanSwapchain::VulkanSwapchain(VulkanSwapchain &&other) noexcept {
  *this = std::move(other);
}

VulkanSwapchain &VulkanSwapchain::operator=(VulkanSwapchain &&other) noexcept {
  if (this != &other) {
    cleanup();
    if (m_surface != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(m_context->getInstance(), m_surface, nullptr);
    }

    m_context = other.m_context;
    m_surface = other.m_surface;
    m_swapchain = other.m_swapchain;
    m_format = other.m_format;
    m_extent = other.m_extent;
    m_images = std::move(other.m_images);
    m_imageViews = std::move(other.m_imageViews);
    m_isActive = other.m_isActive;

    other.m_context = nullptr;
    other.m_surface = VK_NULL_HANDLE;
    other.m_swapchain = VK_NULL_HANDLE;
    other.m_format = VK_FORMAT_UNDEFINED;
    other.m_extent = VkExtent2D{0, 0};
    other.m_isActive = false;
  }
  return *this;
}

RetiredSwapchain VulkanSwapchain::recreate(uint32_t width, uint32_t height) {
  RetiredSwapchain retired;
  retired.swapchain = m_swapchain;
  retired.imageViews = m_imageViews;

  m_imageViews.clear();
  m_images.clear();

  if (!build(width, height)) {
    m_swapchain = retired.swapchain;
    m_imageViews = retired.imageViews;
    return RetiredSwapchain{};
  }

  return retired;
}

void VulkanSwapchain::cleanup() {
  VkDevice device = m_context->getDevice();

  for (auto imageView : m_imageViews) {
    vkDestroyImageView(device, imageView, nullptr);
  }
  m_imageViews.clear();
  m_images.clear();

  if (m_swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, m_swapchain, nullptr);
    m_swapchain = VK_NULL_HANDLE;
  }
  m_isActive = false;
}

bool VulkanSwapchain::build(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    m_isActive = false;
    return false;
  }

  VkPhysicalDevice physicalDevice = m_context->getPhysicalDevice();
  VkDevice device = m_context->getDevice();

  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface,
                                            &capabilities);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount,
                                       nullptr);
  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount,
                                       formats.data());

  VkSurfaceFormatKHR selectedFormat = formats[0]; // Default fallback

  for (const auto &availableFormat : formats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      selectedFormat = availableFormat;
    }
  }
  m_format = selectedFormat.format;

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface,
                                            &presentModeCount, nullptr);
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      physicalDevice, m_surface, &presentModeCount, presentModes.data());

  VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (const auto &mode : presentModes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      selectedPresentMode = mode;
      break;
    }
  }

  m_extent.width = std::clamp(width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
  m_extent.height = std::clamp(height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);

  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = m_format;
  createInfo.imageColorSpace = selectedFormat.colorSpace;
  createInfo.imageExtent = m_extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = m_context->getQueueFamilies();
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily,
                                   indices.presentFamily};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = selectedPresentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = m_swapchain;

  if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain) !=
      VK_SUCCESS) {
    std::cerr << "avk: Failed to construct VkSwapchainKHR." << std::endl;
    return false;
  }

  uint32_t swapchainImageCount = 0;
  vkGetSwapchainImagesKHR(device, m_swapchain, &swapchainImageCount, nullptr);
  m_images.resize(swapchainImageCount);
  vkGetSwapchainImagesKHR(device, m_swapchain, &swapchainImageCount,
                          m_images.data());

  m_imageViews.resize(swapchainImageCount);
  for (size_t i = 0; i < swapchainImageCount; ++i) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_images[i];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_imageViews[i]) !=
        VK_SUCCESS) {
      std::cerr << "avk: Failed to create VkImageView for swapchain element #"
                << i << std::endl;
      return false;
    }
  }

  m_isActive = true;
  return true;
}

} // namespace avk
