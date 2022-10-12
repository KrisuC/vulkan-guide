#define VMA_IMPLEMENTATION

#include "vk_engine.h"

#include <iostream>
#include <fstream>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"

#define VK_CHECK(X)												\
	{															\
		VkResult Err = X;										\
		if (Err)												\
		{														\
			throw Err;											\
		}													    \
	}

#define CHECKF(X, Str)											\
	{															\
		if (!X)													\
		{														\
			throw Str;											\
		}													    \
	}

#define CHECK(X)												\
	{															\
		if (!X)													\
		{														\
			throw "";											\
		}													    \
	}

#define ONE_SECOND 1000000000

void FVulkanEngine::Init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
	
	_Window = SDL_CreateWindow(
		"Vulkan Engine",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		_WindowExtent.width,
		_WindowExtent.height,
		window_flags
	);

	InitVulkan();
	InitSwapChain();
	InitCommands();
	InitDefaultRenderPass();
	InitFramebuffers();
	InitSyncStructures();
	InitPipelines();

	LoadMeshes();
	
	//everything went fine
	_bIsInitialized = true;
}
void FVulkanEngine::Cleanup()
{	
	if (_bIsInitialized) 
	{	
		vkWaitForFences(_Device, 1, &_RenderFence, true, ONE_SECOND);
		_MainDeletionQueue.Flush();

		vkDestroySurfaceKHR(_Instance, _Surface, nullptr);
		vkDestroyDevice(_Device, nullptr);
		vkb::destroy_debug_utils_messenger(_Instance, _DebugMessenger);
		vkDestroyInstance(_Instance, nullptr);

		SDL_DestroyWindow(_Window);
	}
}

void FVulkanEngine::Run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			switch (e.type)
			{
			case SDL_QUIT: 
				bQuit = true; 
				break;
			case SDL_KEYDOWN:
				std::cout << "Key pressed.\n";
				switch (e.key.keysym.scancode)
				{
				case SDL_SCANCODE_ESCAPE:
					bQuit = true;
					break;
				case SDL_SCANCODE_SPACE:
					_SelectedShader = 1 - _SelectedShader;
					break;
				}
				break;
			case SDL_KEYUP:
				std::cout << "Key Released.\n";
				break;
			}
			if (e.type == SDL_QUIT)
			{
				bQuit = true;
			}
		}

		Draw();
	}
}

void FVulkanEngine::InitVulkan()
{
	vkb::InstanceBuilder Builder;

	vkb::Instance VkbInstance = Builder.set_app_name("Example Vulkan Application")
		.request_validation_layers(true) // TODO: using only on debug mode
		.require_api_version(1, 1, 0)
		.use_default_debug_messenger()
		.build()
		.value();

	_Instance = VkbInstance.instance;
	_DebugMessenger = VkbInstance.debug_messenger;

	SDL_Vulkan_CreateSurface(_Window, _Instance, &_Surface);

	vkb::PhysicalDeviceSelector Selector(VkbInstance);
	vkb::PhysicalDevice PhysicalDevice = Selector
		.set_minimum_version(1, 1)
		.set_surface(_Surface)
		.select()
		.value();

	vkb::DeviceBuilder Devicebuilder(PhysicalDevice);
	vkb::Device VkbDevice = Devicebuilder.build().value();

	_Device = VkbDevice.device;
	_ChosenGPU = PhysicalDevice.physical_device;

	_GraphicsQueue = VkbDevice.get_queue(vkb::QueueType::graphics).value();
	_GraphicsQueueFamily = VkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VmaAllocatorCreateInfo AllocatorInfo{};
	AllocatorInfo.physicalDevice = _ChosenGPU;
	AllocatorInfo.device = _Device;
	AllocatorInfo.instance = _Instance;
	vmaCreateAllocator(&AllocatorInfo, &_Allocator);
	_MainDeletionQueue.PushFunction([=]() {
		vmaDestroyAllocator(_Allocator);
	});
}

void FVulkanEngine::InitSwapChain()
{
	vkb::SwapchainBuilder SwapChainBuilder(_ChosenGPU, _Device, _Surface);

	vkb::Swapchain vkbSwapChain = SwapChainBuilder
		.use_default_format_selection()
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(_WindowExtent.width, _WindowExtent.height)
		.build()
		.value();

	_SwapChain = vkbSwapChain.swapchain;
	_SwapChainImages = vkbSwapChain.get_images().value();
	_SwapChainImageViews = vkbSwapChain.get_image_views().value();

	_SwapChainImageFormat = vkbSwapChain.image_format;

	_MainDeletionQueue.PushFunction([=]() {
		vkDestroySwapchainKHR(_Device, _SwapChain, nullptr);
	});
}

void FVulkanEngine::InitCommands()
{	
	VkCommandPoolCreateInfo CommandPoolInfo = VkInit::CommandPoolCreateInfo(_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(_Device, &CommandPoolInfo, nullptr, &_CommandPool));

	VkCommandBufferAllocateInfo CmdAllocInfo = VkInit::CommandBufferAllocateInfo(_CommandPool);
	VK_CHECK(vkAllocateCommandBuffers(_Device, &CmdAllocInfo, &_MainCommandBuffer));

	_MainDeletionQueue.PushFunction([=]() {
		vkDestroyCommandPool(_Device, _CommandPool, nullptr);
	});
}

void FVulkanEngine::InitDefaultRenderPass()
{
	// Declaring attachment
	VkAttachmentDescription ColorAttachment{};
	ColorAttachment.format = _SwapChainImageFormat;
	ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Creating subpass 0
	VkAttachmentReference ColorAttachmentRef{};
	ColorAttachmentRef.attachment = 0;
	ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDescription Subpass{};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachmentRef;

	VkRenderPassCreateInfo RenderPassInfo{};
	RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	RenderPassInfo.pNext = nullptr;
	RenderPassInfo.attachmentCount = 1;
	RenderPassInfo.pAttachments = &ColorAttachment;
	RenderPassInfo.subpassCount = 1;
	RenderPassInfo.pSubpasses = &Subpass;

	VK_CHECK(vkCreateRenderPass(_Device, &RenderPassInfo, nullptr, &_RenderPass));

	_MainDeletionQueue.PushFunction(
		[=]() { vkDestroyRenderPass(_Device, _RenderPass, nullptr); }
	);
}

void FVulkanEngine::InitFramebuffers()
{
	VkFramebufferCreateInfo FramebufferInfo{};
	FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	FramebufferInfo.pNext = nullptr;
	FramebufferInfo.renderPass = _RenderPass;
	FramebufferInfo.attachmentCount = 1;
	FramebufferInfo.width = _WindowExtent.width;
	FramebufferInfo.height = _WindowExtent.height;
	FramebufferInfo.layers = 1;

	const uint32_t SwapChainImageCount = _SwapChainImages.size();
	_Framebuffers = std::vector<VkFramebuffer>(SwapChainImageCount);

	for (int i = 0; i < SwapChainImageCount; i++)
	{
		FramebufferInfo.pAttachments = &_SwapChainImageViews[i];
		VK_CHECK(vkCreateFramebuffer(_Device, &FramebufferInfo, nullptr, &_Framebuffers[i]));

		_MainDeletionQueue.PushFunction([=]() {
			vkDestroyFramebuffer(_Device, _Framebuffers[i], nullptr);
			vkDestroyImageView(_Device, _SwapChainImageViews[i], nullptr);
		});
	}
}

void FVulkanEngine::InitSyncStructures()
{
	VkFenceCreateInfo FenceCreateInfo = VkInit::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK(vkCreateFence(_Device, &FenceCreateInfo, nullptr, &_RenderFence));

	_MainDeletionQueue.PushFunction(
		[=]() { vkDestroyFence(_Device, _RenderFence, nullptr); }
	);

	VkSemaphoreCreateInfo SemaphoreCreateInfo = VkInit::SemaphoreCreateInfo();
	VK_CHECK(vkCreateSemaphore(_Device, &SemaphoreCreateInfo, nullptr, &_PresentSemaphore));
	VK_CHECK(vkCreateSemaphore(_Device, &SemaphoreCreateInfo, nullptr, &_RenderSemaphore));

	_MainDeletionQueue.PushFunction([=]() {
		vkDestroySemaphore(_Device, _PresentSemaphore, nullptr);
		vkDestroySemaphore(_Device, _RenderSemaphore, nullptr);
	});
}

void FVulkanEngine::Draw()
{
	VK_CHECK(vkWaitForFences(_Device, 1, &_RenderFence, true, ONE_SECOND));	// 1 sec
	VK_CHECK(vkResetFences(_Device, 1, &_RenderFence));

	uint32_t SwapChainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_Device, _SwapChain, ONE_SECOND, _PresentSemaphore, nullptr, &SwapChainImageIndex)); // signal _PresentSemaphore

	VK_CHECK(vkResetCommandBuffer(_MainCommandBuffer, 0));
	VkCommandBuffer Cmd = _MainCommandBuffer;

	VkCommandBufferBeginInfo CmdBeginInfo{};
	CmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	CmdBeginInfo.pNext = nullptr;
	CmdBeginInfo.pInheritanceInfo = nullptr;
	CmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(Cmd, &CmdBeginInfo));
	{
		VkClearValue ClearValue;
		float Flash = abs(sin(_FrameNumber / 120.f));
		ClearValue.color = { { 0.0f, 0.0f, Flash, 1.0f } };

		VkRenderPassBeginInfo RpInfo{};
		RpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		RpInfo.pNext = nullptr;
		RpInfo.renderPass = _RenderPass;
		RpInfo.renderArea.offset.x = 0;
		RpInfo.renderArea.offset.y = 0;
		RpInfo.renderArea.extent = _WindowExtent;
		RpInfo.framebuffer = _Framebuffers[SwapChainImageIndex];

		RpInfo.clearValueCount = 1;
		RpInfo.pClearValues = &ClearValue;

		vkCmdBeginRenderPass(Cmd, &RpInfo, VK_SUBPASS_CONTENTS_INLINE);

		// vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _SelectedShader ? _TrianglePipeline : _RedTrianglePipeline);
		vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _MeshPipeline);
		VkDeviceSize Offset = 0;
		vkCmdBindVertexBuffers(Cmd, 0, 1, &_TriangleMesh._VertexBuffer._Buffer, &Offset);
		vkCmdDraw(Cmd, 3, 1, 0, 0);

		vkCmdEndRenderPass(Cmd);
	}
	VK_CHECK(vkEndCommandBuffer(Cmd));

	VkSubmitInfo Submit{};
	Submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	Submit.pNext = nullptr;
	VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	Submit.pWaitDstStageMask = &WaitStage;
	
	// Wait for image is aquired, making sure image for rendering is ready
	Submit.waitSemaphoreCount = 1;
	Submit.pWaitSemaphores = &_PresentSemaphore;
	// Lock _RenderSemaphore, unlock when the GPU job is done
	Submit.signalSemaphoreCount = 1;
	Submit.pSignalSemaphores = &_RenderSemaphore; 

	Submit.commandBufferCount = 1;
	Submit.pCommandBuffers = &Cmd;

	VK_CHECK(vkQueueSubmit(_GraphicsQueue, 1, &Submit, _RenderFence));

	VkPresentInfoKHR PresentInfo{};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.pNext = nullptr;
	
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = &_SwapChain;
	
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = &_RenderSemaphore; // Wait for rendering is completed

	PresentInfo.pImageIndices = &SwapChainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_GraphicsQueue, &PresentInfo));

	_FrameNumber++;
}

VkShaderModule FVulkanEngine::LoadShaderModule(const char* FilePath)
{
	// ios::ate put stream cursor at end of file
	std::ifstream File(FilePath, std::ios::ate | std::ios::binary);

	CHECK(File.is_open());

	size_t FileSize = File.tellg();
	std::vector<uint32_t> Buffer(FileSize / sizeof(uint32_t));

	File.seekg(0);
	File.read((char*)Buffer.data(), FileSize);
	File.close();

	VkShaderModuleCreateInfo ShaderInfo{};
	ShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ShaderInfo.pNext = nullptr;
	ShaderInfo.codeSize = Buffer.size() * sizeof(uint32_t);
	ShaderInfo.pCode = Buffer.data();

	VkShaderModule ShaderModule;
	VK_CHECK(vkCreateShaderModule(_Device, &ShaderInfo, nullptr, &ShaderModule));
	return ShaderModule;
}

void FVulkanEngine::InitPipelines()
{
	// Shit code, should use shader permutations
	VkShaderModule TriangleVertShader = LoadShaderModule("../../shaders/triangle.vert.spv");
	VkShaderModule TriangleFragShader = LoadShaderModule("../../shaders/triangle.frag.spv");
	VkShaderModule RedTriangleVertShader = LoadShaderModule("../../shaders/red_triangle.vert.spv");
	VkShaderModule RedTriangleFragShader = LoadShaderModule("../../shaders/red_triangle.frag.spv");

	// Descriptor sets and push constants
	VkPipelineLayoutCreateInfo PipelineLayoutInfo = VkInit::PipelineLayoutCreateInfo();
	VK_CHECK(vkCreatePipelineLayout(_Device, &PipelineLayoutInfo, nullptr, &_TrianglePipelineLayout));

	FPipelineBuilder PipelineBuilder;
	PipelineBuilder._ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, TriangleVertShader));
	PipelineBuilder._ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, TriangleFragShader));
	PipelineBuilder._VertexInputInfo = VkInit::VertexInputStateCreateInfo();
	PipelineBuilder._InputAssembly = VkInit::InputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	PipelineBuilder._Viewport.x = 0.f;
	PipelineBuilder._Viewport.y = 0.f;
	PipelineBuilder._Viewport.width = (float)_WindowExtent.width;
	PipelineBuilder._Viewport.height = (float)_WindowExtent.height;
	PipelineBuilder._Viewport.minDepth = 0.f;
	PipelineBuilder._Viewport.maxDepth = 1.f;
	PipelineBuilder._Scissor.offset = { 0, 0 };
	PipelineBuilder._Scissor.extent = _WindowExtent;
	PipelineBuilder._Rasterizer = VkInit::RasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
	PipelineBuilder._Multisampling = VkInit::MultisampleStateCreateInfo();
	PipelineBuilder._ColorBlendAttachment = VkInit::ColorBlendAttachmentState();
	PipelineBuilder._PipelineLayout = _TrianglePipelineLayout;

	_TrianglePipeline = PipelineBuilder.BuildPipeline(_Device, _RenderPass);

	// Building for red triangle
	PipelineBuilder._ShaderStages.clear();
	PipelineBuilder._ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, RedTriangleVertShader));
	PipelineBuilder._ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, RedTriangleFragShader));
	_RedTrianglePipeline = PipelineBuilder.BuildPipeline(_Device, _RenderPass);

	// Triangle pipelines
	FVertexInputDescription VertexDescription = FVertex::GetVertexDescription();
	PipelineBuilder._VertexInputInfo.vertexBindingDescriptionCount = VertexDescription._Bindings.size();
	PipelineBuilder._VertexInputInfo.pVertexBindingDescriptions = VertexDescription._Bindings.data();
	PipelineBuilder._VertexInputInfo.vertexAttributeDescriptionCount = VertexDescription._Attributes.size();
	PipelineBuilder._VertexInputInfo.pVertexAttributeDescriptions = VertexDescription._Attributes.data();

	PipelineBuilder._ShaderStages.clear();
	VkShaderModule MeshVertShader = LoadShaderModule("../../shaders/tri_mesh.vert.spv");
	PipelineBuilder._ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, MeshVertShader));
	PipelineBuilder._ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, TriangleFragShader));

	_MeshPipeline = PipelineBuilder.BuildPipeline(_Device, _RenderPass);
	
	// Cleaning up
	vkDestroyShaderModule(_Device, RedTriangleVertShader, nullptr);
	vkDestroyShaderModule(_Device, RedTriangleFragShader, nullptr);
	vkDestroyShaderModule(_Device, TriangleVertShader, nullptr);
	vkDestroyShaderModule(_Device, TriangleFragShader, nullptr);
	vkDestroyShaderModule(_Device, MeshVertShader, nullptr);

	_MainDeletionQueue.PushFunction([=]() {
		vkDestroyPipeline(_Device, _RedTrianglePipeline, nullptr);
		vkDestroyPipeline(_Device, _TrianglePipeline, nullptr);
		vkDestroyPipeline(_Device, _MeshPipeline, nullptr);

		vkDestroyPipelineLayout(_Device, _TrianglePipelineLayout, nullptr);
	});
}

VkPipeline FPipelineBuilder::BuildPipeline(VkDevice Device, VkRenderPass Pass)
{
	VkPipelineViewportStateCreateInfo ViewportState{};
	ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	ViewportState.pNext = nullptr;
	ViewportState.viewportCount = 1;
	ViewportState.pViewports = &_Viewport;
	ViewportState.scissorCount = 1;
	ViewportState.pScissors = &_Scissor;

	// Match fragment shader output
	VkPipelineColorBlendStateCreateInfo ColorBlending{};
	ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	ColorBlending.pNext = nullptr;
	ColorBlending.logicOpEnable = VK_FALSE;
	ColorBlending.attachmentCount = 1;
	ColorBlending.pAttachments = &_ColorBlendAttachment;

	VkGraphicsPipelineCreateInfo PipelineInfo{};
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.pNext = nullptr;
	PipelineInfo.stageCount = _ShaderStages.size();
	PipelineInfo.pStages = _ShaderStages.data();
	PipelineInfo.pVertexInputState = &_VertexInputInfo;
	PipelineInfo.pInputAssemblyState = &_InputAssembly;
	PipelineInfo.pViewportState = &ViewportState;
	PipelineInfo.pRasterizationState = &_Rasterizer;
	PipelineInfo.pMultisampleState = &_Multisampling;
	PipelineInfo.pColorBlendState = &ColorBlending;
	PipelineInfo.layout = _PipelineLayout;
	PipelineInfo.renderPass = Pass; // Specify pass using this PSO
	PipelineInfo.subpass = 0;
	PipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkPipeline NewPipeline;
	if (vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &NewPipeline) != VK_SUCCESS)
	{
		std::cerr << "Failed to create graphics pipeline\n";
		return VK_NULL_HANDLE;
	}
	else
	{
		return NewPipeline;
	}
}

void FVulkanEngine::LoadMeshes()
{
	_TriangleMesh._Vertices.resize(3);

	_TriangleMesh._Vertices[0]._Position = {  1.f,  1.f, 0.f };
	_TriangleMesh._Vertices[1]._Position = { -1.f,  1.f, 0.f };
	_TriangleMesh._Vertices[2]._Position = {  0.f, -1.f, 0.f };

	_TriangleMesh._Vertices[0]._Color = { 0.f, 1.f, 0.f };
	_TriangleMesh._Vertices[1]._Color = { 0.f, 1.f, 0.f };
	_TriangleMesh._Vertices[2]._Color = { 0.f, 1.f, 0.f };

	UploadMesh(_TriangleMesh);
}

// Upload to device memory
void FVulkanEngine::UploadMesh(FMesh& Mesh)
{
	VkBufferCreateInfo BufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	BufferInfo.size = Mesh._Vertices.size() * sizeof(FVertex);
	BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	VmaAllocationCreateInfo VmaAllocInfo{};
	// Writable by CPU, readable by GPU
	VmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	VK_CHECK(vmaCreateBuffer(_Allocator, &BufferInfo, &VmaAllocInfo,
							 &Mesh._VertexBuffer._Buffer,
							 &Mesh._VertexBuffer._Allocation,
							 nullptr));

	_MainDeletionQueue.PushFunction([=]() { 
		vmaDestroyBuffer(_Allocator, Mesh._VertexBuffer._Buffer, Mesh._VertexBuffer._Allocation); 
	});

	void* Data;
	vmaMapMemory(_Allocator, Mesh._VertexBuffer._Allocation, &Data);
	memcpy(Data, Mesh._Vertices.data(), Mesh._Vertices.size() * sizeof(FVertex));
	vmaUnmapMemory(_Allocator, Mesh._VertexBuffer._Allocation);
}