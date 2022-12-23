#include "vk_mesh.h"
#include <tiny_obj_loader.h>
#include <iostream>
#include <fmt/core.h>

FVertexInputDescription FVertex::GetVertexDescription()
{
	FVertexInputDescription Description;

	VkVertexInputBindingDescription MainBinding{};
	MainBinding.binding = 0;
	MainBinding.stride = sizeof(FVertex);
	MainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	Description._Bindings.push_back(MainBinding);

	// Position - Location 0
	VkVertexInputAttributeDescription PositionAttribute{};
	PositionAttribute.binding = 0;
	PositionAttribute.location = 0;
	PositionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	PositionAttribute.offset = offsetof(FVertex, _Position);

	// Normal - Location 1
	VkVertexInputAttributeDescription NormalAttribute{};
	NormalAttribute.binding = 0;
	NormalAttribute.location = 1;
	NormalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	NormalAttribute.offset = offsetof(FVertex, _Normal);
	
	// Color - Location 2
	VkVertexInputAttributeDescription ColorAttribute{};
	ColorAttribute.binding = 0;
	ColorAttribute.location = 2;
	ColorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	ColorAttribute.offset = offsetof(FVertex, _Color);

	// UV - Location 3
	VkVertexInputAttributeDescription UvAttribute{};
	UvAttribute.binding = 0;
	UvAttribute.location = 3;
	UvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	UvAttribute.offset = offsetof(FVertex, _UV);

	Description._Attributes.push_back(PositionAttribute);
	Description._Attributes.push_back(NormalAttribute);
	Description._Attributes.push_back(ColorAttribute);
	Description._Attributes.push_back(UvAttribute);
	return Description;
}

bool FMesh::LoadFromObj(const char* FileName)
{
	tinyobj::attrib_t Attrib;
	std::vector<tinyobj::shape_t> Shapes;
	std::vector<tinyobj::material_t> Materials;

	std::string Warn;
	std::string Err;

	tinyobj::LoadObj(&Attrib, &Shapes, &Materials, &Warn, &Err, FileName, nullptr);
	if (!Warn.empty())
	{
		std::cerr << "Warning: " << Warn << std::endl;
	}

	if (!Err.empty())
	{
		std::cerr << "Error: " << Err << std::endl;
		return false;
	}

	for (size_t ShapeIndex = 0; ShapeIndex < Shapes.size(); ShapeIndex++)
	{
		size_t IndexOffset = 0;
		// Iterating faces (num_face_vertices[] is number of vertices of every face)
		for (size_t FaceIndex = 0; FaceIndex < Shapes[ShapeIndex].mesh.num_face_vertices.size(); FaceIndex++)
		{
			const int FaceVertices = 3; // Hardcode to triangle for now
			for (size_t VertexIndex = 0; VertexIndex < FaceVertices; VertexIndex++)
			{
				// Loading vertex
				tinyobj::index_t Index = Shapes[ShapeIndex].mesh.indices[IndexOffset + VertexIndex];

				auto LoadFloat3 = [](tinyobj::real_t* RealPtr) -> glm::vec3 { return glm::vec3(RealPtr[0], RealPtr[1], RealPtr[2]); };
				auto LoadFloat2 = [](tinyobj::real_t* RealPtr) -> glm::vec2 { return glm::vec2(RealPtr[0], RealPtr[1]); };

				FVertex NewVertex;
				NewVertex._Position = LoadFloat3(&Attrib.vertices[3 * Index.vertex_index]);
				NewVertex._Normal = LoadFloat3(&Attrib.normals[3 * Index.normal_index]);
				NewVertex._Color = NewVertex._Normal;
				NewVertex._UV = LoadFloat2(&Attrib.texcoords[2 * Index.texcoord_index]);
				NewVertex._UV.y = 1.f - NewVertex._UV.y; // This is how vulkan uv work (from bottom to top?)

				_Vertices.push_back(NewVertex);
				// fmt::print("vertex: {} {} {}\n", NewVertex._Position.x, NewVertex._Position.y, NewVertex._Position.z);
			}
			IndexOffset += FaceVertices;
		}
	}

	return true;
}

