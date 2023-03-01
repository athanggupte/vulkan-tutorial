#include "VulkanPipeline.h"

#include "BufferData.h"
#include "VulkanFunctions.h"

#include <fstream>
#include <optional>

namespace detail
{
	static std::vector<char> readFile(const std::string& filepath);

	struct ShaderStagesDesc
	{
		std::optional<std::string> vertShaderFile;
		std::optional<std::string> fragShaderFile;
		std::optional<std::string> geomShaderFile;
		std::optional<std::string> tessContShaderFile;
		std::optional<std::string> tessEvalShaderFile;
	};

	struct VulkanShaderModulePack
	{
		VkShaderModule vertShader{};
		VkShaderModule fragShader{};
		VkShaderModule geomShader{};
		VkShaderModule tessContShader{};
		VkShaderModule tessEvalShader{};

		uint32_t numStages;

		void destroy(VkDevice device)
		{
			vkDestroyShaderModule(device, vertShader, nullptr);
			vkDestroyShaderModule(device, fragShader, nullptr);
			
			if (geomShader)     vkDestroyShaderModule(device, geomShader, nullptr);
			if (tessContShader) vkDestroyShaderModule(device, tessContShader, nullptr);
			if (tessEvalShader) vkDestroyShaderModule(device, tessEvalShader, nullptr);
		}
	};

	VulkanShaderModulePack createShaderModules(VkDevice device, const ShaderStagesDesc& shader_stages_desc);
	std::vector<VkPipelineShaderStageCreateInfo> createShaderStages(VkDevice device, const VulkanShaderModulePack& shader_module_pack);

}


void VulkanPipeline::create(VkDevice device, const std::string& vertex_shader, const std::string& fragment_shader, VkExtent2D swapchain_extent, VkRenderPass render_pass)
{
	// Create shader stages
	detail::ShaderStagesDesc shaderStagesDesc{};
	shaderStagesDesc.vertShaderFile = vertex_shader;
	shaderStagesDesc.fragShaderFile = fragment_shader;

	detail::VulkanShaderModulePack shaderModulePack = detail::createShaderModules(device, shaderStagesDesc);
	std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfos = detail::createShaderStages(device, shaderModulePack);

	// Set vertex input definition
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	auto& bindingDescriptions = Vertex::getBindingDescriptions();
	auto& attributeDescriptions = Vertex::getAttributeDescriptions();

	vertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
	vertexInputStateCreateInfo.pVertexBindingDescriptions = bindingDescriptions.data();
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	// Set up fixed stages of the pipeline
	// Select dynamic state variables
  	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

	std::vector<VkDynamicState> dynamicStates {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	// Set the primitive assembly description
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

  	// Create the viewport
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapchain_extent.width);
	viewport.height = static_cast<float>(swapchain_extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Create the scissor rectangle
	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = swapchain_extent;

	// Set the immutable viewport and scissor rectangle for the pipeline
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	// Set the rasterizer state
	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE; // VK_TRUE : clamps the pixels beyond the near and far planes to them;
															  // useful for shadow maps (requires enabling GPU feature)
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE; // VK_TRUE : geometry never passes through rasterizer stage
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL; // can set to wireframe (LINE) or point cloud (POINT)
																	 // (required enabling a GPU feature)
	rasterizationStateCreateInfo.lineWidth = 1.0f; // (lineWidth > 1.0f requires enabling wideLines GPU feature)

	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE; // bias the depth values by a linear transformation of a
															 // constant value or the slope of the fragment
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f; // (optional)
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f; // (optional)
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f; // (optional)

	// Set multisampling state
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{}; // (requires enabling a GPU feature)
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.minSampleShading = 1.0f; // (optional)
	multisampleStateCreateInfo.pSampleMask = nullptr; // (optional)
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE; // (optional)
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE; // (optional)

	// Set depth and stencil testing state
	// TODO: implement depth and stencil testing

	// Create color blending state for framebuffer attachment
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // (optional)
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // (optional)
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // (optional)
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // (optional)
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // (optional)
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // (optional)

	// Set global color blend state
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachment;
	colorBlendStateCreateInfo.blendConstants[0] = 0.0f; // (optional)
	colorBlendStateCreateInfo.blendConstants[1] = 0.0f; // (optional)
	colorBlendStateCreateInfo.blendConstants[2] = 0.0f; // (optional)
	colorBlendStateCreateInfo.blendConstants[3] = 0.0f; // (optional)

	// Create descriptor sets
	createDescriptorSetLayout(device);

	// Create a pipeline layout of uniform values
	createPipelineLayout(device);

	// Create the pipeline
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStagesCreateInfos.size());
	pipelineCreateInfo.pStages = shaderStagesCreateInfos.data();

	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = nullptr; // (optional)
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo; // (optional)

	pipelineCreateInfo.layout = m_PipelineLayout;
	pipelineCreateInfo.renderPass = render_pass;
	pipelineCreateInfo.subpass = 0;

	// Used to create a new pipeline from an existing one
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // using handle
	pipelineCreateInfo.basePipelineIndex = -1; // using index of a pipeline about to be created in the vkCreateGraphicsPipelines call

	if (VK_SUCCESS != vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_Pipeline))
	{
		throw std::runtime_error("Failed to create Pipeline!");
	}

	// Cleanup shader modules
	shaderModulePack.destroy(device);
}

void VulkanPipeline::destroy(VkDevice device)
{
	vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
	vkDestroyPipeline(device, m_Pipeline, nullptr);
	vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
}


void VulkanPipeline::createDescriptorSetLayout(VkDevice device)
{
	VkDescriptorSetLayoutBinding uboLayoutBinding{};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr; // (optional) for image samplers

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 1;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding layoutBindings[] = {
		uboLayoutBinding,
		samplerLayoutBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = std::size(layoutBindings);
	layoutCreateInfo.pBindings = layoutBindings;

	if (VK_SUCCESS != vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &m_DescriptorSetLayout))
	{
		throw std::runtime_error("Failed to create Descriptor Set Layout!");
	}
}

void VulkanPipeline::createPipelineLayout(VkDevice device)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_DescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // (optional)
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr; // (optional)

	if (VK_SUCCESS != vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_PipelineLayout))
	{
		throw std::runtime_error("Failed to create Pipeline Layout!");
	}
}

std::vector<char> detail::readFile(const std::string& filepath)
{
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		throw std::runtime_error("Failed to open file: " + filepath + "!");
	}

	size_t fileSize = file.tellg();
	std::vector<char> buf(fileSize);

	file.seekg(0);
	file.read(buf.data(), fileSize);

	file.close();

	return buf;
}

detail::VulkanShaderModulePack detail::createShaderModules(VkDevice device, const ShaderStagesDesc& shader_stages_desc)
{
	VulkanShaderModulePack shaderModulePack{};
	shaderModulePack.vertShader = vulkan::createShaderModule(device, readFile(shader_stages_desc.vertShaderFile.value()));
	shaderModulePack.fragShader = vulkan::createShaderModule(device, readFile(shader_stages_desc.fragShaderFile.value()));

	// TODO: Handle other shader types here

	shaderModulePack.numStages = 2;

	return shaderModulePack;
}

std::vector<VkPipelineShaderStageCreateInfo> detail::createShaderStages(VkDevice device, const VulkanShaderModulePack& shader_module_pack)
{
	std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfos(shader_module_pack.numStages);
	// Create vertex shader stage
	shaderStagesCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStagesCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStagesCreateInfos[0].module = shader_module_pack.vertShader;
	shaderStagesCreateInfos[0].pName = "main"; // specify entry point
	
	// Create fragment shader stage
	shaderStagesCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStagesCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStagesCreateInfos[1].module = shader_module_pack.fragShader;
	shaderStagesCreateInfos[1].pName = "main"; // specify entry point

	return shaderStagesCreateInfos;
}
