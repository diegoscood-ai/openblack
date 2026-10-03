/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::physics
{
/// LH3DIsland::GetNormal (0x803630): the flat normal of the landscape triangle under the point.
[[nodiscard]] glm::vec3 LandscapeNormal(glm::vec3 point);

/// One row of Data\PhysicsConstants.txt (EditorPhysics::PhysicsConstants 0xCC63E0), indexed by the object's
/// GetPhysicsConstantsType.
struct PhysicsData
{
	float density;     ///< relative density: buoyancy = frac m g / density, sinks above 1
	float contact;     ///< contact spring k per unit of mass
	float penetration; ///< penetration-rate stiffness per unit of mass
	float friction;    ///< Coulomb mu against the landscape (0.3 min(mu) between two bodies)
	float angularKeep; ///< fraction of the angular momentum kept after one second
	float drag;        ///< quadratic air drag (x 0.3 R^2 for dynamic bodies)
};

/// The LH3DLib rigid body (PhysOb 0x7FB730..0x7FE7B0): the vertex cloud of a mesh as point masses, penalty contacts
/// (a spring and a Coulomb friction anchor per vertex, tested 0.06 s ahead) against the landscape, the sea and the
/// triangles of other bodies, integrated with semi-implicit Euler in 0.005 s substeps.
///
/// The original accumulates torques as F x r and rotates the row axes by -|w| dt; both signs cancel, so this port uses
/// r x F and rotates the column axes by +|w| dt, which gives the same motion.
class PhysOb
{
public:
	static constexpr float k_Dt = 0.005f;        // 0x8C7674
	static constexpr int k_SubstepsPerTurn = 20; // GameTurnUpdate 0x646046: 20 x 0.005 = one 0.1 s game turn
	static constexpr float k_Gravity = 9.81f;    // 0x8CF02C
	static constexpr float k_LookAhead = 0.06f;  // 0x8CA27C
	static constexpr float k_MaxSpeed = 124.0f;  // 0xC371FC, also the hand's throw cap
	static constexpr float k_MaxOmega = 9.42478f; // 0x9A2BB0, 3 pi

	enum class Result
	{
		None,    ///< static, or resting and not touched
		Moved,   ///< 1
		Stopped, ///< 2: came to rest, EndPhysics
		Pushed,  ///< 3: a resting body was knocked and must become a physics object
		Delete,  ///< 4: below -4 R
	};

	struct Vertex
	{
		float pen0 {0.0f};          ///< penetration at the first substep of the current contact (0 = none)
		glm::vec3 anchor {0.0f};    ///< friction anchor (world)
		glm::vec3 local {0.0f};     ///< (mesh vertex - com) x scale
		float len {0.0f};           ///< |local|
		float predLen {0.0f};       ///< |R local + v 0.06|, at least 0.001
		float pen {0.0f};           ///< penetration this substep (> 0 inside)
		glm::vec3 contact {0.0f};   ///< contact point on the surface
		glm::vec3 world {0.0f};     ///< predicted position R local + T + v 0.06
		glm::vec3 normal {0.0f};    ///< contact normal, then the contact force
		PhysOb* other {nullptr};    ///< contact partner, null for the landscape
	};

	struct Face
	{
		std::array<uint32_t, 3> indices {};
		glm::vec3 localNormal {0.0f};
		glm::vec3 worldNormal {0.0f};
	};

	/// Initialise (0x7FB780): restCounter = -(scale x mesh half height x 1000). meshHeight is the half height.
	void Initialise(float scale, float meshHeight);
	/// SetUpConstants (0x7FB810).
	void SetUpConstants(float mass, const PhysicsData& data, bool dynamic);
	/// BuildFromVertices (0x7FBAE0) + SetUpMoi + SetUpPos: raw mesh-space positions and triangles, and the object's
	/// rotation (unit columns) and origin.
	void Build(std::span<const glm::vec3> positions, std::span<const std::array<uint32_t, 3>> triangles,
	           const glm::mat3& rotation, glm::vec3 origin);
	/// A hand-built body (SetUpPhysObAsATree 0x63A230, Villager/Animal::SetUpPhysOb): vertices already scaled and about
	/// the centre of mass, the centre of mass in mesh units, the radius, and the drag factor applied after SetUpMoi.
	void BuildShape(std::span<const glm::vec3> local, std::span<const std::array<uint32_t, 3>> triangles, glm::vec3 com,
	                float radius, float dragFactor, const glm::mat3& rotation, glm::vec3 origin, float inertiaFactor = 1.0f);
	/// SetUpPos (0x7FC760) from the object's rotation (unit columns) and origin.
	void SetUpPos(const glm::mat3& rotation, glm::vec3 origin);
	/// AdjustToGroundLevel (0x7FCB80): optionally align to the slope, then lower (or raise) the body until its lowest
	/// vertex touches the landscape; with noPullDown it is only raised.
	void AdjustToGroundLevel(bool noPullDown, bool alignToNormal);

	// One substep, in GameTurnUpdate's order
	void ZeroForces();             // 0x7FD200
	void GroundAndWater();         // 0x7FD4D0
	void CollideVertices(PhysOb& b); // 0x7FDA60
	void ContactForces();          // 0x7FDE40
	Result Integrate();            // 0x7FE260

	/// fn_7FD140: the object's origin from the body.
	[[nodiscard]] glm::vec3 ObjectOrigin() const;
	/// The same origin for another pose of the body (fn_007FCE80 0x7FD097..0x7FD117: T - R s com)
	[[nodiscard]] glm::vec3 ObjectOrigin(const glm::mat3& rotation, glm::vec3 centre) const;
	[[nodiscard]] const glm::mat3& Rotation() const { return _rotation; }
	[[nodiscard]] glm::vec3 Centre() const { return _centre; }
	[[nodiscard]] float Radius() const { return _radius; }
	[[nodiscard]] float Mass() const { return _mass; }
	[[nodiscard]] const std::vector<Vertex>& Vertices() const { return _vertices; }

	glm::vec3 velocity {0.0f};
	/// Angular momentum (world).
	glm::vec3 angularMomentum {0.0f};
	glm::vec3 force {0.0f};  ///< this substep's force accumulator (PhysOb+0x104)
	glm::vec3 torque {0.0f}; ///< this substep's torque accumulator
	glm::vec3 externalForce {0.0f};
	glm::vec3 externalTorque {0.0f};
	float density {0.8f};
	PhysOb* lastHit {nullptr};
	int restCounter {0};
	int numContacts {0};
	bool resting {false};
	bool justSetUp {false};
	bool touched {false};
	bool inWater {false};

	/// Sets the angular velocity (world) through the inertia tensor.
	void SetAngularVelocity(glm::vec3 omega);

	/// fn_007FDD60 (PhysicsObject::RaiseUntilNotIntersecting 0x644B79 / 0x644B8A): the largest dot(q - hit, direction)
	/// over this body's vertices q whose ray back along -direction meets a face of `other` (fn_007FC310), 0 if none.
	/// With direction (0, -1, 0) it is how far this body must go up for its vertices to leave `other` through its top.
	[[nodiscard]] float PenetrationAlong(const PhysOb& other, glm::vec3 direction) const;

private:
	void SetUpMoi();
	[[nodiscard]] glm::vec3 BodyOmega() const;
	bool RaySegmentVsFaces(glm::vec3 point, glm::vec3 direction, glm::vec3& hit, glm::vec3& normal) const;
	/// fn_007FC310: the face entry nearest to the point on the ray point + t direction, t < 0 (unbounded), among the
	/// faces facing against the direction (dot(n, direction) < -0.0001 with n = (v1 - v0) x (v2 - v0), not unit).
	bool RayBehindVsFaces(glm::vec3 point, glm::vec3 direction, glm::vec3& hit) const;

	float _scale {1.0f};
	glm::mat3 _rotation {1.0f}; ///< columns = body axes in the world
	glm::vec3 _centre {0.0f};   ///< world centre of mass (the original's M.T)
	glm::vec3 _predictedCentre {0.0f};
	glm::vec3 _com {0.0f};      ///< centre of mass in (unscaled) mesh space
	float _speed {0.0f};
	float _mass {1.0f};
	float _kContact {0.0f};
	float _kPenetration {0.0f};
	float _friction {1.0f};
	float _angularDampStep {1.0f};
	float _drag {0.0f};
	float _radius {0.0f};
	bool _dynamic {true};
	/// Inverse inertia tensor in the original's [row][column] indexing (the tensor keeps its I[1][2] = -xz bug).
	std::array<std::array<float, 3>, 3> _inverseInertia {};
	std::array<std::array<float, 3>, 3> _inertia {};
	std::vector<Vertex> _vertices;
	std::vector<Face> _faces;
};
} // namespace openblack::ecs::physics
