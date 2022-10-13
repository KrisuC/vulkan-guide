// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"
#include <vector>
#include <deque>
#include <functional>
#include "vk_mem_alloc.h"
#include "vk_mesh.h"
#include "glm/glm.hpp"
// xiangyang maomao mingzi jiao jinjiaodawang he yinjiaodawang!

class FPipelineBuilder
{
public:
	
	std::vector<VkPipelineShaderStageCreateInfo> _ShaderStages;
	VkPipelineVertexInputStateCreateInfo _VertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _InputAssembly;
	VkViewport _Viewport;
	VkRect2D _Scissor;
	VkPipelineRasterizationStateCreateInfo _Rasterizer;
	VkPipelineMultisampleStateCreateInfo _Multisampling;
	VkPipelineColorBlendAttachmentState _ColorBlendAttachment;
	VkPipelineLayout _PipelineLayout;

	VkPipeline BuildPipeline(VkDevice _Device, VkRenderPass _Pass);
};

class FDeletionQueue
{
	std::deque<std::function<void()>> Deletors;

public:
	void PushFunction(std::function<void()>&& Function)
	{
		Deletors.push_back(Function);
	}

	void Flush()
	{
		for (auto It = Deletors.rbegin(); It != Deletors.rend(); It++)
		{
			(*It)();
		}
		Deletors.clear();
	}
};

class FVulkanEngine {
public:

	bool _bIsInitialized{ false };
	int _FrameNumber {0};

	VkInstance _Instance;
	VkDebugUtilsMessengerEXT _DebugMessenger;
	VkPhysicalDevice _ChosenGPU;
	VkDevice _Device;
	VkSurfaceKHR _Surface;

	VkSwapchainKHR _SwapChain;
	VkFormat _SwapChainImageFormat;
	std::vector<VkImage> _SwapChainImages;
	std::vector<VkImageView> _SwapChainImageViews;

	VkQueue _GraphicsQueue;
	uint32_t _GraphicsQueueFamily;

	VkCommandPool _CommandPool;
	VkCommandBuffer _MainCommandBuffer;

	VkRenderPass _RenderPass;
	std::vector<VkFramebuffer> _Framebuffers;

	VkSemaphore _PresentSemaphore, _RenderSemaphore;
	VkFence _RenderFence;

	VkPipelineLayout _TrianglePipelineLayout;
	VkPipeline _TrianglePipeline;
	VkPipeline _RedTrianglePipeline;

	int _SelectedShader{ 0 };

	FDeletionQueue _MainDeletionQueue;

	VmaAllocator _Allocator;

	VkPipeline _MeshPipeline;
	FMesh _TriangleMesh;
	VkPipelineLayout _MeshPipelineLayout;

	// 128 Bytes
	struct FMeshPushConstant
	{
		glm::vec4 _Data;
		glm::mat4 _RenderMatrix;
	};

	VkExtent2D _WindowExtent{ 720 , 460 };

	struct SDL_Window* _Window{ nullptr };

	//initializes everything in the engine
	void Init();
	void InitVulkan();
	void InitSwapChain();
	void InitCommands();
	void InitDefaultRenderPass();
	void InitFramebuffers();
	void InitSyncStructures();
	void InitPipelines();

	VkShaderModule LoadShaderModule(const char* FilePath);
	void LoadMeshes();
	void UploadMesh(FMesh& Mesh);

	//shuts down the engine
	void Cleanup();

	//draw loop
	void Draw();

	//run main loop
	void Run();
};

