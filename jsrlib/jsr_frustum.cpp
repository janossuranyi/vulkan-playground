#include <array>
#include "jsr_frustum.h"
#include "jsr_bounds.h"

namespace jsrlib {

	using namespace glm;

	Frustum::Frustum(const glm::mat4& vp)
	{
		// We have to access rows easily, so transpose.
		const mat4 tvp = glm::transpose(vp);
		const mat4 ivp = glm::inverse(vp);
		// Based on Fast Extraction of Viewing Frustum Planes from the World- View-Projection Matrix, G. Gribb, K. Hartmann
		// (https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf)
		planes[0] = tvp[3] + tvp[0];
		planes[1] = tvp[3] - tvp[0];
		planes[2] = tvp[3] - tvp[1];
		planes[3] = tvp[3] + tvp[1];
		planes[4] = tvp[3] + tvp[2];
		planes[5] = tvp[3] - tvp[2];

		//planes[4] = tvp[2];

		for (int i = 0; i < 6; ++i)
		{
			planes[i] /= length(vec3(planes[i]));
		}

		// Reproject the 8 corners of the frustum from NDC to world space.
		static const std::array<vec4, 8> ndcCorner = {
			glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f),
			glm::vec4(-1.0f, -1.0f,  1.0f, 1.0f),
			glm::vec4(-1.0f,  1.0f, -1.0f, 1.0f),
			glm::vec4(-1.0f,  1.0f,  1.0f, 1.0f),
			glm::vec4(1.0f, -1.0f, -1.0f, 1.0f),
			glm::vec4(1.0f, -1.0f,  1.0f, 1.0f),
			glm::vec4(1.0f,  1.0f, -1.0f, 1.0f),
			glm::vec4(1.0f,  1.0f,  1.0f, 1.0f) };

		for (int i = 0; i < 8; ++i) {
			const vec4 corn = ivp * ndcCorner[i];
			corners[i] = vec3(corn) / corn[3];
		}
	}

	bool Frustum::Intersects(const Bounds& box) const
	{
		const auto sphere = box.GetSphere();
		const auto center = vec4(sphere.GetCenter(), 1.0f);
		const auto range = sphere.GetRadius();

		for (int pid = 0; pid < 6; ++pid)
		{
			const auto dist = dot(planes[pid], center);
			if (dist + range < 0.0f)
			{
				return false;
			}
		}

		const auto corners = box.GetHomogenousCorners();
		// For each of the frustum planes, check if all points are in the "outside" half-space.
		for (int pid = 0; pid < 6; ++pid) {
			if ((glm::dot(planes[pid], corners[0]) < 0.0f) &&
				(glm::dot(planes[pid], corners[1]) < 0.0f) &&
				(glm::dot(planes[pid], corners[2]) < 0.0f) &&
				(glm::dot(planes[pid], corners[3]) < 0.0f) &&
				(glm::dot(planes[pid], corners[4]) < 0.0f) &&
				(glm::dot(planes[pid], corners[5]) < 0.0f) &&
				(glm::dot(planes[pid], corners[6]) < 0.0f) &&
				(glm::dot(planes[pid], corners[7]) < 0.0f)) {
				return false;
			}
		}
		/// \todo Implement frustum corner checks to weed out more false positives.
		return true;
	}

	bool Frustum::Intersects(const glm::vec3& point) const
	{
		for (int pid = 0; pid < 6; ++pid)
		{
			const float pos = planes[pid].w;
			const vec3 normal = vec3(planes[pid]);
			const auto dist = pos + dot(normal, point);
			if (dist < 0.0f)
			{
				return false;
			}
		}

		return true;
	}

	bool Frustum::Intersects2(const Bounds& box) const
	{
		bool result = true; // inside

		const auto sphere = box.GetSphere();
		const auto center = vec4(sphere.GetCenter(), 1.0f);
		const auto range = sphere.GetRadius();

		for (int pid = 0; pid < 6; ++pid)
		{
			const auto dist = dot(planes[pid], center);
			if (dist + range < 0.0f)
			{
				return false;
			}
		}

		for (int i = 0; i < 6; i++)
		{
			const float pos = planes[i].w;
			const vec3 normal = vec3(planes[i]);

			if (dot(normal, box.GetPositiveVertex(normal)) + pos < 0.0f)
			{
				return false;	// outside
			}

			if (dot(normal, box.GetNegativeVertex(normal)) + pos < 0.0f)
			{
				result = true;	// intersect
			}
		}

		return result;
	}

	void Frustum::GetCorners(std::vector<glm::vec3>& v)
	{
		for (int i = 0; i < 8; ++i) v.push_back(corners[i]);
	}

	const glm::vec4* Frustum::GetPlanes() const
	{
		return planes;
	}
}