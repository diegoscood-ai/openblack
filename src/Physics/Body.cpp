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
#include <bit>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "3D/AffineMatrix.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsX87.h"
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

/// A vector's length summed as the original sums it, z first, then y, then x
float LengthAsTheOriginal(glm::vec3 v)
{
	return std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
}

/// Two vectors' dot product summed as the original sums it, z first, then y, then x
float DotAsTheOriginal(glm::vec3 a, glm::vec3 b)
{
	return (a.z * b.z + a.y * b.y) + a.x * b.x;
}

float LengthSquared(glm::vec3 v)
{
	return glm::dot(v, v);
}
} // namespace

float openblack::physics::SpinKeptPerStep(float angularKeep)
{
	// The step's length as the game holds it for the power: a double a little above 0.005, not the float 0.005
	constexpr double k_StepPower = std::bit_cast<double>(uint64_t {0x3F747AE14CCCCCCD});
	return gutils::x87::Store(gutils::x87::Pow(angularKeep, k_StepPower));
}

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
	_spinKeptPerStep = SpinKeptPerStep(data.angularKeep);
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
		q.length = LengthAsTheOriginal(q.local);
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
		// inverted as the original inverts its matrices, row by row with its own order of products and a determinant
		// that is never nearer 0 than a ten-billionth
		affine::AffineMatrix rows;
		for (size_t i = 0; i < 3; ++i)
		{
			for (size_t j = 0; j < 3; ++j)
			{
				rows.m.at(3 * i + j) = I.at(i).at(j);
			}
		}
		rows.m.at(9) = rows.m.at(10) = rows.m.at(11) = 0.0f;
		const auto inv = affine::Inverse(rows);
		for (size_t i = 0; i < 3; ++i)
		{
			for (size_t j = 0; j < 3; ++j)
			{
				_inverseInertia.at(i).at(j) = inv.m.at(3 * i + j);
			}
		}
		// the radius times the drag, times the radius again, then 0.3, in the original's order
		_drag = _radius * _drag * _radius * 0.3f;
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
		f.localNormal = NormalisedAsTheOriginal(glm::cross(b - a, c - a));
		f.worldNormal = TurnByRows(_axes, f.localNormal);
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
		// a height that is not a number is taken too, as the original's compare takes it
		if (!(d >= lowest))
		{
			lowest = d;
		}
	}
	if (noPullDown && lowest > 0.0f)
	{
		lowest = 0.0f;
	}
	_centre.y -= lowest;
	// the points and faces keep where they were until the next step clears its forces, as in the original
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
		// summed as the original sums it; under a thousandth, or not a number, a thousandth
		const float ahead = LengthAsTheOriginal(q.world - _centre);
		q.aheadLength = ahead >= 0.001f ? ahead : 0.001f;
	}
	_aheadCentre = _centre + ahead;
	for (auto& f : _faces)
	{
		f.worldNormal = TurnByRows(_axes, f.localNormal);
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
			density += k_SoakPerStep; // waterlogged
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
	}
	// touching the land, it is not in the water, whether or not it was over the sea
	inWater = false;
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
	return RayAgainstFaces(_points, _faces, point, direction, hit, normal);
}

bool Body::RayAgainstFaces(std::span<const Point> points, std::span<const Face> faces, glm::vec3 point, glm::vec3 direction,
                           glm::vec3& hit, glm::vec3& normal)
{
	// the segment from point - direction (t = -1, the other body's centre) to point (t = 0); the entry
	// nearest to the point wins
	float best = -1.0f;
	bool found = false;
	for (const auto& f : faces)
	{
		// each product summed in the original's order, and every compare as the original's takes a value that is not a
		// number
		const auto& n = f.worldNormal;
		const float dn = (direction.x * n.x + direction.z * n.z) + n.y * direction.y;
		if (dn >= -0.0001f)
		{
			continue;
		}
		const auto& v0 = points[f.indices[0]].world;
		const float t = -(DotAsTheOriginal(point - v0, n) / dn);
		if (t >= 0.0f || !(t > best))
		{
			continue;
		}
		const auto p = point + t * direction;
		const auto& v1 = points[f.indices[1]].world;
		const auto& v2 = points[f.indices[2]].world;
		// strictly inside all three edges
		if (!(DotAsTheOriginal(glm::cross(v1 - v0, p - v0), n) > 0.0f) ||
		    !(DotAsTheOriginal(glm::cross(v2 - v1, p - v1), n) > 0.0f) ||
		    !(DotAsTheOriginal(glm::cross(v0 - v2, p - v2), n) > 0.0f))
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
	return RayBehindFaces(_points, _faces, point, direction, hit);
}

bool Body::RayBehindFaces(std::span<const Point> points, std::span<const Face> faces, glm::vec3 point, glm::vec3 direction,
                          glm::vec3& hit)
{
	// best starts at -10000; the plane normal is the raw cross product of the world
	// vertices, so the -0.0001 threshold is on the unnormalised one; t < 0 and nearest to 0 wins.
	// The original's mesh-collide branch of OverlapDepth is not used by openblack's bodies.
	float best = -10000.0f;
	bool found = false;
	for (const auto& f : faces)
	{
		const auto& v0 = points[f.indices[0]].world;
		const auto& v1 = points[f.indices[1]].world;
		const auto& v2 = points[f.indices[2]].world;
		const auto n = glm::cross(v1 - v0, v2 - v0);
		// the products summed in the original's order, the plane's against a ten-thousandth as a double, and each
		// edge test not passed by a value that is not a number
		const float dn = (n.x * direction.x + n.z * direction.z) + n.y * direction.y;
		if (!(static_cast<double>(dn) < -0.0001))
		{
			continue;
		}
		const float t = -(DotAsTheOriginal(n, point - v0) / dn);
		if (!(t < 0.0f) || !(t > best))
		{
			continue;
		}
		const auto p = point + t * direction;
		if (!(DotAsTheOriginal(glm::cross(v1 - v0, p - v0), n) > 0.0f) ||
		    !(DotAsTheOriginal(glm::cross(v2 - v1, p - v1), n) > 0.0f) ||
		    !(DotAsTheOriginal(glm::cross(v0 - v2, p - v2), n) > 0.0f))
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
		const auto fromOther = q.world - b._centre;
		if ((fromOther.z * fromOther.z + fromOther.y * fromOther.y) + fromOther.x * fromOther.x >= radius2)
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
		const float depth = ContactDepth(q.aheadLength, hit, _centre, direction);
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

ContactMaterial openblack::physics::MixContactMaterials(const ContactMaterial& own, const ContactMaterial& other)
{
	return {.stiffness = std::min(own.stiffness, other.stiffness),
	        .damping = std::min(own.damping, other.damping),
	        .friction = std::min(own.friction, other.friction) * 0.3f};
}

float openblack::physics::ContactPush(float stiffness, float damping, float depth, float firstDepth, float aheadLength,
                                      float length)
{
	// the damping term in the original's order of float products, then the spring's, and nothing that is not a number
	const float push = firstDepth == 0.0f
	                       ? depth * stiffness
	                       : (depth - firstDepth) * 200.0f * (aheadLength / length) * damping + stiffness * firstDepth;
	return push >= 0.0f ? push : 0.0f;
}

glm::vec3 openblack::physics::NormalisedAsTheOriginal(glm::vec3 v)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
	{
		return v;
	}
	const float inverse = 1.0f / std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
	return {inverse * v.x, inverse * v.y, inverse * v.z};
}

glm::vec3 openblack::physics::TurnByRows(const glm::mat3& rows, glm::vec3 v)
{
	return {(rows[2].x * v.z + rows[1].x * v.y) + rows[0].x * v.x, (rows[0].y * v.x + rows[1].y * v.y) + rows[2].y * v.z,
	        (rows[0].z * v.x + rows[1].z * v.y) + rows[2].z * v.z};
}

std::optional<glm::vec3> Body::PointContact(Point& q, const ContactMaterial& material)
{
	// a depth that is not above 0, or not a number, is no contact, as the original's compare makes it
	if (!(q.depth > 0.0f))
	{
		q.anchor = q.contact;
		q.firstDepth = 0.0f;
		return std::nullopt;
	}
	const float k = material.stiffness;
	const float mu = material.friction;
	const float normalForce = ContactPush(k, material.damping, q.depth, q.firstDepth, q.aheadLength, q.length);
	if (q.firstDepth == 0.0f)
	{
		q.firstDepth = q.depth;
	}
	const float maxFriction = normalForce * mu;
	auto contactForce = q.normal * normalForce;
	auto friction = (q.anchor - q.contact) * k;
	// the hold's size summed z first, and the anchor dragged by the friction times the inverse of the spring, as the
	// original works them out
	const float friction2 = (friction.z * friction.z + friction.y * friction.y) + friction.x * friction.x;
	if (friction2 > maxFriction * maxFriction)
	{
		friction *= maxFriction / std::sqrt(friction2);
		const float inverseStiffness = 1.0f / k;
		q.anchor = glm::vec3(inverseStiffness * friction.x + q.contact.x, inverseStiffness * friction.y + q.contact.y,
		                     inverseStiffness * friction.z + q.contact.z);
	}
	contactForce += friction;
	q.normal = contactForce;
	return contactForce;
}

float Body::ContactDepth(float aheadLength, glm::vec3 hit, glm::vec3 centre, glm::vec3 direction)
{
	return aheadLength - DotAsTheOriginal(hit - centre, direction) / aheadLength;
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
		ContactMaterial material {.stiffness = _stiffness, .damping = _damping, .friction = _friction};
		if (q.other != nullptr)
		{
			material = MixContactMaterials(
			    material, {.stiffness = q.other->_stiffness, .damping = q.other->_damping, .friction = q.other->_friction});
		}
		const auto pushed = PointContact(q, material);
		if (!pushed)
		{
			continue;
		}
		const auto contactForce = *pushed;
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
	// Every product and sum below is in the original's order: with the game's single precision floats the order is
	// part of the result. openblack's spin has the opposite sign of the original's (its turning force is r x F, the
	// original's F x r); every step is linear in it, so the results are the original's negated, exactly.
	angularMomentum += _torque * k_StepSeconds;
	angularMomentum *= _spinKeptPerStep;
	const auto& r0 = _axes[0];
	const auto& r1 = _axes[1];
	const auto& r2 = _axes[2];
	const auto& m = angularMomentum;
	// the spin in the body's frame, then through the inverse inertia
	const float lx = (r0.y * m.y + r0.z * m.z) + m.x * r0.x;
	const float ly = (r1.y * m.y + r1.x * m.x) + r1.z * m.z;
	const float lz = (r2.y * m.y + r2.x * m.x) + r2.z * m.z;
	const auto& n = _inverseInertia;
	glm::vec3 wl((lz * n[2][0] + ly * n[1][0]) + lx * n[0][0], (lz * n[2][1] + ly * n[1][1]) + lx * n[0][1],
	             (lz * n[2][2] + ly * n[1][2]) + lx * n[0][2]);
	const float w2 = (wl.z * wl.z + wl.y * wl.y) + wl.x * wl.x;
	if (w2 > k_MaxSpin * k_MaxSpin && !(wl.x == 0.0f && wl.y == 0.0f && wl.z == 0.0f))
	{
		wl *= k_MaxSpin / std::sqrt(w2);
	}
	// back to the world's frame
	const glm::vec3 omega((wl.z * r2.x + wl.y * r1.x) + wl.x * r0.x, (wl.z * r2.y + wl.y * r1.y) + wl.x * r0.y,
	                      (wl.z * r2.z + wl.y * r1.z) + wl.x * r0.z);
	const float inverseMass = 1.0f / _mass;
	velocity += glm::vec3(inverseMass * _force.x * k_StepSeconds, inverseMass * _force.y * k_StepSeconds,
	                      inverseMass * _force.z * k_StepSeconds);
	_speed = LengthAsTheOriginal(velocity);
	if (_speed > k_MaxSpeed)
	{
		velocity *= k_MaxSpeed / _speed;
		_speed = k_MaxSpeed;
	}

	// rest: at least 1 (4 once resting), growing after 15000 counts
	const float threshold =
	    resting ? 4.0f : (_restCounter > 15000 ? static_cast<float>(_restCounter) * k_RestLimitPerCount : 1.0f);
	const float threshold2 = threshold * threshold;
	if (_contacts > 0 && threshold > _speed && threshold2 * 0.25f > w2)
	{
		const float perInertia = 1.0f / (_radius * _radius * _mass);
		const glm::vec3 torque(perInertia * _torque.x, perInertia * _torque.y, perInertia * _torque.z);
		if (threshold2 > (torque.z * torque.z + torque.y * torque.y) + torque.x * torque.x)
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
	}
	const auto step = omega * k_StepSeconds;
	const float angle = LengthAsTheOriginal(step);
	if (angle > 1e-05f)
	{
		// the turn about the step's axis (the original's, the opposite of openblack's spin), the rows times it
		const float inv = 1.0f / angle;
		const auto turn = affine::AxisAngle(-(step * inv), angle);
		for (int k = 0; k < 3; ++k)
		{
			const glm::vec3 r = _axes[k];
			_axes[k] = glm::vec3((r.z * turn[2][0] + r.y * turn[1][0]) + r.x * turn[0][0],
			                     (r.z * turn[2][1] + r.y * turn[1][1]) + r.x * turn[0][1],
			                     (r.x * turn[0][2] + r.z * turn[2][2]) + r.y * turn[1][2]);
		}
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
