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
		VkSurfaceFormatKHR surfaceFormat,
		VkPresentModeKHR presentMode,
		const VkExtent2D& extent,
		uint32_t imageCount,
		const VulkanQueueFamilyIndices& indices);
	void createImageViews(VkDevice device, VkFormat format);
	void createFramebuffers(VkDevice device, VkRenderPass render_pass, const VkExtent2D& extent);

	void destroy(VkDevice device);

	VkSwapchainKHR m_Swapchain{};
	std::vector<VkImage> m_Images;
	std::vector<VkImageView> m_ImageViews;
	std::vector<VkFramebuffer> m_Framebuffers;
};
