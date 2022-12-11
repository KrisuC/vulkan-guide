#version 450

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;

layout (location = 0) out vec3 OutColor;

// Descriptor set 0, and slot 0 of set 0
layout (set = 0, binding = 0) uniform FCameraBuffer
{
	mat4 View;
	mat4 Proj;
	mat4 ViewProj;
} CameraData;

layout (push_constant) uniform FPushConstants
{
	vec4 Data;
	mat4 RenderMatrix;
} PushConstants;

void main()
{
	mat4 TransformMatrix = CameraData.ViewProj * PushConstants.RenderMatrix;
	gl_Position = TransformMatrix * vec4(vPosition, 1.f);
	OutColor = vNormal;
}