#include "pch.h"
#include "world.h"
#include "frustum.h"
#include <jsrlib/jsr_logger.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace jsr {

	using namespace glm;

	void Node3d::setPosition(const glm::vec3& v)
	{
		m_position = v;
		m_needToUpdate = true;
	}
	void Node3d::setScale(const glm::vec3& v)
	{
		m_scale = v;
		m_needToUpdate = true;
	}
	void Node3d::setRotation(const glm::quat& v)
	{
		m_rotation = v;
		m_needToUpdate = true;
	}
	void Node3d::setTransform(const glm::mat4& m)
	{
		glm::vec3 tmp1;
		glm::vec4 tmp2;
		glm::decompose(m, m_scale, m_rotation, m_position, tmp1, tmp2);
		m_needToUpdate = true;
	}
	void Node3d::setNeedToUpdate(bool b)
	{
		m_needToUpdate = b;
	}
	bool Node3d::getNeedToUpdate() const
	{
		return m_needToUpdate;
	}
	void Node3d::setEntity(const std::vector<int>& v)
	{
		m_entity = v;
	}
	void Node3d::addEntity(int v)
	{
		m_entity.push_back(v);
	}
	void Node3d::setChildren(const std::vector<int>& v)
	{
		m_children = v;
	}
	void Node3d::add(int child)
	{
		m_children.push_back(child);
	}
	const glm::mat4& Node3d::getTransform() const
	{
		return m_nodeMatrix;
	}
	const glm::vec3& Node3d::getPosition() const
	{
		return m_position;
	}
	const glm::vec3& Node3d::getScale() const
	{
		return m_scale;
	}
	const glm::quat& Node3d::getRotation() const
	{
		return m_rotation;
	}
	void Node3d::updateTransform(const glm::mat4& parentMatrix)
	{
		using namespace glm;
		const mat4 translate = glm::translate(mat4(1.0f), m_position);
		const mat4 rotate = glm::mat4_cast(m_rotation);
		const mat4 scale = glm::scale(mat4(1.0f), m_scale);
		auto mModel = (translate * rotate * scale);

		m_nodeMatrix = parentMatrix * mModel;
	}
	int World::add(const Node3d& n)
	{
		scene.nodes.push_back(n);
		int idx = (int) scene.nodes.size() - 1;
		scene.rootNodes.push_back(idx);

		return idx;
	}
	int World::add(int parent, const Node3d& n)
	{
		int idx = add(n);
		scene.nodes[parent].add(idx);
		scene.nodes[idx].setParent(parent);

		return 0;
	}
	void World::addUpdatableNode(int n)
	{
		_nodesToUpdate.push_back(n);
	}
	void World::addUpdatableNode(int n, const int* data)
	{
		for (size_t i(0); i < n; ++i)
		{
			_nodesToUpdate.push_back(data[i]);
		}
	}

	static int getObjectStartIndex(const World& world, int node) {
		int result = 0;
		for (size_t i = 0; i < node; ++i) {
			if (world.scene.nodes[i].isMesh()) {
				result += world.scene.nodes[i].getEntities().size();
			}
		}

		return result;
	}

	void World::update()
	{
		const glm::mat4 initialMatrix = glm::mat4(1.f);

		const bool dirty = !_nodesToUpdate.empty();
		std::vector<glm::mat4> parentMatrices(_nodesToUpdate.size(), initialMatrix);
		while (!_nodesToUpdate.empty())
		{
			int currentNodeIndex = _nodesToUpdate.back();
			_nodesToUpdate.pop_back();
			const glm::mat4 parentMatrix = parentMatrices.back();
			parentMatrices.pop_back();
			Node3d& node = scene.nodes[currentNodeIndex];

			if (!node.getNeedToUpdate()) { continue; }

			node.updateTransform(parentMatrix);
			node.setNeedToUpdate(false);

			for (const int child : node.getChildren()) {
				_nodesToUpdate.push_back(child);
				parentMatrices.push_back(node.getTransform());
				scene.nodes[child].setNeedToUpdate(true);
			}
		}
	}
	RenderEntityList World::getVisibleEntities(const Frustum& frustum)
	{
		const std::vector<int>& lst = scene.entities[EntityType_Mesh];
		RenderEntityList result;

		intersectTestCount = 0;
#if 0
		IntersectBVH(frustum, bvhRootNodeIdx, result);
#else
		for (const int e : lst) {
			const Node3d& node = scene.nodes[e];
			for (const int p : node.getEntities())
			{
				const MeshData& md = meshes[p];
				const Bounds bounds = md.aabb.Transform(node.getTransform());
				intersectTestCount++;
				if (frustum.Intersects2(bounds)) {
					RenderEntity ent;
					ent.aabb = bounds;
					ent.object.modelMatrix = node.getTransform();
					ent.object.meshIndex = p;
					result.push_back(ent);
				}
			}
		}
#endif
		jsrlib::Info("Intersect tested: %d", intersectTestCount);
		return result;
	}
	void World::updateBVH()
	{
		if (bvhNode.size() < scene.entities[EntityType_Mesh].size()) {
			bvhNode.resize(scene.entities[EntityType_Mesh].size());
			aabbs.resize(scene.entities[EntityType_Mesh].size());
		}

		buildBVH();
	}

	void World::buildBVH()
	{
		bvhNodesUsed = 2;

		for (size_t i = 0; i < scene.entities[EntityType_Mesh].size(); ++i)
		{
			const Node3d& node = scene.nodes[scene.entities[EntityType_Mesh][i]];
			Bounds b;
			for (int e : node.getEntities())
			{
				b << meshes[e].aabb.Transform(node.getTransform());
			}
			aabbs[i] = b;
		}

		BVHNode& root = bvhNode[0];
		root.leftFirst = 0;
		root.primCount = scene.entities[EntityType_Mesh].size();

		UdateNodeBounds(0);
		Subdivide(0);
	}
	void World::UdateNodeBounds(uint32_t nodeIdx)
	{
		BVHNode& node = bvhNode[nodeIdx];
		node.aabb.min() = glm::vec3(std::numeric_limits<float>::max());
		node.aabb.max() = glm::vec3(std::numeric_limits<float>::lowest());
		size_t i;
		for (size_t first = node.leftFirst, i = 0; i < node.primCount; ++i)
		{
			node.aabb << aabbs[first + i];
		}
	}
	void World::Subdivide(uint32_t nodeIdx)
	{
		using namespace glm;

		BVHNode& node = bvhNode[nodeIdx];
		/*
		if (node.primCount <= 2) {
			return;
		}
		*/
		int axis;
		float splitPos;
		float splitCost = FindBestSplitPlane(node, axis, splitPos);
		float nosplitCost = CalculateNodeCost(node);
		if (splitCost >= nosplitCost) return;

		/*
		vec3 extent = node.aabb.max() - node.aabb.min();

		if (extent.y > extent.x) axis = 1;
		if (extent.z > extent[axis]) axis = 2;
		float splitPos = node.aabb.min()[axis] + extent[axis] * 0.5f;
		*/

		size_t i = node.leftFirst;
		size_t j = i + node.primCount - 1;

		while (i <= j)
		{
			if (aabbs[i].GetCenter()[axis] < splitPos)
			{
				++i;
			}
			else 
			{
				std::swap(scene.entities[EntityType_Mesh][i], scene.entities[EntityType_Mesh][j]);
				std::swap(aabbs[i], aabbs[j]);
				--j;
			}
		}

		size_t leftCount = i - node.leftFirst;
		if (leftCount == 0 || leftCount == node.primCount) {
			return;
		}
		
		// create child nodes
		size_t leftChildIndex = bvhNodesUsed++;
		size_t rightChildIndex = bvhNodesUsed++;
		
		if (leftChildIndex >= bvhNode.size() || rightChildIndex >= bvhNode.size()) {
			return;
		}

		bvhNode[leftChildIndex].leftFirst = node.leftFirst;
		bvhNode[leftChildIndex].primCount = leftCount;
		bvhNode[rightChildIndex].leftFirst = i;
		bvhNode[rightChildIndex].primCount = node.primCount - leftCount;
		node.primCount = 0;
		node.leftFirst = (uint32_t)leftChildIndex;

		UdateNodeBounds(leftChildIndex);
		UdateNodeBounds(rightChildIndex);

		Subdivide(leftChildIndex);
		Subdivide(rightChildIndex);
	}
	void World::IntersectBVH(const Frustum& frustum, uint32_t nodeIdx, RenderEntityList& out)
	{
		std::vector<uint32_t> nodeToTest = { nodeIdx };

		while (!nodeToTest.empty())
		{
			const uint32_t _nodeIdx = nodeToTest.back();
			nodeToTest.pop_back();

			BVHNode& node = bvhNode[_nodeIdx];
			intersectTestCount++;
			if (!frustum.Intersects2(node.aabb)) {
				continue;
			}

			if (node.isLeaf())
			{
				for (size_t i = 0; i < node.primCount; ++i) {
					const Node3d& p = scene.nodes[scene.entities[EntityType_Mesh][node.leftFirst + i]];
					for (const int e : p.getEntities()) {
						const MeshData& md = meshes[e];
						Bounds bbox = md.aabb.Transform(p.getTransform());

						intersectTestCount++;
						if (frustum.Intersects2(bbox)) {
							out.emplace_back();
							RenderEntity& ent = out.back();
							ent.aabb = bbox;
							ent.object.modelMatrix = p.getTransform();
							ent.object.meshIndex = e;
						}
					}
				}
			}
			else
			{
				//IntersectBVH(frustum, node.leftChild, out);
				//IntersectBVH(frustum, node.leftChild + 1, out);
				nodeToTest.push_back(node.leftFirst);
				nodeToTest.push_back(node.leftFirst + 1);
			}
		}
	}
	float World::EvaluateSAH(BVHNode& node, int axis, float pos)
	{
		Bounds leftBox, rightBox;
		int leftCount = 0, rightCount = 0;
		for (size_t i(0); i < node.primCount; ++i)
		{
			const Bounds& bbox = aabbs[node.leftFirst + i];
			if (bbox.GetCenter()[axis] < pos)
			{
				leftCount++;
				leftBox.Extend(bbox);
			}
			else
			{
				rightCount++;
				rightBox.Extend(bbox);
			}
		}

		float cost = leftCount * leftBox.area() + rightCount * rightBox.area();
		return cost > 0 ? cost : std::numeric_limits<float>::max();
	}

#define BINS 8
	float World::FindBestSplitPlane(BVHNode& node, int& axis, float& splitPos)
	{
		float bestCost = std::numeric_limits<float>::max();

		struct Bin { Bounds bounds; int primCount = 0; };
		Bin bin[BINS];

		for (int a = 0; a < 3; a++)
		{
			float boundsMin = node.aabb.min()[a];
			float boundsMax = node.aabb.max()[a];

			if (boundsMin == boundsMax) continue;

			float scale = BINS / (boundsMax - boundsMin);

			for (uint32_t i = 1; i < node.primCount; i++)
			{
				int binIdx = std::min(BINS - 1, (int)((aabbs[node.leftFirst + i].GetCenter()[a] - boundsMin) * scale));
				bin[binIdx].primCount++;
				bin[binIdx].bounds.Extend(aabbs[node.leftFirst + i]);

			}

			float leftArea[BINS - 1], rightArea[BINS - 1];
			int leftCount[BINS - 1], rightCount[BINS - 1];

			Bounds leftBox, rightBox;
			int leftSum = 0, rightSum = 0;
			for (int i = 0; i < BINS - 1; i++)
			{
				leftSum += bin[i].primCount;
				leftCount[i] = leftSum;
				leftBox.Extend(bin[i].bounds);
				leftArea[i] = leftBox.area();
				rightSum += bin[BINS - 1 - i].primCount;
				rightCount[BINS - 2 - i] = rightSum;
				rightBox.Extend(bin[BINS - 1 - i].bounds);
				rightArea[BINS - 2 - i] = rightBox.area();
			}

			scale = (boundsMax - boundsMin) / BINS;
			for (int i = 0; i < BINS - 1; i++)
			{
				float planeCost = leftCount[i] * leftArea[i] + rightCount[i] * rightArea[i];
				if (planeCost < bestCost)
					axis = a, splitPos = boundsMin + scale * (i + 1), bestCost = planeCost;
			}
		}
		return bestCost;
	}

	float World::CalculateNodeCost(BVHNode& node)
	{
		return node.primCount * node.aabb.area();
	}
	void World::RefitBVH()
	{
		for (int i = int(bvhNodesUsed) - 1; i >= 0; --i)
		{
			if (i == 1) { continue; }
			BVHNode& node = bvhNode[i];
			if (node.isLeaf())
			{
				UdateNodeBounds(i);
				continue;
			}
			BVHNode& leftChild = bvhNode[node.leftFirst];
			BVHNode& rightChild = bvhNode[node.leftFirst + 1];
			node.aabb.min() = glm::min(leftChild.aabb.min(), rightChild.aabb.min());
			node.aabb.max() = glm::max(leftChild.aabb.max(), rightChild.aabb.max());
		}
	}
}