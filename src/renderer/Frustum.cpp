#include <array>
#include "./Frustum.h"
#include "./Bounds.h"
#include "../jobsys.h"

namespace jsr {

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


	ForwardPlusFrustum::ForwardPlusFrustum(int tileX, int tileY, const glm::mat4& view, const glm::mat4& proj, float zNear, float zFar)
	{
		const int tileNumber = tileX * tileY;

		m_lightList = new int[tileNumber * MaxLights];
		m_frustums = new frustumPlanes_t[tileNumber];
		m_visibleLightOffset = new std::atomic_uint[tileNumber];

		tilex = tileX;
		tiley = tileY;
		memset(m_lightList, -1, tileNumber * sizeof(*m_lightList));
		memset(m_visibleLightOffset, 0, tileNumber * sizeof(*m_visibleLightOffset));

		const glm::mat4 vp = proj * view;
		for (int y = 0; y < tiley; ++y)
		{
			for (int x = 0; x < tilex; ++x)
			{
				const int index = y * tilex + x;
				assert(index < tileNumber);
				frustumPlanes_t& frustum = m_frustums[index];

				glm::vec2 negativeStep = (2.0f * glm::vec2(x, y)) / glm::vec2(tileX, tileY);
				glm::vec2 positiveStep = (2.0f * glm::vec2(glm::ivec2(x, y) + glm::ivec2(1, 1))) / glm::vec2(tileX, tileY);
				// Set up starting values for planes using steps and min and max z values
				frustum.planes[0] = glm::vec4(1.0, 0.0, 0.0, 1.0 - negativeStep.x); // Left
				frustum.planes[1] = glm::vec4(-1.0, 0.0, 0.0, -1.0 + positiveStep.x); // Right
				frustum.planes[2] = glm::vec4(0.0, 1.0, 0.0, 1.0 - negativeStep.y); // Bottom
				frustum.planes[3] = glm::vec4(0.0, -1.0, 0.0, -1.0 + positiveStep.y); // Top
				frustum.planes[4] = glm::vec4(0.0, 0.0, -1.0, -zNear); // Near
				frustum.planes[5] = glm::vec4(0.0, 0.0, 1.0, zFar); // Far

				// Transform the first four planes
				for (int i = 0; i < 4; i++) {
					frustum.planes[i] = frustum.planes[i] * vp;
					frustum.planes[i] /= length(glm::vec3(frustum.planes[i]));
				}

				// Transform the depth planes
				frustum.planes[4] = frustum.planes[4] * view;
				frustum.planes[4] /= length(glm::vec3(frustum.planes[4]));
				frustum.planes[5] = frustum.planes[5] * view;
				frustum.planes[5] /= length(glm::vec3(frustum.planes[5]));

			}
		}
	}
	ForwardPlusFrustum::~ForwardPlusFrustum()
	{
		delete[] m_lightList;
		delete[] m_frustums;
		delete[] m_visibleLightOffset;
	}

	uint32_t ForwardPlusFrustum::addLight(int id, glm::vec3 pos, float radius)
	{
		uint32_t tileNumber = 0;
		for (int y = 0; y < tiley; ++y)
		{
			float distance = 0.0f;
			for (int x = 0; x < tilex; ++x)
			{
				const int index = y * tilex + x;
				const frustumPlanes_t& frustum = m_frustums[index];
				for (int j = 0; j < 6; ++j)
				{
					distance = glm::dot(glm::vec4(pos, 1), frustum.planes[j]) + radius;
					if (distance <= 0) break;
				}
				if (distance > 0.0f) {
					uint32_t offset = m_visibleLightOffset[index].fetch_add(1);
					assert(offset < MaxLights);
					m_lightList[MaxLights * index + offset] = id;
					++tileNumber;
				}
			}
		}
		return tileNumber;
	}


	static glm::vec4 clipToView(const glm::vec4& clip, const glm::mat4& inverseProjection) {
		//View space transform
		vec4 view = inverseProjection * clip;

		//Perspective projection
		view = view / view.w;

		return view;
	}

	static vec4 screen2View(const vec4& screen, const vec2& screenDimensions, const mat4& inverseProjection) {

		//Convert to NDC
		vec2 texCoord = glm::vec2(screen) / screenDimensions;

		//Convert to clipSpace
		// vec4 clip = vec4(vec2(texCoord.x, 1.0 - texCoord.y)* 2.0 - 1.0, screen.z, screen.w);
		vec4 clip = vec4(vec2(texCoord.x, texCoord.y) * 2.0f - 1.0f, screen.z, screen.w);
		//Not sure which of the two it is just yet
		
		return clipToView(clip, inverseProjection);
	}


	//Creates a line from the eye to the screenpoint, then finds its intersection
	//With a z oriented plane located at the given distance to the origin
	static vec3 lineIntersectionToZPlane(const vec3& A, const vec3& B, float zDistance) {
		//Because this is a Z based normal this is fixed
		vec3 normal = vec3(0.0f, 0.0f, 1.0f);
/*
* ( p - p0 ) dot N = 0
(L dot N)*d + (L0 - p0) dot N = 0
d = (p0 - L0) dot N / (L dot N)
*/

		vec3 ab = B - A;

		//Computing the intersection length for the line and the plane
		float t = (zDistance - dot(normal, A)) / dot(normal, ab);

		//Computing the actual xyz position of the point along the line
		vec3 result = A + t * ab;

		return result;
	}

	void ClusteredFrustum::setup(const glm::uvec2 screenDimension, const glm::mat4& invProj, float zNear, float zFar)
	{
		using namespace glm;

		m_screenDim = screenDimension;

		const vec3 eyePos = vec3(0.0f);
		uvec2 utileSizePx = uvec2((uint32_t)(m_screenDim.x + GRID_DIM.x - 1) / (float)GRID_DIM.x, (uint32_t)(m_screenDim.y + GRID_DIM.y - 1) / (float)GRID_DIM.y);
		vec2 tileSizePx = vec2(utileSizePx);

		m_cluster.resize(GRID_DIM.x * GRID_DIM.y * GRID_DIM.z);
		m_lightGrid.resize(GRID_DIM.x * GRID_DIM.y * GRID_DIM.z);

		size_t index = 0;
		const float zFarPerzNear = zFar / zNear;
		m_globalIndexCount = 0;

		for (uint32_t z = 0; z < GRID_DIM.z; ++z)
		{
			for (uint32_t y = 0; y < GRID_DIM.y; ++y)
			{
				for (uint32_t x = 0; x < GRID_DIM.x; ++x)
				{
					vec4 maxPointSS = vec4(vec2(x + 1, y + 1) * tileSizePx, -1.0f, 1.0f);
					vec4 minPointSS = vec4(vec2(x, y) * tileSizePx, -1.0f, 1.0f);
					vec3 maxPointVS = vec3(screen2View(maxPointSS, vec2(screenDimension), invProj));
					vec3 minPointVS = vec3(screen2View(minPointSS, vec2(screenDimension), invProj));

					//Near and far values of the cluster in view space
					float tileNear	= -zNear * pow(zFarPerzNear,	   z / float(GRID_DIM.z));
					float tileFar	= -zNear * pow(zFarPerzNear, (z + 1) / float(GRID_DIM.z));

					//Finding the 4 intersection points made from the maxPoint to the cluster near/far plane
					vec3 minPointNear	= lineIntersectionToZPlane(eyePos, minPointVS, tileNear);
					vec3 minPointFar	= lineIntersectionToZPlane(eyePos, minPointVS, tileFar);
					vec3 maxPointNear	= lineIntersectionToZPlane(eyePos, maxPointVS, tileNear);
					vec3 maxPointFar	= lineIntersectionToZPlane(eyePos, maxPointVS, tileFar);
					m_cluster[index].minPoint = vec4(min(min(minPointNear, minPointFar), min(maxPointNear, maxPointFar)),0.0f);
					m_cluster[index].maxPoint = vec4(max(max(minPointNear, minPointFar), max(maxPointNear, maxPointFar)),0.0f);
					++index;
				}
			}
		}
	}

	void ClusteredFrustum::setMaxLights(uint32_t n)
	{
		m_globalLightIndexList.resize(n);
	}
	bool ClusteredFrustum::testSphereAABB(const std::vector<Light>& lights, uint32_t light, uint32_t tile) {
		const float radius = lights[light].range;
		const vec3 center = vec3(m_viewMatrix * vec4(lights[light].position,1.0f));

		const VolumeTileAABB currentCell = m_cluster[tile];
		m_cluster[tile].maxPoint[3] = tile;

		Sphere lightSphere(center, radius);
		Bounds aabb(vec3(currentCell.minPoint), vec3(currentCell.maxPoint));

		return aabb.Intersects(lightSphere);
	}

	void ClusteredFrustum::cullLights(const std::vector<Light>& lights, const glm::mat4& viewMatrix)
	{
		m_globalIndexCount = 0;
		setMaxLights(lights.size());
		jsrlib::counting_semaphore counter;

		for (size_t i = 0; i < m_cluster.size(); ++i) {
			jobsys.submitJob([this, i, &viewMatrix, &lights](int threadId)
				{
					cullLights(lights, i, viewMatrix);
				}, &counter);
		}

		counter.wait();
	}

	void ClusteredFrustum::cullLights(const std::vector<Light>& lights, size_t cellIndex, const glm::mat4& viewMatrix)
	{
		uint32_t visibleLightCount = 0;
		uint32_t visibleLightIndices[256];
		
		VolumeTileAABB& cell = m_cluster[cellIndex];
		cell.maxPoint[3] = (float)cellIndex;

		for (size_t i = 0; i < lights.size(); ++i)
		{
			const float radius = lights[i].range;
			const vec3 center = vec3(m_viewMatrix * vec4(lights[i].position, 1.0f));
			Sphere lightSphere(center, radius);
			Bounds aabb(vec3(cell.minPoint), vec3(cell.maxPoint));
			if (aabb.Intersects(lightSphere)) {
				assert(visibleLightCount < 256);
				visibleLightIndices[visibleLightCount++] = (uint32_t) i;
			}
		}
		if (visibleLightCount > 0) {
			uint32_t offset = m_globalIndexCount.fetch_add(visibleLightCount);
			m_lightGrid[cellIndex] = ((offset & (1UL << 24) - 1) << 24) | (visibleLightCount & 255);
		}
		else {
			m_lightGrid[cellIndex] = 0;
		}
	}

}