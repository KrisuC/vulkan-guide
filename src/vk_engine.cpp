#define VMA_IMPLEMENTATION

#include "vk_engine.h"

#include <iostream>
#include <fstream>
#include <algorithm>

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

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
		if (!(X))													\
		{														\
			throw Str;											\
		}													    \
	}

#define CHECK(X)												\
	{															\
		if (!(X))													\
		{														\
			throw "";											\
		}													    \
	}

#define ONE_SECOND 1000000000
#define INF_TIME   9999999999

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
	InitDepthBuffer();
	InitCommands();
	InitDefaultRenderPass();
	InitFramebuffers();
	InitSyncStructures();
	InitDescriptors();
	InitPipelines();
	LoadMeshes();
	InitScene();
	
	//everything went fine
	_bIsInitialized = true;
}
void FVulkanEngine::Cleanup()
{	
	if (_bIsInitialized) 
	{	
		for (int32_t i = 0; i < FRAME_OVERLAP; i++)
		{
			vkWaitForFences(_Device, 1, &_Frames[i]._RenderFence, true, ONE_SECOND);
		}
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
	VkPhysicalDeviceShaderDrawParametersFeatures ShaderDrawParamtersFeatures{};
	ShaderDrawParamtersFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	ShaderDrawParamtersFeatures.pNext = nullptr;
	ShaderDrawParamtersFeatures.shaderDrawParameters = VK_TRUE;

	vkb::Device VkbDevice = Devicebuilder.add_pNext(&ShaderDrawParamtersFeatures).build().value();

	_Device = VkbDevice.device;
	_GpuProperties = VkbDevice.physical_device.properties;
	_ChosenGPU = PhysicalDevice.physical_device;

	std::cout << "The GPU has a minimum buffer alignment of " << _GpuProperties.limits.minUniformBufferOffsetAlignment << std::endl;

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

void FVulkanEngine::InitDepthBuffer()
{
	VkExtent3D DepthImageExtent{ _WindowExtent.width, _WindowExtent.height, 1 };
	_DepthFormat = VK_FORMAT_D32_SFLOAT;

	VkImageCreateInfo DepthInfo = VkInit::ImageCreateInfo(_DepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, DepthImageExtent);

	VmaAllocationCreateInfo DepthImageAllocInfo{};
	DepthImageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // more like a hint
	DepthImageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // force to  be device local

	vmaCreateImage(_Allocator, &DepthInfo, &DepthImageAllocInfo, &_DepthImage._Image, &_DepthImage._Allocation, nullptr);

	VkImageViewCreateInfo DepthViewInfo = VkInit::ImageViewCreateInfo(_DepthFormat, _DepthImage._Image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(_Device, &DepthViewInfo, nullptr, &_DepthImageView));

	_MainDeletionQueue.PushFunction([=]() {
		vkDestroyImageView(_Device, _DepthImageView, nullptr);
		vmaDestroyImage(_Allocator, _DepthImage._Image, _DepthImage._Allocation);
	});
}

void FVulkanEngine::InitCommands()
{	
	VkCommandPoolCreateInfo CommandPoolInfo = VkInit::CommandPoolCreateInfo(_GraphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (int32_t i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateCommandPool(_Device, &CommandPoolInfo, nullptr, &_Frames[i]._CommandPool));

		VkCommandBufferAllocateInfo CmdAllocInfo = VkInit::CommandBufferAllocateInfo(_Frames[i]._CommandPool);
		VK_CHECK(vkAllocateCommandBuffers(_Device, &CmdAllocInfo, &_Frames[i]._MainCommandBuffer));

		_MainDeletionQueue.PushFunction([=]()
		{
			vkDestroyCommandPool(_Device, _Frames[i]._CommandPool, nullptr);
		});
	}

	// Upload context
	VkCommandPoolCreateInfo UploadCommandPoolInfo = VkInit::CommandPoolCreateInfo(_GraphicsQueueFamily);
	VK_CHECK(vkCreateCommandPool(_Device, &UploadCommandPoolInfo, nullptr, &_UploadContext._CommandPool));

	_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroyCommandPool(_Device, _UploadContext._CommandPool, nullptr);
	});

	VkCommandBufferAllocateInfo CmdAllocInfo = VkInit::CommandBufferAllocateInfo(_UploadContext._CommandPool, 1);
	VK_CHECK(vkAllocateCommandBuffers(_Device, &CmdAllocInfo, &_UploadContext._CommandBuffer));
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

	VkAttachmentDescription DepthAttachment{};
	DepthAttachment.flags = 0;
	DepthAttachment.format = _DepthFormat;
	DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // But not used for now
	DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // not used as SRV

	VkAttachmentDescription Attachments[2]{ ColorAttachment, DepthAttachment };

	// Creating subpass 0 - Don't really access to image, but just require attachment information
	VkAttachmentReference ColorAttachmentRef{};
	ColorAttachmentRef.attachment = 0;
	ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // layout is for compression (?)
	VkAttachmentReference DepthAttachmentRef{};
	DepthAttachmentRef.attachment = 1; // index into VkRenderpassCreateInfo pAttachments
	DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkSubpassDescription Subpass{};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachmentRef;
	Subpass.pDepthStencilAttachment = &DepthAttachmentRef;

	// Dependency to synchronize accessing
	// Presentation -> Previous color attachment output -> Presentation -> Current color attachment output 
	VkSubpassDependency ColorDependency{};
	ColorDependency.srcSubpass = VK_SUBPASS_EXTERNAL; // includes commands that occur earlier in submission order than the vkCmdBeginRenderPass used to begin the render pass instance
	// wait for color attachment output, because color attachment output will wait for _PresentSemaphore (in SubmitInfo), after which image presentation is finished and will be available for rendering (?)
	ColorDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // previous color output
	ColorDependency.srcAccessMask = 0; // 0 means we don't care, just make sure the srcStage is finished. excution dependency
	ColorDependency.dstSubpass = 0;
	ColorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	ColorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency DepthDependency{};
	DepthDependency.srcSubpass = VK_SUBPASS_EXTERNAL; 
	// Cmds earlier must finishing early/late z stage memory access (last frame's)
	DepthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	DepthDependency.srcAccessMask = 0;
	DepthDependency.dstSubpass = 0;
	DepthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	DepthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkSubpassDependency Dependencies[2]{ ColorDependency, DepthDependency };

	VkRenderPassCreateInfo RenderPassInfo{};
	RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	RenderPassInfo.pNext = nullptr;
	RenderPassInfo.attachmentCount = 2;
	RenderPassInfo.pAttachments = Attachments;
	RenderPassInfo.subpassCount = 1;
	RenderPassInfo.pSubpasses = &Subpass;
	RenderPassInfo.dependencyCount = 2;
	RenderPassInfo.pDependencies = Dependencies;

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
	FramebufferInfo.attachmentCount = 2; // Color and depth
	FramebufferInfo.width = _WindowExtent.width;
	FramebufferInfo.height = _WindowExtent.height;
	FramebufferInfo.layers = 1;

	const size_t SwapChainImageCount = _SwapChainImages.size();
	_Framebuffers = std::vector<VkFramebuffer>(SwapChainImageCount);

	// Creating a framebuffer for each of the swapchain image
	for (size_t i = 0; i < SwapChainImageCount; i++)
	{
		VkImageView Attachments[2]{ _SwapChainImageViews[i], _DepthImageView };
		FramebufferInfo.pAttachments = Attachments;
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
	VkSemaphoreCreateInfo SemaphoreCreateInfo = VkInit::SemaphoreCreateInfo();

	for (int32_t i = 0; i < FRAME_OVERLAP; i++)
	{
		VK_CHECK(vkCreateFence(_Device, &FenceCreateInfo, nullptr, &_Frames[i]._RenderFence));

		_MainDeletionQueue.PushFunction([=]() 
		{ 
			vkDestroyFence(_Device, _Frames[i]._RenderFence, nullptr); 
		});

		VK_CHECK(vkCreateSemaphore(_Device, &SemaphoreCreateInfo, nullptr, &_Frames[i]._PresentSemaphore));
		VK_CHECK(vkCreateSemaphore(_Device, &SemaphoreCreateInfo, nullptr, &_Frames[i]._RenderSemaphore));

		_MainDeletionQueue.PushFunction([=]()
		{
			vkDestroySemaphore(_Device, _Frames[i]._PresentSemaphore, nullptr);
			vkDestroySemaphore(_Device, _Frames[i]._RenderSemaphore, nullptr);
		});
	}

	// Upload context
	// SIGNALED_BIT is not set because we don't need to signal it like _RenderFence in vkQueueSubmit
	VkFenceCreateInfo UploadFenceCreateInfo = VkInit::FenceCreateInfo();

	VK_CHECK(vkCreateFence(_Device, &UploadFenceCreateInfo, nullptr, &_UploadContext._UploadFence));
	_MainDeletionQueue.PushFunction([=]()
	{
		vkDestroyFence(_Device, _UploadContext._UploadFence, nullptr);
	});
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
	// Creating pipeline layout
	VkDescriptorSetLayout SetLayouts[]{ _GlobalSetLayout, _ObjectSetLayout };

	VkPipelineLayoutCreateInfo MeshPipelineLayoutInfo = VkInit::PipelineLayoutCreateInfo();
	MeshPipelineLayoutInfo.pushConstantRangeCount = 0;
	MeshPipelineLayoutInfo.pPushConstantRanges = nullptr;
	MeshPipelineLayoutInfo.setLayoutCount = 2;
	MeshPipelineLayoutInfo.pSetLayouts = SetLayouts;

	VK_CHECK(vkCreatePipelineLayout(_Device, &MeshPipelineLayoutInfo, nullptr, &_MeshPipelineLayout));
	_MainDeletionQueue.PushFunction([=](){
		vkDestroyPipelineLayout(_Device, _MeshPipelineLayout, nullptr);
	});

	FPipelineBuilder PipelineBuilder;
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
	PipelineBuilder._DepthStencilInfo = VkInit::DepthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);
	// Vertex descritption
	FVertexInputDescription VertexDescription = FVertex::GetVertexDescription();
	PipelineBuilder._VertexInputInfo.vertexBindingDescriptionCount = VertexDescription._Bindings.size();
	PipelineBuilder._VertexInputInfo.pVertexBindingDescriptions = VertexDescription._Bindings.data();
	PipelineBuilder._VertexInputInfo.vertexAttributeDescriptionCount = VertexDescription._Attributes.size();
	PipelineBuilder._VertexInputInfo.pVertexAttributeDescriptions = VertexDescription._Attributes.data();

	VkShaderModule MeshVertShader = LoadShaderModule("../../shaders/tri_mesh.vert.spv");
	VkShaderModule TriangleFragShader = LoadShaderModule("../../shaders/default_lit.frag.spv");
	PipelineBuilder._ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, MeshVertShader));
	PipelineBuilder._ShaderStages.push_back(VkInit::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, TriangleFragShader));
	PipelineBuilder._PipelineLayout = _MeshPipelineLayout;

	_MeshPipeline = PipelineBuilder.BuildPipeline(_Device, _RenderPass);

	CreateMaterial(_MeshPipeline, _MeshPipelineLayout, "DefaultMaterial");
	
	// Cleaning up
	vkDestroyShaderModule(_Device, TriangleFragShader, nullptr);
	vkDestroyShaderModule(_Device, MeshVertShader, nullptr);

	_MainDeletionQueue.PushFunction([=]() {
		vkDestroyPipeline(_Device, _MeshPipeline, nullptr);
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
	PipelineInfo.pDepthStencilState = &_DepthStencilInfo;
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
	FMesh TriangleMesh;
	TriangleMesh._Vertices.resize(3);
	TriangleMesh._Vertices[0]._Position = {  1.f,  1.f, 0.f };
	TriangleMesh._Vertices[1]._Position = { -1.f,  1.f, 0.f };
	TriangleMesh._Vertices[2]._Position = {  0.f, -1.f, 0.f };
	TriangleMesh._Vertices[0]._Normal = { 0.f, 1.f, 0.f };
	TriangleMesh._Vertices[1]._Normal = { 0.f, 1.f, 0.f };
	TriangleMesh._Vertices[2]._Normal = { 0.f, 1.f, 0.f };
	UploadMesh(TriangleMesh);
	_Meshes["Triangle"] = TriangleMesh;

	FMesh MonkeyMesh;
	MonkeyMesh.LoadFromObj("../../assets/monkey_smooth.obj");
	UploadMesh(MonkeyMesh);
	_Meshes["Monkey"] = MonkeyMesh;
}

// Upload to device memory
void FVulkanEngine::UploadMesh(FMesh& Mesh)
{
	const size_t BufferSize = Mesh._Vertices.size() * sizeof(FVertex);

	// Creating staging buffer
	VkBufferCreateInfo StagingBufferInfo{};
	StagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	StagingBufferInfo.pNext = nullptr;
	StagingBufferInfo.size = BufferSize;
	StagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo VmaAllocInfo{};
	VmaAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	FAllocatedBuffer StagingBuffer;
	
	VK_CHECK(vmaCreateBuffer(_Allocator, &StagingBufferInfo, &VmaAllocInfo,
		&StagingBuffer._Buffer, &StagingBuffer._Allocation, nullptr));

	// Copying data from cpu to staging buffer
	void* Data;
	vmaMapMemory(_Allocator, StagingBuffer._Allocation, &Data);
	memcpy(Data, Mesh._Vertices.data(), Mesh._Vertices.size() * sizeof(FVertex));
	vmaUnmapMemory(_Allocator, StagingBuffer._Allocation);

	// Creating actual vertex buffer
	VkBufferCreateInfo VertexBufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	VertexBufferInfo.size = BufferSize;
	VertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	VK_CHECK(vmaCreateBuffer(_Allocator, &VertexBufferInfo, &VmaAllocInfo,
							 &Mesh._VertexBuffer._Buffer,
							 &Mesh._VertexBuffer._Allocation,
							 nullptr));

	ImmediateSubmit([=](VkCommandBuffer Cmd)
	{
		VkBufferCopy Copy;
		Copy.dstOffset = 0;
		Copy.srcOffset = 0;
		Copy.size = BufferSize;
		vkCmdCopyBuffer(Cmd, StagingBuffer._Buffer, Mesh._VertexBuffer._Buffer, 1, &Copy);
	});

	// Instantly delete because we force sync
	vmaDestroyBuffer(_Allocator, StagingBuffer._Buffer, StagingBuffer._Allocation);

	_MainDeletionQueue.PushFunction([=]() { 
		vmaDestroyBuffer(_Allocator, Mesh._VertexBuffer._Buffer, Mesh._VertexBuffer._Allocation); 
	});
}

void FVulkanEngine::Draw()
{
	// Wait for rendering is done for previous double bufferred frame
	VK_CHECK(vkWaitForFences(_Device, 1, &GetCurrentFrame()._RenderFence, true, ONE_SECOND));	// 1 sec
	VK_CHECK(vkResetFences(_Device, 1, &GetCurrentFrame()._RenderFence));

	uint32_t SwapChainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(_Device, _SwapChain, ONE_SECOND, GetCurrentFrame()._PresentSemaphore, nullptr, &SwapChainImageIndex)); // signal _PresentSemaphore

	VK_CHECK(vkResetCommandBuffer(GetCurrentFrame()._MainCommandBuffer, 0));
	VkCommandBuffer Cmd = GetCurrentFrame()._MainCommandBuffer;

	VkCommandBufferBeginInfo CmdBeginInfo{};
	CmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	CmdBeginInfo.pNext = nullptr;
	CmdBeginInfo.pInheritanceInfo = nullptr;
	CmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(Cmd, &CmdBeginInfo));
	{
		VkClearValue ColorClearValue{};
		// float Flash = abs(sin(_FrameNumber / 120.f));
		ColorClearValue.color = { { 0.0f, 0.0f, 0.f, 1.0f } };

		VkClearValue DepthClearValue{};
		DepthClearValue.depthStencil.depth = 1.f; // far, no reverse Z

		VkRenderPassBeginInfo RpInfo{};
		RpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		RpInfo.pNext = nullptr;
		RpInfo.renderPass = _RenderPass;
		RpInfo.renderArea.offset.x = 0;
		RpInfo.renderArea.offset.y = 0;
		RpInfo.renderArea.extent = _WindowExtent;
		RpInfo.framebuffer = _Framebuffers[SwapChainImageIndex];

		// Order is same as RpInfo
		VkClearValue ClearValues[2]{ ColorClearValue, DepthClearValue };
		RpInfo.clearValueCount = 2;
		RpInfo.pClearValues = ClearValues;

		vkCmdBeginRenderPass(Cmd, &RpInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			DrawObjects(Cmd, _Renderables.data(), _Renderables.size());
		}
		vkCmdEndRenderPass(Cmd);
	}
	VK_CHECK(vkEndCommandBuffer(Cmd));

	VkSubmitInfo Submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// Wait for the present is finished (image is available) before we output color attachment
	Submit.pWaitDstStageMask = &WaitStage; // which each corresponding semaphore wait will occur
	// Wait for image is aquired, making sure image for rendering is ready
	Submit.waitSemaphoreCount = 1;
	Submit.pWaitSemaphores = &GetCurrentFrame()._PresentSemaphore;
	// Lock _RenderSemaphore, unlock when the GPU job is done
	Submit.signalSemaphoreCount = 1;
	Submit.pSignalSemaphores = &GetCurrentFrame()._RenderSemaphore;
	Submit.commandBufferCount = 1;
	Submit.pCommandBuffers = &Cmd;

	VK_CHECK(vkQueueSubmit(_GraphicsQueue, 1, &Submit, GetCurrentFrame()._RenderFence));

	VkPresentInfoKHR PresentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = &_SwapChain;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = &GetCurrentFrame()._RenderSemaphore; // Wait for rendering is completed
	PresentInfo.pImageIndices = &SwapChainImageIndex;

	VK_CHECK(vkQueuePresentKHR(_GraphicsQueue, &PresentInfo));

	_FrameNumber++;
}

FMaterial* FVulkanEngine::CreateMaterial(VkPipeline Pipeline, VkPipelineLayout Layout, const std::string& Name)
{
	FMaterial Mat;
	Mat._Pipeline = Pipeline;
	Mat._PipelineLayout = Layout;
	_Materials[Name] = Mat;
	return &_Materials[Name];
}

FMaterial* FVulkanEngine::GetMaterial(const std::string& Name)
{
	auto It = _Materials.find(Name);
	return It == _Materials.end() ? nullptr : &It->second;
}

FMesh* FVulkanEngine::GetMesh(const std::string& Name)
{
	auto It = _Meshes.find(Name);
	return It == _Meshes.end() ? nullptr : &It->second;
}

// Hard coded init scene
void FVulkanEngine::InitScene()
{
	FRenderObject Monkey;
	Monkey._Mesh = GetMesh("Monkey");
	Monkey._Material = GetMaterial("DefaultMaterial");
	Monkey._TransformMatrix = glm::mat4(1.f);
	Monkey._Color = glm::vec3(1, 1, 1);
	_Renderables.push_back(Monkey);

	for (int x = -20; x <= 20; x++)
	{
		for (int y = -20; y <= 20; y++)
		{
			FRenderObject Triangle;
			Triangle._Mesh = GetMesh("Triangle");
			Triangle._Material = GetMaterial("DefaultMaterial");
			glm::mat4 Translation = glm::translate(glm::mat4(1.f), glm::vec3(x, 0, y));
			Triangle._Color = glm::vec3(((x + 20.f) / 40.f), 0, ((y + 20.f) / 40.f));
			glm::mat4 Scale = glm::scale(glm::mat4(1.f), glm::vec3(0.2, 0.2, 0.2));
			Triangle._TransformMatrix = Translation * Scale;
			_Renderables.push_back(Triangle);
		}
	}
}

void FVulkanEngine::DrawObjects(VkCommandBuffer Cmd, FRenderObject* FirstObject, int ObjectCount)
{
	int32_t FrameIndex = _FrameNumber % FRAME_OVERLAP;
	float FrameD = (_FrameNumber / 120.f);

	// Updating uniforms and SSBO
	FGpuGlobalData GlobalData;

	glm::vec3 CamPos = glm::vec3(0.f, -10.f, -25.f)/* * std::abs(std::sin(FrameD)) * 0.7f + glm::vec3(0.f, -1.f, -1.f)*/;
	glm::mat4 View = glm::translate(glm::mat4(1.f), CamPos);
	glm::mat4 Projection = glm::perspective(glm::radians(70.f), (float)_WindowExtent.width / _WindowExtent.height, 0.1f, 400.f);
	Projection[1][1] *= -1;

	GlobalData._Camera._Proj = Projection;
	GlobalData._Camera._View = View;
	GlobalData._Camera._ViewProj = Projection * View;

	GlobalData._Environment._AmbientColor = glm::vec4(0.f); // { std::sin(FrameD), 0, std::cos(FrameD), 1 };

	// Sending data from CPU to GPU
	// Double bufferred
	byte* GlobalDataMapped;
	vmaMapMemory(_Allocator, _SceneGlobalBuffer._Allocation, (void**)& GlobalDataMapped);
	{
		GlobalDataMapped += PadUniformBufferSize(sizeof(FGpuGlobalData)) * FrameIndex;
		*reinterpret_cast<FGpuGlobalData*>(GlobalDataMapped) = GlobalData;
	}
	vmaUnmapMemory(_Allocator, _SceneGlobalBuffer._Allocation);

	byte* ObjectDataMapped;
	vmaMapMemory(_Allocator, _SceneObjectBuffer._Allocation, (void**)&ObjectDataMapped);
	{	
		ObjectDataMapped += PadUniformBufferSize(sizeof(FGpuObjectData) * MAX_OBJECTS) * FrameIndex;
		for (int32_t i = 0; i < ObjectCount; i++)
		{
			const FRenderObject& Object = FirstObject[i];
			FGpuObjectData* ObjectInBufferToWrite = reinterpret_cast<FGpuObjectData*>(ObjectDataMapped) + i;
			ObjectInBufferToWrite->_ModelMatrix = Object._TransformMatrix;
			ObjectInBufferToWrite->_Color = glm::vec4(Object._Color, 1.f);
		}
	}
	vmaUnmapMemory(_Allocator, _SceneObjectBuffer._Allocation);

	FMesh* LastMesh = nullptr;
	FMaterial* LastMaterial = nullptr;
	for (int i = 0; i < ObjectCount; i++)
	{
		FRenderObject& Object = FirstObject[i];
		// Rebind pipeline if not same material
		if (Object._Material != LastMaterial)
		{
			vkCmdBindPipeline(Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Object._Material->_Pipeline);
			LastMaterial = Object._Material;

			uint32_t GlobalsOffset = PadUniformBufferSize(sizeof(FGpuGlobalData)) * FrameIndex;
			uint32_t ObjectsBufferOffset = PadUniformBufferSize(sizeof(FGpuObjectData) * MAX_OBJECTS) * FrameIndex;

			uint32_t Offsets[2]{ GlobalsOffset, ObjectsBufferOffset };

			VkDescriptorSet SetsToBind[]{ _SceneGlobalDescriptorSet, _SceneObjectDescriptorSet };
			vkCmdBindDescriptorSets(
				Cmd, 
				VK_PIPELINE_BIND_POINT_GRAPHICS, 					
				Object._Material->_PipelineLayout, 
				0, 2, SetsToBind, 
				2, Offsets);
		}

		if (Object._Mesh != LastMesh)
		{
			VkDeviceSize Offset = 0;
			vkCmdBindVertexBuffers(Cmd, 0, 1, &Object._Mesh->_VertexBuffer._Buffer, &Offset);
			LastMesh = Object._Mesh;
		}
		CHECK(Object._Mesh != nullptr);
		// Using first instance to hack primitive ID
		vkCmdDraw(Cmd, Object._Mesh->_Vertices.size(), 1, 0, i);
	}
}

FFrameData& FVulkanEngine::GetCurrentFrame()
{
	return _Frames[_FrameNumber % FRAME_OVERLAP];
}

FAllocatedBuffer FVulkanEngine::CreateBuffer(size_t AllocSize, VkBufferUsageFlags Usage, VmaMemoryUsage MemoryUsage)
{
	VkBufferCreateInfo BufferInfo{};
	BufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	BufferInfo.pNext = nullptr;

	BufferInfo.size = AllocSize;
	BufferInfo.usage = Usage;

	VmaAllocationCreateInfo VmaAllocInfo{};
	VmaAllocInfo.usage = MemoryUsage;

	FAllocatedBuffer NewBuffer;

	VK_CHECK(vmaCreateBuffer(_Allocator, &BufferInfo, &VmaAllocInfo,
			 &NewBuffer._Buffer,
			 &NewBuffer._Allocation,
			 nullptr));

	return NewBuffer;
}

// Set 0 -> Binding 0 -> Globals
//						-> Camera
//						-> Environment
// Set 1 -> Binding 0 -> Objects
void FVulkanEngine::InitDescriptors()
{
	// 1. Creating descriptor set layout
	VkDescriptorSetLayoutBinding GlobalBind = VkInit::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	VkDescriptorSetLayoutCreateInfo Set0Info{};
	Set0Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	Set0Info.pNext = nullptr;
	Set0Info.bindingCount = 1;
	Set0Info.pBindings = &GlobalBind;

	vkCreateDescriptorSetLayout(_Device, &Set0Info, nullptr, &_GlobalSetLayout);

	VkDescriptorSetLayoutBinding ObjectBind = VkInit::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT, 0);
	VkDescriptorSetLayoutCreateInfo Set1Info{};
	Set1Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	Set1Info.pNext = nullptr;
	Set1Info.bindingCount = 1;
	Set1Info.pBindings = &ObjectBind;

	vkCreateDescriptorSetLayout(_Device, &Set1Info, nullptr, &_ObjectSetLayout);

	// 2. Creating descriptor pool
	// Up to 10 uniform buffer for now
	std::vector<VkDescriptorPoolSize> Sizes{ 
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10}
	};
	VkDescriptorPoolCreateInfo PoolInfo{};
	PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	PoolInfo.flags = 0;
	PoolInfo.maxSets = 10;
	PoolInfo.poolSizeCount = (uint32_t)Sizes.size();
	PoolInfo.pPoolSizes = Sizes.data();

	vkCreateDescriptorPool(_Device, &PoolInfo, nullptr, &_DescriptorPool);

	// 2.5. Creating Buffers
	_SceneGlobalBuffer = CreateBuffer(
		PadUniformBufferSize(sizeof(FGpuGlobalData)) * FRAME_OVERLAP, 
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
		VMA_MEMORY_USAGE_CPU_TO_GPU);

	// @todo: make this grow dynamically
	_SceneObjectBuffer = CreateBuffer(
		PadUniformBufferSize(sizeof(FGpuObjectData)) * MAX_OBJECTS * FRAME_OVERLAP, 
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
		VMA_MEMORY_USAGE_CPU_TO_GPU);

	// 3. Allocating descriptor sets
	VkDescriptorSet SetsToAllocate[2];
	VkDescriptorSetLayout SetLayouts[]{ _GlobalSetLayout, _ObjectSetLayout };

	VkDescriptorSetAllocateInfo AllocInfo{};
	AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	AllocInfo.pNext = nullptr;
	AllocInfo.descriptorPool = _DescriptorPool;
	AllocInfo.descriptorSetCount = 2;
	AllocInfo.pSetLayouts = SetLayouts;

	VK_CHECK(vkAllocateDescriptorSets(_Device, &AllocInfo, SetsToAllocate));
	_SceneGlobalDescriptorSet = SetsToAllocate[0];
	_SceneObjectDescriptorSet = SetsToAllocate[1];

	// 4. Pointing descriptor to buffer
	VkDescriptorBufferInfo GlobalBufferInfo{};
	GlobalBufferInfo.buffer = _SceneGlobalBuffer._Buffer;
	GlobalBufferInfo.offset = 0;
	GlobalBufferInfo.range = sizeof(FGpuGlobalData);

	VkDescriptorBufferInfo ObjectBufferInfo{};
	ObjectBufferInfo.buffer = _SceneObjectBuffer._Buffer;
	ObjectBufferInfo.offset = 0;
	ObjectBufferInfo.range = sizeof(FGpuObjectData) * MAX_OBJECTS;

	VkWriteDescriptorSet GlobalWrite = VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _SceneGlobalDescriptorSet, &GlobalBufferInfo, 0);
	VkWriteDescriptorSet ObjectWrite = VkInit::WriteDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, _SceneObjectDescriptorSet, &ObjectBufferInfo, 0);

	VkWriteDescriptorSet SetWrites[]{ GlobalWrite, ObjectWrite };

	vkUpdateDescriptorSets(_Device, 2, SetWrites, 0, nullptr);

	_MainDeletionQueue.PushFunction([&]()
	{
		vmaDestroyBuffer(_Allocator, _SceneGlobalBuffer._Buffer, _SceneGlobalBuffer._Allocation);
		vmaDestroyBuffer(_Allocator, _SceneObjectBuffer._Buffer, _SceneObjectBuffer._Allocation);
		vkDestroyDescriptorSetLayout(_Device, _GlobalSetLayout, nullptr);
		vkDestroyDescriptorSetLayout(_Device, _ObjectSetLayout, nullptr);
		vkDestroyDescriptorPool(_Device, _DescriptorPool, nullptr);
	});
}

size_t FVulkanEngine::PadUniformBufferSize(size_t OriginalSize)
{
	size_t MinUboAlignment = _GpuProperties.limits.minUniformBufferOffsetAlignment;
	size_t AlignedSize = OriginalSize;
	if (MinUboAlignment > 0)
	{
		AlignedSize = (AlignedSize + MinUboAlignment - 1) & ~(MinUboAlignment - 1);
	}
	return AlignedSize;
}

void FVulkanEngine::ImmediateSubmit(std::function<void(VkCommandBuffer Cmd)>&& Function)
{
	VkCommandBuffer Cmd = _UploadContext._CommandBuffer;

	VkCommandBufferBeginInfo CmdBeginInfo = VkInit::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	
	VK_CHECK(vkBeginCommandBuffer(Cmd, &CmdBeginInfo));
	Function(Cmd);
	VK_CHECK(vkEndCommandBuffer(Cmd));

	VkSubmitInfo Submit = VkInit::SubmitInfo(&Cmd);

	VK_CHECK(vkQueueSubmit(_GraphicsQueue, 1, &Submit, _UploadContext._UploadFence));

	// Force sync, better way is to be async
	vkWaitForFences(_Device, 1, &_UploadContext._UploadFence, true, INF_TIME);
	vkResetFences(_Device, 1, &_UploadContext._UploadFence);

	vkResetCommandPool(_Device, _UploadContext._CommandPool, 0);
}