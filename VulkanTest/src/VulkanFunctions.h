#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vulkan
{
	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	    const VkAllocationCallbacks* pAllocator,
	    VkDebugUtilsMessengerEXT* pMessenger
	);

	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugUtilsMessenger,
		const VkAllocationCallbacks* pAllocator
	);

	VkShaderModule CreateShaderModule(
		VkDevice device,
		const std::vector<char> shader_code
	);
}
