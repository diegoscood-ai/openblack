/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Physics/PhysicsData.h"

namespace openblack::physics
{
class Ground;

/// One step of the physics; twenty of them make a game turn of 0.1 s
inline constexpr float k_StepSeconds = 0.005f;
inline constexpr int k_StepsPerTurn = 20;
inline constexpr float k_Gravity = 9.81f;
/// How far ahead along its motion a body tests its contacts
inline constexpr float k_LookAheadSeconds = 0.06f;
/// No body moves faster than this, nor does the hand throw faster
inline constexpr float k_MaxSpeed = 124.0f;
/// No body spins faster than 3 pi radians a second
inline constexpr float k_MaxSpin = 9.42478f;

/// The share of a body's spin kept at each step: the share kept after a second to the power of the step's length, as
/// the game's C runtime works the power out on the FPU, stored as a float
[[nodiscard]] float SpinKeptPerStep(float angularKeep);

/// What a step did to a body
enum class StepResult
{
	None,    ///< static, or resting and not touched
	Moved,   ///< 1
	Stopped, ///< 2: came to rest, EndPhysics
	Knocked, ///< 3: a resting body was knocked and must become a physics object
	Delete,  ///< 4: below -4 R
};

/// The shape of a body before it is placed in the world
struct Shape
{
	/// The points, about the centre of mass, already scaled
	std::vector<glm::vec3> points;
	/// Triangles of points, the faces other bodies' points hit
	std::vector<std::array<uint32_t, 3>> faces;
	/// The centre of mass in the model's own units, unscaled
	glm::vec3 centreOfMass {0.0f};
	/// The farthest point from the centre of mass, scaled
	float radius {0.0f};
	/// Multiplies the air drag once the body is set up
	float dragFactor {1.0f};
};

/// The rigid body: the vertex cloud of a mesh as point masses, penalty contacts
/// (a spring and a Coulomb friction anchor per vertex, tested 0.06 s ahead) against the landscape, the sea and the
/// triangles of other bodies, integrated with semi-implicit Euler in 0.005 s substeps.
///
/// The original accumulates torques as F x r and rotates the row axes by -|w| dt; both signs cancel, so this port uses
/// r x F and rotates the column axes by +|w| dt, which gives the same motion.
class Body
{
public:
	struct Point
	{
		float firstDepth {0.0f};  ///< penetration at the first substep of the current contact (0 = none)
		glm::vec3 anchor {0.0f};  ///< friction anchor (world)
		glm::vec3 local {0.0f};   ///< (mesh vertex - com) x scale
		float length {0.0f};      ///< |local|
		float aheadLength {0.0f}; ///< |R local + v 0.06|, at least 0.001
		float depth {0.0f};       ///< penetration this substep (> 0 inside)
		glm::vec3 contact {0.0f}; ///< contact point on the surface
		glm::vec3 world {0.0f};   ///< predicted position R local + T + v 0.06
		glm::vec3 normal {0.0f};  ///< contact normal, then the contact force
		Body* other {nullptr};    ///< contact partner, null for the landscape
	};

	struct Face
	{
		std::array<uint32_t, 3> indices {};
		glm::vec3 localNormal {0.0f};
		glm::vec3 worldNormal {0.0f};
	};

	/// The rest counter = -(scale x mesh half height x 1000). meshHeight is the half height.
	void Initialise(float scale, float meshHeight);
	void SetUpConstants(float mass, const PhysicsData& data, bool dynamic);
	/// The body from a model's shape (shapes::FromModel), then SetUpInertia + SetUpPos, with the object's rotation (unit
	/// columns) and origin; the shape's drag factor is not applied.
	void Build(const Shape& shape, const glm::mat3& rotation, glm::vec3 origin);
	/// A hand-built body (trees, villagers and animals): vertices already scaled and about
	/// the centre of mass, the centre of mass in mesh units, the radius, and the drag factor applied after
	/// SetUpInertia.
	void BuildShape(std::span<const glm::vec3> local, std::span<const std::array<uint32_t, 3>> triangles, glm::vec3 com,
	                float radius, float dragFactor, const glm::mat3& rotation, glm::vec3 origin, float inertiaFactor = 1.0f,
	                bool rowsScaled = false);
	/// The pose from the object's rotation (unit columns, times the body's scale here) and origin; with
	/// rowsScaled the rows already are the object's world matrix with its scale and are
	/// used as they are, as the body's rows.
	void SetUpPos(const glm::mat3& rotation, glm::vec3 origin, bool rowsScaled = false);
	/// Optionally align to the slope, then lower (or raise) the body until its lowest
	/// vertex touches the landscape; with noPullDown it is only raised.
	void SettleOnLand(const Ground& ground, bool noPullDown, bool alignToSlope);

	// One substep, in ProcessTurn's order
	void ClearForces();
	void TouchGround(const Ground& ground);
	void TouchFaces(Body& b);
	void ApplyContacts();
	StepResult Integrate();

	/// The object's origin from the body, T - (s R) com in the original's order:
	/// each row times the scale first, then t0 = ((r2.x cz) + (r1.x cy)) + (cx r0.x), t1 = ((r2.y cz) +
	/// (r0.y cx)) + (r1.y cy), t2 = ((r2.z cz) + (r0.z cx)) + (r1.z cy), float products and sums.
	[[nodiscard]] glm::vec3 ObjectOrigin() const;
	/// The same origin for another pose of the body (T - R s com)
	[[nodiscard]] glm::vec3 ObjectOrigin(const glm::mat3& rotation, glm::vec3 centre) const;
	[[nodiscard]] const glm::mat3& Axes() const { return _axes; }
	[[nodiscard]] glm::vec3 Centre() const { return _centre; }
	[[nodiscard]] float Radius() const { return _radius; }
	[[nodiscard]] float Mass() const { return _mass; }
	[[nodiscard]] const std::vector<Point>& Points() const { return _points; }
	[[nodiscard]] int Contacts() const { return _contacts; }
	[[nodiscard]] int RestCounter() const { return _restCounter; }
	/// The force and turning force summed this substep
	[[nodiscard]] glm::vec3 Force() const { return _force; }
	[[nodiscard]] glm::vec3 Torque() const { return _torque; }

	glm::vec3 velocity {0.0f};
	/// Angular momentum (world).
	glm::vec3 angularMomentum {0.0f};
	glm::vec3 externalForce {0.0f};
	glm::vec3 externalTorque {0.0f};
	float density {0.8f};
	Body* lastHit {nullptr};
	bool resting {false};
	bool justSetUp {false};
	bool touched {false};
	bool inWater {false};

	/// No more drag, in the air or in the water (a script's aimed throw, SET_TARGET)
	void ClearDrag() { _drag = 0.0f; }

	/// Sets the angular velocity (world) through the inertia tensor.
	void SetAngularVelocity(glm::vec3 omega);
	/// As AddObject sets it: w is in the body's axes and the original's sign; t_x = ((M3 wy +
	/// M6 wz) + M0 wx) + 0, t_y = ((M1 wx + M4 wy) + M7 wz) + 0, t_z = ((M2 wx + M5 wy) + M8 wz) + 0 (M = the inertia
	/// tensor, its translation zeroed by SetUpInertia), L_i = ((t_z r2_i + t_y r1_i) + t_x r0_i) with the body's
	/// rows, stored negated (this port's sign).
	void SetAngularVelocityFromBody(glm::vec3 w);
	/// At the hand's release: the angular momentum = h M (the inertia tensor, no body rotation; its
	/// translation is zeroed by SetUpInertia, (inferred) so not added), in the original's order of float products and
	/// sums: x = (I00 hx + I20 hz) + I10 hy, y = (I21 hz + I01 hx) + I11 hy, z = (I22 hz + I02 hx) + I12 hy. The original's
	/// angular momentum is the opposite of this port's (its torque is F x r, see the class comment), so the result is
	/// stored negated (an exact sign change).
	void SetAngularMomentumFromHand(glm::vec3 h);
	/// The value SetAngularMomentumFromHand stores, for a tensor `inertia` ([row][col] as the original's):
	/// -(((I00 hx + I20 hz) + I10 hy) + 0, ((I21 hz + I01 hx) + I11 hy) + 0, ((I22 hz + I02 hx) + I12 hy) + 0); the + 0
	/// is the added zeroed translation, which makes a -0 a +0
	[[nodiscard]] static glm::vec3 HandAngularMomentum(const std::array<std::array<float, 3>, 3>& inertia, glm::vec3 h);
	/// The prediction's drawn origin: the normalised rows (glm's columns) times the
	/// scale, t_i = ((cz m(6+i)) + (cy m(3+i))) + (cx m(i)) with the body's com (cy in
	/// the middle for every i, unlike ObjectOrigin), origin = centre - t
	[[nodiscard]] glm::vec3 DrawOrigin(const glm::mat3& rows, glm::vec3 centre) const;
	[[nodiscard]] static glm::vec3 DrawOrigin(const glm::mat3& rows, glm::vec3 centre, float scale, glm::vec3 com);
	/// For RaiseClearOfWhatIsUnder: the largest dot(q - hit, direction)
	/// over this body's vertices q whose ray back along -direction meets a face of `other` (RayBehindVsFaces), 0 if none.
	/// With direction (0, -1, 0) it is how far this body must go up for its vertices to leave `other` through its top.
	[[nodiscard]] float OverlapDepth(const Body& other, glm::vec3 direction) const;
	/// ProcessTurn's end of turn for a resting proxy: the centre goes to the object's position plus
	/// its centre of mass through the current rows, scaled
	void FollowObject(glm::vec3 objectPosition);

private:
	void SetUpInertia();
	[[nodiscard]] glm::vec3 BodyOmega() const;
	bool RaySegmentVsFaces(glm::vec3 point, glm::vec3 direction, glm::vec3& hit, glm::vec3& normal) const;
	/// The face entry nearest to the point on the ray point + t direction, t < 0 (unbounded), among the
	/// faces facing against the direction (dot(n, direction) < -0.0001 with n = (v1 - v0) x (v2 - v0), not unit).
	bool RayBehindVsFaces(glm::vec3 point, glm::vec3 direction, glm::vec3& hit) const;

	float _scale {1.0f};
	glm::mat3 _axes {1.0f};   ///< columns = body axes in the world
	glm::vec3 _centre {0.0f}; ///< world centre of mass (the translation of the body's matrix)
	glm::vec3 _aheadCentre {0.0f};
	glm::vec3 _centreOfMass {0.0f}; ///< centre of mass in (unscaled) mesh space
	float _speed {0.0f};
	float _mass {1.0f};
	float _stiffness {0.0f};
	float _damping {0.0f};
	float _friction {1.0f};
	float _spinKeptPerStep {1.0f};
	float _drag {0.0f};
	float _radius {0.0f};
	bool _dynamic {true};
	int _contacts {0};
	int _restCounter {0};
	glm::vec3 _force {0.0f};  ///< this substep's force accumulator
	glm::vec3 _torque {0.0f}; ///< this substep's torque accumulator
	/// Inverse inertia tensor in the original's [row][column] indexing (the tensor keeps its I[1][2] = -xz bug).
	std::array<std::array<float, 3>, 3> _inverseInertia {};
	std::array<std::array<float, 3>, 3> _inertia {};
	std::vector<Point> _points;
	std::vector<Face> _faces;
};
} // namespace openblack::physics
