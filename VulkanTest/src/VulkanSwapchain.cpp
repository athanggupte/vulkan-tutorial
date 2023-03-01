#include "VulkanSwapchain.h"

#include <stdexcept>
#include <vector>

#include "VulkanCommon.h"
#include "VulkanFunctions.h"

void VulkanSwapchain::create(
	VkDevice device,
	VkSurfaceKHR surface,
	const VulkanSwapchainSupportDetails& swapchain_support_details,
	VkSurfaceFormatKHR surfaceFormat,
	VkPresentModeKHR presentMode,
	const VkExtent2D& extent,
	uint32_t imageCount,
	const VulkanQueueFamilyIndices& indices)
{
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
}

void VulkanSwapchain::createImageViews(VkDevice device, VkFormat format)
{
	m_ImageViews.resize(m_Images.size());

	for (int i = 0; i < m_Images.size(); i++)
	{
		m_ImageViews[i] = vulkan::createImageView(device, m_Images[i], format, 1, 1);
	}
}

void VulkanSwapchain::createFramebuffers(VkDevice device, VkRenderPass render_pass, const VkExtent2D& extent)
{
	m_Framebuffers.resize(m_ImageViews.size());
	for (size_t i = 0; i < m_ImageViews.size(); i++)
	{
		VkImageView attachments[] = {
			m_ImageViews[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = render_pass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = attachments;
		framebufferCreateInfo.width = extent.width;
		framebufferCreateInfo.height = extent.height;
		framebufferCreateInfo.layers = 1;

		if (VK_SUCCESS != vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &m_Framebuffers[i]))
		{
			throw std::runtime_error("Failed to create Framebuffer!");
		}
	}
}

void VulkanSwapchain::destroy(VkDevice device)
{
	for (VkFramebuffer framebuffer : m_Framebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}

	for (VkImageView imageView : m_ImageViews)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
}