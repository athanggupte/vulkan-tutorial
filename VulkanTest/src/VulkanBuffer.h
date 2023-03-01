#pragma once
#include <vulkan/vulkan_core.h>

struct VulkanDeviceContext;

class VulkanBuffer
{
public:
	void create(
		const VulkanDeviceContext& device_context,
		VkDeviceSize size,
		VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties);

	void destroy(VkDevice device);

	VkBuffer m_Buffer{};
	VkDeviceMemory m_Memory{};
};
