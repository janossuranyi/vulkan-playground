#pragma once

#include "pch.h"

namespace jsr {

	static const float epsilon = 1e-5f;

	/*
	Implements an AABB
	*/
	class Bounds;
	class Sphere
	{
	public:
		Sphere() = default;
		Sphere(const glm::vec3& center, float r);
		bool Contains(const glm::vec3& p) const;
		float Distance(const glm::vec3& p) const;
		bool RayIntersect(const glm::vec3& start, const glm::vec3& dir, float& tnear, float& tfar) const;
		bool LineIntersect(const glm::vec3& start, const glm::vec3& end, float& tnear, float& tfar) const;
		bool Intersect(const Sphere& s) const; 
		bool Intersect(const Bounds& s) const;
		glm::vec3 GetCenter() const;
		float GetRadius() const;
	private:
		glm::vec3 center;
		float radius;
		float radius2;
	};

	class Bounds
	{
	public:
		Bounds();
		Bounds(const glm::vec3& a, const glm::vec3& b);
		Bounds& Extend(const glm::vec3& v);
		Bounds& Extend(const Bounds& other);
		Bounds& operator<<(const glm::vec3& v);
		Bounds& operator<<(const Bounds& other);
		bool Contains(const glm::vec3& p) const;
		float GetRadius() const;
		glm::vec3 ClosestPoint(const glm::vec3& p);
		glm::vec3 GetCenter() const;
		glm::vec3 GetMin() const;
		glm::vec3 GetMax() const;
		glm::vec3 operator[](size_t index) const;
		glm::vec3& operator[](size_t index);
		glm::vec3& Min();
		glm::vec3& Max();
		glm::vec3 GetPositiveVertex(const glm::vec3& N) const;
		glm::vec3 GetNegativeVertex(const glm::vec3& N) const;
		const glm::vec3& Min() const;
		const glm::vec3& Max() const;
		std::vector<glm::vec3> GetCorners() const;
		std::vector<glm::vec4> GetHomogenousCorners() const;
		Sphere GetSphere() const;
		Bounds Transform(const glm::mat4& trans) const;
		float area() const;
		/*
		compute the near and far intersections of the cube(stored in the x and y components) using the slab method
		no intersection means vec.x > vec.y (really tNear > tFar)
		*/
		bool RayIntersect(const glm::vec3& start, const glm::vec3& invdir, float& tmin, float& tmax);
		bool Intersects(const Sphere& s);
		bool operator==(const Bounds& other) const;
		bool operator!=(const Bounds& other) const;
		Bounds operator+(const Bounds& other) const;
		bool Empty() const;
	private:
		glm::vec3 b[2];
	};
}