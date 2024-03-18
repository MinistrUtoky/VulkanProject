#pragma once 
#include <types.h>
#include "pipelines.h"
#include <fstream>
#include "initializers.h"

namespace vkutil {
	bool load_shader_module(const char* filePath, VkDevice vulkanDevice, VkShaderModule* outVulkanShaderModule);
};


class PipelineBuilder {
public:
	std::vector<VkPipelineShaderStageCreateInfo> _vulkanPipelineShaderStages;
	VkPipelineInputAssemblyStateCreateInfo _vulkanPipelineInputAssembler;
	VkPipelineRasterizationStateCreateInfo _vulkanPipelineRasterizator;
	VkPipelineColorBlendAttachmentState _vulkanPipelineColorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _vulkanPipelineMultisampling;
	VkPipelineLayout _vulkanPipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _vulkanPipelineDepthStencil;
	VkPipelineRenderingCreateInfo _vulkanPipelineRenderingInfo;
	VkFormat _vulkanColorAttachmentFormat;
	
	PipelineBuilder() { 
		clear(); 
	};
	void clear();
	VkPipeline build_pipeline(VkDevice vulkanDevice);
	void set_shaders(VkShaderModule vulkanVertexShaderModule, VkShaderModule vulkanFragmentShaderModule);
	void set_input_topology(VkPrimitiveTopology vulkanPrimitiveTopology);
	void set_polygon_mode(VkPolygonMode vulkanPolygonMode);
	void set_cull_mode(VkCullModeFlags vulkanCullModeFlags, VkFrontFace vulkanFrontFace);
	void set_multisampling_none();
	void disable_blending();
	void set_color_attachment_format(VkFormat vulkanFormat);
	void set_depth_format(VkFormat format);
	void enable_depth_test(bool depthWriteEnable, VkCompareOp vulkanCompareOp);
	void disable_depth_test();
	void enable_blending_additive();
	void enable_blending_alphablend();
};