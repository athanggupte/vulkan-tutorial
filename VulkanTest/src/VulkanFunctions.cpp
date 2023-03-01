#include "VulkanFunctions.h"

#include <stdexcept>

#define VKTUT_VK_CREATE_DEBUG_UTILS_MESSENGER_NAME "vkCreateDebugUtilsMessengerEXT"
#define VKTUT_VK_DESTROY_DEBUG_UTILS_MESSENGER_NAME "vkDestroyDebugUtilsMessengerEXT"

VkResult vulkan::createDebugUtilsMessengerEXT(
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

void vulkan::destroyDebugUtilsMessengerEXT(
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

VkShaderModule vulkan::createShaderModule(VkDevice device, const std::vector<char> shader_code)
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

VkImageView vulkan::createImageView(
	VkDevice device,
	VkImage image,
	VkFormat format,
	uint32_t mip_level_count,
	uint32_t array_layer_count)
{
	VkImageViewCreateInfo imageViewCreateInfo{};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = mip_level_count;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = array_layer_count;

	VkImageView imageView{};
	if (VK_SUCCESS != vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageView))
	{
		throw std::runtime_error("Failed to create texture Image View!");
	}

	return imageView;
}

uint32_t vulkan::findMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memoryProperties{};
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if (type_filter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable Memory Type!");
}

VkCommandBuffer vulkan::beginOneShotCommands(VkDevice device, VkCommandPool command_pool)
{
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = command_pool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer{};
	
	if (VK_SUCCESS != vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer))
	{
		throw std::runtime_error("Failed to allocate Command Buffers!");
	}

	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

	return commandBuffer;
}

void vulkan::endOneShotCommands(VkDevice device, VkCommandBuffer command_buffer, VkCommandPool command_pool, VkQueue queue)
{
	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command_buffer;
	
	vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);

	// Here, we're synchronously waiting on the queue to go idle
	// 1. Use Fences to fire multiple commands at once and wait for them before starting submitting
	// 2. Use Semaphores to schedule the rendering commands on the GPU to start after the transfer commands are completed - no CPU side waiting
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

void vulkan::findImageLayoutTransitionAccessMasksAndStages(
	VkImageLayout old_layout, VkImageLayout new_layout,
	VkAccessFlags& src_access_mask, VkAccessFlags& dst_access_mask,
	VkPipelineStageFlags& src_stage, VkPipelineStageFlags& dst_stage)
{
	if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		// Undefined -> Transfer destination : transfer writes, don't need to wait on anything
		src_access_mask = 0;
		dst_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;

		src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		// Transfer destination -> Shader read : shader reads wait on transfer writes (in fragment shader)
		src_access_mask = VK_ACCESS_TRANSFER_WRITE_BIT;
		dst_access_mask = VK_ACCESS_SHADER_READ_BIT;

		src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		throw std::runtime_error("Unsupported layout transition!");
	}
}
