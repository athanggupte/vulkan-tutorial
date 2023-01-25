#include "VulkanContext.h"

#include <stdexcept>
#include <vector>
#include <optional>
#include <set>

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
	static VulkanSwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

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


void VulkanContext::initContext(const char* app_name, GLFWwindow* window, bool enable_debugging)
{
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

	VulkanQueueFamilyIndices indices = detail::find_queue_families(m_PhysicalDevice, m_Surface);

	// Create a logical device for the selected GPU
	// (can create multiple logical devices for same physical device with different extensions and features)
	createLogicalDevice(indices);

	createSwapchain(window, indices);

	createImageViews();

	m_RenderPass.create(m_Device, m_Swapchain.m_Format);

	createFramebuffers();

	m_GraphicsPipeline.create(m_Device,
							  "..\\build\\bin\\Debug-x86_64\\VulkanTest\\simple_shader.vert.spv",
	                          "..\\build\\bin\\Debug-x86_64\\VulkanTest\\simple_shader.frag.spv",
							  m_Swapchain.m_Extent,
	                          m_RenderPass.m_RenderPass);

	createCommandPool(indices);

	createCommandBuffer();

	createSyncObjects();
}

void VulkanContext::shutdownContext()
{
	vkDeviceWaitIdle(m_Device);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

	m_GraphicsPipeline.destroy(m_Device);

	for (VkFramebuffer framebuffer : m_SwapchainFramebuffers)
	{
		vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
	}

	m_RenderPass.destroy(m_Device);

	for (VkImageView imageView : m_SwapchainImageViews)
	{
		vkDestroyImageView(m_Device, imageView, nullptr);
	}

	m_Swapchain.destroy(m_Device);
	vkDestroyDevice(m_Device, nullptr);
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

	if (detail::s_EnableDebugLayers)
	{
		vulkan::DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
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
	appInfo.apiVersion = VK_API_VERSION_1_0;

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

	if (VK_SUCCESS != vulkan::CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger))
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
			m_PhysicalDevice = device;
			break;
		}
	}

	if (VK_NULL_HANDLE == m_PhysicalDevice)
	{
		throw std::runtime_error("Failed to find a suitable GPU!");
	}
}

void VulkanContext::createLogicalDevice(const VulkanQueueFamilyIndices& indices)
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	// Select unique queue families for the required operations - Graphics, Presentation, ...
	std::set<uint32_t> uniqueQueueFamilies { indices.graphicsFamily.value(), indices.presentFamily.value() };

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

	if (VK_SUCCESS != vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device))
	{
		throw std::runtime_error("Failed to create Logical Device!");
	}

	// Get handle to the queues
	vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
}

void VulkanContext::createSwapchain(GLFWwindow* window, const VulkanQueueFamilyIndices& indices)
{
	VulkanSwapchainSupportDetails swapchainSupportDetails = detail::querySwapchainSupport(m_PhysicalDevice, m_Surface);

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	m_Swapchain.create(
		m_Device,
		m_Surface,
		swapchainSupportDetails,
		indices,
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height));
}

void VulkanContext::createImageViews()
{
	m_SwapchainImageViews.resize(m_Swapchain.m_Images.size());

	for (int i = 0; i < m_Swapchain.m_Images.size(); i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_Swapchain.m_Images[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_Swapchain.m_Format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		if (VK_SUCCESS != vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapchainImageViews[i]))
		{
			throw std::runtime_error("Failed to create Image View!");
		}
	}
}

void VulkanContext::createFramebuffers()
{
	m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());
	for (size_t i = 0; i < m_SwapchainImageViews.size(); i++)
	{
		VkImageView attachments[] = {
			m_SwapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo{};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = m_RenderPass.m_RenderPass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = attachments;
		framebufferCreateInfo.width = m_Swapchain.m_Extent.width;
		framebufferCreateInfo.height = m_Swapchain.m_Extent.height;
		framebufferCreateInfo.layers = 1;

		if (VK_SUCCESS != vkCreateFramebuffer(m_Device, &framebufferCreateInfo, nullptr, &m_SwapchainFramebuffers[i]))
		{
			throw std::runtime_error("Failed to create Framebuffer!");
		}
	}
}

void VulkanContext::createCommandPool(const VulkanQueueFamilyIndices& indices)
{
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();

	if (VK_SUCCESS != vkCreateCommandPool(m_Device, &commandPoolCreateInfo, nullptr, &m_CommandPool))
	{
		throw std::runtime_error("Failed to create Command Pool!");
	}
}

void VulkanContext::createCommandBuffer()
{
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = m_CommandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

	
	if (VK_SUCCESS != vkAllocateCommandBuffers(m_Device, &allocateInfo, m_CommandBuffers.data()))
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
		if (VK_SUCCESS != vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_ImageAvailableSemaphores[i]) ||
			VK_SUCCESS != vkCreateSemaphore(m_Device, &semaphoreCreateInfo, nullptr, &m_RenderFinishedSemaphores[i]) ||
			VK_SUCCESS != vkCreateFence(m_Device, &fenceCreateInfo, nullptr, &m_InFlightFences[i]))
		{
			throw std::runtime_error("Failed to create Sync objects for frame!");
		}
	}
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
	renderPassBeginInfo.framebuffer = m_SwapchainFramebuffers[image_index];
	renderPassBeginInfo.renderArea.offset = {0, 0};
	renderPassBeginInfo.renderArea.extent = m_Swapchain.m_Extent;
	VkClearValue clearColor = {{{ 0.f, 0.f, 0.f, 1.f }}};
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(command_buffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); // INLINE : no secondary cmd buffers present


	// Drawing commands
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline.m_Pipeline);

#if 0
  // Create the viewport
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_Swapchain.m_Extent.width);
	viewport.height = static_cast<float>(m_Swapchain.m_Extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);

	// Create the scissor rectangle
	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = m_Swapchain.m_Extent;
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
#endif

	vkCmdDraw(command_buffer, 3, 1, 0, 0);

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
	vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]); // reset to unsignalled state

	// Acquire an image from the swapchain
	uint32_t imageIndex;
	vkAcquireNextImageKHR(m_Device, m_Swapchain.m_Swapchain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

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

	if (VK_SUCCESS != vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]))
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

	if (VK_SUCCESS != vkQueuePresentKHR(m_PresentQueue, &presentInfo))
	{
		throw std::runtime_error("Failed to Present swapchain image!");
	}

	// Advance to next frame
	m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
	VulkanSwapchainSupportDetails swapchainSupport = querySwapchainSupport(device, surface);
	swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();

	return true;
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

VulkanSwapchainSupportDetails detail::querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
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

	return VK_FALSE;
}

