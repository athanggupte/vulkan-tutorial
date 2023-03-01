#pragma once
#include <optional>
#include <vector>
#include <vulkan/vulkan_core.h>


struct VulkanQueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	std::optional<uint32_t> transferFamily;
	std::optional<uint32_t> computeFamily;

	bool isPartiallyComplete() const
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}

	bool isComplete() const
	{
		return isPartiallyComplete() && transferFamily.has_value();
	}
};

struct VulkanSwapchainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};
