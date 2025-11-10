/*
* Copyright (c) 2016-2019, Super GP Individual.
* All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this library for any
* purpose and without fee is hereby granted, provided that the above
* copyright notice appear in all copies and that both the copyright
* notice and this permission notice appear in supporting documentation,
* and that the name of SGL not be used in advertising or publicity
* pertaining to distribution of the software without specific,
* written prior permission.
*/



#ifndef SGL_GEOMETRY
#define SGL_GEOMETRY


//geometry const macros

#define PI 3.1415926535897932


typedef struct {
	int x, y;
}vec2i;
typedef struct {
	float x, y;
}vec2f;
typedef struct {
	int x, y, z;
}vec3i;
typedef struct {
	float x, y, z;
}vec3f;
typedef struct {
	int x, y, z, w;
}vec4i;
typedef struct {
	float x, y, z, w;
}vec4f;

vec2i Vec2i(int x, int y);
vec2f Vec2f(float x, float y);
vec3i Vec3i(int x, int y, int z);
vec3f Vec3f(float x, float y, float z);
vec4i Vec4i(int x, int y, int z, int w);
vec4f Vec4f(float x, float y, float z, float w);

#ifdef __cplusplus

#include <istream>
#include <vector>

#define VECN(Tv) (sizeof(Tv) /sizeof(Tv::x))


using std::vector;

vec3i Vec3i(vec2i v, int z);
vec3f Vec3f(vec2f v, float z);
vec4i Vec4i(vec3i v, int w);
vec4f Vec4f(vec3f v, float w);

vec2f normalize(vec2f v);
vec3f normalize(vec3f v);
vec4f normalize(vec4f v);
float square(vec2f v);
float square(vec3f v);
float square(vec4f v);
float length(vec2f v);
float length(vec3f v);
float length(vec4f v);

#endif

#endif