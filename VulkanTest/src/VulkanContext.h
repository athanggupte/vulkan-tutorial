#pragma once

#include "VulkanSwapchain.h"

#define GLFW_INCLUDE_VULKAN
#include "VulkanBuffer.h"
#include "VulkanCommon.h"
#include "VulkanImage.h"
#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "GLFW/glfw3.h"

class Application;

struct VulkanDeviceContext
{
	VkPhysicalDevice m_PhysicalDevice{};
	VkDevice m_Device{};
	VulkanQueueFamilyIndices m_QueueFamilyIndices{};
	VkQueue m_GraphicsQueue{}, m_PresentQueue{}, m_TransferQueue{};

	VkPhysicalDeviceProperties m_PhysicalDeviceProperties{};
	VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures{};
	std::vector<VkExtensionProperties> m_AvailableExtensions;

	void retrieveDeviceContext();
};

class VulkanContext
{
public:
	void initContext(const char* app_name, GLFWwindow* window, bool enable_debugging = true);
	void shutdownContext();

	void drawFrame();
	void handleFramebufferResized(int width, int height);

private:
	void createInstance(const char* app_name);
	void setupDebugMessenger();
	void createSurface(GLFWwindow* window);
	void selectPhysicalDevice();
	void createLogicalDevice();
	void createCommandPool();
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffers();
	void createDescriptorPool();
	void createDescriptorSets();
	void createCommandBuffer();
	void createSyncObjects();

	void createTextureImage(const std::string& texture_file);
	void createTextureImageView();
	void createTextureSampler();

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout);
	void copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);
	void copyBufferToImage(VkBuffer src_buffer, VkImage dst_image, uint32_t width, uint32_t height);

	void updateUniformBuffers(uint32_t current_image);
	void recreateSwapchain();

	void recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index);

	const uint32_t MAX_FRAMES_IN_FLIGHT = 1;

	GLFWwindow *m_Window{};

	VkInstance m_Instance{};
	VkSurfaceKHR m_Surface{};

	VulkanDeviceContext m_DeviceContext{};

	// Swap chain objects
	VulkanSwapchain m_Swapchain{};
	VkSurfaceFormatKHR m_SwapchainImageFormat{};
	VkExtent2D m_SwapchainImageExtent{};

	// Render pass
	VulkanRenderPass m_RenderPass{};

	// Pipelines
	VulkanPipeline m_GraphicsPipeline{};

	// Command generation objects
	VkCommandPool m_CommandPool{};
	std::vector<VkCommandBuffer> m_CommandBuffers{MAX_FRAMES_IN_FLIGHT};

	// Synchronization objects
	std::vector<VkSemaphore> m_ImageAvailableSemaphores{MAX_FRAMES_IN_FLIGHT};
	std::vector<VkSemaphore> m_RenderFinishedSemaphores{MAX_FRAMES_IN_FLIGHT};
	std::vector<VkFence> m_InFlightFences{MAX_FRAMES_IN_FLIGHT};

	// Rendering objects
	VulkanBuffer m_VertexBuffer{};
	VulkanBuffer m_IndexBuffer{};

	// Texturing objects
	VulkanImage m_TextureImage{};
	VkImageView m_TextureImageView{};
	VkSampler m_TextureSampler{};

	// Descriptor objects
	VkDescriptorPool m_DescriptorPool{};
	std::vector<VkDescriptorSet> m_DescriptorSets{MAX_FRAMES_IN_FLIGHT};

	std::vector<VulkanBuffer> m_UniformBuffers{MAX_FRAMES_IN_FLIGHT};
	std::vector<void*> m_UniformBufferMapped{MAX_FRAMES_IN_FLIGHT};

	VkDebugUtilsMessengerEXT m_DebugMessenger{};

	bool m_IsFramebufferResized = false;
	uint32_t m_CurrentFrame = 0;
};
