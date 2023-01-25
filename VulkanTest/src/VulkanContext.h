#pragma once

#include "VulkanSwapchain.h"

#define GLFW_INCLUDE_VULKAN
#include "VulkanCommon.h"
#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "GLFW/glfw3.h"

class VulkanContext
{
public:
	void initContext(const char* app_name, GLFWwindow* window, bool enable_debugging = true);
	void shutdownContext();

	void drawFrame();

private:
	void createInstance(const char* app_name);
	void setupDebugMessenger();
	void createSurface(GLFWwindow* window);
	void selectPhysicalDevice();
	void createLogicalDevice(const VulkanQueueFamilyIndices& indices);
	void createSwapchain(GLFWwindow* window, const VulkanQueueFamilyIndices& indices);
	void createImageViews();
	void createFramebuffers();
	void createCommandPool(const VulkanQueueFamilyIndices& indices);
	void createCommandBuffer();
	void createSyncObjects();

	void recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index);


	VkInstance m_Instance{};
	VkSurfaceKHR m_Surface{};

	VkPhysicalDevice m_PhysicalDevice{};
	VkDevice m_Device{};
	VkQueue m_GraphicsQueue{}, m_PresentQueue{};

	VulkanSwapchain m_Swapchain{};
	std::vector<VkImageView> m_SwapchainImageViews;

	VulkanRenderPass m_RenderPass{};
	std::vector<VkFramebuffer> m_SwapchainFramebuffers;

	VulkanPipeline m_GraphicsPipeline;

	VkCommandPool m_CommandPool{};

	const uint32_t MAX_FRAMES_IN_FLIGHT = 1;
	uint32_t m_CurrentFrame = 0;

	std::vector<VkCommandBuffer> m_CommandBuffers{MAX_FRAMES_IN_FLIGHT};

	std::vector<VkSemaphore> m_ImageAvailableSemaphores{MAX_FRAMES_IN_FLIGHT};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores{MAX_FRAMES_IN_FLIGHT};
	std::vector<VkFence> m_InFlightFences{MAX_FRAMES_IN_FLIGHT};

	VkDebugUtilsMessengerEXT m_DebugMessenger{};
};
