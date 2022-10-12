#include <vk_engine.h>

int main(int argc, char* argv[])
{
	FVulkanEngine Engine;

	Engine.Init();	
	
	Engine.Run();	

	Engine.Cleanup();	

	return 0;
}
