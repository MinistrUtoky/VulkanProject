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
struct ScalableDescriptorAllocator {
public:
	struct PoolSizeRatio2 {
		VkDescriptorType type;
		float ratio;
	};
	void initialize_pools(VkDevice vulkanDevice, uint32_t initialSets, std::span<PoolSizeRatio2> poolSizeRatios);
	void clear_pools(VkDevice vulkanDevice);
	void destroy_pools(VkDevice vulkanDevice);
	VkDescriptorSet allocate(VkDevice vulkanDevice, VkDescriptorSetLayout vulkanDescriptorSetLayout);

private:
	VkDescriptorPool get_pool(VkDevice vulkanDevice);
	VkDescriptorPool create_pool(VkDevice vulkanDevice, uint32_t setCount, std::span<PoolSizeRatio2> poolSizeRatios);
	std::vector<PoolSizeRatio2> _poolSizeRatios;
	std::vector<VkDescriptorPool> _fullPools;
	std::vector<VkDescriptorPool> _readyPools;
	uint32_t _setsPerPool;
};
struct DescriptorWriter {
	std::deque<VkDescriptorImageInfo> descriptorImageInfos;
	std::deque<VkDescriptorBufferInfo> descriptorBufferInfos;
	std::vector<VkWriteDescriptorSet> descriptorWriteSets;
	void write_image(int binding, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout, VkDescriptorType descriptorType);
	void write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType descriptorType);
	//void write_sampler();
	void clear();
	void update_set(VkDevice vulkanDevice, VkDescriptorSet descriptorSet);
};