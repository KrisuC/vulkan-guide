#version 450

#define RED_TRIANGLE 1

layout (location = 0) out vec3 OutColor;

void main()
{
	const vec3 Positions[3] = vec3[3](
		vec3(1.f, 1.f, 0.f),
		vec3(-1.f, 1.f, 0.f),
		vec3(0.f, -1.f, 0.f)
	);

	const vec3 Colors[3] = vec3[3](
		vec3(1.f, 0.f, 0.f),
		vec3(0.f, 1.f, 0.f),
		vec3(0.f, 0.f, 1.f)
	);

	gl_Position = vec4(Positions[gl_VertexIndex], 1.f);
#if RED_TRIANGLE
	OutColor = vec3(1.f, 0.f, 0.f);
#else
	OutColor = Colors[gl_VertexIndex];
#endif
}
