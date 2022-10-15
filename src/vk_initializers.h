// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

namespace VkInit {

VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags Flags = 0);

VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool Pool, uint32_t Count = 1, VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits Stage, VkShaderModule ShaderModule);

VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo();

VkPipelineInputAssemblyStateCreateInfo InputAssemblyCreateInfo(VkPrimitiveTopology Topology);

VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo(VkPolygonMode PolygonMode);

VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo();

VkPipelineColorBlendAttachmentState ColorBlendAttachmentState();

VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo();

VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags Flags = 0);

VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags Flags = 0);

VkImageCreateInfo ImageCreateInfo(VkFormat Format, VkImageUsageFlags UsageFlags, VkExtent3D Extent);

VkImageViewCreateInfo ImageViewCreateInfo(VkFormat Format, VkImage Image, VkImageAspectFlags AspectFlags);

VkPipelineDepthStencilStateCreateInfo DepthStencilCreateInfo(bool bDepthTest, bool bDepthWrite, VkCompareOp CompareOp);

}

