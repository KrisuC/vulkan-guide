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

using byte = char;

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
	VkPipelineDepthStencilStateCreateInfo _DepthStencilInfo;

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

struct FMaterial
{
	VkPipeline _Pipeline;
	VkPipelineLayout _PipelineLayout;
};

// Representing a single Draw
struct FRenderObject
{
	FMesh* _Mesh;
	FMaterial* _Material;
	glm::mat4 _TransformMatrix;
};

struct FGpuGlobalData
{
	struct FGpuCameraData
	{
		glm::mat4 _View;
		glm::mat4 _Proj;
		glm::mat4 _ViewProj;
	};

	struct FGpuEvironmentData
	{
		glm::vec4 _FogColor;
		glm::vec4 _FogDistances;
		glm::vec4 _AmbientColor;
		glm::vec4 _SunlightDirection;
		glm::vec4 _SunlightColor;
	};

	FGpuCameraData _Camera;
	FGpuEvironmentData _Environment;
};

struct FGpuObjectData
{
	glm::mat4 _ModelMatrix;
};

// 1 frame writing commands on CPU, another frame excuting on the GPU
struct FFrameData
{
	VkSemaphore _PresentSemaphore;
	VkSemaphore _RenderSemaphore;
	VkFence _RenderFence;

	VkCommandPool _CommandPool;
	VkCommandBuffer _MainCommandBuffer;

	// @TODO: refactoring these using dynamic offset
	// Scene-wide and object-wide descriptor and buffer
	FAllocatedBuffer _SceneGlobalBuffer;
	VkDescriptorSet _SceneGlobalDescriptorSet;

	FAllocatedBuffer _SceneObjectBuffer;
	VkDescriptorSet _SceneObjectDescriptorSet;
};

constexpr uint32_t FRAME_OVERLAP = 2;

class FVulkanEngine {
public:

	bool _bIsInitialized{ false };
	int _FrameNumber {0};

	// Hardware abstraction
	VkInstance _Instance;
	VkDebugUtilsMessengerEXT _DebugMessenger;
	VkPhysicalDevice _ChosenGPU;
	VkDevice _Device;
	VkSurfaceKHR _Surface;
	VkPhysicalDeviceProperties _GpuProperties;

	// Swap chain
	VkSwapchainKHR _SwapChain;
	VkFormat _SwapChainImageFormat;

	// Images
	std::vector<VkImage> _SwapChainImages;
	std::vector<VkImageView> _SwapChainImageViews;

	VkImageView _DepthImageView;
	FAllocatedImage _DepthImage;
	VkFormat _DepthFormat;
	
	VkExtent2D _WindowExtent{ 720 , 460 };
	struct SDL_Window* _Window{ nullptr };

	// Queues
	VkQueue _GraphicsQueue;
	uint32_t _GraphicsQueueFamily;

	// Render pass
	VkRenderPass _RenderPass;
	std::vector<VkFramebuffer> _Framebuffers;

	// PSO
	VkPipeline _MeshPipeline;
	VkPipelineLayout _MeshPipelineLayout;

	// Descriptors
	VkDescriptorSetLayout _GlobalSetLayout;
	VkDescriptorSetLayout _ObjectSetLayout;

	// Double buffer
	FFrameData _Frames[FRAME_OVERLAP];

	VkDescriptorPool _DescriptorPool;

	// Helper
	int _SelectedShader{ 0 };

	FDeletionQueue _MainDeletionQueue;

	VmaAllocator _Allocator;

	//initializes everything in the engine
	void Init();
	void InitVulkan();
	void InitSwapChain();
	void InitDepthBuffer();
	void InitCommands();
	void InitDefaultRenderPass();
	void InitFramebuffers();
	void InitSyncStructures();
	void InitPipelines();
	void InitDescriptors();

	VkShaderModule LoadShaderModule(const char* FilePath);
	void LoadMeshes();
	void UploadMesh(FMesh& Mesh);

	FFrameData& GetCurrentFrame();

	FAllocatedBuffer CreateBuffer(size_t AllocSize, VkBufferUsageFlags Usage, VmaMemoryUsage MemoryUsage);

	size_t PadUniformBufferSize(size_t OriginalSize);

	//shuts down the engine
	void Cleanup();

	//draw loop
	void Draw();

	//run main loop
	void Run();

	// Scene management
	std::vector<FRenderObject> _Renderables;
	std::unordered_map<std::string, FMaterial> _Materials;
	std::unordered_map<std::string, FMesh> _Meshes;

	FMaterial* CreateMaterial(VkPipeline Pipeline, VkPipelineLayout Layout, const std::string& Name);
	FMaterial* GetMaterial(const std::string& Name);
	FMesh* GetMesh(const std::string& Name);

	void InitScene();
	void DrawObjects(VkCommandBuffer Cmd, FRenderObject* First, int Count);
};

