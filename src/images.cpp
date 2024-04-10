#include <images.h>
#include <initializers.h>

void vkutil::image_transition(VkCommandBuffer vulkanCommandBuffer, VkImage image, VkImageLayout currentImageLayout, VkImageLayout newImageLayout) {
	VkImageMemoryBarrier2 imageMemoryBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; // ALL_COMMANDS is bad when there are many transitions per frame but for now it's okay
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; // here too
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	imageMemoryBarrier.oldLayout = currentImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	VkImageAspectFlags imageAspectFlags = (newImageLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT);
	imageMemoryBarrier.subresourceRange = vkinit::image_subresource_range(imageAspectFlags);
	imageMemoryBarrier.image = image;
	VkDependencyInfo vulkanDependencyInfo{};
	vulkanDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	vulkanDependencyInfo.pNext = nullptr;
	vulkanDependencyInfo.imageMemoryBarrierCount = 1;
	vulkanDependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;
	vkCmdPipelineBarrier2(vulkanCommandBuffer, &vulkanDependencyInfo);
}

void vkutil::copy_image_to_image(VkCommandBuffer vulkanCommandBuffer, VkImage sourceVulkanImage, VkImage destinatedVulkanImage, VkExtent2D sourceImageExtent, VkExtent2D destinatedImageExtent) {
	VkImageBlit2 vulkanImageBlit{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };
	vulkanImageBlit.srcOffsets[1].x = sourceImageExtent.width;
	vulkanImageBlit.srcOffsets[1].y = sourceImageExtent.height;
	vulkanImageBlit.srcOffsets[1].z = 1;

	vulkanImageBlit.dstOffsets[1].x = destinatedImageExtent.width;
	vulkanImageBlit.dstOffsets[1].y = destinatedImageExtent.height;
	vulkanImageBlit.dstOffsets[1].z = 1;
	
	vulkanImageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vulkanImageBlit.srcSubresource.baseArrayLayer = 0;
	vulkanImageBlit.srcSubresource.layerCount = 1;
	vulkanImageBlit.srcSubresource.mipLevel = 0;

	vulkanImageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vulkanImageBlit.dstSubresource.baseArrayLayer = 0;
	vulkanImageBlit.dstSubresource.layerCount = 1;
	vulkanImageBlit.dstSubresource.mipLevel = 0;

	VkBlitImageInfo2 vulkanBlitImageInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
	vulkanBlitImageInfo.dstImage = destinatedVulkanImage;
	vulkanBlitImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vulkanBlitImageInfo.srcImage = sourceVulkanImage;
	vulkanBlitImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vulkanBlitImageInfo.filter = VK_FILTER_LINEAR;
	vulkanBlitImageInfo.regionCount = 1;
	vulkanBlitImageInfo.pRegions = &vulkanImageBlit;

	vkCmdBlitImage2(vulkanCommandBuffer, &vulkanBlitImageInfo);
}

void vkutil::mipmaps_generation(VkCommandBuffer vulkanCommandBuffer, VkImage vulkanImage, VkExtent2D imageExtent) {

	int mipLevels = int(std::floor(std::log2(std::max(imageExtent.width, imageExtent.height)))+1);
	for (int mipLevel = 0; mipLevel < mipLevels; mipLevel++) {
		VkExtent2D imageExtentHalved = imageExtent;
		imageExtent.width *= 0.5f;
		imageExtent.height *= 0.5f;
		VkImageMemoryBarrier2 imageMemoryBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,.pNext = nullptr };
		imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		VkImageAspectFlags imageAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		imageMemoryBarrier.subresourceRange = vkinit::image_subresource_range(imageAspectFlags);
		imageMemoryBarrier.subresourceRange.levelCount = 1;
		imageMemoryBarrier.subresourceRange.baseMipLevel = mipLevel;
		imageMemoryBarrier.image = vulkanImage;
		VkDependencyInfo dependencyInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,.pNext = nullptr };
		dependencyInfo.imageMemoryBarrierCount = 1;
		dependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;
		vkCmdPipelineBarrier2(vulkanCommandBuffer, &dependencyInfo);
		if (mipLevel < mipLevels - 1) {
			VkImageBlit2 imageBlit{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,.pNext = nullptr };
			imageBlit.srcOffsets[1].x = imageExtent.width;
			imageBlit.srcOffsets[1].y = imageExtent.height;
			imageBlit.srcOffsets[1].z = 1;
			imageBlit.dstOffsets[1].x = imageExtentHalved.width;
			imageBlit.dstOffsets[1].y = imageExtentHalved.height;
			imageBlit.dstOffsets[1].z = 1;

			imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.srcSubresource.baseArrayLayer = 0;
			imageBlit.srcSubresource.layerCount = 1;
			imageBlit.srcSubresource.mipLevel = mipLevel;
			imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBlit.dstSubresource.baseArrayLayer = 0;
			imageBlit.dstSubresource.layerCount = 1;
			imageBlit.dstSubresource.mipLevel = mipLevel+1;
			VkBlitImageInfo2 blitImageInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
			blitImageInfo.dstImage = vulkanImage;
			blitImageInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			blitImageInfo.srcImage = vulkanImage;
			blitImageInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			blitImageInfo.filter = VK_FILTER_LINEAR;
			blitImageInfo.regionCount = 1;
			blitImageInfo.pRegions = &imageBlit;
			imageExtent = imageExtentHalved;
		}
	}
	image_transition(vulkanCommandBuffer, vulkanImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
