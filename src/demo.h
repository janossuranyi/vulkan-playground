#pragma once

#include "pch.h"

inline void hash_combine(size_t& seed, size_t hash)
{
	hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
	seed ^= hash;
}

namespace jsr {
	namespace utils {
		void printVector(const glm::vec4& v);
		bool pointInFrustum(const glm::vec4& p);
		bool pointInFrustum(const glm::vec4& p, const glm::vec4 planes[6]);

		inline float dot4(const glm::vec4& a, const glm::vec4& b) {
			return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		}

		inline float dot3(const glm::vec3& a, const glm::vec3& b) {
			return a.x * b.x + a.y * b.y + a.z * b.z;
		}
		inline float linearZ(float depth, float znear, float zfar) {

			return znear * zfar / (zfar + depth * (znear - zfar));
		}

		inline glm::vec4 multMatrix(const glm::mat4& m, const glm::vec4& v) {
			glm::vec4 r;
			r.x = m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * v.w;
			r.y = m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * v.w;
			r.z = m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * v.w;
			r.w = m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3] * v.w;

			return r;
		}

		inline glm::vec4 multMatrix(const glm::vec4& v, const glm::mat4& m) {
			glm::vec4 r;
			r.x = dot4(m[0], v);
			r.y = dot4(m[1], v);
			r.z = dot4(m[2], v);
			r.w = dot4(m[3], v);

			return r;
		}

	}

}