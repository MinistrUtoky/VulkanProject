#pragma once

#include <types.h>

struct DescriptorLayoutBuilder {
	std::vector<VkDescriptorSetLayoutBinding> vulkanDescrSetLayoutBindings;

	void add_binding(uint32_t binding, VkDescriptorType vulkanDescriptorType);
	void clear();
	VkDescriptorSetLayout build(VkDevice vulkanDevice, VkShaderStageFlags vulkanShaderStageFlags);
};

struct DescriptorAllocator {
	struct PoolSizeRatio {
		VkDescriptorType vulkanDescriptorType;
		float ratio;
	};
	VkDescriptorPool vulkanDescriptorPool;
	void initialize_pool(VkDevice vulkanDevice, uint32_t maxSets, std::span<PoolSizeRatio> poolSizeRatios);
	void clear_descriptors(VkDevice vulkanDevice);
	void destroy_pool(VkDevice vulkanDevice);
	VkDescriptorSet allocate(VkDevice vulkanDevice, VkDescriptorSetLayout vulkanDescriptorSetLayout);
};