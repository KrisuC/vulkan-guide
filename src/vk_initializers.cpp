#include <vk_initializers.h>

namespace VkInit
{

VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags Flags)
{
	VkCommandPoolCreateInfo Info{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	Info.queueFamilyIndex = QueueFamilyIndex;
	Info.flags = Flags;
	return Info;
}

VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool Pool, uint32_t Count, VkCommandBufferLevel Level)
{
	VkCommandBufferAllocateInfo Info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	Info.commandPool = Pool;
	Info.commandBufferCount = Count;
	Info.level = Level;
	return Info;
}

VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits Stage, VkShaderModule ShaderModule)
{
	VkPipelineShaderStageCreateInfo Info{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	Info.stage = Stage;
	Info.module = ShaderModule;
	Info.pName = "main";
	return Info;
}

VkPipelineVertexInputStateCreateInfo VertexInputStateCreateInfo()
{
	VkPipelineVertexInputStateCreateInfo Info{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	Info.vertexBindingDescriptionCount = 0;
	Info.vertexAttributeDescriptionCount = 0;
	return Info;
}

VkPipelineInputAssemblyStateCreateInfo InputAssemblyCreateInfo(VkPrimitiveTopology Topology)
{
	VkPipelineInputAssemblyStateCreateInfo Info{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	Info.topology = Topology;
	Info.primitiveRestartEnable = false;
	return Info;
}

VkPipelineRasterizationStateCreateInfo RasterizationStateCreateInfo(VkPolygonMode PolygonMode)
{
	VkPipelineRasterizationStateCreateInfo Info{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	Info.depthClampEnable = VK_FALSE;
	Info.rasterizerDiscardEnable = VK_FALSE;

	Info.polygonMode = PolygonMode;
	Info.lineWidth = 1.f;
	Info.cullMode = VK_CULL_MODE_NONE; // UE4 is back face culling default
	Info.frontFace = VK_FRONT_FACE_CLOCKWISE; // UE4 is counter clockwise
	Info.depthBiasEnable = VK_FALSE;
	
	return Info;
}

VkPipelineMultisampleStateCreateInfo MultisampleStateCreateInfo()
{
	VkPipelineMultisampleStateCreateInfo Info{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	Info.sampleShadingEnable = VK_FALSE;
	Info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	Info.minSampleShading = 1.f;
	Info.pSampleMask = nullptr;
	Info.alphaToCoverageEnable = VK_FALSE;
	Info.alphaToOneEnable = VK_FALSE;
	return Info;
	// i love xiaogou >3<
}

VkPipelineColorBlendAttachmentState ColorBlendAttachmentState()
{
	VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
	ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	ColorBlendAttachment.blendEnable = VK_FALSE;
	return ColorBlendAttachment;
}

VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo()
{
	VkPipelineLayoutCreateInfo Info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	Info.flags = 0;
	Info.setLayoutCount = 0;
	Info.pSetLayouts = nullptr;
	Info.pushConstantRangeCount = 0;
	Info.pPushConstantRanges = nullptr;
	return Info;
}

VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags Flags)
{
	VkFenceCreateInfo FenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	FenceCreateInfo.flags = Flags;
	return FenceCreateInfo;
}

VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags Flags)
{
	VkSemaphoreCreateInfo SemaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	SemaphoreCreateInfo.flags = Flags;
	return SemaphoreCreateInfo;
}

}
