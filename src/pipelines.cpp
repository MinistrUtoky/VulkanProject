#include <pipelines.h>

bool vkutil::load_shader_module(const char* filePath, VkDevice vulkanDevice, VkShaderModule* outVulkanShaderModule) {
	// creating shader module out of shader (.comp) file
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);	

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