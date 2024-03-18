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

VkDescriptorPool ScalableDescriptorAllocator::create_pool(VkDevice vulkanDevice, uint32_t setCount, std::span<PoolSizeRatio2> poolSizeRatios)
{
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
	for (PoolSizeRatio2 ratio : poolSizeRatios)
		descriptorPoolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * setCount)
			});
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = setCount;
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)descriptorPoolSizes.size();
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	VkDescriptorPool newDescriptorPool;
	vkCreateDescriptorPool(vulkanDevice, &descriptorPoolCreateInfo, nullptr, &newDescriptorPool);
	return newDescriptorPool;
}
VkDescriptorPool ScalableDescriptorAllocator::get_pool(VkDevice vulkanDevice) {
	VkDescriptorPool newDescriptorPool;
	if (_readyPools.size() != 0) {
		newDescriptorPool = _readyPools.back();
		_readyPools.pop_back();
	}
	else {
		newDescriptorPool = create_pool(vulkanDevice, _setsPerPool, _poolSizeRatios);
		_setsPerPool = 1.5 * _setsPerPool;
		// MAX_NUMBER_OF_SETS = 4092
		if (_setsPerPool > 4092) {
			_setsPerPool = 4092;
		}
	}
	return newDescriptorPool;
}
void ScalableDescriptorAllocator::initialize_pools(VkDevice vulkanDevice, uint32_t initialSets, std::span<PoolSizeRatio2> poolSizeRatios) {
	_poolSizeRatios.clear();
	for (auto poolSizeRatio : poolSizeRatios)
		_poolSizeRatios.push_back(poolSizeRatio);
	VkDescriptorPool newDescriptorPool = create_pool(vulkanDevice, initialSets, poolSizeRatios);
	_setsPerPool = initialSets * 1.5;
	_readyPools.push_back(newDescriptorPool);
}
void ScalableDescriptorAllocator::clear_pools(VkDevice vulkanDevice) {
	for (auto pool : _readyPools) 
		vkResetDescriptorPool(vulkanDevice, pool, 0);	
	for (auto pool : _fullPools) {
		vkResetDescriptorPool(vulkanDevice, pool, 0);
		_readyPools.push_back(pool);
	}
	_fullPools.clear();
}
void ScalableDescriptorAllocator::destroy_pools(VkDevice vulkanDevice) {
	for (auto pool : _readyPools)
		vkDestroyDescriptorPool(vulkanDevice, pool, nullptr);
	for (auto pool : _fullPools)
		vkDestroyDescriptorPool(vulkanDevice, pool, nullptr);
	_fullPools.clear();
}
VkDescriptorSet ScalableDescriptorAllocator::allocate(VkDevice vulkanDevice, VkDescriptorSetLayout vulkanDescriptorSetLayout) {
	VkDescriptorPool someDescriptorPool = get_pool(vulkanDevice);
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = someDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &vulkanDescriptorSetLayout;

	VkDescriptorSet descriptorSet;
	VkResult vulkanResult = vkAllocateDescriptorSets(vulkanDevice, &descriptorSetAllocateInfo, &descriptorSet);

	if (vulkanResult == VK_ERROR_OUT_OF_POOL_MEMORY || vulkanResult == VK_ERROR_FRAGMENTED_POOL) {
		_fullPools.push_back(someDescriptorPool);
		someDescriptorPool = get_pool(vulkanDevice);
		descriptorSetAllocateInfo.descriptorPool = someDescriptorPool;
		VK_CHECK(vkAllocateDescriptorSets(vulkanDevice, &descriptorSetAllocateInfo, &descriptorSet));
	}
	_readyPools.push_back(someDescriptorPool);
	return descriptorSet;
}


void DescriptorWriter::write_image(int binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout, VkDescriptorType descriptorType) {
	VkDescriptorImageInfo& descriptorImageInfo = descriptorImageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = imageView,
		.imageLayout = imageLayout
		});
	VkWriteDescriptorSet descriptorWriteSet = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	descriptorWriteSet.dstBinding = binding;
	descriptorWriteSet.dstSet = VK_NULL_HANDLE;
	descriptorWriteSet.descriptorCount = 1;
	descriptorWriteSet.descriptorType = descriptorType;
	descriptorWriteSet.pImageInfo = &descriptorImageInfo;
	descriptorWriteSets.push_back(descriptorWriteSet);
}
void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType descriptorType) {
	VkDescriptorBufferInfo& descriptorBufferInfo = descriptorBufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
		});
	VkWriteDescriptorSet writeDescriptorSet = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.dstSet = VK_NULL_HANDLE;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = descriptorType;
	writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
	descriptorWriteSets.push_back(writeDescriptorSet);
}
void DescriptorWriter::clear() {
	descriptorImageInfos.clear();
	descriptorWriteSets.clear();
	descriptorBufferInfos.clear();
}
void DescriptorWriter::update_set(VkDevice vulkanDevice, VkDescriptorSet descriptorSet) {
	for (VkWriteDescriptorSet& writeDescriptorSet : descriptorWriteSets)
		writeDescriptorSet.dstSet = descriptorSet;
	vkUpdateDescriptorSets(vulkanDevice, (uint32_t)descriptorWriteSets.size(), descriptorWriteSets.data(), 0, nullptr);
}