#pragma once

#include "pch.h"
#include "renderer/VulkanRenderer.h"
#include "renderer/ResourceHandles.h"
#include "renderer/Vertex.h"
#include "renderer/Material.h"
#include "renderer/Bounds.h"

#include <tiny_gltf.h>

namespace jsr {

	const int nodeType_Empty = 0;
	const int nodeType_Mesh = 1;
	const int nodeType_Light = 2;
	const int nodeType_Camera = 3;

	enum NodeFilter {
		nodeFilter_Light = 1,
		nodeFilter_Mesh = 2,
		nodeFilter_Camera = 4
	};

	enum MaterialFilter {
		materialFilter_None = 0,
		materialFilter_Opaque = 1,
		materialFilter_Transparent = 2,
		materialFilter_AlphaTest = 4
	};

	struct MeshPrimitive {
		int material = -1;
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t baseVertex;
		glm::vec3 bbox[2];
	};

	struct FrontendMesh {
		std::string name;
		std::vector<MeshPrimitive> primitives;
	};
	
	struct Node {
		int nodeType = nodeType_Empty;
		int parent = -1;
		int object = -1;
		glm::vec3 position = glm::vec3(0.0f);
		glm::vec3 scale = glm::vec3(1.0f);
		glm::quat rotation = glm::quat(1.f,0.f,0.f,0.f);
		glm::mat4 _worldTransMatrix = glm::mat4(1.0f);
		bool dirty = false;
		std::vector<int> children;
	};

	struct S_MATRIXES {
		glm::mat4 mModel;
		glm::mat4 mNormal;
	};
	static_assert((sizeof(S_MATRIXES) % 16) == 0);

	struct S_OBJECT {
		int matrixesIdx;
		int materialIdx;
	};
	static_assert((sizeof(S_OBJECT) % 4) == 0);

	struct S_MATERIAL {
		glm::vec4 baseColorFactor;
		glm::vec4 emissiveFactor;
		glm::vec4 normalScale;
		float occlusionStrenght;
		float metallicFactor;
		float roughnessFactor;
		int type;
		int flags;
		int baseColorTexture;
		int normalTexture;
		int specularTexture;
		int occlusionTexture;
		int emissiveTexture;
		int dummy0;
		int dummy1;
	};
	static_assert((sizeof(S_MATERIAL) & 15) == 0);

	class Model {
	public:
		Model(jsr::VulkanRenderer* backend) : m_backend(backend) {}
		bool load_from_gltf(const std::filesystem::path& filename);
		void cleanup();

		static std::vector<glm::vec3> getPositions(const tinygltf::Model& model, const tinygltf::Primitive& primitive);
		static std::vector<glm::vec3> getNormals(const tinygltf::Model& model, const tinygltf::Primitive& primitive);
		static std::vector<glm::vec4> getTangents(const tinygltf::Model& model, const tinygltf::Primitive& primitive);
		static std::vector<glm::vec2> getUVs(const tinygltf::Model& model, const tinygltf::Primitive& primitive);
		static std::vector<glm::vec4> getColors(const tinygltf::Model& model, const tinygltf::Primitive& primitive);

		std::vector<int> visibleNodes(const glm::mat4& mView, const glm::mat4& mProjection, NodeFilter nodeFilter, MaterialFilter materialFilter);

		handle::Mesh m_backendMesh;
		handle::StorageBuffer m_materialBuffer;
		handle::StorageBuffer m_matrixBuffer;

		glm::mat4 getWorldTransform(int object);

		std::vector<S_MATRIXES> m_matrixes;
		std::vector<handle::Image> m_images;
		std::vector<FrontendMesh> m_meshes;
		std::vector<Node> m_nodes;
		std::vector<int> m_scene;
		std::vector<Material> m_materials;
		std::vector<S_MATERIAL> m_shaderMaterials;
		jsr::VulkanRenderer* m_backend;
		jsr::Bounds m_bounds;
	};

}