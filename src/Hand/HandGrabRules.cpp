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
/// Held from the side, a gate totem and the weeping stones hang this share of their height below the hand
constexpr float k_HeavyStaticLowering = 0.7f;
/// ...and the singing stone this share
constexpr float k_SingingStoneLowering = 0.4f;
/// Loose things hang this share of their height below the hand, held from the side
constexpr float k_SideLowering = 0.7f;
/// The one loose thing held on the palm sits this share of its height above it
constexpr float k_PalmMobileLowering = -0.3f;
/// People and animals hang this share of their height below the hand
constexpr float k_LivingLowering = 0.65f;
/// Trees hang this share of their height below the hand
constexpr float k_TreeLowering = 0.1f;
/// A tree spreads out of the hand this share of its radius
constexpr float k_TreeHoldRadiusShare = 0.2f;
/// Held on the palm, a thing spreads out of the hand this share of its height
constexpr float k_PalmHoldRadiusShare = 0.75f;

/// A vector's length with its squares summed x, z, then y, as the tug sums them
float LengthAcrossThenUp(glm::vec3 v)
{
	return std::sqrt(v.x * v.x + v.z * v.z + v.y * v.y);
}

bool GateTotem(MobileStaticInfo type)
{
	return type >= MobileStaticInfo::GateTotemApe && type <= MobileStaticInfo::GateTotemTiger;
}

/// Any thing's own reach out of the hand: its height on the palm, its radius otherwise
float ObjectHoldRadius(HoldType hold, float height, float radius)
{
	return hold == HoldType::Above ? height * k_PalmHoldRadiusShare : radius;
}
} // namespace

bool hand_grab::ValidForPlaceInHand(const Holdable& thing)
{
	switch (thing.kind)
	{
	case GrabKind::Villager:
		// Not one at home, in a hand already, or hiding in a building
		return thing.available && !thing.atHome && !thing.inHand && !thing.hiding;
	case GrabKind::Animal:
		return thing.speciesAllows;
	case GrabKind::Rock:
	case GrabKind::DeadTree:
		return !(thing.isRock && thing.radius > k_RockMaxPickUpRadius);
	case GrabKind::MobileStatic:
	case GrabKind::MobileObject:
	case GrabKind::Poo:
	case GrabKind::Tree:
		return true;
	case GrabKind::None:
		break;
	}
	return false;
}

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

bool hand_grab::PassesGate(const Gate& gate)
{
	return gate.spaceInHand && !gate.alreadyInHand && gate.valid && !gate.cannotBePickedUp && !gate.carried && gate.inInfluence;
}

std::string_view hand_grab::GateRefusal(const Gate& gate)
{
	if (!gate.inInfluence)
	{
		return "the hand is outside its player's influence";
	}
	if (!gate.spaceInHand)
	{
		return "the hand holds something already";
	}
	if (gate.alreadyInHand)
	{
		return "it is in a hand already";
	}
	if (gate.cannotBePickedUp)
	{
		return "a script says it can't be picked up";
	}
	if (gate.carried)
	{
		return "something else carries it";
	}
	if (!gate.valid)
	{
		return "the hand can't hold that kind of thing";
	}
	return {};
}

uint32_t hand_grab::ElapsedMs(uint32_t nowMs, uint32_t pressMs, uint32_t turn, uint32_t pressTurn)
{
	const uint32_t byClock = nowMs - pressMs;
	const uint32_t byTurns = (turn + 1) * k_TurnMs - pressTurn * k_TurnMs;
	return std::min(byClock, byTurns);
}

HoldFacts hand_grab::HoldOf(GrabKind kind, MobileStaticInfo staticType, MeshId mesh, float height, float radius)
{
	HoldFacts facts;
	switch (kind)
	{
	case GrabKind::Villager:
	case GrabKind::Animal:
		facts.type = HoldType::Villager;
		facts.loweringMultiplier = k_LivingLowering;
		facts.holdRadius = ObjectHoldRadius(facts.type, height, radius);
		break;
	case GrabKind::Tree:
	case GrabKind::DeadTree:
		facts.type = HoldType::Tree;
		facts.loweringMultiplier = k_TreeLowering;
		facts.holdRadius = radius * k_TreeHoldRadiusShare;
		break;
	case GrabKind::MobileObject:
		facts.type = HoldType::Side;
		facts.loweringMultiplier = k_SideLowering;
		facts.holdRadius = ObjectHoldRadius(facts.type, height, radius);
		break;
	case GrabKind::Poo:
		facts.type = HoldType::Above;
		facts.loweringMultiplier = k_PalmMobileLowering;
		facts.holdRadius = ObjectHoldRadius(facts.type, height, radius);
		break;
	case GrabKind::Rock:
	case GrabKind::MobileStatic:
		if (GateTotem(staticType) || staticType == MobileStaticInfo::WeepingStone ||
		    staticType == MobileStaticInfo::WeepingStoneReward)
		{
			facts.type = HoldType::Side;
			facts.loweringMultiplier = k_HeavyStaticLowering;
		}
		else if (staticType == MobileStaticInfo::SingingStone_1)
		{
			facts.type = HoldType::Side;
			facts.loweringMultiplier = k_SingingStoneLowering;
		}
		else if (mesh == MeshId::ObjectToyCuddly || mesh == MeshId::ObjectToySkittle)
		{
			facts.type = HoldType::Side;
		}
		facts.holdRadius = ObjectHoldRadius(facts.type, height, radius);
		break;
	case GrabKind::None:
		facts.holdRadius = ObjectHoldRadius(facts.type, height, radius);
		break;
	}
	return facts;
}

float hand_grab::HoldDistance(float loweringMultiplier, float height, float handSize)
{
	// the hand's height times its size, then the share: the original's order of the products
	return std::max(loweringMultiplier * height, k_StandardHandHeight * handSize * k_LeastLoweringShare);
}

float hand_grab::HandRise(HoldType hold, float lowering, float handSize, std::optional<float> rootedHeight)
{
	float rise = 0.0f;
	switch (hold)
	{
	case HoldType::Above:
		rise = k_AboveHang;
		break;
	case HoldType::Magic:
		rise = k_StandardHandHeight * handSize;
		break;
	case HoldType::Grain:
	case HoldType::Tree:
	case HoldType::Side:
	case HoldType::Villager:
		rise = std::max(lowering, k_MinimumHang);
		break;
	case HoldType::None:
	case HoldType::Fingers:
		break;
	}
	if (rootedHeight.has_value())
	{
		rise += k_RootedHangShare * *rootedHeight;
	}
	return rise;
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

bool hand_grab::IsFastRelease(glm::vec3 velocity)
{
	return glm::dot(velocity, velocity) > k_FastReleaseSpeedSquared;
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

glm::vec3 hand_grab::ReleaseSpinTorque(float mass, float speed, glm::vec3 handMoved)
{
	// A twist about the level axis across the hand's motion since it let go, its turning sense as the bodies count it
	const float strength = k_ReleaseSpinFactor * mass * speed;
	return {strength * handMoved.z, 0.0f, -strength * handMoved.x};
}

bool hand_grab::LandsOnRelease(const Landing& landing)
{
	if (landing.thrown || (landing.raised && !landing.computerVillager))
	{
		return false;
	}
	const bool ground =
	    landing.dryLand || (landing.nearestAltitude.has_value() && *landing.nearestAltitude > k_LowestLandingAltitude);
	if (!ground)
	{
		return false;
	}
	return !landing.needsGentleSlope || landing.normalY >= k_LeastLandingNormalY;
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

namespace
{
/// The game turns a scoop takes to ramp up: its seconds at ten turns a second, whole
int32_t ScoopRampTurns(const ScoopFacts& facts)
{
	constexpr float k_TurnsPerSecond = 1000.0f / static_cast<float>(k_TurnMs);
	return static_cast<int32_t>(k_TurnsPerSecond * facts.rampSeconds);
}
} // namespace

float hand_grab::ScoopRamp(uint32_t turns, const ScoopFacts& facts)
{
	const auto rampTurns = ScoopRampTurns(facts);
	// A ramp of no turns is over at once, except at its very first turn, where the game's share comes out as nothing
	if (rampTurns <= 0)
	{
		return turns == 0 ? 0.0f : 1.0f;
	}
	const float share = std::clamp(static_cast<float>(turns) / static_cast<float>(rampTurns), 0.0f, 1.0f);
	return share * share;
}

uint32_t hand_grab::ScoopAmount(uint32_t turns, const ScoopFacts& facts)
{
	const float ramp = ScoopRamp(turns, facts);
	// The difference is taken as the game takes it, unsigned
	const auto rise = static_cast<double>(static_cast<uint32_t>(facts.perTurnEnd - facts.perTurn));
	return static_cast<uint32_t>(static_cast<double>(facts.perTurn) + rise * static_cast<double>(ramp));
}

uint32_t hand_grab::ScoopTaken(uint32_t wanted, uint32_t sourceHas, uint32_t held, const ScoopFacts& facts)
{
	uint32_t taken = std::min(wanted, sourceHas);
	if (facts.maxPickedUp != 0)
	{
		const uint32_t room = held < facts.maxPickedUp ? facts.maxPickedUp - held : 0;
		taken = std::min(taken, room);
	}
	return taken;
}

float hand_grab::ScoopTip(float height, float handLength)
{
	if (height >= k_ScoopTipLowHeight)
	{
		return k_ScoopTip;
	}
	return std::atan((k_ScoopTipLowHeight - height) / handLength) + k_ScoopTip;
}
