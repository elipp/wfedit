#include "curve.h"

#include <xmmintrin.h>
#include <smmintrin.h>

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

	points24 = mat24(P0, P1, P2, P3);
	matrix_repr = multiply44_24(BEZIER4::weights, points24);
};

BEZIER4::BEZIER4(const mat24 &PV) 
	: points24(PV) {

	P0 = points24.row(0);
	P1 = points24.row(1);
	P2 = points24.row(2);
	P3 = points24.row(3);

	matrix_repr = multiply44_24(BEZIER4::weights, points24);
};

BEZIER4 *BEZIER4::split(float t) {
	if (t < 0.0 || t > 1.0) {
		return NULL;
	}

	float tm1 = t - 1;
	float tm2 = tm1*tm1;
	float tm3 = tm2*tm1;

	float t2 = t*t;
	float t3 = t2*t;

	// Refer to http://pomax.github.io/bezierinfo/, §10: Splitting curves using matrices.

	mat4 first(
		vec4(1, -tm1, tm2, -tm3),
		vec4(0, t, -2 * (tm1)*t, 3 * tm2*t),
		vec4(0, 0, t2, -3 * tm1*t2),
		vec4(0, 0, 0, t3)
		);

	mat4 tmp = first.transposed();

	mat4 second = mat4(
		tmp.column(3),
		vec4(_mm_shuffle_ps(tmp.column(2).getData(), tmp.column(2).getData(), _MM_SHUFFLE(3, 0, 1, 2))),
		vec4(_mm_shuffle_ps(tmp.column(1).getData(), tmp.column(1).getData(), _MM_SHUFFLE(2, 3, 0, 1))),
		vec4(_mm_shuffle_ps(tmp.column(0).getData(), tmp.column(0).getData(), _MM_SHUFFLE(1, 2, 3, 0)))
		)
		.transposed();

	mat24 C1 = multiply44_24(first, this->points24);
	mat24 C2 = multiply44_24(second, this->points24);

	BEZIER4 *ret = new BEZIER4[2];

	ret[0] = BEZIER4(C1);
	ret[1] = BEZIER4(C2);

	return ret;
	
}

CATMULLROM4 BEZIER4::convert_to_CATMULLROM4() const {
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
	
	points24 = mat24(P0, P1, P2, P3);

	matrix_repr = multiply44_24(CATMULLROM4::weights, multiply44_24(intermediate, points24));
}


CATMULLROM4::CATMULLROM4(const mat24 &PV, float a_tension)
	: points24(PV), tension(a_tension) {
	
	P0 = points24.row(0);
	P1 = points24.row(1);
	P2 = points24.row(2);
	P3 = points24.row(3);

	const float &T = tension; // just an alias

	mat4 intermediate = 0.5*mat4(
		vec4(0, 0, -T, 0),
		vec4(2, 0, 0, -T),
		vec4(0, 2, T, 0),
		vec4(0, 0, 0, T));

	matrix_repr = multiply44_24(CATMULLROM4::weights, multiply44_24(intermediate, points24));
}
vec2 CATMULLROM4::evaluate(float s) {
	float s2 = s*s;
	float s3 = s2*s;

	vec4 S(1, s, s2, s3);

	return multiply4_24(S, matrix_repr);
}


CATMULLROM4 *CATMULLROM4::split(float s) const {
	BEZIER4 B = this->convert_to_BEZIER4();
	BEZIER4 *SB = B.split(s);

	// no idea if this is correct or not XD

	CATMULLROM4 *SC = new CATMULLROM4[2];
	SC[0] = SB[0].convert_to_CATMULLROM4();
	SC[1] = SB[1].convert_to_CATMULLROM4();

	delete[] SB;
	return SC;
}

BEZIER4 CATMULLROM4::convert_to_BEZIER4() const {
	float coef = 1.0 / (6.0*tension);
	return BEZIER4(
		P1,
		P1 + coef*(P2 - P0),
		P2 + coef*(P3 - P1),
		P2);

}