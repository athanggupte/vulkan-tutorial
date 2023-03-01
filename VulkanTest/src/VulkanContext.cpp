#include "VulkanContext.h"

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <optional>
#include <set>
#include <chrono>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "BufferData.h"
#include "stb_image.h"
#include "VulkanCommon.h"
#include "VulkanFunctions.h"

namespace detail
{
	static bool check_validation_layer_support();
	static void populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	static std::vector<const char*> get_required_instance_extensions();
	static bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface);
	static VulkanQueueFamilyIndices find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface);
	static bool check_device_extension_support(VkPhysicalDevice device);
	static VulkanSwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface);

	static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats);
	static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes);
	static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t window_width, uint32_t window_height);

	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
	    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	    VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
	    void*                                            pUserData
	);

	namespace
	{
	#ifdef VKTUT_VK_ENABLE_VALIDATION
		bool s_EnableDebugLayers = true;
	#else
		const bool s_EnableDebugLayers = false;
	#endif

		const std::vector<const char*> s_ValidationLayers {
			"VK_LAYER_KHRONOS_validation",
		};

		// TODO: Allow specification of required extensions through init file/API
		const std::vector<const char*> s_InstanceExtensions {};
		const std::vector<const char*> s_DeviceExtensions {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		};

	}
}


void VulkanDeviceContext::retrieveDeviceContext()
{
	vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_PhysicalDeviceProperties);
	vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_PhysicalDeviceFeatures);

	uint32_t extensionCount{};
	vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);

	m_AvailableExtensions.resize(extensionCount);
	vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, m_AvailableExtensions.data());
}

void VulkanContext::initContext(const char* app_name, GLFWwindow* window, bool enable_debugging)
{
	m_Window = window;

#ifdef VKTUT_VK_ENABLE_VALIDATION
	detail::s_EnableDebugLayers = enable_debugging;
#endif

	// Create a Vulkan instance
	// (instance is the link between application and Vulkan API)
	createInstance(app_name);

	// Add debug messenger for handling debug callbacks
	// TODO: extend the API to allow passing parameters to govern the type and severity of messages
	setupDebugMessenger();

	// Create a window surface for presenting on screen
	// (can be skipped if rendering off-screen or in headless mode)
	createSurface(window);

	// Select a Vulkan supported GPU for use
	// (can use multiple physical devices as well)
	selectPhysicalDevice();

	m_DeviceContext.m_QueueFamilyIndices = detail::find_queue_families(m_DeviceContext.m_PhysicalDevice, m_Surface);

	// Create a logical device for the selected GPU
	// (can create multiple logical devices for same physical device with different extensions and features)
	createLogicalDevice();

	// createSwapchain(window, indices);
	VulkanSwapchainSupportDetails swapchainSupportDetails = detail::query_swapchain_support(m_DeviceContext.m_PhysicalDevice, m_Surface);

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	VkSurfaceFormatKHR surfaceFormat = detail::chooseSwapSurfaceFormat(swapchainSupportDetails.formats);
	VkPresentModeKHR presentMode = detail::chooseSwapPresentMode(swapchainSupportDetails.presentModes);
	VkExtent2D extent = detail::chooseSwapExtent(swapchainSupportDetails.capabilities, width, height);

	m_RenderPass.create(m_DeviceContext.m_Device, surfaceFormat.format);

	uint32_t imageCount = swapchainSupportDetails.capabilities.minImageCount + 1;
	if (swapchainSupportDetails.capabilities.maxImageCount > 0 && imageCount > swapchainSupportDetails.capabilities.maxImageCount)
	{
		imageCount--;
	}

	// Save the extent and format
	m_SwapchainImageFormat = surfaceFormat;
	m_SwapchainImageExtent = extent;

	m_Swapchain.create(
		m_DeviceContext.m_Device,
		m_Surface,
		swapchainSupportDetails,
		surfaceFormat,
		presentMode,
		extent,
		imageCount,
		m_DeviceContext.m_QueueFamilyIndices
	);

	m_Swapchain.createImageViews(m_DeviceContext.m_Device, m_SwapchainImageFormat.format);
	m_Swapchain.createFramebuffers(m_DeviceContext.m_Device, m_RenderPass.m_RenderPass, m_SwapchainImageExtent);

	m_GraphicsPipeline.create(m_DeviceContext.m_Device,
							  "..\\build\\bin\\Debug-x86_64\\VulkanTest\\mesh_shader.vert.spv",
	                          "..\\build\\bin\\Debug-x86_64\\VulkanTest\\simple_shader.frag.spv",
							  m_SwapchainImageExtent,
	                          m_RenderPass.m_RenderPass);

	createCommandPool();
	createCommandBuffer();
	createSyncObjects();

	createVertexBuffer();
	createIndexBuffer();

	createTextureImage("assets\\pusheen-thug-life.png");
	createTextureImageView();
	createTextureSampler();

	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
}

void VulkanContext::shutdownContext()
{
	vkDeviceWaitIdle(m_DeviceContext.m_Device);

	m_Swapchain.destroy(m_DeviceContext.m_Device);

	vkDestroySampler(m_DeviceContext.m_Device, m_TextureSampler, nullptr);
	vkDestroyImageView(m_DeviceContext.m_Device, m_TextureImageView, nullptr);
	m_TextureImage.destroy(m_DeviceContext.m_Device);

	m_IndexBuffer.destroy(m_DeviceContext.m_Device);
	m_VertexBuffer.destroy(m_DeviceContext.m_Device);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_UniformBuffers[i].destroy(m_DeviceContext.m_Device);

		vkDestroyDescriptorPool(m_DeviceContext.m_Device, m_DescriptorPool, nullptr);

		vkDestroySemaphore(m_DeviceContext.m_Device, m_ImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_DeviceContext.m_Device, m_RenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_DeviceContext.m_Device, m_InFlightFences[i], nullptr);
	}

	vkFreeCommandBuffers(m_DeviceContext.m_Device, m_CommandPool, m_CommandBuffers.size(), m_CommandBuffers.data());

	vkDestroyCommandPool(m_DeviceContext.m_Device, m_CommandPool, nullptr);

	m_GraphicsPipeline.destroy(m_DeviceContext.m_Device);

	m_RenderPass.destroy(m_DeviceContext.m_Device);


	vkDestroyDevice(m_DeviceContext.m_Device, nullptr);
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

	if (detail::s_EnableDebugLayers)
	{
		vulkan::destroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
	}

	vkDestroyInstance(m_Instance, nullptr);
}

void VulkanContext::createInstance(const char* app_name)
{
	// TODO: Extract application and engine versioning info from environment/macros
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = app_name;
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.pEngineName = "No engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Check if validation layers are available
	if (detail::s_EnableDebugLayers && !detail::check_validation_layer_support())
	{
		throw std::runtime_error("Validation layers requested, but not available!");
	}

	// If debugging is enabled, then enable required validation layers and debug messenger
	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
	if (detail::s_EnableDebugLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(detail::s_ValidationLayers.size());
		createInfo.ppEnabledLayerNames = detail::s_ValidationLayers.data();

		detail::populate_debug_messenger_create_info(debugMessengerCreateInfo);
		createInfo.pNext = &debugMessengerCreateInfo;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
	}
	
	const std::vector<const char*> extensions = detail::get_required_instance_extensions();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (VK_SUCCESS != vkCreateInstance(&createInfo, nullptr, &m_Instance))
	{
		throw std::runtime_error("Failed to create Vulkan Instance!");
	}
}

void VulkanContext::setupDebugMessenger()
{
	if (!detail::s_EnableDebugLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	detail::populate_debug_messenger_create_info(createInfo);

	if (VK_SUCCESS != vulkan::createDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger))
	{
		throw std::runtime_error("Failed to set up Debug Messenger!");
	}
}

void VulkanContext::createSurface(GLFWwindow* window)
{
	if (VK_SUCCESS != glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface))
	{
		throw std::runtime_error("Failed to create window Surface!");
	}
}

void VulkanContext::selectPhysicalDevice()
{
	// Find all the available physical devices with Vulkan support
	uint32_t deviceCount{};
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

	if (0 == deviceCount)
	{
		throw std::runtime_error("Failed to find GPU with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

	// Select the first device suited to our needs
	// (can be modified to allow selection of device or assign priorities to devices based on available features and extensions)
	for (const VkPhysicalDevice& device : devices)
	{
		if (detail::is_device_suitable(device, m_Surface))
		{
			m_DeviceContext.m_PhysicalDevice = device;
			break;
		}
	}

	if (VK_NULL_HANDLE == m_DeviceContext.m_PhysicalDevice)
	{
		throw std::runtime_error("Failed to find a suitable GPU!");
	}

	m_DeviceContext.retrieveDeviceContext();
}

void VulkanContext::createLogicalDevice()
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	// Select unique queue families for the required operations - Graphics, Presentation, ...
	std::set<uint32_t> uniqueQueueFamilies { m_DeviceContext.m_QueueFamilyIndices.graphicsFamily.value(), m_DeviceContext.m_QueueFamilyIndices.presentFamily.value() };
	if (m_DeviceContext.m_QueueFamilyIndices.transferFamily.has_value())
	{
		uniqueQueueFamilies.emplace(m_DeviceContext.m_QueueFamilyIndices.transferFamily.value());
	}

	// Create a queue for each of the required queue families
	float queuePriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	// TODO: Replicate the required physical device features required
	VkPhysicalDeviceFeatures deviceFeatures{};
	deviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.pEnabledFeatures = &deviceFeatures;

	// Enable the required device extensions
	// (the availability of these extensions was already checked while selecting the physical device)
	createInfo.enabledExtensionCount = static_cast<uint32_t>(detail::s_DeviceExtensions.size());
	createInfo.ppEnabledExtensionNames = detail::s_DeviceExtensions.data();

	if (detail::s_EnableDebugLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(detail::s_ValidationLayers.size());
		createInfo.ppEnabledLayerNames = detail::s_ValidationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	if (VK_SUCCESS != vkCreateDevice(m_DeviceContext.m_PhysicalDevice, &createInfo, nullptr, &m_DeviceContext.m_Device))
	{
		throw std::runtime_error("Failed to create Logical Device!");
	}

	// Get handle to the queues
	vkGetDeviceQueue(m_DeviceContext.m_Device, m_DeviceContext.m_QueueFamilyIndices.graphicsFamily.value(), 0, &m_DeviceContext.m_GraphicsQueue);
	vkGetDeviceQueue(m_DeviceContext.m_Device, m_DeviceContext.m_QueueFamilyIndices.presentFamily.value(), 0, &m_DeviceContext.m_PresentQueue);
	if (m_DeviceContext.m_QueueFamilyIndices.transferFamily.has_value())
	{
		vkGetDeviceQueue(m_DeviceContext.m_Device, m_DeviceContext.m_QueueFamilyIndices.transferFamily.value(), 0, &m_DeviceContext.m_TransferQueue);
	}
	else
	{
		printf("[WARN] Failed to find dedicated Transfer Queue Family!\n");
	}
}


void VulkanContext::createCommandPool()
{
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = m_DeviceContext.m_QueueFamilyIndices.graphicsFamily.value();

	if (VK_SUCCESS != vkCreateCommandPool(m_DeviceContext.m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool))
	{
		throw std::runtime_error("Failed to create Command Pool!");
	}
}

void VulkanContext::createVertexBuffer()
{
	size_t size = Mesh::getNumVertices() * sizeof(Vertex);

	// Create a temporary staging buffer
	VulkanBuffer stagingBuffer;
	stagingBuffer.create(m_DeviceContext,
	                     size,
	                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Fill the buffer
	void* data;
	vkMapMemory(m_DeviceContext.m_Device, stagingBuffer.m_Memory, 0, size, 0, &data);
	std::memcpy(data, Mesh::getVertices(), size);
	vkUnmapMemory(m_DeviceContext.m_Device, stagingBuffer.m_Memory);

	// Create the vertex buffer
	m_VertexBuffer.create(m_DeviceContext,
	                      size,
	                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Copy vertex data from staging buffer to index buffer
	copyBuffer(stagingBuffer.m_Buffer, m_VertexBuffer.m_Buffer, size);

	// Cleanup staging buffer
	stagingBuffer.destroy(m_DeviceContext.m_Device);
}

void VulkanContext::createIndexBuffer()
{
	size_t size = Mesh::getNumIndices() * sizeof(uint16_t);

	// Create a temporary staging buffers
	VulkanBuffer stagingBuffer;
	stagingBuffer.create(m_DeviceContext,
	                     size,
	                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Fill the buffer
	void* data;
	vkMapMemory(m_DeviceContext.m_Device, stagingBuffer.m_Memory, 0, size, 0, &data);
	std::memcpy(data, Mesh::getIndices(), size);
	vkUnmapMemory(m_DeviceContext.m_Device, stagingBuffer.m_Memory);

	// Create the index buffer
	m_IndexBuffer.create(m_DeviceContext,
	                     size,
	                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Copy vertex data from staging buffer to index buffer
	copyBuffer(stagingBuffer.m_Buffer, m_IndexBuffer.m_Buffer, size);

	// Cleanup staging buffer
	stagingBuffer.destroy(m_DeviceContext.m_Device);
}

void VulkanContext::createUniformBuffers()
{
	VkDeviceSize size = sizeof(MatricesUBO);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_UniformBuffers[i].create(m_DeviceContext, size,
		                           VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(m_DeviceContext.m_Device, m_UniformBuffers[i].m_Memory, 0, size, 0, &m_UniformBufferMapped[i]);
		assert(m_UniformBufferMapped[i] != nullptr);
	}
}

void VulkanContext::createDescriptorPool()
{
	VkDescriptorPoolSize descriptorPoolSizes[2];
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorPoolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.poolSizeCount = std::size(descriptorPoolSizes);
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;
	descriptorPoolCreateInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

	if (VK_SUCCESS != vkCreateDescriptorPool(m_DeviceContext.m_Device, &descriptorPoolCreateInfo, nullptr, &m_DescriptorPool))
	{
		throw std::runtime_error("Failed to create Descriptor Pool!");
	}
}

void VulkanContext::createDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(MAX_FRAMES_IN_FLIGHT, m_GraphicsPipeline.m_DescriptorSetLayout);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = m_DescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

	if (VK_SUCCESS != vkAllocateDescriptorSets(m_DeviceContext.m_Device, &descriptorSetAllocateInfo, m_DescriptorSets.data()))
	{
		throw std::runtime_error("Failed to allocate Descriptor Sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo descriptorBufferInfo{};
		descriptorBufferInfo.buffer = m_UniformBuffers[i].m_Buffer;
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(MatricesUBO); // VK_WHOLE_SIZE

		VkDescriptorImageInfo descriptorImageInfo{};
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorImageInfo.imageView = m_TextureImageView;
		descriptorImageInfo.sampler = m_TextureSampler;

		VkWriteDescriptorSet descriptorWrites[2]{};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = m_DescriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &descriptorBufferInfo; // used to read buffer data
		descriptorWrites[0].pImageInfo = nullptr; // (optional) used to read image data
		descriptorWrites[0].pTexelBufferView = nullptr; // (optional) used to read buffer views

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = m_DescriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &descriptorImageInfo; // (optional) used to read image data

		vkUpdateDescriptorSets(m_DeviceContext.m_Device, std::size(descriptorWrites), descriptorWrites, 0, nullptr);
	}
}

void VulkanContext::createCommandBuffer()
{
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = m_CommandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

	
	if (VK_SUCCESS != vkAllocateCommandBuffers(m_DeviceContext.m_Device, &allocateInfo, m_CommandBuffers.data()))
	{
		throw std::runtime_error("Failed to allocate Command Buffers!");
	}
}

void VulkanContext::createSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Creates the fence in the signalled state which prevents blocking indefinitely on the first frame

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (VK_SUCCESS != vkCreateSemaphore(m_DeviceContext.m_Device, &semaphoreCreateInfo, nullptr, &m_ImageAvailableSemaphores[i]) ||
			VK_SUCCESS != vkCreateSemaphore(m_DeviceContext.m_Device, &semaphoreCreateInfo, nullptr, &m_RenderFinishedSemaphores[i]) ||
			VK_SUCCESS != vkCreateFence(m_DeviceContext.m_Device, &fenceCreateInfo, nullptr, &m_InFlightFences[i]))
		{
			throw std::runtime_error("Failed to create Sync objects for frame!");
		}
	}
}

void VulkanContext::createTextureImage(const std::string& texture_file)
{
	// Read the pixel data from the texture file
	int texWidth, texHeight, texChannels;
	stbi_uc *pixels = stbi_load(texture_file.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels)
	{
		throw std::runtime_error("Failed to Load Texture!");
	}

	// Create a staging buffer to copy data to device memory
	//  - Can be done using a staging image as well but buffers are faster
	VulkanBuffer stagingBuffer;
	stagingBuffer.create(m_DeviceContext,
	                     imageSize,
	                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Fill the buffer
	void* data;
	vkMapMemory(m_DeviceContext.m_Device, stagingBuffer.m_Memory, 0, imageSize, 0, &data);
	std::memcpy(data, pixels, imageSize);
	vkUnmapMemory(m_DeviceContext.m_Device, stagingBuffer.m_Memory);

	// Free the pixel data
	stbi_image_free(pixels);

	// Create the image and image memory
	m_TextureImage.create(m_DeviceContext,
		                  texWidth,
		                  texHeight,
		                  1,
		                  VK_IMAGE_TYPE_2D,
		                  VK_FORMAT_R8G8B8A8_SRGB,
		                  VK_IMAGE_TILING_OPTIMAL,
		                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Transition image to TRANSFER_DST_OPTIMAL
	transitionImageLayout(m_TextureImage.m_Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy data from staging buffer to image
	copyBufferToImage(stagingBuffer.m_Buffer, m_TextureImage.m_Image, texWidth, texHeight);

	// Transition image to SHADER_READ_ONLY_OPTIMAL
	transitionImageLayout(m_TextureImage.m_Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Cleanup staging buffer
	stagingBuffer.destroy(m_DeviceContext.m_Device);
}

void VulkanContext::createTextureImageView()
{
	m_TextureImageView = vulkan::createImageView(m_DeviceContext.m_Device, m_TextureImage.m_Image, VK_FORMAT_R8G8B8A8_SRGB, 1, 1);
}

void VulkanContext::createTextureSampler()
{
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.mipLodBias = 0.f;
	samplerCreateInfo.minLod = 0.f;
	samplerCreateInfo.maxLod = 0.f;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	if (m_DeviceContext.m_PhysicalDeviceFeatures.samplerAnisotropy)
	{
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = m_DeviceContext.m_PhysicalDeviceProperties.limits.maxSamplerAnisotropy;
	}
	else
	{
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.maxAnisotropy = 1.f;
	}

	if (VK_SUCCESS != vkCreateSampler(m_DeviceContext.m_Device, &samplerCreateInfo, nullptr, &m_TextureSampler))
	{
		throw std::runtime_error("Failed to create Texture Sampler!");
	}
}

void VulkanContext::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout)
{
	VkCommandBuffer commandBuffer = vulkan::beginOneShotCommands(m_DeviceContext.m_Device, m_CommandPool);

	VkImageMemoryBarrier imageMemoryBarrier{};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = old_layout;
	imageMemoryBarrier.newLayout = new_layout;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage, dstStage;

	vulkan::findImageLayoutTransitionAccessMasksAndStages(
		old_layout, new_layout,
		imageMemoryBarrier.srcAccessMask, imageMemoryBarrier.dstAccessMask,
		srcStage, dstStage
	);

	vkCmdPipelineBarrier(
		commandBuffer,
		srcStage, dstStage,
		0,
		0, nullptr, // MemoryBarrier
		0, nullptr, // BufferMemoryBarrier
		1, &imageMemoryBarrier
	);

	vulkan::endOneShotCommands(m_DeviceContext.m_Device, commandBuffer, m_CommandPool, m_DeviceContext.m_GraphicsQueue);
}

void VulkanContext::copyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	commandPoolCreateInfo.queueFamilyIndex = m_DeviceContext.m_QueueFamilyIndices.transferFamily.value();

	VkCommandPool transferCommandPool{};

	if (VK_SUCCESS != vkCreateCommandPool(m_DeviceContext.m_Device, &commandPoolCreateInfo, nullptr, &transferCommandPool))
	{
		throw std::runtime_error("Failed to create Transfer Command Pool!");
	}

	VkCommandBuffer transferCommandBuffer = vulkan::beginOneShotCommands(m_DeviceContext.m_Device, transferCommandPool);

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	copyRegion.srcOffset = 0; // optional
	copyRegion.dstOffset = 0; // optional

	vkCmdCopyBuffer(transferCommandBuffer, src_buffer, dst_buffer, 1, &copyRegion);

	vulkan::endOneShotCommands(m_DeviceContext.m_Device,
	                           transferCommandBuffer,
		                       transferCommandPool,
		                       m_DeviceContext.m_TransferQueue);
	
	vkDestroyCommandPool(m_DeviceContext.m_Device, transferCommandPool, nullptr);
}

void VulkanContext::copyBufferToImage(VkBuffer src_buffer, VkImage dst_image, uint32_t width, uint32_t height)
{
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	commandPoolCreateInfo.queueFamilyIndex = m_DeviceContext.m_QueueFamilyIndices.transferFamily.value();

	VkCommandPool transferCommandPool{};

	if (VK_SUCCESS != vkCreateCommandPool(m_DeviceContext.m_Device, &commandPoolCreateInfo, nullptr, &transferCommandPool))
	{
		throw std::runtime_error("Failed to create Transfer Command Pool!");
	}

	VkCommandBuffer transferCommandBuffer = vulkan::beginOneShotCommands(m_DeviceContext.m_Device, transferCommandPool);

	VkBufferImageCopy copyRegion{};
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.imageOffset = { 0, 0, 0 };
	copyRegion.imageExtent = { width, height, 1 };
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageSubresource.layerCount = 1;

	vkCmdCopyBufferToImage(transferCommandBuffer, src_buffer, dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	vulkan::endOneShotCommands(m_DeviceContext.m_Device,
	                           transferCommandBuffer,
		                       transferCommandPool,
		                       m_DeviceContext.m_TransferQueue);
	
	vkDestroyCommandPool(m_DeviceContext.m_Device, transferCommandPool, nullptr);
}

void VulkanContext::updateUniformBuffers(uint32_t current_image)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	MatricesUBO ubo{};
	ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
	ubo.view = glm::lookAt(glm::vec3{ 2.f, 2.f, 2.f}, glm::vec3{0.f, 0.f, 0.f}, glm::vec3{0.f, 0.f, 1.f});
	ubo.proj = glm::perspective(glm::radians(45.f),
	                            static_cast<float>(m_SwapchainImageExtent.width) / static_cast<float>(m_SwapchainImageExtent.height),
								0.1f, 10.f);
	ubo.proj[1][1] *= -1; // invert Y of clip space (OpenGL->Vulkan)

	memcpy(m_UniformBufferMapped[current_image], &ubo, sizeof(ubo));
}

void VulkanContext::recreateSwapchain()
{
	vkDeviceWaitIdle(m_DeviceContext.m_Device);

	m_Swapchain.destroy(m_DeviceContext.m_Device);

	VulkanSwapchainSupportDetails swapchainSupportDetails = detail::query_swapchain_support(m_DeviceContext.m_PhysicalDevice, m_Surface);

	VkPresentModeKHR presentMode = detail::chooseSwapPresentMode(swapchainSupportDetails.presentModes);
	
	int width, height;
	glfwGetFramebufferSize(m_Window, &width, &height);

	m_SwapchainImageExtent = detail::chooseSwapExtent(swapchainSupportDetails.capabilities, width, height);


	m_Swapchain.create(
		m_DeviceContext.m_Device,
		m_Surface,
		swapchainSupportDetails,
		m_SwapchainImageFormat,
		presentMode,
		m_SwapchainImageExtent,
		static_cast<uint32_t>(m_Swapchain.m_Images.size()),
		m_DeviceContext.m_QueueFamilyIndices
	);

	m_Swapchain.createImageViews(m_DeviceContext.m_Device, m_SwapchainImageFormat.format);
	m_Swapchain.createFramebuffers(m_DeviceContext.m_Device, m_RenderPass.m_RenderPass, m_SwapchainImageExtent);
}

void VulkanContext::recordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // usage-related flags
	beginInfo.pInheritanceInfo = nullptr; // (optional) only for secondary buffers to specify which state to inherit from primary

	if (VK_SUCCESS != vkBeginCommandBuffer(command_buffer, &beginInfo))
	{
		throw std::runtime_error("Failed to Begin Recording command buffer!");
	}

	// Start a render pass
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = m_RenderPass.m_RenderPass;
	renderPassBeginInfo.framebuffer = m_Swapchain.m_Framebuffers[image_index];
	renderPassBeginInfo.renderArea.offset = {0, 0};
	renderPassBeginInfo.renderArea.extent = m_SwapchainImageExtent;
	VkClearValue clearColor = {{{ 0.f, 0.f, 0.f, 1.f }}};
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(command_buffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // INLINE : no secondary cmd buffers present


	// Drawing commands
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline.m_Pipeline);

#if 1
  // Create the viewport
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_SwapchainImageExtent.width);
	viewport.height = static_cast<float>(m_SwapchainImageExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	// Create the scissor rectangle
	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = m_SwapchainImageExtent;
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
#endif

	VkBuffer vertexBuffers[] = { m_VertexBuffer.m_Buffer };
	VkDeviceSize vertexOffsets[] = { 0 };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, vertexBuffers, vertexOffsets);

	vkCmdBindIndexBuffer(command_buffer, m_IndexBuffer.m_Buffer, 0, VK_INDEX_TYPE_UINT16);

	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline.m_PipelineLayout, 0, 1,
	                        &m_DescriptorSets[m_CurrentFrame], 0, nullptr);

	// vkCmdDraw(command_buffer, static_cast<uint32_t>(Mesh::getNumVertices()), 1, 0, 0);
	vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(Mesh::getNumIndices()), 1, 0, 0, 0);

	// End the render pass
	vkCmdEndRenderPass(command_buffer);

	// End recording the command buffer
	if (VK_SUCCESS != vkEndCommandBuffer(command_buffer))
	{
		throw std::runtime_error("Failed to Record command buffer!");
	}
}

void VulkanContext::drawFrame()
{
	/**
	 * Basic flow:
	 *  - Wait for prev frame to finish - (Fence)
	 *  - Acquire image from swapchain
	 *  - Record cmd buffer for drawing
	 *  - Submit recorded cmd buffer
	 *  - Present the swap chain image - (Semaphore)
	 *
	 * Synchronization primitives -
	 *  - Semaphore : Synchronizing between GPU tasks
	 *  - Fences : Synchronizing between GPU and CPU
	 */

	// Wait for the previous frame to finish
	vkWaitForFences(m_DeviceContext.m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

	// Acquire an image from the swapchain
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(m_DeviceContext.m_Device,
											m_Swapchain.m_Swapchain,
											UINT64_MAX,
	                                        m_ImageAvailableSemaphores[m_CurrentFrame],
											VK_NULL_HANDLE,
											&imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR /*|| m_IsFramebufferResized*/)
	{
		/*m_IsFramebufferResized = false;*/
		recreateSwapchain();
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("Failed to Acquire swapchain image!");
	}

	// Reset fence to unsignalled state to begin rendering next frame
	vkResetFences(m_DeviceContext.m_Device, 1, &m_InFlightFences[m_CurrentFrame]);

	// Update uniforms
	updateUniformBuffers(m_CurrentFrame);

	// Record the command buffer for drawing on acquired image
	vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);
	recordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex);

	// Submit the command buffer for processing
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // Which stages of the pipeline to wait in
	VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] }; // Which semaphores to wait on
																  // for each entry - waitStages[i] waits on waitSemaphores[i]
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

	VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] }; // Which semaphores to signal once the execution is finished
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (VK_SUCCESS != vkQueueSubmit(m_DeviceContext.m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]))
	{
		throw std::runtime_error("Failed to Submit Draw command buffer!");
	}

	// Present the swapchain image to the window
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pSwapchains = &m_Swapchain.m_Swapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pImageIndices = &imageIndex;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame];

	presentInfo.pResults = nullptr; // (optional) used to provide an array of VkResult values to check for individual swapchain results

	result = vkQueuePresentKHR(m_DeviceContext.m_PresentQueue, &presentInfo);

	if (VK_ERROR_OUT_OF_DATE_KHR == result || VK_SUBOPTIMAL_KHR == result || m_IsFramebufferResized)
	{
		m_IsFramebufferResized = false;
		recreateSwapchain();
	}
	else if (VK_SUCCESS != result)
	{
		throw std::runtime_error("Failed to Present swapchain image!");
	}

	// Advance to next frame
	m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanContext::handleFramebufferResized(int width, int height)
{
	m_IsFramebufferResized = true;
}

/**************************************************************************************************
									Helper Functions
 **************************************************************************************************/

bool detail::check_validation_layer_support()
{
	// List all the available validation layers
	uint32_t layerCount{};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	// Check if all the requested validation layers are available
	// NOTE: a nested loop here is not much of a problem since the number of validation layers required is very small (~1-5)
	for (const char* layerName : detail::s_ValidationLayers)
	{
		bool layerFound = false;

		for (const VkLayerProperties& layerProperties : availableLayers)
		{
			if (0 == strcmp(layerName, layerProperties.layerName))
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

void detail::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = 
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = detail::debug_messenger_callback;
	createInfo.pUserData = nullptr;
}

std::vector<const char*> detail::get_required_instance_extensions()
{
	std::vector<const char*> extensions;

#ifdef VKTUT_USE_GLFW
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
#endif

	if (detail::s_EnableDebugLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	extensions.insert(extensions.end(), detail::s_InstanceExtensions.begin(), detail::s_InstanceExtensions.end());

	printf("Vulkan extensions required : [%d]\n", static_cast<uint32_t>(extensions.size()));
	for (const auto& extension : extensions)
	{
		printf("\t%s\n", extension);
	}

	return extensions;
}

bool detail::is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	// Check if required device properties are present
	VkPhysicalDeviceProperties deviceProperties{};
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	if (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU != deviceProperties.deviceType)
	{
		return false;
	}

	// Check if required device features are present
	VkPhysicalDeviceFeatures deviceFeatures{};
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
	if (!deviceFeatures.geometryShader)
	{
		return false;
	}

	// Check if required queue families are present
	VulkanQueueFamilyIndices indices = find_queue_families(device, surface);
	if (!indices.isComplete())
	{
		return false;
	}

	// Check if required extensions are supported
	bool extensionsSupported = check_device_extension_support(device);
	if (!extensionsSupported)
	{
		return false;
	}

	// Check if the swap chain support is adequate
	bool swapchainAdequate = false;
	VulkanSwapchainSupportDetails swapchainSupport = query_swapchain_support(device, surface);
	swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();

	return swapchainAdequate;
}

VulkanQueueFamilyIndices detail::find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	VulkanQueueFamilyIndices indices{};

	uint32_t queueFamiliesCount{};
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamiliesCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamiliesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamiliesCount, queueFamilies.data());

	int i = 0;
	for (const VkQueueFamilyProperties& queueFamily : queueFamilies)
	{
		// Find a queue family that supports Graphics operations
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
		}

		// Find a queue family that supports presenting to the window surface
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = i;
		}

		if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT && !(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			indices.transferFamily = i;
		}

		if (indices.isComplete())
		{
			break;
		}
		++i;
	}

	return indices;
}

bool detail::check_device_extension_support(VkPhysicalDevice device)
{
	// List all the available extensions on the physical device
	uint32_t extensionCount{};
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	// Check if the required extensions are all available on the physical device
	std::set<std::string> requiredExtensions(detail::s_DeviceExtensions.begin(), detail::s_DeviceExtensions.end());
	for (const VkExtensionProperties& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

VulkanSwapchainSupportDetails detail::query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	VulkanSwapchainSupportDetails details;

	// Query the physical device's surface capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
	
	// Query the physical device's supported surface formats
	uint32_t formatsCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatsCount, nullptr);

	if (0 != formatsCount)
	{
		details.formats.resize(formatsCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatsCount, details.formats.data());
	}
	
	// Query the physical device's supported presentation modes
	uint32_t presentModesCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, nullptr);

	if (0 != presentModesCount)
	{
		details.presentModes.resize(presentModesCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, details.presentModes.data());
	}

	return details;
}

VkBool32 detail::debug_messenger_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	(void)fprintf(stderr, "Validation layer: %s\n", pCallbackData->pMessage);

	assert(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT == 0);

	return VK_FALSE;
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
