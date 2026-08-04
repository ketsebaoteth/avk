#include "avk/avk_renderer.h"
#include "avk/avk_core.h"
#include "avk/avk_pipeline.h"
#include <cstring>
#include <iostream>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace avk {

Renderer::Renderer(VulkanContext *context) : m_context(context) {
  if (!m_context || !m_context->isValid()) {
    std::cerr
        << "avk: Cannot initialize Renderer with an invalid VulkanContext."
        << std::endl;
    return;
  }

  m_pipelineCache = std::make_unique<PipelineCache>(m_context);

  buildQuadBuffer();

  constexpr uint32_t FRAMES_IN_FLIGHT = 3;
  VkDeviceSize singleFrameSize = sizeof(InstanceData) * m_maxInstances;
  VkDeviceSize totalBufferSize = singleFrameSize * FRAMES_IN_FLIGHT;

  m_instanceBuffer = m_context->getAllocator()->createBuffer(
      totalBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
}

Renderer::~Renderer() { release(); }

Renderer::Renderer(Renderer &&other) noexcept { *this = std::move(other); }

Renderer &Renderer::operator=(Renderer &&other) noexcept {
  if (this != &other) {
    release();

    m_context = other.m_context;
    m_pipelineCache = std::move(other.m_pipelineCache);
    m_quadVertexBuffer = std::move(other.m_quadVertexBuffer);
    m_instanceBuffer = std::move(other.m_instanceBuffer);
    m_drawQueue = std::move(other.m_drawQueue);
    m_frameIndex = other.m_frameIndex;

    other.m_context = nullptr;
  }
  return *this;
}

void Renderer::release() {
  m_quadVertexBuffer.destroy();
  m_instanceBuffer.destroy();
  m_pipelineCache.reset();
}

void Renderer::begin() { m_drawQueue.clear(); }

void Renderer::submit(const InstanceData &instance) {
  if (m_drawQueue.size() >= m_maxInstances) {
    std::cerr << "avk: Draw queue limit exceeded! Max capacity: "
              << m_maxInstances << std::endl;
    return;
  }
  m_drawQueue.push_back(instance);
}

void Renderer::render(VkCommandBuffer cmd, VkImageView targetView,
                      VkFormat targetFormat, VkExtent2D extent) {
  ZoneScopedN("Renderer_Render");

  if (m_drawQueue.empty()) {
    return;
  }

  // 1. Copy instance list to mapped buffer
  constexpr uint32_t FRAMES_IN_FLIGHT = 3;
  uint32_t bufferFrameIndex = m_frameIndex % FRAMES_IN_FLIGHT;
  VkDeviceSize singleFrameByteSize = sizeof(InstanceData) * m_maxInstances;
  VkDeviceSize currentFrameOffset = bufferFrameIndex * singleFrameByteSize;

  {
    ZoneScopedN("Renderer_CopyInstanceBuffer");
    uint8_t *mappedPtr =
        static_cast<uint8_t *>(m_instanceBuffer.getMappedData());
    std::memcpy(mappedPtr + currentFrameOffset, m_drawQueue.data(),
                m_drawQueue.size() * sizeof(InstanceData));
  }

  m_frameIndex++;

  // Configure dynamic rendering attachment
  VkRenderingAttachmentInfo colorAttachmentInfo{};
  colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachmentInfo.imageView = targetView;
  colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  VkRenderingInfo renderingInfo{};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea.offset = {0, 0};
  renderingInfo.renderArea.extent = extent;
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &colorAttachmentInfo;

  // =========================================================================
  // BEGIN Dynamic Rendering Pass
  // =========================================================================
  vkCmdBeginRendering(cmd, &renderingInfo);

#ifdef TRACY_ENABLE
  // ✅ FIX: Extract context handle into the enclosing function scope
  // so TracyVkZone stays alive for the whole render pass!
  TracyVkCtx tracyVkCtx = m_context ? m_context->getTracyVkCtx() : nullptr;
  TracyVkZone(tracyVkCtx, cmd, "Vulkan_UI_Render_Pass");
#endif

  {
    ZoneScopedN("Renderer_RecordDrawCommands");

    // Bindless Descriptor Set
    VkDescriptorSet bindlessSet =
        m_context->getTextureManager()->getDescriptorSet();
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineCache->getPipelineLayout(), 0, 1,
                            &bindlessSet, 0, nullptr);

    // Viewport & Scissor
    VkViewport viewport{0.0f,
                        0.0f,
                        static_cast<float>(extent.width),
                        static_cast<float>(extent.height),
                        0.0f,
                        1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Push Constants
    float screenSize[2] = {static_cast<float>(extent.width),
                           static_cast<float>(extent.height)};
    vkCmdPushConstants(cmd, m_pipelineCache->getPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(screenSize),
                       screenSize);

    // Bind Buffers
    VkBuffer vertexBuffers[] = {m_quadVertexBuffer.getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

    VkBuffer instanceBuffers[] = {m_instanceBuffer.getBuffer()};
    VkDeviceSize instanceOffsets[] = {currentFrameOffset};
    vkCmdBindVertexBuffers(cmd, 1, 1, instanceBuffers, instanceOffsets);

    // -------------------------------------------------------------------------
    // ⚡ DYNAMIC PIPELINE SWITCHING (Eliminates GPU Warp Divergence)
    // -------------------------------------------------------------------------
    uint32_t batchStart = 0;
    PipelineType currentType = (m_drawQueue[0].fillType == 3)
                                   ? PipelineType::Text
                                   : PipelineType::Shape;
    VkPipeline currentPipeline =
        m_pipelineCache->getOrCreatePipeline(targetFormat, currentType);

    if (currentPipeline != VK_NULL_HANDLE) {
      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline);
    }

    uint32_t totalInstances = static_cast<uint32_t>(m_drawQueue.size());

    for (uint32_t i = 0; i < totalInstances; ++i) {
      PipelineType instanceType = (m_drawQueue[i].fillType == 3)
                                      ? PipelineType::Text
                                      : PipelineType::Shape;

      if (instanceType != currentType) {
        uint32_t batchCount = i - batchStart;
        if (batchCount > 0 && currentPipeline != VK_NULL_HANDLE) {
          vkCmdDraw(cmd, 6, batchCount, 0, batchStart);
        }

        currentType = instanceType;
        currentPipeline =
            m_pipelineCache->getOrCreatePipeline(targetFormat, currentType);
        if (currentPipeline != VK_NULL_HANDLE) {
          vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            currentPipeline);
        }
        batchStart = i;
      }
    }

    // Flush remaining batch
    uint32_t remainingCount = totalInstances - batchStart;
    if (remainingCount > 0 && currentPipeline != VK_NULL_HANDLE) {
      vkCmdDraw(cmd, 6, remainingCount, 0, batchStart);
    }
  }

  // =========================================================================
  // END Dynamic Rendering Pass
  // =========================================================================
  vkCmdEndRendering(cmd);

#ifdef TRACY_ENABLE
  if (tracyVkCtx) {
    TracyVkCollect(tracyVkCtx, cmd);
  }
#endif
}

void Renderer::buildQuadBuffer() {
  const std::vector<Vertex> quadVertices = {
      {{0.0f, 0.0f}, {0.0f, 0.0f}}, {{1.0f, 0.0f}, {1.0f, 0.0f}},
      {{1.0f, 1.0f}, {1.0f, 1.0f}},

      {{0.0f, 0.0f}, {0.0f, 0.0f}}, {{1.0f, 1.0f}, {1.0f, 1.0f}},
      {{0.0f, 1.0f}, {0.0f, 1.0f}}};

  VkDeviceSize bufferSize = sizeof(Vertex) * quadVertices.size();

  m_quadVertexBuffer = m_context->getAllocator()->createBuffer(
      bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  std::memcpy(m_quadVertexBuffer.getMappedData(), quadVertices.data(),
              bufferSize);
}

} // namespace avk
