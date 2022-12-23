#version 460

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;
layout (location = 3) in vec2 vTexCoord;

layout (location = 0) out vec3 OutColor;
layout (location = 1) out vec2 OutTexCoord;

// Descriptor set 0, and slot 0 of set 0
layout (set = 0, binding = 0) uniform FGlobalBuffer
{
	mat4 View;
	mat4 Proj;
	mat4 ViewProj;
		
	vec4 FogColor;
	vec4 FogDistances;
	vec4 AmbientColor;
	vec4 SunlightDirection;
	vec4 SunlightColor;
} GlobalBuffer;

struct FObjectData
{
	mat4 Model;
	vec4 Color;
};

layout (std140, set = 1, binding = 0) readonly buffer FObjectBuffer
{
	FObjectData Objects[];	
} ObjectBuffer;

void main()
{
	// Ugly hacking to using gl_BaseInstance to pass Primitive ID
	uint PrimitiveID = gl_BaseInstance;
	mat4 ModelMatrix = ObjectBuffer.Objects[PrimitiveID].Model;
	mat4 TransformMatrix = GlobalBuffer.ViewProj * ModelMatrix;
	gl_Position = TransformMatrix * vec4(vPosition, 1.f);
	OutColor = ObjectBuffer.Objects[PrimitiveID].Color.xyz;
	OutTexCoord = vTexCoord;
}