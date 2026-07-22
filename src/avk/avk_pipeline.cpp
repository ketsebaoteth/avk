#include "avk/avk_pipeline.h"
#include "avk/avk_core.h"
#include "avk/avk_types.h"
#include <fstream>
#include <iostream>
#include <utility>

namespace avk {

PipelineCache::PipelineCache(VulkanContext *context) : m_context(context) {
  if (!m_context || !m_context->isValid()) {
    std::cerr
        << "avk: Cannot initialize PipelineCache with an invalid VulkanContext."
        << std::endl;
    return;
  }

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pushConstantRange.offset = 0;
  pushConstantRange.size = sizeof(float) * 2;

  VkDescriptorSetLayout layouts[] = {
      m_context->getTextureManager()->getDescriptorSetLayout()};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = layouts;
  pipelineLayoutInfo.pushConstantRangeCount = 1;
  pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

  if (vkCreatePipelineLayout(m_context->getDevice(), &pipelineLayoutInfo,
                             nullptr, &m_pipelineLayout) != VK_SUCCESS) {
    std::cerr << "avk: Failed to build Pipeline Layout." << std::endl;
  }
}

PipelineCache::~PipelineCache() { release(); }

PipelineCache::PipelineCache(PipelineCache &&other) noexcept {
  *this = std::move(other);
}

PipelineCache &PipelineCache::operator=(PipelineCache &&other) noexcept {
  if (this != &other) {
    release();

    m_context = other.m_context;
    m_pipelineLayout = other.m_pipelineLayout;
    m_pipelines = std::move(other.m_pipelines);

    other.m_context = nullptr;
    other.m_pipelineLayout = VK_NULL_HANDLE;
  }
  return *this;
}

void PipelineCache::release() {
  VkDevice device = m_context->getDevice();
  if (device == VK_NULL_HANDLE)
    return;

  for (auto &[format, pipeline] : m_pipelines) {
    vkDestroyPipeline(device, pipeline, nullptr);
  }
  m_pipelines.clear();

  if (m_pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    m_pipelineLayout = VK_NULL_HANDLE;
  }
}

VkPipeline PipelineCache::getOrCreatePipeline(VkFormat colorFormat) {
  auto it = m_pipelines.find(colorFormat);
  if (it != m_pipelines.end()) {
    return it->second;
  }

  VkDevice device = m_context->getDevice();

  // Load compiled shader files from the CMake configured binary directory
  std::string vertPath = std::string(AVK_SHADER_BINARY_DIR) + "/ui.vert.spv";
  std::string fragPath = std::string(AVK_SHADER_BINARY_DIR) + "/ui.frag.spv";

  VkShaderModule vertModule = loadShaderModule(vertPath);
  VkShaderModule fragModule = loadShaderModule(fragPath);

  if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
    std::cerr << "avk: Failed to compile graphics pipeline due to missing "
                 "shader assets."
              << std::endl;
    return VK_NULL_HANDLE;
  }

  VkPipelineShaderStageCreateInfo shaderStages[2]{};
  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = vertModule;
  shaderStages[0].pName = "main";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = fragModule;
  shaderStages[1].pName = "main";

  std::vector<VkVertexInputBindingDescription> bindings = {
      {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX},
      {1, sizeof(InstanceData), VK_VERTEX_INPUT_RATE_INSTANCE}};

  std::vector<VkVertexInputAttributeDescription> attributes = {
      {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, pos)},
      {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)},
      {2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, rectXYWH)},
      {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT,
       offsetof(InstanceData, borderRadius)},
      {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, fillColorA)},
      {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, fillColorB)},
      {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT,
       offsetof(InstanceData, strokeColor)},
      {7, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(InstanceData, gradientStart)},
      {8, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(InstanceData, gradientEnd)},
      {9, 1, VK_FORMAT_R32G32B32A32_SFLOAT,
       offsetof(InstanceData, strokeFillColorA)},
      {10, 1, VK_FORMAT_R32G32B32A32_SFLOAT,
       offsetof(InstanceData, strokeFillColorB)},
      {11, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, clipRect)},
      {12, 1, VK_FORMAT_R32_SFLOAT, offsetof(InstanceData, strokeThickness)},
      {13, 1, VK_FORMAT_R32_UINT, offsetof(InstanceData, shapeType)},
      {14, 1, VK_FORMAT_R32_UINT, offsetof(InstanceData, fillType)},
      {15, 1, VK_FORMAT_R32_UINT, offsetof(InstanceData, textureIndex)},
      {16, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(InstanceData, uvBounds)},
      {17, 1, VK_FORMAT_R32_SFLOAT, offsetof(InstanceData, blur)}};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount =
      static_cast<uint32_t>(bindings.size());
  vertexInputInfo.pVertexBindingDescriptions = bindings.data();
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attributes.size());
  vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;

  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  VkPipelineRenderingCreateInfo renderingCreateInfo{};
  renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingCreateInfo.colorAttachmentCount = 1;
  renderingCreateInfo.pColorAttachmentFormats = &colorFormat;

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &renderingCreateInfo;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = m_pipelineLayout;
  pipelineInfo.renderPass = VK_NULL_HANDLE;
  pipelineInfo.subpass = 0;

  VkPipeline pipeline = VK_NULL_HANDLE;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                nullptr, &pipeline) != VK_SUCCESS) {
    std::cerr << "avk: Failed to compile graphics pipeline." << std::endl;
    pipeline = VK_NULL_HANDLE;
  }

  vkDestroyShaderModule(device, vertModule, nullptr);
  vkDestroyShaderModule(device, fragModule, nullptr);

  if (pipeline != VK_NULL_HANDLE) {
    m_pipelines[colorFormat] = pipeline;
  }

  return pipeline;
}

VkShaderModule PipelineCache::loadShaderModule(const std::string &fileName) {
  std::ifstream file(fileName, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "avk: Failed to open compiled shader binary file: " << fileName
              << std::endl;
    return VK_NULL_HANDLE;
  }

  size_t fileSize = static_cast<size_t>(file.tellg());
  std::vector<char> buffer(fileSize);
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = buffer.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

  VkShaderModule shaderModule = VK_NULL_HANDLE;
  if (vkCreateShaderModule(m_context->getDevice(), &createInfo, nullptr,
                           &shaderModule) != VK_SUCCESS) {
    std::cerr << "avk: Failed to generate Shader Module for " << fileName
              << std::endl;
  }
  return shaderModule;
}

} // namespace avk
