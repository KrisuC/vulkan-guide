#pragma once

#include <vk_types.h>
#include <vk_engine.h>

namespace VkUtils
{

bool LoadImageFromFile(FVulkanEngine& Engine, const char* File, FAllocatedImage& OutImage);

}