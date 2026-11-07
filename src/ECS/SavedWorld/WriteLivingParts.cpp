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

#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldReader.h>
#include <WorshipRecords.h>
#include <fmt/format.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/LivingWalkPath.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingFootpath.h"
#include "ECS/Registry.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"

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

WallHugHeld openblack::ecs::saved_world_write::WallHugHeldOf(const Registry& registry, entt::entity entity)
{
	WallHugHeld held;
	if (const auto* brain = registry.TryGet<const components::AnimalBrain>(entity))
	{
		held.turnsToStateChange = brain->counter;
		held.speed = brain->speed;
		held.angle = brain->angle;
		held.moveState = brain->moveState;
		held.turnsToObject = brain->turnsToObj;
		if (brain->hugCircle.set)
		{
			held.circleObject = brain->hugCircle.owner;
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
		if (const auto* circle = registry.TryGet<const components::WallHugObjectReference>(entity))
		{
			held.turnsToObject = circle->stepsAway;
			held.circleObject = circle->entity;
		}
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
	const auto* brain = registry.TryGet<const components::AnimalBrain>(entity);
	if (villager == nullptr && brain == nullptr)
	{
		return held;
	}
	if (villager != nullptr)
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
		// the circle's entry distance, index and place are not held: as kept
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
		part.footpathNode.number =
		    footpath != entt::null && held.footpath->node >= 0
		        ? context.Reference(SourceKey {.type = gamesave::FootpathNodeRecord::k_Type,
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
		part.reaction.number =
		    *held.reaction != 0 ? context.Reference(SourceKey {.type = gamesave::ReactionRecord::k_Type, .id = *held.reaction})
		                        : 0;
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
	if (held.walksATrack && part.dataPath.number == 0)
	{
		context.Fail(fmt::format("{} walks a track whose data no load kept", NameOf(context, entity)));
		return std::nullopt;
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
