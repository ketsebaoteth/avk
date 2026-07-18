#pragma once

#include "avk_types.h"
#include "avk_allocator.h"
#include <volk.h>
#include <vector>
#include <memory>

namespace avk {

class VulkanContext;
class PipelineCache;

/**
 * @brief Stateless, pure-Vulkan UI Rasterizer.
 * Processes instanced immediate-mode shapes and records commands directly into active command streams.
 */
class Renderer {
public:
    explicit Renderer(VulkanContext* context);
    ~Renderer();

    // Disable copies
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Enable moves
    Renderer(Renderer&& other) noexcept;
    Renderer& operator=(Renderer&& other) noexcept;

    /**
     * @brief Begins an immediate-mode rendering scope, clearing the draw queue.
     */
    void begin();

    /**
     * @brief Submits a shape instance block for batch processing.
     */
    void submit(const InstanceData& instance);

    /**
     * @brief Records the drawing commands into a Vulkan command list using Dynamic Rendering.
     * @param cmd Current frame command list.
     * @param targetView The output image view (e.g. current swapchain render target).
     * @param targetFormat Color format of the render target (required for pipeline verification).
     * @param extent Pixel boundary of the output canvas.
     */
    void render(VkCommandBuffer cmd, VkImageView targetView, VkFormat targetFormat, VkExtent2D extent);

private:
    void release();
    void buildQuadBuffer();

    VulkanContext* m_context = nullptr;
    std::unique_ptr<PipelineCache> m_pipelineCache;

    AllocatedBuffer m_quadVertexBuffer;
    AllocatedBuffer m_instanceBuffer;

    std::vector<InstanceData> m_drawQueue;

    const size_t m_maxInstances = 20000;
};

} // namespace avk