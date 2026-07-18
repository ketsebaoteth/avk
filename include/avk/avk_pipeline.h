#pragma once

#include <volk.h>
#include <unordered_map>
#include <vector>
#include <string>

namespace avk {

    class VulkanContext;

    /**
     * @brief Dynamic Vulkan Graphics Pipeline Cache.
     * Compiles and loads pipelines dynamically using offline compiled SPIR-V shader files on disk.
     */
    class PipelineCache {
    public:
        explicit PipelineCache(VulkanContext* context);
        ~PipelineCache();

        PipelineCache(const PipelineCache&) = delete;
        PipelineCache& operator=(const PipelineCache&) = delete;

        PipelineCache(PipelineCache&& other) noexcept;
        PipelineCache& operator=(PipelineCache&& other) noexcept;

        /**
         * @brief Creates or retrieves a graphics pipeline configured for a specific output format.
         */
        VkPipeline getOrCreatePipeline(VkFormat colorFormat);
    
        VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

    private:
        void release();
        VkShaderModule loadShaderModule(const std::string& fileName);

        VulkanContext* m_context = nullptr;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

        std::unordered_map<VkFormat, VkPipeline> m_pipelines;
    };

} // namespace avk