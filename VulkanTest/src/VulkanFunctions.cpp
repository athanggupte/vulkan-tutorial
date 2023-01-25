#include "VulkanFunctions.h"

#include <stdexcept>

#define VKTUT_VK_CREATE_DEBUG_UTILS_MESSENGER_NAME "vkCreateDebugUtilsMessengerEXT"
#define VKTUT_VK_DESTROY_DEBUG_UTILS_MESSENGER_NAME "vkDestroyDebugUtilsMessengerEXT"

VkResult vulkan::CreateDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, VKTUT_VK_CREATE_DEBUG_UTILS_MESSENGER_NAME);

	if (nullptr != func)
	{
		return func(instance, pCreateInfo, pAllocator, pMessenger);
	}

	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void vulkan::DestroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugUtilsMessenger,
	const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, VKTUT_VK_DESTROY_DEBUG_UTILS_MESSENGER_NAME);

	if (nullptr != func)
	{
		func(instance, debugUtilsMessenger, pAllocator);
	}
}

VkShaderModule vulkan::CreateShaderModule(VkDevice device, const std::vector<char> shader_code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = shader_code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(shader_code.data());

	VkShaderModule shaderModule;
	if (VK_SUCCESS != vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule))
	{
		throw std::runtime_error("Failed to create VkShaderModule!");
	}

	return shaderModule;
}
