#version 450

layout (location = 0) in vec3 InColor;

layout (location = 0) out vec4 OutFragColor;

layout (set = 0, binding = 1) uniform FSceneData
{
	vec4 FogColor;
	vec4 FogDistance;
	vec4 AmbientColor;
	vec4 SunlightDirection;
	vec4 SunlightColor;
} SceneData;

void main()
{
	OutFragColor = vec4(InColor + SceneData.AmbientColor.xyz, 1.0f);
}
