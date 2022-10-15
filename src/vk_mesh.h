#pragma once

#include "vk_types.h"
#include <vector>
#include <glm/vec3.hpp>

struct FVertexInputDescription
{
	std::vector<VkVertexInputBindingDescription> _Bindings;
	std::vector<VkVertexInputAttributeDescription> _Attributes;

	VkPipelineVertexInputStateCreateFlags _Flags = 0;
};

class FVertex
{
public:
	glm::vec3 _Position;
	glm::vec3 _Normal;
	glm::vec3 _Color;

	static FVertexInputDescription GetVertexDescription();
};

struct FMesh
{
	std::vector<FVertex> _Vertices;
	FAllocatedBuffer _VertexBuffer;

	bool LoadFromObj(const char* FileName);
};

