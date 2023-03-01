#include "VulkanBuffer.h"

#include <stdexcept>

#include "VulkanCommon.h"
#include "VulkanContext.h"
#include "VulkanFunctions.h"

namespace detail
{
}

void VulkanBuffer::create(
	const VulkanDeviceContext& device_context,
	VkDeviceSize size,
	VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usage;

	if (device_context.m_QueueFamilyIndices.transferFamily.has_value())
	{
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;

		uint32_t queueFamilyIndices[] = { device_context.m_QueueFamilyIndices.graphicsFamily.value(), device_context.m_QueueFamilyIndices.transferFamily.value() };
		bufferCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		bufferCreateInfo.queueFamilyIndexCount = std::size(queueFamilyIndices);
	}
	else
	{
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	if (VK_SUCCESS != vkCreateBuffer(device_context.m_Device, &bufferCreateInfo, nullptr, &m_Buffer))
	{
		throw std::runtime_error("Failed to create Vertex Buffer!");
	}

	// Allocate memory to the buffer
	VkMemoryRequirements memoryRequirements{};
	vkGetBufferMemoryRequirements(device_context.m_Device, m_Buffer, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo{};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = vulkan::findMemoryType(device_context.m_PhysicalDevice, memoryRequirements.memoryTypeBits, properties);

	if (VK_SUCCESS != vkAllocateMemory(device_context.m_Device, &memoryAllocateInfo, nullptr, &m_Memory))
	{
		throw std::runtime_error("Failed to allocate vertex buffer Memory!");
	}

	vkBindBufferMemory(device_context.m_Device, m_Buffer, m_Memory, 0);

}

void VulkanBuffer::destroy(VkDevice device)
{
	vkDestroyBuffer(device, m_Buffer, nullptr);
	vkFreeMemory(device, m_Memory, nullptr);
}
