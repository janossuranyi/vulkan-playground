#include "renderer/Vertex.h"

namespace jsr {

    	VertexInputDescription Vertex::vertex_input_description() {

		VertexInputDescription ret{};

		//we will have just 1 vertex buffer binding, with a per-vertex rate
		VkVertexInputBindingDescription mainBinding = {};
		mainBinding.binding = 0;
		mainBinding.stride = sizeof(Vertex);
		mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		ret.bindings.push_back(mainBinding);

		//Position will be stored at Location 0
		VkVertexInputAttributeDescription positionAttribute = {};
		positionAttribute.binding = 0;
		positionAttribute.location = 0;
		positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
		positionAttribute.offset = offsetof(Vertex, xyz);

		//UV will be stored at Location 1
		VkVertexInputAttributeDescription uvAttribute = {};
		uvAttribute.binding = 0;
		uvAttribute.location = 1;
		uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
		uvAttribute.offset = offsetof(Vertex, uv);

		//Normal will be stored at Location 2
		VkVertexInputAttributeDescription normalAttribute = {};
		normalAttribute.binding = 0;
		normalAttribute.location = 2;
		normalAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		normalAttribute.offset = offsetof(Vertex, normal);

		//Tangent will be stored at Location 3
		VkVertexInputAttributeDescription tangentAttribute = {};
		tangentAttribute.binding = 0;
		tangentAttribute.location = 3;
		tangentAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		tangentAttribute.offset = offsetof(Vertex, tangent);

		//Color will be stored at Location 4
		VkVertexInputAttributeDescription colorAttribute = {};
		colorAttribute.binding = 0;
		colorAttribute.location = 4;
		colorAttribute.format = VK_FORMAT_R8G8B8A8_UNORM;
		colorAttribute.offset = offsetof(Vertex, color);

		ret.attributes.push_back(positionAttribute);
		ret.attributes.push_back(uvAttribute);
		ret.attributes.push_back(normalAttribute);
		ret.attributes.push_back(tangentAttribute);
		ret.attributes.push_back(colorAttribute);

		return ret;
	}
	VertexInputDescription Vertex::vertex_input_description_position_only()
	{
		VertexInputDescription ret{};

		//we will have just 1 vertex buffer binding, with a per-vertex rate
		VkVertexInputBindingDescription mainBinding = {};
		mainBinding.binding = 0;
		mainBinding.stride = sizeof(Vertex);
		mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		ret.bindings.push_back(mainBinding);

		//Position will be stored at Location 0
		VkVertexInputAttributeDescription positionAttribute = {};
		positionAttribute.binding = 0;
		positionAttribute.location = 0;
		positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
		positionAttribute.offset = offsetof(Vertex, xyz);

		ret.attributes.push_back(positionAttribute);

		return ret;
	}

}