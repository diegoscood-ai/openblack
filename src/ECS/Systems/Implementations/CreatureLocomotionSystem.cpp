/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureLocomotionSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Audio/AnimEffectKeys.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "RoutePlanner/RouteFollower.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
namespace locomotion = openblack::creature_locomotion;
using Motion = CreatureLocomotion::Motion;
using FollowerState = openblack::route_planner::RouteFollower::State;

namespace
{
/// A game turn's milliseconds and seconds, as the game's tick time sets them
float TurnMs()
{
	return static_cast<float>(game_clock::MsPerTurn());
}
float TurnSeconds()
{
	return TurnMs() / 1000.0f;
}
/// The follower moves a tenth of its speed at each step along its route
constexpr float k_FollowerStepSeconds = 0.1f;
/// A new destination this close to the current one carries on the same walk
constexpr float k_SameDestination = 0.1f;
/// Fidgets while waiting for a route: the first comes after this long, a look about is followed by a longer wait, and
/// a puzzled face is held this long
constexpr float k_FidgetMs = 3000.0f;
constexpr float k_LookAboutFidgetMs = 6000.0f;
constexpr float k_PuzzledMs = 2000.0f;
constexpr size_t k_PuzzledFace = creature_layers::animations::k_FirstFace + 6;
/// Following something, the walk is pointed anew once it has moved this far from where the walk heads
constexpr float k_FollowRetarget = 1.0f;
/// Running away, a creature goes up to this much further than its species' distance, and looks this far round the
/// point for somewhere to stand
constexpr uint32_t k_RunAwayExtra = 40;
constexpr float k_RunAwaySearch = 30.0f;
/// Arriving where it ran away to, anywhere within this
constexpr float k_RunAwayArrival = 5.0f;
/// Looking for somewhere to stand after straying off where it can
constexpr float k_StraySearch = 1000.0f;
/// What is known of a species without the game's creature tables: the ape's speeds
constexpr float k_DefaultSlowFraction = 0.5f;
constexpr float k_DefaultWalkFraction = 0.7f;
constexpr float k_DefaultRunFraction = 0.9f;
constexpr float k_DefaultRunAwayDistance = 50.0f;
/// Turning or stepping, the steps never move it less than this a turn
constexpr float k_Tiny = 1e-4f;

struct Fractions
{
	float slow;
	float walk;
	float run;
	float runAway;
};

Fractions FractionsOf(CreatureType species)
{
	if (Locator::infoConstants::has_value())
	{
		const auto& creatures = Locator::infoConstants::value().creature;
		const auto row = creature::InfoRow(species);
		if (row < creatures.size())
		{
			const auto& info = creatures.at(row);
			return {.slow = info.slowSpeed, .walk = info.walkSpeed, .run = info.runSpeed, .runAway = info.runAwayDistance};
		}
	}
	return {.slow = k_DefaultSlowFraction,
	        .walk = k_DefaultWalkFraction,
	        .run = k_DefaultRunFraction,
	        .runAway = k_DefaultRunAwayDistance};
}

/// The species' animations, as its base mesh has them: the blends follow the base's lengths and moves
struct Moves
{
	const CreatureRig* rig {nullptr};

	[[nodiscard]] const skeletal_animation::Animation* Get(size_t index) const
	{
		if (rig == nullptr)
		{
			return nullptr;
		}
		const auto* animation = rig->GetAnimation(CreatureRig::Mesh::Base, index);
		return animation != nullptr && !animation->frames.empty() ? animation : nullptr;
	}
	[[nodiscard]] bool Has(size_t index) const { return Get(index) != nullptr; }
	[[nodiscard]] float Duration(size_t index) const
	{
		const auto* animation = Get(index);
		return animation != nullptr ? static_cast<float>(std::max(animation->duration, 1u)) : 1.0f;
	}
	/// An animation's own length, none when the species lacks it
	[[nodiscard]] std::optional<int32_t> Length(size_t index) const
	{
		const auto* animation = Get(index);
		return animation != nullptr ? std::optional(static_cast<int32_t>(animation->duration)) : std::nullopt;
	}
	[[nodiscard]] glm::vec2 Displacement(size_t index) const
	{
		const auto* animation = Get(index);
		return animation != nullptr ? glm::vec2(animation->displacement.x, animation->displacement.z) : glm::vec2(0.0f);
	}
	[[nodiscard]] locomotion::Cycle Cycle(size_t index) const
	{
		return {.durationMs = Duration(index), .stride = glm::length(Displacement(index))};
	}
	[[nodiscard]] bool CanWalk() const { return Has(locomotion::animations::k_Walk) && Has(locomotion::animations::k_Run); }
	[[nodiscard]] bool HasAll(size_t first) const { return Has(first) && Has(first + 1) && Has(first + 2); }
	/// A step's blend lasts its two animations' lengths blended, in whole milliseconds
	[[nodiscard]] float PairDuration(const locomotion::Pair& pair) const
	{
		return static_cast<float>(
		    locomotion::BlendedLength(Length(pair.from).value_or(0), Length(pair.to).value_or(0), pair.weight));
	}
	/// The whole move of a step's blend, up and down too, at the blended stride's length
	[[nodiscard]] glm::vec3 PairMove(const locomotion::Pair& pair) const
	{
		const auto step = [this](size_t index) {
			const auto* animation = Get(index);
			return animation != nullptr ? locomotion::StepMove {.move = animation->displacement, .stride = animation->stride}
			                            : locomotion::StepMove {.move = glm::vec3(0.0f), .stride = 0.0f};
		};
		return locomotion::BlendedMove(step(pair.from), step(pair.to), pair.weight);
	}
};

Moves MovesOf(CreatureType species)
{
	if (!Locator::resources::has_value())
	{
		return {};
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(species);
	return {.rig = rigs.Contains(id) ? &*rigs.Handle(id) : nullptr};
}

float HeightAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

/// Whether a creature can stand at a point: k_CreatureRadius for a destination, k_CreatureTurningRadius for where it
/// is as it walks
bool IsValid(glm::vec2 point, float radius)
{
	return land_avoid::IsPosValid(glm::vec3(point.x, 0.0f, point.y), radius);
}

/// The creature's size and speeds this turn. Its speeds and its stride follow the size it is drawn at (smaller in its
/// temple's pen); the radius it keeps clear by stays its own size's
void Measure(CreatureLocomotion& self, const ecs::Registry& registry, entt::entity entity, const Creature& creature,
             const Transform& transform)
{
	self.speeds = locomotion::SpeedsFor(ecs::creature_pose::GivenSize(registry, entity));
	self.scale = std::abs(ecs::creature_pose::DrawnScale(registry, entity).x);
	const auto ownScale = std::abs(transform.scale.x);
	if (!Locator::resources::has_value())
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = creature::GetIdFromType(creature.species, creature::CreatureBody::Appearance::Base);
	if (meshes.Contains(meshId))
	{
		// (approximate) its radius is the mean of its mesh's half width and half length; the game's own radius is not
		// read yet
		const auto size = meshes.Handle(meshId)->GetBoundingBox().Size();
		self.radius = ownScale * (size.x + size.z) * 0.25f;
	}
}

void SetIdle(CreatureLocomotion& self)
{
	self.motion = Motion::Standing;
	self.speed = 0.0f;
	self.move.reset();
	self.tracks.clear();
}

/// The follower is kept while it is idle; one that is planning or following a route is dropped. A walk from standing
/// starts with a fresh one either way
void DropRoute(CreatureLocomotion& self)
{
	if (self.follower != nullptr && self.follower->GetState() != FollowerState::Idle)
	{
		self.follower.reset();
	}
	self.routeReady = false;
	self.planEnd = -1;
}

void ClearWalk(CreatureLocomotion& self)
{
	SetIdle(self);
	DropRoute(self);
	self.braking = false;
	self.destination.reset();
	self.facingOnly = false;
	self.turnAfterAction.reset();
}

glm::vec2 PositionOf(const CreatureLocomotion& self)
{
	return glm::xz(self.toPosition);
}

void PlaceAt(CreatureLocomotion& self, glm::vec2 point)
{
	self.toPosition = glm::vec3(point.x, HeightAt(point), point.y);
}

/// Put back at a point outside a walk: its Transform goes there at once and it is drawn there with no slide
void SnapTo(entt::entity creature, CreatureLocomotion& self, glm::vec2 point)
{
	PlaceAt(self, point);
	self.fromPosition = self.toPosition;
	ecs::creature_pose::CommitTurnPose(creature, self.toPosition, self.heading);
	ecs::NotifyTeleported(creature);
}

/// The follower reports how its plan ended to the creature it walks
void RecordPlanEnd(int32_t context, int32_t code)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = static_cast<entt::entity>(context);
	if (registry.Valid(creature) && std::as_const(registry).AllOf<CreatureLocomotion>(creature))
	{
		registry.Get<CreatureLocomotion>(creature).planEnd = code;
	}
}

/// How far along its route the creature is bound to go before a re-planned route can turn off it: a hair while it
/// turns on the spot, what is left of its step's move while it steps off, and nothing while it walks or waits
float FollowerStopDistance(int32_t context)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0.0f;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto creature = static_cast<entt::entity>(context);
	const auto* self = registry.Valid(creature) ? registry.TryGet<const CreatureLocomotion>(creature) : nullptr;
	if (self == nullptr)
	{
		return 0.0f;
	}
	switch (self->motion)
	{
	case Motion::Turning:
		return locomotion::k_TurningStopDistance;
	case Motion::Stepping:
		if (!self->move.has_value() || self->follower == nullptr)
		{
			return 0.0f;
		}
		return locomotion::SteppingStopDistance(self->move->displacement, self->scale, self->follower->GetAlongNode());
	default:
		return 0.0f;
	}
}

route_planner::RouteFollower& FreshFollower(entt::entity creature, CreatureLocomotion& self)
{
	self.follower = std::make_unique<route_planner::RouteFollower>();
	self.follower->Init(static_cast<int32_t>(entt::to_integral(creature)), &RecordPlanEnd, {}, &FollowerStopDistance,
	                    self.searchTurns);
	self.follower->SetIdle();
	return *self.follower;
}

bool RouteFinished(const CreatureLocomotion& self)
{
	return self.follower == nullptr || !self.follower->HasRoute();
}

/// Where the follower is after moving some distance along the route, which way it went and whether the route ended
struct Advanced
{
	glm::vec2 position;
	/// The route's own heading where the follower now is: along its straight, or round its arc
	float heading;
	bool finished;
	/// Whether the step took it on to the next node of the route
	bool newNode;
};

Advanced AdvanceAlongRoute(CreatureLocomotion& self, float distance)
{
	const auto before = PositionOf(self);
	if (self.follower == nullptr)
	{
		return {.position = before, .heading = self.heading, .finished = true};
	}
	auto& follower = *self.follower;
	const auto node = follower.GetCurrentNode();
	follower.SetSpeed(distance / k_FollowerStepSeconds);
	follower.MoveAlongRoute();
	const glm::vec2 after {follower.GetPosition().x, follower.GetPosition().z};
	// the follower's heading turns the other way round from the creature's
	return {.position = after,
	        .heading = -follower.GetHeading(),
	        .finished = !follower.HasRoute(),
	        .newNode = follower.GetCurrentNode() != node};
}

/// The two animations of a turn or step, through the same fraction of each
void TrackPair(CreatureLocomotion& self, const Moves& moves, float fromMs, float advanceMs)
{
	if (!self.move.has_value() || !self.move->animations.has_value())
	{
		return;
	}
	const auto& pair = *self.move->animations;
	const auto duration = self.move->durationMs;
	const auto first = self.tracks.size();
	for (const auto& [animation, weight] : {std::pair {pair.from, 1.0f - pair.weight}, std::pair {pair.to, pair.weight}})
	{
		const auto own = moves.Duration(animation);
		const auto ratio = duration > 0.0f ? own / duration : 0.0f;
		self.tracks.push_back({.animation = animation,
		                       .fromMs = fromMs * ratio,
		                       .advanceMs = advanceMs * ratio,
		                       .durationMs = own,
		                       .weight = weight,
		                       .looping = false,
		                       .breathing = false});
	}
	// They are drawn as one animation, the two blended keyframe by keyframe, through the blend's own length
	self.tracks.at(first).pair = CreatureLocomotion::Track::Pair {
	    .to = pair.to, .weight = pair.weight, .fromMs = fromMs, .advanceMs = advanceMs, .durationMs = duration};
}

/// Starts turning on the spot to the target heading: the spins of the turn's side blended by its angle, or without
/// them over the stand's length
void StartTurn(CreatureLocomotion& self, const Moves& moves, float startRouteHeading)
{
	// The original's yaw is minus the heading
	const auto spins =
	    locomotion::SpinsFor(-self.heading, -self.targetHeading, [&moves](size_t index) { return moves.Length(index); });
	self.motion = Motion::Turning;
	self.move = CreatureLocomotion::Move {
	    .animations = spins.has_value()
	                      ? std::optional(locomotion::Pair {.from = spins->from, .to = spins->to, .weight = spins->weight})
	                      : std::nullopt,
	    .timeMs = 0.0f,
	    .durationMs =
	        spins.has_value() ? static_cast<float>(spins->durationMs) : moves.Duration(locomotion::animations::k_Stand),
	    .displacement = glm::vec3(0.0f),
	    .startHeading = self.heading,
	    .routeHeading = startRouteHeading};
}

/// Turns on the spot or steps off along the route, or walks straight on, by how far round it is: towards where the
/// route's heading at the follower leads over the node it is on (locomotion::RouteStartTarget)
void BeginLeg(CreatureLocomotion& self, const Moves& moves)
{
	if (RouteFinished(self))
	{
		self.motion = Motion::Walking;
		return;
	}
	const auto position = PositionOf(self);
	const auto target =
	    locomotion::RouteStartTarget(position, self.follower->GetHeading(), self.follower->GetCurrentNodeLength());
	const auto offset = target - position;
	self.targetHeading = locomotion::HeadingFromDirection(offset);
	const auto angle = affine::WrapAngle(self.targetHeading - self.heading);
	const auto side = locomotion::SideOf(angle);
	const auto firstStep =
	    side == locomotion::Side::Right ? locomotion::animations::k_RightStep : locomotion::animations::k_LeftStep;
	locomotion::StartOptions options {.stepStrides = std::nullopt, .moving = true};
	if (moves.HasAll(firstStep))
	{
		options.stepStrides =
		    std::array {glm::length(moves.Displacement(firstStep)), glm::length(moves.Displacement(firstStep + 1)),
		                glm::length(moves.Displacement(firstStep + 2))};
	}
	const auto distanceInMesh = self.scale > 0.0f ? glm::length(offset) / self.scale : 0.0f;
	const auto start = locomotion::ChooseStart(angle, distanceInMesh, options);
	self.speed = 0.0f;
	switch (start.kind)
	{
	case locomotion::Start::Kind::Walk:
		self.motion = Motion::Walking;
		self.move.reset();
		break;
	case locomotion::Start::Kind::Step:
		self.motion = Motion::Stepping;
		self.move = CreatureLocomotion::Move {.animations = start.animations,
		                                      .timeMs = 0.0f,
		                                      .durationMs = moves.PairDuration(*start.animations),
		                                      .displacement = moves.PairMove(*start.animations),
		                                      .startHeading = self.heading,
		                                      .routeHeading = self.targetHeading};
		break;
	case locomotion::Start::Kind::Turn:
		StartTurn(self, moves, self.targetHeading);
		break;
	}
}

void Arrive(CreatureLocomotion& self)
{
	ClearWalk(self);
	self.failed = false;
}

/// A step of a turn on the spot. Turning to face something, it is over once the heading is all but the target's, which
/// is checked first, so that a turn whose spins have ended is over at its next step; turning on the way, it sets off
/// along the route as soon as its time is up, walking in the same turn: true when it sets off.
bool TurnStep(CreatureLocomotion& self, const Moves& moves, float playbackRate)
{
	auto& move = *self.move;
	if (self.facingOnly && locomotion::TurnIsOver(-self.heading, -self.targetHeading))
	{
		self.heading = self.targetHeading;
		ClearWalk(self);
		return false;
	}
	const auto advance = TurnMs() * playbackRate;
	const auto from = move.timeMs;
	if (move.animations.has_value())
	{
		move.timeMs = std::min(move.timeMs + advance, move.durationMs);
		// The spin's own root turns the body; its heading changes once the turn is over
		TrackPair(self, moves, from, move.timeMs - from);
	}
	else
	{
		// The original's yaw is minus the heading
		self.heading = -locomotion::SpinlessYaw(-self.heading, -self.targetHeading, from, move.durationMs);
		move.timeMs = std::min(move.timeMs + advance, move.durationMs);
	}
	if (move.timeMs < move.durationMs)
	{
		return false;
	}
	self.heading = self.targetHeading;
	self.tracks.clear();
	if (self.facingOnly)
	{
		// Facing the way, the turn is over at its next step
		move.animations.reset();
		move.timeMs = 0.0f;
		return false;
	}
	self.move.reset();
	if (RouteFinished(self))
	{
		ClearWalk(self);
		return false;
	}
	// It walks on along the route from the start of its walk, without judging the way again
	self.motion = Motion::Walking;
	self.walkTimeMs = 0.0f;
	return true;
}

/// A turn of a step off. The creature steps a turn behind its follower: it reads the route's heading where the follower
/// got to by the last turn, is put where the follower was then, and only then the follower moves on by the step's move.
/// True when the step is over and it walks on in this same turn
bool StepOff(CreatureLocomotion& self, const Moves& moves)
{
	auto& move = *self.move;
	// the follower's heading turns the other way round from the creature's
	const auto routeHeading = self.follower != nullptr ? -self.follower->GetHeading() : move.routeHeading;

	// The route turning too far in that one turn ends the step: facing the route as it was, it goes back to the start
	// of the node it is on, to start again from there. A smaller turn is added to the step's heading at once
	const auto following = locomotion::StepHeadingFollowingRoute(self.heading, move.routeHeading, routeHeading);
	if (!following.has_value())
	{
		self.heading = move.routeHeading;
		self.speed = 0.0f;
		if (self.follower != nullptr)
		{
			self.follower->BackToNodeStart();
			PlaceAt(self, {self.follower->GetPosition().x, self.follower->GetPosition().z});
		}
		self.tracks.clear();
		self.move.reset();
		BeginLeg(self, moves);
		return false;
	}
	self.heading = *following;
	move.routeHeading = routeHeading;
	if (RouteFinished(self))
	{
		// the follower came to the route's end by the last turn: it stands where it is, facing the route
		self.heading = move.routeHeading;
		Arrive(self);
		return false;
	}

	const auto from = move.timeMs;
	if (from + TurnMs() >= move.durationMs)
	{
		// Over before it moves on: it walks on along the route in this same turn, at the step's speed, its walk taken up
		// by how far through the step it was
		const auto side = move.animations.has_value() && move.animations->from >= locomotion::animations::k_LeftStep
		                      ? locomotion::Side::Left
		                      : locomotion::Side::Right;
		self.walkTimeMs =
		    locomotion::WalkTimeAfterStep(from, move.durationMs, moves.Duration(locomotion::animations::k_Walk), side);
		self.heading = move.routeHeading;
		self.tracks.clear();
		self.move.reset();
		self.motion = Motion::Walking;
		return true;
	}
	move.timeMs = from + TurnMs();
	// The step's move for this turn carries it along the route: its follower goes at that move's speed
	const auto speed =
	    locomotion::StepFollowerSpeed(move.displacement, self.scale, static_cast<int32_t>(game_clock::MsPerTurn()),
	                                  static_cast<int32_t>(move.durationMs), -self.heading);
	self.speed = speed;
	self.distance = k_FollowerStepSeconds * speed;
	PlaceAt(self, {self.follower->GetPosition().x, self.follower->GetPosition().z});
	self.follower->SetSpeed(speed);
	self.follower->MoveAlongRoute();
	// The step's own root turns the body round towards the route as it steps; its heading follows once the step ends
	TrackPair(self, moves, from, move.timeMs - from);
	return false;
}

void Walk(CreatureLocomotion& self, const Moves& moves, float targetSpeed, float breathPhase)
{
	if (RouteFinished(self))
	{
		Arrive(self);
		return;
	}
	const auto seconds = TurnSeconds();
	// It slows to stop at the route's end. The follower's routes round their corners into arcs, so there is no corner
	// to slow for.
	const auto cap = locomotion::StopCap(self.follower->RemainingLength());
	const auto position = PositionOf(self);
	const auto ahead = position + (locomotion::DirectionOf(self.heading) * self.radius);
	const auto slope = locomotion::SlopeFactor(HeightAt(ahead), HeightAt(position), self.radius);
	self.speed = std::min(locomotion::Accelerate(self.speed, slope * targetSpeed, seconds), cap);

	const auto distance = self.speed * seconds;
	self.distance = distance;
	const auto advanced = AdvanceAlongRoute(self, distance);
	PlaceAt(self, advanced.position);
	if (advanced.finished)
	{
		Arrive(self);
		return;
	}
	if (locomotion::MeetsCorner(self.heading, advanced.heading))
	{
		// A corner: it stops, at the start of the node it has just come on to, and sets off again from there
		self.speed = 0.0f;
		if (advanced.newNode && self.follower != nullptr)
		{
			self.follower->BackToNodeStart();
			PlaceAt(self, {self.follower->GetPosition().x, self.follower->GetPosition().z});
		}
		BeginLeg(self, moves);
		return;
	}
	self.heading = advanced.heading;

	const auto walkFrom = self.walkTimeMs;
	const auto stand = moves.Cycle(locomotion::animations::k_Stand);
	const auto walk = moves.Cycle(locomotion::animations::k_Walk);
	const auto run = moves.Cycle(locomotion::animations::k_Run);
	const auto gait =
	    locomotion::BlendGait(self.speed, self.speeds, distance, self.scale, stand, walk, run, self.walkTimeMs, breathPhase);
	self.walkTimeMs = gait.walkTimeMs;
	for (const auto& slot : gait.slots)
	{
		CreatureLocomotion::Track track {.animation = slot.animation,
		                                 .fromMs = walkFrom,
		                                 .advanceMs = gait.walkAdvanceMs,
		                                 .durationMs = walk.durationMs,
		                                 .weight = slot.weight,
		                                 .looping = true,
		                                 .breathing = false};
		if (slot.animation == locomotion::animations::k_Stand)
		{
			track.durationMs = stand.durationMs;
			track.breathing = true;
		}
		else if (slot.animation == locomotion::animations::k_Run)
		{
			// The run keeps in step with the walk
			const auto ratio = walk.durationMs > 0.0f ? run.durationMs / walk.durationMs : 0.0f;
			track.fromMs = walkFrom * ratio;
			track.advanceMs = gait.walkAdvanceMs * ratio;
			track.durationMs = run.durationMs;
		}
		self.tracks.push_back(track);
	}
}

/// How far round something reaches, for walking up to it
float RadiusOf(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* other = registry.TryGet<const CreatureLocomotion>(entity))
	{
		return other->radius;
	}
	if (const auto* fixed = registry.TryGet<const Fixed>(entity))
	{
		return fixed->boundingRadius;
	}
	return 1.0f;
}

bool IsMovingMotion(Motion motion)
{
	return motion != Motion::Standing;
}

/// A thing a walk's first search meets on its way. (pending) The original lists those its creature could clear away,
/// for the give-up when the search finds it shut in; openblack doesn't port that give-up yet, so nothing is kept
void MetObstacle(void* /*creature*/, int32_t /*thing*/) {}

/// The planning of a route a turn on: whether it is ready, has failed or waits. Whether the plan ended with no way
/// there, as the follower reports it (not a give-up of its own)
bool StepPlanning(CreatureLocomotion& self)
{
	if (self.follower == nullptr)
	{
		return false;
	}
	// the first search of a walk from standing runs longer, noting the things it meets on the way; the others run the
	// follower's own number of search turns, the turn's share
	if (self.firstSearch)
	{
		self.firstSearch = false;
		self.follower->Update(&MetObstacle, locomotion::k_FirstSearchTurns);
	}
	else
	{
		self.follower->Update(nullptr, 0);
	}
	// Stopped on its way while its route was still being planned: it stands
	if (self.braking && (self.motion == Motion::Planning || self.motion == Motion::Confused) &&
	    self.follower->GetState() == FollowerState::Idle)
	{
		ClearWalk(self);
		return false;
	}
	if ((self.motion != Motion::Planning && self.motion != Motion::Confused) || self.routeReady)
	{
		return false;
	}
	const auto state = self.follower->GetState();
	const auto outcome = locomotion::PlanningOutcome(state == FollowerState::Following || state == FollowerState::Replanning,
	                                                 state == FollowerState::GivenUp, self.planEnd);
	if (outcome == locomotion::Planning::Ready)
	{
		self.routeReady = true;
	}
	else if (outcome == locomotion::Planning::Failed)
	{
		// No way there: it stands where it is
		ClearWalk(self);
		self.failed = true;
		return state != FollowerState::GivenUp;
	}
	return false;
}

/// No route to where its walk was to go: what the creature was doing is given up as a failure, and it refuses
void CantFindRoute(entt::entity creature)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().AbandonAction(creature);
	}
	if (Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().PlayNow(creature, audio::SoundAction::Refusal);
	}
}
} // namespace

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::StartMove(entt::entity creature, CreatureLocomotion& self,
                                                                         glm::vec2 point, float fraction, float minDistance,
                                                                         float maxDistance)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !MovesOf(body->species).CanWalk())
	{
		return MoveResult::Busy;
	}
	if (!IsValid(point, land_avoid::k_CreatureRadius))
	{
		return MoveResult::InvalidDestination;
	}
	self.fraction = fraction;
	self.failed = false;
	self.facingOnly = false;
	// Sent where it is already going, it goes on as it was, braking too
	if (IsMovingMotion(self.motion) && self.destination.has_value() &&
	    glm::distance(*self.destination, point) <= k_SameDestination)
	{
		return MoveResult::Started;
	}
	self.braking = false;

	// the planner looks no further than a route of twice the way there (from where it stood when asked) and a fixed
	// length more, however long it takes
	auto position = PositionOf(self);
	const auto planLength = locomotion::MaxRouteLength(glm::distance(position, point));
	const route_planner::Point2D destination {point.x, point.y};
	// Already on its way (waiting for its route, turning or stepping off into it, or walking it), the follower it has
	// takes the new point: the plan under way or the route goes on while the new one is planned
	if (IsMovingMotion(self.motion) && self.follower != nullptr && self.follower->GetState() != FollowerState::Idle &&
	    self.follower->GetState() != FollowerState::None)
	{
		self.ring = locomotion::MoveRing(glm::distance(position, point), minDistance, maxDistance);
		self.destination = point;
		self.firstSearch = false;
		// its plans start from its nav radius
		self.follower->SetDest(destination, self.ring.min, self.ring.max, ecs::object::GetNavRadius(creature), planLength);
		return MoveResult::Started;
	}
	// Straying somewhere it can't stand, it is put back on the nearest place it can first
	if (!IsValid(position, land_avoid::k_CreatureTurningRadius))
	{
		if (const auto valid = land_avoid::NearestValid(position, land_avoid::k_CreatureTurningRadius, k_StraySearch))
		{
			position = *valid;
			SnapTo(creature, self, position);
		}
	}
	const auto distance = glm::distance(position, point);
	self.ring = locomotion::MoveRing(distance, minDistance, maxDistance);
	self.destination = point;
	self.fidgetMs = k_FidgetMs;
	self.fidgeted = false;

	// setting off from standing, it starts afresh: no plan, no route and no obstacle known from an earlier walk
	auto& follower = FreshFollower(creature, self);
	// its plans keep things off by its nav radius
	follower.SetMargin(ecs::object::GetNavRadius(creature));
	const route_planner::Point2D start {position.x, position.y};
	follower.SetPosition(start);
	follower.SetPlanStart(start);
	self.routeReady = false;
	self.planEnd = -1;
	self.firstSearch = true;
	// the route is cut short of the point by the ring's inside, and the outside is only how near a way round an
	// obstacle must come to count as there
	follower.SetDest(destination, self.ring.min, self.ring.max, ecs::object::GetNavRadius(creature), planLength);
	SetIdle(self);
	self.motion = Motion::Planning;
	return MoveResult::Started;
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::MoveTo(entt::entity creature, glm::vec2 point, Pace pace,
                                                                      float minDistance, float maxDistance)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature>(creature))
	{
		return MoveResult::Busy;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	const auto& body = std::as_const(registry).Get<Creature>(creature);
	if (!self.started)
	{
		return MoveResult::Busy;
	}
	self.following.reset();
	const auto fractions = FractionsOf(body.species);
	return StartMove(creature, self, point, pace == Pace::Run ? fractions.run : fractions.walk, minDistance, maxDistance);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::WalkBack(entt::entity creature, glm::vec2 point, float hurry,
                                                                        float maxDistance)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature>(creature))
	{
		return MoveResult::Busy;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	const auto& body = std::as_const(registry).Get<Creature>(creature);
	if (!self.started)
	{
		return MoveResult::Busy;
	}
	self.following.reset();
	return StartMove(creature, self, point, hurry + FractionsOf(body.species).walk, 0.0f, maxDistance);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::MoveToObject(entt::entity creature, entt::entity target,
                                                                            Pace pace, float extra)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !ecs::IsAvailable(target))
	{
		return MoveResult::Busy;
	}
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	const auto* at = registry.TryGet<const Transform>(target);
	if (self == nullptr || at == nullptr)
	{
		return MoveResult::Busy;
	}
	const auto ring = locomotion::ObjectRing(self->radius, RadiusOf(registry, target), extra);
	return MoveTo(creature, glm::xz(at->position), pace, ring.min, ring.max);
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::Follow(entt::entity creature, entt::entity target,
                                                                      float distance, Pace pace)
{
	const auto result = MoveToObject(creature, target, pace, distance);
	auto& registry = Locator::entitiesRegistry::value();
	if (result != MoveResult::Busy && std::as_const(registry).AllOf<CreatureLocomotion>(creature))
	{
		auto& self = registry.Get<CreatureLocomotion>(creature);
		self.following = target;
		self.followDistance = distance;
	}
	return result;
}

CreatureLocomotionSystem::MoveResult CreatureLocomotionSystem::FleeFrom(entt::entity creature, glm::vec2 threat)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature>(creature))
	{
		return MoveResult::Busy;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	const auto& body = std::as_const(registry).Get<Creature>(creature);
	if (!self.started)
	{
		return MoveResult::Busy;
	}
	const auto fractions = FractionsOf(body.species);
	const auto distance = static_cast<float>(game_random::GameRand(k_RunAwayExtra)) + fractions.runAway;
	auto point = locomotion::RunAwayPoint(PositionOf(self), threat, distance);
	if (!IsValid(point, land_avoid::k_CreatureRadius))
	{
		const auto valid = land_avoid::NearestValid(point, land_avoid::k_CreatureRadius, k_RunAwaySearch);
		if (!valid.has_value())
		{
			return MoveResult::InvalidDestination;
		}
		point = *valid;
	}
	self.following.reset();
	return StartMove(creature, self, point, fractions.run, 0.0f, k_RunAwayArrival);
}

bool CreatureLocomotionSystem::TurnToFace(entt::entity creature, glm::vec2 point)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature>(creature))
	{
		return false;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	const auto& body = std::as_const(registry).Get<Creature>(creature);
	// Braking to a stop, it turns nowhere until it stands
	if (!self.started || self.braking)
	{
		return false;
	}
	const auto offset = point - PositionOf(self);
	if (glm::length(offset) <= k_Tiny)
	{
		return false;
	}
	ClearWalk(self);
	self.following.reset();
	self.facingOnly = true;
	self.targetHeading = locomotion::HeadingOf(offset);
	// It turns on the spot however little it has to turn: the turn ends once within a hair of the way
	const auto moves = MovesOf(body.species);
	StartTurn(self, moves, self.heading);
	return true;
}

bool CreatureLocomotionSystem::TurnToHeading(entt::entity creature, float heading)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion, Creature, CreatureAnimation>(creature))
	{
		return false;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	// Braking to a stop, it turns nowhere until it stands
	if (!self.started || self.braking)
	{
		return false;
	}
	// it stops where it is
	ClearWalk(self);
	self.following.reset();
	self.speed = 0.0f;
	self.motion = Motion::Standing;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	if (creature_layers::IsPlaying(animation.body))
	{
		// its action ends first, a static action's loop or a dance, and the turn waits for it
		animation.body = creature_layers::EndDance(creature_layers::EndLoop(animation.body));
		self.turnAfterAction = heading;
		return true;
	}
	self.turnAfterAction.reset();
	self.facingOnly = true;
	self.targetHeading = heading;
	StartTurn(self, MovesOf(std::as_const(registry).Get<Creature>(creature).species), self.heading);
	return true;
}

void CreatureLocomotionSystem::Place(entt::entity creature, glm::vec3 position)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature))
	{
		return;
	}
	const glm::vec2 point {position.x, position.z};
	// Put there on the ground, its walk and route given up and nothing left to blend from where it was
	if (auto* self = registry.TryGet<CreatureLocomotion>(creature); self != nullptr && self->started)
	{
		ClearWalk(*self);
		self->following.reset();
		SnapTo(creature, *self, point);
		return;
	}
	if (auto* transform = registry.TryGet<Transform>(creature))
	{
		transform->position = glm::vec3(point.x, HeightAt(point), point.y);
		ecs::NotifyTeleported(creature);
	}
}

void CreatureLocomotionSystem::BrakeToStop(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !std::as_const(registry).AllOf<CreatureLocomotion>(creature))
	{
		return;
	}
	auto& self = registry.Get<CreatureLocomotion>(creature);
	// Only a walk brakes; standing, or turning to face something, it stops as Stop has it
	if (!IsMovingMotion(self.motion) || self.facingOnly || self.follower == nullptr)
	{
		Stop(creature);
		return;
	}
	self.following.reset();
	self.braking = true;
	self.follower->StopMoving();
}

void CreatureLocomotionSystem::Stop(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature) && std::as_const(registry).AllOf<CreatureLocomotion>(creature))
	{
		auto& self = registry.Get<CreatureLocomotion>(creature);
		ClearWalk(self);
		self.following.reset();
	}
}

bool CreatureLocomotionSystem::IsMoving(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* self = registry.Valid(creature) ? registry.TryGet<const CreatureLocomotion>(creature) : nullptr;
	return self != nullptr && (IsMovingMotion(self->motion) || self->turnAfterAction.has_value());
}

bool CreatureLocomotionSystem::IsValidPosition(glm::vec2 point, float radius) const
{
	return IsValid(point, radius);
}

void CreatureLocomotionSystem::ShareSearchTurns()
{
	auto& registry = Locator::entitiesRegistry::value();
	size_t searching = 0;
	registry.Each<const CreatureLocomotion>([&searching](entt::entity /*entity*/, const CreatureLocomotion& self) {
		if (self.follower != nullptr &&
		    (self.follower->GetState() == FollowerState::Planning || self.follower->GetState() == FollowerState::Replanning))
		{
			++searching;
		}
	});
	const auto share = locomotion::SearchTurnsShare(searching);
	registry.Each<CreatureLocomotion>([share](entt::entity /*entity*/, CreatureLocomotion& self) {
		self.searchTurns = share;
		if (self.follower != nullptr)
		{
			self.follower->SetTurnsPerUpdate(share);
		}
	});
}

void CreatureLocomotionSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto turnMs = TurnMs();

	// The creatures, in the registry's order, found without making any storage
	std::vector<entt::entity> creatures;
	lookup.Each<const Creature, const CreatureLocomotion, const Transform, const CreatureAnimation>(
	    [&creatures](entt::entity entity, const auto&...) { creatures.push_back(entity); });

	// Following something, the walk is pointed anew as it moves away
	std::vector<std::pair<entt::entity, entt::entity>> retarget;
	for (const auto entity : creatures)
	{
		auto& self = registry.Get<CreatureLocomotion>(entity);
		if (!self.following.has_value() || !self.started)
		{
			continue;
		}
		const auto* at = ecs::IsAvailable(*self.following) ? lookup.TryGet<const Transform>(*self.following) : nullptr;
		if (at == nullptr)
		{
			self.following.reset();
			continue;
		}
		const auto target = glm::xz(at->position);
		const bool moved = IsMovingMotion(self.motion) && self.destination.has_value() &&
		                   glm::distance(target, *self.destination) > k_FollowRetarget;
		const bool left =
		    !IsMovingMotion(self.motion) && glm::distance(target, PositionOf(self)) > self.ring.max + k_FollowRetarget;
		if (moved || left)
		{
			retarget.emplace_back(entity, *self.following);
		}
	}
	for (const auto& [entity, target] : retarget)
	{
		const auto& self = lookup.Get<CreatureLocomotion>(entity);
		const auto& body = lookup.Get<Creature>(entity);
		const auto distance = self.followDistance;
		const auto fractions = FractionsOf(body.species);
		Follow(entity, target, distance, self.fraction >= fractions.run ? Pace::Run : Pace::Walk);
	}

	for (const auto entity : creatures)
	{
		const auto& creature = lookup.Get<Creature>(entity);
		const auto& transform = lookup.Get<Transform>(entity);
		auto& self = registry.Get<CreatureLocomotion>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		if (!self.started)
		{
			self.started = true;
			self.heading = ecs::creature_pose::ReadHeading(transform.rotation);
			self.toPosition = transform.position;
			self.toHeading = self.heading;
		}
		self.fromPosition = self.toPosition;
		self.fromHeading = self.toHeading;
		self.tracks.clear();
		self.distance = 0.0f;
		Measure(self, lookup, entity, creature, transform);
		const auto moves = MovesOf(creature.species);
		// A turn asked for while its body played an action starts once the body is free
		if (self.turnAfterAction.has_value() && !creature_layers::IsPlaying(animation.body))
		{
			self.facingOnly = true;
			self.targetHeading = *self.turnAfterAction;
			self.turnAfterAction.reset();
			StartTurn(self, moves, self.heading);
		}

		// The route is planned a little each turn; with no way there, the creature gives up at the end of its turn
		bool noRoute = StepPlanning(self);

		const auto* needs = lookup.TryGet<const CreatureNeeds>(entity);
		const auto fractions = FractionsOf(creature.species);
		const auto fraction =
		    locomotion::RequiredFraction(self.fraction, fractions.slow, needs != nullptr ? needs->needs.exhaustion : 0.0f);
		const auto targetSpeed = locomotion::TargetSpeed(fraction, self.speeds.run);

		switch (self.motion)
		{
		case Motion::Standing:
			self.speed = 0.0f;
			break;
		case Motion::Planning:
			if (self.routeReady)
			{
				// Sitting or playing an action, it gets up and finishes first
				if (creature_layers::IsLooping(animation.body))
				{
					animation.body = creature_layers::EndLoop(animation.body);
				}
				if (!creature_layers::IsPlaying(animation.body))
				{
					self.routeReady = false;
					BeginLeg(self, moves);
				}
				break;
			}
			self.fidgetMs -= turnMs;
			if (self.fidgetMs <= 0.0f)
			{
				// The first fidget is always a look about
				const auto choice = self.fidgeted ? game_random::GameRand(3) : 0u;
				self.fidgeted = true;
				self.fidgetMs = k_FidgetMs;
				if (choice == 0)
				{
					self.fidgetMs = k_LookAboutFidgetMs;
				}
				else if (choice == 1)
				{
					animation.face =
					    creature_layers::PullFace(animation.face, k_PuzzledFace, k_PuzzledMs, creature_face::Cue::Lost);
				}
				else
				{
					const bool mirrored = game_random::GameRand(2) != 0;
					if (auto confused =
					        creature_layers::PlayOnce(animation.body, creature_layers::animations::k_Confused, mirrored))
					{
						animation.body = *confused;
						self.motion = Motion::Confused;
					}
				}
			}
			break;
		case Motion::Confused:
			if (!creature_layers::IsPlaying(animation.body))
			{
				self.motion = Motion::Planning;
			}
			break;
		case Motion::Turning:
			// at the animation rate of the size it is drawn at, as every animation but the walk's
			if (TurnStep(self, moves,
			             creature_layers::BodyRate(ecs::creature_pose::DrawnSize(lookup, entity), animation.realSpeed)))
			{
				Walk(self, moves, targetSpeed, animation.breathPhase);
			}
			break;
		case Motion::Stepping:
			if (StepOff(self, moves))
			{
				Walk(self, moves, targetSpeed, animation.breathPhase);
			}
			break;
		case Motion::Walking:
			Walk(self, moves, targetSpeed, animation.breathPhase);
			break;
		}

		// Strayed somewhere it can't stand, it is put back where it can and sets off again
		if ((self.motion == Motion::Walking || self.motion == Motion::Stepping) && self.destination.has_value() &&
		    !IsValid(PositionOf(self), land_avoid::k_CreatureTurningRadius))
		{
			if (const auto valid =
			        land_avoid::NearestValid(PositionOf(self), land_avoid::k_CreatureTurningRadius, k_StraySearch))
			{
				SnapTo(entity, self, *valid);
				const auto destination = *self.destination;
				const auto ring = self.ring;
				SetIdle(self);
				DropRoute(self);
				// a walk that can't set off again from there is no way there either
				noRoute = StartMove(entity, self, destination, self.fraction, ring.min, ring.max) != MoveResult::Started;
			}
		}
		self.toHeading = self.heading;
		// The Transform moves once a turn, to where the turn ends
		ecs::creature_pose::CommitTurnPose(entity, self.toPosition, self.toHeading);
		if (noRoute)
		{
			CantFindRoute(entity);
		}
	}
}

void CreatureLocomotionSystem::Update(float turnFraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto f = std::clamp(turnFraction, 0.0f, 1.0f);
	std::vector<entt::entity> creatures;
	lookup.Each<const CreatureLocomotion, const CreatureAnimation>(
	    [&creatures](entt::entity entity, const auto&...) { creatures.push_back(entity); });
	bool moved = false;
	for (const auto entity : creatures)
	{
		const auto& self = lookup.Get<CreatureLocomotion>(entity);
		if (!self.started)
		{
			continue;
		}
		// Only the drawn pose moves between turns; the Transform stays where the turn put it
		if (lookup.AllOf<CreatureDrawPose>(entity))
		{
			auto& pose = registry.Get<CreatureDrawPose>(entity);
			const auto position = self.fromPosition + ((self.toPosition - self.fromPosition) * f);
			const auto heading = self.fromHeading + (locomotion::WrapAngle(self.toHeading - self.fromHeading) * f);
			const auto rotation = affine::AngleY(-heading);
			if (position != pose.position || rotation != pose.rotation)
			{
				pose.position = position;
				pose.rotation = rotation;
				moved = true;
			}
		}
		auto& animation = registry.Get<CreatureAnimation>(entity);
		animation.slots = SlotsAt(self.tracks, animation.breathPhase, f);
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

void CreatureLocomotionSystem::AnimationAt(entt::entity creature, float turnFraction, CreatureAnimationInputs& inputs) const
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	const auto* self = lookup.TryGet<const CreatureLocomotion>(creature);
	const auto* animation = lookup.TryGet<const CreatureAnimation>(creature);
	if (self == nullptr || animation == nullptr || !self->started)
	{
		return;
	}
	inputs.slots = SlotsAt(self->tracks, animation->breathPhase, std::clamp(turnFraction, 0.0f, 1.0f));
}

std::vector<CreatureAnimation::Slot> CreatureLocomotionSystem::SlotsAt(std::span<const CreatureLocomotion::Track> tracks,
                                                                       float breathPhase, float share)
{
	std::vector<CreatureAnimation::Slot> slots;
	for (const auto& track : tracks)
	{
		const auto time = track.breathing
		                      ? static_cast<float>(creature_animation::BreathTime(
		                            breathPhase, static_cast<uint32_t>(std::max(track.durationMs, 1.0f))))
		                      : locomotion::TrackTime(track.fromMs, track.advanceMs, track.durationMs, track.looping, share);
		slots.push_back({.animation = track.animation, .timeMs = time, .weight = track.weight, .mirrored = false});
		if (const auto& pair = track.pair)
		{
			// The blend lasts its length cut to whole milliseconds
			const auto length = pair->durationMs > 0.0f ? pair->durationMs : 0.0f;
			slots.back().pair = CreatureAnimation::Slot::Pair {
			    .to = pair->to,
			    .weight = pair->weight,
			    .timeMs = locomotion::TrackTime(pair->fromMs, pair->advanceMs, pair->durationMs, false, share),
			    .durationMs = static_cast<uint32_t>(length)};
		}
	}
	return slots;
}
