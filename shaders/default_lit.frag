#version 450

layout (location = 0) in vec3 InColor;

layout (location = 0) out vec4 OutFragColor;

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

void main()
{
	OutFragColor = vec4(InColor + GlobalBuffer.AmbientColor.xyz, 1.0f);
}
