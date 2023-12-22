#pragma once
#include "pch.h"
#include "renderer/Bounds.h"
#include "renderer/Light.h"

namespace jsr {

	struct plane_t
	{
		glm::vec3	normal;
		float		distance;

		plane_t() : normal(glm::vec3(0, 0, 1)), distance(0.0f) {}
		plane_t(const glm::vec3& n, float d) : normal(glm::normalize(n)), distance(d) {}
		float PointDistance(const glm::vec3& p) const { return glm::dot(normal, p) + distance; }
		glm::vec4 GetVec4() const { return glm::vec4(normal, distance); }
	};

	class Bounds;
	class Frustum
	{
	public:
		Frustum(const glm::mat4& vp);
		bool Intersects(const Bounds& box) const;
		bool Intersects(const glm::vec3& point) const;
		bool Intersects2(const Bounds& box) const;
		void GetCorners(std::vector<glm::vec3>& v);
		const glm::vec4* GetPlanes() const;
	private:
		glm::vec4	planes[6];
		glm::vec3	corners[8];
	};

	class ForwardPlusFrustum
	{
	public:
		const int MaxLights = 1024;
		ForwardPlusFrustum(int tileX, int tileY, const glm::mat4& view, const glm::mat4& proj, float zNear, float zFar);
		virtual ~ForwardPlusFrustum();
		uint32_t addLight(int id, glm::vec3 pos, float radius);
	private:
		struct frustumPlanes_t { glm::vec4 planes[6]; };
		int tilex, tiley;
		frustumPlanes_t* m_frustums;
		int* m_lightList;
		std::atomic_uint* m_visibleLightOffset;
	};

	struct VolumeTileAABB {
		glm::vec4 minPoint;
		glm::vec4 maxPoint;
	};

	class ClusteredFrustum {
	public:
		const glm::uvec3 GRID_DIM{16u, 8u, 24u};
		void setup(const glm::uvec2 screenDimension, const glm::mat4& invProj, float znear, float zfar);
		void setMaxLights(uint32_t n);
		void cullLights(const std::vector<Light>& lights, const glm::mat4& viewMatrix);
	private:
		void cullLights(const std::vector<Light>& lights, size_t cellIndex, const glm::mat4& viewMatrix);
		bool testSphereAABB(const std::vector<Light>& lights, uint32_t light, uint32_t tile);

		glm::uvec2 m_screenDim;
		std::vector<VolumeTileAABB> m_cluster;
		std::vector<uint32_t> m_lightGrid;	// offset/count
		std::vector<uint32_t> m_globalLightIndexList;
		std::atomic_uint m_globalIndexCount;
		glm::mat4 m_viewMatrix;
	};
}
