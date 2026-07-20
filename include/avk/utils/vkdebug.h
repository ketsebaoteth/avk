#include <iostream>
#include <volk.h>
namespace utils::vk {
inline VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  (void)messageSeverity;
  (void)messageType;
  (void)pUserData;
  std::cerr << "[Vulkan Validation]: " << pCallbackData->pMessage << "\n";
  return VK_FALSE;
}
} // namespace utils::vk
