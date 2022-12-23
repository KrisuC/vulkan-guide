#include <vk_textures.h>
#include <iostream>

#include <vk_initializers.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace VkUtils
{

bool LoadImageFromFile(FVulkanEngine& Engine, const char* File, FAllocatedImage& OutImage)
{
	int32_t TexWidth, TexHeight, TexChannels;

	// Hard-coded rgba format
	stbi_uc* Pixels = stbi_load(File, &TexWidth, &TexHeight, &TexChannels, STBI_rgb_alpha);

	if (!Pixels)
	{
		std::cerr << "Failed to load texture file" << File << std::endl;
		return false;
	}

	VkDeviceSize ImageSize = TexWidth * TexHeight * 4; // 4 bytes for RGBA
	VkFormat ImageFormat = VK_FORMAT_R8G8B8A8_SRGB;

	// Copying data to staging buffer
	FAllocatedBuffer StagingBuffer = Engine.CreateBuffer(ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* Data;
	vmaMapMemory(Engine._Allocator, StagingBuffer._Allocation, &Data);
	memcpy(Data, Pixels, ImageSize);
	vmaUnmapMemory(Engine._Allocator, StagingBuffer._Allocation);

	stbi_image_free(Pixels);

	// Creating Image
	VkExtent3D ImageExtent;
	ImageExtent.width = TexWidth;
	ImageExtent.height = TexHeight;
	ImageExtent.depth = 1;

	VkImageCreateInfo ImageInfo = VkInit::ImageCreateInfo(ImageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, ImageExtent);

	FAllocatedImage NewImage;

	VmaAllocationCreateInfo ImageAllocInfo{};
	ImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	vmaCreateImage(Engine._Allocator, &ImageInfo, &ImageAllocInfo, &NewImage._Image, &NewImage._Allocation, nullptr);

	Engine.ImmediateSubmit([&](VkCommandBuffer Cmd)
	{
		VkImageSubresourceRange Range;
		Range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // color, depth or stencil...
		Range.baseMipLevel = 0;
		Range.levelCount = 1;
		Range.baseArrayLayer = 0; // texture array
		Range.layerCount = 1;

		VkImageMemoryBarrier ImageBarrierToTransfer{};
		ImageBarrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		ImageBarrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ImageBarrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		ImageBarrierToTransfer.image = NewImage._Image;
		ImageBarrierToTransfer.subresourceRange = Range;
		ImageBarrierToTransfer.srcAccessMask = 0;
		ImageBarrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		// Perform layout transition from UNDEFINED to TRANSFER_DST_OPTIMAL
		vkCmdPipelineBarrier(
			Cmd, 
			/* Block from top of pipe to transfer finished, TOP_OF_PIPE means commands can just start without waiting? */
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, /* SrcStageMask */
			VK_PIPELINE_STAGE_TRANSFER_BIT,    /* DstStageMask */
			0,								   /* DependencyFlags */
			0, nullptr, 
			0, nullptr, 
			1, &ImageBarrierToTransfer);	   /* ImageMemoryBarrier */

		VkBufferImageCopy CopyRegion{};
		CopyRegion.bufferOffset = 0;
		CopyRegion.bufferRowLength = 0;
		CopyRegion.bufferImageHeight = 0;

		CopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		CopyRegion.imageSubresource.mipLevel = 0;
		CopyRegion.imageSubresource.baseArrayLayer = 0;
		CopyRegion.imageSubresource.layerCount = 1;
		CopyRegion.imageExtent = ImageExtent;

		vkCmdCopyBufferToImage(
			Cmd, StagingBuffer._Buffer, NewImage._Image, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /* DstImageLayout */
			1, &CopyRegion);

		VkImageMemoryBarrier ImageBarrierToReadable = ImageBarrierToTransfer;
		ImageBarrierToReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		ImageBarrierToReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ImageBarrierToReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		ImageBarrierToReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			Cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, /* Change this if it's gonna be sampled in VS */
			0,
			0, nullptr,
			0, nullptr,
			1, &ImageBarrierToReadable);
	});

	// Instantly delete because we force sync
	vmaDestroyBuffer(Engine._Allocator, StagingBuffer._Buffer, StagingBuffer._Allocation);
	Engine._MainDeletionQueue.PushFunction([=]()
	{
		vmaDestroyImage(Engine._Allocator, NewImage._Image, NewImage._Allocation);
	});

	std::cout << "Texture loaded successfully " << File << std::endl;

	OutImage = NewImage;

	return true;
}

}