#include "vk_mesh.h"

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

	Description._Attributes.push_back(PositionAttribute);
	Description._Attributes.push_back(NormalAttribute);
	Description._Attributes.push_back(ColorAttribute);
	return Description;
}
