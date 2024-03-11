#include <descriptors.h>

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType vulkanDescriptorType) {
	VkDescriptorSetLayoutBinding newVulkanLayoutBinding{};
	newVulkanLayoutBinding.binding = binding;
	newVulkanLayoutBinding.descriptorCount = 1;
	newVulkanLayoutBinding.descriptorType = vulkanDescriptorType;
	vulkanDescrSetLayoutBindings.push_back(newVulkanLayoutBinding);
}

void DescriptorLayoutBuilder::clear() {
	vulkanDescrSetLayoutBindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice vulkanDevice, VkShaderStageFlags vulkanShaderStageFlags) {
	for (VkDescriptorSetLayoutBinding& b : vulkanDescrSetLayoutBindings) {
		b.stageFlags |= vulkanShaderStageFlags;
	}
	VkDescriptorSetLayoutCreateInfo vulkanDescriptorLayoutCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	vulkanDescriptorLayoutCreateInfo.pNext = nullptr;
	vulkanDescriptorLayoutCreateInfo.pBindings = vulkanDescrSetLayoutBindings.data();
	vulkanDescriptorLayoutCreateInfo.bindingCount = (uint32_t)vulkanDescrSetLayoutBindings.size();
	vulkanDescriptorLayoutCreateInfo.flags = 0;
	VkDescriptorSetLayout vulkanDescriptorSetLayout;
	VK_CHECK(vkCreateDescriptorSetLayout(vulkanDevice, &vulkanDescriptorLayoutCreateInfo, nullptr, &vulkanDescriptorSetLayout));
	return vulkanDescriptorSetLayout;
}

void DescriptorAllocator::initialize_pool(VkDevice vulkanDevice, uint32_t maxSets, std::span<PoolSizeRatio> poolSizeRatios) {
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
	for (PoolSizeRatio ratio : poolSizeRatios) {
		descriptorPoolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.vulkanDescriptorType,
			.descriptorCount = uint32_t(ratio.ratio * maxSets)
			});
	}
	VkDescriptorPoolCreateInfo vulkanDescriptorPoolCreateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	vulkanDescriptorPoolCreateInfo.flags = 0;
	vulkanDescriptorPoolCreateInfo.maxSets = maxSets;
	vulkanDescriptorPoolCreateInfo.poolSizeCount = (uint32_t)descriptorPoolSizes.size();
	vulkanDescriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	vkCreateDescriptorPool(vulkanDevice, &vulkanDescriptorPoolCreateInfo, nullptr, &vulkanDescriptorPool);
}
void DescriptorAllocator :: clear_descriptors(VkDevice vulkanDevice) {
	vkResetDescriptorPool(vulkanDevice, vulkanDescriptorPool, 0);
}
void DescriptorAllocator :: destroy_pool(VkDevice vulkanDevice) {
	vkDestroyDescriptorPool(vulkanDevice, vulkanDescriptorPool, nullptr);
}
VkDescriptorSet DescriptorAllocator::allocate(VkDevice vulkanDevice, VkDescriptorSetLayout vulkanDescriptorSetLayout) {
	VkDescriptorSetAllocateInfo vulkanDescriptorSetAllocateInfo = { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	vulkanDescriptorSetAllocateInfo.pNext = nullptr;
	vulkanDescriptorSetAllocateInfo.descriptorPool = vulkanDescriptorPool;
	vulkanDescriptorSetAllocateInfo.descriptorSetCount = 1;
	vulkanDescriptorSetAllocateInfo.pSetLayouts = &vulkanDescriptorSetLayout;
	VkDescriptorSet vulkanDescriptorSet;
	VK_CHECK(vkAllocateDescriptorSets(vulkanDevice, &vulkanDescriptorSetAllocateInfo, &vulkanDescriptorSet));
	return vulkanDescriptorSet;
}