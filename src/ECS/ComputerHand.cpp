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
#include <string_view>
#include <utility>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "ECS/ComputerPlayerScores.h"

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
	if (!hand.steps.empty())
	{
		auto& first = hand.steps.front();
		const auto move = hand.checkPrerequisite ? Prerequisite(first.type) : None;
		if (move != None)
		{
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
	const float distance = gutils::GetDistance(hand.point, hand.target);
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

StageResult MoveStraight(ComputerHand& hand)
{
	const float distance = gutils::GetDistance(hand.point, hand.target);
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

HandActions::HandActions(ComputerHand& hand, StageRunner& runner, const ThingFacts& facts, computer_player::ScoreWorld* scores,
                         Missing missing)
    : _hand(hand)
    , _runner(runner)
    , _facts(facts)
    , _scores(scores)
    , _missing(std::move(missing))
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
	       kind == ActionKind::GetWoodFromOtherTown || kind == ActionKind::GetFoodFromOtherTownForStoragePit;
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
	return false;
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
