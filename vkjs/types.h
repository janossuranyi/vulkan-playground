#ifndef VKJS_TYPES_H_
#define VKJS_TYPES_H_

#include <cinttypes>
#include <glm/glm.hpp>

namespace vkjs{

typedef uint64_t	u64;
typedef int64_t		s64;
typedef uint32_t	u32;
typedef int32_t		s32;
typedef uint16_t	u16;
typedef int16_t		s16;
typedef uint8_t		u8;
typedef int8_t		s8;

typedef uint32_t uint;
typedef struct uvec2 {
	union {
		struct {
			uint x;
			uint y;
		};
		struct {
			uint r;
			uint g;
		};
		struct {
			uint s;
			uint t;
		};
	};
} uvec2;
typedef struct uvec3 {
	union {
		struct {
			uint x;
			uint y;
			uint z;
		};
		struct {
			uint r;
			uint g;
			uint b;
		};
		struct {
			uint s;
			uint t;
			uint p;
		};
	};
} uvec3;

typedef struct uvec4 {
	union {
		struct {
			uint x;
			uint y;
			uint z;
			uint w;
		};
		struct {
			uint r;
			uint g;
			uint b;
			uint a;
		};
		struct {
			uint s;
			uint t;
			uint p;
			uint q;
		};
	};
} uvec4;

typedef struct vec2 {
	union {
		struct {
			float x;
			float y;
		};
		struct {
			float r;
			float g;
		};
		struct {
			float s;
			float t;
		};
	};
	vec2() { x = y = 0.0f; }
	vec2(const vec2&) = default;
	inline vec2(const glm::vec4& v) { x = v.x; y = v.y; }
	inline vec2(const glm::vec3& v) { x = v.x; y = v.y; }
	inline vec2(const glm::vec2& v) { x = v.x; y = v.y; }
	inline vec2(float f) { x = f; y = f; }
	inline vec2(float f0, float f1) { x = f0; y = f1; }
	inline operator glm::vec4() { return glm::vec4(x, y, 0.0f, 1.0f); }
	inline operator glm::vec3() { return glm::vec3(x, y, 0.0f); }
	inline operator glm::vec2() { return glm::vec2(x, y); }

} vec2;

typedef struct vec3 {
	union {
		struct {
			float x;
			float y;
			float z;
		};
		struct {
			float r;
			float g;
			float b;
		};
		struct {
			float s;
			float t;
			float p;
		};
	};
	vec3() { x = y = z = 0.0f; }
	vec3(const vec3&) = default;
	inline vec3(const glm::vec4& v) { x = v.x; y = v.y; z = v.z; }
	inline vec3(const glm::vec3& v) { x = v.x; y = v.y; z = v.z; }
	inline vec3(const glm::vec2& v) { x = v.x; y = v.y; z = 0.0f; }
	inline vec3(float f) { x = f; y = f; z = f; }
	inline vec3(float f0, float f1) { x = f0; y = f1; z = 0.0f; }
	inline vec3(float f0, float f1, float f2) { x = f0; y = f1; z = f2; }
	inline operator glm::vec4() { return glm::vec4(x, y, z, 1.0f); }
	inline operator glm::vec3() { return glm::vec3(x, y, z); }
	inline operator glm::vec2() { return glm::vec2(x, y); }
} vec3;


typedef struct vec4 {
	union {
		struct {
			float x;
			float y;
			float z;
			float w;
		};
		struct {
			float r;
			float g;
			float b;
			float a;
		};
		struct {
			float s;
			float t;
			float p;
			float q;
		};
	};
	vec4() { x = y = z = 0.0f; w = 1.0f; }
	vec4(const vec4&) = default;
	inline vec4(const glm::vec4& v) { x = v.x; y = v.y; z = v.z; w = v.w; }
	inline vec4(const glm::vec3& v) { x = v.x; y = v.y; z = v.z; w = 1.0f; }
	inline vec4(const glm::vec2& v) { x = v.x; y = v.y; z = 0.0f; w = 1.0f; }
	inline vec4(float f) { x = f; y = f; z = f; w = f; }
	inline vec4(float f0, float f1) { x = f0; y = f1; z = 0.0f; w = 1.0f; }
	inline vec4(float f0, float f1, float f2, float f3) { x = f0; y = f1; z = f2; w = f3; }
	inline operator glm::vec4() { return glm::vec4(x, y, z, w); }
	inline operator glm::vec3() { return glm::vec3(x, y, z); }
	inline operator glm::vec2() { return glm::vec2(x, y); }
} vec4;


}
#endif // !VKJS_TYPES_H_
