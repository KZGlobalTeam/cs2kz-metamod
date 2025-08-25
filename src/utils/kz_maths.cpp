
#include "kz_maths.h"

#include <math.h>
#include "commonmacros.h"
#include <assert.h>
#include <limits.h>

v2i v2i::add(v2i a, v2i b)
{
	v2i result = V2i(a.x + b.x, a.y + b.y);
	return result;
}

v2i v2i::adds(v2i a, int32_t b)
{
	v2i result = V2i(a.x + b, a.y + b);
	return result;
}

v2i v2i::sub(v2i a, v2i b)
{
	v2i result = V2i(a.x - b.x, a.y - b.y);
	return result;
}

v2i v2i::subs(v2i a, int32_t b)
{
	v2i result = V2i(a.x - b, a.y - b);
	return result;
}

v2i v2i::mul(v2i a, v2i b)
{
	v2i result = V2i(a.x * b.x, a.y * b.y);
	return result;
}

v2i v2i::muls(v2i a, int32_t b)
{
	v2i result = V2i(a.x * b, a.y * b);
	return result;
}

v2i v2i::div(v2i a, v2i b)
{
	v2i result = V2i(a.x / b.x, a.y / b.y);
	return result;
}

v2i v2i::divs(v2i a, int32_t b)
{
	v2i result = V2i(a.x / b, a.y / b);
	return result;
}

bool v2i::equals(v2i a, v2i b)
{
	bool result = (a.x == b.x && a.y == b.y);
	return result;
}

v2i v2i::negate(v2i value)
{
	v2i result = V2i(-value.x, -value.y);
	return result;
}

v2i v2i::mod(v2i a, v2i b)
{
	v2i result = V2i(i32mod(a.x, b.x), i32mod(a.y, b.y));
	return result;
}

v2i v2i::mods(v2i a, int32_t b)
{
	v2i result = V2i(i32mod(a.x, b), i32mod(a.y, b));
	return result;
}

v2i v2i::max(v2i a, v2i b)
{
	v2i result = V2i(KZM_MAX(a.x, b.x), KZM_MAX(a.y, b.y));
	return result;
}

v2i v2i::min(v2i a, v2i b)
{
	v2i result = V2i(KZM_MIN(a.x, b.x), KZM_MIN(a.y, b.y));
	return result;
}

v2i v2i::sign(v2i value)
{
	v2i result = V2i(i32sign(value.x), i32sign(value.y));
	return result;
}

v2i v2i::abs(v2i value)
{
	v2i result = V2i(KZM_ABS(value.x), KZM_ABS(value.y));
	return result;
}

v2 v2i::tov2(v2i value)
{
	v2 result = V2((float)value.x, (float)value.y);
	return result;
}

v2i v2i::divfloor(v2i a, v2i b)
{
	v2i result = V2i(i32divfloor(a.x, b.x), i32divfloor(a.y, b.y));
	return result;
}

v2i v2i::divfloors(v2i a, int32_t b)
{
	v2i result = V2i(i32divfloor(a.x, b), i32divfloor(a.y, b));
	return result;
}

v2i v2i::divceil(v2i a, v2i b)
{
	v2i result = V2i(i32divceil(a.x, b.x), i32divceil(a.y, b.y));
	return result;
}

v2i v2i::divceils(v2i a, int32_t b)
{
	v2i result = V2i(i32divceil(a.x, b), i32divceil(a.y, b));
	return result;
}

v3i v3i::add(v3i a, v3i b)
{
	v3i result = V3i(a.x + b.x, a.y + b.y, a.z + b.z);
	return result;
}

v3i v3i::adds(v3i a, int32_t b)
{
	v3i result = V3i(a.x + b, a.y + b, a.z + b);
	return result;
}

v3i v3i::sub(v3i a, v3i b)
{
	v3i result = V3i(a.x - b.x, a.y - b.y, a.z - b.z);
	return result;
}

v3i v3i::subs(v3i a, int32_t b)
{
	v3i result = V3i(a.x - b, a.y - b, a.z - b);
	return result;
}

v3i v3i::mul(v3i a, v3i b)
{
	v3i result = V3i(a.x * b.x, a.y * b.y, a.z * b.z);
	return result;
}

v3i v3i::muls(v3i a, int32_t b)
{
	v3i result = V3i(a.x * b, a.y * b, a.z * b);
	return result;
}

v3i v3i::div(v3i a, v3i b)
{
	v3i result = V3i(a.x / b.x, a.y / b.y, a.z / b.z);
	return result;
}

v3i v3i::divs(v3i a, int32_t b)
{
	v3i result = V3i(a.x / b, a.y / b, a.z / b);
	return result;
}

bool v3i::equals(v3i a, v3i b)
{
	bool result = (a.x == b.x && a.y == b.y && a.z == b.z);
	return result;
}

v3i v3i::negate(v3i value)
{
	v3i result = V3i(-value.x, -value.y, -value.z);
	return result;
}

v3i v3i::mod(v3i a, v3i b)
{
	v3i result = V3i(i32mod(a.x, b.x), i32mod(a.y, b.y), i32mod(a.z, b.z));
	return result;
}

v3i v3i::mods(v3i a, int32_t b)
{
	v3i result = V3i(i32mod(a.x, b), i32mod(a.y, b), i32mod(a.z, b));
	return result;
}

v3i v3i::max(v3i a, v3i b)
{
	v3i result = V3i(KZM_MAX(a.x, b.x), KZM_MAX(a.y, b.y), KZM_MAX(a.z, b.z));
	return result;
}

v3i v3i::min(v3i a, v3i b)
{
	v3i result = V3i(KZM_MIN(a.x, b.x), KZM_MIN(a.y, b.y), KZM_MIN(a.z, b.z));
	return result;
}

v3i v3i::sign(v3i value)
{
	v3i result = V3i(i32sign(value.x), i32sign(value.y), i32sign(value.z));
	return result;
}

v3i v3i::abs(v3i value)
{
	v3i result = V3i(KZM_ABS(value.x), KZM_ABS(value.y), KZM_ABS(value.z));
	return result;
}

v3 v3i::tov3(v3i value)
{
	v3 result = V3((float)value.x, (float)value.y, (float)value.z);
	return result;
}

v3i v3i::divfloor(v3i a, v3i b)
{
	v3i result = V3i(i32divfloor(a.x, b.x), i32divfloor(a.y, b.y), i32divfloor(a.z, b.z));
	return result;
}

v3i v3i::divfloors(v3i a, int32_t b)
{
	v3i result = V3i(i32divfloor(a.x, b), i32divfloor(a.y, b), i32divfloor(a.z, b));
	return result;
}

v3i v3i::divceil(v3i a, v3i b)
{
	v3i result = V3i(i32divceil(a.x, b.x), i32divceil(a.y, b.y), i32divceil(a.z, b.z));
	return result;
}

v3i v3i::divceils(v3i a, int32_t b)
{
	v3i result = V3i(i32divceil(a.x, b), i32divceil(a.y, b), i32divceil(a.z, b));
	return result;
}

v4i v4i::add(v4i a, v4i b)
{
	v4i result = V4i(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
	return result;
}

v4i v4i::adds(v4i a, int32_t b)
{
	v4i result = V4i(a.x + b, a.y + b, a.z + b, a.w + b);
	return result;
}

v4i v4i::sub(v4i a, v4i b)
{
	v4i result = V4i(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
	return result;
}

v4i v4i::subs(v4i a, int32_t b)
{
	v4i result = V4i(a.x - b, a.y - b, a.z - b, a.w - b);
	return result;
}

v4i v4i::mul(v4i a, v4i b)
{
	v4i result = V4i(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
	return result;
}

v4i v4i::muls(v4i a, int32_t b)
{
	v4i result = V4i(a.x * b, a.y * b, a.z * b, a.w * b);
	return result;
}

v4i v4i::div(v4i a, v4i b)
{
	v4i result = V4i(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
	return result;
}

v4i v4i::divs(v4i a, int32_t b)
{
	v4i result = V4i(a.x / b, a.y / b, a.z / b, a.w / b);
	return result;
}

bool v4i::equals(v4i a, v4i b)
{
	bool result = (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w);
	return result;
}

v4i v4i::negate(v4i value)
{
	v4i result = V4i(-value.x, -value.y, -value.z, -value.w);
	return result;
}

v4i v4i::mod(v4i a, v4i b)
{
	v4i result = V4i(i32mod(a.x, b.x), i32mod(a.y, b.y), i32mod(a.z, b.z), i32mod(a.w, b.w));
	return result;
}

v4i v4i::mods(v4i a, int32_t b)
{
	v4i result = V4i(i32mod(a.x, b), i32mod(a.y, b), i32mod(a.z, b), i32mod(a.w, b));
	return result;
}

v4i v4i::max(v4i a, v4i b)
{
	v4i result = V4i(KZM_MAX(a.x, b.x), KZM_MAX(a.y, b.y), KZM_MAX(a.z, b.z), KZM_MAX(a.w, b.w));
	return result;
}

v4i v4i::min(v4i a, v4i b)
{
	v4i result = V4i(KZM_MIN(a.x, b.x), KZM_MIN(a.y, b.y), KZM_MIN(a.z, b.z), KZM_MIN(a.w, b.w));
	return result;
}

v4i v4i::sign(v4i value)
{
	v4i result = V4i(i32sign(value.x), i32sign(value.y), i32sign(value.z), i32sign(value.w));
	return result;
}

v4i v4i::abs(v4i value)
{
	v4i result = V4i(KZM_ABS(value.x), KZM_ABS(value.y), KZM_ABS(value.z), KZM_ABS(value.w));
	return result;
}

v4 v4i::tov4(v4i value)
{
	v4 result = V4((float)value.x, (float)value.y, (float)value.z, (float)value.w);
	return result;
}

v4i v4i::divfloor(v4i a, v4i b)
{
	v4i result = V4i(i32divfloor(a.x, b.x), i32divfloor(a.y, b.y), i32divfloor(a.z, b.z), i32divfloor(a.w, b.w));
	return result;
}

v4i v4i::divfloors(v4i a, int32_t b)
{
	v4i result = V4i(i32divfloor(a.x, b), i32divfloor(a.y, b), i32divfloor(a.z, b), i32divfloor(a.w, b));
	return result;
}

v4i v4i::divceil(v4i a, v4i b)
{
	v4i result = V4i(i32divceil(a.x, b.x), i32divceil(a.y, b.y), i32divceil(a.z, b.z), i32divceil(a.w, b.w));
	return result;
}

v4i v4i::divceils(v4i a, int32_t b)
{
	v4i result = V4i(i32divceil(a.x, b), i32divceil(a.y, b), i32divceil(a.z, b), i32divceil(a.w, b));
	return result;
}

v2 v2::add(v2 a, v2 b)
{
	v2 result = V2(a.x + b.x, a.y + b.y);
	return result;
}

v2 v2::adds(v2 a, float b)
{
	v2 result = V2(a.x + b, a.y + b);
	return result;
}

v2 v2::sub(v2 a, v2 b)
{
	v2 result = V2(a.x - b.x, a.y - b.y);
	return result;
}

v2 v2::subs(v2 a, float b)
{
	v2 result = V2(a.x - b, a.y - b);
	return result;
}

v2 v2::mul(v2 a, v2 b)
{
	v2 result = V2(a.x * b.x, a.y * b.y);
	return result;
}

v2 v2::muls(v2 a, float b)
{
	v2 result = V2(a.x * b, a.y * b);
	return result;
}

v2 v2::div(v2 a, v2 b)
{
	v2 result = V2(a.x / b.x, a.y / b.y);
	return result;
}

v2 v2::divs(v2 a, float b)
{
	v2 result = V2(a.x / b, a.y / b);
	return result;
}

bool v2::equals(v2 a, v2 b)
{
	bool result = (a.x == b.x && a.y == b.y);
	return result;
}

v2 v2::negate(v2 value)
{
	v2 result = V2(-value.x, -value.y);
	return result;
}

float v2::dot(v2 a, v2 b)
{
	float result = a.x * b.x + a.y * b.y;
	return result;
}

float v2::cross(v2 a, v2 b)
{
	float result = (a.x * b.y) - (a.y * b.x);
	return result;
}

v2 v2::mod(v2 a, v2 b)
{
	v2 result = V2(f32mod(a.x, b.x), f32mod(a.y, b.y));
	return result;
}

v2 v2::mods(v2 a, float b)
{
	v2 result = V2(f32mod(a.x, b), f32mod(a.y, b));
	return result;
}

v2 v2::max(v2 a, v2 b)
{
	v2 result = V2(KZM_MAX(a.x, b.x), KZM_MAX(a.y, b.y));
	return result;
}

v2 v2::min(v2 a, v2 b)
{
	v2 result = V2(KZM_MIN(a.x, b.x), KZM_MIN(a.y, b.y));
	return result;
}

v2 v2::fma(v2 direction, v2 scale, v2 add)
{
	v2 result = V2(f32fma(direction.x, scale.x, add.x), f32fma(direction.y, scale.y, add.y));
	return result;
}

v2 v2::fma(v2 direction, float scale, v2 add)
{
	v2 result = V2(f32fma(direction.x, scale, add.x), f32fma(direction.y, scale, add.y));
	return result;
}

v2 v2::sign(v2 value)
{
	v2 result = V2(f32sign(value.x), f32sign(value.y));
	return result;
}

v2 v2::abs(v2 value)
{
	v2 result = V2(KZM_ABS(value.x), KZM_ABS(value.y));
	return result;
}

v2i v2::tov2i(v2 value)
{
	v2i result = V2i((int32_t)value.x, (int32_t)value.y);
	return result;
}

v2 v2::floor(v2 value)
{
	v2 result = V2(f32floor(value.x), f32floor(value.y));
	return result;
}

v2 v2::ceil(v2 value)
{
	v2 result = V2(f32ceil(value.x), f32ceil(value.y));
	return result;
}

v2 v2::round(v2 value)
{
	v2 result = V2(f32round(value.x), f32round(value.y));
	return result;
}

v2 v2::normalise(v2 value)
{
	float length = len(value);
	if (length > 0)
	{
		length = 1.0f / length;
	}
	v2 result = muls(value, length);
	return result;
}

float v2::normInPlace(v2 *value)
{
	float result = len(*value);
	float invLength = result;
	if (invLength > 0)
	{
		invLength = 1.0f / invLength;
	}
	*value = muls(*value, invLength);
	return result;
}

float v2::lensq(v2 value)
{
	float result = (value.x * value.x + value.y * value.y);
	return result;
}

float v2::len(v2 value)
{
	float result = f32sqrt(value.x * value.x + value.y * value.y);
	return result;
}

v2 v2::lerp(v2 a, v2 b, float t)
{
	v2 result = V2(f32lerp(a.x, b.x, t), f32lerp(a.y, b.y, t));
	return result;
}

Vector2D v2::Vec()
{
	Vector2D result = Vector2D(x, y);
	return result;
}

v3 v3::add(v3 a, v3 b)
{
	v3 result = V3(a.x + b.x, a.y + b.y, a.z + b.z);
	return result;
}

v3 v3::adds(v3 a, float b)
{
	v3 result = V3(a.x + b, a.y + b, a.z + b);
	return result;
}

v3 v3::sub(v3 a, v3 b)
{
	v3 result = V3(a.x - b.x, a.y - b.y, a.z - b.z);
	return result;
}

v3 v3::subs(v3 a, float b)
{
	v3 result = V3(a.x - b, a.y - b, a.z - b);
	return result;
}

v3 v3::mul(v3 a, v3 b)
{
	v3 result = V3(a.x * b.x, a.y * b.y, a.z * b.z);
	return result;
}

v3 v3::muls(v3 a, float b)
{
	v3 result = V3(a.x * b, a.y * b, a.z * b);
	return result;
}

v3 v3::div(v3 a, v3 b)
{
	v3 result = V3(a.x / b.x, a.y / b.y, a.z / b.z);
	return result;
}

v3 v3::divs(v3 a, float b)
{
	v3 result = V3(a.x / b, a.y / b, a.z / b);
	return result;
}

bool v3::equals(v3 a, v3 b)
{
	bool result = (a.x == b.x && a.y == b.y && a.z == b.z);
	return result;
}

v3 v3::negate(v3 value)
{
	v3 result = V3(-value.x, -value.y, -value.z);
	return result;
}

float v3::dot(v3 a, v3 b)
{
	float result = a.x * b.x + a.y * b.y + a.z * b.z;
	return result;
}

v3 v3::cross(v3 a, v3 b)
{
	v3 result = V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	return result;
}

v3 v3::mod(v3 a, v3 b)
{
	v3 result = V3(f32mod(a.x, b.x), f32mod(a.y, b.y), f32mod(a.z, b.z));
	return result;
}

v3 v3::mods(v3 a, float b)
{
	v3 result = V3(f32mod(a.x, b), f32mod(a.y, b), f32mod(a.z, b));
	return result;
}

v3 v3::max(v3 a, v3 b)
{
	v3 result = V3(KZM_MAX(a.x, b.x), KZM_MAX(a.y, b.y), KZM_MAX(a.z, b.z));
	return result;
}

v3 v3::min(v3 a, v3 b)
{
	v3 result = V3(KZM_MIN(a.x, b.x), KZM_MIN(a.y, b.y), KZM_MIN(a.z, b.z));
	return result;
}

v3 v3::fma(v3 direction, v3 scale, v3 add)
{
	v3 result = V3(f32fma(direction.x, scale.x, add.x), f32fma(direction.y, scale.y, add.y), f32fma(direction.z, scale.z, add.z));
	return result;
}

v3 v3::fma(v3 direction, float scale, v3 add)
{
	v3 result = V3(f32fma(direction.x, scale, add.x), f32fma(direction.y, scale, add.y), f32fma(direction.z, scale, add.z));
	return result;
}

v3 v3::sign(v3 value)
{
	v3 result = V3(f32sign(value.x), f32sign(value.y), f32sign(value.z));
	return result;
}

v3 v3::abs(v3 value)
{
	v3 result = V3(KZM_ABS(value.x), KZM_ABS(value.y), KZM_ABS(value.z));
	return result;
}

v3i v3::tov3i(v3 value)
{
	v3i result = V3i((int32_t)value.x, (int32_t)value.y, (int32_t)value.z);
	return result;
}

v3 v3::floor(v3 value)
{
	v3 result = V3(f32floor(value.x), f32floor(value.y), f32floor(value.z));
	return result;
}

v3 v3::ceil(v3 value)
{
	v3 result = V3(f32ceil(value.x), f32ceil(value.y), f32ceil(value.z));
	return result;
}

v3 v3::round(v3 value)
{
	v3 result = V3(f32round(value.x), f32round(value.y), f32round(value.z));
	return result;
}

v3 v3::normalise(v3 value)
{
	float length = len(value);
	if (length > 0)
	{
		length = 1.0f / length;
	}
	v3 result = muls(value, length);
	return result;
}

float v3::normInPlace(v3 *value)
{
	float result = len(*value);
	float invLength = result;
	if (invLength > 0)
	{
		invLength = 1.0f / invLength;
	}
	*value = muls(*value, invLength);
	return result;
}

float v3::lensq(v3 value)
{
	float result = (value.x * value.x + value.y * value.y + value.z * value.z);
	return result;
}

float v3::len(v3 value)
{
	float result = f32sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	return result;
}

v3 v3::lerp(v3 a, v3 b, float t)
{
	v3 result = V3(f32lerp(a.x, b.x, t), f32lerp(a.y, b.y, t), f32lerp(a.z, b.z, t));
	return result;
}

Vector v3::Vec()
{
	Vector result = Vector(x, y, z);
	return result;
}

v4 v4::add(v4 a, v4 b)
{
	v4 result = V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
	return result;
}

v4 v4::adds(v4 a, float b)
{
	v4 result = V4(a.x + b, a.y + b, a.z + b, a.w + b);
	return result;
}

v4 v4::sub(v4 a, v4 b)
{
	v4 result = V4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
	return result;
}

v4 v4::subs(v4 a, float b)
{
	v4 result = V4(a.x - b, a.y - b, a.z - b, a.w - b);
	return result;
}

v4 v4::mul(v4 a, v4 b)
{
	v4 result = V4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);
	return result;
}

v4 v4::muls(v4 a, float b)
{
	v4 result = V4(a.x * b, a.y * b, a.z * b, a.w * b);
	return result;
}

v4 v4::div(v4 a, v4 b)
{
	v4 result = V4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w);
	return result;
}

v4 v4::divs(v4 a, float b)
{
	v4 result = V4(a.x / b, a.y / b, a.z / b, a.w / b);
	return result;
}

bool v4::equals(v4 a, v4 b)
{
	bool result = (a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w);
	return result;
}

v4 v4::negate(v4 value)
{
	v4 result = V4(-value.x, -value.y, -value.z, -value.w);
	return result;
}

float v4::dot(v4 a, v4 b)
{
	float result = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	return result;
}

v4 v4::mod(v4 a, v4 b)
{
	v4 result = V4(f32mod(a.x, b.x), f32mod(a.y, b.y), f32mod(a.z, b.z), f32mod(a.w, b.w));
	return result;
}

v4 v4::mods(v4 a, float b)
{
	v4 result = V4(f32mod(a.x, b), f32mod(a.y, b), f32mod(a.z, b), f32mod(a.w, b));
	return result;
}

v4 v4::max(v4 a, v4 b)
{
	v4 result = V4(KZM_MAX(a.x, b.x), KZM_MAX(a.y, b.y), KZM_MAX(a.z, b.z), KZM_MAX(a.w, b.w));
	return result;
}

v4 v4::min(v4 a, v4 b)
{
	v4 result = V4(KZM_MIN(a.x, b.x), KZM_MIN(a.y, b.y), KZM_MIN(a.z, b.z), KZM_MIN(a.w, b.w));
	return result;
}

v4 v4::fma(v4 direction, v4 scale, v4 add)
{
	v4 result = V4(f32fma(direction.x, scale.x, add.x), f32fma(direction.y, scale.y, add.y), f32fma(direction.z, scale.z, add.z),
				   f32fma(direction.w, scale.w, add.w));
	return result;
}

v4 v4::fma(v4 direction, float scale, v4 add)
{
	v4 result = V4(f32fma(direction.x, scale, add.x), f32fma(direction.y, scale, add.y), f32fma(direction.z, scale, add.z),
				   f32fma(direction.w, scale, add.w));
	return result;
}

v4 v4::sign(v4 value)
{
	v4 result = V4(f32sign(value.x), f32sign(value.y), f32sign(value.z), f32sign(value.w));
	return result;
}

v4 v4::abs(v4 value)
{
	v4 result = V4(KZM_ABS(value.x), KZM_ABS(value.y), KZM_ABS(value.z), KZM_ABS(value.w));
	return result;
}

v4i v4::tov4i(v4 value)
{
	v4i result = V4i((int32_t)value.x, (int32_t)value.y, (int32_t)value.z, (int32_t)value.w);
	return result;
}

v4 v4::floor(v4 value)
{
	v4 result = V4(f32floor(value.x), f32floor(value.y), f32floor(value.z), f32floor(value.w));
	return result;
}

v4 v4::ceil(v4 value)
{
	v4 result = V4(f32ceil(value.x), f32ceil(value.y), f32ceil(value.z), f32ceil(value.w));
	return result;
}

v4 v4::round(v4 value)
{
	v4 result = V4(f32round(value.x), f32round(value.y), f32round(value.z), f32round(value.w));
	return result;
}

v4 v4::normalise(v4 value)
{
	float length = len(value);
	if (length > 0)
	{
		length = 1.0f / length;
	}
	v4 result = muls(value, length);
	return result;
}

float v4::normInPlace(v4 *value)
{
	float result = len(*value);
	float invLength = result;
	if (invLength > 0)
	{
		invLength = 1.0f / invLength;
	}
	*value = muls(*value, invLength);
	return result;
}

float v4::lensq(v4 value)
{
	float result = (value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
	return result;
}

float v4::len(v4 value)
{
	float result = f32sqrt(value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
	return result;
}

v4 v4::lerp(v4 a, v4 b, float t)
{
	v4 result = V4(f32lerp(a.x, b.x, t), f32lerp(a.y, b.y, t), f32lerp(a.z, b.z, t), f32lerp(a.w, b.w, t));
	return result;
}

Vector4D v4::Vec()
{
	Vector4D result = Vector4D(x, y, z, w);
	return result;
}

Quaternion v4::Quat()
{
	Quaternion result = Quaternion(x, y, z, w);
	return result;
}

int32_t i32wrap(int32_t value, int32_t max)
{
	int32_t result = value;
	int32_t num = i32divfloor(value, max);
	result -= max * num;
	return result;
}

int32_t i32divfloor(int32_t a, int32_t b)
{
	int32_t d = a / b;
	int32_t result = d;
	if (d * b != a)
	{
		result = d - ((a < 0) ^ (b < 0));
	}

	return result;
}

int32_t i32divceil(int32_t a, int32_t b)
{
	bool positive = (b >= 0) == (a >= 0);
	int32_t result = (a / b) + (a % b != 0 && positive);
	return result;
}

int32_t i32mod(int32_t a, int32_t b)
{
	int32_t result = a % b;
	if (result < 0)
	{
		result += b;
	}
	return result;
}

int32_t i32sign(int32_t value)
{
	int32_t result = 0;
	if (value > 0)
	{
		result = 1;
	}
	else if (value < 0)
	{
		result = -1;
	}
	return result;
}

float f32cos(float value)
{
	float result = cosf(value);
	return result;
}

float f32sin(float value)
{
	float result = sinf(value);
	return result;
}

float f32exp(float value)
{
	float result = expf(value);
	return result;
}

float f32log(float value)
{
	float result = logf(value);
	return result;
}

float f32log2(float value)
{
	float result = log2f(value);
	return result;
}

float f32sqrt(float value)
{
	float result = sqrtf(value);
	return result;
}

float f32tan(float value)
{
	float result = tanf(value);
	return result;
}

float f32atan(float value)
{
	float result = atanf(value);
	return result;
}

float f32atan2(float y, float x)
{
	float result = atan2f(y, x);
	return result;
}

float f32asin(float value)
{
	float result = asinf(value);
	return result;
}

float f32acos(float value)
{
	float result = acosf(value);
	return result;
}

float f32pow(float a, float b)
{
	float result = powf(a, b);
	return result;
}

float f32mod(float a, float b)
{
	float result = fmodf(a, b);
	return result;
}

float f32sign(float value)
{
	float result = 1;
	if (value == 0)
	{
		result = 0;
	}
	else if (value < 0)
	{
		result = -1;
	}
	return result;
}

float f32round(float value)
{
	float result = roundf(value);
	return result;
}

float f32floor(float value)
{
	float result = floorf(value);
	return result;
}

float f32ceil(float value)
{
	float result = ceilf(value);
	return result;
}

float f32fma(float a, float b, float c)
{
	float result = fmaf(a, b, c);
	return result;
}

float f32lerp(float a, float b, float t)
{
	float result = a + (b - a) * t;
	return result;
}

int8_t f32toi8(float value)
{
	int8_t result;
	if (value > (float)INT8_MAX)
	{
		result = INT8_MAX;
	}
	else if (value < (float)INT8_MIN)
	{
		result = INT8_MIN;
	}
	else
	{
		result = (int8_t)value;
	}
	return result;
}

int16_t f32toi16(float value)
{
	int16_t result;
	if (value > (float)INT16_MAX)
	{
		result = INT16_MAX;
	}
	else if (value < (float)INT16_MIN)
	{
		result = INT16_MIN;
	}
	else
	{
		result = (int16_t)value;
	}
	return result;
}

int32_t f32toi32(float value)
{
	int32_t result;
	if (value > (float)INT32_MAX)
	{
		result = INT32_MAX;
	}
	else if (value < (float)INT32_MIN)
	{
		result = INT32_MIN;
	}
	else
	{
		result = (int32_t)value;
	}
	return result;
}

int64_t f32toi64(float value)
{
	int64_t result;
	if (value > (float)INT64_MAX)
	{
		result = INT64_MAX;
	}
	else if (value < (float)INT64_MIN)
	{
		result = INT64_MIN;
	}
	else
	{
		result = (int64_t)value;
	}
	return result;
}

double f64cos(double value)
{
	double result = cos(value);
	return result;
}

double f64sin(double value)
{
	double result = sin(value);
	return result;
}

double f64exp(double value)
{
	double result = exp(value);
	return result;
}

double f64log(double value)
{
	double result = log(value);
	return result;
}

double f64log2(double value)
{
	double result = log2(value);
	return result;
}

double f64sqrt(double value)
{
	double result = sqrt(value);
	return result;
}

double f64tan(double value)
{
	double result = tan(value);
	return result;
}

double f64atan(double value)
{
	double result = atan(value);
	return result;
}

double f64atan2(double y, double x)
{
	double result = atan2(y, x);
	return result;
}

double f64asin(double value)
{
	double result = asin(value);
	return result;
}

double f64acos(double value)
{
	double result = acos(value);
	return result;
}

double f64pow(double a, double b)
{
	double result = pow(a, b);
	return result;
}

double f64mod(double a, double b)
{
	double result = fmod(a, b);
	return result;
}

double f64sign(double value)
{
	double result = 1;
	if (value == 0)
	{
		result = 0;
	}
	else if (value < 0)
	{
		result = -1;
	}
	return result;
}

double f64round(double value)
{
	double result = round(value);
	return result;
}

double f64floor(double value)
{
	double result = floor(value);
	return result;
}

double f64ceil(double value)
{
	double result = ceil(value);
	return result;
}

double f64fma(double a, double b, double c)
{
	double result = fmaf(a, b, c);
	return result;
}

int8_t f64toi8(double value)
{
	int8_t result;
	if (value > (double)INT8_MAX)
	{
		result = INT8_MAX;
	}
	else if (value < (double)INT8_MIN)
	{
		result = INT8_MIN;
	}
	else
	{
		result = (int8_t)value;
	}
	return result;
}

int16_t f64toi16(double value)
{
	int16_t result;
	if (value > (double)INT16_MAX)
	{
		result = INT16_MAX;
	}
	else if (value < (double)INT16_MIN)
	{
		result = INT16_MIN;
	}
	else
	{
		result = (int16_t)value;
	}
	return result;
}

int32_t f64toi32(double value)
{
	int32_t result;
	if (value > (double)INT32_MAX)
	{
		result = INT32_MAX;
	}
	else if (value < (double)INT32_MIN)
	{
		result = INT32_MIN;
	}
	else
	{
		result = (int32_t)value;
	}
	return result;
}

int64_t f64toi64(double value)
{
	int64_t result;
	if (value > (double)INT64_MAX)
	{
		result = INT64_MAX;
	}
	else if (value < (double)INT64_MIN)
	{
		result = INT64_MIN;
	}
	else
	{
		result = (int64_t)value;
	}
	return result;
}

mat4 mat4::transpose(mat4 matrix)
{
	mat4 result;
	for (int32_t col = 0; col < 4; col++)
	{
		for (int32_t row = 0; row < 4; row++)
		{
			result.e[col][row] = matrix.e[row][col];
		}
	}
	return result;
}

mat4 mat4::diagonal(float diagonal)
{
	mat4 result = {};
	result.e[0][0] = diagonal;
	result.e[1][1] = diagonal;
	result.e[2][2] = diagonal;
	result.e[3][3] = diagonal;
	return result;
}

// NOTE: yoink https://www.mesa3d.org/
mat4 mat4::invert(mat4 in)
{
	float inv[16];
	float *m = (float *)&in;
	inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

	inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

	inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

	inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

	inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

	inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

	inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

	inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

	inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

	inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

	inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

	inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

	inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

	inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

	inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

	inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

	double det = (double)m[0] * (double)inv[0] + (double)m[1] * (double)inv[4] + (double)m[2] * (double)inv[8] + (double)m[3] * (double)inv[12];

	mat4 result = {};
	if (det != 0)
	{
		det = 1.0 / det;

		for (int32_t i = 0; i < 16; i++)
		{
			result.arr[i] = inv[i] * (float)det;
		}
	}
	return result;
}

mat4 mat4::rotation(v3 eulerAngles)
{
	mat4 result = diagonal(1);

	float cosU = f32cos(eulerAngles.z);
	float cosV = f32cos(eulerAngles.x);
	float cosW = f32cos(eulerAngles.y);

	float sinU = f32sin(eulerAngles.z);
	float sinV = f32sin(eulerAngles.x);
	float sinW = f32sin(eulerAngles.y);

	result.e[0][0] = cosV * cosW;
	result.e[0][1] = cosV * sinW;
	result.e[0][2] = -sinV;

	result.e[1][0] = sinU * sinV * cosW - cosU * sinW;
	result.e[1][1] = cosU * cosW + sinU * sinV * sinW;
	result.e[1][2] = sinU * cosV;

	result.e[2][0] = sinU * sinW + cosU * sinV * cosW;
	result.e[2][1] = cosU * sinV * sinW - sinU * cosW;
	result.e[2][2] = cosU * cosV;

	return result;
}

mat4 mat4::translation(v3 translation)
{
	mat4 result = diagonal(1);
	result.e[3][0] = translation.x;
	result.e[3][1] = translation.y;
	result.e[3][2] = translation.z;
	return result;
}

mat4 mat4::scale(v3 scale)
{
	mat4 result = {};
	result.e[0][0] = scale.x;
	result.e[1][1] = scale.y;
	result.e[2][2] = scale.z;
	result.e[3][3] = 1;
	return result;
}

mat4 mat4::scalef32(float scale)
{
	mat4 result = {};
	result.e[0][0] = scale;
	result.e[1][1] = scale;
	result.e[2][2] = scale;
	result.e[3][3] = 1;
	return result;
}

mat4 mat4::mul(mat4 left, mat4 right)
{
	mat4 result;
	for (int32_t column = 0; column < 4; column++)
	{
		for (int32_t row = 0; row < 4; row++)
		{
			float sum = 0;
			for (int32_t i = 0; i < 4; ++i)
			{
				sum += left.e[i][row] * right.e[column][i];
			}

			result.e[column][row] = sum;
		}
	}
	return result;
}

v4 mat4::mulv4(mat4 matrix, v4 vec)
{
	v4 result;
	for (int32_t row = 0; row < 4; ++row)
	{
		float sum = 0;
		for (int32_t column = 0; column < 4; ++column)
		{
			sum += matrix.e[column][row] * vec.e[column];
		}
		result.e[row] = sum;
	}
	return result;
}

mat3 mat3::transpose(mat3 matrix)
{
	mat3 result;
	for (int32_t col = 0; col < 3; col++)
	{
		for (int32_t row = 0; row < 3; row++)
		{
			result.e[col][row] = matrix.e[row][col];
		}
	}
	return result;
}

mat3 mat3::diagonal(float diagonal)
{
	mat3 result = {};
	result.e[0][0] = diagonal;
	result.e[1][1] = diagonal;
	result.e[2][2] = diagonal;
	return result;
}

// https://stackoverflow.com/a/18504573
mat3 mat3::invert(mat3 matrix)
{
	float det = matrix.e[0][0] * (matrix.e[1][1] * matrix.e[2][2] - matrix.e[2][1] * matrix.e[1][2])
				- matrix.e[0][1] * (matrix.e[1][0] * matrix.e[2][2] - matrix.e[1][2] * matrix.e[2][0])
				+ matrix.e[0][2] * (matrix.e[1][0] * matrix.e[2][1] - matrix.e[1][1] * matrix.e[2][0]);

	float invdet = 1.0f / det;

	mat3 result;
	result.e[0][0] = (matrix.e[1][1] * matrix.e[2][2] - matrix.e[2][1] * matrix.e[1][2]) * invdet;
	result.e[0][1] = (matrix.e[0][2] * matrix.e[2][1] - matrix.e[0][1] * matrix.e[2][2]) * invdet;
	result.e[0][2] = (matrix.e[0][1] * matrix.e[1][2] - matrix.e[0][2] * matrix.e[1][1]) * invdet;
	result.e[1][0] = (matrix.e[1][2] * matrix.e[2][0] - matrix.e[1][0] * matrix.e[2][2]) * invdet;
	result.e[1][1] = (matrix.e[0][0] * matrix.e[2][2] - matrix.e[0][2] * matrix.e[2][0]) * invdet;
	result.e[1][2] = (matrix.e[1][0] * matrix.e[0][2] - matrix.e[0][0] * matrix.e[1][2]) * invdet;
	result.e[2][0] = (matrix.e[1][0] * matrix.e[2][1] - matrix.e[2][0] * matrix.e[1][1]) * invdet;
	result.e[2][1] = (matrix.e[2][0] * matrix.e[0][1] - matrix.e[0][0] * matrix.e[2][1]) * invdet;
	result.e[2][2] = (matrix.e[0][0] * matrix.e[1][1] - matrix.e[1][0] * matrix.e[0][1]) * invdet;
	return result;
}

mat3 mat3::translation2d(v2 translation)
{
	mat3 result = diagonal(1);
	result.e[2][0] = translation.x;
	result.e[2][1] = translation.y;
	return result;
}

mat3 mat3::scale(v3 scale)
{
	mat3 result = {};
	result.e[0][0] = scale.x;
	result.e[1][1] = scale.y;
	result.e[2][2] = scale.z;
	return result;
}

mat3 mat3::scale2d(v2 scale)
{
	mat3 result = {};
	result.e[0][0] = scale.x;
	result.e[1][1] = scale.y;
	result.e[2][2] = 1;
	return result;
}

mat3 mat3::scalef32(float scale)
{
	mat3 result = {};
	result.e[0][0] = scale;
	result.e[1][1] = scale;
	result.e[2][2] = scale;
	return result;
}

mat3 mat3::mul(mat3 left, mat3 right)
{
	mat3 result;
	for (int32_t column = 0; column < 3; column++)
	{
		for (int32_t row = 0; row < 3; row++)
		{
			float sum = 0;
			for (int32_t i = 0; i < 3; ++i)
			{
				sum += left.e[i][row] * right.e[column][i];
			}

			result.e[column][row] = sum;
		}
	}
	return result;
}

v3 mat3::mulv3(mat3 matrix, v3 vec)
{
	v3 result;
#if 0
	for (int32_t row = 0; row < 3; row++)
	{
		float sum = 0;
		for (int32_t column = 0; column < 3; column++)
		{
			sum += matrix.e[column][row] * vec.e[column ];
		}
		
		result.e[row] = sum;
	}
#else
	result.x = vec.e[0] * matrix.col[0].x;
	result.y = vec.e[0] * matrix.col[0].y;
	result.z = vec.e[0] * matrix.col[0].z;

	result.x += vec.e[1] * matrix.col[1].x;
	result.y += vec.e[1] * matrix.col[1].y;
	result.z += vec.e[1] * matrix.col[1].z;

	result.x += vec.e[2] * matrix.col[2].x;
	result.y += vec.e[2] * matrix.col[2].y;
	result.z += vec.e[2] * matrix.col[2].z;

#endif
	return result;
}

mat3 mat3::rotate(float angle, v3 axis)
{
	mat3 result = diagonal(1);

	float sinTheta = f32sin(KZM_DEGTORAD(angle));
	float cosTheta = f32cos(KZM_DEGTORAD(angle));
	float cosValue = 1.0f - cosTheta;

	result.e[0][0] = (axis.x * axis.x * cosValue) + cosTheta;
	result.e[0][1] = (axis.x * axis.y * cosValue) + (axis.z * sinTheta);
	result.e[0][2] = (axis.x * axis.z * cosValue) - (axis.y * sinTheta);

	result.e[1][0] = (axis.y * axis.x * cosValue) - (axis.z * sinTheta);
	result.e[1][1] = (axis.y * axis.y * cosValue) + cosTheta;
	result.e[1][2] = (axis.y * axis.z * cosValue) + (axis.x * sinTheta);

	result.e[2][0] = (axis.z * axis.x * cosValue) + (axis.y * sinTheta);
	result.e[2][1] = (axis.z * axis.y * cosValue) - (axis.x * sinTheta);
	result.e[2][2] = (axis.z * axis.z * cosValue) + cosTheta;

	return result;
}

mat4 mat3::tomat4(mat3 matrix)
{
	mat4 result = mat4::diagonal(1);
	for (int32_t row = 0; row < 3; row++)
	{
		result.col[row].xyz = matrix.col[row];
	}
	return result;
}

uint64_t NextPowerOf2(uint64_t value)
{
	uint64_t result = value;
	if (result)
	{
		result--;
		result |= result >> 1;
		result |= result >> 2;
		result |= result >> 4;
		result |= result >> 8;
		result |= result >> 16;
		result |= result >> 32;
		result++;
	}
	return result;
}

#ifdef DEBUG
#define ASSERT_EQ(a, b) assert((a) == (b))

/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

/*
 * This code is derived from the full C implementation, which is in turn
 * derived from the canonical C++ PCG implementation. The C++ version
 * has many additional features and is preferable if you can use C++ in
 * your project.
 */

struct pcg_state_setseq_64
{                   // Internals are *Private*.
	uint64_t state; // RNG state.  All values are possible.
	uint64_t inc;   // Controls which RNG sequence (stream) is
					// selected. Must *always* be odd.
};
typedef struct pcg_state_setseq_64 pcg32_random_t;

// If you *must* statically initialize it, here's one.

#define PCG32_INITIALIZER {0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL}

// pcg32_srandom(initstate, initseq)
// pcg32_srandom_r(rng, initstate, initseq):
//     Seed the rng.  Specified in two parts, state initializer and a
//     sequence selection constant (a.k.a. stream id)

static void pcg32_srandom(uint64_t initstate, uint64_t initseq);
static void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate, uint64_t initseq);

// pcg32_random()
// pcg32_random_r(rng)
//     Generate a uniformly distributed 32-bit random number

static uint32_t pcg32_random(void);
static uint32_t pcg32_random_r(pcg32_random_t *rng);

// pcg32_boundedrand(bound):
// pcg32_boundedrand_r(rng, bound):
//     Generate a uniformly distributed number, r, where 0 <= r < bound

static uint32_t pcg32_boundedrand(uint32_t bound);
static uint32_t pcg32_boundedrand_r(pcg32_random_t *rng, uint32_t bound);

static pcg32_random_t pcg32_global = PCG32_INITIALIZER;

// pcg32_srandom(initstate, initseq)
// pcg32_srandom_r(rng, initstate, initseq):
//     Seed the rng.  Specified in two parts, state initializer and a
//     sequence selection constant (a.k.a. stream id)

static void pcg32_srandom_r(pcg32_random_t *rng, uint64_t initstate, uint64_t initseq)
{
	rng->state = 0U;
	rng->inc = (initseq << 1u) | 1u;
	pcg32_random_r(rng);
	rng->state += initstate;
	pcg32_random_r(rng);
}

static void pcg32_srandom(uint64_t seed, uint64_t seq)
{
	pcg32_srandom_r(&pcg32_global, seed, seq);
}

// pcg32_random()
// pcg32_random_r(rng)
//     Generate a uniformly distributed 32-bit random number

static uint32_t pcg32_random_r(pcg32_random_t *rng)
{
	uint64_t oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
	uint32_t xorshifted = (uint32_t)(((oldstate >> 18u) ^ oldstate) >> 27u);
	uint32_t rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31));
}

static uint32_t pcg32_random()
{
	return pcg32_random_r(&pcg32_global);
}

// pcg32_boundedrand(bound):
// pcg32_boundedrand_r(rng, bound):
//     Generate a uniformly distributed number, r, where 0 <= r < bound

static uint32_t pcg32_boundedrand_r(pcg32_random_t *rng, uint32_t bound)
{
	// To avoid bias, we need to make the range of the RNG a multiple of
	// bound, which we do by dropping output less than a threshold.
	// A naive scheme to calculate the threshold would be to do
	//
	//     uint32_t threshold = 0x100000000ull % bound;
	//
	// but 64-bit div/mod is slower than 32-bit div/mod (especially on
	// 32-bit platforms).  In essence, we do
	//
	//     uint32_t threshold = (0x100000000ull-bound) % bound;
	//
	// because this version will calculate the same modulus, but the LHS
	// value is less than 2^32.

	uint32_t threshold = (0u - bound) % bound;

	// Uniformity guarantees that this loop will terminate.  In practice, it
	// should usually terminate quickly; on average (assuming all bounds are
	// equally likely), 82.25% of the time, we can expect it to require just
	// one iteration.  In the worst case, someone passes a bound of 2^31 + 1
	// (i.e., 2147483649), which invalidates almost 50% of the range.  In
	// practice, bounds are typically small and only a tiny amount of the range
	// is eliminated.
	for (;;)
	{
		uint32_t r = pcg32_random_r(rng);
		if (r >= threshold)
		{
			return r % bound;
		}
	}
}

static uint32_t pcg32_boundedrand(uint32_t bound)
{
	return pcg32_boundedrand_r(&pcg32_global, bound);
}

// PCG source END

static float RandF32_(pcg32_random_t *rng)
{
	int32_t integer = (int32_t)pcg32_random_r(rng);
	float frac = (float)pcg32_random_r(rng) / (float)UINT32_MAX;
	float result = (float)integer + frac;
	return result;
}

static int32_t RandS32_(pcg32_random_t *rng)
{
	int32_t result = (int32_t)pcg32_random_r(rng);
	return result;
}

static void KzMathsTestInner(pcg32_random_t *rng)
{
	// NOTE(GameChaos): first, test unions
	{
		v2 test = {{12555, -24050}};
		ASSERT_EQ(test.x, 12555);
		ASSERT_EQ(test.y, -24050);

		ASSERT_EQ(test.x, test.e[0]);
		ASSERT_EQ(test.y, test.e[1]);

		static_assert(Q_ARRAYSIZE(test.e) == 2, "");
		static_assert(sizeof(test) == 8, "");
	}
	{
		v3 test = {{12555, -24050, -9824}};
		ASSERT_EQ(test.x, 12555);
		ASSERT_EQ(test.y, -24050);
		ASSERT_EQ(test.z, -9824);

		ASSERT_EQ(test.r, 12555);
		ASSERT_EQ(test.g, -24050);
		ASSERT_EQ(test.b, -9824);

		ASSERT_EQ(test.x, test.e[0]);
		ASSERT_EQ(test.y, test.e[1]);
		ASSERT_EQ(test.z, test.e[2]);

		ASSERT_EQ(test.x, test.r);
		ASSERT_EQ(test.y, test.g);
		ASSERT_EQ(test.z, test.b);

		static_assert(Q_ARRAYSIZE(test.e) == 3, "");
		static_assert(sizeof(test) == 12, "");
	}
	{
		v4 test = {{12555, -24050, -9824, -1002}};
		ASSERT_EQ(test.x, 12555);
		ASSERT_EQ(test.y, -24050);
		ASSERT_EQ(test.z, -9824);
		ASSERT_EQ(test.w, -1002);

		ASSERT_EQ(test.r, 12555);
		ASSERT_EQ(test.g, -24050);
		ASSERT_EQ(test.b, -9824);
		ASSERT_EQ(test.a, -1002);

		ASSERT_EQ(test.x, test.e[0]);
		ASSERT_EQ(test.y, test.e[1]);
		ASSERT_EQ(test.z, test.e[2]);
		ASSERT_EQ(test.w, test.e[3]);

		ASSERT_EQ(test.x, test.r);
		ASSERT_EQ(test.y, test.g);
		ASSERT_EQ(test.z, test.b);
		ASSERT_EQ(test.w, test.a);

		ASSERT_EQ(test.x, test.xyz.x);
		ASSERT_EQ(test.y, test.xyz.y);
		ASSERT_EQ(test.z, test.xyz.z);

		ASSERT_EQ(test.x, test.rgb.x);
		ASSERT_EQ(test.y, test.rgb.y);
		ASSERT_EQ(test.z, test.rgb.z);

		static_assert(Q_ARRAYSIZE(test.e) == 4, "");
		static_assert(sizeof(test) == 16, "");
		static_assert(sizeof(test.xyz) == 12, "");
		static_assert(sizeof(test.rgb) == 12, "");
	}
	{
		v2i test = {{12555, -24050}};
		ASSERT_EQ(test.x, 12555);
		ASSERT_EQ(test.y, -24050);

		ASSERT_EQ(test.x, test.e[0]);
		ASSERT_EQ(test.y, test.e[1]);

		static_assert(Q_ARRAYSIZE(test.e) == 2, "");
		static_assert(sizeof(test) == 8, "");
	}
	{
		v3i test = {{12555, -24050, -9824}};
		ASSERT_EQ(test.x, 12555);
		ASSERT_EQ(test.y, -24050);
		ASSERT_EQ(test.z, -9824);

		ASSERT_EQ(test.r, 12555);
		ASSERT_EQ(test.g, -24050);
		ASSERT_EQ(test.b, -9824);

		ASSERT_EQ(test.x, test.e[0]);
		ASSERT_EQ(test.y, test.e[1]);
		ASSERT_EQ(test.z, test.e[2]);

		ASSERT_EQ(test.x, test.r);
		ASSERT_EQ(test.y, test.g);
		ASSERT_EQ(test.z, test.b);

		static_assert(Q_ARRAYSIZE(test.e) == 3, "");
		static_assert(sizeof(test) == 12, "");
	}
	{
		v4i test = {{12555, -24050, -9824, -1002}};
		ASSERT_EQ(test.x, 12555);
		ASSERT_EQ(test.y, -24050);
		ASSERT_EQ(test.z, -9824);
		ASSERT_EQ(test.w, -1002);

		ASSERT_EQ(test.r, 12555);
		ASSERT_EQ(test.g, -24050);
		ASSERT_EQ(test.b, -9824);
		ASSERT_EQ(test.a, -1002);

		ASSERT_EQ(test.x, test.e[0]);
		ASSERT_EQ(test.y, test.e[1]);
		ASSERT_EQ(test.z, test.e[2]);
		ASSERT_EQ(test.w, test.e[3]);

		ASSERT_EQ(test.x, test.r);
		ASSERT_EQ(test.y, test.g);
		ASSERT_EQ(test.z, test.b);
		ASSERT_EQ(test.w, test.a);

		ASSERT_EQ(test.x, test.xyz.x);
		ASSERT_EQ(test.y, test.xyz.y);
		ASSERT_EQ(test.z, test.xyz.z);

		ASSERT_EQ(test.x, test.rgb.x);
		ASSERT_EQ(test.y, test.rgb.y);
		ASSERT_EQ(test.z, test.rgb.z);

		static_assert(Q_ARRAYSIZE(test.e) == 4, "");
		static_assert(sizeof(test) == 16, "");
		static_assert(sizeof(test.xyz) == 12, "");
		static_assert(sizeof(test.rgb) == 12, "");
	}

	// test functions
	{
		ASSERT_EQ((f32fma(4, 2, 6)), 14);
		ASSERT_EQ((f32fma(2, 5, -10)), 0);
	}
	{
		v2 testA = {{RandF32_(rng), RandF32_(rng)}};
		v2 testB = {{RandF32_(rng), RandF32_(rng)}};
		float scalarB = RandF32_(rng);
		int32_t elements = 2;
		{
			v2 result = v2::add(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + testB.e[i]);
			}
		}
		{
			v2 result = v2::adds(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v2 result = v2::sub(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - testB.e[i]);
			}
		}
		{
			v2 result = v2::subs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v2 result = v2::mul(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * testB.e[i]);
			}
		}
		{
			v2 result = v2::muls(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v2 result = v2::div(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / testB.e[i]);
			}
		}

		{
			v2 result = v2::divs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / scalarB);
			}
		}
		{
			v2 result = v2::mod(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32mod(testA.e[i], testB.e[i]));
			}
		}
		{
			v2 result = v2::mods(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32mod(testA.e[i], scalarB));
			}
		}
		{
			v2 result = v2::max(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				float val = KZM_MAX(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			v2 result = v2::min(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				float val = KZM_MIN(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			assert(v2::equals((v2) {{0, 1}}, (v2) {{0, 1}}));
			assert(!v2::equals((v2) {{1, 1}}, (v2) {{0, 1}}));
			assert(!v2::equals((v2) {{0, 2}}, (v2) {{0, 1}}));
		}
		{
			v2 result = v2::sign(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32sign(testA.e[i]));
			}
		}
		{
			v2 result = v2::abs(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], KZM_ABS(testA.e[i]));
			}
		}
		{
			v2i result = v2::tov2i(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], (int32_t)testA.e[i]);
			}
		}

		{
			v2 result = v2::floor(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32floor(testA.e[i]));
			}
		}
		{
			v2 result = v2::ceil(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32ceil(testA.e[i]));
			}
		}
		{
			v2 result = v2::round(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32round(testA.e[i]));
			}
		}
	}
	{
		v3 testA = {{RandF32_(rng), RandF32_(rng), RandF32_(rng)}};
		v3 testB = {{RandF32_(rng), RandF32_(rng), RandF32_(rng)}};
		v3 testC = {{RandF32_(rng), RandF32_(rng), RandF32_(rng)}};
		float scalarB = RandF32_(rng);
		int32_t elements = 3;
		{
			v3 result = v3::add(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + testB.e[i]);
			}
		}
		{
			v3 result = v3::adds(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v3 result = v3::sub(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - testB.e[i]);
			}
		}
		{
			v3 result = v3::subs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v3 result = v3::mul(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * testB.e[i]);
			}
		}
		{
			v3 result = v3::muls(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v3 result = v3::div(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / testB.e[i]);
			}
		}

		{
			v3 result = v3::divs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / scalarB);
			}
		}
		{
			assert(v3::equals(v3 {{0, 1, 2}}, v3 {{0, 1, 2}}));
			assert(!v3::equals(v3 {{1, 1, 2}}, v3 {{0, 1, 2}}));
			assert(!v3::equals(v3 {{0, 2, 2}}, v3 {{0, 1, 2}}));
			assert(!v3::equals(v3 {{0, 1, 3}}, v3 {{0, 1, 2}}));
		}
		{
			v3 result = v3::negate(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], -testA.e[i]);
			}
		}
		{
			ASSERT_EQ(v3::dot(testA, testB), (testA.x * testB.x + testA.y * testB.y + testA.z * testB.z));
		}
		{
			v3 result = v3::mod(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32mod(testA.e[i], testB.e[i]));
			}
		}
		{
			v3 result = v3::mods(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32mod(testA.e[i], scalarB));
			}
		}
		{
			v3 result = v3::max(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				float val = KZM_MAX(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			v3 result = v3::min(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				float val = KZM_MIN(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			v3 result = v3::fma(testA, testB, testC);
			for (int32_t i = 0; i < elements; i++)
			{
				float val = f32fma(testA[i], testB[i], testC[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			v3 result = v3::sign(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32sign(testA.e[i]));
			}
		}
		{
			v3 result = v3::abs(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], KZM_ABS(testA.e[i]));
			}
		}
		{
			v3i result = v3::tov3i(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], (int32_t)testA.e[i]);
			}
		}

		{
			v3 result = v3::floor(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32floor(testA.e[i]));
			}
		}
		{
			v3 result = v3::ceil(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32ceil(testA.e[i]));
			}
		}
		{
			v3 result = v3::round(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				// TODO: expected for all tests
				float expected = f32round(testA.e[i]);
				ASSERT_EQ(result.e[i], expected);
			}
		}
		{
			v3 unNormalised = V3(0, 0, 16);
			v3 expected = V3(0, 0, 1);
			v3 result = v3::normalise(unNormalised);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], expected.e[i]);
			}
		}
		{
			v3 result = V3(0, 0, 16);
			float length = v3::normInPlace(&result);
			ASSERT_EQ(length, 16);
			v3 expected = V3(0, 0, 1);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], expected.e[i]);
			}
		}
		{
			assert(v3::lensq(testA) == (testA.x * testA.x + testA.y * testA.y + testA.z * testA.z));
			assert(v3::len(testA) == sqrtf(testA.x * testA.x + testA.y * testA.y + testA.z * testA.z));
			assert(v3::len(testB) == sqrtf(testB.x * testB.x + testB.y * testB.y + testB.z * testB.z));
			assert(v3::len(testC) == sqrtf(testC.x * testC.x + testC.y * testC.y + testC.z * testC.z));
		}
	}
	{
		v4 testA = {{RandF32_(rng), RandF32_(rng), RandF32_(rng), RandF32_(rng)}};
		v4 testB = {{RandF32_(rng), RandF32_(rng), RandF32_(rng), RandF32_(rng)}};
		float scalarB = RandF32_(rng);
		int32_t elements = 4;
		{
			v4 result = v4::add(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + testB.e[i]);
			}
		}
		{
			v4 result = v4::adds(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v4 result = v4::sub(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - testB.e[i]);
			}
		}
		{
			v4 result = v4::subs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v4 result = v4::mul(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * testB.e[i]);
			}
		}
		{
			v4 result = v4::muls(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v4 result = v4::div(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / testB.e[i]);
			}
		}

		{
			v4 result = v4::divs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / scalarB);
			}
		}
		{
			v4 result = v4::mod(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32mod(testA.e[i], testB.e[i]));
			}
		}
		{
			v4 result = v4::mods(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32mod(testA.e[i], scalarB));
			}
		}
		{
			v4 result = v4::max(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				float val = KZM_MAX(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			v4 result = v4::min(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				float val = KZM_MIN(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			assert(v4::equals((v4) {{0, 1, 2, 3}}, (v4) {{0, 1, 2, 3}}));
			assert(!v4::equals((v4) {{1, 1, 2, 3}}, (v4) {{0, 1, 2, 3}}));
			assert(!v4::equals((v4) {{0, 2, 2, 3}}, (v4) {{0, 1, 2, 3}}));
			assert(!v4::equals((v4) {{0, 1, 3, 3}}, (v4) {{0, 1, 2, 3}}));
			assert(!v4::equals((v4) {{0, 1, 3, 5}}, (v4) {{0, 1, 2, 3}}));
		}
		{
			v4 result = v4::sign(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32sign(testA.e[i]));
			}
		}
		{
			v4 result = v4::abs(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], KZM_ABS(testA.e[i]));
			}
		}
		{
			v4i result = v4::tov4i(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], (int32_t)testA.e[i]);
			}
		}

		{
			v4 result = v4::floor(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32floor(testA.e[i]));
			}
		}
		{
			v4 result = v4::ceil(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], f32ceil(testA.e[i]));
			}
		}
		{
			v4 result = v4::round(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				float expected = f32round(testA.e[i]);
				ASSERT_EQ(result.e[i], expected);
			}
		}
	}
	// integer vectors
	{
		v2i testA = {{RandS32_(rng), RandS32_(rng)}};
		v2i testB = {{RandS32_(rng), RandS32_(rng)}};
		int32_t scalarB = RandS32_(rng);
		int32_t elements = 2;
		{
			v2i result = v2i::add(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + testB.e[i]);
			}
		}
		{
			v2i result = v2i::adds(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v2i result = v2i::sub(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - testB.e[i]);
			}
		}
		{
			v2i result = v2i::subs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v2i result = v2i::mul(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * testB.e[i]);
			}
		}
		{
			v2i result = v2i::muls(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v2i result = v2i::div(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / testB.e[i]);
			}
		}

		{
			v2i result = v2i::divs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / scalarB);
			}
		}
		{
			v2i result = v2i::mod(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32mod(testA.e[i], testB.e[i]));
			}
		}
		{
			v2i result = v2i::mods(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32mod(testA.e[i], scalarB));
			}
		}
		{
			v2i result = v2i::max(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				int32_t val = KZM_MAX(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			v2i result = v2i::min(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				int32_t val = KZM_MIN(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			assert(v2i::equals((v2i) {{0, 1}}, (v2i) {{0, 1}}));
			assert(!v2i::equals((v2i) {{1, 1}}, (v2i) {{0, 1}}));
			assert(!v2i::equals((v2i) {{0, 2}}, (v2i) {{0, 1}}));
		}
		{
			v2i result = v2i::sign(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32sign(testA.e[i]));
			}
		}
		{
			v2i result = v2i::abs(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], KZM_ABS(testA.e[i]));
			}
		}
		{
			v2 result = v2i::tov2(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], (float)testA.e[i]);
			}
		}

		{
			v2i result = v2i::divfloor(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32divfloor(testA.e[i], testB.e[i]));
			}
		}

		{
			v2i result = v2i::divfloors(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32divfloor(testA.e[i], scalarB));
			}
		}

		{
			v2i result = v2i::divceil(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32divceil(testA.e[i], testB.e[i]));
			}
		}

		{
			v2i result = v2i::divceils(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				// TODO:
				int32_t expected = i32divceil(testA.e[i], scalarB);
				ASSERT_EQ(result.e[i], expected);
			}
		}
	}
	{
		v3i testA = {{RandS32_(rng), RandS32_(rng), RandS32_(rng)}};
		v3i testB = {{RandS32_(rng), RandS32_(rng), RandS32_(rng)}};
		int32_t scalarB = RandS32_(rng);
		int32_t elements = 3;
		{
			v3i result = v3i::add(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + testB.e[i]);
			}
		}
		{
			v3i result = v3i::adds(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v3i result = v3i::sub(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - testB.e[i]);
			}
		}
		{
			v3i result = v3i::subs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v3i result = v3i::mul(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * testB.e[i]);
			}
		}
		{
			v3i result = v3i::muls(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v3i result = v3i::div(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / testB.e[i]);
			}
		}

		{
			v3i result = v3i::divs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / scalarB);
			}
		}
		{
			v3i result = v3i::mod(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32mod(testA.e[i], testB.e[i]));
			}
		}
		{
			v3i result = v3i::mods(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32mod(testA.e[i], scalarB));
			}
		}
		{
			v3i result = v3i::max(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				int32_t val = KZM_MAX(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			v3i result = v3i::min(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				int32_t val = KZM_MIN(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			assert(v3i::equals((v3i) {{0, 1, 2}}, (v3i) {{0, 1, 2}}));
			assert(!v3i::equals((v3i) {{1, 1, 2}}, (v3i) {{0, 1, 2}}));
			assert(!v3i::equals((v3i) {{0, 2, 2}}, (v3i) {{0, 1, 2}}));
			assert(!v3i::equals((v3i) {{0, 1, 3}}, (v3i) {{0, 1, 2}}));
		}
		{
			v3i result = v3i::sign(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32sign(testA.e[i]));
			}
		}
		{
			v3i result = v3i::abs(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], KZM_ABS(testA.e[i]));
			}
		}
		{
			v3 result = v3i::tov3(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], (float)testA.e[i]);
			}
		}

		{
			v3i result = v3i::divfloor(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32divfloor(testA.e[i], testB.e[i]));
			}
		}

		{
			v3i result = v3i::divfloors(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32divfloor(testA.e[i], scalarB));
			}
		}

		{
			v3i result = v3i::divceil(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32divceil(testA.e[i], testB.e[i]));
			}
		}

		{
			v3i result = v3i::divceils(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				// TODO:
				int32_t expected = i32divceil(testA.e[i], scalarB);
				ASSERT_EQ(result.e[i], expected);
			}
		}
	}
	{
		v4i testA = {{RandS32_(rng), RandS32_(rng), RandS32_(rng), RandS32_(rng)}};
		v4i testB = {{RandS32_(rng), RandS32_(rng), RandS32_(rng), RandS32_(rng)}};
		int32_t scalarB = RandS32_(rng);
		int32_t elements = 4;
		{
			v4i result = v4i::add(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + testB.e[i]);
			}
		}
		{
			v4i result = v4i::adds(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v4i result = v4i::sub(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - testB.e[i]);
			}
		}
		{
			v4i result = v4i::subs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v4i result = v4i::mul(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * testB.e[i]);
			}
		}
		{
			v4i result = v4i::muls(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v4i result = v4i::div(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / testB.e[i]);
			}
		}

		{
			v4i result = v4i::divs(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / scalarB);
			}
		}
		{
			v4i result = v4i::mod(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32mod(testA.e[i], testB.e[i]));
			}
		}
		{
			v4i result = v4i::mods(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32mod(testA.e[i], scalarB));
			}
		}
		{
			v4i result = v4i::max(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				int32_t val = KZM_MAX(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			v4i result = v4i::min(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				int32_t val = KZM_MIN(testA.e[i], testB.e[i]);
				ASSERT_EQ(result.e[i], val);
			}
		}
		{
			assert(v4i::equals((v4i) {{0, 1, 2, 3}}, (v4i) {{0, 1, 2, 3}}));
			assert(!v4i::equals((v4i) {{1, 1, 2, 3}}, (v4i) {{0, 1, 2, 3}}));
			assert(!v4i::equals((v4i) {{0, 2, 2, 3}}, (v4i) {{0, 1, 2, 3}}));
			assert(!v4i::equals((v4i) {{0, 1, 3, 3}}, (v4i) {{0, 1, 2, 3}}));
			assert(!v4i::equals((v4i) {{0, 1, 3, 5}}, (v4i) {{0, 1, 2, 3}}));
		}
		{
			v4i result = v4i::sign(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32sign(testA.e[i]));
			}
		}
		{
			v4i result = v4i::abs(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], KZM_ABS(testA.e[i]));
			}
		}
		{
			v4 result = v4i::tov4(testA);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], (float)testA.e[i]);
			}
		}

		{
			v4i result = v4i::divfloor(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32divfloor(testA.e[i], testB.e[i]));
			}
		}

		{
			v4i result = v4i::divfloors(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				// TODO:
				int32_t expected = i32divfloor(testA.e[i], scalarB);
				ASSERT_EQ(result.e[i], expected);
			}
		}

		{
			v4i result = v4i::divceil(testA, testB);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], i32divceil(testA.e[i], testB.e[i]));
			}
		}

		{
			v4i result = v4i::divceils(testA, scalarB);
			for (int32_t i = 0; i < elements; i++)
			{
				// TODO:
				int32_t expected = i32divceil(testA.e[i], scalarB);
				ASSERT_EQ(result.e[i], expected);
			}
		}
	}

	// test c++ operators and functions
	{
		v3 testA = {{RandF32_(rng), RandF32_(rng), RandF32_(rng)}};
		v3 testB = {{RandF32_(rng), RandF32_(rng), RandF32_(rng)}};
		v3 testC = {{RandF32_(rng), RandF32_(rng), RandF32_(rng)}};
		float scalarB = RandF32_(rng);
		int32_t elements = 3;

		{
			ASSERT_EQ(testA.x, testA[0]);
			ASSERT_EQ(testA.y, testA[1]);
			ASSERT_EQ(testA.z, testA[2]);
			ASSERT_EQ(testB.x, testB[0]);
			ASSERT_EQ(testB.y, testB[1]);
			ASSERT_EQ(testB.z, testB[2]);
			ASSERT_EQ(testC.x, testC[0]);
			ASSERT_EQ(testC.y, testC[1]);
			ASSERT_EQ(testC.z, testC[2]);
		}
		{
			v3 result = testA + testB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + testB.e[i]);
			}
		}
		{
			v3 result = testA + scalarB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v3 result = scalarB + testA;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v3 result = testA - testB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - testB.e[i]);
			}
		}
		{
			v3 result = testA - scalarB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v3 result = scalarB - testA;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v3 result = testA * testB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * testB.e[i]);
			}
		}
		{
			v3 result = testA * scalarB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v3 result = scalarB * testA;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v3 result = testA / testB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / testB.e[i]);
			}
		}
		{
			v3 result = testA / scalarB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / scalarB);
			}
		}
		{
			v3 result = scalarB / testA;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], scalarB / testA.e[i]);
			}
		}
		{
			v3 result = -testA;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], -testA.e[i]);
			}
		}
		{
			assert((v3 {{0, 1, 2}} == v3 {{0, 1, 2}}));
			assert((v3 {{1, 1, 2}} != v3 {{0, 1, 2}}));
			assert((v3 {{0, 2, 2}} != v3 {{0, 1, 2}}));
			assert((v3 {{0, 1, 3}} != v3 {{0, 1, 2}}));
			assert((V3(0, 1, 2) == v3 {{0, 1, 2}}));
			Vector testAVec = testA.Vec();
			Vector testBVec = testB.Vec();
			assert(V3(testAVec) == testA);
			assert(V3(testBVec) == testB);
			assert(testAVec.x == testA.x && testAVec.y == testA.y && testAVec.z == testA.z);
			assert(testBVec.x == testB.x && testBVec.y == testB.y && testBVec.z == testB.z);
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(testAVec[i], testA.e[i]);
				ASSERT_EQ(testBVec[i], testB.e[i]);
			}
			assert((V3(V2(0, 1), 2) == v3 {{0, 1, 2}}));
			assert((V3(42) == v3 {{42, 42, 42}}));
		}

		{
			v3 result = testA;
			result += testB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + testB.e[i]);
			}
		}
		{
			v3 result = testA;
			result += scalarB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] + scalarB);
			}
		}
		{
			v3 result = testA;
			result -= testB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - testB.e[i]);
			}
		}
		{
			v3 result = testA;
			result -= scalarB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] - scalarB);
			}
		}
		{
			v3 result = testA;
			result *= testB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * testB.e[i]);
			}
		}
		{
			v3 result = testA;
			result *= scalarB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] * scalarB);
			}
		}
		{
			v3 result = testA;
			result /= testB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / testB.e[i]);
			}
		}

		{
			v3 result = testA;
			result /= scalarB;
			for (int32_t i = 0; i < elements; i++)
			{
				ASSERT_EQ(result.e[i], testA.e[i] / scalarB);
			}
		}
	}
}

void KzMathsTest()
{
	pcg32_random_t rng = {};
	pcg32_srandom_r(&rng, __rdtsc() ^ (intptr_t)&KzMathsTest, 54);
	for (int32_t i = 0; i < 100; i++)
	{
		KzMathsTestInner(&rng);
	}
}

#undef ASSERT_EQ

#endif // DEBUG
