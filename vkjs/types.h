#ifndef VKJS_TYPES_H_
#define VKJS_TYPES_H_

#include <cinttypes>

namespace vkjs {

	typedef uint64_t	u64;
	typedef int64_t		s64;
	typedef uint32_t	u32;
	typedef int32_t		s32;
	typedef uint16_t	u16;
	typedef int16_t		s16;
	typedef uint8_t		u8;
	typedef int8_t		s8;
	typedef uint32_t	uint;

	template<int L, typename T> struct vec;

	template<typename T>
	struct vec<2, T> {
		union {
			struct { T x, y; };
			struct { T r, g; };
			struct { T s, t; };
		};
		constexpr vec();
		constexpr vec(T _x, T _y);
		constexpr vec(const vec&);
		constexpr size_t length() const { return 2; }
		constexpr T operator[](size_t i) const;
		constexpr T& operator[](size_t i);
		const T& operator[](size_t i) const;
	};


	template<typename T> 
	constexpr vec<2, T>::vec() { x = y = static_cast<T>(0);  }
	template<typename T>
	constexpr vec<2, T>::vec(T _x, T _y) : x(_x), y(_y) {}
	template<typename T>
	constexpr vec<2, T>::vec(const vec<2, T>& o) : x(o.x), y(o.y) {}
	template<typename T>
	constexpr T vec<2, T>::operator[](size_t i) const {
		switch (i) {
		case 0: return x;
		case 1: return y;
		default:
			return static_cast<T>(0);
		}
	}
	template<typename T>
	constexpr T& vec<2, T>::operator[](size_t i) {
		switch (i) {
		case 0: return x;
		case 1: return y;
		default:
			assert(false);
		}
	}
	template<typename T>
	const T& vec<2, T>::operator[](size_t i) const {
		return operator[](i);
	}
}
#endif // !VKJS_TYPES_H_
