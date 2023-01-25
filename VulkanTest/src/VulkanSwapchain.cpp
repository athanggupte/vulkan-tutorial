#include "VulkanSwapchain.h"

#include <stdexcept>
#include <vector>
#include <algorithm>

#include "VulkanCommon.h"

namespace detail
{
	static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
	static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);
	static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t window_width, uint32_t window_height);

}

void VulkanSwapchain::create(
	VkDevice device,
	VkSurfaceKHR surface,
	const VulkanSwapchainSupportDetails& swapchain_support_details,
	const VulkanQueueFamilyIndices& indices,
	uint32_t window_width, uint32_t window_height)
{
	VkSurfaceFormatKHR surfaceFormat = detail::chooseSwapSurfaceFormat(swapchain_support_details.formats);
	VkPresentModeKHR presentMode = detail::chooseSwapPresentMode(swapchain_support_details.presentModes);
	VkExtent2D extent = detail::chooseSwapExtent(swapchain_support_details.capabilities, window_width, window_height);

	uint32_t imageCount = swapchain_support_details.capabilities.minImageCount + 1;
	if (swapchain_support_details.capabilities.maxImageCount > 0 && imageCount > swapchain_support_details.capabilities.maxImageCount)
	{
		imageCount--;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	// currentTransform : no transform
	createInfo.preTransform = swapchain_support_details.capabilities.currentTransform;

	// OPAQUE : ignore alpha
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	// Images options
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;


	// TODO: Consider changing to VK_IMAGE_USAGE_TRANSFER_DST_BIT in case of blitting from postprocessing framebuffer image
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;     // optional
		createInfo.pQueueFamilyIndices = nullptr; // optional
	}

	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (VK_SUCCESS != vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_Swapchain))
	{
		throw std::runtime_error("Failed to create Swapchain!");
	}

	// Save the swapchain images for reference during rendering
	vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr);
	m_Images.resize(imageCount);
	vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, m_Images.data());

	// Save the extent and format for reference later
	m_Format = surfaceFormat.format;
	m_Extent = extent;
}

void VulkanSwapchain::destroy(VkDevice device)
{
	vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
}

VkSurfaceFormatKHR detail::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats)
{
	// Check if the preferred surface format is available
	constexpr static VkSurfaceFormatKHR PREFERED_FORMAT { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	for (const VkSurfaceFormatKHR& format : available_formats)
	{
		if (PREFERED_FORMAT.format == format.format
			&& PREFERED_FORMAT.colorSpace == format.colorSpace)
		{
			return format;
		}
	}

	// TODO: rank format based on suitability and select the highest ranked one
	// Otherwise select thef first available format
	return available_formats[0];
}

VkPresentModeKHR detail::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes)
{
	// Check if the MAILBOX (triple buffering) format is available
	for (const VkPresentModeKHR& presentMode : available_present_modes)
	{
		if (VK_PRESENT_MODE_MAILBOX_KHR == presentMode)
		{
			return presentMode;
		}
	}

	// Otherwise select the base FIFO format (guaranteed to be available by the specification)
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D detail::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t window_width, uint32_t window_height)
{
	// Check if the extent is set by Vulkan
	if (std::numeric_limits<uint32_t>::max() != capabilities.currentExtent.width)
	{
		return capabilities.currentExtent;
	}

	// Otherwise find the best set of extents to match the window width and height in pixels
	VkExtent2D actualExtent { window_width, window_height };

	actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return actualExtent;
}