#include "winsgl.h"

vec2i Vec2i(int x, int y) {
	vec2i ret;
	ret.x = x;
	ret.y = y;
	return ret;
}
vec2f Vec2f(float x, float y) {
	vec2f ret;
	ret.x = x;
	ret.y = y;
	return ret;
}
vec3i Vec3i(int x, int y, int z) {
	vec3i ret;
	ret.x = x;
	ret.y = y;
	ret.z = z;
	return ret;
}
vec3f Vec3f(float x, float y, float z) {
	vec3f ret;
	ret.x = x;
	ret.y = y;
	ret.z = z;
	return ret;
}
vec4i Vec4i(int x, int y, int z, int w) {
	vec4i ret;
	ret.x = x;
	ret.y = y;
	ret.z = z;
	ret.w = w;
	return ret;
}
vec4f Vec4f(float x, float y, float z, float w) {
	vec4f ret;
	ret.x = x;
	ret.y = y;
	ret.z = z;
	ret.w = w;
	return ret;
}

vec3i Vec3i(vec2i v, int z) {
	vec3i ret;
	ret.x = v.x;
	ret.y = v.y;
	ret.z = z;
	return ret;
}
vec3f Vec3f(vec2f v, float z) {
	vec3f ret;
	ret.x = v.x;
	ret.y = v.y;
	ret.z = z;
	return ret;
}
vec4i Vec4i(vec3i v, int w) {
	vec4i ret;
	ret.x = v.x;
	ret.y = v.y;
	ret.z = v.z;
	ret.w = w;
	return ret;
}
vec4f Vec4f(vec3f v, float w) {
	vec4f ret;
	ret.x = v.x;
	ret.y = v.y;
	ret.z = v.z;
	ret.w = w;
	return ret;
}

vec2f normalize(vec2f v) {
	float length = sqrt(v.x * v.x + v.y * v.y);
	return Vec2f(v.x / length, v.y / length);
}
vec3f normalize(vec3f v) {
	float length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return Vec3f(v.x / length, v.y / length, v.z / length);
}
vec4f normalize(vec4f v) {
	float length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
	return Vec4f(v.x / length, v.y / length, v.z / length, v.w / length);
}
float square(vec2f v) {
	return v.x * v.x + v.y * v.y;
}
float square(vec3f v) {
	return v.x * v.x + v.y * v.y + v.z * v.z;
}
float square(vec4f v) {
	return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}
float length(vec2f v) {
	return sqrt(v.x * v.x + v.y * v.y);
}
float length(vec3f v) {
	return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
float length(vec4f v) {
	return sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}
