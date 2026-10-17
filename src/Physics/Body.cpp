/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Body.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "3D/ObjectMatrix.h"
#include "Ground.h"

using namespace openblack;
using namespace openblack::physics;

namespace
{
float Altitude(const Ground& ground, glm::vec3 point)
{
	return ground.HeightAt(glm::vec2(point.x, point.z));
}

/// The landscape normal under the point
glm::vec3 Normal(const Ground& ground, glm::vec3 point)
{
	return ground.NormalAt(glm::vec2(point.x, point.z));
}

/// TouchGround: the landscape cell nearest the point is not open sea
bool CellHasLand(const Ground& ground, glm::vec3 point)
{
	return !ground.IsSeaCell(glm::vec2(point.x, point.z));
}

float LengthSquared(glm::vec3 v)
{
	return glm::dot(v, v);
}
} // namespace

void Body::Initialise(float scale, float meshHeight)
{
	_scale = scale;
	_centreOfMass = glm::vec3(0.0f);
	_radius = 0.0f;
	angularMomentum = glm::vec3(0.0f);
	velocity = glm::vec3(0.0f);
	_speed = 0.0f;
	// scale x the drawn mesh's half extent y x 1000, as a float, rounded to nearest,
	// negated; Initialise runs in AddObject / AddProxy before the body is set up
	_restCounter = -static_cast<int>(std::nearbyint(scale * meshHeight * 1000.0f));
}

void Body::SetUpConstants(float mass, const PhysicsData& data, bool dynamic)
{
	_mass = mass;
	density = data.density;
	_stiffness = mass * data.contact;
	_damping = mass * data.penetration;
	_friction = data.friction;
	// pow(keep, (double)0.005f) on the FPU, then stored as a float. (approximate, last
	// bit) the original's pow at single FPU precision is not emulated: here a double pow rounded once to float
	_spinKeptPerStep = static_cast<float>(std::pow(static_cast<double>(data.angularKeep), 0.0050000000745058061));
	_drag = data.drag;
	_dynamic = dynamic;
}

void Body::Build(const Shape& shape, const glm::mat3& rotation, glm::vec3 origin)
{
	_points.assign(shape.points.size(), Point {});
	for (size_t i = 0; i < shape.points.size(); ++i)
	{
		_points[i].local = shape.points[i];
	}
	_centreOfMass = shape.centreOfMass;
	_radius = shape.radius;
	_faces.clear();
	_faces.reserve(shape.faces.size());
	for (const auto& t : shape.faces)
	{
		_faces.push_back(Face {t});
	}
	SetUpInertia();
	SetUpPos(rotation, origin);
}

void Body::BuildShape(std::span<const glm::vec3> local, std::span<const std::array<uint32_t, 3>> triangles, glm::vec3 com,
                      float radius, float dragFactor, const glm::mat3& rotation, glm::vec3 origin, float inertiaFactor,
                      bool rowsScaled)
{
	_points.assign(local.size(), Point {});
	for (size_t i = 0; i < local.size(); ++i)
	{
		_points[i].local = local[i];
	}
	_faces.clear();
	for (const auto& t : triangles)
	{
		_faces.push_back(Face {t});
	}
	_centreOfMass = com;
	_radius = radius;
	SetUpInertia();
	_drag *= dragFactor;
	if (_dynamic && inertiaFactor != 1.0f)
	{
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				_inertia[i][j] *= inertiaFactor;
				_inverseInertia[i][j] /= inertiaFactor;
			}
		}
	}
	SetUpPos(rotation, origin, rowsScaled);
}

void Body::FollowObject(glm::vec3 objectPosition)
{
	// the original's order: each axis summed over the unscaled rows, then times the scale, then added to the position
	const auto& r0 = _axes[0];
	const auto& r1 = _axes[1];
	const auto& r2 = _axes[2];
	const float tx = ((r2.x * _centreOfMass.z) + (r1.x * _centreOfMass.y)) + (_centreOfMass.x * r0.x);
	const float ty = ((r2.y * _centreOfMass.z) + (r0.y * _centreOfMass.x)) + (r1.y * _centreOfMass.y);
	const float tz = ((r2.z * _centreOfMass.z) + (r0.z * _centreOfMass.x)) + (r1.z * _centreOfMass.y);
	_centre = glm::vec3((tx * _scale) + objectPosition.x, (ty * _scale) + objectPosition.y, (tz * _scale) + objectPosition.z);
}

void Body::SetUpInertia()
{
	externalForce = glm::vec3(0.0f);
	externalTorque = glm::vec3(0.0f);
	for (auto& q : _points)
	{
		q.length = glm::length(q.local);
		q.aheadLength = q.length;
	}
	if (_dynamic && !_points.empty())
	{
		auto& I = _inertia;
		I = {};
		const float mi = _mass / static_cast<float>(_points.size());
		for (const auto& q : _points)
		{
			const float x = q.local.x;
			const float y = q.local.y;
			const float z = q.local.z;
			I[0][0] += (y * y + z * z) * mi;
			I[0][1] -= x * y * mi;
			I[0][2] -= x * z * mi;
			I[1][0] -= x * y * mi;
			I[1][1] += (x * x + z * z) * mi;
			I[1][2] -= x * z * mi; // the original's bug: x z instead of y z
			I[2][0] -= x * z * mi;
			I[2][1] -= y * z * mi;
			I[2][2] += (x * x + y * y) * mi;
		}
		// glm's [column][row] holds the transpose; the inverse of the transpose is the transpose of the inverse, so the
		// indexing carries over
		glm::mat3 m;
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				m[i][j] = I[i][j];
			}
		}
		const auto inv = glm::determinant(m) != 0.0f ? glm::inverse(m) : glm::mat3(1.0f);
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				_inverseInertia[i][j] = inv[i][j];
			}
		}
		_drag *= _radius * _radius * 0.3f;
	}
	else
	{
		_inertia = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
		_inverseInertia = _inertia;
	}
	inWater = false;
	touched = false;
	justSetUp = true;
}

void Body::SetUpPos(const glm::mat3& rotation, glm::vec3 origin, bool rowsScaled)
{
	// the body's rows = the object's matrix, its rows scaled (the unit rows times the scale, or as given)
	glm::mat3 rows = rotation;
	if (!rowsScaled)
	{
		for (int k = 0; k < 3; ++k)
		{
			rows[k] = rotation[k] * _scale;
		}
	}
	// the centre of mass offset from the scaled rows, in the original's order
	const float tx = ((rows[1].x * _centreOfMass.y + rows[2].x * _centreOfMass.z) + _centreOfMass.x * rows[0].x);
	const float ty = ((rows[1].y * _centreOfMass.y + rows[2].y * _centreOfMass.z) + rows[0].y * _centreOfMass.x);
	const float tz = ((rows[1].z * _centreOfMass.y + rows[2].z * _centreOfMass.z) + rows[0].z * _centreOfMass.x);
	// each row times 1 / sqrt((z z + y y) + x x), unless it is all zero
	for (int k = 0; k < 3; ++k)
	{
		auto& r = rows[k];
		if (!(r.x == 0.0f && r.y == 0.0f && r.z == 0.0f))
		{
			const float inv = 1.0f / std::sqrt((r.z * r.z + r.y * r.y) + r.x * r.x);
			r = glm::vec3(inv * r.x, inv * r.y, inv * r.z);
		}
	}
	_axes = rows;
	// the translation += t
	_centre = glm::vec3(origin.x + tx, origin.y + ty, origin.z + tz);
	for (auto& q : _points)
	{
		// world_i = ((r2_i z + r1_i y) + r0_i x) + T_i with the normalised rows, then the contact
		// anchor = world
		q.world = glm::vec3(((_axes[2].x * q.local.z + q.local.y * _axes[1].x) + q.local.x * _axes[0].x) + _centre.x,
		                    ((_axes[2].y * q.local.z + _axes[1].y * q.local.y) + _axes[0].y * q.local.x) + _centre.y,
		                    ((_axes[2].z * q.local.z + _axes[1].z * q.local.y) + _axes[0].z * q.local.x) + _centre.z);
		q.contact = q.world;
		q.firstDepth = 0.0f;
		q.depth = 0.0f;
	}
	_contacts = 0;
	for (auto& f : _faces)
	{
		const auto& a = _points[f.indices[0]].local;
		const auto& b = _points[f.indices[1]].local;
		const auto& c = _points[f.indices[2]].local;
		const auto n = glm::cross(b - a, c - a);
		f.localNormal = LengthSquared(n) > 0.0f ? glm::normalize(n) : glm::vec3(0.0f);
		f.worldNormal = _axes * f.localNormal;
	}
}

void Body::SettleOnLand(const Ground& ground, bool noPullDown, bool alignToSlope)
{
	if (alignToSlope)
	{
		// the normal at the centre's map coordinates, in fixed point (the same cell as
		// LandIsland::GetNormalAt)
		const auto n = Normal(ground, _centre);
		const auto& r0 = _axes[0];
		glm::vec3 f(n.z * r0.y - n.y * r0.z, n.x * r0.z - n.z * r0.x, n.y * r0.x - n.x * r0.y); // r0 x n
		if (!(f.x == 0.0f && f.y == 0.0f && f.z == 0.0f))
		{
			const float inv = 1.0f / std::sqrt((f.z * f.z + f.y * f.y) + f.x * f.x);
			f = glm::vec3(inv * f.x, inv * f.y, inv * f.z);
		}
		_axes[0] = glm::vec3(f.z * n.y - f.y * n.z, n.z * f.x - f.z * n.x, f.y * n.x - n.y * f.x); // n x f
		_axes[1] = n;
		_axes[2] = f;
	}
	float lowest = 1000.0f;
	for (const auto& q : _points)
	{
		// ((r2 z + r1 y) + r0 x) + T for x, ((r1 y + r0 x) + r2 z) + T for y and z
		const glm::vec3 world(((q.local.z * _axes[2].x + q.local.y * _axes[1].x) + _axes[0].x * q.local.x) + _centre.x,
		                      ((_axes[1].y * q.local.y + q.local.x * _axes[0].y) + _axes[2].y * q.local.z) + _centre.y,
		                      ((q.local.x * _axes[0].z + q.local.z * _axes[2].z) + q.local.y * _axes[1].z) + _centre.z);
		const float d = world.y - Altitude(ground, world);
		if (d < lowest)
		{
			lowest = d;
		}
	}
	if (noPullDown && lowest > 0.0f)
	{
		lowest = 0.0f;
	}
	_centre.y -= lowest;
	for (auto& q : _points)
	{
		q.world = _axes * q.local + _centre;
	}
	for (auto& f : _faces)
	{
		f.worldNormal = _axes * f.localNormal;
	}
}

void Body::ClearForces()
{
	touched = false;
	if (resting)
	{
		_aheadCentre = _centre;
		_force = glm::vec3(0.0f);
		_torque = glm::vec3(0.0f);
		return;
	}
	const auto ahead = velocity * k_LookAheadSeconds;
	for (auto& q : _points)
	{
		q.world = _axes * q.local + _centre + ahead;
		q.aheadLength = std::max(glm::length(q.world - _centre), 0.001f);
	}
	_aheadCentre = _centre + ahead;
	for (auto& f : _faces)
	{
		f.worldNormal = _axes * f.localNormal;
	}
	_force = externalForce;
	_torque = externalTorque;
}

void Body::TouchGround(const Ground& ground)
{
	if (resting)
	{
		for (auto& q : _points)
		{
			q.contact = q.world;
			q.depth = 0.0f;
		}
		return;
	}
	_force -= velocity * (_speed * _drag);
	_contacts = 0;
	_force.y -= _mass * k_Gravity;

	const bool water = Altitude(ground, _centre) < 0.0001f && _centre.y < _radius && !_points.empty() &&
	                   !CellHasLand(ground, _points.front().world);
	if (water)
	{
		const auto submerged = std::count_if(_points.begin(), _points.end(), [](const Point& q) { return q.world.y < 0.0f; });
		if (submerged > 0)
		{
			inWater = true;
			touched = true;
			const float fraction = std::min((_radius - _centre.y) / (2.0f * _radius), 1.0f);
			density += 6.66667e-05f; // waterlogged (about 1/15000 a substep)
			const float buoyancy = fraction * _mass * k_Gravity / density;
			const float d = fraction * _speed * _drag * 100.0f;
			const glm::vec3 waterForce(-d * velocity.x, buoyancy - d * velocity.y, -d * velocity.z);
			_force += waterForce;
			const auto share = waterForce * (0.02f / static_cast<float>(submerged));
			for (auto& q : _points)
			{
				q.contact = q.world;
				q.depth = 0.0f;
				if (q.world.y < 0.0f)
				{
					_torque += glm::cross(q.world - _aheadCentre, share);
				}
			}
			// no landscape contact while any vertex is under the sea
			return;
		}
		inWater = false;
	}
	for (auto& q : _points)
	{
		const float altitude = Altitude(ground, q.world);
		q.depth = altitude - q.world.y;
		q.other = nullptr;
		q.contact = q.world;
		if (q.depth >= 0.0f)
		{
			++_contacts;
			q.normal = Normal(ground, q.world);
			q.contact.y = altitude;
			touched = true;
		}
	}
}

bool Body::RaySegmentVsFaces(glm::vec3 point, glm::vec3 direction, glm::vec3& hit, glm::vec3& normal) const
{
	// the segment from point - direction (t = -1, the other body's centre) to point (t = 0); the entry
	// nearest to the point wins
	float best = -1.0f;
	bool found = false;
	for (const auto& f : _faces)
	{
		const float dn = glm::dot(direction, f.worldNormal);
		if (dn >= -0.0001f)
		{
			continue;
		}
		const auto& v0 = _points[f.indices[0]].world;
		const float t = -glm::dot(point - v0, f.worldNormal) / dn;
		if (t <= -1.0f || t >= 0.0f || t <= best)
		{
			continue;
		}
		const auto p = point + t * direction;
		const auto& v1 = _points[f.indices[1]].world;
		const auto& v2 = _points[f.indices[2]].world;
		// strictly inside all three edges
		if (glm::dot(glm::cross(v1 - v0, p - v0), f.worldNormal) <= 0.0f ||
		    glm::dot(glm::cross(v2 - v1, p - v1), f.worldNormal) <= 0.0f ||
		    glm::dot(glm::cross(v0 - v2, p - v2), f.worldNormal) <= 0.0f)
		{
			continue;
		}
		best = t;
		hit = p;
		normal = f.worldNormal;
		found = true;
	}
	return found;
}

bool Body::RayBehindVsFaces(glm::vec3 point, glm::vec3 direction, glm::vec3& hit) const
{
	// best starts at -10000; the plane normal is the raw cross product of the world
	// vertices, so the -0.0001 threshold is on the unnormalised one; t < 0 and nearest to 0 wins.
	// The original's mesh-collide branch of OverlapDepth is not used by openblack's bodies.
	float best = -10000.0f;
	bool found = false;
	for (const auto& f : _faces)
	{
		const auto& v0 = _points[f.indices[0]].world;
		const auto& v1 = _points[f.indices[1]].world;
		const auto& v2 = _points[f.indices[2]].world;
		const auto n = glm::cross(v1 - v0, v2 - v0);
		const float dn = glm::dot(n, direction);
		if (!(dn < -0.0001f))
		{
			continue;
		}
		const float t = -glm::dot(point - v0, n) / dn;
		if (!(t < 0.0f) || !(t > best))
		{
			continue;
		}
		const auto p = point + t * direction;
		if (glm::dot(glm::cross(v1 - v0, p - v0), n) <= 0.0f || glm::dot(glm::cross(v2 - v1, p - v1), n) <= 0.0f ||
		    glm::dot(glm::cross(v0 - v2, p - v2), n) <= 0.0f)
		{
			continue;
		}
		best = t;
		hit = p;
		found = true;
	}
	return found;
}

float Body::OverlapDepth(const Body& other, glm::vec3 direction) const
{
	float deepest = 0.0f;
	for (const auto& q : _points)
	{
		glm::vec3 hit;
		if (other.RayBehindVsFaces(q.world, direction, hit))
		{
			deepest = std::max(deepest, glm::dot(q.world - hit, direction));
		}
	}
	return deepest;
}

void Body::TouchFaces(Body& b)
{
	const float radius2 = b._radius * b._radius;
	for (auto& q : _points)
	{
		if (LengthSquared(q.world - b._centre) >= radius2)
		{
			continue;
		}
		const auto direction = q.world - _centre;
		glm::vec3 hit;
		glm::vec3 normal;
		if (!b.RaySegmentVsFaces(q.world, direction, hit, normal))
		{
			continue;
		}
		++_contacts;
		q.contact = hit;
		q.other = &b;
		q.normal = normal;
		// the predicted length (from ClearForces, at
		// least 0.001), not length: depth = aheadLength - ((hit - centre) . dir) / aheadLength, so the 0.06 s look-ahead stops
		// fast bodies at thin walls (the physical shield)
		const float depth = q.aheadLength - glm::dot(hit - _centre, direction) / q.aheadLength;
		if (depth > q.depth)
		{
			q.depth = depth;
			if (!(resting && b.resting))
			{
				lastHit = &b;
				touched = true;
				b.lastHit = this;
				b.touched = true;
			}
		}
	}
}

void Body::ApplyContacts()
{
	_restCounter += 5;
	justSetUp = false;
	if (_contacts == 0)
	{
		return;
	}
	for (auto& q : _points)
	{
		if (q.depth <= 0.0f)
		{
			q.anchor = q.contact;
			q.firstDepth = 0.0f;
			continue;
		}
		float k = _stiffness;
		float c = _damping;
		float mu = _friction;
		if (q.other != nullptr)
		{
			k = std::min(k, q.other->_stiffness);
			c = std::min(c, q.other->_damping);
			mu = std::min(mu, q.other->_friction) * 0.3f;
		}
		float normalForce = 0.0f;
		if (q.firstDepth == 0.0f)
		{
			q.firstDepth = q.depth;
			normalForce = q.depth * k;
		}
		else
		{
			normalForce =
			    k * q.firstDepth + c * (q.depth - q.firstDepth) * 200.0f * (q.aheadLength / std::max(q.length, 1e-6f));
		}
		normalForce = std::max(normalForce, 0.0f);
		const float maxFriction = normalForce * mu;
		auto contactForce = q.normal * normalForce;
		auto friction = (q.anchor - q.contact) * k;
		if (LengthSquared(friction) > maxFriction * maxFriction)
		{
			friction *= maxFriction / glm::length(friction);
			if (k > 0.0f)
			{
				q.anchor = q.contact + friction / k;
			}
		}
		contactForce += friction;
		q.normal = contactForce;
		_force += contactForce;
		_torque += glm::cross(q.world - _aheadCentre, contactForce);
		if (q.other != nullptr)
		{
			q.other->_force -= contactForce;
			q.other->_torque += glm::cross(q.world - q.other->_centre, -contactForce);
		}
	}
}

glm::vec3 Body::BodyOmega() const
{
	const auto l = glm::transpose(_axes) * angularMomentum;
	glm::vec3 w(0.0f);
	for (int j = 0; j < 3; ++j)
	{
		w[j] = l.x * _inverseInertia[0][j] + l.y * _inverseInertia[1][j] + l.z * _inverseInertia[2][j];
	}
	return w;
}

void Body::SetAngularVelocity(glm::vec3 omega)
{
	const auto w = glm::transpose(_axes) * omega;
	glm::vec3 l(0.0f);
	for (int j = 0; j < 3; ++j)
	{
		l[j] = w.x * _inertia[0][j] + w.y * _inertia[1][j] + w.z * _inertia[2][j];
	}
	angularMomentum = _axes * l;
}

glm::vec3 Body::HandAngularMomentum(const std::array<std::array<float, 3>, 3>& inertia, glm::vec3 h)
{
	const auto& I = inertia;
	const float x = ((I[0][0] * h.x + I[2][0] * h.z) + I[1][0] * h.y) + 0.0f;
	const float y = ((I[2][1] * h.z + I[0][1] * h.x) + I[1][1] * h.y) + 0.0f;
	const float z = ((I[2][2] * h.z + I[0][2] * h.x) + I[1][2] * h.y) + 0.0f;
	return -glm::vec3(x, y, z);
}

void Body::SetAngularMomentumFromHand(glm::vec3 h)
{
	angularMomentum = HandAngularMomentum(_inertia, h);
}

glm::vec3 Body::DrawOrigin(const glm::mat3& rows, glm::vec3 centre, float scale, glm::vec3 com)
{
	const glm::vec3 m0 = rows[0] * scale;
	const glm::vec3 m1 = rows[1] * scale;
	const glm::vec3 m2 = rows[2] * scale;
	const float t0 = ((com.z * m2.x) + (com.y * m1.x)) + (com.x * m0.x);
	const float t1 = ((com.z * m2.y) + (com.y * m1.y)) + (com.x * m0.y);
	const float t2 = ((com.z * m2.z) + (com.y * m1.z)) + (com.x * m0.z);
	return glm::vec3(centre.x - t0, centre.y - t1, centre.z - t2);
}

glm::vec3 Body::DrawOrigin(const glm::mat3& rows, glm::vec3 centre) const
{
	return DrawOrigin(rows, centre, _scale, _centreOfMass);
}

void Body::SetAngularVelocityFromBody(glm::vec3 w)
{
	const auto& I = _inertia;
	const float tx = ((I[1][0] * w.y + I[2][0] * w.z) + I[0][0] * w.x) + 0.0f;
	const float ty = ((I[0][1] * w.x + I[1][1] * w.y) + I[2][1] * w.z) + 0.0f;
	const float tz = ((I[0][2] * w.x + I[1][2] * w.y) + I[2][2] * w.z) + 0.0f;
	glm::vec3 l;
	for (int i = 0; i < 3; ++i)
	{
		l[i] = (tz * _axes[2][i] + ty * _axes[1][i]) + tx * _axes[0][i];
	}
	angularMomentum = -l;
}

StepResult Body::Integrate()
{
	if (!_dynamic || (resting && !touched))
	{
		return StepResult::None;
	}
	if (_centre.y < -4.0f * _radius)
	{
		return StepResult::Delete;
	}
	angularMomentum += _torque * k_StepSeconds;
	angularMomentum *= _spinKeptPerStep;
	auto wl = BodyOmega();
	const float w2 = LengthSquared(wl);
	if (w2 > k_MaxSpin * k_MaxSpin)
	{
		wl *= k_MaxSpin / std::sqrt(w2);
	}
	const auto omega = _axes * wl;
	velocity += _force * (k_StepSeconds / _mass);
	_speed = glm::length(velocity);
	if (_speed > k_MaxSpeed)
	{
		velocity *= k_MaxSpeed / _speed;
		_speed = k_MaxSpeed;
	}

	// rest: at least 1 (4 once resting), growing after 15000 counts
	const float threshold = resting ? 4.0f : (_restCounter > 15000 ? static_cast<float>(_restCounter) * 6.66667e-05f : 1.0f);
	const auto torquePerInertia = _torque / (_radius * _radius * _mass);
	if (_contacts > 0 && threshold > _speed && threshold * threshold * 0.25f > w2 &&
	    threshold * threshold > LengthSquared(torquePerInertia))
	{
		if (resting)
		{
			velocity = glm::vec3(0.0f);
			_speed = 0.0f;
			angularMomentum = glm::vec3(0.0f);
			return StepResult::None;
		}
		if (_restCounter > 0)
		{
			velocity = glm::vec3(0.0f);
			_speed = 0.0f;
			angularMomentum = glm::vec3(0.0f);
			return StepResult::Stopped;
		}
	}
	const auto step = omega * k_StepSeconds;
	const float angle = glm::length(step);
	if (angle > 1e-05f)
	{
		// inv = 1 / angle, axis = step inv, M = the axis-angle rotation
		// (affine::AxisAngle = glm::rotate(-angle)) and the rows times M (r_k' = r_k M, glm's
		// M * R). The original's torque is F x r, openblack's r x F, so its
		// omega and axis are the opposite of openblack's: the original's axis is -(step inv)
		const float inv = 1.0f / angle;
		_axes = affine::AxisAngle(-(step * inv), angle) * _axes;
	}
	_centre += velocity * k_StepSeconds;
	return resting ? StepResult::Knocked : StepResult::Moved;
}

glm::vec3 Body::ObjectOrigin() const
{
	// the body's rows (glm's columns) times the scale, then the translation -= t
	const glm::vec3 r0 = _axes[0] * _scale;
	const glm::vec3 r1 = _axes[1] * _scale;
	const glm::vec3 r2 = _axes[2] * _scale;
	const float t0 = ((r2.x * _centreOfMass.z) + (r1.x * _centreOfMass.y)) + (_centreOfMass.x * r0.x);
	const float t1 = ((r2.y * _centreOfMass.z) + (r0.y * _centreOfMass.x)) + (r1.y * _centreOfMass.y);
	const float t2 = ((r2.z * _centreOfMass.z) + (r0.z * _centreOfMass.x)) + (r1.z * _centreOfMass.y);
	return glm::vec3(_centre.x - t0, _centre.y - t1, _centre.z - t2);
}

glm::vec3 Body::ObjectOrigin(const glm::mat3& rotation, glm::vec3 centre) const
{
	return centre - rotation * (_centreOfMass * _scale);
}
