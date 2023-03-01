#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vulkan
{
	VkResult createDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	    const VkAllocationCallbacks* pAllocator,
	    VkDebugUtilsMessengerEXT* pMessenger
	);

	void destroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugUtilsMessenger,
		const VkAllocationCallbacks* pAllocator
	);

	VkShaderModule createShaderModule(
		VkDevice device,
		const std::vector<char> shader_code
	);

	VkImageView createImageView(
		VkDevice device,
		VkImage image,
		VkFormat format,
		uint32_t mip_level_count,
		uint32_t array_layer_count
	);

	uint32_t findMemoryType(
		VkPhysicalDevice physical_device,
		uint32_t type_filter,
		VkMemoryPropertyFlags properties
	);

	VkCommandBuffer beginOneShotCommands(
		VkDevice device,
		VkCommandPool command_pool
	);

	void endOneShotCommands(
		VkDevice device,
		VkCommandBuffer command_buffer,
		VkCommandPool command_pool,
		VkQueue queue
	);

	void findImageLayoutTransitionAccessMasksAndStages(
		VkImageLayout old_layout,
		VkImageLayout new_layout,
		VkAccessFlags& src_access_mask,
		VkAccessFlags& dst_access_mask,
		VkPipelineStageFlags& src_stage,
		VkPipelineStageFlags& dst_stage
	);
}
