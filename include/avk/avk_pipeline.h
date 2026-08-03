#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <volk.h>

namespace avk {

class VulkanContext;

/**
 * @brief Specialized Pipeline Types to prevent GPU Warp Divergence in Vulkan.
 */
enum class PipelineType : uint8_t {
  Shape = 0, // ui_shape.frag.spv (Boxes, Borders, Gradients, Shadows, Images)
  Text = 1   // ui_text.frag.spv  (Pure MTSDF Vector Text)
};

/**
 * @brief Dynamic Vulkan Graphics Pipeline Cache.
 * Compiles and caches specialized graphics pipelines for specific render
 * formats & types.
 */
class PipelineCache {
public:
  explicit PipelineCache(VulkanContext *context);
  ~PipelineCache();

  PipelineCache(const PipelineCache &) = delete;
  PipelineCache &operator=(const PipelineCache &) = delete;

  PipelineCache(PipelineCache &&other) noexcept;
  PipelineCache &operator=(PipelineCache &&other) noexcept;

  /**
   * @brief Retrieves or compiles a specialized pipeline for a specific output
   * format and type.
   */
  VkPipeline getOrCreatePipeline(VkFormat colorFormat,
                                 PipelineType type = PipelineType::Shape);

  VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

private:
  struct PipelineKey {
    VkFormat format;
    PipelineType type;

    bool operator==(const PipelineKey &other) const {
      return format == other.format && type == other.type;
    }
  };

  struct PipelineKeyHash {
    std::size_t operator()(const PipelineKey &key) const noexcept {
      return std::hash<uint32_t>()(static_cast<uint32_t>(key.format)) ^
             (std::hash<uint8_t>()(static_cast<uint8_t>(key.type)) << 16);
    }
  };

  void release();
  VkShaderModule loadShaderModule(const std::string &fileName);

  VulkanContext *m_context = nullptr;
  VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

  std::unordered_map<PipelineKey, VkPipeline, PipelineKeyHash> m_pipelines;
};

} // namespace avk
