#pragma once

#include "lin_alg.h"
#include <cmath>

struct vec2 {
	float x, y;
	vec2(float xx, float yy) : x(xx), y(yy) {}
	vec2() {};
	vec2 operator+(const vec2 &v) const {
		return vec2(this->x + v.x, this->y + v.y);
	}

	vec2 operator-(const vec2 &v) const {
		return vec2(this->x - v.x, this->y - v.y);
	}

	float length() const {
		return sqrt(x*x + y*y);
	}
};

vec2 operator*(float c, const vec2& v);
vec2 operator*(const vec2& v, float c);

struct mat24 { // 2 columns, 4 rows
	vec4 columns[2];
	
	mat24() {};
	mat24(const vec4 &C0, const vec4 &C1) : columns{ C0, C1 } {};
	mat24(const vec2 &R0, const vec2 &R1, const vec2 &R2, const vec2 &R3) {
		columns[0] = vec4(R0.x, R1.x, R2.x, R3.x);
		columns[1] = vec4(R0.y, R1.y, R2.y, R3.y);
	}
};


struct BEZIER4 {

	static const mat4 weights;

	vec2 control_points[4];
	mat24 matrix_repr;
	vec2 evaluate(float t);
	float dydx(float t, float dt = 0.001);
	float dxdt(float t, float dt = 0.001);
	float dydt(float t, float dt = 0.001);

	BEZIER4(const vec2 &P0, const vec2 &P1, const vec2 &P2, const vec2 &P3);
};