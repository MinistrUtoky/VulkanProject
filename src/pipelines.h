#pragma once 
#include <types.h>
#include "pipelines.h"
#include <fstream>
#include "initializers.h"

namespace vkutil {
	bool load_shader_module(const char* filePath, VkDevice vulkanDevice, VkShaderModule* outVulkanShaderModule);
};