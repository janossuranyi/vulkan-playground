
#include "pch.h"
#include "gltf_loader.h"
#include <map>
#include <tiny_gltf.h>
#include <jsrlib/jsr_logger.h>
#include <glm/gtc/type_ptr.hpp>

namespace jsr {

	namespace fs = std::filesystem;

	static bool getGltfAttributeIndex(const std::map<std::string, int>& attributes, const std::string& name, int* out);
	static std::vector<uint8_t> getGltfAttribute(const tinygltf::Model& model, int attribIndex, size_t typeSize, int expectedType, int expectedCompType);
	static void processGltfMaterials(const tinygltf::Model& model, World& world);
	static bool processGltfMeshes(const tinygltf::Model& model, World& world);
	static void processGltfNodes(const tinygltf::Model& model, World& world);

	bool gltfLoadWorld(std::filesystem::path filename, World& world)
	{
		std::string warn, err;
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		bool result = false;

		if (filename.extension() == ".gltf") {
			result = loader.LoadASCIIFromFile(&model, &err, &warn, filename.generic_u8string());
		}
		else {
			result = loader.LoadBinaryFromFile(&model, &err, &warn, filename.generic_u8string());
		}

		if (!result) {
			jsrlib::Error("Cannot load GLTF: %s", err.c_str());
			return false;
		}

		if (!warn.empty()) {
			jsrlib::Info("load GLTF warning: %s", warn.c_str());
		}

		processGltfMaterials(model, world);
		processGltfMeshes(model, world);
		processGltfNodes(model, world);
		world.update();

		return result;
	}
	bool getGltfAttributeIndex(const std::map<std::string, int>& attributes, const std::string& name, int* out)
	{
		if (attributes.find(name) != attributes.end())
		{
			*out = attributes.at(name);
			return true;
		}
		return false;
	}

	std::vector<uint8_t> getGltfAttribute(const tinygltf::Model& model, int attribIndex, size_t typeSize, int expectedType, int expectedCompType)
	{
		const tinygltf::Accessor& accessor = model.accessors[attribIndex];
		const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
		const tinygltf::Buffer& buffer = model.buffers[view.buffer];

		assert(expectedType == accessor.type);
		assert(expectedCompType == accessor.componentType);
		assert(view.byteStride == 0 || view.byteStride == typeSize);
		
		const size_t byteCount = accessor.count * typeSize;

		std::vector<uint8_t> byteData(byteCount);
		memcpy(byteData.data(), buffer.data.data() + view.byteOffset + accessor.byteOffset, byteCount);

		return byteData;
	}

	void processGltfMaterials(const tinygltf::Model& model, World& world)
	{
		world.materials.resize(model.materials.size());

		// materials
		for (size_t i = 0; i < model.materials.size(); ++i) {
			const auto& src = model.materials[i];

			if (src.alphaMode == "OPAQUE") world.materials[i].alphaMode = AlphaMode::ALPHA_MODE_OPAQUE;
			else if (src.alphaMode == "BLEND") world.materials[i].alphaMode = AlphaMode::ALPHA_MODE_BLEND;
			else if (src.alphaMode == "MASK") world.materials[i].alphaMode = AlphaMode::ALPHA_MODE_MASK;

			world.materials[i].type = MATERIAL_TYPE_METALLIC_ROUGHNESS;
			world.materials[i].baseColorFactor = glm::make_vec4((double*)src.pbrMetallicRoughness.baseColorFactor.data());
			world.materials[i].emissiveFactor = glm::make_vec3(src.emissiveFactor.data());
			world.materials[i].metallicFactor = (float)src.pbrMetallicRoughness.metallicFactor;
			world.materials[i].roughnessFactor = (float)src.pbrMetallicRoughness.roughnessFactor;
			world.materials[i].alphaCutoff = (float)src.alphaCutoff;
			world.materials[i].doubleSided = src.doubleSided;
			world.materials[i].normalScale = glm::vec3((float)src.normalTexture.scale);
			world.materials[i].normalScale.y *= -1.0f;
			world.materials[i].name = src.name;

			if (src.emissiveTexture.index > -1) {
				world.materials[i].texturePaths.emissiveTexturePath = fs::path(model.images[model.textures[src.emissiveTexture.index].source].uri);
			}
			if (src.normalTexture.index > -1) {
				world.materials[i].texturePaths.normalTexturePath = fs::path(model.images[model.textures[src.normalTexture.index].source].uri);
			}
			if (src.pbrMetallicRoughness.baseColorTexture.index > -1) {
				world.materials[i].texturePaths.albedoTexturePath = fs::path(model.images[model.textures[src.pbrMetallicRoughness.baseColorTexture.index].source].uri);
			}
			if (src.pbrMetallicRoughness.metallicRoughnessTexture.index > -1) {
				world.materials[i].texturePaths.specularTexturePath = fs::path(model.images[model.textures[src.pbrMetallicRoughness.metallicRoughnessTexture.index].source].uri);
			}
		}
	}

	bool processGltfMeshes(const tinygltf::Model& model, World& world)
	{
		size_t mesh_count(0);
		std::vector<uint8_t> attribVec;

		for (size_t i = 0; i < model.meshes.size(); ++i)
		{
			const tinygltf::Mesh& mesh = model.meshes[i];
			for (size_t j = 0; j < mesh.primitives.size(); ++j)
			{
				++mesh_count;
				const auto& tris = mesh.primitives[j];
				bool allAttribExists = true;
				int pos_index(-1), normal_index(-1), color_index(-1), tangent_index(-1), uv_index(-1);
				allAttribExists &= getGltfAttributeIndex(tris.attributes, "POSITION", &pos_index);
				allAttribExists &= getGltfAttributeIndex(tris.attributes, "NORMAL", &normal_index);
				allAttribExists &= getGltfAttributeIndex(tris.attributes, "TANGENT", &tangent_index);
				allAttribExists &= getGltfAttributeIndex(tris.attributes, "TEXCOORD_0", &uv_index);
				getGltfAttributeIndex(tris.attributes, "COLOR_0", &color_index);

				if (!allAttribExists) {
					jsrlib::Error("File contains meshes with missing attributes");
					return false;
				}
				MeshData data;
				data.material = tris.material;
				data.aabb.Extend(glm::vec3(glm::make_vec3(model.accessors[pos_index].minValues.data())));
				data.aabb.Extend(glm::vec3(glm::make_vec3(model.accessors[pos_index].maxValues.data())));

				{
					const std::vector<uint8_t> positionBytes = getGltfAttribute(model, pos_index, sizeof(glm::vec3), TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
					const std::vector<uint8_t> normalBytes = getGltfAttribute(model, normal_index, sizeof(glm::vec3), TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT);
					const std::vector<uint8_t> tangentBytes = getGltfAttribute(model, tangent_index, sizeof(glm::vec4), TINYGLTF_TYPE_VEC4, TINYGLTF_COMPONENT_TYPE_FLOAT);
					const std::vector<uint8_t> uvBytes = getGltfAttribute(model, uv_index, sizeof(glm::vec2), TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT);

					data.positions.resize(positionBytes.size() / sizeof(glm::vec3));
					memcpy(data.positions.data(), positionBytes.data(), positionBytes.size());

					data.normals.resize(normalBytes.size() / sizeof(glm::vec3));
					memcpy(data.normals.data(), normalBytes.data(), normalBytes.size());

					data.tangents.resize(tangentBytes.size() / sizeof(glm::vec4));
					memcpy(data.tangents.data(), tangentBytes.data(), tangentBytes.size());

					data.uvs.resize(uvBytes.size() / sizeof(glm::vec2));
					memcpy(data.uvs.data(), uvBytes.data(), uvBytes.size());

				}

				if (model.accessors[tris.indices].componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					const std::vector<uint8_t> indexBytes = getGltfAttribute(model, tris.indices, sizeof(uint16_t), TINYGLTF_TYPE_SCALAR, TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
					data.indices.reserve(indexBytes.size() / sizeof(uint16_t));
					for (size_t i = 0; i < indexBytes.size(); i += sizeof(uint16_t)) {
						data.indices.push_back(*reinterpret_cast<const uint16_t*>(&indexBytes[i]));
					}
				}
				else if (model.accessors[tris.indices].componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					const std::vector<uint8_t> indexBytes = getGltfAttribute(model, tris.indices, sizeof(uint32_t), TINYGLTF_TYPE_SCALAR, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
					data.indices.reserve(indexBytes.size() / sizeof(uint32_t));
					for (size_t i = 0; i < indexBytes.size(); i += sizeof(uint32_t)) {
						data.indices.push_back(*reinterpret_cast<const uint32_t*>(&indexBytes[i]));
					}
				}

				world.meshes.push_back(std::move(data));
			}
		}
		return true;
	}

	void processGltfNodes(const tinygltf::Model& model, World& world)
	{
		std::vector<std::vector<int>> mesh_primitives;
		int running_index(0);

		for (size_t i = 0; i < model.meshes.size(); ++i)
		{
			std::vector<int> tmp(model.meshes[i].primitives.size());
			for (size_t j = 0; j < model.meshes[i].primitives.size(); ++j)
			{
				tmp[j] = running_index++;
			}
			mesh_primitives.push_back(tmp);
		}

		world.scene.nodes.resize(model.nodes.size());
		for (size_t nodeIdx(0); nodeIdx < model.nodes.size(); ++nodeIdx)
		{
			const auto& node = model.nodes[nodeIdx];
			Node3d& myNode = world.scene.nodes[nodeIdx];

			myNode.nodeType = EntityType::Empty;

			if (node.mesh > -1) {
				myNode.nodeType = EntityType::Mesh;
				myNode.setEntity(mesh_primitives[node.mesh]);
				world.scene.entities[static_cast<size_t>(EntityType::Mesh)].push_back(nodeIdx);
			}
			else if (node.camera > -1) {
				myNode.nodeType = EntityType::Camera;
				myNode.setEntity({ node.camera });
				world.scene.entities[static_cast<size_t>(EntityType::Camera)].push_back(nodeIdx);
			}
			else if (node.extensions.find("KHR_lights_punctual") != node.extensions.end()) {
				myNode.nodeType = EntityType::Light;
				myNode.setEntity({ node.extensions.at("KHR_lights_punctual").Get("light").GetNumberAsInt() });
				world.scene.entities[static_cast<size_t>(EntityType::Light)].push_back(nodeIdx);
			}


			myNode.setChildren(node.children);
			for (int child : node.children)
			{
				world.scene.nodes[child].setParent(nodeIdx);
			}

			if (node.matrix.empty()) {
				if (!node.translation.empty()) {
					myNode.setPosition( glm::vec3(glm::make_vec3(node.translation.data())) );
				}
				if (!node.rotation.empty()) {
					myNode.setRotation(glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]));
				}
				if (!node.scale.empty()) {
					myNode.setScale(glm::vec3(glm::make_vec3(node.scale.data())));
				}
			}
			else {
				auto mtx = glm::mat4(glm::make_mat4(node.matrix.data()));
				myNode.setTransform(mtx);
			}
		}

		for (size_t i = 0; i < model.scenes[model.defaultScene].nodes.size(); ++i)
		{
			world.scene.rootNodes.push_back(model.scenes[model.defaultScene].nodes[i]);
			world.addUpdatableNode(model.scenes[model.defaultScene].nodes[i]);
		}
	}
}