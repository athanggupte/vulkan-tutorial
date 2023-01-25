#include "VulkanRenderPass.h"

#include <stdexcept>


void VulkanRenderPass::create(VkDevice device, VkFormat swapchain_image_format)
{
	// Create the color buffer attachment
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain_image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Create the subpasses and attachment references for the subpasses
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Can be raytracing bindpoint
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef; // index of the attachment in this array is the layout location of the out value in fragment shader

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;

	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Wait on the previous output to be written
	dependency.srcAccessMask = 0;                                            // for all operations to finish

	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Wait in the current color attachment stage
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;         // Halt all WRITE operations until dependency complete

	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	if (VK_SUCCESS != vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &m_RenderPass))
	{
		throw std::runtime_error("Failed to create Render Pass!");
	}
}

void VulkanRenderPass::destroy(VkDevice device)
{
	vkDestroyRenderPass(device, m_RenderPass, nullptr);
}
