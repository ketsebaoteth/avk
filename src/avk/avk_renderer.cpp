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

  // Allocate memory for the instanced draw batch.
  // We map it permanently on creation using VMA flags to allow sequential-write
  // host-access.
  VkDeviceSize instanceBufferSize = sizeof(InstanceData) * m_maxInstances;
  m_instanceBuffer = m_context->getAllocator()->createBuffer(
      instanceBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
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
  if (m_drawQueue.empty()) {
    return;
  }

  // 1. Copy instance list to permanently mapped GPU memory via fast memcpy
  std::memcpy(m_instanceBuffer.getMappedData(), m_drawQueue.data(),
              m_drawQueue.size() * sizeof(InstanceData));

  // 2. Fetch the format-specific graphics pipeline
  VkPipeline pipeline = m_pipelineCache->getOrCreatePipeline(targetFormat);
  if (pipeline == VK_NULL_HANDLE) {
    return;
  }

  // 3. Configure dynamic rendering attachment
  VkRenderingAttachmentInfo colorAttachmentInfo{};
  colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAttachmentInfo.imageView = targetView;
  colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  // We use LOAD to draw the UI transparently on top of existing background
  // renderings
  colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  VkRenderingInfo renderingInfo{};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea.offset = {0, 0};
  renderingInfo.renderArea.extent = extent;
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &colorAttachmentInfo;

  // begin dynamic rendering pass
  vkCmdBeginRendering(cmd, &renderingInfo);

  // bind Shader pipeline
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  // for textures
  VkDescriptorSet bindlessSet =
      m_context->getTextureManager()->getDescriptorSet();
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_pipelineCache->getPipelineLayout(), 0, 1,
                          &bindlessSet, 0, nullptr);

  // Set dynamic viewport
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(extent.width);
  viewport.height = static_cast<float>(extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  // Set dynamic scissor
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = extent;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  // Push screen resolution constants to Vertex Shader
  float screenSize[2] = {static_cast<float>(extent.width),
                         static_cast<float>(extent.height)};
  vkCmdPushConstants(cmd, m_pipelineCache->getPipelineLayout(),
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(screenSize),
                     screenSize);

  // Bind Quad Vertex Buffer (Binding 0)
  VkBuffer vertexBuffers[] = {m_quadVertexBuffer.getBuffer()};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

  // Bind Instance Buffer (Binding 1)
  VkBuffer instanceBuffers[] = {m_instanceBuffer.getBuffer()};
  VkDeviceSize instanceOffsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 1, 1, instanceBuffers, instanceOffsets);

  // Record Draw command
  vkCmdDraw(cmd, 6, static_cast<uint32_t>(m_drawQueue.size()), 0, 0);

  // End dynamic rendering pass
  vkCmdEndRendering(cmd);
}

void Renderer::buildQuadBuffer() {
  // 2D Unit Quad representing a simple 6-vertex canvas
  const std::vector<Vertex> quadVertices = {
      {{0.0f, 0.0f}, {0.0f, 0.0f}}, // Tri 1
      {{1.0f, 0.0f}, {1.0f, 0.0f}}, {{1.0f, 1.0f}, {1.0f, 1.0f}},

      {{0.0f, 0.0f}, {0.0f, 0.0f}}, // Tri 2
      {{1.0f, 1.0f}, {1.0f, 1.0f}}, {{0.0f, 1.0f}, {0.0f, 1.0f}}};

  VkDeviceSize bufferSize = sizeof(Vertex) * quadVertices.size();

  // Allocate host-visible coherent buffer using VMA wrapper
  m_quadVertexBuffer = m_context->getAllocator()->createBuffer(
      bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
      VMA_ALLOCATION_CREATE_MAPPED_BIT |
          VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

  std::memcpy(m_quadVertexBuffer.getMappedData(), quadVertices.data(),
              bufferSize);
}

} // namespace avk
