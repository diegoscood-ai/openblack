/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerHand.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <bit>
#include <string_view>
#include <utility>

#include "3D/MapCoords.h"
#include "Common/GameAngle.h"
#include "Common/GameDistance.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "ECS/ComputerPlayerScores.h"
#include "ECS/ComputerPlayerScoresDetail.h"
#include "InfoConstants.h"
#include "Magic/Gestures/GestureIcons.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using openblack::ecs::components::ComputerHand;
using openblack::ecs::components::ComputerMind;
using openblack::ecs::components::HandArgument;
using openblack::ecs::components::HandStep;
using openblack::ecs::components::HandSubAction;

namespace
{

using enum HandSubAction;

/// The move that each step needs in front of it, by the step's number, as the game's table of steps gives it
constexpr std::array<HandSubAction, static_cast<size_t>(None)> k_Prerequisites {
    MoveHandToObject, // PickUpObject
    None,             // MoveHandToPos
    None,             // MoveHandToObject
    None,             // MoveHandToCreatureHead
    None,             // MoveHandNearPos
    None,             // MoveHandNearObject
    None,             // MoveHandToPosStopWhenOutOfInfluence
    MoveHandNearPos,  // ThrowObjectInHandAtPos
    MoveHandToPos,    // DropObjectInHandAtPos
    None,             // SayText
    MoveHandToObject, // GiveObjectInHandToCreature
    None,             // SetLeash
    None,             // AttachLeashToObject
    MoveHandToObject, // SlapCreature
    MoveHandToObject, // StrokeCreature
    MoveHandToObject, // CastSpellAtObject
    MoveHandToPos,    // CastSpellAtPos
    None,             // CastSpellTowardPos
    MoveHandToPos,    // CastShieldAtPos
    None,             // CloseDownCurrentSpell
    MoveHandToObject, // RaiseTotem
    MoveHandToObject, // LowerTotem
    None,             // MakeGesture
    None,             // Wait
    MoveHandToObject, // DropObjectInHandOnObject
    MoveHandToObject, // ForceCreatureToPerformAction
    MoveHandToObject, // ForceCreatureDominantDesire
    MoveHandToObject, // WaitForObjectToBeNearPos
    MoveHandToObject, // WaitForSpellToWearOff
    MoveHandToObject, // CreateFoodFromContainer
    MoveHandToObject, // CreateWoodFromContainer
    MoveHandToObject, // SetCreatureSpeedForJourney
    None,             // MakeWorshippersChantForSpell
    None,             // MakeWorshippersChantForSpellNoWait
    None,             // SetHandToPos
    None,             // MoveHandToPosDirectYInterpolation
    None,             // SetHandToPosDirectY
};

/// The first step's stage is over: the next stage, or after the third the step is done and the next one starts
void Advance(ComputerHand& hand)
{
	++hand.stage;
	if (hand.stage < 3)
	{
		return;
	}
	hand.stage = 0;
	hand.steps.pop_front();
	// The step that the move was put in front of goes on without being looked at again; any other next step is
	if (hand.prerequisiteInserted)
	{
		hand.prerequisiteInserted = false;
	}
	else
	{
		hand.checkPrerequisite = true;
	}
}

} // namespace

namespace openblack::computer_hand
{

HandSubAction Prerequisite(HandSubAction type)
{
	const auto index = static_cast<size_t>(type);
	return index < k_Prerequisites.size() ? k_Prerequisites.at(index) : None;
}

bool IsReady(const ComputerHand& hand)
{
	return hand.steps.empty();
}

map_coords::MapCoords ReportedHandPlace(const ComputerHand* hand, uint32_t turn)
{
	if (hand == nullptr || !hand->lastStepTurn.has_value())
	{
		return {};
	}
	const bool steppedThisTurn = hand->lastStepTurn == std::optional<uint32_t>(turn);
	return map_coords::FromWorld(steppedThisTurn ? hand->turnStartPoint : hand->point);
}

void Append(ComputerHand& hand, HandStep step)
{
	hand.steps.push_back(std::move(step));
}

void Clear(ComputerHand& hand, StageRunner& runner)
{
	hand.steps.clear();
	hand.stage = 0;
	hand.checkPrerequisite = true;
	hand.prerequisiteInserted = false;
	runner.LetGo(hand);
}

void SweepGone(ComputerHand& hand, StageRunner& runner)
{
	for (const auto& step : hand.steps)
	{
		for (const auto& argument : step.arguments)
		{
			if (argument.kind == HandArgument::Kind::Thing && (argument.thing == entt::null || runner.IsGone(argument.thing)))
			{
				Clear(hand, runner);
				return;
			}
		}
	}
}

void KeepMapPlace(ComputerHand& hand)
{
	hand.mapX = map_coords::ToFixed(hand.point.x);
	hand.mapZ = map_coords::ToFixed(hand.point.z);
}

bool Step(ComputerHand& hand, StageRunner& runner)
{
	KeepMapPlace(hand);
	// What the hand holds is drawn where the hand is as its turn begins
	hand.turnStartPoint = hand.point;
	++hand.stepTurns;
	bool kept = true;
	bool putMoveInFront = false;
	if (!hand.steps.empty())
	{
		auto& first = hand.steps.front();
		auto move = hand.checkPrerequisite ? Prerequisite(first.type) : None;
		// an angry miracle cast at a thing or a point is cast from near it
		if (move != None && (first.type == CastSpellAtObject || first.type == CastSpellAtPos) && first.arguments.size() > 1 &&
		    first.arguments.at(1).intValue >= 0 && first.arguments.at(1).intValue < k_MiracleKinds &&
		    runner.IsAngryMiracle(first.arguments.at(1).intValue))
		{
			move = first.type == CastSpellAtObject ? MoveHandNearObject : MoveHandNearPos;
		}
		if (move != None)
		{
			putMoveInFront = true;
			// The move takes a copy of what the step acts on first
			HandStep inFront {.type = move, .arguments = {}};
			inFront.arguments.push_back(first.arguments.empty() ? HandArgument {} : first.arguments.front());
			hand.steps.push_front(std::move(inFront));
			hand.checkPrerequisite = false;
			hand.prerequisiteInserted = true;
		}
		else
		{
			switch (runner.Run(hand, first, hand.stage))
			{
			case StageResult::Done:
				Advance(hand);
				break;
			case StageResult::Failed:
				Clear(hand, runner);
				kept = false;
				break;
			case StageResult::Going:
				break;
			}
		}
	}
	if (hand.steps.empty())
	{
		hand.busy = false;
	}
	// The interface reports the hand's place as its camera at the end of the turn, unless the turn only put a move in
	// front of the first step
	if (!putMoveInFront)
	{
		hand.reportedCamera = hand.point;
	}
	return kept;
}

// The game runs these moves on a 24-bit FPU, so every operation below rounds to a float, as written

float StepLength(float speed)
{
	// The turns in a second are a whole number
	return speed / static_cast<float>(1000 / k_TurnMilliseconds);
}

void HoverOverGround(ComputerHand& hand, const Ground& ground)
{
	const auto mapX = map_coords::MetresToFixedForHandLookup(hand.point.x);
	const auto mapZ = map_coords::MetresToFixedForHandLookup(hand.point.z);
	hand.point.y = ground.AltitudeAt(mapX, mapZ) + k_HoverHeight;
}

glm::vec3 PointFromMap(int32_t mapX, int32_t mapZ, float heightAboveGround, const Ground& ground)
{
	return {map_coords::ToMetres(mapX), ground.AltitudeAt(mapX, mapZ) + heightAboveGround, map_coords::ToMetres(mapZ)};
}

void StartTowards(ComputerHand& hand, const glm::vec3& target)
{
	hand.target = target;
}

StageResult MoveOverGround(ComputerHand& hand, const Ground& ground)
{
	// The distance is the game's across the land; the way is along the land
	const float distance = game_math::GetDistance(hand.point, hand.target);
	float dx = hand.target.x - hand.point.x;
	float dz = hand.target.z - hand.point.z;
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(dx * dx + dz * dz);
		dx = dx * inverse;
		dz = inverse * dz;
	}
	const float step = StepLength(hand.speed);
	if (distance <= step)
	{
		hand.point = hand.target;
		HoverOverGround(hand, ground);
		return StageResult::Done;
	}
	hand.point.x = dx * step + hand.point.x;
	hand.point.z = dz * step + hand.point.z;
	HoverOverGround(hand, ground);
	return StageResult::Going;
}

StageResult MoveUntilOutOfInfluence(ComputerHand& hand, const Ground& ground, StageRunner& runner)
{
	if (MoveOverGround(hand, ground) == StageResult::Done)
	{
		return StageResult::Done;
	}
	// the hand's place on the map grid, with no height: where its player has no influence the move ends
	const map_coords::MapCoords place {.x = map_coords::ToFixed(hand.point.x), .z = map_coords::ToFixed(hand.point.z)};
	return runner.InfluenceAt(place) == 0.0f ? StageResult::Done : StageResult::Going;
}

StageResult MoveStraight(ComputerHand& hand)
{
	const float distance = game_math::GetDistance(hand.point, hand.target);
	float dx = hand.target.x - hand.point.x;
	float dy = hand.target.y - hand.point.y;
	float dz = hand.target.z - hand.point.z;
	if (dx != 0.0f || dy != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(dz * dz + dx * dx + dy * dy);
		dx = dx * inverse;
		dy = inverse * dy;
		dz = dz * inverse;
	}
	const float step = StepLength(hand.speed);
	if (distance <= step)
	{
		hand.point = hand.target;
		return StageResult::Done;
	}
	hand.point.x = dx * step + hand.point.x;
	hand.point.y = dy * step + hand.point.y;
	hand.point.z = dz * step + hand.point.z;
	return StageResult::Going;
}

namespace
{
/// The moves near a point and near a thing: their drift's longest step, and whether the target's height takes the
/// turn's leftover milliseconds
StageResult MoveNear(ComputerHand& hand, const Ground& ground, float longestDrift, bool targetHeightTakesLeftover)
{
	const float awayX = hand.point.x - hand.target.x;
	const float awayZ = hand.point.z - hand.target.z;
	const float distance = std::sqrt(awayZ * awayZ + awayX * awayX);
	float dx = hand.target.x - hand.point.x;
	float dz = hand.target.z - hand.point.z;
	if (!(distance > k_TooNearThePoint))
	{
		// too near: a drift along x, and up before the hold above the ground
		float step = StepLength(hand.speed);
		if (longestDrift < step)
		{
			step = longestDrift;
		}
		hand.point.x = step + hand.point.x;
		hand.point.y = step + hand.point.y;
		hand.point.z = 0.0f + hand.point.z;
		HoverOverGround(hand, ground);
		return StageResult::Going;
	}
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(dz * dz + dx * dx);
		dx = inverse * dx;
		dz = dz * inverse;
	}
	float step = StepLength(hand.speed);
	if (distance < step)
	{
		step = distance;
	}
	const float moveX = step * dx;
	const float moveY = step * 0.0f;
	const float moveZ = dz * step;
	hand.point.x = moveX + hand.point.x;
	hand.point.y = moveY + hand.point.y;
	hand.point.z = moveZ + hand.point.z;
	if (targetHeightTakesLeftover)
	{
		// the point's height takes what is left of a second over the turns in it, as a whole number's bits
		hand.target.y = std::bit_cast<float>(1000u % k_TurnMilliseconds);
	}
	const float leftX = hand.point.x - hand.target.x;
	const float leftZ = hand.point.z - hand.target.z;
	HoverOverGround(hand, ground);
	return std::sqrt(leftZ * leftZ + leftX * leftX) > k_NearThePoint ? StageResult::Going : StageResult::Done;
}
} // namespace

StageResult MoveNearTurn(ComputerHand& hand, const Ground& ground)
{
	return MoveNear(hand, ground, k_LongestDrift, true);
}

StageResult MoveNearThingTurn(ComputerHand& hand, const Ground& ground)
{
	return MoveNear(hand, ground, k_LongestDriftNearThing, false);
}

glm::vec3 ThrowVelocity(const glm::vec3& from, const glm::vec3& to)
{
	const float wayX = to.x - from.x;
	const float wayY = to.y - from.y;
	const float wayZ = to.z - from.z;
	const float distance = std::sqrt((wayZ * wayZ + wayY * wayY) + wayX * wayX);
	const float time = std::sqrt(distance * k_OneOverGravity);
	const float rise = time * time * 0.5f;
	// what the land's pull takes off over the flight, across, up and along
	const glm::vec3 fall {0.0f, rise * k_Gravity, 0.0f};
	const float inverse = 1.0f / time;
	return {(wayX - fall.x) * inverse, (wayY - fall.y) * inverse, (wayZ - fall.z) * inverse};
}

StageResult ThrowAtPosStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner)
{
	if (stage != 1)
	{
		return StageResult::Done;
	}
	const glm::vec3 point = step.arguments.empty() ? glm::vec3(0.0f) : step.arguments.at(0).point;
	runner.ThrowHeld(hand, ThrowVelocity(hand.point, point));
	return StageResult::Done;
}

StageResult GiveToCreatureStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner)
{
	if (stage != 1)
	{
		return StageResult::Done;
	}
	const entt::entity creature = step.arguments.empty() ? entt::entity {entt::null} : step.arguments.at(0).thing;
	runner.ThrowHeld(hand, ThrowVelocity(hand.point, runner.InteractPoint(creature)));
	return StageResult::Done;
}

StageResult MoveToThing(ComputerHand& hand, const Ground& ground)
{
	float dx = hand.target.x - hand.point.x;
	float dz = hand.target.z - hand.point.z;
	const float distance = std::sqrt(dz * dz + dx * dx);
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / distance;
		dx = inverse * dx;
		dz = dz * inverse;
	}
	float step = StepLength(hand.speed);
	if (distance < step)
	{
		step = distance;
	}
	if (distance > k_FarFromObject)
	{
		step = k_FarStride;
	}
	else if (distance > k_NearObject)
	{
		step = k_NearStride;
	}
	hand.point.x = dx * step + hand.point.x;
	hand.point.z = dz * step + hand.point.z;
	const float leftX = hand.point.x - hand.target.x;
	const float leftZ = hand.point.z - hand.target.z;
	HoverOverGround(hand, ground);
	return std::sqrt(leftZ * leftZ + leftX * leftX) <= k_AtObject ? StageResult::Done : StageResult::Going;
}

void PutAt(ComputerHand& hand, const glm::vec3& point, const Ground* ground)
{
	hand.target = point;
	hand.point = point;
	if (ground != nullptr)
	{
		HoverOverGround(hand, *ground);
	}
}

HandStep StepToPoint(HandSubAction type, const glm::vec3& point)
{
	HandStep step {.type = type, .arguments = {}};
	step.arguments.push_back({.kind = HandArgument::Kind::Point, .point = point});
	return step;
}

void TakeForScript(ComputerHand& hand, HandStep step, float speed, StageRunner& runner)
{
	Clear(hand, runner);
	hand.speed = speed;
	runner.LetGo(hand);
	Append(hand, std::move(step));
	hand.busy = true;
	hand.held = true;
}

void Pause(ComputerHand& hand, bool paused)
{
	hand.held = paused;
	hand.speed = hand.playerSpeed;
}

void SetSpeed(ComputerHand& hand, float speed)
{
	hand.playerSpeed = speed;
	hand.speed = speed;
}

bool MovedByScript(const ComputerHand& hand)
{
	return hand.busy || !hand.queue.empty();
}

bool TakesItsTurn(const ComputerHand& hand)
{
	return !hand.steps.empty() || hand.held;
}

glm::vec3 OnGround(const ComputerHand& hand, const Ground& ground)
{
	return PointFromMap(hand.mapX, hand.mapZ, 0.0f, ground);
}

std::optional<PlayerNames> LetGoCreditedTo(PlayerNames handPlayer)
{
	return handPlayer != PlayerNames::NEUTRAL ? std::optional(handPlayer) : std::nullopt;
}

bool MayBePickedUp(entt::entity thing, std::optional<entt::entity> heldByPlayersHand,
                   std::span<const entt::entity> heldByComputerHands)
{
	if (thing == entt::null || heldByPlayersHand == thing)
	{
		return false;
	}
	return std::ranges::find(heldByComputerHands, thing) == heldByComputerHands.end();
}

StageResult PickUpTurn(ComputerHand& hand, entt::entity thing, StageRunner& runner)
{
	if (hand.heldObject != entt::null)
	{
		runner.LetGo(hand);
		return StageResult::Going;
	}
	if (!runner.CanBePickedUp(thing))
	{
		return StageResult::Failed;
	}
	if (runner.TakeIntoHand(hand, thing))
	{
		hand.heldObject = thing;
	}
	return StageResult::Done;
}

bool TakeLeftOver(ComputerHand& hand, entt::entity leftOver, StageRunner& runner)
{
	if (leftOver == entt::null || !runner.TakeIntoHand(hand, leftOver))
	{
		return false;
	}
	hand.heldObject = leftOver;
	return true;
}

StageResult DropTurn(ComputerHand& hand, StageRunner& runner)
{
	runner.LetGo(hand);
	return StageResult::Done;
}

StageResult GiveHeldTurn(ComputerHand& hand, entt::entity thing, StageRunner& runner)
{
	runner.GiveHeldTo(hand, thing);
	return StageResult::Done;
}

StageResult TakeFromContainerTurn(ComputerHand& hand, entt::entity container, int32_t asked, ResourceType type,
                                  StageRunner& runner)
{
	// only a pot of the resource is filled; anything else in the hand leaves no room, and the step waits
	const auto held = hand.heldObject;
	const bool potHeld = held != entt::null && runner.IsPotOf(held, type);
	if (held != entt::null && !potHeld)
	{
		return StageResult::Going;
	}
	int32_t amount = asked;
	if (potHeld)
	{
		amount -= runner.ResourceOf(held, type);
	}
	if (const auto has = runner.ResourceOf(container, type); amount >= has)
	{
		amount = has;
	}
	auto pot = potHeld ? held : entt::entity {entt::null};
	if (amount > 0)
	{
		runner.RemoveResource(container, type, amount);
		if (potHeld)
		{
			runner.AddToPot(pot, type, amount);
		}
		else
		{
			pot = runner.MakeHandPot(container, type, amount);
			if (pot != entt::null && runner.TakeIntoHand(hand, pot))
			{
				hand.heldObject = pot;
			}
		}
	}
	return pot != entt::null ? StageResult::Done : StageResult::Failed;
}

uint8_t GestureOfMiracle(const InfoConstants& info, int32_t miracle, int32_t which)
{
	// the tables' rows name no seed: the game gives each miracle its first seed at load
	if (miracle < 0 || static_cast<size_t>(miracle) >= magic::k_MagicTypeCount)
	{
		return 0;
	}
	const auto seed = magic::GetSpellSeedOfMagicInfo(info, static_cast<MagicType>(miracle));
	if (seed == SpellSeedType::None)
	{
		return 0;
	}
	const auto& seedInfo = magic::GetSpellSeedInfo(info, seed);
	return static_cast<uint8_t>(which == 0 ? seedInfo.selectionGesture : seedInfo.gesture);
}

glm::vec3 GesturePoint(const glm::vec3& origin, const glm::vec3& shapePoint)
{
	// each step rounds to a float, as the game's 24-bit arithmetic does
	const float across = shapePoint.x * k_GestureSize;
	const float up = shapePoint.z * k_GestureSize;
	return {across + origin.x, up + origin.y, origin.z};
}

void StartGesture(ComputerHand& hand, const std::vector<glm::vec3>& shape)
{
	hand.gestureOrigin = hand.point;
	hand.gestureProgress = 0.0f;
	hand.point = GesturePoint(hand.gestureOrigin, magic::gestures::ShapePointAt(shape, 0.0f));
}

StageResult GestureTurn(ComputerHand& hand, const std::vector<glm::vec3>& shape, float lengthRatio)
{
	const float turns = lengthRatio * k_GestureTurnsForTheCircle;
	const float share = 1.0f / turns;
	float progress = share + hand.gestureProgress;
	if (!(progress < 1.0f))
	{
		progress = 1.0f;
	}
	hand.gestureProgress = progress;
	if (!(progress < 1.0f))
	{
		return StageResult::Done;
	}
	hand.point = GesturePoint(hand.gestureOrigin, magic::gestures::ShapePointAt(shape, progress));
	return StageResult::Going;
}

void EndGesture(ComputerHand& hand)
{
	hand.point = hand.gestureOrigin;
}

StageResult GestureStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner)
{
	// the miracle and which of its gestures, both whole numbers
	const int32_t miracle = step.arguments.empty() ? 0 : step.arguments.at(0).intValue;
	const int32_t which = step.arguments.size() < 2 ? 0 : step.arguments.at(1).intValue;
	const auto gesture = runner.GestureOf(miracle, which);
	if (stage == 0)
	{
		StartGesture(hand, runner.GestureShape(gesture));
		return StageResult::Done;
	}
	if (stage == 1)
	{
		return GestureTurn(hand, runner.GestureShape(gesture), runner.GestureLengthRatio(gesture));
	}
	EndGesture(hand);
	return StageResult::Done;
}

uint32_t CountedSeconds(int32_t turns)
{
	// the turns as an unsigned whole number, over the whole turns in a second
	return static_cast<uint32_t>(turns) / (1000 / k_TurnMilliseconds);
}

StageResult ChantNoWaitStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner)
{
	return stage == 2 ? StageResult::Done : ChantStage(hand, step, stage, runner);
}

StageResult ChantStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner)
{
	// the miracle, a whole number, and its icon
	const int32_t miracle = step.arguments.empty() ? 0 : step.arguments.at(0).intValue;
	const entt::entity icon = step.arguments.size() < 2 ? entt::entity {entt::null} : step.arguments.at(1).thing;
	// the count of turns is the one a wait counts down
	if (stage == 0)
	{
		hand.waitTurns = 0;
		return runner.HasWorshipSite(icon) ? StageResult::Done : StageResult::Failed;
	}
	if (stage == 1)
	{
		++hand.waitTurns;
		if (!runner.IsWorshipIcon(icon))
		{
			return StageResult::Failed;
		}
		if (runner.IsIconCharging(icon))
		{
			return StageResult::Done;
		}
		const auto powerUp = runner.PowerUpOf(miracle);
		if (runner.ValidForStartCharge(icon, powerUp))
		{
			runner.StartCharge(icon, powerUp);
			return StageResult::Done;
		}
		if (!runner.HasWorshipSite(icon))
		{
			return StageResult::Failed;
		}
		return CountedSeconds(hand.waitTurns) > k_SecondsToStartCharging ? StageResult::Failed : StageResult::Going;
	}
	if (!runner.IsWorshipIcon(icon))
	{
		return StageResult::Failed;
	}
	if (!(runner.ChantNeeded(icon) > 0.0f))
	{
		return StageResult::Done;
	}
	++hand.waitTurns;
	return CountedSeconds(hand.waitTurns) > k_SecondsToCharge ? StageResult::Failed : StageResult::Going;
}

void FollowThing(ComputerHand& hand, const FollowedPlace& at, const Ground& ground)
{
	hand.target = {map_coords::ToMetres(at.place.x), at.height + at.place.altitude, map_coords::ToMetres(at.place.z)};
	float dx = hand.target.x - hand.point.x;
	float dz = hand.target.z - hand.point.z;
	const float distance = std::sqrt(dz * dz + dx * dx);
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / distance;
		dx = inverse * dx;
		dz = dz * inverse;
	}
	float step = StepLength(hand.speed);
	if (distance < step)
	{
		step = distance;
	}
	if (distance > k_FarFromObject)
	{
		step = k_FarStride;
	}
	else if (distance > k_NearObject)
	{
		step = k_NearStride;
	}
	hand.point.x = dx * step + hand.point.x;
	hand.point.z = dz * step + hand.point.z;
	const auto mapX = map_coords::MetresToFixedForHandLookup(hand.point.x);
	const auto mapZ = map_coords::MetresToFixedForHandLookup(hand.point.z);
	const float over = at.height * k_HeightsOverAFollowedThing;
	hand.point.y = ground.AltitudeAt(mapX, mapZ) + over;
}

StageResult CastAtObjectStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner,
                              const Ground& ground)
{
	// the thing, the miracle, a whole number, and its icon
	const entt::entity thing = step.arguments.empty() ? entt::entity {entt::null} : step.arguments.at(0).thing;
	const int32_t miracle = step.arguments.size() < 2 ? 0 : step.arguments.at(1).intValue;
	const entt::entity icon = step.arguments.size() < 3 ? entt::entity {entt::null} : step.arguments.at(2).thing;
	const auto follow = [&]() {
		if (const auto at = runner.PlaceToFollow(thing); at.has_value())
		{
			FollowThing(hand, *at, ground);
		}
	};
	if (stage == 0)
	{
		return StageResult::Done;
	}
	if (stage == 1)
	{
		// an angry miracle is cast from where the hand is, without following the thing first
		if (miracle < 0 || miracle >= k_MiracleKinds || !runner.IsAngryMiracle(miracle))
		{
			follow();
		}
		const auto spell = runner.CastOnObject(thing, miracle, hand.point, icon);
		if (spell == entt::null)
		{
			return StageResult::Failed;
		}
		runner.KeepCast(spell, thing);
		hand.waitTurns = WaitTurns(runner.SecondsWithTheThingAfterACast(miracle));
		return StageResult::Done;
	}
	// a time of no turns counts below zero, and the hand follows the thing until the step is given up
	--hand.waitTurns;
	if (hand.waitTurns == 0)
	{
		return StageResult::Done;
	}
	follow();
	return StageResult::Going;
}

namespace
{
/// The place on the map grid of a step's point, on the land: x and z cut toward zero, no height
map_coords::MapCoords GroundPlaceOf(const glm::vec3& point)
{
	return {.x = map_coords::ToFixed(point.x), .z = map_coords::ToFixed(point.z), .altitude = 0.0f};
}
} // namespace

StageResult CastAtPosStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner)
{
	// the point, the miracle and its icon
	const glm::vec3 point = step.arguments.empty() ? glm::vec3(0.0f) : step.arguments.at(0).point;
	const int32_t miracle = step.arguments.size() < 2 ? 0 : step.arguments.at(1).intValue;
	const entt::entity icon = step.arguments.size() < 3 ? entt::entity {entt::null} : step.arguments.at(2).thing;
	if (stage == 0)
	{
		return StageResult::Done;
	}
	if (stage == 1)
	{
		const auto to = GroundPlaceOf(point);
		const auto spell = runner.CastAtPlace(miracle, to, map_coords::FromWorld(hand.point), icon, 1.0f);
		if (spell == entt::null)
		{
			return StageResult::Failed;
		}
		runner.KeepCastAt(spell, to);
		hand.waitTurns = WaitTurns(runner.SecondsWithTheThingAfterACast(miracle));
		return StageResult::Done;
	}
	// a time of no turns counts below zero and never ends
	--hand.waitTurns;
	return hand.waitTurns == 0 ? StageResult::Done : StageResult::Going;
}

void LeashToThing(ComputerHand& hand, entt::entity creature, entt::entity thing, LeashType type)
{
	const HandArgument creatureArgument {.kind = HandArgument::Kind::Thing, .thing = creature};
	Append(hand, {.type = HandSubAction::SetLeash,
	              .arguments = {creatureArgument, {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(type)}}});
	Append(hand, {.type = HandSubAction::AttachLeashToObject,
	              .arguments = {creatureArgument, {.kind = HandArgument::Kind::Thing, .thing = thing}}});
}

void TeachMiracle(ComputerHand& hand, entt::entity creature, entt::entity icon, MagicType miracle)
{
	// the action the creature is ordered to do at the icon, by its row of the game's table
	constexpr int32_t k_LessonAction = 82;
	const HandArgument creatureArgument {.kind = HandArgument::Kind::Thing, .thing = creature};
	const HandArgument iconArgument {.kind = HandArgument::Kind::Thing, .thing = icon};
	const HandArgument miracleArgument {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(miracle)};
	Append(hand, {.type = HandSubAction::MoveHandToObject, .arguments = {iconArgument}});
	Append(hand,
	       {.type = HandSubAction::ForceCreatureToPerformAction,
	        .arguments = {creatureArgument, {.kind = HandArgument::Kind::Int, .intValue = k_LessonAction}, iconArgument}});
	Append(hand, {.type = HandSubAction::MoveHandToObject, .arguments = {creatureArgument}});
	Append(hand, {.type = HandSubAction::MakeWorshippersChantForSpell, .arguments = {miracleArgument, iconArgument}});
	Append(hand, {.type = HandSubAction::CastSpellAtObject, .arguments = {creatureArgument, miracleArgument, iconArgument}});
}

float JourneyDistance(const glm::vec3& from, const glm::vec3& to)
{
	const float dx = to.x - from.x;
	const float dy = to.y - from.y;
	const float dz = to.z - from.z;
	return std::sqrt(dz * dz + dy * dy + dx * dx);
}

float JourneyFraction(float distance, float walk, float run)
{
	constexpr float k_LongJourney = 100.0f;
	constexpr float k_LongJourneyRun = 1.5f;
	return distance > k_LongJourney ? run * k_LongJourneyRun : (run + walk) * 0.5f;
}

StageResult JourneySpeedStage(const HandStep& step, uint8_t stage, StageRunner& runner)
{
	if (stage != 1)
	{
		return StageResult::Done;
	}
	const entt::entity thing = step.arguments.empty() ? entt::entity {entt::null} : step.arguments.at(0).thing;
	const auto body = runner.JourneyBodyOf(thing);
	if (!body.has_value())
	{
		return StageResult::Failed;
	}
	const glm::vec3 point = step.arguments.size() < 2 ? glm::vec3(0.0f) : step.arguments.at(1).point;
	const float distance = JourneyDistance(body->point, point);
	constexpr float k_LongJourney = 100.0f;
	if (!(distance > k_LongJourney))
	{
		// a short journey draws a number it does not use
		constexpr float k_ShortJourneyDraw = 0.4f;
		runner.DrawThrownAway(k_ShortJourneyDraw);
	}
	const float fraction = JourneyFraction(distance, body->walk, body->run);
	runner.SetJourneySpeed(thing, fraction > 0.0f ? std::min(fraction, 1.0f) : 0.0f);
	return StageResult::Done;
}

void SpeedForJourney(ComputerHand& hand, entt::entity creature, const glm::vec3& point)
{
	Append(hand, {.type = HandSubAction::SetCreatureSpeedForJourney,
	              .arguments = {{.kind = HandArgument::Kind::Thing, .thing = creature},
	                            {.kind = HandArgument::Kind::Point, .point = point}}});
}

StageResult RaiseTotemStage(const HandStep& step, uint8_t stage, StageRunner& runner)
{
	if (stage == 0)
	{
		const entt::entity totem = step.arguments.empty() ? entt::entity {entt::null} : step.arguments.at(0).thing;
		const float share = step.arguments.size() < 2 ? 0.0f : step.arguments.at(1).floatValue;
		runner.RaiseTotem(totem, share);
	}
	return StageResult::Done;
}

bool TeachNormalAction(ComputerHand& hand, entt::entity creature, uint32_t action, entt::entity totem)
{
	// of the ordinary actions only the one done at a totem has a lesson
	constexpr uint32_t k_TotemLesson = 2;
	// the action the creature is ordered to do at the totem, by its row of the game's table
	constexpr int32_t k_TotemAction = 20;
	if (action != k_TotemLesson || totem == entt::null)
	{
		return false;
	}
	const HandArgument totemArgument {.kind = HandArgument::Kind::Thing, .thing = totem};
	Append(hand, {.type = HandSubAction::ForceCreatureToPerformAction,
	              .arguments = {{.kind = HandArgument::Kind::Thing, .thing = creature},
	                            {.kind = HandArgument::Kind::Int, .intValue = k_TotemAction},
	                            totemArgument}});
	Append(hand, {.type = HandSubAction::RaiseTotem,
	              .arguments = {totemArgument, {.kind = HandArgument::Kind::Float, .floatValue = 1.0f}}});
	return true;
}

StageResult ForceDominantDesireStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner)
{
	if (stage == 0)
	{
		const entt::entity creature = step.arguments.empty() ? entt::entity {entt::null} : step.arguments.at(0).thing;
		const int32_t desire = step.arguments.size() < 2 ? 0 : step.arguments.at(1).intValue;
		const float seconds = step.arguments.size() < 3 ? 0.0f : step.arguments.at(2).floatValue;
		runner.ForceDominantDesire(creature, desire, seconds);
		hand.waitTurns = 0;
	}
	return StageResult::Done;
}

StageResult ForceActionStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner, const Ground& ground)
{
	const entt::entity creature = step.arguments.empty() ? entt::entity {entt::null} : step.arguments.at(0).thing;
	if (stage == 0)
	{
		const auto action = step.arguments.size() < 2 ? 0 : step.arguments.at(1).intValue;
		const entt::entity thing = step.arguments.size() < 3 ? entt::entity {entt::null} : step.arguments.at(2).thing;
		if (action >= 0)
		{
			runner.OrderCreature(creature, static_cast<uint32_t>(action), thing);
		}
		return StageResult::Done;
	}
	if (stage == 1)
	{
		if (const auto at = runner.PlaceToFollow(creature); at.has_value())
		{
			FollowThing(hand, *at, ground);
		}
		return runner.IsCommanded(creature) ? StageResult::Going : StageResult::Done;
	}
	return StageResult::Done;
}

StageResult SetLeashStage(const HandStep& step, uint8_t stage, StageRunner& runner)
{
	if (stage == 1)
	{
		const entt::entity creature = step.arguments.empty() ? entt::entity {entt::null} : step.arguments.at(0).thing;
		const int32_t type = step.arguments.size() < 2 ? 0 : step.arguments.at(1).intValue;
		if (runner.IsCreatureWithPlayer(creature))
		{
			runner.SetLeashType(creature, type);
		}
	}
	return StageResult::Done;
}

StageResult AttachLeashStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner, const Ground& ground)
{
	const entt::entity creature = step.arguments.empty() ? entt::entity {entt::null} : step.arguments.at(0).thing;
	const entt::entity thing = step.arguments.size() < 2 ? entt::entity {entt::null} : step.arguments.at(1).thing;
	if (stage == 0)
	{
		if (!runner.IsCreatureWithPlayer(creature))
		{
			return StageResult::Failed;
		}
		if (runner.IsLeashTied(creature))
		{
			runner.TakeLeashOff(creature);
		}
		return StageResult::Done;
	}
	if (stage == 1)
	{
		if (!runner.IsCreatureWithPlayer(creature) || !runner.IsLeashableObject(thing))
		{
			return StageResult::Failed;
		}
		runner.TieLeash(creature, thing);
		constexpr float k_TieWaitSeconds = 2.0f;
		hand.waitTurns = WaitTurns(k_TieWaitSeconds);
		return StageResult::Done;
	}
	const auto at = runner.PlaceToFollow(thing);
	if (at.has_value())
	{
		FollowThing(hand, *at, ground);
	}
	if (hand.waitTurns > 0)
	{
		--hand.waitTurns;
		return StageResult::Going;
	}
	const auto handPlace = map_coords::FromWorld(hand.point);
	const auto place = at.has_value() ? at->place : map_coords::MapCoords {};
	return game_math::GetDistanceInMetres(handPlace, place) < k_TiedLeashNear ? StageResult::Done : StageResult::Going;
}

StageResult CastShieldAtPosStage(ComputerHand& hand, const HandStep& step, uint8_t stage, StageRunner& runner)
{
	if (stage != 1)
	{
		return StageResult::Done;
	}
	// the point, the miracle, the radius and the icon
	const glm::vec3 point = step.arguments.empty() ? glm::vec3(0.0f) : step.arguments.at(0).point;
	const int32_t miracle = step.arguments.size() < 2 ? 0 : step.arguments.at(1).intValue;
	const float radius = step.arguments.size() < 3 ? 0.0f : step.arguments.at(2).floatValue;
	const entt::entity icon = step.arguments.size() < 4 ? entt::entity {entt::null} : step.arguments.at(3).thing;
	const auto to = GroundPlaceOf(point);
	// cast from the hand's place on the map grid, with no height
	const auto spell = runner.CastAtPlace(miracle, to, {.x = hand.mapX, .z = hand.mapZ}, icon, radius);
	if (spell == entt::null)
	{
		return StageResult::Failed;
	}
	runner.KeepCastAt(spell, to);
	return StageResult::Done;
}

int32_t WaitTurns(float seconds)
{
	// the turns in a second are a whole number, and the product is cut toward zero
	const auto turnsInASecond = static_cast<float>(1000 / k_TurnMilliseconds);
	return static_cast<int32_t>(turnsInASecond * seconds);
}

void StartWait(ComputerHand& hand, float seconds)
{
	hand.waitTurns = WaitTurns(seconds);
}

StageResult WaitTurn(ComputerHand& hand)
{
	// counted down first, so a wait of no turns goes below zero and goes on
	--hand.waitTurns;
	return hand.waitTurns != 0 ? StageResult::Going : StageResult::Done;
}

std::optional<MindAction> FindMindAction(std::string_view name, const computer_player::MindRowNames& names)
{
	// the first node the name names, in the order the game looks; what it does is its row's, whatever its name is now
	const auto node = computer_player::FindNode(name, names);
	if (!node.has_value())
	{
		return std::nullopt;
	}
	const auto own = computer_player::NamePrefix(computer_player::RowOf(*node).name);
	if (own == "FindPlaceToUseScaffoldForBuilding")
	{
		return MindAction::FindPlaceToUseScaffoldForBuilding;
	}
	if (own == "DIRECTPickupAndDropObject")
	{
		return MindAction::DirectPickupAndDropObject;
	}
	if (own == "TakeVillagerFromTownToWorshipSite")
	{
		return MindAction::TakeVillagerFromTownToWorshipSite;
	}
	if (own == "GetFoodFromOtherTownForObject")
	{
		return MindAction::GetFoodFromOtherTownForObject;
	}
	if (own == "GetWoodFromOtherTown")
	{
		return MindAction::GetWoodFromOtherTown;
	}
	if (own == "GetFoodFromOtherTownForStoragePit")
	{
		return MindAction::GetFoodFromOtherTownForStoragePit;
	}
	// the casting leaves of the towns' wants share one action, whichever name they have
	if (computer_player::RowOf(*node).action == computer_player::ActionKind::ShallCastFoodSpell)
	{
		return MindAction::CastIconsMiracleOnTownStore;
	}
	return MindAction::NotPorted;
}

bool IsDirectAction(std::string_view name)
{
	return computer_player::IsDirectName(name);
}

bool PickUpAndDrop(ComputerHand& hand, entt::entity thing, const glm::vec3& point, StageRunner& runner)
{
	if (!runner.CanBePickedUp(thing))
	{
		return false;
	}
	const HandArgument speed {.kind = HandArgument::Kind::Float, .floatValue = hand.playerSpeed};
	HandStep pickUp {.type = HandSubAction::PickUpObject, .arguments = {}};
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = thing});
	pickUp.arguments.push_back(speed);
	Append(hand, std::move(pickUp));
	HandStep drop {.type = HandSubAction::DropObjectInHandAtPos, .arguments = {}};
	drop.arguments.push_back({.kind = HandArgument::Kind::Point, .point = point});
	drop.arguments.push_back(speed);
	Append(hand, std::move(drop));
	return true;
}

void PickUpAndGive(ComputerHand& hand, entt::entity thing, entt::entity to)
{
	HandStep pickUp {.type = HandSubAction::PickUpObject, .arguments = {}};
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = thing});
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Float, .floatValue = hand.playerSpeed});
	Append(hand, std::move(pickUp));
	Append(hand,
	       {.type = HandSubAction::DropObjectInHandOnObject, .arguments = {{.kind = HandArgument::Kind::Thing, .thing = to}}});
}

bool PickUpAndThrow(ComputerHand& hand, entt::entity thing, const glm::vec3& point, StageRunner& runner)
{
	if (!runner.CanBePickedUp(thing))
	{
		return false;
	}
	HandStep pickUp {.type = HandSubAction::PickUpObject, .arguments = {}};
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = thing});
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Float, .floatValue = hand.playerSpeed});
	Append(hand, std::move(pickUp));
	Append(hand,
	       {.type = HandSubAction::ThrowObjectInHandAtPos, .arguments = {{.kind = HandArgument::Kind::Point, .point = point}}});
	return true;
}

void TakeToGate(ComputerHand& hand, entt::entity villager, std::optional<glm::vec3> gate, StageRunner& runner)
{
	// a site without a gate, or a villager that may not be picked up, gives no steps (and the action is done)
	if (!gate.has_value() || !runner.CanBePickedUp(villager))
	{
		return;
	}
	const HandArgument speed {.kind = HandArgument::Kind::Float, .floatValue = hand.playerSpeed};
	HandStep pickUp {.type = HandSubAction::PickUpObject, .arguments = {}};
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = villager});
	pickUp.arguments.push_back(speed);
	Append(hand, std::move(pickUp));
	// the move to the gate carries a number its move does not read
	HandStep move {.type = HandSubAction::MoveHandToPos, .arguments = {}};
	move.arguments.push_back({.kind = HandArgument::Kind::Point, .point = *gate});
	move.arguments.push_back({.kind = HandArgument::Kind::Float, .floatValue = k_GateMoveNumber});
	Append(hand, std::move(move));
	HandStep wait {.type = HandSubAction::Wait, .arguments = {}};
	wait.arguments.push_back({.kind = HandArgument::Kind::Float, .floatValue = k_SecondsAtTheGate});
	Append(hand, std::move(wait));
	HandStep drop {.type = HandSubAction::DropObjectInHandAtPos, .arguments = {}};
	drop.arguments.push_back({.kind = HandArgument::Kind::Point, .point = *gate});
	drop.arguments.push_back(speed);
	Append(hand, std::move(drop));
}

void FetchFoodFor(ComputerHand& hand, entt::entity pile, const glm::vec3& pilePoint, int32_t asked, entt::entity object)
{
	HandStep move {.type = HandSubAction::MoveHandToPos, .arguments = {}};
	move.arguments.push_back({.kind = HandArgument::Kind::Point, .point = pilePoint});
	Append(hand, std::move(move));
	HandStep take {.type = HandSubAction::CreateFoodFromContainer, .arguments = {}};
	take.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = pile});
	take.arguments.push_back({.kind = HandArgument::Kind::Int, .intValue = asked});
	Append(hand, std::move(take));
	HandStep give {.type = HandSubAction::DropObjectInHandOnObject, .arguments = {}};
	give.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = object});
	Append(hand, std::move(give));
}

void FetchWoodFor(ComputerHand& hand, entt::entity pile, uint32_t asked, entt::entity thing)
{
	// no move of its own: the take has the move to the pile in front of it
	HandStep take {.type = HandSubAction::CreateWoodFromContainer, .arguments = {}};
	take.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = pile});
	take.arguments.push_back({.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(asked)});
	Append(hand, std::move(take));
	HandStep give {.type = HandSubAction::DropObjectInHandOnObject, .arguments = {}};
	give.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = thing});
	Append(hand, std::move(give));
}

namespace
{
/// Nearer than 200 m to the icon the hand goes to it, else it makes the miracle's gesture; then the icon's worshippers
/// chant for the miracle, by the chant's step given
void ReachIconAndChant(ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon,
                       HandSubAction chant = HandSubAction::MakeWorshippersChantForSpell)
{
	const HandArgument miracleArgument {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(miracle)};
	const HandArgument iconArgument {.kind = HandArgument::Kind::Thing, .thing = icon};
	if (metresToIcon < k_GestureFromIconMetres)
	{
		Append(hand, {.type = HandSubAction::MoveHandToObject, .arguments = {iconArgument}});
	}
	else
	{
		Append(hand, {.type = HandSubAction::MakeGesture,
		              .arguments = {miracleArgument, {.kind = HandArgument::Kind::Int, .intValue = 0}}});
	}
	Append(hand, {.type = chant, .arguments = {miracleArgument, iconArgument}});
}
} // namespace

bool CastIconsMiracleAt(ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon, entt::entity thing)
{
	const auto type = static_cast<uint32_t>(miracle);
	const auto between = [type](MagicType first, MagicType last) {
		return type >= static_cast<uint32_t>(first) && type <= static_cast<uint32_t>(last);
	};
	// the miracles cast at a place: from inside the influence for fireballs and lightning, at the thing's place for the
	// storms, tornadoes and shields
	if (between(MagicType::Fireball, MagicType::LightningBoltPowerUpTwo) ||
	    between(MagicType::StormWindRain, MagicType::PhysicalShield))
	{
		return false;
	}
	const HandArgument miracleArgument {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(type)};
	const HandArgument iconArgument {.kind = HandArgument::Kind::Thing, .thing = icon};
	ReachIconAndChant(hand, icon, miracle, metresToIcon);
	// the miracles cast at an object; the flocks and anything else are not cast at all
	const bool atObject = between(MagicType::ExplosionOne, MagicType::FoodPowerUpOne) ||
	                      between(MagicType::Wood, MagicType::WaterPowerUpOne) ||
	                      between(MagicType::CreatureSpellFreeze, MagicType::CreatureSpellItchy);
	if (atObject)
	{
		Append(hand, {.type = HandSubAction::CastSpellAtObject,
		              .arguments = {{.kind = HandArgument::Kind::Thing, .thing = thing}, miracleArgument, iconArgument}});
	}
	return true;
}

bool CastIconsMiracleAtOrToward(ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon,
                                entt::entity thing, const TowardPlaces& toward)
{
	const auto type = static_cast<uint32_t>(miracle);
	const auto between = [type](MagicType first, MagicType last) {
		return type >= static_cast<uint32_t>(first) && type <= static_cast<uint32_t>(last);
	};
	if (between(MagicType::StormWindRain, MagicType::PhysicalShield))
	{
		ReachIconAndChant(hand, icon, miracle, metresToIcon);
		const HandArgument placeArgument {.kind = HandArgument::Kind::Point, .point = toward.place};
		Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {placeArgument}});
		Append(hand, {.type = HandSubAction::CastShieldAtPos,
		              .arguments = {placeArgument,
		                            {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(type)},
		                            {.kind = HandArgument::Kind::Float, .floatValue = 0.0f},
		                            {.kind = HandArgument::Kind::Thing, .thing = icon}}});
		return true;
	}
	if (!between(MagicType::Fireball, MagicType::LightningBoltPowerUpTwo))
	{
		return CastIconsMiracleAt(hand, icon, miracle, metresToIcon, thing);
	}
	ReachIconAndChant(hand, icon, miracle, metresToIcon);
	if (!toward.inside.has_value())
	{
		return true;
	}
	Append(hand,
	       {.type = HandSubAction::MoveHandToPos, .arguments = {{.kind = HandArgument::Kind::Point, .point = *toward.inside}}});
	const HandArgument edgeArgument {.kind = HandArgument::Kind::Point, .point = toward.edge};
	Append(hand, {.type = HandSubAction::MoveHandToPosStopWhenOutOfInfluence, .arguments = {edgeArgument}});
	Append(hand, {.type = HandSubAction::CastSpellTowardPos,
	              .arguments = {edgeArgument,
	                            {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(type)},
	                            {.kind = HandArgument::Kind::Thing, .thing = icon}}});
	return true;
}

bool CastIconsMiracleOnAndStay(ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon, entt::entity thing)
{
	if (!CastIconsMiracleAt(hand, icon, miracle, metresToIcon, thing))
	{
		return false;
	}
	Append(hand, {.type = HandSubAction::Wait,
	              .arguments = {{.kind = HandArgument::Kind::Float, .floatValue = k_SecondsAfterCastAtThing}}});
	Append(hand, {.type = HandSubAction::CloseDownCurrentSpell, .arguments = {}});
	return true;
}

void ChargeIconsMiracle(ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon)
{
	ReachIconAndChant(hand, icon, miracle, metresToIcon, HandSubAction::MakeWorshippersChantForSpellNoWait);
}

void ImpressWithIconsMiracle(ComputerHand& hand, entt::entity icon, MagicType miracle, const glm::vec3& point)
{
	const HandArgument miracleArgument {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(miracle)};
	const HandArgument iconArgument {.kind = HandArgument::Kind::Thing, .thing = icon};
	Append(hand, {.type = HandSubAction::MoveHandToObject, .arguments = {iconArgument}});
	Append(hand, {.type = HandSubAction::MakeWorshippersChantForSpell, .arguments = {miracleArgument, iconArgument}});
	Append(hand, {.type = HandSubAction::CastSpellAtPos,
	              .arguments = {{.kind = HandArgument::Kind::Point, .point = point}, miracleArgument, iconArgument}});
}

void AttackCreatureWithIcon(ComputerHand& hand, entt::entity icon, MagicType miracle, entt::entity creature)
{
	const HandArgument creatureArgument {.kind = HandArgument::Kind::Thing, .thing = creature};
	const HandArgument miracleArgument {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(miracle)};
	const HandArgument iconArgument {.kind = HandArgument::Kind::Thing, .thing = icon};
	Append(hand, {.type = HandSubAction::MoveHandNearObject, .arguments = {creatureArgument}});
	Append(hand, {.type = HandSubAction::MakeWorshippersChantForSpell, .arguments = {miracleArgument, iconArgument}});
	Append(hand, {.type = HandSubAction::CastSpellAtObject, .arguments = {creatureArgument, miracleArgument, iconArgument}});
}

map_coords::MapCoords WorkerDropPlace(const map_coords::MapCoords& centre, float radius,
                                      const std::function<bool(const map_coords::MapCoords&)>& woodThere)
{
	constexpr double k_PastTheBuilding = 1.25;
	constexpr float k_EighthOfATurn = 0.7853982f;
	constexpr float k_Turn = 6.2831855f;
	const auto reach = static_cast<float>(static_cast<double>(radius) * k_PastTheBuilding);
	map_coords::MapCoords place {};
	for (float angle = 0.0f; angle < k_Turn; angle = angle + k_EighthOfATurn)
	{
		const auto around = centre + game_math::GetPosFromAngle(angle, reach);
		if (map_coords::InBounds(around))
		{
			place = around;
		}
		if (woodThere(around))
		{
			break;
		}
	}
	return place;
}

glm::vec3 FoodThrowFrom(const glm::vec3& hand, const glm::vec3& creature)
{
	constexpr float k_StandOff = 20.0f;
	float dx = creature.x - hand.x;
	float dy = creature.y - hand.y;
	float dz = creature.z - hand.z;
	const float distance = std::sqrt(dy * dy + dx * dx + dz * dz);
	if (dx != 0.0f || dy != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(dy * dy + dx * dx + dz * dz);
		dx = dx * inverse;
		dy = dy * inverse;
		dz = dz * inverse;
	}
	if (distance < k_StandOff)
	{
		return {hand.x - dx * k_StandOff, hand.y - dy * k_StandOff, hand.z - dz * k_StandOff};
	}
	const float along = distance - k_StandOff;
	return {dx * along + hand.x, dy * along + hand.y, dz * along + hand.z};
}

bool ThrowFoodAt(ComputerHand& hand, entt::entity food, const glm::vec3& creature, const std::function<glm::vec3()>& target,
                 StageRunner& runner)
{
	if (!runner.CanBePickedUp(food))
	{
		return false;
	}
	HandStep pickUp {.type = HandSubAction::PickUpObject, .arguments = {}};
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = food});
	pickUp.arguments.push_back({.kind = HandArgument::Kind::Float, .floatValue = hand.playerSpeed});
	Append(hand, std::move(pickUp));
	Append(hand, {.type = HandSubAction::MoveHandToPos,
	              .arguments = {{.kind = HandArgument::Kind::Point, .point = FoodThrowFrom(hand.point, creature)}}});
	Append(hand, {.type = HandSubAction::ThrowObjectInHandAtPos,
	              .arguments = {{.kind = HandArgument::Kind::Point, .point = target()}}});
	return true;
}

map_coords::MapCoords HomeDropPlace(const map_coords::MapCoords& building, const map_coords::MapCoords& arrive,
                                    float buildingRadius, float villagerRadius)
{
	constexpr float k_PastTheBuilding = 1.25f;
	const float angle = game_math::Get3DAngleFromXZ(building, arrive);
	const float scaled = buildingRadius * k_PastTheBuilding;
	const float distance = scaled + villagerRadius;
	return building + game_math::GetPosFromAngle(angle, distance);
}

entt::entity ThingOfChosenLeaf(const ComputerMind& mind, uint16_t node)
{
	// each node's choice in turn, to the one that chose itself
	auto leaf = node;
	for (size_t depth = 0; depth < mind.nodes.size(); ++depth)
	{
		const auto chosen = mind.nodes.at(leaf).chosen;
		if (!chosen.has_value() || *chosen == leaf)
		{
			break;
		}
		leaf = *chosen;
	}
	return computer_player::ThingAbove(computer_player::GameTree(), mind, leaf);
}

void CastOnCreatureFromIcon(ComputerHand& hand, entt::entity icon, MagicType miracle, entt::entity creature)
{
	const HandArgument miracleArgument {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(miracle)};
	const HandArgument iconArgument {.kind = HandArgument::Kind::Thing, .thing = icon};
	Append(hand, {.type = HandSubAction::MoveHandToObject, .arguments = {iconArgument}});
	Append(hand, {.type = HandSubAction::MakeWorshippersChantForSpell, .arguments = {miracleArgument, iconArgument}});
	Append(hand, {.type = HandSubAction::CastSpellAtObject,
	              .arguments = {{.kind = HandArgument::Kind::Thing, .thing = creature}, miracleArgument, iconArgument}});
}

void HealCreatureFromIcon(ComputerHand& hand, entt::entity icon, entt::entity creature)
{
	CastOnCreatureFromIcon(hand, icon, MagicType::Heal, creature);
}

bool CastIconsMiracleAtPlace(ComputerHand& hand, entt::entity icon, MagicType miracle, float metresToIcon,
                             const map_coords::MapCoords& place, float radius, const ThingFacts& facts)
{
	const auto type = static_cast<uint32_t>(miracle);
	const auto between = [type](MagicType first, MagicType last) {
		return type >= static_cast<uint32_t>(first) && type <= static_cast<uint32_t>(last);
	};
	// fireballs and lightning are cast toward the place from inside the influence
	if (between(MagicType::Fireball, MagicType::LightningBoltPowerUpTwo))
	{
		return false;
	}
	ReachIconAndChant(hand, icon, miracle, metresToIcon);
	const HandArgument miracleArgument {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(type)};
	const HandArgument iconArgument {.kind = HandArgument::Kind::Thing, .thing = icon};
	const HandArgument placeArgument {.kind = HandArgument::Kind::Point, .point = facts.LandPoint(place)};
	const bool atObject =
	    between(MagicType::ExplosionOne, MagicType::FoodPowerUpOne) || between(MagicType::Wood, MagicType::WaterPowerUpOne);
	const bool asShield = atObject || between(MagicType::StormWindRain, MagicType::PhysicalShield) ||
	                      between(MagicType::FlockFlying, MagicType::FlockGround);
	if (atObject)
	{
		// at the point of the place's circle that faces the hand
		const map_coords::MapCoords handPlace {.x = hand.mapX, .z = hand.mapZ};
		const auto side = place + game_math::GetPosFromAngle(game_math::Get3DAngleFromXZ(place, handPlace), radius);
		Append(hand, {.type = HandSubAction::CastSpellAtPos,
		              .arguments = {
		                  {.kind = HandArgument::Kind::Point, .point = facts.LandPoint(side)}, miracleArgument, iconArgument}});
	}
	if (asShield)
	{
		// then, for a miracle cast at an object too, the same miracle again at the place as a shield with the radius
		Append(hand, {.type = HandSubAction::MoveHandToPos, .arguments = {placeArgument}});
		Append(hand,
		       {.type = HandSubAction::CastShieldAtPos,
		        .arguments = {
		            placeArgument, miracleArgument, {.kind = HandArgument::Kind::Float, .floatValue = radius}, iconArgument}});
	}
	return true;
}

HandActions::HandActions(ComputerHand& hand, StageRunner& runner, const ThingFacts& facts, computer_player::ScoreWorld* scores,
                         Missing missing, Ran ran)
    : _hand(hand)
    , _runner(runner)
    , _facts(facts)
    , _scores(scores)
    , _missing(std::move(missing))
    , _ran(std::move(ran))
{
}

float HandActions::Score(computer_player::ScoreKind kind, ComputerMind& mind, uint16_t node)
{
	// without what the rules read of the game nothing scores: a weighing that needs a score is not made (HasScore)
	if (_scores == nullptr)
	{
		return 0.0f;
	}
	return computer_player::Score(kind, computer_player::GameTree(), mind, node, *_scores);
}

bool HandActions::HasScore(computer_player::ScoreKind kind) const
{
	return _scores != nullptr && computer_player::IsScorePorted(kind);
}

bool HandActions::HasAction(computer_player::ActionKind kind) const
{
	using computer_player::ActionKind;
	return kind == ActionKind::FindPlaceToUseScaffoldForBuilding || kind == ActionKind::DirectPickupAndDropObject ||
	       kind == ActionKind::TakeVillagerFromTownToWorshipSite || kind == ActionKind::GetFoodFromOtherTownForObject ||
	       kind == ActionKind::GetWoodFromOtherTown || kind == ActionKind::GetFoodFromOtherTownForStoragePit ||
	       kind == ActionKind::ShallCastFoodSpell || kind == ActionKind::CastFoodSpellOn ||
	       kind == ActionKind::ShallCastWoodSpell || kind == ActionKind::CastFoodSpellOnWorshipSite ||
	       kind == ActionKind::RestoreMyCreatureHealth || kind == ActionKind::ChargeUpSpell ||
	       kind == ActionKind::CreateDiscipleToFulfillTownDesire || kind == ActionKind::CastHealSpellAtWorshipSite ||
	       kind == ActionKind::DefendtownWithSpells || kind == ActionKind::CastSpellToDefendAgainstBelief ||
	       kind == ActionKind::CastPhysicalShieldToProtect || kind == ActionKind::CastSpellToImpress ||
	       kind == ActionKind::ThrowThingsToImpress || kind == ActionKind::ThrowrockatCreature ||
	       kind == ActionKind::DestroytownWithSpell || kind == ActionKind::AttackCreatureWithSpells ||
	       kind == ActionKind::CombineScaffolds || kind == ActionKind::CastFoodForMyCreature ||
	       kind == ActionKind::GiveBuildingSiteWorkers || kind == ActionKind::TownWithHighestFoodDesire ||
	       kind == ActionKind::GiveBuildingSiteWood || kind == ActionKind::FillWorkshop ||
	       kind == ActionKind::GiveCreatureFood || kind == ActionKind::GiveWorshippersFood ||
	       kind == ActionKind::ReacttoCreatureStarvingEmergency || kind == ActionKind::TakeVillagerFromWorshipSiteToTown ||
	       kind == ActionKind::ThrowFoodatCreature || kind == ActionKind::UseCreatureAsDiverseionAgainstPlayer ||
	       kind == ActionKind::AttackCreatureWithCreature || kind == ActionKind::DestroytownWithCreature ||
	       kind == ActionKind::UseCreatureToImpressTown || kind == ActionKind::TeachCreatureAggressiveMagic ||
	       kind == ActionKind::TeachCreatureCompassionateMagic || kind == ActionKind::TeachCreatureImpressiveMagic ||
	       kind == ActionKind::TeachCreatureNormalAction || kind == ActionKind::SendCreatureHome ||
	       kind == ActionKind::GetCreaturetoHelp || kind == ActionKind::DefendTownWithCreature ||
	       kind == ActionKind::GetCreaturetoHelpBuild;
}

void HandActions::GiveUpSteps()
{
	Clear(_hand, _runner);
}

bool HandActions::RunAction(computer_player::ActionKind kind, ComputerMind& mind, uint16_t node)
{
	using computer_player::ActionKind;
	// both act on the thing of the node or of the nearest node above it that has one
	const auto thing = computer_player::ThingAbove(computer_player::GameTree(), mind, node);
	if (kind == ActionKind::DirectPickupAndDropObject)
	{
		const auto marker = _facts.MarkerPoint(mind.nodes.at(node).secondThing);
		return _facts.IsObject(thing) && marker.has_value() && PickUpAndDrop(_hand, thing, *marker, _runner);
	}
	if (kind == ActionKind::FindPlaceToUseScaffoldForBuilding)
	{
		// the place is the point of the node it chose
		const auto chosen = computer_player::ChosenBelow(mind, node);
		return _facts.HasPlace(thing) && chosen.has_value() &&
		       PickUpAndDrop(_hand, thing, mind.nodes.at(*chosen).point, _runner);
	}
	if (kind == ActionKind::TakeVillagerFromTownToWorshipSite)
	{
		// the villager is the node's, the worship site the nearest above its parent's
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto site = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		if (!_facts.IsVillager(thing) || !_facts.IsWorshipSite(site))
		{
			return false;
		}
		TakeToGate(_hand, thing, _facts.GatePoint(site), _runner);
		return true;
	}
	if (kind == ActionKind::GetFoodFromOtherTownForObject)
	{
		// the pile is the node's, the object the nearest above its parent's, and the amount its parent's number
		const auto parent = computer_player::GameTree().nodes[node].parent;
		if (!parent.has_value())
		{
			return false;
		}
		const auto object = computer_player::ThingAbove(computer_player::GameTree(), mind, *parent);
		if (!_facts.IsObject(thing) || !_facts.IsObject(object))
		{
			return false;
		}
		FetchFoodFor(_hand, thing, _facts.MapPoint(thing), mind.nodes.at(*parent).intValue, object);
		return true;
	}
	if (kind == ActionKind::GetWoodFromOtherTown)
	{
		// the pile is the node's; the wood goes to the thing above its parent, or to a building site's building
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto above = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                      : entt::entity {entt::null};
		const auto to = _facts.HasPlace(above) ? above : _facts.SiteBuilding(above);
		if (!_facts.HasPlace(thing) || to == entt::null)
		{
			return false;
		}
		const auto wood = _facts.ResourceOf(thing, ResourceType::Wood);
		if (wood == 0)
		{
			return false;
		}
		FetchWoodFor(_hand, thing, wood, to);
		return true;
	}
	if (kind == ActionKind::GetFoodFromOtherTownForStoragePit)
	{
		// the pile is the node's, the town the nearest above its parent's; the food goes where the town takes the
		// pile's resource, and the amount is the node's own, cut toward zero
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto town = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		if (!_facts.IsObject(thing) || !_facts.IsTown(town))
		{
			return false;
		}
		const auto drop = _facts.ResourceDropObject(town, _facts.ResourceTypeOf(thing));
		if (drop == entt::null)
		{
			return false;
		}
		FetchFoodFor(_hand, thing, _facts.MapPoint(thing), static_cast<int32_t>(mind.nodes.at(node).floatValue), drop);
		return true;
	}
	if (kind == ActionKind::ShallCastFoodSpell)
	{
		// the icon is the node's, the town the nearest above its parent's; the miracle goes where the town takes wood
		// for the wood miracle, food for any other
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto town = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		const auto miracle = _facts.IconMagicType(thing);
		if (!miracle.has_value() || !_facts.IsTown(town))
		{
			return false;
		}
		const auto store =
		    _facts.ResourceDropObject(town, *miracle == MagicType::Wood ? ResourceType::Wood : ResourceType::Food);
		if (store == entt::null)
		{
			return false;
		}
		const auto metresToIcon =
		    game_math::GetDistanceInMetres(map_coords::MapCoords {.x = _hand.mapX, .z = _hand.mapZ}, _facts.MapPlace(thing));
		return CastIconsMiracleAt(_hand, thing, *miracle, metresToIcon, store);
	}
	if (kind == ActionKind::CastFoodSpellOn)
	{
		// the icon is the node's, the object to cast on the nearest above its parent's: the creature to feed, the
		// workshop to fill or the burning building
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto object = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                       : entt::entity {entt::null};
		const auto miracle = _facts.IconMagicType(thing);
		if (!miracle.has_value() || !_facts.IsObject(object))
		{
			return false;
		}
		const auto metresToIcon =
		    game_math::GetDistanceInMetres(map_coords::MapCoords {.x = _hand.mapX, .z = _hand.mapZ}, _facts.MapPlace(thing));
		return CastIconsMiracleOnAndStay(_hand, thing, *miracle, metresToIcon, object);
	}
	if (kind == ActionKind::ShallCastWoodSpell)
	{
		// the icon is the node's, the building site the nearest above its parent's; the miracle goes at the place where
		// the site's building takes resources from a god, its own place
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto site = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		const auto miracle = _facts.IconMagicType(thing);
		const auto building = _facts.SiteBuilding(site);
		if (!miracle.has_value() || building == entt::null)
		{
			return false;
		}
		const auto metresToIcon =
		    game_math::GetDistanceInMetres(map_coords::MapCoords {.x = _hand.mapX, .z = _hand.mapZ}, _facts.MapPlace(thing));
		return CastIconsMiracleAtPlace(_hand, thing, *miracle, metresToIcon, _facts.MapPlace(building), 0.0f, _facts);
	}
	if (kind == ActionKind::CastFoodSpellOnWorshipSite)
	{
		// the icon is the node's, the worship site the nearest above its parent's; the miracle goes where the site
		// keeps its food
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto site = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		const auto miracle = _facts.IconMagicType(thing);
		const auto food = _facts.WorshipFoodPlace(site);
		if (!miracle.has_value() || !food.has_value())
		{
			return false;
		}
		const auto metresToIcon =
		    game_math::GetDistanceInMetres(map_coords::MapCoords {.x = _hand.mapX, .z = _hand.mapZ}, _facts.MapPlace(thing));
		return CastIconsMiracleAtPlace(_hand, thing, *miracle, metresToIcon, *food, 0.0f, _facts);
	}
	if (kind == ActionKind::CastFoodForMyCreature)
	{
		// the icon is the node's, the creature the nearest above its parent's
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto creature = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                         : entt::entity {entt::null};
		if (!_facts.IsCreature(creature) || !_facts.IconMagicType(thing).has_value())
		{
			return false;
		}
		CastOnCreatureFromIcon(_hand, thing, MagicType::Food, creature);
		return true;
	}
	if (kind == ActionKind::RestoreMyCreatureHealth)
	{
		// the icon is the node's, the creature the nearest above its parent's
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto creature = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                         : entt::entity {entt::null};
		if (!_facts.IsCreature(creature) || !_facts.IconMagicType(thing).has_value())
		{
			return false;
		}
		HealCreatureFromIcon(_hand, thing, creature);
		return true;
	}
	if (kind == ActionKind::ChargeUpSpell)
	{
		// the icon is the node's
		const auto miracle = _facts.IconMagicType(thing);
		if (!miracle.has_value())
		{
			return false;
		}
		const auto metresToIcon =
		    game_math::GetDistanceInMetres(map_coords::MapCoords {.x = _hand.mapX, .z = _hand.mapZ}, _facts.MapPlace(thing));
		ChargeIconsMiracle(_hand, thing, *miracle, metresToIcon);
		return true;
	}
	if (kind == ActionKind::CreateDiscipleToFulfillTownDesire)
	{
		// the villager is the node's, the town the nearest above its parent's, and the kind of disciple the node's number
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto town = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		if (!_facts.IsVillager(thing) || !_facts.IsTown(town))
		{
			return false;
		}
		// it becomes a disciple first, whether or not the hand may then take it
		_runner.MakeDisciple(thing, mind.nodes.at(node).intValue);
		return PickUpAndDrop(_hand, thing, _facts.MapPoint(thing), _runner);
	}
	if (kind == ActionKind::CastHealSpellAtWorshipSite)
	{
		// the icon is the node's, the worship site the nearest above its parent's; the miracle goes at the place of the
		// site's most hurt worshipper
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto site = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		const auto miracle = _facts.IconMagicType(thing);
		if (!miracle.has_value() || !_facts.IsWorshipSite(site))
		{
			return false;
		}
		const auto worshipper = MostHurtWorshipper(_facts.SiteWorshippers(site));
		if (worshipper == entt::null)
		{
			return false;
		}
		const auto metresToIcon =
		    game_math::GetDistanceInMetres(map_coords::MapCoords {.x = _hand.mapX, .z = _hand.mapZ}, _facts.MapPlace(thing));
		return CastIconsMiracleAtPlace(_hand, thing, *miracle, metresToIcon, _facts.MapPlace(worshipper), 0.0f, _facts);
	}
	if (kind == ActionKind::DefendtownWithSpells || kind == ActionKind::CastSpellToDefendAgainstBelief ||
	    kind == ActionKind::CastPhysicalShieldToProtect)
	{
		return RunShield(kind, mind, node, thing);
	}
	if (kind == ActionKind::ThrowThingsToImpress)
	{
		// the thing is the node's, the town the nearest above its parent's, and the point the node's own
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto town = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		if (thing == entt::null || !_facts.IsTown(town))
		{
			return false;
		}
		const auto point = mind.nodes.at(node).point;
		const float metres = game_math::GetDistanceInMetres(map_coords::FromWorld(point),
		                                                    map_coords::MapCoords {.x = _hand.mapX, .z = _hand.mapZ});
		if (!(metres < k_ImpressThrowReach) || !_facts.IsObject(thing))
		{
			return false;
		}
		return PickUpAndThrow(_hand, thing, point, _runner);
	}
	if (kind == ActionKind::AttackCreatureWithCreature)
	{
		// its creature is the node's, the other creature the nearest above its parent's: its creature is put on the
		// angry leash and the leash is tied to the other
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto other = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                      : entt::entity {entt::null};
		if (!_facts.IsCreature(other) || !_facts.IsCreature(thing))
		{
			return false;
		}
		LeashToThing(_hand, thing, other);
		return true;
	}
	if (kind == ActionKind::DestroytownWithCreature)
	{
		// the thing with a place nearest above its parent gives the town (a town is its own), the creature is the
		// nearest above the node; the creature is put on the angry leash tied to the town's store, else its centre, else
		// its newest building
		if (_scores == nullptr)
		{
			return false;
		}
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto object = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                       : entt::entity {entt::null};
		if (object == entt::null || !_scores->HasPlace(object))
		{
			return false;
		}
		const auto town = _scores->IsTown(object) ? object : _scores->TownOf(object);
		if (town == entt::null)
		{
			return false;
		}
		const auto target = computer_player::CreatureDestroyTarget(*_scores, town);
		if (!_facts.IsCreature(thing) || target == entt::null)
		{
			return false;
		}
		// a creature that knows none of the angry miracles it could be taught has one more lesson of them owed
		if (_scores->MiracleKnowledge(thing, CreatureDesires::Anger) < 1.0f)
		{
			++mind.destroyLessons;
		}
		LeashToThing(_hand, thing, target);
		return true;
	}
	if (kind == ActionKind::TeachCreatureAggressiveMagic || kind == ActionKind::TeachCreatureCompassionateMagic ||
	    kind == ActionKind::TeachCreatureImpressiveMagic)
	{
		// the creature is the nearest above its parent, the icon the nearest above the node
		if (_scores == nullptr)
		{
			return false;
		}
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto creature = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                         : entt::entity {entt::null};
		if (!_facts.IsCreature(creature) || !_facts.IconMagicType(thing).has_value())
		{
			return false;
		}
		const auto desire = kind == ActionKind::TeachCreatureAggressiveMagic      ? CreatureDesires::Anger
		                    : kind == ActionKind::TeachCreatureCompassionateMagic ? CreatureDesires::Compassion
		                                                                          : CreatureDesires::ToImpress;
		const auto miracle = _scores->MiracleLeftToLearn(creature, desire);
		if (!miracle.has_value())
		{
			return false;
		}
		// the lessons owed of the kind are given
		auto& owed = kind == ActionKind::TeachCreatureAggressiveMagic      ? mind.destroyLessons
		             : kind == ActionKind::TeachCreatureCompassionateMagic ? mind.helpLessons
		                                                                   : mind.impressLessons;
		owed = 0;
		Append(_hand, StepToPoint(HandSubAction::MoveHandToPos, _facts.MapPoint(creature)));
		_runner.TeachCreature(creature, static_cast<uint32_t>(CreatureActionLearningType::Magic), *miracle);
		TeachMiracle(_hand, creature, thing, static_cast<MagicType>(*miracle));
		return true;
	}
	if (kind == ActionKind::SendCreatureHome)
	{
		// the creature is the nearest above the node
		if (_scores == nullptr || !_facts.IsCreature(thing))
		{
			return false;
		}
		const auto home = _scores->CreatureHome(thing);
		if (!home.has_value())
		{
			return false;
		}
		// the action the creature is ordered to do, going home, by its row of the game's table
		constexpr int32_t k_GoHomeAction = 162;
		const HandArgument creatureArgument {.kind = HandArgument::Kind::Thing, .thing = thing};
		SpeedForJourney(_hand, thing, _facts.LandPoint(*home));
		Append(_hand, StepToPoint(HandSubAction::MoveHandToPos, _facts.MapPoint(thing)));
		Append(_hand, {.type = HandSubAction::ForceCreatureToPerformAction,
		               .arguments = {
		                   creatureArgument, {.kind = HandArgument::Kind::Int, .intValue = k_GoHomeAction}, creatureArgument}});
		return true;
	}
	if (kind == ActionKind::GetCreaturetoHelp || kind == ActionKind::DefendTownWithCreature)
	{
		// defending a town with the creature runs the same as getting it to help: the town of the thing nearest above
		// its parent, the creature the nearest above the node
		if (_scores == nullptr)
		{
			return false;
		}
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto object = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                       : entt::entity {entt::null};
		const auto town =
		    object != entt::null && _scores->HasPlace(object) ? _scores->TownOf(object) : entt::entity {entt::null};
		if (town == entt::null || !_scores->IsTown(town))
		{
			return false;
		}
		auto target = _scores->StoragePit(town);
		if (target == entt::null)
		{
			const auto buildings = _scores->TownAbodes(town);
			target = buildings.empty() ? entt::entity {entt::null} : buildings.front();
		}
		if (!_facts.IsCreature(thing) || target == entt::null)
		{
			return false;
		}
		// a creature that knows none of the compassionate miracles it could be taught has one more lesson owed
		if (_scores->MiracleKnowledge(thing, CreatureDesires::Compassion) < 1.0f)
		{
			++mind.helpLessons;
		}
		SpeedForJourney(_hand, thing, _facts.MapPoint(target));
		LeashToThing(_hand, thing, target, LeashType::Good);
		return true;
	}
	if (kind == ActionKind::GetCreaturetoHelpBuild)
	{
		// the building site is the nearest above its parent, the creature the nearest above the node
		if (_scores == nullptr)
		{
			return false;
		}
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto site = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		const auto building =
		    site != entt::null && _scores->IsBuildingSite(site) ? _facts.SiteBuilding(site) : entt::entity {entt::null};
		if (building == entt::null || !_facts.IsCreature(thing))
		{
			return false;
		}
		if (_scores->MiracleKnowledge(thing, CreatureDesires::Compassion) < 1.0f)
		{
			++mind.helpLessons;
		}
		// the action the creature is ordered to do at the building, by its row of the game's table
		constexpr int32_t k_HelpBuildAction = 34;
		constexpr float k_WaitSeconds = 0.5f;
		constexpr float k_CompassionSeconds = 120.0f;
		const HandArgument creatureArgument {.kind = HandArgument::Kind::Thing, .thing = thing};
		SpeedForJourney(_hand, thing, _facts.MapPoint(building));
		Append(_hand, {.type = HandSubAction::ForceCreatureToPerformAction,
		               .arguments = {creatureArgument,
		                             {.kind = HandArgument::Kind::Int, .intValue = k_HelpBuildAction},
		                             {.kind = HandArgument::Kind::Thing, .thing = building}}});
		Append(_hand,
		       {.type = HandSubAction::Wait, .arguments = {{.kind = HandArgument::Kind::Float, .floatValue = k_WaitSeconds}}});
		Append(_hand,
		       {.type = HandSubAction::ForceCreatureDominantDesire,
		        .arguments = {creatureArgument,
		                      {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(CreatureDesires::Compassion)},
		                      {.kind = HandArgument::Kind::Float, .floatValue = k_CompassionSeconds}}});
		return true;
	}
	if (kind == ActionKind::TeachCreatureNormalAction)
	{
		// the creature is the nearest above the node
		if (_scores == nullptr || !_facts.IsCreature(thing))
		{
			return false;
		}
		const auto action = _scores->NormalActionLeftToLearn(thing);
		if (!action.has_value())
		{
			return false;
		}
		// the action the creature is ordered to do as its lesson starts, by its row of the game's table
		constexpr int32_t k_LessonAction = 82;
		constexpr float k_TotemReach = 250.0f;
		const HandArgument creatureArgument {.kind = HandArgument::Kind::Thing, .thing = thing};
		Append(_hand, StepToPoint(HandSubAction::MoveHandToPos, _facts.MapPoint(thing)));
		Append(_hand, {.type = HandSubAction::ForceCreatureToPerformAction,
		               .arguments = {
		                   creatureArgument, {.kind = HandArgument::Kind::Int, .intValue = k_LessonAction}, creatureArgument}});
		_runner.TeachCreature(thing, static_cast<uint32_t>(CreatureActionLearningType::Normal), *action);
		// the totem is looked for only for the lesson that needs one
		constexpr uint32_t k_TotemLesson = 2;
		const auto totem = *action == k_TotemLesson
		                       ? _scores->NearestOfType(_scores->Place(thing), ObjectType::TotemStatue, k_TotemReach)
		                       : entt::entity {entt::null};
		return TeachNormalAction(_hand, thing, *action, totem);
	}
	if (kind == ActionKind::UseCreatureToImpressTown)
	{
		// as destroying a town with it: the town from the thing with a place nearest above its parent, the creature the
		// nearest above the node; it goes on the learning leash tied to the town's store, else its newest building, and
		// its desire to impress is made dominant for two minutes
		if (_scores == nullptr)
		{
			return false;
		}
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto object = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                       : entt::entity {entt::null};
		if (object == entt::null || !_scores->HasPlace(object))
		{
			return false;
		}
		const auto town = _scores->IsTown(object) ? object : _scores->TownOf(object);
		if (town == entt::null)
		{
			return false;
		}
		auto target = _scores->StoragePit(town);
		if (target == entt::null)
		{
			const auto buildings = _scores->TownAbodes(town);
			target = buildings.empty() ? entt::entity {entt::null} : buildings.front();
		}
		if (!_facts.IsCreature(thing) || target == entt::null)
		{
			return false;
		}
		constexpr float k_ImpressSeconds = 120.0f;
		LeashToThing(_hand, thing, target, LeashType::Rope);
		Append(_hand,
		       {.type = HandSubAction::ForceCreatureDominantDesire,
		        .arguments = {{.kind = HandArgument::Kind::Thing, .thing = thing},
		                      {.kind = HandArgument::Kind::Int, .intValue = static_cast<int32_t>(CreatureDesires::ToImpress)},
		                      {.kind = HandArgument::Kind::Float, .floatValue = k_ImpressSeconds}}});
		return true;
	}
	if (kind == ActionKind::UseCreatureAsDiverseionAgainstPlayer)
	{
		// using the creature as a diversion does nothing: the action fails at once, with no steps
		return false;
	}
	if (kind == ActionKind::ThrowFoodatCreature)
	{
		// the food is the chosen leaf's, the creature the node's; the food is thrown at a point drawn round the
		// creature, at the height of its size
		if (_scores == nullptr)
		{
			return false;
		}
		const auto food = ThingOfChosenLeaf(mind, node);
		if (!_facts.HasPlace(food) || !_facts.IsCreature(thing))
		{
			return false;
		}
		const auto creature = _facts.MapPoint(thing);
		const auto target = [this, &creature, thing]() {
			constexpr float k_Spread = 4.0f;
			constexpr float k_HalfSpread = 2.0f;
			glm::vec3 at = creature;
			at.y = computer_player::scores::CreatureSize(*_scores, thing);
			at.x = (_scores->GameFloatRand(k_Spread) - k_HalfSpread) + at.x;
			at.z = (_scores->GameFloatRand(k_Spread) - k_HalfSpread) + at.z;
			return at;
		};
		return ThrowFoodAt(_hand, food, creature, target, _runner);
	}
	if (kind == ActionKind::ThrowrockatCreature)
	{
		// the thing is the node's, the creature the nearest above its parent's, and the thing goes at its place
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto creature = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                         : entt::entity {entt::null};
		if (!_facts.HasPlace(thing) || !_facts.IsCreature(creature))
		{
			return false;
		}
		return PickUpAndThrow(_hand, thing, _facts.MapPoint(creature), _runner);
	}
	if (kind == ActionKind::DestroytownWithSpell)
	{
		// the icon is the node's, the town or object the nearest above its parent's; a town's building most worth
		// attacking from the hand is the one aimed at
		if (_scores == nullptr)
		{
			return false;
		}
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto above = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                      : entt::entity {entt::null};
		const auto miracle = _facts.IconMagicType(thing);
		if (above == entt::null || !miracle.has_value())
		{
			return false;
		}
		const map_coords::MapCoords handPlace {.x = _hand.mapX, .z = _hand.mapZ};
		const auto target =
		    _facts.IsTown(above) ? computer_player::scores::BestAttackObject(*_scores, above, handPlace) : above;
		if (target == entt::null || (!_facts.IsTown(above) && !_facts.IsObject(above)))
		{
			return false;
		}
		const auto edge = _facts.NearestEdgeTo(target, handPlace);
		const auto inside = computer_player::NearestPlaceInInfluence(*_scores, edge);
		const TowardPlaces toward {.place = _facts.LandPoint(_facts.MapPlace(target)),
		                           .edge = _facts.LandPoint(edge),
		                           .inside = inside.has_value() ? std::optional(_facts.LandPoint(*inside)) : std::nullopt};
		const auto metresToIcon = game_math::GetDistanceInMetres(handPlace, _facts.MapPlace(thing));
		return CastIconsMiracleAtOrToward(_hand, thing, *miracle, metresToIcon, target, toward);
	}
	if (kind == ActionKind::AttackCreatureWithSpells)
	{
		// the icon is the node's, the creature the nearest above its parent's; two or three rounds, drawn afresh after
		// each
		if (_scores == nullptr)
		{
			return false;
		}
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto creature = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                         : entt::entity {entt::null};
		const auto miracle = _facts.IconMagicType(thing);
		if (!miracle.has_value() || !_facts.IsCreature(creature))
		{
			return false;
		}
		constexpr uint32_t k_LeastRounds = 2;
		uint32_t rounds = _scores->GameRand(2) + k_LeastRounds;
		for (uint32_t done = 0; done < rounds; rounds = _scores->GameRand(2) + k_LeastRounds)
		{
			AttackCreatureWithIcon(_hand, thing, *miracle, creature);
			++done;
		}
		return true;
	}
	if (kind == ActionKind::CombineScaffolds)
	{
		// the scaffold is the node's, the one it is combined with the chosen leaf's
		const auto other = ThingOfChosenLeaf(mind, node);
		if (!_facts.IsScaffold(thing) || !_facts.IsScaffold(other) || !_runner.CanBePickedUp(thing))
		{
			return false;
		}
		PickUpAndGive(_hand, thing, other);
		return true;
	}
	if (kind == ActionKind::GiveBuildingSiteWorkers)
	{
		// the villager is the chosen leaf's, the building site the node's; the villager goes by the site's building,
		// at a place round it
		if (_scores == nullptr)
		{
			return false;
		}
		const auto villager = ThingOfChosenLeaf(mind, node);
		const auto building = _facts.SiteBuilding(thing);
		if (!_facts.HasPlace(villager) || !_facts.IsAvailableBuildingSite(thing) || building == entt::null)
		{
			return false;
		}
		const auto woodThere = [this](const map_coords::MapCoords& place) {
			if (!map_coords::InBounds(place))
			{
				return false;
			}
			const auto cell = map_coords::Cell(place);
			const auto things = _scores->CellObjects(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
			return std::ranges::any_of(things,
			                           [this](entt::entity each) { return _facts.ResourceOf(each, ResourceType::Wood) != 0; });
		};
		const auto place = WorkerDropPlace(_facts.MapPlace(building), _scores->ThingRadius(building), woodThere);
		return PickUpAndDrop(_hand, villager, _facts.LandPoint(place), _runner);
	}
	if (kind == ActionKind::TownWithHighestFoodDesire)
	{
		// the object is the chosen leaf's (a field, a fish farm or a tree), the town the node's; it goes where the town
		// takes the object's resource
		const auto object = ThingOfChosenLeaf(mind, node);
		if (!_facts.IsObject(object) || !_facts.IsTown(thing))
		{
			return false;
		}
		const auto drop = _facts.ResourceDropObject(thing, _facts.ResourceTypeOf(object));
		if (drop == entt::null || !_runner.CanBePickedUp(object))
		{
			return false;
		}
		PickUpAndGive(_hand, object, drop);
		return true;
	}
	if (kind == ActionKind::GiveBuildingSiteWood)
	{
		// the wood is the chosen leaf's, the building site the node's; it is dropped at the site's building's place
		const auto wood = ThingOfChosenLeaf(mind, node);
		const auto building = _facts.SiteBuilding(thing);
		if (!_facts.HasPlace(wood) || !_facts.IsAvailableBuildingSite(thing) || building == entt::null)
		{
			return false;
		}
		return PickUpAndDrop(_hand, wood, _facts.LandPoint(_facts.MapPlace(building)), _runner);
	}
	if (kind == ActionKind::FillWorkshop)
	{
		// the wood is the chosen leaf's, the workshop the node's; a thing the hand may not take gives nothing more
		const auto wood = ThingOfChosenLeaf(mind, node);
		if (!_facts.HasPlace(wood) || !_facts.HasPlace(thing))
		{
			return false;
		}
		if (_runner.CanBePickedUp(wood))
		{
			PickUpAndGive(_hand, wood, thing);
		}
		return true;
	}
	if (kind == ActionKind::GiveCreatureFood || kind == ActionKind::GiveWorshippersFood ||
	    kind == ActionKind::ReacttoCreatureStarvingEmergency)
	{
		// the food is the chosen leaf's (a field or a fish farm), the creature or the worship site the node's
		const auto food = ThingOfChosenLeaf(mind, node);
		const bool to = kind == ActionKind::GiveCreatureFood      ? _facts.HasPlace(thing)
		                : kind == ActionKind::GiveWorshippersFood ? _facts.IsWorshipSite(thing)
		                                                          : _facts.IsCreature(thing);
		if (!_facts.HasPlace(food) || !to || !_runner.CanBePickedUp(food))
		{
			return false;
		}
		if (kind == ActionKind::ReacttoCreatureStarvingEmergency)
		{
			HandStep pickUp {.type = HandSubAction::PickUpObject, .arguments = {}};
			pickUp.arguments.push_back({.kind = HandArgument::Kind::Thing, .thing = food});
			pickUp.arguments.push_back({.kind = HandArgument::Kind::Float, .floatValue = _hand.playerSpeed});
			Append(_hand, std::move(pickUp));
			Append(_hand, {.type = HandSubAction::GiveObjectInHandToCreature,
			               .arguments = {{.kind = HandArgument::Kind::Thing, .thing = thing}}});
			return true;
		}
		PickUpAndGive(_hand, food, thing);
		return true;
	}
	if (kind == ActionKind::TakeVillagerFromWorshipSiteToTown)
	{
		// the villager is the node's, the worship site the nearest above its parent's; it goes home to its town
		if (_scores == nullptr)
		{
			return false;
		}
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto site = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		if (!_facts.IsVillager(thing) || !_facts.IsWorshipSite(site))
		{
			return false;
		}
		const auto home = _facts.HomeOf(thing);
		if (home.town == entt::null)
		{
			return false;
		}
		const auto building = home.abode != entt::null ? home.abode : _scores->TownCentre(home.town);
		const auto place = building != entt::null ? HomeDropPlace(_facts.MapPlace(building), _facts.ArrivePlace(building),
		                                                          _scores->ThingRadius(building), _scores->ThingRadius(thing))
		                                          : _facts.MapPlace(home.town);
		return PickUpAndDrop(_hand, thing, _facts.LandPoint(place), _runner);
	}
	if (kind == ActionKind::CastSpellToImpress)
	{
		// the icon is the node's, the town the nearest above its parent's; the miracle goes at the node's point
		const auto parent = computer_player::GameTree().nodes[node].parent;
		const auto town = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
		                                     : entt::entity {entt::null};
		const auto miracle = _facts.IconMagicType(thing);
		if (!_facts.IsTown(town) || !miracle.has_value())
		{
			return false;
		}
		ImpressWithIconsMiracle(_hand, thing, *miracle, mind.nodes.at(node).point);
		return true;
	}
	return false;
}

std::optional<ShieldCast> ShieldFor(computer_player::ActionKind kind, const computer_player::ScoreWorld& world,
                                    const ComputerMind& mind, entt::entity above, MagicType miracle,
                                    const map_coords::MapCoords& objectPlace)
{
	using computer_player::ActionKind;
	// the physical shield reaches a little past the object
	constexpr float k_PastTheObject = 1.2f;
	if (kind == ActionKind::CastPhysicalShieldToProtect)
	{
		if (computer_player::CastNear(world, mind, objectPlace, miracle, 0.0f))
		{
			return std::nullopt;
		}
		return ShieldCast {.place = objectPlace, .radius = world.ThingRadius(above) * k_PastTheObject};
	}
	if (!world.IsTown(above))
	{
		return std::nullopt;
	}
	if (kind == ActionKind::CastSpellToDefendAgainstBelief &&
	    computer_player::BeliefAttackDefendObject(world, above) == entt::null)
	{
		return std::nullopt;
	}
	const auto centre = world.TownAreaCentre(above);
	// a miracle the mind cast there already is not cast again, but against a belief attack
	if (kind == ActionKind::DefendtownWithSpells && computer_player::CastNear(world, mind, centre, miracle, 0.0f))
	{
		return std::nullopt;
	}
	return ShieldCast {.place = centre, .radius = computer_player::TownRadius(world.TownArea(above))};
}

bool HandActions::RunShield(computer_player::ActionKind kind, ComputerMind& mind, uint16_t node, entt::entity icon)
{
	using computer_player::ActionKind;
	// the shields read the town's area and the casts the mind made, as the score rules read them
	if (_scores == nullptr)
	{
		return false;
	}
	// the icon is the node's, the town or the object the nearest above its parent's
	const auto parent = computer_player::GameTree().nodes[node].parent;
	const auto above = parent.has_value() ? computer_player::ThingAbove(computer_player::GameTree(), mind, *parent)
	                                      : entt::entity {entt::null};
	const auto miracle = _facts.IconMagicType(icon);
	if (!miracle.has_value() || (kind == ActionKind::CastPhysicalShieldToProtect && !_facts.IsObject(above)))
	{
		return false;
	}
	const auto shield = ShieldFor(kind, *_scores, mind, above, *miracle, _facts.MapPlace(above));
	if (!shield.has_value())
	{
		return false;
	}
	const auto metresToIcon =
	    game_math::GetDistanceInMetres(map_coords::MapCoords {.x = _hand.mapX, .z = _hand.mapZ}, _facts.MapPlace(icon));
	const bool cast = CastIconsMiracleAtPlace(_hand, icon, *miracle, metresToIcon, shield->place, shield->radius, _facts);
	// against a belief attack the action is done whatever the cast gave
	return kind == ActionKind::CastSpellToDefendAgainstBelief || cast;
}

void HandActions::TimeSteps()
{
	// the original's estimate of the steps' time is kept only by its saved games
}

void HandActions::ActionMissing(computer_player::ActionKind kind, uint16_t node)
{
	if (_missing)
	{
		_missing(kind, node);
	}
}

void HandActions::ActionRan(computer_player::ActionKind kind, uint16_t node, bool done)
{
	if (_ran)
	{
		_ran(kind, node, done);
	}
}

entt::entity MostHurtWorshipper(std::span<const ThingFacts::Worshipper> worshippers)
{
	// the least life so far starts at full; a tie keeps the first
	float least = 1.0f;
	entt::entity chosen = entt::null;
	for (const auto& each : worshippers)
	{
		if (each.hiding)
		{
			continue;
		}
		if (each.life < least)
		{
			least = each.life;
			chosen = each.villager;
		}
	}
	return chosen;
}

bool ForceAction(ComputerHand& hand, ComputerMind& mind, std::string_view name, entt::entity first, entt::entity second,
                 uint32_t turn, StageRunner& runner, const ThingFacts& facts, const computer_player::MindRowNames& names)
{
	HandActions actions(hand, runner, facts);
	return ForceAction(hand, mind, name, first, second, turn, facts, actions, names);
}

bool ForceAction(ComputerHand& hand, ComputerMind& mind, std::string_view name, entt::entity first, entt::entity second,
                 uint32_t turn, const ThingFacts& facts, computer_player::MindWorld& world,
                 const computer_player::MindRowNames& names)
{
	const computer_player::ForcedOn on {
	    .first = first,
	    .second = second,
	    .secondWithPlace = facts.HasPlace(second) ? second : entt::null,
	    .marker = facts.MarkerPoint(second),
	};
	if (!computer_player::Force(mind, name, on, turn, world, names))
	{
		return false;
	}
	hand.busy = true;
	return true;
}

void QueueAction(ComputerHand& hand, std::string name, entt::entity first, entt::entity second)
{
	hand.queue.push_back({.name = std::move(name), .first = first, .second = second});
}

void SweepQueue(ComputerHand& hand, const StageRunner& runner)
{
	const auto gone = [&runner](entt::entity thing) { return thing != entt::null && runner.IsGone(thing); };
	std::erase_if(hand.queue, [&gone](const ecs::components::QueuedHandAction& queued) {
		return gone(queued.first) || gone(queued.second);
	});
}

std::optional<ecs::components::QueuedHandAction> TakeNext(ComputerHand& hand)
{
	if (hand.queue.empty())
	{
		return std::nullopt;
	}
	auto next = std::move(hand.queue.front());
	hand.queue.pop_front();
	return next;
}

void NewTurnsOfTheHand(HeldDraw& draw, const glm::vec3& turnStartPoint, uint32_t turns, bool smooth)
{
	if (turns == 0)
	{
		return;
	}
	// Each turn keeps the time drawn since the turn before and starts again; a turn with no frame in between kept none
	const auto before = draw.hand;
	draw.hand = turnStartPoint;
	draw.durationMs = turns == 1 ? draw.elapsedMs : 0;
	draw.elapsedMs = 0;
	draw.start = smooth ? before : draw.hand;
}

glm::vec3 SlideOfHeld(const glm::vec3& start, const glm::vec3& hand, int32_t elapsedMs, int32_t durationMs)
{
	if (durationMs == 0)
	{
		return hand;
	}
	const float share = static_cast<float>(std::min(elapsedMs, durationMs)) / static_cast<float>(durationMs);
	const glm::vec3 way = hand - start;
	return {way.x * share + start.x, way.y * share + start.y, way.z * share + start.z};
}

glm::vec3 HeldDrawPoint(HeldDraw& draw, uint32_t frameGameMs)
{
	draw.elapsedMs += static_cast<int32_t>(frameGameMs);
	return SlideOfHeld(draw.start, draw.hand, draw.elapsedMs, draw.durationMs);
}

glm::vec3 OnTheMapGrid(const glm::vec3& point, const Ground& ground)
{
	const auto mapX = map_coords::ToFixed(point.x);
	const auto mapZ = map_coords::ToFixed(point.z);
	const float aboveGround = point.y - ground.AltitudeAt(mapX, mapZ);
	return {map_coords::ToMetres(mapX), ground.AltitudeAt(mapX, mapZ) + aboveGround, map_coords::ToMetres(mapZ)};
}

} // namespace openblack::computer_hand
