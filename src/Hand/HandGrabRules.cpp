/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandGrabRules.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/ObjectMatrix.h"
#include "Physics/Body.h"

using namespace openblack;
using namespace openblack::hand_grab;

namespace
{
/// A vector's length with its squares summed x, z, then y, as the tug sums them
float LengthAcrossThenUp(glm::vec3 v)
{
	return std::sqrt(v.x * v.x + v.z * v.z + v.y * v.y);
}
} // namespace

PlaceInHandRule hand_grab::PlaceInHandRuleOf(const PlaceInHandClasses& thing)
{
	// the most particular class first: villagers, animals and scaffolds are loose things too, and the rest of the
	// mobiles go in last
	if (thing.creature)
	{
		return PlaceInHandRule::Never;
	}
	if (thing.villager)
	{
		return PlaceInHandRule::Villager;
	}
	if (thing.animal)
	{
		return PlaceInHandRule::Animal;
	}
	if (thing.scaffold)
	{
		return PlaceInHandRule::Scaffold;
	}
	if (thing.seedOrTeleport)
	{
		return PlaceInHandRule::Seed;
	}
	if (thing.fireBall)
	{
		return PlaceInHandRule::FireBall;
	}
	if (thing.field)
	{
		return PlaceInHandRule::Field;
	}
	if (thing.fishFarm)
	{
		return PlaceInHandRule::FishFarm;
	}
	if (thing.pot)
	{
		return thing.pile ? PlaceInHandRule::Pile : PlaceInHandRule::Always;
	}
	// the bonfire and a building's fragment say no of their own, ahead of the rock's and the mobile static's rules
	if (thing.bonfire || thing.fragment)
	{
		return PlaceInHandRule::Never;
	}
	if (thing.mobileStatic && thing.rock)
	{
		return PlaceInHandRule::Rock;
	}
	if (thing.deadTree || thing.mobileStatic || thing.tree || thing.bigForest || thing.oneOffSpellSeed || thing.mobile)
	{
		return PlaceInHandRule::Always;
	}
	return PlaceInHandRule::Never;
}

bool hand_grab::FieldValidForPlaceInHand(float growth, float food)
{
	return growth > 0.0f && food > 0.0f;
}

bool hand_grab::PileValidForPlaceInHand(PotType kind, PotInfo row, uint32_t resource)
{
	const bool handRow =
	    (kind == PotType::PileFood && row == PotInfo::HandFood) || (kind == PotType::PileWood && row == PotInfo::HandWood);
	return handRow && resource != 0;
}

bool hand_grab::PileValidForLockedSelect(PotType kind, PotInfo row)
{
	return (kind == PotType::PileFood && row != PotInfo::HandFood) || (kind == PotType::PileWood && row != PotInfo::HandWood);
}

uint32_t hand_grab::ElapsedMs(uint32_t nowMs, uint32_t pressMs, uint32_t turn, uint32_t pressTurn)
{
	const uint32_t byClock = nowMs - pressMs;
	const uint32_t byTurns = (turn + 1) * k_TurnMs - pressTurn * k_TurnMs;
	return std::min(byClock, byTurns);
}

float hand_grab::HoldDistance(float loweringMultiplier, float height, float handSize)
{
	// the hand's height times its size, then the share: the original's order of the products
	return std::max(loweringMultiplier * height, k_StandardHandHeight * handSize * k_LeastLoweringShare);
}

float hand_grab::CursorRaise(float handRise)
{
	return k_CursorRaiseShare * handRise;
}

void HandSpring::Start(glm::vec3 position)
{
	_position = position;
	_velocity = glm::vec3(0.0f);
}

void HandSpring::Count(uint32_t frameMs)
{
	_elapsedMs += frameMs;
}

void HandSpring::Step(glm::vec3 target)
{
	constexpr float k_Step = static_cast<float>(k_SpringStepMs) * 0.001f;
	// At least one step a frame, then as many as the game's time has run
	_steps = 0;
	do
	{
		_steppedMs += k_SpringStepMs;
		++_steps;
		_velocity += (k_SpringStiffness * (target - _position) - k_SpringDamping * _velocity) * k_Step;
		if (const float speed = glm::length(_velocity); speed > k_MaxThrowSpeed)
		{
			_velocity *= k_MaxThrowSpeed / speed;
		}
		_position += _velocity * k_Step;
	} while (_steppedMs < _elapsedMs);
}

Countdown hand_grab::CountDown(int32_t& msLeft, uint32_t frameMs)
{
	if (msLeft <= 0)
	{
		return Countdown::Cancelled;
	}
	msLeft -= static_cast<int32_t>(frameMs);
	if (msLeft < 0)
	{
		return Countdown::RunOut;
	}
	return msLeft == 0 ? Countdown::Cancelled : Countdown::Running;
}

float hand_grab::MaxForce(float strength)
{
	return (strength + 1.0f) * k_MaxForcePerStrength;
}

Tug hand_grab::TugOf(const glm::mat3& rotation, glm::vec3 scale, glm::vec3 base)
{
	Tug tug {.axes = rotation, .base = base};
	for (int axis = 0; axis < 3; ++axis)
	{
		tug.axes[axis] *= scale[axis];
	}
	return tug;
}

glm::mat3 hand_grab::TugRotation(const Tug& tug)
{
	auto rotation = tug.axes;
	affine::NormaliseRows(rotation);
	return rotation;
}

glm::vec3 hand_grab::TugGrip(const Tug& tug, float holdDistance)
{
	return tug.base + TugRotation(tug)[1] * holdDistance;
}

TugResult hand_grab::PullAt(Tug& tug, const TugFrame& frame)
{
	TugResult result;
	const auto grip = TugGrip(tug, frame.holdDistance);
	// The pull is a spring from where the hand grips it to the hand, no stronger than the hand
	const auto stretch = frame.hand - grip;
	const float distance = LengthAcrossThenUp(stretch);
	result.force = k_TugStiffness * distance >= frame.maxForce && distance > 0.0f ? stretch * (frame.maxForce / distance)
	                                                                              : stretch * k_TugStiffness;
	// It stretches towards the hand, by how far the hand is from its grip against how far up it is gripped
	if (frame.holdDistance > 0.0f)
	{
		result.stretchTarget = std::min((distance + frame.holdDistance) / frame.holdDistance, k_TugMaxStretch);
	}
	// A tree comes free once the hand is strong enough to lift it and pulls harder than it weighs; anything else at once
	result.comesFree =
	    !frame.tree || (frame.weight * physics::k_Gravity <= frame.maxForce && frame.weight < LengthAcrossThenUp(result.force));

	// The pull turns it about its base, against its heaviness (times its inverse, as the original) and a damping that
	// grows with its spin, its squares summed z, x, then y
	const glm::vec3 torque =
	    frame.leans ? glm::cross(grip - tug.base, result.force) * (1.0f / k_TugTurnInertia) : glm::vec3(0.0f);
	const auto& w = tug.spin;
	const float spinLength = std::sqrt(w.z * w.z + w.x * w.x + w.y * w.y);
	const float damping = spinLength * k_TugTurnDamping * frame.seconds / k_TugTurnInertia;
	tug.spin += torque * frame.seconds - tug.spin * damping;
	const float halfHeight = frame.height * 0.5f;
	const auto halfBefore = tug.base + tug.axes[1] * halfHeight;
	const auto turn = tug.spin * frame.seconds;
	if (turn != glm::vec3(0.0f))
	{
		// The angle is the turn's length and the axis the turn times one over it. The game's axis-angle turn about the
		// opposite of that axis, by the same angle, applied to each axis as a row (the sums from the last term), turns
		// the way glm turns about the axis itself
		const float angle = std::sqrt(turn.x * turn.x + turn.y * turn.y + turn.z * turn.z);
		const float inverse = 1.0f / angle;
		const auto r = affine::AxisAngle(-(turn * inverse), angle);
		const auto before = tug.axes;
		for (int axis = 0; axis < 3; ++axis)
		{
			const auto& m = before[axis];
			for (int k = 0; k < 3; ++k)
			{
				tug.axes[axis][k] = (r[2][k] * m[2] + r[1][k] * m[1]) + r[0][k] * m[0];
			}
		}
	}
	// How fast its middle moves as it leans, the speed it has when it comes free
	const auto halfAfter = tug.base + tug.axes[1] * halfHeight;
	tug.pullVelocity =
	    frame.leans && frame.seconds > 0.0f ? (halfAfter - halfBefore) * (1.0f / frame.seconds) : glm::vec3(0.0f);
	return result;
}

glm::vec3 hand_grab::TugHandPoint(const Tug& tug, float holdDistance, float stretch)
{
	return tug.base + (TugRotation(tug)[1] * stretch) * holdDistance;
}

bool hand_grab::StepTugFade(TugFade& fade, float frameSeconds)
{
	const bool pulls = !fade.waiting;
	if (fade.waiting)
	{
		fade.waiting = !(fade.seconds >= k_PullBlendSeconds);
	}
	if (fade.seconds < k_PullBlendSeconds)
	{
		fade.seconds += frameSeconds;
	}
	return pulls;
}

float hand_grab::GroundRange(glm::vec3 from, glm::vec3 to)
{
	const float dx = to.x - from.x;
	const float dz = to.z - from.z;
	return std::sqrt(dz * dz + dx * dx);
}

TugPlane hand_grab::TugPlaneAt(glm::vec3 eye, glm::vec3 hand, glm::vec3 base, glm::vec3 landNormal)
{
	// The line from the eye through the hand carried on to the base's distance from the eye, a step at a time as the
	// original rounds them
	const float handRange = GroundRange(eye, hand);
	const float baseRange = GroundRange(eye, base);
	const float drop = eye.y - hand.y;
	const float share = baseRange / handRange;
	const float height = eye.y - drop * share;
	return {.point = {base.x, height, base.z}, .normal = landNormal};
}

glm::vec3 hand_grab::TugHandOnPlane(const TugPlane& plane, glm::vec3 eye, glm::vec3 direction)
{
	const auto& n = plane.normal;
	// Both dot products are summed z, then y, then x, as the original sums them
	const float across = (plane.point.z - eye.z) * n.z + (plane.point.y - eye.y) * n.y + (plane.point.x - eye.x) * n.x;
	const float along = direction.z * n.z + direction.y * n.y + direction.x * n.x;
	const float t = across / along;
	return {direction.x * t + eye.x, direction.y * t + eye.y, direction.z * t + eye.z};
}

bool hand_grab::IsThrow(glm::vec3 velocity, bool byCreature)
{
	const float across = velocity.x * velocity.x + velocity.z * velocity.z;
	return across > (byCreature ? k_CreatureThrowSpeedSquared : k_HandThrowSpeedSquared);
}

bool hand_grab::PotPours(glm::vec3 velocity)
{
	// not faster: a speed that is not a number pours too, as the original's unordered compare does
	return !(glm::dot(velocity, velocity) > k_PourSpeedSquared);
}

bool hand_grab::PotDissolvesOnRelease(bool isPot, glm::vec3 velocity)
{
	return isPot && PotPours(velocity);
}

LandedOutcome hand_grab::OutcomeOfLanding(const LandedThing& thing)
{
	if (!thing.living && !thing.fence && !(thing.tree && !thing.burning && thing.onLand))
	{
		return LandedOutcome::Settles;
	}
	// A tree a god's hand held tilted, or mustn't plant again, falls rather than being planted
	if (thing.tree && !thing.byCreature &&
	    (thing.dontReplant || std::abs(thing.tiltX) > k_UprightTilt || std::abs(thing.tiltZ) > k_UprightTilt))
	{
		return LandedOutcome::Falls;
	}
	return LandedOutcome::LeavesPhysics;
}

GripStartEffect hand_grab::GripStartEffectAt(bool land, bool wideScreen, bool paused, bool drawsHeld)
{
	if (land)
	{
		return wideScreen ? GripStartEffect::None : GripStartEffect::Dust;
	}
	return paused || drawsHeld ? GripStartEffect::None : GripStartEffect::Splash;
}
