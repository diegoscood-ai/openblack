/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WriteLivingParts.h"

#include <algorithm>
#include <bit>
#include <iterator>
#include <string>
#include <utility>

#include <CreatureRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldReader.h>
#include <WorshipRecords.h>
#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "Common/GameAngle.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/LivingWalkPath.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/LivingFootpath.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/WriteDataPaths.h"
#include "ECS/ThingCircles.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;

namespace
{
namespace components = ecs::components;

/// The living status bits openblack holds: on a villager, dead, the land of its last landing, a skeleton and special
/// (its own), poisoned and downed (their components); on an animal, dying, the land of its last landing and downed;
/// on either, the way along its footpath while it is on one
constexpr uint16_t k_VillagerStatus = components::Villager::k_StatusDead | components::Villager::k_StatusLandTypeMask |
                                      components::Villager::k_StatusSkeleton | components::Villager::k_StatusSpecial;
constexpr uint16_t k_Poisoned = 0x0002;
constexpr uint16_t k_FootpathReverse = 0x0008;
constexpr uint16_t k_Downed = 0x0080;
constexpr uint16_t k_AnimalStatus = 0x0001 | 0x0030 | k_Downed;
/// The alpha the heal chakra, the only thing that gives a living thing its own specular colour, always writes
constexpr uint8_t k_SpecularAlpha = 0xFF;
/// The turns to the circle ahead when there is none
constexpr uint8_t k_NoTurnsToObject = 0xFF;

std::string NameOf(const WriteContext& context, entt::entity entity)
{
	const auto type = context.ClassOf(entity);
	return fmt::format("entity {} ({})", static_cast<uint32_t>(entt::to_integral(entity)),
	                   type.has_value() ? gamesave::TypeName(*type) : std::string("of no class"));
}

/// Map coordinates for a place in metres: those given while they are that place (a place in metres does not give back
/// the same coordinates), else that place's
gamesave::MapCoords CoordsAt(const gamesave::MapCoords& given, glm::vec2 metres)
{
	if (map_coords::ToMetres(given.x) == metres.x && map_coords::ToMetres(given.z) == metres.y)
	{
		return given;
	}
	return {.x = map_coords::ToFixed(metres.x), .z = map_coords::ToFixed(metres.y), .altitude = given.altitude};
}

bool IsCircling(uint8_t moveState)
{
	return moveState >= gamesave::k_FirstCircleState && moveState <= gamesave::k_LastCircleState;
}
} // namespace

std::optional<openblack::ecs::components::AnimalBrain>
openblack::ecs::saved_world_write::AnimalBrainOf(const Registry& registry, entt::entity entity)
{
	if (const auto* brain = registry.TryGet<const components::AnimalBrain>(entity))
	{
		return *brain;
	}
	if (registry.AllOf<components::Animal, components::Transform>(entity))
	{
		return animal_ai::BrainAsMade(entity);
	}
	return std::nullopt;
}

WallHugHeld openblack::ecs::saved_world_write::WallHugHeldOf(const Registry& registry, entt::entity entity)
{
	WallHugHeld held;
	if (const auto brain = AnimalBrainOf(registry, entity))
	{
		held.turnsToStateChange = brain->counter;
		held.speed = brain->speed;
		held.angle = brain->angle;
		held.moveState = brain->moveState;
		held.turnsToObject = brain->turnsToObj;
		held.turnsToStepRebuild = brain->turnsUntilStepRebuild;
		if (brain->hugCircle.set)
		{
			// the circle it goes round or heads for, as a villager's: its entry distance; a thing's circle by its place
			// among the circles of the thing's row (-1 for a thing of one circle) and no place; water or the land's
			// edge by the index 1 and its centre with no height
			const auto& circle = brain->hugCircle;
			held.circleObject = circle.owner;
			held.circleEntryDistance = brain->hugGoalDistance;
			if (circle.owner != entt::null && registry.Valid(circle.owner))
			{
				const auto* shape = ecs::map_cells::CollideDataOf(circle.owner);
				const auto circles = openblack::ecs::thing_circles::CirclesOf(registry, circle.owner);
				const auto found = std::ranges::find_if(circles, [&circle](const auto& each) {
					return each.centre == circle.centre && each.radius == circle.radius;
				});
				if (shape == nullptr || shape->children.empty())
				{
					held.circleIndex = -1;
				}
				else if (found != circles.end())
				{
					held.circleIndex = static_cast<int32_t>(found - circles.begin());
				}
				held.circleCoords = gamesave::MapCoords {};
			}
			else if (circle.owner == entt::null)
			{
				held.circleIndex = 1;
				held.circleCoords = gamesave::MapCoords {
				    .x = map_coords::ToFixed(circle.centre.x), .z = map_coords::ToFixed(circle.centre.y), .altitude = 0.0f};
			}
		}
		else
		{
			// no circle held: none written, with the distance its last circle left
			held.holdsNoCircle = true;
			held.circleEntryDistance = brain->hugGoalDistance;
		}
		held.goal = brain->goal;
		held.goalAltitude = brain->goalAltitude;
		held.target = brain->target;
		held.step = brain->step;
	}
	else if (const auto* wallHug = registry.TryGet<const components::WallHug>(entity))
	{
		// a villager's: its state counter is kept with its states
		if (const auto* action = registry.TryGet<const components::LivingAction>(entity))
		{
			held.turnsToStateChange = std::bit_cast<int16_t>(action->turnsUntilStateChange);
		}
		held.speed = ecs::WholeSpeed(*wallHug);
		held.angle = wallHug->gameAngle;
		held.turnsToStepRebuild = wallHug->turnsUntilStepRebuild;
		held.goal = wallHug->goal;
		held.step = wallHug->step;
		// the thing a villager walks to, until the walk ends
		if (const auto* villager = registry.TryGet<const components::Villager>(entity);
		    villager != nullptr && villager->walkTarget != entt::null && registry.Valid(villager->walkTarget))
		{
			held.target = villager->walkTarget;
		}
		if (const auto* circle = registry.TryGet<const components::WallHugObjectReference>(entity))
		{
			held.turnsToObject = wallHug->turnsToObj;
			held.circleObject = circle->entity;
			held.circleEntryDistance = wallHug->entryDistance;
			const bool ofAThing = !circle->landscape && circle->entity != entt::null && registry.Valid(circle->entity);
			if (ofAThing)
			{
				// a thing's circle: its place among the circles of the thing's row, -1 for a thing of one circle
				const auto* shape = ecs::map_cells::CollideDataOf(circle->entity);
				held.circleIndex = shape != nullptr && !shape->children.empty() ? static_cast<int32_t>(circle->circle) : -1;
				held.circleCoords = gamesave::MapCoords {};
			}
			else
			{
				// water or the land's edge: no thing, the index 1 and the circle's centre with no height
				held.circleIndex = 1;
				held.circleCoords = gamesave::MapCoords {
				    .x = map_coords::ToFixed(circle->centre.x), .z = map_coords::ToFixed(circle->centre.y), .altitude = 0.0f};
			}
		}
		else
		{
			// no circle: the turns to one and the entry distance as the walk keeps them all the same
			held.holdsNoCircle = true;
			held.turnsToObject = wallHug->turnsToObj;
			held.circleEntryDistance = wallHug->entryDistance;
		}
	}
	else if (const auto* creature = registry.TryGet<const components::Creature>(entity))
	{
		// a creature walks by its own body, not by the walk round obstacles: only the turns the hand's pick-up left and
		// the angle a script gives it, as the game keeps it, are its own; the rest is as it is made
		// the turns the hand's pick-up left are not held yet: as its load kept them, 0 as it is made
		const auto* kept = registry.TryGet<const components::LoadedRecord<gamesave::CreatureRecord>>(entity);
		held.turnsToStateChange = kept != nullptr ? kept->record.move.turnsToStateChange : int16_t {0};
		held.angle = static_cast<uint16_t>(game_math::ConvertAngle3DToGame(creature->scriptYAngle));
	}
	if (const auto* footpath = registry.TryGet<const components::LivingFootpath>(entity))
	{
		held.nextFootpath = footpath->next;
	}
	return held;
}

LivingHeld openblack::ecs::saved_world_write::LivingHeldOf(const Registry& registry, entt::entity entity)
{
	LivingHeld held;
	const auto* villager = registry.TryGet<const components::Villager>(entity);
	const auto brain = AnimalBrainOf(registry, entity);
	const auto* creature = registry.TryGet<const components::Creature>(entity);
	if (villager == nullptr && !brain.has_value() && creature == nullptr)
	{
		return held;
	}
	if (creature != nullptr && villager == nullptr && !brain.has_value())
	{
		// a creature: no living states (the game drives none on a creature), the reaction it follows and what it reacts
		// to, its birth turn, and of its status bits, poisoned and the skeleton mark
		if (const auto* reaction = registry.TryGet<const components::CreatureReaction>(entity))
		{
			held.reaction = reaction->reaction;
			held.reactionObject = reaction->object;
		}
		held.birthTurn = creature->birthTurn;
		// of its status bits poisoned; the skeleton mark and the others as kept
		held.status = static_cast<uint16_t>(registry.AllOf<components::Poisoned>(entity) ? k_Poisoned : 0);
		held.statusMask = k_Poisoned;
	}
	else if (villager != nullptr)
	{
		if (const auto* action = registry.TryGet<const components::LivingAction>(entity))
		{
			held.states = action->states;
			held.turnsSinceStateChange = action->turnsSinceStateChange;
		}
		// the reaction is held while the villager follows one of the reactions openblack keeps on it
		if (const auto* reaction = registry.TryGet<const components::LivingReaction>(entity))
		{
			held.reaction = reaction->reaction;
			held.reactionObject = reaction->object;
		}
		held.birthTurn = villager->birthTurn;
		held.status = static_cast<uint16_t>(villager->status & k_VillagerStatus);
		if (registry.AllOf<components::Poisoned>(entity))
		{
			held.status |= k_Poisoned;
		}
		if (registry.AllOf<components::DownedVillager>(entity))
		{
			held.status |= k_Downed;
		}
		held.statusMask = k_VillagerStatus | k_Poisoned | k_Downed;
		held.flock = villager->flock;
		held.flockPlace = villager->flockOrder;
		held.crecheWalk = std::bit_cast<uint32_t>(villager->crecheWalk);
	}
	else
	{
		held.states = std::array<uint8_t, 3> {brain->topState, brain->finalState, brain->previousState};
		held.turnsSinceStateChange = brain->turnsSinceStateChange;
		held.reaction = brain->reaction;
		held.reactionObject = brain->predator;
		held.birthTurn = std::bit_cast<uint32_t>(brain->birthTurn);
		held.status = static_cast<uint16_t>(brain->status & k_AnimalStatus);
		held.statusMask = k_AnimalStatus;
		if (const auto* animal = registry.TryGet<const components::Animal>(entity))
		{
			held.flock = animal->flock;
			held.flockPlace = animal->flockOrder;
		}
	}
	if (const auto* footpath = registry.TryGet<const components::LivingFootpath>(entity))
	{
		held.footpath = FootpathHeld {.footpath = footpath->footpath, .node = footpath->node, .reverse = footpath->reverse};
		if (footpath->reverse)
		{
			held.status |= k_FootpathReverse;
		}
		held.statusMask |= k_FootpathReverse;
	}
	// its own specular colour: none until the heal chakra gives it one, and none again once the chakra takes it away
	if (const auto* specular = registry.TryGet<const components::SpecularColour>(entity))
	{
		held.specular = std::array<uint8_t, 4> {specular->colour.b, specular->colour.g, specular->colour.r, k_SpecularAlpha};
	}
	else
	{
		held.specular = std::array<uint8_t, 4> {};
	}
	held.dance = DanceHeld {};
	if (const auto* dancer = registry.TryGet<const components::Dancer>(entity))
	{
		held.dance->dance = dancer->dance;
		held.dance->group = dancer->group;
		const auto* dance = dancer->dance != entt::null ? registry.TryGet<const components::Dance>(dancer->dance) : nullptr;
		if (dance != nullptr && dancer->group < dance->groups.all.size())
		{
			const auto& dancers = dance->groups.all[dancer->group].dancers;
			held.dance->place = static_cast<uint8_t>(std::distance(dancers.begin(), std::ranges::find(dancers, entity)));
		}
	}
	held.reactionsDone = std::vector<gamesave::ReactionDone> {};
	if (const auto* memory = registry.TryGet<const components::ReactionRecords>(entity))
	{
		for (uint8_t i = 0; i < memory->count; ++i)
		{
			held.reactionsDone->push_back({.reaction = memory->records[i].type, .turn = memory->records[i].turn});
		}
	}
	held.walksATrack = registry.AllOf<components::LivingWalkPath>(entity);
	return held;
}

LivingParts openblack::ecs::saved_world_write::LivingConstructorParts(const GLivingInfo& info)
{
	LivingParts parts;
	// the speed is set from the row's default speed, kept within 0 and 0xFFFF
	parts.move.speed =
	    static_cast<uint16_t>(std::clamp(static_cast<int32_t>(static_cast<uint32_t>(info.speedGroup.speedDefault)), 0, 0xFFFF));
	// the strength is the row's, truncated to a whole number
	parts.living.firstWord = static_cast<uint32_t>(static_cast<int32_t>(info.strength));
	return parts;
}

gamesave::MobilePart openblack::ecs::saved_world_write::MobilePartOf(const gamesave::MobilePart* kept)
{
	return kept != nullptr ? *kept : gamesave::MobilePart {};
}

std::optional<gamesave::MobileObjectPart> openblack::ecs::saved_world_write::MobileObjectPartOf(
    WriteContext& context, entt::entity entity, const gamesave::MobileObjectPart* kept, std::optional<entt::entity> madeWith)
{
	if (kept != nullptr)
	{
		return *kept;
	}
	if (!madeWith.has_value())
	{
		context.Fail(fmt::format("{} was made in play: the object it was made with is not given", NameOf(context, entity)));
		return std::nullopt;
	}
	// a moving object is made with its object, its x and z angles 0 and no path
	return gamesave::MobileObjectPart {.object = context.Reference(*madeWith)};
}

std::optional<gamesave::WallHugPart>
openblack::ecs::saved_world_write::WallHugPartOf(WriteContext& context, entt::entity entity, const gamesave::WallHugPart* kept,
                                                 const gamesave::WallHugPart& made, const WallHugHeld& held)
{
	gamesave::WallHugPart part = kept != nullptr ? *kept : made;
	part.turnsToStateChange = held.turnsToStateChange.value_or(part.turnsToStateChange);
	part.speed = held.speed.value_or(part.speed);
	part.angle = held.angle.value_or(part.angle);
	part.moveState = held.moveState.value_or(part.moveState);
	if (IsCircling(part.moveState))
	{
		// the circle's entry distance, index and place: as held, else as kept
		if (!part.circle.has_value() && held.circleEntryDistance.has_value() && held.circleIndex.has_value() &&
		    held.circleCoords.has_value())
		{
			part.circle = gamesave::CircleHug {};
		}
		if (part.circle.has_value() && held.circleEntryDistance.has_value() && held.circleIndex.has_value() &&
		    held.circleCoords.has_value())
		{
			part.circle->entryDistance = *held.circleEntryDistance;
			part.circle->index = *held.circleIndex;
			part.circle->coords = *held.circleCoords;
		}
		if (!part.circle.has_value() && held.holdsNoCircle)
		{
			// no circle held (the straight look ahead found none, or the walk began afresh): no object, the index 0, no
			// place, no turns to it unless held, and the entry distance as held (an animal's), else 0
			part.circle =
			    gamesave::CircleHug {.turnsToObject = k_NoTurnsToObject, .entryDistance = held.circleEntryDistance.value_or(0)};
		}
		if (!part.circle.has_value())
		{
			context.Fail(fmt::format("{} walks round a circle (move state {}) whose entry distance, index and place no "
			                         "load kept",
			                         NameOf(context, entity), part.moveState));
			return std::nullopt;
		}
		part.circle->turnsToObject = held.turnsToObject.value_or(part.circle->turnsToObject);
		if (held.circleObject.has_value())
		{
			part.circle->object.number = context.Reference(*held.circleObject);
		}
	}
	else
	{
		part.circle.reset();
	}
	if (part.moveState == gamesave::k_StepThroughState)
	{
		if (held.turnsToStepRebuild.has_value())
		{
			part.turnsToStepRebuild = held.turnsToStepRebuild;
		}
		else if (!part.turnsToStepRebuild.has_value())
		{
			context.Fail(fmt::format("{} steps through to its goal, and neither openblack nor a load holds the turns to "
			                         "its next step",
			                         NameOf(context, entity)));
			return std::nullopt;
		}
	}
	else
	{
		part.turnsToStepRebuild.reset();
	}
	if (held.nextFootpath.has_value())
	{
		part.footpath.number = context.Reference(*held.nextFootpath);
	}
	if (held.goal.has_value())
	{
		part.goal = CoordsAt(part.goal, *held.goal);
	}
	part.goal.altitude = held.goalAltitude.value_or(part.goal.altitude);
	if (held.target.has_value())
	{
		part.target.number = context.Reference(*held.target);
	}
	if (held.step.has_value())
	{
		// the step's height is not held: as kept
		part.step.x = held.step->x;
		part.step.z = held.step->y;
	}
	return part;
}

std::optional<gamesave::LivingFields> openblack::ecs::saved_world_write::LivingFieldsOf(WriteContext& context,
                                                                                        entt::entity entity,
                                                                                        const gamesave::LivingFields* kept,
                                                                                        const gamesave::LivingFields& made,
                                                                                        const LivingHeld& held)
{
	gamesave::LivingFields part = kept != nullptr ? *kept : made;
	if (held.footpath.has_value())
	{
		const auto footpath = held.footpath->footpath;
		part.footpath.number = context.Reference(footpath);
		part.footpathAgain = part.footpath;
		// (chosen difference, 2026-11-11) a node its footpath no longer holds, which a re-plan deleted under the
		// living, is written as no node: the original writes the freed node, which is undefined
		const auto* path = footpath != entt::null && context.GetRegistry().Valid(footpath)
		                       ? context.GetRegistry().TryGet<const components::Footpath>(footpath)
		                       : nullptr;
		const bool nodeHeld = path != nullptr && held.footpath->node >= 0 &&
		                      std::ranges::any_of(path->nodes, [&held](const components::Footpath::Node& node) {
			                      return node.id == held.footpath->node;
		                      });
		part.footpathNode.number =
		    nodeHeld ? context.Reference(SourceKey {.type = gamesave::FootpathNodeRecord::k_Type,
		                                            .id = PartId(footpath, static_cast<uint32_t>(held.footpath->node))})
		             : 0;
	}
	if (held.states.has_value())
	{
		std::ranges::copy(*held.states, part.action.begin());
	}
	if (held.turnsSinceStateChange.has_value())
	{
		part.action[4] = static_cast<uint8_t>(*held.turnsSinceStateChange & 0xFFU);
		part.action[5] = static_cast<uint8_t>(*held.turnsSinceStateChange >> 8U);
	}
	if (held.reaction.has_value())
	{
		// none for a reaction that has gone: the game holds a living's reaction by a pointer that its deletion sets to
		// none
		const bool listed = *held.reaction != 0 &&
		                    (!Locator::reactionSystem::has_value() || ecs::effects::reactions::Find(*held.reaction) != nullptr);
		part.reaction.number =
		    listed ? context.Reference(SourceKey {.type = gamesave::ReactionRecord::k_Type, .id = *held.reaction}) : 0;
	}
	else if (kept != nullptr && part.reaction.number != 0)
	{
		// a loaded living that no longer follows the reaction its load gave it back has stopped reacting: none, and no
		// object reacted to, as a living that stops reacting clears both; one its load did not make again as kept
		const auto target = context.TargetOf(part.reaction.number);
		if (target.has_value() && target->id.has_value() && target->type == gamesave::ReactionRecord::k_Type)
		{
			part.reaction.number = 0;
			part.otherObject.number = 0;
		}
	}
	if (held.birthTurn.has_value())
	{
		part.birthTurn = *held.birthTurn;
	}
	else if (kept == nullptr)
	{
		context.Fail(fmt::format("{} was made in play, and openblack does not hold the birth turn drawn for it",
		                         NameOf(context, entity)));
		return std::nullopt;
	}
	// a walk along a track: its data path, written by its own writer; none once it has stopped walking it
	if (held.walksATrack)
	{
		part.dataPath.number =
		    context.Reference(SourceKey {.type = gamesave::DataPathRecord::k_Type, .id = DataPathIdOf(entity)});
	}
	part.status = static_cast<uint16_t>((part.status & ~held.statusMask) | (held.status & held.statusMask));
	if (held.flock.has_value())
	{
		part.flock.number = context.Reference(*held.flock);
	}
	if (held.reactionObject.has_value())
	{
		part.otherObject.number = context.Reference(*held.reactionObject);
	}
	part.secondWord = held.crecheWalk.value_or(part.secondWord);
	part.specular = held.specular.value_or(part.specular);
	part.secondByte = held.flockPlace.value_or(part.secondByte);
	if (held.dance.has_value())
	{
		const auto dance = held.dance->dance;
		if (dance != entt::null)
		{
			part.danceGroup.number = context.Reference(SourceKey {
			    .type = gamesave::DanceGroupRecord::k_Type, .id = PartId(dance, static_cast<uint32_t>(held.dance->group))});
			part.thirdByte = held.dance->place;
		}
		else
		{
			// its place in its last group stays as it was
			part.danceGroup.number = 0;
		}
	}
	if (held.reactionsDone.has_value())
	{
		part.reactionsDone = *held.reactionsDone;
	}
	return part;
}

std::optional<LivingParts> openblack::ecs::saved_world_write::LivingPartsOf(WriteContext& context, entt::entity entity,
                                                                            const LivingParts* kept, const LivingParts& made)
{
	const auto& registry = context.GetRegistry();
	auto move =
	    WallHugPartOf(context, entity, kept != nullptr ? &kept->move : nullptr, made.move, WallHugHeldOf(registry, entity));
	if (!move.has_value())
	{
		return std::nullopt;
	}
	auto living =
	    LivingFieldsOf(context, entity, kept != nullptr ? &kept->living : nullptr, made.living, LivingHeldOf(registry, entity));
	if (!living.has_value())
	{
		return std::nullopt;
	}
	return LivingParts {.mobile = MobilePartOf(kept != nullptr ? &kept->mobile : nullptr),
	                    .move = std::move(*move),
	                    .living = std::move(*living)};
}
