#include "BufferData.h"

#include <vector>
#include <xutility>

std::vector<VkVertexInputBindingDescription>& Vertex::getBindingDescriptions()
{
	static std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
	static bool initialized = false;
	if (!initialized)
	{
		bindingDescriptions[0].binding = 0;
		bindingDescriptions[0].stride = sizeof(Vertex);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		initialized = true;
	}
	return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription>& Vertex::getAttributeDescriptions()
{
	static std::vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
	static bool initialized = false;
	if (!initialized)
	{
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, uv);

		initialized = true;
	}
	return attributeDescriptions;
}

namespace mesh
{
	static constexpr Vertex vertices[] = {
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
		{{ 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
	};

	static constexpr uint16_t indices[] = {
		0, 1, 2, 2, 3, 0
	};
}

const Vertex* Mesh::getVertices()
{
	return mesh::vertices;
}

const uint16_t* Mesh::getIndices()
{
	return mesh::indices;
}

size_t Mesh::getNumVertices()
{
	return std::size(mesh::vertices);
}

size_t Mesh::getNumIndices()
{
	return std::size(mesh::indices);
}

