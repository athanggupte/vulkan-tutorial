#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 uv;

	static std::vector<VkVertexInputBindingDescription>& getBindingDescriptions();
	static std::vector<VkVertexInputAttributeDescription>& getAttributeDescriptions();
};


struct Mesh
{
	static const Vertex* getVertices();
	static const uint16_t* getIndices();
	static size_t getNumVertices(); // number of vertices
	static size_t getNumIndices(); // number of vertices
};


struct MatricesUBO
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

