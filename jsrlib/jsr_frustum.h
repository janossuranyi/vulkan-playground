#pragma once
#include "pch.h"
#include "jsr_bounds.h"

namespace jsrlib {

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
}
