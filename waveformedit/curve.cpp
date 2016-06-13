#include "curve.h"

const mat4 BEZIER4::weights = mat4(
	vec4(1, -3, 3, -1), 
	vec4(0, 3, -6, 3), 
	vec4(0, 0, 3, -3), 
	vec4(0, 0, 0, 1));

const mat4 CATMULLROM4::weights = mat4(
	vec4(1, 0, -3, 2),
	vec4(0, 0, 3, -2),
	vec4(0, 1, -2, 1),
	vec4(0, 0, -1, 1));

static inline vec2 bezier2(const vec2 &a, const vec2 &b, float t) {
	return (1 - t)*a + t*b;
}

static inline vec2 bezier3(const vec2 &a, const vec2 &b, const vec2 &c, float t) {
	return bezier2(bezier2(a, b, t), bezier2(b, c, t), t);
}

static inline vec2 bezier4(const vec2 &a, const vec2 &b, const vec2 &c, const vec2 &d, float t) {
	return bezier2(bezier3(a, b, c, t), bezier3(b, c, d, t), t);
}

static inline mat24 multiply44_24(const mat4 &M44, const mat24 &M24) {
	return mat24(M44*M24.columns[0], M44*M24.columns[1]);
}

static inline vec2 multiply4_24(const vec4 &V4, const mat24 &M24) {
	return vec2(dot4(V4, M24.columns[0]), dot4(V4, M24.columns[1]));
}

vec2 operator*(float c, const vec2& v) {
	return vec2(c*v.x, c*v.y);
}

vec2 operator*(const vec2& v, float c) {
	return c * v;
}

vec2 BEZIER4::evaluate(float t) {
	//return bezier4(control_points[0], control_points[1], control_points[2], control_points[3], t);
	float t2 = t*t;
	float t3 = t2*t;
	vec4 tv(1, t, t2, t3);

	return multiply4_24(tv, matrix_repr);
}

float BEZIER4::dydx(float t, float dt) {

	t = t + dt > 1.0 ? t - dt : t;
	t = t - dt < 0.0 ? t + dt : t;

	vec2 p1 = evaluate(t + dt);
	vec2 p0 = evaluate(t);

	return (p1.y - p0.y) / (p1.x - p0.x);

}

float BEZIER4::dxdt(float t, float dt) {
	t = t + dt > 1.0 ? t - dt : t;
	t = t - dt < 0.0 ? t + dt : t;
	
	vec2 p1 = evaluate(t + dt);
	vec2 p0 = evaluate(t);
	
	return (p1.x - p0.x) / dt;
}

float BEZIER4::dydt(float t, float dt) {
	t = t + dt > 1.0 ? t - dt : t;
	t = t - dt < 0.0 ? t + dt : t;

	vec2 p1 = evaluate(t + dt);
	vec2 p0 = evaluate(t);

	return (p1.y - p0.y) / dt;
}

BEZIER4::BEZIER4(const vec2 &aP0, const vec2 &aP1, const vec2 &aP2, const vec2 &aP3) 
	: P0(aP0), P1(aP1), P2(aP2), P3(aP3) {

	matrix_repr = multiply44_24(BEZIER4::weights, mat24(P0, P1, P2, P3));
};

CATMULLROM4 BEZIER4::convert_to_CATMULLROM4() {
	return CATMULLROM4(
		P3 + 6 * (P0 - P1),
		P0,
		P3,
		P1 + 6 * (P3 - P2),
		1);
}

CATMULLROM4::CATMULLROM4(const vec2 &aP0, const vec2 &aP1, const vec2 &aP2, const vec2 &aP3, float a_tension)
: P0(aP0), P1(aP1), P2(aP2), P3(aP3), tension(a_tension) {
	const float &T = tension; // just an alias

	mat4 intermediate = 0.5*mat4(
		vec4(0, 0, -T, 0), 
		vec4(2, 0, 0, -T), 
		vec4(0, 2, T, 0), 
		vec4(0, 0, 0, T));
	
	mat24 P(P0, P1, P2, P3);

	matrix_repr = multiply44_24(weights, multiply44_24(intermediate, P));
}

vec2 CATMULLROM4::evaluate(float s) {
	float s2 = s*s;
	float s3 = s2*s;

	vec4 S(1, s, s2, s3);

	return multiply4_24(S, matrix_repr);
}

BEZIER4 CATMULLROM4::convert_to_BEZIER4() {
	float coef = 1.0 / (6.0*tension);
	return BEZIER4(
		P1,
		P1 + coef*(P2 - P0),
		P2 + coef*(P3 - P1),
		P2);

}