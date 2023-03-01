#pragma once
#include <string>
#include <vulkan/vulkan_core.h>

struct VulkanDeviceContext;

class VulkanImage
{
public:
	void create(
		const VulkanDeviceContext& device_context,
		uint32_t width,
		uint32_t height,
		uint32_t depth,
		VkImageType type,
		VkFormat format,
		VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties);

	void destroy(VkDevice device);

	VkImage m_Image{};
	VkDeviceMemory m_Memory{};
};
