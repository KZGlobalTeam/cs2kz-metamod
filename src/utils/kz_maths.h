#pragma once

#include "mathlib/mathlib.h"
#include "mathlib/vector4d.h"

#define KZM_PI   3.141592653589793238462643383279502884197169399f
#define KZM_PI_2 1.570796326794896619231321691639751442098584699f
#define KZM_PI_4 0.7853981633974483096156608458198757210492923f

#define KZM_PI64   3.141592653589793238462643383279502884197169399
#define KZM_PI64_2 1.570796326794896619231321691639751442098584699
#define KZM_PI64_4 0.7853981633974483096156608458198757210492923

#define KZM_EULER   2.7182818284590452353602874713527f
#define KZM_EULER64 2.7182818284590452353602874713527

#define KZM_RADTODEG(value) ((value) * 180.0f / KZM_PI)
#define KZM_DEGTORAD(value) ((value) * KZM_PI / 180.0f)

#define KZM_MIN(a, b)            ((a) > (b) ? (b) : (a))
#define KZM_MAX(a, b)            ((a) < (b) ? (b) : (a))
#define KZM_MIN3(a, b, c)        (KZM_MIN(a, KZM_MIN(b, c)))
#define KZM_MAX3(a, b, c)        (KZM_MAX(a, KZM_MAX(b, c)))
#define KZM_ABS(value)           ((value) < 0 ? -(value) : (value))
#define KZM_CLAMP(min, val, max) (KZM_MAX(KZM_MIN(val, max), min))

#include <stdalign.h>
#include <stdint.h>
#include <type_traits>

union v2i;
union v3i;
union v4i;
union v2;
union v3;
union v4;
union mat3;
union mat4;

union v2i
{
	struct
	{
		int32_t x, y;
	};

	struct
	{
		int32_t u, v;
	};

	int32_t e[2];

	static v2i add(v2i a, v2i b);
	static v2i adds(v2i a, int32_t b);
	static v2i sub(v2i a, v2i b);
	static v2i subs(v2i a, int32_t b);
	static v2i mul(v2i a, v2i b);
	static v2i muls(v2i a, int32_t b);
	static v2i div(v2i a, v2i b);
	static v2i divs(v2i a, int32_t b);
	static bool equals(v2i a, v2i b);
	static v2i negate(v2i value);

	static v2i mod(v2i a, v2i b);
	static v2i mods(v2i a, int32_t b);
	static v2i max(v2i a, v2i b);
	static v2i min(v2i a, v2i b);
	static v2i sign(v2i value);
	static v2i abs(v2i value);
	static v2 tov2(v2i value);

	static v2i divfloor(v2i a, v2i b);
	static v2i divfloors(v2i a, int32_t b);
	static v2i divceil(v2i a, v2i b);
	static v2i divceils(v2i a, int32_t b);

	inline int32_t &operator[](int i)
	{
		return e[i];
	}
};

static_assert(std::is_pod_v<v2i> == true);

inline v2i V2i(int32_t value)
{
	v2i result = {{value, value}};
	return result;
}

inline v2i V2i(int32_t x, int32_t y)
{
	v2i result = {{x, y}};
	return result;
}

inline v2i operator+(v2i a, v2i b)
{
	return v2i::add(a, b);
}

inline v2i operator+(v2i a, int32_t b)
{
	return v2i::adds(a, b);
}

inline v2i operator+(int32_t a, v2i b)
{
	return b + a;
}

inline v2i &operator+=(v2i &a, v2i b)
{
	a = a + b;
	return a;
}

inline v2i &operator+=(v2i &a, int32_t b)
{
	a = a + b;
	return a;
}

inline v2i operator-(v2i a, v2i b)
{
	return v2i::sub(a, b);
}

inline v2i operator-(v2i a, int32_t b)
{
	return v2i::subs(a, b);
}

inline v2i operator-(int32_t a, v2i b)
{
	return b - a;
}

inline v2i &operator-=(v2i &a, v2i b)
{
	a = a - b;
	return a;
}

inline v2i &operator-=(v2i &a, int32_t b)
{
	a = a - b;
	return a;
}

inline v2i operator*(v2i a, v2i b)
{
	return v2i::mul(a, b);
}

inline v2i operator*(v2i a, int32_t b)
{
	return v2i::muls(a, b);
}

inline v2i operator*(int32_t a, v2i b)
{
	return b * a;
}

inline v2i &operator*=(v2i &a, v2i b)
{
	a = a * b;
	return a;
}

inline v2i &operator*=(v2i &a, int32_t b)
{
	a = a * b;
	return a;
}

inline v2i operator/(v2i a, v2i b)
{
	return v2i::div(a, b);
}

inline v2i operator/(v2i a, int32_t b)
{
	return v2i::divs(a, b);
}

inline v2i operator/(int32_t a, v2i b)
{
	return v2i::div(V2i(a), b);
}

inline v2i &operator/=(v2i &a, v2i b)
{
	a = a / b;
	return a;
}

inline v2i &operator/=(v2i &a, int32_t b)
{
	a = a / b;
	return a;
}

inline v2i operator-(v2i a)
{
	return v2i::negate(a);
}

inline bool operator==(v2i a, v2i b)
{
	return v2i::equals(a, b);
}

inline bool operator!=(v2i a, v2i b)
{
	return !v2i::equals(a, b);
}

union v3i
{
	struct
	{
		int32_t x, y, z;
	};

	struct
	{
		int32_t r, g, b;
	};

	v2i xy;
	int32_t e[3];

	static v3i add(v3i a, v3i b);
	static v3i adds(v3i a, int32_t b);
	static v3i sub(v3i a, v3i b);
	static v3i subs(v3i a, int32_t b);
	static v3i mul(v3i a, v3i b);
	static v3i muls(v3i a, int32_t b);
	static v3i div(v3i a, v3i b);
	static v3i divs(v3i a, int32_t b);
	static bool equals(v3i a, v3i b);
	static v3i negate(v3i value);

	static v3i mod(v3i a, v3i b);
	static v3i mods(v3i a, int32_t b);
	static v3i max(v3i a, v3i b);
	static v3i min(v3i a, v3i b);
	static v3i sign(v3i value);
	static v3i abs(v3i value);
	static v3 tov3(v3i value);

	static v3i divfloor(v3i a, v3i b);
	static v3i divfloors(v3i a, int32_t b);
	static v3i divceil(v3i a, v3i b);
	static v3i divceils(v3i a, int32_t b);

	inline int32_t &operator[](int i)
	{
		return e[i];
	}
};

static_assert(std::is_pod_v<v3i> == true);

inline v3i V3i(int32_t value)
{
	v3i result = {{value, value, value}};
	return result;
}

inline v3i V3i(v2i xy, int32_t z)
{
	v3i result;
	result.xy = xy;
	result.z = z;
	return result;
}

inline v3i V3i(int32_t x, int32_t y, int32_t z)
{
	v3i result = {{x, y, z}};
	return result;
}

inline v3i operator+(v3i a, v3i b)
{
	return v3i::add(a, b);
}

inline v3i operator+(v3i a, int32_t b)
{
	return v3i::adds(a, b);
}

inline v3i operator+(int32_t a, v3i b)
{
	return b + a;
}

inline v3i &operator+=(v3i &a, v3i b)
{
	a = a + b;
	return a;
}

inline v3i &operator+=(v3i &a, int32_t b)
{
	a = a + b;
	return a;
}

inline v3i operator-(v3i a, v3i b)
{
	return v3i::sub(a, b);
}

inline v3i operator-(v3i a, int32_t b)
{
	return v3i::subs(a, b);
}

inline v3i operator-(int32_t a, v3i b)
{
	return b - a;
}

inline v3i &operator-=(v3i &a, v3i b)
{
	a = a - b;
	return a;
}

inline v3i &operator-=(v3i &a, int32_t b)
{
	a = a - b;
	return a;
}

inline v3i operator*(v3i a, v3i b)
{
	return v3i::mul(a, b);
}

inline v3i operator*(v3i a, int32_t b)
{
	return v3i::muls(a, b);
}

inline v3i operator*(int32_t a, v3i b)
{
	return b * a;
}

inline v3i &operator*=(v3i &a, v3i b)
{
	a = a * b;
	return a;
}

inline v3i &operator*=(v3i &a, int32_t b)
{
	a = a * b;
	return a;
}

inline v3i operator/(v3i a, v3i b)
{
	return v3i::div(a, b);
}

inline v3i operator/(v3i a, int32_t b)
{
	return v3i::divs(a, b);
}

inline v3i operator/(int32_t a, v3i b)
{
	return v3i::div(V3i(a), b);
}

inline v3i &operator/=(v3i &a, v3i b)
{
	a = a / b;
	return a;
}

inline v3i &operator/=(v3i &a, int32_t b)
{
	a = a / b;
	return a;
}

inline v3i operator-(v3i a)
{
	return v3i::negate(a);
}

inline bool operator==(v3i a, v3i b)
{
	return v3i::equals(a, b);
}

inline bool operator!=(v3i a, v3i b)
{
	return !v3i::equals(a, b);
}

union v4i
{
	struct
	{
		int32_t x, y, z, w;
	};

	struct
	{
		int32_t r, g, b, a;
	};

	v2i xy;
	v3i xyz;
	v3i rgb;
	int32_t e[4];

	static v4i add(v4i a, v4i b);
	static v4i adds(v4i a, int32_t b);
	static v4i sub(v4i a, v4i b);
	static v4i subs(v4i a, int32_t b);
	static v4i mul(v4i a, v4i b);
	static v4i muls(v4i a, int32_t b);
	static v4i div(v4i a, v4i b);
	static v4i divs(v4i a, int32_t b);
	static bool equals(v4i a, v4i b);
	static v4i negate(v4i value);

	static v4i mod(v4i a, v4i b);
	static v4i mods(v4i a, int32_t b);
	static v4i max(v4i a, v4i b);
	static v4i min(v4i a, v4i b);
	static v4i sign(v4i value);
	static v4i abs(v4i value);
	static v4 tov4(v4i value);

	static v4i divfloor(v4i a, v4i b);
	static v4i divfloors(v4i a, int32_t b);
	static v4i divceil(v4i a, v4i b);
	static v4i divceils(v4i a, int32_t b);

	inline int32_t &operator[](int i)
	{
		return e[i];
	}
};

static_assert(std::is_pod_v<v4i> == true);

inline v4i V4i(int32_t value)
{
	v4i result = {{value, value, value, value}};
	return result;
}

inline v4i V4i(v2i xy, int32_t z, int32_t w)
{
	v4i result;
	result.xy = xy;
	result.z = z;
	result.w = w;
	return result;
}

inline v4i V4i(v3i xyz, int32_t w)
{
	v4i result;
	result.xyz = xyz;
	result.w = w;
	return result;
}

inline v4i V4i(int32_t x, int32_t y, int32_t z, int32_t w)
{
	v4i result = {{x, y, z, w}};
	return result;
}

inline v4i V4i(IntVector4D v)
{
	v4i result = V4i(v.x, v.y, v.z, v.w);
	return result;
}

inline v4i operator+(v4i a, v4i b)
{
	return v4i::add(a, b);
}

inline v4i operator+(v4i a, int32_t b)
{
	return v4i::adds(a, b);
}

inline v4i operator+(int32_t a, v4i b)
{
	return b + a;
}

inline v4i &operator+=(v4i &a, v4i b)
{
	a = a + b;
	return a;
}

inline v4i &operator+=(v4i &a, int32_t b)
{
	a = a + b;
	return a;
}

inline v4i operator-(v4i a, v4i b)
{
	return v4i::sub(a, b);
}

inline v4i operator-(v4i a, int32_t b)
{
	return v4i::subs(a, b);
}

inline v4i operator-(int32_t a, v4i b)
{
	return b - a;
}

inline v4i &operator-=(v4i &a, v4i b)
{
	a = a - b;
	return a;
}

inline v4i &operator-=(v4i &a, int32_t b)
{
	a = a - b;
	return a;
}

inline v4i operator*(v4i a, v4i b)
{
	return v4i::mul(a, b);
}

inline v4i operator*(v4i a, int32_t b)
{
	return v4i::muls(a, b);
}

inline v4i operator*(int32_t a, v4i b)
{
	return b * a;
}

inline v4i &operator*=(v4i &a, v4i b)
{
	a = a * b;
	return a;
}

inline v4i &operator*=(v4i &a, int32_t b)
{
	a = a * b;
	return a;
}

inline v4i operator/(v4i a, v4i b)
{
	return v4i::div(a, b);
}

inline v4i operator/(v4i a, int32_t b)
{
	return v4i::divs(a, b);
}

inline v4i operator/(int32_t a, v4i b)
{
	return v4i::div(V4i(a), b);
}

inline v4i &operator/=(v4i &a, v4i b)
{
	a = a / b;
	return a;
}

inline v4i &operator/=(v4i &a, int32_t b)
{
	a = a / b;
	return a;
}

inline v4i operator-(v4i a)
{
	return v4i::negate(a);
}

inline bool operator==(v4i a, v4i b)
{
	return v4i::equals(a, b);
}

inline bool operator!=(v4i a, v4i b)
{
	return !v4i::equals(a, b);
}

union v2
{
	struct
	{
		float x, y;
	};

	struct
	{
		float u, v;
	};

	float e[2];

	static v2 add(v2 a, v2 b);
	static v2 adds(v2 a, float b);
	static v2 sub(v2 a, v2 b);
	static v2 subs(v2 a, float b);
	static v2 mul(v2 a, v2 b);
	static v2 muls(v2 a, float b);
	static v2 div(v2 a, v2 b);
	static v2 divs(v2 a, float b);
	static bool equals(v2 a, v2 b);
	static v2 negate(v2 value);

	static float dot(v2 a, v2 b);
	static float cross(v2 a, v2 b);
	static v2 mod(v2 a, v2 b);
	static v2 mods(v2 a, float b);
	static v2 max(v2 a, v2 b);
	static v2 min(v2 a, v2 b);
	static v2 fma(v2 direction, v2 scale, v2 add);
	static v2 fma(v2 direction, float scale, v2 add);
	static v2 sign(v2 value);
	static v2 abs(v2 value);
	static v2i tov2i(v2 value);

	static v2 floor(v2 value);
	static v2 ceil(v2 value);
	static v2 round(v2 value);
	static v2 normalise(v2 value);
	static float normInPlace(v2 *value);
	static float lensq(v2 value);
	static float len(v2 value);
	static v2 lerp(v2 a, v2 b, float t);

	Vector2D Vec();

	inline float &operator[](int i)
	{
		return e[i];
	}
};

static_assert(std::is_pod_v<v2> == true);

inline v2 V2(float value)
{
	v2 result = {{value, value}};
	return result;
}

inline v2 V2(float x, float y)
{
	v2 result = {{x, y}};
	return result;
}

inline v2 V2(const Vector2D &v)
{
	v2 result = {{v.x, v.y}};
	return result;
}

inline v2 operator+(v2 a, v2 b)
{
	return v2::add(a, b);
}

inline v2 operator+(v2 a, float b)
{
	return v2::adds(a, b);
}

inline v2 operator+(float a, v2 b)
{
	return b + a;
}

inline v2 &operator+=(v2 &a, v2 b)
{
	a = a + b;
	return a;
}

inline v2 &operator+=(v2 &a, float b)
{
	a = a + b;
	return a;
}

inline v2 operator-(v2 a, v2 b)
{
	return v2::sub(a, b);
}

inline v2 operator-(v2 a, float b)
{
	return v2::subs(a, b);
}

inline v2 operator-(float a, v2 b)
{
	return b - a;
}

inline v2 &operator-=(v2 &a, v2 b)
{
	a = a - b;
	return a;
}

inline v2 &operator-=(v2 &a, float b)
{
	a = a - b;
	return a;
}

inline v2 operator*(v2 a, v2 b)
{
	return v2::mul(a, b);
}

inline v2 operator*(v2 a, float b)
{
	return v2::muls(a, b);
}

inline v2 operator*(float a, v2 b)
{
	return b * a;
}

inline v2 &operator*=(v2 &a, v2 b)
{
	a = a * b;
	return a;
}

inline v2 &operator*=(v2 &a, float b)
{
	a = a * b;
	return a;
}

inline v2 operator/(v2 a, v2 b)
{
	return v2::div(a, b);
}

inline v2 operator/(v2 a, float b)
{
	return v2::divs(a, b);
}

inline v2 operator/(float a, v2 b)
{
	return v2::div(V2(a), b);
}

inline v2 &operator/=(v2 &a, v2 b)
{
	a = a / b;
	return a;
}

inline v2 &operator/=(v2 &a, float b)
{
	a = a / b;
	return a;
}

inline v2 operator-(v2 a)
{
	return v2::negate(a);
}

inline bool operator==(v2 a, v2 b)
{
	return v2::equals(a, b);
}

inline bool operator!=(v2 a, v2 b)
{
	return !v2::equals(a, b);
}

union v3
{
	struct
	{
		float x, y, z;
	};

	struct
	{
		float r, g, b;
	};

	struct
	{
		float pitch, yaw, roll;
	};

	v2 xy;
	float e[3];

	static v3 add(v3 a, v3 b);
	static v3 adds(v3 a, float b);
	static v3 sub(v3 a, v3 b);
	static v3 subs(v3 a, float b);
	static v3 mul(v3 a, v3 b);
	static v3 muls(v3 a, float b);
	static v3 div(v3 a, v3 b);
	static v3 divs(v3 a, float b);
	static bool equals(v3 a, v3 b);
	static v3 negate(v3 value);

	static float dot(v3 a, v3 b);
	static v3 cross(v3 a, v3 b);
	static v3 mod(v3 a, v3 b);
	static v3 mods(v3 a, float b);
	static v3 max(v3 a, v3 b);
	static v3 min(v3 a, v3 b);
	static v3 fma(v3 direction, v3 scale, v3 add);
	static v3 fma(v3 direction, float scale, v3 add);
	static v3 sign(v3 value);
	static v3 abs(v3 value);
	static v3i tov3i(v3 value);

	static v3 floor(v3 value);
	static v3 ceil(v3 value);
	static v3 round(v3 value);
	static v3 normalise(v3 value);
	static float normInPlace(v3 *value);
	static float lensq(v3 value);
	static float len(v3 value);
	static v3 lerp(v3 a, v3 b, float t);

	Vector Vec();

	inline float &operator[](int i)
	{
		return e[i];
	}
};

static_assert(std::is_trivially_copyable_v<v3> && std::is_standard_layout_v<v3> && std::is_trivially_default_constructible_v<v3>);

inline v3 V3(float value)
{
	v3 result = {{value, value, value}};
	return result;
}

inline v3 V3(v2 xy, float z)
{
	v3 result;
	result.xy = xy;
	result.z = z;
	return result;
}

inline v3 V3(float x, float y, float z)
{
	v3 result = {{x, y, z}};
	return result;
}

inline v3 V3(const Vector &v)
{
	v3 result = V3(v.x, v.y, v.z);
	return result;
}

inline v3 V3(const QAngle &v)
{
	v3 result = V3(v.x, v.y, v.z);
	return result;
}

inline v3 operator+(v3 a, v3 b)
{
	return v3::add(a, b);
}

inline v3 operator+(v3 a, float b)
{
	return v3::adds(a, b);
}

inline v3 operator+(float a, v3 b)
{
	return b + a;
}

inline v3 &operator+=(v3 &a, v3 b)
{
	a = a + b;
	return a;
}

inline v3 &operator+=(v3 &a, float b)
{
	a = a + b;
	return a;
}

inline v3 operator-(v3 a, v3 b)
{
	return v3::sub(a, b);
}

inline v3 operator-(v3 a, float b)
{
	return v3::subs(a, b);
}

inline v3 operator-(float a, v3 b)
{
	return b - a;
}

inline v3 &operator-=(v3 &a, v3 b)
{
	a = a - b;
	return a;
}

inline v3 &operator-=(v3 &a, float b)
{
	a = a - b;
	return a;
}

inline v3 operator*(v3 a, v3 b)
{
	return v3::mul(a, b);
}

inline v3 operator*(v3 a, float b)
{
	return v3::muls(a, b);
}

inline v3 operator*(float a, v3 b)
{
	return b * a;
}

inline v3 &operator*=(v3 &a, v3 b)
{
	a = a * b;
	return a;
}

inline v3 &operator*=(v3 &a, float b)
{
	a = a * b;
	return a;
}

inline v3 operator/(v3 a, v3 b)
{
	return v3::div(a, b);
}

inline v3 operator/(v3 a, float b)
{
	return v3::divs(a, b);
}

inline v3 operator/(float a, v3 b)
{
	return v3::div(V3(a), b);
}

inline v3 &operator/=(v3 &a, v3 b)
{
	a = a / b;
	return a;
}

inline v3 &operator/=(v3 &a, float b)
{
	a = a / b;
	return a;
}

inline v3 operator-(v3 a)
{
	return v3::negate(a);
}

inline bool operator==(v3 a, v3 b)
{
	return v3::equals(a, b);
}

inline bool operator!=(v3 a, v3 b)
{
	return !v3::equals(a, b);
}

union v4
{
	struct
	{
		float x, y, z, w;
	};

	struct
	{
		float r, g, b, a;
	};

	v2 xy;
	v3 xyz;
	v3 rgb;
	float e[4];

	static v4 add(v4 a, v4 b);
	static v4 adds(v4 a, float b);
	static v4 sub(v4 a, v4 b);
	static v4 subs(v4 a, float b);
	static v4 mul(v4 a, v4 b);
	static v4 muls(v4 a, float b);
	static v4 div(v4 a, v4 b);
	static v4 divs(v4 a, float b);
	static bool equals(v4 a, v4 b);
	static v4 negate(v4 value);

	static float dot(v4 a, v4 b);
	static v4 cross(v4 a, v4 b);
	static v4 mod(v4 a, v4 b);
	static v4 mods(v4 a, float b);
	static v4 max(v4 a, v4 b);
	static v4 min(v4 a, v4 b);
	static v4 fma(v4 direction, v4 scale, v4 add);
	static v4 fma(v4 direction, float scale, v4 add);
	static v4 sign(v4 value);
	static v4 abs(v4 value);
	static v4i tov4i(v4 value);

	static v4 floor(v4 value);
	static v4 ceil(v4 value);
	static v4 round(v4 value);
	static v4 normalise(v4 value);
	static float normInPlace(v4 *value);
	static float lensq(v4 value);
	static float len(v4 value);
	static v4 lerp(v4 a, v4 b, float t);

	Vector4D Vec();
	Quaternion Quat();

	inline float &operator[](int i)
	{
		return e[i];
	}
};

static_assert(std::is_pod_v<v4> == true);

inline v4 V4(float value)
{
	v4 result = {{value, value, value, value}};
	return result;
}

inline v4 V4(v2 xy, float z, float w)
{
	v4 result;
	result.xy = xy;
	result.z = z;
	result.w = w;
	return result;
}

inline v4 V4(v3 xyz, float w)
{
	v4 result;
	result.xyz = xyz;
	result.w = w;
	return result;
}

inline v4 V4(float x, float y, float z, float w)
{
	v4 result = {{x, y, z, w}};
	return result;
}

inline v4 V4(Quaternion quat)
{
	v4 result = V4(quat.x, quat.y, quat.z, quat.w);
	return result;
}

inline v4 V4(Vector4D v)
{
	v4 result = V4(v.x, v.y, v.z, v.w);
	return result;
}

inline v4 operator+(v4 a, v4 b)
{
	return v4::add(a, b);
}

inline v4 operator+(v4 a, float b)
{
	return v4::adds(a, b);
}

inline v4 operator+(float a, v4 b)
{
	return b + a;
}

inline v4 &operator+=(v4 &a, v4 b)
{
	a = a + b;
	return a;
}

inline v4 &operator+=(v4 &a, float b)
{
	a = a + b;
	return a;
}

inline v4 operator-(v4 a, v4 b)
{
	return v4::sub(a, b);
}

inline v4 operator-(v4 a, float b)
{
	return v4::subs(a, b);
}

inline v4 operator-(float a, v4 b)
{
	return b - a;
}

inline v4 &operator-=(v4 &a, v4 b)
{
	a = a - b;
	return a;
}

inline v4 &operator-=(v4 &a, float b)
{
	a = a - b;
	return a;
}

inline v4 operator*(v4 a, v4 b)
{
	return v4::mul(a, b);
}

inline v4 operator*(v4 a, float b)
{
	return v4::muls(a, b);
}

inline v4 operator*(float a, v4 b)
{
	return b * a;
}

inline v4 &operator*=(v4 &a, v4 b)
{
	a = a * b;
	return a;
}

inline v4 &operator*=(v4 &a, float b)
{
	a = a * b;
	return a;
}

inline v4 operator/(v4 a, v4 b)
{
	return v4::div(a, b);
}

inline v4 operator/(v4 a, float b)
{
	return v4::divs(a, b);
}

inline v4 operator/(float a, v4 b)
{
	return v4::div(V4(a), b);
}

inline v4 &operator/=(v4 &a, v4 b)
{
	a = a / b;
	return a;
}

inline v4 &operator/=(v4 &a, float b)
{
	a = a / b;
	return a;
}

inline v4 operator-(v4 a)
{
	return v4::negate(a);
}

inline bool operator==(v4 a, v4 b)
{
	return v4::equals(a, b);
}

inline bool operator!=(v4 a, v4 b)
{
	return !v4::equals(a, b);
}

union mat3
{
	float e[3][3];
	float arr[9];
	v3 col[3]; // column

	static mat3 transpose(mat3 matrix);
	static mat3 diagonal(float diagonal);
	static mat3 invert(mat3 matrix);
	static mat3 translation2d(v2 translation); // 2d translation
	static mat3 scale(v3 scale);
	static mat3 scale2d(v2 scale);
	static mat3 scalef32(float scale);
	static mat3 mul(mat3 left, mat3 right);
	static v3 mulv3(mat3 matrix, v3 vec);
	static mat3 rotate(float angle, v3 axis);
	static mat4 tomat4(mat3 matrix);

	inline v3 &operator[](int i)
	{
		return col[i];
	}
};

static_assert(std::is_pod_v<mat3> == true);

inline mat3 operator*(mat3 a, mat3 b)
{
	return mat3::mul(a, b);
}

inline v3 operator*(mat3 a, v3 b)
{
	return mat3::mulv3(a, b);
}

union mat4
{
	float e[4][4];
	float arr[16];
	v4 col[4]; // column

	static mat4 transpose(mat4 matrix);
	static mat4 diagonal(float diagonal);
	static mat4 invert(mat4 in);
	static mat4 rotation(v3 eulerAngles);
	static mat4 translation(v3 translation);
	static mat4 scale(v3 scale);
	static mat4 scalef32(float scale);
	static mat4 mul(mat4 left, mat4 right);
	static v4 mulv4(mat4 matrix, v4 vec);

	inline v4 &operator[](int i)
	{
		return col[i];
	}
};

static_assert(std::is_pod_v<mat4> == true);

inline mat4 operator*(mat4 a, mat4 b)
{
	return mat4::mul(a, b);
}

inline v4 operator*(mat4 a, v4 b)
{
	return mat4::mulv4(a, b);
}

int32_t i32wrap(int32_t value, int32_t max);
int32_t i32divfloor(int32_t a, int32_t b);
int32_t i32divceil(int32_t a, int32_t b);
int32_t i32mod(int32_t a, int32_t b);
int32_t i32sign(int32_t value);

float f32cos(float value);
float f32sin(float value);
float f32exp(float value);
float f32log(float value);
float f32log2(float value);
float f32sqrt(float value);
float f32tan(float value);
float f32atan(float value);
float f32atan2(float y, float x);
float f32asin(float value);
float f32acos(float value);
float f32pow(float a, float b);
float f32mod(float a, float b);
float f32sign(float value);
float f32round(float value);
float f32floor(float value);
float f32ceil(float value);
float f32fma(float a, float b, float c);
float f32lerp(float a, float b, float t);
int8_t f32toi8(float value);
int16_t f32toi16(float value);
int32_t f32toi32(float value);
int64_t f32toi64(float value);

double f64cos(double value);
double f64sin(double value);
double f64exp(double value);
double f64log(double value);
double f64log2(double value);
double f64sqrt(double value);
double f64tan(double value);
double f64atan(double value);
double f64atan2(double y, double x);
double f64asin(double value);
double f64acos(double value);
double f64pow(double a, double b);
double f64mod(double a, double b);
double f64sign(double value);
double f64round(double value);
double f64floor(double value);
double f64ceil(double value);
double f64fma(double a, double b, double c);
int8_t f64toi8(double value);
int16_t f64toi16(double value);
int32_t f64toi32(double value);
int64_t f64toi64(double value);

uint64_t NextPowerOf2(uint64_t value);

#ifdef DEBUG
// testing
void KzMathsTest();
#endif // DEBUG
