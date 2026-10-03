/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PhysOb.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::physics;

namespace
{
const LandIslandInterface* Terrain()
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}

float Altitude(glm::vec3 point)
{
	const auto* terrain = Terrain();
	return terrain != nullptr ? terrain->GetHeightAt(glm::vec2(point.x, point.z)) : 0.0f;
}

/// LH3DIsland::GetNormal (0x803630): LandIsland::GetNormalAt (land_normal::OfCell); up without an island
glm::vec3 Normal(glm::vec3 point)
{
	const auto* terrain = Terrain();
	return terrain != nullptr ? terrain->GetNormalAt(glm::vec2(point.x, point.z)) : glm::vec3(0.0f, 1.0f, 0.0f);
}

/// GroundAndWater: the landscape cell under the point (x 0.1, 0..511) has an altitude of at least 1.
bool CellHasLand(glm::vec3 point)
{
	const auto* terrain = Terrain();
	if (terrain == nullptr)
	{
		return true;
	}
	const auto last = static_cast<float>(terrain->GetCellsPerSide() - 1);
	const auto cell = glm::u16vec2(glm::clamp(glm::vec2(point.x, point.z) * 0.1f, glm::vec2(0.0f), glm::vec2(last)));
	return terrain->GetCellAltitude(terrain->GetCell(cell)) >= 1;
}

float LengthSquared(glm::vec3 v)
{
	return glm::dot(v, v);
}
} // namespace

glm::vec3 openblack::ecs::physics::LandscapeNormal(glm::vec3 point)
{
	return Normal(point);
}

void PhysOb::Initialise(float scale, float meshHeight)
{
	_scale = scale;
	_com = glm::vec3(0.0f);
	_radius = 0.0f;
	angularMomentum = glm::vec3(0.0f);
	velocity = glm::vec3(0.0f);
	_speed = 0.0f;
	restCounter = -static_cast<int>(scale * meshHeight * 1000.0f); // 0x7FB7D6: mesh+0x28 is the half height
}

void PhysOb::SetUpConstants(float mass, const PhysicsData& data, bool dynamic)
{
	_mass = mass;
	density = data.density;
	_kContact = mass * data.contact;
	_kPenetration = mass * data.penetration;
	_friction = data.friction;
	_angularDampStep = std::pow(data.angularKeep, k_Dt);
	_drag = data.drag;
	_dynamic = dynamic;
}

void PhysOb::Build(std::span<const glm::vec3> positions, std::span<const std::array<uint32_t, 3>> triangles,
                   const glm::mat3& rotation, glm::vec3 origin)
{
	_vertices.assign(positions.size(), Vertex {});
	_com = glm::vec3(0.0f);
	for (const auto& p : positions)
	{
		_com += p;
	}
	if (!positions.empty())
	{
		_com /= static_cast<float>(positions.size());
	}
	_radius = 0.0f;
	for (size_t i = 0; i < positions.size(); ++i)
	{
		_vertices[i].local = (positions[i] - _com) * _scale;
		_radius = std::max(_radius, glm::length(_vertices[i].local));
	}
	_faces.clear();
	_faces.reserve(triangles.size());
	for (const auto& t : triangles)
	{
		if (t[0] < _vertices.size() && t[1] < _vertices.size() && t[2] < _vertices.size())
		{
			_faces.push_back(Face {t});
		}
	}
	SetUpMoi();
	SetUpPos(rotation, origin);
}

void PhysOb::BuildShape(std::span<const glm::vec3> local, std::span<const std::array<uint32_t, 3>> triangles, glm::vec3 com,
                        float radius, float dragFactor, const glm::mat3& rotation, glm::vec3 origin, float inertiaFactor)
{
	_vertices.assign(local.size(), Vertex {});
	for (size_t i = 0; i < local.size(); ++i)
	{
		_vertices[i].local = local[i];
	}
	_faces.clear();
	for (const auto& t : triangles)
	{
		_faces.push_back(Face {t});
	}
	_com = com;
	_radius = radius;
	SetUpMoi();
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
	SetUpPos(rotation, origin);
}

void PhysOb::SetUpMoi()
{
	externalForce = glm::vec3(0.0f);
	externalTorque = glm::vec3(0.0f);
	for (auto& q : _vertices)
	{
		q.len = glm::length(q.local);
		q.predLen = q.len;
	}
	if (_dynamic && !_vertices.empty())
	{
		auto& I = _inertia;
		I = {};
		const float mi = _mass / static_cast<float>(_vertices.size());
		for (const auto& q : _vertices)
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
		_drag *= _radius * _radius * 0.3f; // 0x8AB23C
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

void PhysOb::SetUpPos(const glm::mat3& rotation, glm::vec3 origin)
{
	_rotation = rotation;
	for (int i = 0; i < 3; ++i)
	{
		_rotation[i] = glm::normalize(_rotation[i]);
	}
	_centre = origin + _rotation * (_com * _scale);
	for (auto& q : _vertices)
	{
		q.world = _rotation * q.local + _centre;
		q.contact = q.world;
		q.pen0 = 0.0f;
		q.pen = 0.0f;
	}
	numContacts = 0;
	for (auto& f : _faces)
	{
		const auto& a = _vertices[f.indices[0]].local;
		const auto& b = _vertices[f.indices[1]].local;
		const auto& c = _vertices[f.indices[2]].local;
		const auto n = glm::cross(b - a, c - a);
		f.localNormal = LengthSquared(n) > 0.0f ? glm::normalize(n) : glm::vec3(0.0f);
		f.worldNormal = _rotation * f.localNormal;
	}
}

void PhysOb::AdjustToGroundLevel(bool noPullDown, bool alignToNormal)
{
	if (alignToNormal)
	{
		const auto n = Normal(_centre);
		auto forward = glm::cross(_rotation[0], n);
		if (LengthSquared(forward) > 1e-8f)
		{
			forward = glm::normalize(forward);
			_rotation[2] = forward;
			_rotation[0] = glm::cross(n, forward);
			_rotation[1] = n;
		}
	}
	float lowest = 1000.0f;
	for (const auto& q : _vertices)
	{
		const auto world = _rotation * q.local + _centre;
		lowest = std::min(lowest, world.y - Altitude(world));
	}
	if (noPullDown && lowest > 0.0f)
	{
		lowest = 0.0f;
	}
	_centre.y -= lowest;
	for (auto& q : _vertices)
	{
		q.world = _rotation * q.local + _centre;
	}
	for (auto& f : _faces)
	{
		f.worldNormal = _rotation * f.localNormal;
	}
}

void PhysOb::ZeroForces()
{
	touched = false;
	if (resting)
	{
		_predictedCentre = _centre;
		force = glm::vec3(0.0f);
		torque = glm::vec3(0.0f);
		return;
	}
	const auto ahead = velocity * k_LookAhead;
	for (auto& q : _vertices)
	{
		q.world = _rotation * q.local + _centre + ahead;
		q.predLen = std::max(glm::length(q.world - _centre), 0.001f);
	}
	_predictedCentre = _centre + ahead;
	for (auto& f : _faces)
	{
		f.worldNormal = _rotation * f.localNormal;
	}
	force = externalForce;
	torque = externalTorque;
}

void PhysOb::GroundAndWater()
{
	if (resting)
	{
		for (auto& q : _vertices)
		{
			q.contact = q.world;
			q.pen = 0.0f;
		}
		return;
	}
	force -= velocity * (_speed * _drag);
	numContacts = 0;
	force.y -= _mass * k_Gravity;

	const bool water = Altitude(_centre) < 0.0001f && _centre.y < _radius && !_vertices.empty() &&
	                   !CellHasLand(_vertices.front().world);
	if (water)
	{
		const auto submerged = std::count_if(_vertices.begin(), _vertices.end(), [](const Vertex& q) { return q.world.y < 0.0f; });
		if (submerged > 0)
		{
			inWater = true;
			touched = true;
			const float fraction = std::min((_radius - _centre.y) / (2.0f * _radius), 1.0f);
			density += 6.66667e-05f; // 0x9A2BC8: waterlogged
			const float buoyancy = fraction * _mass * k_Gravity / density;
			const float d = fraction * _speed * _drag * 100.0f;
			const glm::vec3 waterForce(-d * velocity.x, buoyancy - d * velocity.y, -d * velocity.z);
			force += waterForce;
			const auto share = waterForce * (0.02f / static_cast<float>(submerged));
			for (auto& q : _vertices)
			{
				q.contact = q.world;
				q.pen = 0.0f;
				if (q.world.y < 0.0f)
				{
					torque += glm::cross(q.world - _predictedCentre, share);
				}
			}
			// no landscape contact while any vertex is under the sea
			return;
		}
		inWater = false;
	}
	for (auto& q : _vertices)
	{
		const float altitude = Altitude(q.world);
		q.pen = altitude - q.world.y;
		q.other = nullptr;
		q.contact = q.world;
		if (q.pen >= 0.0f)
		{
			++numContacts;
			q.normal = Normal(q.world);
			q.contact.y = altitude;
			touched = true;
		}
	}
}

bool PhysOb::RaySegmentVsFaces(glm::vec3 point, glm::vec3 direction, glm::vec3& hit, glm::vec3& normal) const
{
	// fn_7FBF80: the segment from point - direction (t = -1, the other body's centre) to point (t = 0); the entry
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
		const auto& v0 = _vertices[f.indices[0]].world;
		const float t = -glm::dot(point - v0, f.worldNormal) / dn;
		if (t <= -1.0f || t >= 0.0f || t <= best)
		{
			continue;
		}
		const auto p = point + t * direction;
		const auto& v1 = _vertices[f.indices[1]].world;
		const auto& v2 = _vertices[f.indices[2]].world;
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

bool PhysOb::RayBehindVsFaces(glm::vec3 point, glm::vec3 direction, glm::vec3& hit) const
{
	// fn_007FC310: best starts at -10000 (0xC61C4000); the plane normal is the raw cross product of the world
	// vertices, so the -0.0001 (qword 0x9A2BC0) threshold is on the unnormalised one; t < 0 and nearest to 0 wins.
	// The mesh-collide branch of fn_007FDD60 (other +0x168, fn_008683C0) is not used by openblack's bodies.
	float best = -10000.0f;
	bool found = false;
	for (const auto& f : _faces)
	{
		const auto& v0 = _vertices[f.indices[0]].world;
		const auto& v1 = _vertices[f.indices[1]].world;
		const auto& v2 = _vertices[f.indices[2]].world;
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

float PhysOb::PenetrationAlong(const PhysOb& other, glm::vec3 direction) const
{
	float deepest = 0.0f;
	for (const auto& q : _vertices)
	{
		glm::vec3 hit;
		if (other.RayBehindVsFaces(q.world, direction, hit))
		{
			deepest = std::max(deepest, glm::dot(q.world - hit, direction));
		}
	}
	return deepest;
}

void PhysOb::CollideVertices(PhysOb& b)
{
	const float radius2 = b._radius * b._radius;
	for (auto& q : _vertices)
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
		++numContacts;
		q.contact = hit;
		q.other = &b;
		q.normal = normal;
		const float len = std::max(q.len, 1e-6f);
		const float pen = q.len - glm::dot(hit - _centre, direction) / len;
		if (pen > q.pen)
		{
			q.pen = pen;
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

void PhysOb::ContactForces()
{
	restCounter += 5;
	justSetUp = false;
	if (numContacts == 0)
	{
		return;
	}
	for (auto& q : _vertices)
	{
		if (q.pen <= 0.0f)
		{
			q.anchor = q.contact;
			q.pen0 = 0.0f;
			continue;
		}
		float k = _kContact;
		float c = _kPenetration;
		float mu = _friction;
		if (q.other != nullptr)
		{
			k = std::min(k, q.other->_kContact);
			c = std::min(c, q.other->_kPenetration);
			mu = std::min(mu, q.other->_friction) * 0.3f;
		}
		float normalForce = 0.0f;
		if (q.pen0 == 0.0f)
		{
			q.pen0 = q.pen;
			normalForce = q.pen * k;
		}
		else
		{
			normalForce = k * q.pen0 + c * (q.pen - q.pen0) * 200.0f * (q.predLen / std::max(q.len, 1e-6f));
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
		force += contactForce;
		torque += glm::cross(q.world - _predictedCentre, contactForce);
		if (q.other != nullptr)
		{
			q.other->force -= contactForce;
			q.other->torque += glm::cross(q.world - q.other->_centre, -contactForce);
		}
	}
}

glm::vec3 PhysOb::BodyOmega() const
{
	const auto l = glm::transpose(_rotation) * angularMomentum;
	glm::vec3 w(0.0f);
	for (int j = 0; j < 3; ++j)
	{
		w[j] = l.x * _inverseInertia[0][j] + l.y * _inverseInertia[1][j] + l.z * _inverseInertia[2][j];
	}
	return w;
}

void PhysOb::SetAngularVelocity(glm::vec3 omega)
{
	const auto w = glm::transpose(_rotation) * omega;
	glm::vec3 l(0.0f);
	for (int j = 0; j < 3; ++j)
	{
		l[j] = w.x * _inertia[0][j] + w.y * _inertia[1][j] + w.z * _inertia[2][j];
	}
	angularMomentum = _rotation * l;
}

PhysOb::Result PhysOb::Integrate()
{
	if (!_dynamic || (resting && !touched))
	{
		return Result::None;
	}
	if (_centre.y < -4.0f * _radius) // 0x9A2BD0
	{
		return Result::Delete;
	}
	angularMomentum += torque * k_Dt;
	angularMomentum *= _angularDampStep;
	auto wl = BodyOmega();
	const float w2 = LengthSquared(wl);
	if (w2 > k_MaxOmega * k_MaxOmega)
	{
		wl *= k_MaxOmega / std::sqrt(w2);
	}
	const auto omega = _rotation * wl;
	velocity += force * (k_Dt / _mass);
	_speed = glm::length(velocity);
	if (_speed > k_MaxSpeed)
	{
		velocity *= k_MaxSpeed / _speed;
		_speed = k_MaxSpeed;
	}

	// rest: at least 1 (4 once resting), growing after 15000 counts (0x8AB418, 0x9A2BCC)
	const float threshold = resting ? 4.0f : (restCounter > 15000 ? static_cast<float>(restCounter) * 6.66667e-05f : 1.0f);
	const auto torquePerInertia = torque / (_radius * _radius * _mass);
	if (numContacts > 0 && threshold > _speed && threshold * threshold * 0.25f > w2 &&
	    threshold * threshold > LengthSquared(torquePerInertia))
	{
		if (resting)
		{
			velocity = glm::vec3(0.0f);
			_speed = 0.0f;
			angularMomentum = glm::vec3(0.0f);
			return Result::None;
		}
		if (restCounter > 0)
		{
			velocity = glm::vec3(0.0f);
			_speed = 0.0f;
			angularMomentum = glm::vec3(0.0f);
			return Result::Stopped;
		}
	}
	const auto step = omega * k_Dt;
	const float angle = glm::length(step);
	if (angle > 1e-05f) // 0x99A100
	{
		// fn_007FE260 0x7FE706..0x7FE748: inv = 1 / angle (fdiv 0x7FE710), axis = step inv, M = fn_007FB180(axis, angle)
		// (lh_matrix::AxisAngle = glm::rotate(-angle)) and the rows times M (fn_0046D9D0 0x7FE748: r_k' = r_k M, glm's
		// M * R). The original's torque is F x r (0x7FE0F9..0x7FE11F, 0x7FD7FD..0x7FD823), openblack's r x F, so its
		// omega and axis are the opposite of openblack's: the original's axis is -(step inv)
		const float inv = 1.0f / angle;
		_rotation = lh_matrix::AxisAngle(-(step * inv), angle) * _rotation;
	}
	_centre += velocity * k_Dt;
	return resting ? Result::Pushed : Result::Moved;
}

glm::vec3 PhysOb::ObjectOrigin() const
{
	return ObjectOrigin(_rotation, _centre);
}

glm::vec3 PhysOb::ObjectOrigin(const glm::mat3& rotation, glm::vec3 centre) const
{
	return centre - rotation * (_com * _scale);
}
