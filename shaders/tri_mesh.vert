#version 450

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vColor;

layout (location = 0) out vec3 OutColor;

layout (push_constant) uniform FPushConstants
{
	vec4 Data;
	mat4 RenderMatrix;
} PushConstants;

void main()
{
	gl_Position = PushConstants.RenderMatrix * vec4(vPosition, 1.f);
	OutColor = PushConstants.Data.xyz;
}