#include "VulkanImage.h"

#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanFunctions.h"

#include <stdexcept>

void VulkanImage::create(
	const VulkanDeviceContext& device_context,
	uint32_t width,
	uint32_t height,
	uint32_t depth,
	VkImageType type,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage, VkMemoryPropertyFlags properties)
{
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = type;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = depth;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Can be UNDEFINED or PREINITIALIZED
	imageCreateInfo.usage = usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	if (VK_SUCCESS != vkCreateImage(device_context.m_Device, &imageCreateInfo, nullptr, &m_Image))
	{
		throw std::runtime_error("Failed to create Image!");
	}

	VkMemoryRequirements memoryRequirements{};
	vkGetImageMemoryRequirements(device_context.m_Device, m_Image, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = vulkan::findMemoryType(device_context.m_PhysicalDevice, memoryRequirements.memoryTypeBits, properties);

	if (VK_SUCCESS != vkAllocateMemory(device_context.m_Device, &memoryAllocateInfo, nullptr, &m_Memory))
	{
		throw std::runtime_error("Failed to Allocate image memory!");
	}

	vkBindImageMemory(device_context.m_Device, m_Image, m_Memory, 0);
}

void VulkanImage::destroy(VkDevice device)
{
	vkDestroyImage(device, m_Image, nullptr);
	vkFreeMemory(device, m_Memory, nullptr);
}
