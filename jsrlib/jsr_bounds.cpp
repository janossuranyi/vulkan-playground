#include <glm/glm.hpp>
#include <limits>
#include "jsr_bounds.h"

namespace jsrlib {

	using namespace glm;

	Sphere::Sphere(const glm::vec3& center_, float r) :
		center(center_),
		radius(r),
		radius2(r*r)
	{}
	bool Sphere::Contains(const glm::vec3& p) const
	{
		const auto v = center - p;
		return glm::dot(v, v) <= radius2;
	}
	float Sphere::Distance(const glm::vec3& p) const
	{
		return glm::length(p - center) - radius;
	}
	bool Sphere::RayIntersect(const glm::vec3& start, const glm::vec3& dir, float& tnear, float& tfar) const
	{
		/*
		* https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection.html
		*/

		vec3 const L = center - start;
		float const tca = dot(L, dir);
		
		if (tca < 0.0f) return false;
		
		float const d2 = dot(L, L) - tca * tca;
		if (d2 > radius2) return false;

		float const thc = sqrt(radius2 - d2);
		tnear = tca - thc;
		tfar  = tca + thc;

		tnear = min(tnear, tfar);
		tfar  = max(tnear, tfar);

		if (tnear < 0.0f)
		{
			tnear = tfar;
			if (tnear < 0.0f) return false;
		}

		return true;
	}
	bool Sphere::LineIntersect(const glm::vec3& start, const glm::vec3& end, float& tnear, float& tfar) const
	{
		const glm::vec3 v = end - start;

		if (RayIntersect(start, v, tnear, tfar))
		{
			if (tnear < 0.0f || tfar < 0.0f)
			{
				// p0 is inside the sphere
				tnear = 0.0f;
				tfar  = 0.0f;
				return true;
			}

			const float l = length(v);
			if ((l - tnear) >= 0.0f || (l - tfar) >= 0.0f)
			{
				return true;
			}
		}

		return false;
	}
	bool Sphere::Intersect(const Sphere& s) const
	{
		const vec3 c = s.center - center;
		const float distance2 = dot(c, c);
		const float sr = s.radius + radius;

		return distance2 <= (sr * sr);
	}
	glm::vec3 Sphere::GetCenter() const
	{
		return center;
	}
	float Sphere::GetRadius() const
	{
		return radius;
	}
	bool Sphere::Intersect(const Bounds& box) const
	{
		// get box closest point to sphere center by clamping
		vec3  const v = glm::max(box.min(), glm::min(center, box.max()));
		float const d = dot(v, v);

		return d < radius2;
	}

	Bounds::Bounds() : _min( vec3(std::numeric_limits<float>::max()) ), _max( vec3(std::numeric_limits<float>::lowest()) ) {}

	Bounds::Bounds(const glm::vec3& a, const glm::vec3& b) :
		_min{ glm::min(a, b) }, _max{ glm::max(a, b) } {}

	Bounds& Bounds::Extend(const glm::vec3& v)
	{
		_min = glm::min(_min, v);
		_max = glm::max(_max, v);

		return *this;
	}

	Bounds& Bounds::Extend(const Bounds& other)
	{
		_min = glm::min(_min, other.min());
		_max = glm::max(_max, other.max());

		return *this;
	}

	Bounds& Bounds::operator<<(const glm::vec3& v)
	{
		return Extend(v);
	}
	Bounds& Bounds::operator<<(const Bounds& other)
	{
		return Extend(other);
	}
	bool Bounds::Contains(const glm::vec3& p) const
	{
		return all(greaterThanEqual(p, _min)) && all(lessThanEqual(p, _max));
	}
	float Bounds::GetRadius() const
	{
		return length(_max - _min) * 0.5f;
	}
	glm::vec3 Bounds::ClosestPoint(const glm::vec3& p)
	{
		glm::vec3 r = p;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (p[axis] > _max[axis]) {
				r[axis] = _max[axis];
			}
			else if (p[axis] < _min[axis]) {
				r[axis] = _min[axis];
			}
		}
		return r;
	}
	glm::vec3 Bounds::GetCenter() const
	{
		return (_min + _max) * 0.5f;
	}
	glm::vec3 Bounds::GetMin() const
	{
		return _min;
	}
	glm::vec3 Bounds::GetMax() const
	{
		return _max;
	}
	std::vector<glm::vec3> Bounds::GetCorners() const
	{
		return {
			vec3(_min[0], _min[1], _min[2]),
			vec3(_min[0], _min[1], _max[2]),
			vec3(_min[0], _max[1], _min[2]),
			vec3(_min[0], _max[1], _max[2]),
			vec3(_max[0], _min[1], _min[2]),
			vec3(_max[0], _min[1], _max[2]),
			vec3(_max[0], _max[1], _min[2]),
			vec3(_max[0], _max[1], _max[2]) };
	}
	std::vector<glm::vec4> Bounds::GetHomogenousCorners() const
	{
		return {
			vec4(_min[0], _min[1], _min[2], 1.0f),
			vec4(_min[0], _min[1], _max[2], 1.0f),
			vec4(_min[0], _max[1], _min[2], 1.0f),
			vec4(_min[0], _max[1], _max[2], 1.0f),
			vec4(_max[0], _min[1], _min[2], 1.0f),
			vec4(_max[0], _min[1], _max[2], 1.0f),
			vec4(_max[0], _max[1], _min[2], 1.0f),
			vec4(_max[0], _max[1], _max[2], 1.0f) };
	}	
	bool Bounds::RayIntersect(const glm::vec3& start, const glm::vec3& invDir, float& tnear, float& tfar)
	{

		// fast slab method

		// where the ray intersects this line
		// O + tD = Bmin
		// O + tD = Bmax
		// tMin = (B0 - O) / D
		// tMax = (B1 - O) / D
		vec3 const tMin = (_min - start) * invDir;
		vec3 const tMax = (_max - start) * invDir;

		vec3 const t1 = glm::min(tMin, tMax);
		vec3 const t2 = glm::max(tMin, tMax);
		tnear = glm::max(glm::max(t1.x, t1.y), t1.z);
		tfar  = glm::min(glm::min(t2.x, t2.y), t2.z);

		return tfar >= tnear;
	}
	bool Bounds::Intersects(const Sphere& s)
	{
		const glm::vec3 center = s.GetCenter();
		const glm::vec3 closes_point = ClosestPoint(center);
		const glm::vec3 difference_vec = center - closes_point;
		const float d2 = glm::dot(difference_vec, difference_vec);
		float r2 = s.GetRadius();
		r2 *= r2;

		return d2 < r2;
	}
	Bounds Bounds::Transform(const glm::mat4& trans) const
	{
		Bounds b{};
		auto corners = GetHomogenousCorners();
		b._min = vec3(trans * corners[0]);
		b._max = b.min();
		for (auto i = 1; i < 8; ++i)
		{
			b << vec3(trans * corners[i]);
		}

		return b;
	}
	float Bounds::area() const
	{
		const vec3 e = _max - _min;
		return e.x * e.y + e.y * e.z + e.z * e.x;
	}
	glm::vec3 Bounds::operator[](size_t index) const
	{
		assert(index < 2);
		switch (index) {
		case 0: return _min;
		case 1: return _max;
		}
	}
	glm::vec3& Bounds::operator[](size_t index)
	{
		assert(index < 2);
		switch (index) {
		case 0: return _min;
		case 1: return _max;
		}
	}
	glm::vec3& Bounds::min()
	{
		return _min;
	}
	glm::vec3& Bounds::max()
	{
		return _max;
	}
	glm::vec3 Bounds::GetPositiveVertex(const glm::vec3& N) const
	{
		glm::vec3 v = _min;
		if (N.x >= 0.0f) v.x = _max.x;
		if (N.y >= 0.0f) v.y = _max.y;
		if (N.z >= 0.0f) v.z = _max.z;

		return v;
	}
	glm::vec3 Bounds::GetNegativeVertex(const glm::vec3& N) const
	{
		glm::vec3 v = _max;
		if (N.x >= 0.0f) v.x = _min.x;
		if (N.y >= 0.0f) v.y = _min.y;
		if (N.z >= 0.0f) v.z = _min.z;

		return v;
	}
	Sphere Bounds::GetSphere() const
	{
		const auto center = 0.5f * (_min + _max);
		const auto radius = glm::length(_max - center);

		return { center,radius };
	}
	const glm::vec3& Bounds::min() const
	{
		return _min;
	}
	const glm::vec3& Bounds::max() const
	{
		return _max;
	}
	bool Bounds::operator==(const Bounds& other) const
	{
		return _min == other._min && _max == other._max;
	}
	bool Bounds::operator!=(const Bounds& other) const
	{
		return !(operator==(other));
	}
	Bounds Bounds::operator+(const Bounds& other) const
	{
		Bounds result{};
		result._min = glm::min(_min, other._min);
		result._max = glm::max(_max, other._max);

		return result;
	}
	bool Bounds::Empty() const
	{
		return _min.x == std::numeric_limits<float>::max();
	}
}