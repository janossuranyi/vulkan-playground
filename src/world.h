#pragma once

#include "pch.h"
#include "mesh_data.h"
#include "material.h"
#include "light.h"
#include "bounds.h"

namespace jsr {

	enum EntityType { EntityType_Mesh, EntityType_Light, EntityType_Camera, EntityType_Empty };

	class Frustum;

	class Node3d {
	public:
		EntityType nodeType = EntityType_Empty;
		inline EntityType getType() const { return nodeType; }
		inline bool isEmpty() const { return nodeType == EntityType_Empty; }
		inline bool isMesh() const { return nodeType == EntityType_Mesh; }
		inline bool isLight() const { return nodeType == EntityType_Light; }
		inline bool isCamera() const { return nodeType == EntityType_Camera; }
		void setPosition(const glm::vec3& v);
		void setScale(const glm::vec3& v);
		void setRotation(const glm::quat& v);
		void setTransform(const glm::mat4& m);
		void setNeedToUpdate(bool b);
		bool getNeedToUpdate() const;
		void setEntity(const std::vector<int>& v);
		void addEntity(int v);
		void setChildren(const std::vector<int>& v);
		int getParent() const { return m_parent; }
		void setParent(int p) { m_parent = p; }
		void add(int child);
		const std::vector<int>& getChildren() const { return m_children; }
		const std::vector<int>& getEntities() const { return m_entity; }
		const glm::mat4& getTransform() const;
		const glm::vec3& getPosition() const;
		const glm::vec3& getScale() const;
		const glm::quat& getRotation() const;
		void updateTransform(const glm::mat4& parentMatrix);
		jsr::Bounds& bounds();
	private:
		std::vector<int> m_entity;
		bool m_needToUpdate = false;
		int m_parent =-1;
		glm::mat4 m_nodeMatrix = glm::mat4(1.0f);
		glm::vec3 m_position = glm::vec3(0.0f);
		glm::vec3 m_scale = glm::vec3(1.0f);
		glm::quat m_rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
		std::vector<int> m_children;
		jsr::Bounds bv;
	};

	struct ObjectBinary {
		glm::mat4 modelMatrix;
		int meshIndex;
	};

	struct Scene {
		std::vector<int>			rootNodes;
		std::vector<Node3d>			nodes;
		std::vector<int>			entities[EntityType_Empty];
	};

	struct RenderEntity {
		ObjectBinary object;
		Bounds aabb;
	};

	typedef std::vector<RenderEntity> RenderEntityList;

	struct BVHNode {
		Bounds aabb;
		uint32_t leftFirst;
		uint32_t primCount;
		bool isLeaf() const { return primCount > 0; }
	};

	class World {
	public:
		Scene scene;
		std::vector<MeshData>		meshes;
		std::vector<Light>			ligths;
		std::vector<Material>		materials;
		std::vector<Bounds>			aabbs;
		std::vector<BVHNode>		bvhNode;
		uint32_t bvhRootNodeIdx = 0;
		uint32_t bvhNodesUsed = 1;

		Bounds aabb;
		int add(const Node3d& n);
		int add(int parent, const Node3d& n);
		void addUpdatableNode(int n);
		void addUpdatableNode(int n, const int* data);
		void update();
		void updateBVH();
		RenderEntityList getVisibleEntities(const Frustum& frustum);
	private:
		void buildBVH();
		void UdateNodeBounds(uint32_t);
		void Subdivide(uint32_t);
		void IntersectBVH(const Frustum& frustum, uint32_t nodeIdx, RenderEntityList& out);
		float EvaluateSAH(BVHNode& node, int axis, float pos);
		float FindBestSplitPlane(BVHNode& node, int& axis, float& splitPos);
		float CalculateNodeCost(BVHNode& node);
		void RefitBVH();

		std::vector<int> _nodesToUpdate;
		int intersectTestCount = 0;
	};

	struct RenderWorld {
		std::vector<ObjectBinary>		objects;
		std::vector<VertexCacheEntry>	meshes;
	};

}