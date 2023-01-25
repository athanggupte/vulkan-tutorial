#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

#include "VulkanCommon.h"

class VulkanSwapchain
{
public:
	void create(
		VkDevice device,
		VkSurfaceKHR surface,
		const VulkanSwapchainSupportDetails& swapchain_support_details,
		const VulkanQueueFamilyIndices& indices,
		uint32_t window_width, uint32_t window_height
	);

	void destroy(VkDevice device);

	VkSwapchainKHR m_Swapchain{};
	std::vector<VkImage> m_Images;
	VkFormat m_Format{};
	VkExtent2D m_Extent{};
};
