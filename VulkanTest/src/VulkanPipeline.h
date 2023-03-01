#pragma once
#include <string>
#include <vulkan/vulkan_core.h>

class VulkanPipeline
{
public:
	void create(VkDevice device, const std::string& vertex_shader, const std::string& fragment_shader, VkExtent2D swapchain_extent, VkRenderPass
	            render_pass);
	void destroy(VkDevice device);
	
	void createDescriptorSetLayout(VkDevice device);
	void createPipelineLayout(VkDevice device);

	VkDescriptorSetLayout m_DescriptorSetLayout{};
	VkPipelineLayout m_PipelineLayout{};
	VkPipeline m_Pipeline{};
};
