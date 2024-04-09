#pragma once 
#include <vulkan/vulkan.h>

namespace vkutil {
	void image_transition(VkCommandBuffer vulkanCommandBuffer, VkImage image, VkImageLayout currentImageLayout, VkImageLayout newImageLayout);
	void copy_image_to_image(VkCommandBuffer vulkanCommandBuffer, VkImage sourceVulkanImage, VkImage destinatedVulkanImage, VkExtent2D sourceImageExtent, VkExtent2D destinatedImageExtent);
	void mipmaps_generation(VkCommandBuffer vulkanCommandBuffer, VkImage vulkanImage, VkExtent2D vulkanImageExtent2D);
};