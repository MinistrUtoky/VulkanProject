#include <pipelines.h>

bool vkutil::load_shader_module(const char* filePath, VkDevice vulkanDevice, VkShaderModule* outVulkanShaderModule) {
	// creating shader module out of shader (.comp.spv) file
	std::ifstream file;	
	file.open(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open()) return false;
	size_t fileSize = (size_t)file.tellg();
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);
	file.close();
	VkShaderModuleCreateInfo vulkanShaderModuleCreateInfo = {};
	vulkanShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vulkanShaderModuleCreateInfo.pNext = nullptr;
	vulkanShaderModuleCreateInfo.codeSize = buffer.size() * sizeof(uint32_t);
	vulkanShaderModuleCreateInfo.pCode = buffer.data();
	VkShaderModule vulkanShaderModule;
	if (vkCreateShaderModule(vulkanDevice, &vulkanShaderModuleCreateInfo, nullptr, &vulkanShaderModule) != VK_SUCCESS)
		return false;
	
	*outVulkanShaderModule = vulkanShaderModule;
	return true;
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice vulkanDevice) {
	VkPipelineViewportStateCreateInfo vulkanPipelineViewportStateCreateInfo = {};
	vulkanPipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vulkanPipelineViewportStateCreateInfo.pNext = nullptr;
	vulkanPipelineViewportStateCreateInfo.viewportCount = 1;
	vulkanPipelineViewportStateCreateInfo.scissorCount = 1;

	VkPipelineColorBlendStateCreateInfo vulkanPipelineColorBlendingCreateInfo;
	vulkanPipelineColorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	vulkanPipelineColorBlendingCreateInfo.pNext = nullptr;
	vulkanPipelineColorBlendingCreateInfo.logicOpEnable = VK_FALSE;
	vulkanPipelineColorBlendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	vulkanPipelineColorBlendingCreateInfo.attachmentCount = 1;
	vulkanPipelineColorBlendingCreateInfo.pAttachments = &_vulkanPipelineColorBlendAttachment;

	VkPipelineVertexInputStateCreateInfo _vulkanPipelineVertexInputCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkGraphicsPipelineCreateInfo vulkanGraphicsPipelineCreateInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	vulkanGraphicsPipelineCreateInfo.pNext = &_vulkanPipelineRenderingInfo;
	vulkanGraphicsPipelineCreateInfo.stageCount = (uint32_t)_vulkanPipelineShaderStages.size(); 
	vulkanGraphicsPipelineCreateInfo.pStages = _vulkanPipelineShaderStages.data();
	vulkanGraphicsPipelineCreateInfo.pVertexInputState = &_vulkanPipelineVertexInputCreateInfo;
	vulkanGraphicsPipelineCreateInfo.pInputAssemblyState = &_vulkanPipelineInputAssembler;
	vulkanGraphicsPipelineCreateInfo.pViewportState = &vulkanPipelineViewportStateCreateInfo;
	vulkanGraphicsPipelineCreateInfo.pRasterizationState = &_vulkanPipelineRasterizator;
	vulkanGraphicsPipelineCreateInfo.pMultisampleState = &_vulkanPipelineMultisampling;
	vulkanGraphicsPipelineCreateInfo.pColorBlendState = &vulkanPipelineColorBlendingCreateInfo;
	vulkanGraphicsPipelineCreateInfo.pDepthStencilState = &_vulkanPipelineDepthStencil;
	vulkanGraphicsPipelineCreateInfo.layout = _vulkanPipelineLayout;

	VkDynamicState vulkanDynamicState[] = { VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo vulkanPipelineDynamicCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	vulkanPipelineDynamicCreateInfo.pDynamicStates = &vulkanDynamicState[0];
	vulkanPipelineDynamicCreateInfo.dynamicStateCount = 2;
	vulkanGraphicsPipelineCreateInfo.pDynamicState = &vulkanPipelineDynamicCreateInfo;

	VkPipeline buildedPipeline;
	if (vkCreateGraphicsPipelines(vulkanDevice, VK_NULL_HANDLE, 1, &vulkanGraphicsPipelineCreateInfo, nullptr, &buildedPipeline) != VK_SUCCESS) {
		fmt::println("Pipeline creation failed!");
		return VK_NULL_HANDLE;
	}
	else
		return buildedPipeline;
}

void PipelineBuilder::set_shaders(VkShaderModule vulkanVertexShaderModule, VkShaderModule vulkanFragmentShaderModule) {
	_vulkanPipelineShaderStages.clear();
	_vulkanPipelineShaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vulkanVertexShaderModule));
	_vulkanPipelineShaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, vulkanFragmentShaderModule));
}

void PipelineBuilder::set_input_topology(VkPrimitiveTopology vulkanPrimitiveTopology) {
	_vulkanPipelineInputAssembler.topology = vulkanPrimitiveTopology;
	_vulkanPipelineInputAssembler.primitiveRestartEnable = VK_FALSE;
}
void PipelineBuilder::set_polygon_mode(VkPolygonMode vulkanPolygonMode) {
	_vulkanPipelineRasterizator.polygonMode = vulkanPolygonMode;
	_vulkanPipelineRasterizator.lineWidth = 1.f;
}
void PipelineBuilder::set_cull_mode(VkCullModeFlags vulkanCullModeFlags, VkFrontFace vulkanFrontFace) {
	_vulkanPipelineRasterizator.cullMode = vulkanCullModeFlags;
	_vulkanPipelineRasterizator.frontFace = vulkanFrontFace;
}
void PipelineBuilder::set_multisampling_none() {
	_vulkanPipelineMultisampling.sampleShadingEnable = VK_FALSE;
	_vulkanPipelineMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	_vulkanPipelineMultisampling.minSampleShading = 1.0f;
	_vulkanPipelineMultisampling.pSampleMask = nullptr;
	_vulkanPipelineMultisampling.alphaToOneEnable = VK_FALSE;
	_vulkanPipelineMultisampling.alphaToOneEnable = VK_FALSE;
}
void PipelineBuilder::disable_blending() {
	_vulkanPipelineColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
	_vulkanPipelineColorBlendAttachment.blendEnable = VK_FALSE;
}
void PipelineBuilder::set_color_attachment_format(VkFormat vulkanFormat) {
	_vulkanColorAttachmentFormat = vulkanFormat;
	_vulkanPipelineRenderingInfo.colorAttachmentCount = 1;
	_vulkanPipelineRenderingInfo.pColorAttachmentFormats = &_vulkanColorAttachmentFormat;
}
void PipelineBuilder::set_depth_format(VkFormat vulkanFormat)
{
	_vulkanPipelineRenderingInfo.depthAttachmentFormat = vulkanFormat;
}
void PipelineBuilder::enable_depth_test(bool depthWriteEnable, VkCompareOp compareOp) {
	_vulkanPipelineDepthStencil.depthTestEnable = VK_TRUE;
	_vulkanPipelineDepthStencil.depthWriteEnable = depthWriteEnable;
	_vulkanPipelineDepthStencil.depthCompareOp = compareOp;
	_vulkanPipelineDepthStencil.depthBoundsTestEnable = VK_FALSE;
	_vulkanPipelineDepthStencil.stencilTestEnable = VK_FALSE;
	_vulkanPipelineDepthStencil.front = {};
	_vulkanPipelineDepthStencil.back = {};
	_vulkanPipelineDepthStencil.minDepthBounds = 0.f;
	_vulkanPipelineDepthStencil.maxDepthBounds = 1.f;
}
void PipelineBuilder::disable_depth_test() {
	_vulkanPipelineDepthStencil.depthTestEnable = VK_FALSE;
	_vulkanPipelineDepthStencil.depthWriteEnable = VK_FALSE;
	_vulkanPipelineDepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
	_vulkanPipelineDepthStencil.depthBoundsTestEnable = VK_FALSE;
	_vulkanPipelineDepthStencil.stencilTestEnable = VK_FALSE;
	_vulkanPipelineDepthStencil.front = {};
	_vulkanPipelineDepthStencil.back = {};
	_vulkanPipelineDepthStencil.minDepthBounds = 0.f;
	_vulkanPipelineDepthStencil.maxDepthBounds = 1.f;
}

// different blending modes for object transparency and stuff
void PipelineBuilder::enable_blending_additive() {
	_vulkanPipelineColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_vulkanPipelineColorBlendAttachment.colorWriteMask = VK_TRUE;
	_vulkanPipelineColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_vulkanPipelineColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
	_vulkanPipelineColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_vulkanPipelineColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_vulkanPipelineColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_vulkanPipelineColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

}
void PipelineBuilder::enable_blending_alphablend() {
	_vulkanPipelineColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_vulkanPipelineColorBlendAttachment.colorWriteMask = VK_TRUE;
	_vulkanPipelineColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	_vulkanPipelineColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
	_vulkanPipelineColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_vulkanPipelineColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_vulkanPipelineColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_vulkanPipelineColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::clear() {
	_vulkanPipelineInputAssembler = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	_vulkanPipelineRasterizator = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	_vulkanPipelineColorBlendAttachment = {};
	_vulkanPipelineMultisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	_vulkanPipelineLayout = {};
	_vulkanPipelineDepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	_vulkanPipelineRenderingInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	_vulkanPipelineShaderStages.clear();
}

