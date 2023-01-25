#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

class VulkanRenderPass
{
public:
	void create(VkDevice device, VkFormat swapchain_image_format);
	void destroy(VkDevice device);

	VkRenderPass m_RenderPass{};
};
