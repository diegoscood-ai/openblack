/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureMindSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureCreation.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/PerceivedDesires.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureReactions.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
namespace animations = openblack::creature_layers::animations;
using creature_desires::Desire;

namespace openblack::ecs::systems::mind_detail
{
constexpr float k_TurnSeconds = game_clock::k_TurnSeconds;
constexpr float k_TurnsPerSecond = 1.0f / k_TurnSeconds;
/// A creature just put in the world takes its first few turns to stand up before it can pass out
constexpr uint32_t k_TurnsBeforePassingOut = 2;
/// A face told to by hand is held this long
constexpr float k_ToldFaceMs = 3000.0f;
/// How pressing the look a cut scene forces is, as the game's forced plan has it
constexpr float k_CutSceneLookScore = 1.0f;

/// A creature looks for food and water this far away at most
constexpr float k_FoodSearchDistance = 200.0f;
/// Water is looked for this many cells of the walkable mask round the creature, 10 m a cell
constexpr int32_t k_WaterSearchCells = 8;
constexpr float k_WaterCellSize = 10.0f;
/// Where it stands to drink is found this far at most from the water
constexpr float k_ShoreSearchDistance = 60.0f;
/// Food further than this from its mouth, for its size, has got away by the time it eats
constexpr float k_EatReach = 15.0f;
/// Having drunk it doesn't want water for a while, and having had a poo it doesn't want another for longer
constexpr float k_WaterSuppressSeconds = 20.0f;
constexpr float k_PooSuppressSeconds = 60.0f;
/// Having spat out what was not food, or was poisoned, it doesn't want food for a while
constexpr float k_MealRefusedSuppressSeconds = 20.0f;
/// The rows of the game's creature action table that eating and fighting count as
constexpr std::string_view k_EatAction = "EatAfterExamining";
constexpr std::string_view k_FightAction = "Fight";
/// A creature looks for things to pick up this far away at most, and for homes and trees to hurl them at
constexpr float k_ObjectSearchDistance = 80.0f;
constexpr float k_HurlSearchDistance = 120.0f;
/// The creature wants the player's attention more the longer it is alone, fully after a minute, and from lack of
/// anything to do with the player, fully after two
constexpr float k_LonelySeconds = 60.0f;
constexpr float k_UninterestedSeconds = 120.0f;
/// The desire to show how it is grows with all its desires added up, fully at this much
constexpr float k_ManifestSum = 3.0f;

/// Just woken up, the eyelids droop and blink slowly; otherwise they are as they start
constexpr float k_SleepyOpenness = -0.7f;
constexpr int32_t k_SleepyBlinkIntervalMs = 2500;

/// How high above their feet creatures look at other things
constexpr float k_VillagerHeadHeight = 1.8f;
constexpr float k_AbodeLookHeight = 4.0f;
constexpr float k_TreeLookHeight = 6.0f;
constexpr float k_CitadelLookHeight = 30.0f;

float Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}

/// How a species' desires start, from the game's creature tables (creature_creation::SetupFor), before a new
/// creature's draws
std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> SetupFor(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return {};
	}
	return creature_creation::SetupFor(Locator::infoConstants::value(), species).desires;
}

/// The desires each stage of growing up brings and takes away, from the game's tables
std::vector<creature_desires::PhaseDesires> Phases()
{
	std::vector<creature_desires::PhaseDesires> phases;
	if (!Locator::infoConstants::has_value())
	{
		return phases;
	}
	const auto valid = [](uint32_t desire) { return desire < creature_desires::k_DesireCount; };
	for (const auto& entry : Locator::infoConstants::value().creatureDevelopmentPhaseEntry)
	{
		auto& phase = phases.emplace_back();
		const std::array<uint32_t, 10> add {entry.field0x3c, entry.field0x40, entry.field0x44, entry.field0x48,
		                                    entry.field0x4c, entry.field0x50, entry.field0x54, entry.field0x58,
		                                    entry.field0x5c, entry.field0x60};
		const std::array<uint32_t, 4> remove {entry.field0x64, entry.field0x68, entry.field0x6c, entry.field0x70};
		for (const auto desire : add | std::views::filter(valid))
		{
			phase.add.push_back(static_cast<Desire>(desire));
		}
		for (const auto desire : remove | std::views::filter(valid))
		{
			phase.remove.push_back(static_cast<Desire>(desire));
		}
	}
	return phases;
}

/// The sources that follow the creature's state: its body's, and its mind's own
std::optional<float> ReadSource(uint32_t type, const creature_desires::Desires& desires, const CreatureMindState& mind,
                                const creature_physiology::Needs& needs, bool night)
{
	namespace sources = creature_desires::sources;
	if (const auto value = creature_physiology::SourceValue(type, needs, night))
	{
		return value;
	}
	switch (type)
	{
	case sources::k_AttentionFromLoneliness:
		return Clamp01(mind.secondsAlone / k_LonelySeconds);
	case sources::k_AttentionFromLackOfInteraction:
		return Clamp01(mind.secondsAlone / k_UninterestedSeconds);
	case sources::k_ManifestState:
		return Clamp01(desires.sum / k_ManifestSum);
	case sources::k_RunAwayFromPlayer:
		return Clamp01(-mind.attitudeToPlayer);
	case sources::k_AngerFromSadness:
	case sources::k_PlayFromSadness:
	case sources::k_TirednessFromSadness:
		return creature_desires::SourceValue(desires[Desire::Sadness], sources::k_Sadness).value_or(0.0f);
	default:
		return std::nullopt;
	}
}

creature_look::Interest LookInterestOf(AnimalInfo type)
{
	// The class of every bird the original's factory makes. The vulture, goat and zebra are given the class of the
	// same name and the citadel dove and bat the animal's: none of these is made by the factory, so this is assumed
	// (docs/wiki/creature/mind.md#pending)
	return animal_ai::IsDoveClass(type) ? creature_look::Interest::Dove : creature_look::Interest::Animal;
}

std::vector<creature_look::Candidate> GatherCandidates(const ecs::Registry& registry)
{
	std::vector<creature_look::Candidate> candidates;
	const auto add = [&candidates](entt::entity entity, creature_look::Interest kind, const glm::vec3& point) {
		candidates.push_back({.id = entt::to_integral(entity), .kind = kind, .point = point});
	};
	std::as_const(registry).Each<const Creature, const Transform>([&](entt::entity entity, const Creature& creature, const Transform& at) {
		add(entity, creature_look::Interest::Creature,
		    at.position + glm::vec3(0.0f, creature_look::k_HeadHeight * creature.size, 0.0f));
	});
	std::as_const(registry).Each<const Villager, const Transform>([&](entt::entity entity, const Villager&, const Transform& at) {
		add(entity, creature_look::Interest::Villager, at.position + glm::vec3(0.0f, k_VillagerHeadHeight, 0.0f));
	});
	std::as_const(registry).Each<const Abode, const Transform>([&](entt::entity entity, const Abode&, const Transform& at) {
		add(entity, creature_look::Interest::Abode, at.position + glm::vec3(0.0f, k_AbodeLookHeight, 0.0f));
	});
	std::as_const(registry).Each<const Tree, const Transform>([&](entt::entity entity, const Tree&, const Transform& at) {
		add(entity, creature_look::Interest::Tree, at.position + glm::vec3(0.0f, k_TreeLookHeight, 0.0f));
	});
	std::as_const(registry).Each<const Temple, const Transform>([&](entt::entity entity, const Temple&, const Transform& at) {
		add(entity, creature_look::Interest::Citadel, at.position + glm::vec3(0.0f, k_CitadelLookHeight, 0.0f));
	});
	// An animal is looked at where it stands. Like every kind here, it is offered wherever it is, held or dying
	// included, where the original offers only what lies in the cells it scans (docs/wiki/creature/mind.md#pending)
	std::as_const(registry).Each<const Animal, const Transform>(
	    [&](entt::entity entity, const Animal& animal, const Transform& at) {
		    add(entity, LookInterestOf(animal.type), at.position);
	    });
	return candidates;
}

void ApplyEyes(CreatureEyes* eyes, creature_mind::Eyes look)
{
	if (eyes == nullptr || look == creature_mind::Eyes::Unchanged)
	{
		return;
	}
	const bool sleepy = look == creature_mind::Eyes::Sleepy;
	eyes->mode = look == creature_mind::Eyes::Closed ? creature_eyes::Mode::Closed : creature_eyes::Mode::Calm;
	eyes->openness = sleepy ? k_SleepyOpenness : 0.0f;
	eyes->blink.intervalMs = sleepy ? k_SleepyBlinkIntervalMs : creature_eyes::k_BlinkIntervalMs;
}

/// Turns the creature to face down the slope of the land about it, unless the lowest ground is right where it stands
void FaceDownSlope(entt::entity creature, CreatureLocomotionSystemInterface& locomotion)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(creature);
	if (transform == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const glm::vec2 from {transform->position.x, transform->position.z};
	const auto lowest = creature_mind::DownSlope(from, [](glm::vec2 at) -> std::optional<float> {
		if (!map_coords::InBounds(glm::vec3(at.x, 0.0f, at.y)))
		{
			return std::nullopt;
		}
		return Locator::terrainSystem::value().GetHeightAt(at);
	});
	if (glm::distance(lowest, from) >= creature_mind::k_OnTopOf)
	{
		locomotion.TurnToFace(creature, lowest);
	}
}

/// Sends the creature where its mind wants it to go
void Move(entt::entity creature, const creature_mind::Commands& commands)
{
	if (!Locator::creatureLocomotionSystem::has_value())
	{
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	if (commands.stopMoving)
	{
		locomotion.Stop(creature);
	}
	if (!commands.move.has_value())
	{
		return;
	}
	using Kind = creature_mind::Movement::Kind;
	using Pace = CreatureLocomotionSystemInterface::Pace;
	const auto& move = *commands.move;
	const auto pace = move.run ? Pace::Run : Pace::Walk;
	const auto object = move.object.has_value() ? std::optional(static_cast<entt::entity>(*move.object)) : std::nullopt;
	switch (move.kind)
	{
	case Kind::ToPoint:
	case Kind::Nearby:
		locomotion.MoveTo(creature, move.point, pace, move.minDistance, move.maxDistance);
		break;
	case Kind::ToObject:
		if (object.has_value())
		{
			locomotion.MoveToObject(creature, *object, pace, move.maxDistance);
		}
		break;
	case Kind::Follow:
		if (object.has_value())
		{
			locomotion.Follow(creature, *object, move.maxDistance, pace);
		}
		break;
	case Kind::FleeFrom:
		locomotion.FleeFrom(creature, move.point);
		break;
	case Kind::TurnToFace:
		locomotion.TurnToFace(creature, move.point);
		break;
	case Kind::FaceDownSlope:
		FaceDownSlope(creature, locomotion);
		break;
	case Kind::FacePoint:
		// Only a script agenda plans it, and nothing gives a creature one: a script's focus is FocusByScript's
		break;
	case Kind::GoNearObject:
	case Kind::GetAwayFromObject:
	case Kind::TurnToFaceObject:
		// Overseen turn by turn by the mind as it casts (StartSubMove)
		break;
	}
}

/// Whether the thing the mind's step waits for is on the map, none when no step waits for one or the thing is gone
std::optional<bool> WaitedForInMap(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WaitInMap ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return std::nullopt;
	}
	const auto object = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	if (!registry.Valid(object))
	{
		return std::nullopt;
	}
	return ecs::map_cells::IsObjectInMap(object);
}

/// The movements the mind oversees turn by turn, done or failed by what the creature's movement says
bool IsSubMove(creature_mind::Movement::Kind kind)
{
	using Kind = creature_mind::Movement::Kind;
	return kind == Kind::GoNearObject || kind == Kind::GetAwayFromObject || kind == Kind::TurnToFaceObject;
}

void Apply(const creature_mind::Commands& commands, CreatureAnimation& animation, CreatureEyes* eyes)
{
	if (commands.endSit)
	{
		animation.body = creature_layers::EndLoop(animation.body);
	}
	if (commands.playOnce.has_value())
	{
		if (auto body = creature_layers::PlayOnce(animation.body, *commands.playOnce, commands.mirrored))
		{
			animation.body = *body;
		}
	}
	if (commands.startSequence.has_value())
	{
		const auto& sequence = *commands.startSequence;
		if (auto body = creature_layers::PlaySequence(animation.body, sequence[0], sequence[1], sequence[2], commands.holdLoop))
		{
			animation.body = *body;
		}
	}
	if (commands.playOverlay.has_value())
	{
		if (auto gesture = creature_layers::PlayGesture(animation.gesture, *commands.playOverlay))
		{
			animation.gesture = *gesture;
		}
	}
	if (commands.relaxFace)
	{
		animation.face = creature_layers::RelaxFace(animation.face);
	}
	if (commands.face.has_value())
	{
		animation.face = creature_layers::PullFace(animation.face, creature_face::AnimationOf(commands.face->face),
		                                           commands.face->milliseconds, commands.face->cue);
	}
	ApplyEyes(eyes, commands.eyes);
}

/// What, besides why, picks the faces the creature pulls
creature_face::Feelings FeelingsOf(const CreatureMindState& mind)
{
	return {.variety = mind.idle.faceVariety, .attitudeToPlayer = mind.attitudeToPlayer, .idlePick = 0};
}

bool IsNight()
{
	return Locator::dayNightClock::has_value() && Locator::dayNightClock::value().Clock().IsVisualNight();
}

/// Whether one found thing is to be taken over another as good: the lower entity
bool Before(entt::entity entity, entt::entity other)
{
	return entt::to_integral(entity) < entt::to_integral(other);
}

float DesireValue(const creature_desires::Desires& desires, Desire desire)
{
	const auto& state = desires[desire];
	return state.activated ? state.value : 0.0f;
}

/// What something is worth to eat, if anything
std::optional<float> FoodValueOf(entt::entity entity)
{
	return Locator::creatureObjectActionSystem::has_value() ? Locator::creatureObjectActionSystem::value().FoodValueOf(entity)
	                                                        : std::nullopt;
}

/// The food within reach of a point the creature would most like to eat, and where it is: the nearest, unless it has
/// learnt to prefer some kinds of food (weigh says how useful each is, 0.1 for food nothing is known about); food it has
/// learnt is worse than that it leaves alone
std::optional<std::pair<entt::entity, glm::vec2>> NearestFood(const ecs::Registry& registry, entt::entity self, glm::vec2 from,
                                                              const std::function<float(entt::entity)>& weigh)
{
	constexpr float k_FoodDistanceWeight = 0.05f;
	std::optional<std::pair<entt::entity, glm::vec2>> nearest;
	float best = 0.0f;
	const auto consider = [&](entt::entity entity, const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		if (distance > k_FoodSearchDistance || !ecs::IsAvailable(entity) || registry.AllOf<HeldByCreature>(entity) ||
		    !mind_detail::CanBeEaten(registry, self, entity))
		{
			return;
		}
		const auto usefulness = weigh ? weigh(entity) : creature_planner::k_DefaultUsefulness;
		if (usefulness < creature_planner::k_DefaultUsefulness)
		{
			return;
		}
		const auto score = usefulness * creature_planner::DistancePriority(false, distance, k_FoodDistanceWeight) +
		                   1e-6f * (k_FoodSearchDistance - distance);
		if (score > best || (score == best && nearest.has_value() && Before(entity, nearest->first)))
		{
			best = score;
			nearest = {entity, point};
		}
	};
	std::as_const(registry).Each<const Villager, const Transform>(
	    [&](entt::entity entity, const Villager&, const Transform& at) { consider(entity, at); });
	// Animals are eaten too, as the game's creatures eat cows and sheep
	std::as_const(registry).Each<const Animal, const Transform>(
	    [&](entt::entity entity, const Animal&, const Transform& at) { consider(entity, at); });
	std::as_const(registry).Each<const MobileObject, const Transform>(
	    [&](entt::entity entity, const MobileObject&, const Transform& at) { consider(entity, at); });
	std::as_const(registry).Each<const Pot, const Transform>(
	    [&](entt::entity entity, const Pot&, const Transform& at) { consider(entity, at); });
	return nearest;
}

/// The nearest water to a point, searching the cells of the walkable mask round it: water it can wade into, or deep sea,
/// and where it can stand at its edge
std::optional<creature_mind::Wants::WaterSpot> NearestWater(glm::vec2 from)
{
	if (!Locator::terrainSystem::has_value() || !Locator::landAvoidSystem::has_value())
	{
		return std::nullopt;
	}
	const auto size = Locator::landAvoidSystem::value().GetState().size;
	const auto& terrain = Locator::terrainSystem::value();
	const auto cellX = static_cast<int32_t>(std::floor(from.x / k_WaterCellSize));
	const auto cellZ = static_cast<int32_t>(std::floor(from.y / k_WaterCellSize));
	std::optional<glm::vec2> nearest;
	float best = std::numeric_limits<float>::max();
	for (int32_t x = cellX - k_WaterSearchCells; x <= cellX + k_WaterSearchCells; ++x)
	{
		for (int32_t z = cellZ - k_WaterSearchCells; z <= cellZ + k_WaterSearchCells; ++z)
		{
			if (x < 0 || z < 0 || x >= size || z >= size)
			{
				continue;
			}
			const auto centre = (glm::vec2(static_cast<float>(x), static_cast<float>(z)) + 0.5f) * k_WaterCellSize;
			const auto ground = land_avoid::At(x, z);
			// (approximate) the sea is the cells it can't reach whose middle lies at the sea's level
			const bool sea =
			    (ground == land_avoid::k_Avoid || ground == land_avoid::k_Unreachable) && terrain.GetHeightAt(centre) <= 0.0f;
			if (ground != land_avoid::k_Water && !sea)
			{
				continue;
			}
			const auto distance = glm::distance(centre, from);
			if (distance < best)
			{
				best = distance;
				nearest = centre;
			}
		}
	}
	if (!nearest.has_value())
	{
		return std::nullopt;
	}
	// It stands at the edge of the water on its own side, or in the shallows
	const auto shore = land_avoid::NearestValid(*nearest, land_avoid::k_CreatureRadius, k_ShoreSearchDistance);
	if (!shore.has_value())
	{
		return std::nullopt;
	}
	return creature_mind::Wants::WaterSpot {.shore = *shore, .water = *nearest};
}

/// The nearest thing a creature could pick up, and where it is
std::optional<std::pair<entt::entity, glm::vec2>> NearestObject(const ecs::Registry& registry, entt::entity self,
                                                                glm::vec2 from)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& hands = Locator::creatureObjectActionSystem::value();
	std::optional<std::pair<entt::entity, glm::vec2>> nearest;
	float best = k_ObjectSearchDistance;
	std::as_const(registry).Each<const MobileObject, const Transform>([&](entt::entity entity, const MobileObject&, const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		const bool better = distance < best || (distance == best && (!nearest.has_value() || Before(entity, nearest->first)));
		if (better && ecs::IsAvailable(entity) && !registry.AllOf<HeldByCreature>(entity) && hands.CanPickUp(self, entity))
		{
			best = distance;
			nearest = {entity, point};
		}
	});
	return nearest;
}

/// The nearest home or tree, to hurl something at
std::optional<glm::vec2> NearestHurlTarget(const ecs::Registry& registry, glm::vec2 from)
{
	std::optional<std::pair<entt::entity, glm::vec2>> nearest;
	float best = k_HurlSearchDistance;
	const auto consider = [&](entt::entity entity, const Transform& at) {
		const glm::vec2 point {at.position.x, at.position.z};
		const auto distance = glm::distance(point, from);
		const bool better = distance < best || (distance == best && (!nearest.has_value() || Before(entity, nearest->first)));
		if (better && ecs::IsAvailable(entity))
		{
			best = distance;
			nearest = {entity, point};
		}
	};
	std::as_const(registry).Each<const Abode, const Transform>(
	    [&](entt::entity entity, const Abode&, const Transform& at) { consider(entity, at); });
	std::as_const(registry).Each<const Tree, const Transform>(
	    [&](entt::entity entity, const Tree&, const Transform& at) { consider(entity, at); });
	return nearest.has_value() ? std::optional(nearest->second) : std::nullopt;
}

/// The needs the mind might see to now, and the food and water at hand for them
creature_mind::Wants WantsOf(const ecs::Registry& registry, entt::entity creature, const creature_desires::Desires& desires,
                             glm::vec2 position, const std::function<float(entt::entity)>& weighFood)
{
	creature_mind::Wants wants {
	    .hunger = DesireValue(desires, Desire::Hunger),
	    .tiredness = DesireValue(desires, Desire::Tiredness),
	    .poo = DesireValue(desires, Desire::Poo),
	    .water = DesireValue(desires, Desire::Water),
	    .curiosity = DesireValue(desires, Desire::Curiosity),
	    .play = DesireValue(desires, Desire::Play),
	    .anger = DesireValue(desires, Desire::Anger),
	    .holding = registry.AllOf<CreatureHeldObject>(creature),
	};
	if (!wants.holding && std::max({wants.curiosity, wants.play, wants.anger}) >= creature_mind::k_ActOnDesire)
	{
		if (const auto object = NearestObject(registry, creature, position))
		{
			wants.object = entt::to_integral(object->first);
			wants.objectPoint = object->second;
		}
		if (wants.anger >= creature_mind::k_ActOnDesire)
		{
			wants.hurlTarget = NearestHurlTarget(registry, position);
		}
	}
	if (wants.hunger >= creature_mind::k_ActOnNeed)
	{
		if (const auto food = NearestFood(registry, creature, position, weighFood))
		{
			wants.food = entt::to_integral(food->first);
			wants.foodPoint = food->second;
		}
	}
	if (wants.water >= creature_mind::k_ActOnNeed)
	{
		wants.waterSpot = NearestWater(position);
	}
	return wants;
}

/// Having done an action, the desire it satisfies is less, by the game's action table, and its body pays for it
void Satisfied(entt::entity creature, creature_desires::Desires& desires, std::string_view action)
{
	if (Locator::infoConstants::has_value())
	{
		const auto& actions = Locator::infoConstants::value().creatureAction;
		const auto found = std::ranges::find_if(actions, [action](const auto& row) {
			return std::string_view(row.name.data(), strnlen(row.name.data(), row.name.size())) == action;
		});
		if (found != actions.end() && found->desire < creature_desires::k_DesireCount)
		{
			auto& state = desires[static_cast<Desire>(found->desire)];
			if (state.activated)
			{
				state.value = std::clamp(state.value * found->desireMultiplier, 0.0f, std::max(state.max, 0.0f));
			}
		}
	}
	if (Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().FinishAction(creature, action);
	}
}

/// What a step did to the body
void TakeEffect(entt::entity creature, const creature_mind::Commands& commands, creature_desires::Desires& desires)
{
	using creature_mind::Effect;
	if (commands.effect == Effect::None || !Locator::creaturePhysiologySystem::has_value())
	{
		return;
	}
	auto& physiology = Locator::creaturePhysiologySystem::value();
	switch (commands.effect)
	{
	case Effect::Eat:
		// Its body pays for the action; the meal itself takes its hunger down, as its hands judge it when the eating ends
		physiology.FinishAction(creature, k_EatAction);
		break;
	// Examining, throwing about, hurling and sleeping do nothing to its desires as they happen: only the plan they are
	// part of does, once carried out to its end
	case Effect::Examined:
	case Effect::ThrewAbout:
	case Effect::Hurled:
	case Effect::Slept:
	case Effect::TookFromHand:
		break;
	// A step that completes the action sees to its desires as the plan's end would (CompleteByStep)
	case Effect::Completed:
		break;
	case Effect::Drink:
		physiology.Drink(creature);
		creature_desires::Suppress(desires, Desire::Water, k_WaterSuppressSeconds, k_TurnsPerSecond);
		break;
	case Effect::Poo:
		physiology.Poo(creature);
		desires[Desire::Poo].value = 0.0f;
		creature_desires::Suppress(desires, Desire::Poo, k_PooSuppressSeconds, k_TurnsPerSecond);
		break;
	case Effect::Puke:
		physiology.Puke(creature);
		break;
	case Effect::CameRound:
		physiology.WakeFromFaint(creature);
		break;
	case Effect::None:
		break;
	}
}

/// Tells the creature's hands what to do with a thing
void Order(const ecs::Registry& registry, entt::entity creature, const creature_mind::Commands& commands)
{
	if (!commands.object.has_value() || !Locator::creatureObjectActionSystem::has_value())
	{
		return;
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto& order = *commands.object;
	const auto object = order.object.has_value() ? std::optional(static_cast<entt::entity>(*order.object)) : std::nullopt;
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(order.point) : 0.0f;
	const glm::vec3 point {order.point.x, ground, order.point.y};
	using Kind = creature_mind::ObjectOrder::Kind;
	switch (order.kind)
	{
	case Kind::Catch:
		// Its hands' part is over once the catch has started: the body carries it on by itself
		if (!object.has_value() || !registry.Valid(*object) || !hands.Catch(creature, *object))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PickUp:
		if (object.has_value() && registry.Valid(*object) && ecs::IsAvailable(*object))
		{
			hands.PickUp(creature, *object);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PickUpPortion:
		if (object.has_value() && registry.Valid(*object) && ecs::IsAvailable(*object))
		{
			hands.PickUpPortion(creature, *object, order.portion);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PutDown:
		hands.PutDown(creature);
		break;
	case Kind::Discard:
		hands.Discard(creature);
		break;
	case Kind::Eat:
		hands.EatHeld(creature);
		break;
	case Kind::Keep:
		hands.Keep(creature, order.animation);
		break;
	case Kind::Throw:
	case Kind::ThrowNearby:
		// At a point in the air when it has a height, as the player's camera; otherwise at the ground there
		hands.Throw(creature, {point.x, order.pointHeight.value_or(point.y), point.z});
		break;
	case Kind::TakeFromHand:
		if (!object.has_value() || !registry.Valid(*object) || !hands.TakeFromHand(creature, *object, order.mirrored))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::Destroy:
		if (object.has_value() && registry.Valid(*object) && ecs::IsAvailable(*object))
		{
			hands.Destroy(creature, *object);
		}
		else
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PointAt:
		hands.PointAt(creature, point);
		break;
	}
}

creature_mind::HandsState HandsOf(entt::entity creature)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return creature_mind::HandsState::Idle;
	}
	using State = CreatureObjectActionSystemInterface::State;
	switch (Locator::creatureObjectActionSystem::value().GetState(creature))
	{
	case State::Busy:
		return creature_mind::HandsState::Busy;
	case State::Done:
		return creature_mind::HandsState::Done;
	case State::Failed:
		return creature_mind::HandsState::Failed;
	case State::Idle:
		break;
	}
	return creature_mind::HandsState::Idle;
}
} // namespace openblack::ecs::systems::mind_detail

using namespace openblack::ecs::systems::mind_detail;

namespace
{
/// While it walks, the head does not look at what a step gives it to look at once that is this near
constexpr float k_NoGazeWhileWalkingWithin = 10.0f;

/// Where the head looks for a step that gives it something to look at, if anywhere this turn: the top of a thing (not
/// one it holds), or the ground under it, the ground under a point, or the camera. Walking, it does not look at a thing
/// or a point nearer than ten. A creature without a player does not look at the camera.
std::optional<glm::vec3> GazePoint(const ecs::Registry& registry, entt::entity creature, const creature_mind::Gaze& gaze,
                                   bool walking)
{
	if (gaze.camera)
	{
		const auto* body = registry.TryGet<const Creature>(creature);
		if (body == nullptr || body->owner == PlayerNames::NEUTRAL || !Locator::camera::has_value())
		{
			return std::nullopt;
		}
		return Locator::camera::value().GetOrigin();
	}
	const auto* self = registry.TryGet<const Transform>(creature);
	std::optional<glm::vec3> at;
	if (gaze.object.has_value())
	{
		const auto thing = static_cast<entt::entity>(*gaze.object);
		const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
		const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
		if (transform == nullptr || (!gaze.bottom && held != nullptr && held->object == thing))
		{
			return std::nullopt;
		}
		at = transform->position;
	}
	else if (gaze.point.has_value())
	{
		at = gaze.point;
	}
	if (!at.has_value() || self == nullptr)
	{
		return std::nullopt;
	}
	const glm::vec2 flat {at->x, at->z};
	if (walking && glm::distance(glm::vec2(self->position.x, self->position.z), flat) < k_NoGazeWhileWalkingWithin)
	{
		return std::nullopt;
	}
	if (gaze.object.has_value() && !gaze.bottom)
	{
		return glm::vec3 {flat.x, at->y + ecs::object::GetHeight(static_cast<entt::entity>(*gaze.object)), flat.y};
	}
	// On the ground under it
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(flat) : at->y;
	return glm::vec3 {flat.x, ground, flat.y};
}

/// The ground's height at a point, or the height given without a land
float GroundAt(glm::vec2 at, float otherwise)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : otherwise;
}

/// Where the head looks this turn on the way to a point, after this turn's look about (mind.look), or none when it
/// has no look of its own: the point's ground, what it watches, or the sweep's point on the ground, as high above it
/// as the creature stands above its own ground
std::optional<glm::vec3> WalkLookPoint(const ecs::Registry& registry, entt::entity creature, CreatureMindState& mind,
                                       const glm::vec3& point, const Transform& at, uint32_t turnsPerSecond)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& species = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(body->species);
	if (row >= species.size())
	{
		return std::nullopt;
	}
	const auto& info = species.at(row);
	const creature_mind::WalkLookInfo numbers {.sweepAngle = info.lookSweepAngle,
	                                           .sweepStep = info.lookSweepStep,
	                                           .firstDistance = info.lookSweepFirstDistance,
	                                           .farthest = info.lookSweepFarthest,
	                                           .distanceStep = info.lookSweepDistanceStep,
	                                           .pointWithin = info.lookAtPointWithin};
	if (!mind.walkLook.has_value())
	{
		mind.walkLook = creature_mind::StartWalkLook(numbers);
	}
	const glm::vec2 from {at.position.x, at.position.z};
	const glm::vec2 to {point.x, point.z};
	const auto look = creature_mind::LookWhileGoing(*mind.walkLook, numbers, from, to, BodyYaw(registry, creature, at),
	                                                mind.look.id.has_value(), turnsPerSecond);
	using Kind = creature_mind::WalkLookAt::Kind;
	switch (look.kind)
	{
	case Kind::Nothing:
		break;
	case Kind::Point:
		return glm::vec3 {to.x, GroundAt(to, point.y), to.y};
	case Kind::Interesting:
		return mind.look.point;
	case Kind::Sweep:
	{
		const auto above = at.position.y - GroundAt(from, at.position.y);
		return glm::vec3 {look.sweep.x, GroundAt(look.sweep, at.position.y) + above, look.sweep.y};
	}
	}
	return std::nullopt;
}

/// A mind made without the creature archetype (as tests make them) is given what a new creature draws, as the
/// archetype does. Without the game's tables the desires start as their defaults, with no draw
void MakeMind(CreatureType species, CreatureMindState& mind)
{
	if (!Locator::infoConstants::has_value())
	{
		mind.desires = creature_desires::Create(SetupFor(species), [](float low, float) { return low; });
		return;
	}
	const creature_creation::Random random {
	    .rand = [](uint32_t range) { return game_random::GameRand(range); },
	    .floatRand = [](float range) { return game_random::GameFloatRand(range); },
	};
	auto made = creature_creation::Make(creature_creation::SetupFor(Locator::infoConstants::value(), species), random);
	mind.desires = std::move(made.desires);
	mind.personality = made.personality;
	mind.previousActions = std::move(made.previousActions);
}
} // namespace

void CreatureMindSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto random = [](uint32_t range) { return game_random::GameRand(range); };
	std::optional<std::vector<creature_look::Candidate>> candidates;
	const bool night = IsNight();
	// The creatures passing out this turn, which are knocked out once every mind has thought
	std::vector<entt::entity> passingOut;
	// Whether the cinema bars are in for a cut scene, and which script task brought them
	const auto* helpSystem = openblack::help::Get();
	const bool barsIn = helpSystem != nullptr && helpSystem->GetWideScreen() != 0;
	const auto barsOwner = helpSystem != nullptr ? helpSystem->GetWideScreenOwner() : 0u;
	const auto* lookTables = GetTables();
	const auto cutSceneLook = lookTables != nullptr
	                              ? creature_mind_tables::FindAction(*lookTables, creature_mind::k_CutSceneLookAction)
	                              : std::nullopt;

	std::vector<entt::entity> creatures;
	lookup.Each<const Creature, const CreatureMindState, const CreatureAnimation, const Transform>(
	    [&creatures](entt::entity entity, const auto&...) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		// As every Living's, the reaction it follows ends first if it is over
		creature_reactions::ProcessReaction(entity);
		const auto& creature = lookup.Get<Creature>(entity);
		const auto& transform = lookup.Get<Transform>(entity);
		auto& mind = registry.Get<CreatureMindState>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		[&]() {
			SetUpMind(entity, creature.species, mind);
			if (mind.pendingFile != nullptr)
			{
				TakeUpFile(entity, mind);
			}
			if (mind.scriptName.has_value() && mind.learnt.has_value())
			{
				mind.learnt->name = std::move(*mind.scriptName);
				mind.scriptName.reset();
			}
			if (!mind.scriptKnows.empty() && mind.learnt.has_value() && mind.pendingFile == nullptr)
			{
				TakeUpScriptKnows(mind.scriptKnows, GetTables(),
				                  creature_mind_tables::MiracleMultiplier(creature::InfoRow(creature.species)), *mind.learnt);
				mind.scriptKnows.clear();
			}
			++mind.turn;
			// It notes the region of the land where it stands, for its map of the places it went to, unless the hand
			// holds it
			if (!(Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == entity))
			{
				ecs::creature_exploration_land::Visit(entity, {transform.position.x, transform.position.z});
			}
			creature_perceived_desires::Fade(mind.perceivedDesires);
			if (mind.learnt.has_value())
			{
				creature_learning::Age(mind.learnt->contexts, k_TurnSeconds);
				for (auto& turns : mind.learnt->turnsSinceDone)
				{
					turns = std::min(turns + 1, creature_planner::k_NoveltyTurns);
				}
			}
			if (mind.desiresPhase != mind.developmentPhase)
			{
				creature_desires::ActivateForPhase(*mind.desires, Phases(), mind.developmentPhase);
				mind.desiresPhase = mind.developmentPhase;
			}
			mind.secondsAlone += k_TurnSeconds;
			if (mind.feedbackSeconds.has_value())
			{
				*mind.feedbackSeconds += k_TurnSeconds;
			}
			// A desire made dominant for a while is let go of again each turn, until its time is up
			const auto msPerTurn = game_clock::MsPerTurn();
			const auto dominantRate = msPerTurn > 0 ? static_cast<float>(1000 / msPerTurn) : 0.0f;
			static_cast<void>(creature_spell_mind::StepCheat(*mind.desires, mind.dominantDesire, dominantRate));
			auto* needs = registry.TryGet<CreatureNeeds>(entity);
			const auto body = needs != nullptr ? needs->needs : creature_physiology::Needs {};
			// Under a script's control its desires are left as they were, and it plans nothing: it only carries out
			// what the script gave it
			const bool scripted = ecs::script_held::IsControlledByScript(entity);
			if (!scripted)
			{
				creature_desires::UpdateSources(*mind.desires,
				                                [&mind, &body, night](uint32_t type, const creature_desires::Desires& desires) {
					                                return ReadSource(type, desires, mind, body, night);
				                                });
				creature_desires::UpdateDesires(*mind.desires, k_TurnsPerSecond);
			}

			auto* eyes = registry.TryGet<CreatureEyes>(entity);
			// It sits only where the mind says so below
			if (needs != nullptr)
			{
				needs->sitting = false;
			}
			// Paused, following the leash to the hand, fighting or knocked out, the mind leaves the body alone
			if (mind.paused || mind.leash.obeying || lookup.AnyOf<CreatureFighting, CreatureKnockedOut>(entity))
			{
				return;
			}
			// Exhausted, starved or out of life, it passes out as a fight's loser does: it lies where it fell, then a
			// player's creature is carried to its pen to come round. A creature just put in the world stands up first.
			const bool fainting = needs != nullptr && needs->faint.has_value();
			if (fainting && Locator::creatureFightSystem::has_value())
			{
				if (mind.turn > k_TurnsBeforePassingOut)
				{
					needs->faint.reset();
					passingOut.push_back(entity);
				}
				return;
			}
			// Without the fights' knock-outs, it drops where it stands and comes round there
			if (fainting && !creature_mind::IsUnconscious(mind.idle))
			{
				if (Locator::creatureLocomotionSystem::has_value())
				{
					Locator::creatureLocomotionSystem::value().Stop(entity);
				}
				animation.body = {};
				creature_mind::Plan(mind.idle, creature_mind::Activity::Faint, creature_mind::Faint());
				needs->faint.reset();
			}
			// Free to choose what to do next, it weighs every desire's plans first; the idle policy only chooses when
			// none is pressing enough
			if (!scripted && !mind.planActive && mind.idle.step >= mind.idle.agenda.size() &&
			    !(needs != nullptr && needs->faint.has_value()))
			{
				// A try at a miracle that fizzled, or what a stroke or slap left it to do, is what it does first
				ShowFizzle(entity, mind);
				DoAfterFeedback(entity, mind);
				if (mind.idle.step >= mind.idle.agenda.size())
				{
					PlanCreature(entity, mind);
				}
			}
			// A script's cut scene makes every creature free to think for itself look at the camera, after whatever it
			// chose this turn, and again each turn it is doing anything else, until the cut scene is over; the look it
			// is in then plays out to its end
			if (!scripted && cutSceneLook.has_value() &&
			    creature_mind::StartsLookAtCameraInCutScene(barsIn, barsOwner,
			                                                mind_detail::CurrentAction(mind, lookTables) == cutSceneLook))
			{
				std::ignore = ForceActivity(entity, {.desire = creature_desires::Desire::ObeyPlayer,
				                                     .action = creature_mind::k_CutSceneLookAction,
				                                     .actionObject = entity,
				                                     .score = k_CutSceneLookScore});
			}
			const bool moving =
			    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(entity);
			// The turns of a step count while it has one; a going near, getting away or turning goes on a turn
			if (mind.idle.step < mind.idle.agenda.size())
			{
				++mind.stepTurns;
			}
			StepSubMove(entity, creature_layers::IsPlaying(animation.body));
			// A catch takes up the body until it is over
			const bool catching = Locator::creatureObjectActionSystem::has_value() &&
			                      Locator::creatureObjectActionSystem::value().IsCatching(entity);
			const creature_mind::Senses senses {
			    .seconds = k_TurnSeconds,
			    .turnsPerSecond = game_clock::MsPerTurn() != 0 ? 1000u / game_clock::MsPerTurn() : 0u,
			    .bodyBusy = creature_layers::IsPlaying(animation.body) || moving || catching,
			    .bodyLooping = creature_layers::IsLooping(animation.body),
			    .moving = moving,
			    .subMove = SubMoveOf(entity),
			    .position = glm::vec2(transform.position.x, transform.position.z),
			    .strongest = creature_desires::StrongestShowable(*mind.desires, creature_mind::k_MinDesireShown),
			    .feedbackSeconds = mind.feedbackSeconds,
			    .feedbackWasStroke = mind.feedbackWasStroke,
			    // Food and water are only looked for when it is free to choose what to do next
			    .wants = mind.idle.step >= mind.idle.agenda.size()
			                 ? WantsOf(lookup, entity, *mind.desires, glm::vec2(transform.position.x, transform.position.z),
			                           [&lookup, &mind, entity](entt::entity food) {
				                           return FoodUsefulness(lookup, mind, entity, food);
			                           })
			                 : creature_mind::Wants {},
			    .rested = needs != nullptr && needs->rested,
			    .hands = HandsOf(entity),
			    .objectInMap = WaitedForInMap(lookup, mind.idle),
			    .camera = Locator::camera::has_value() ? std::optional(glm::vec2(Locator::camera::value().GetOrigin().x,
			                                                                     Locator::camera::value().GetOrigin().z))
			                                           : std::nullopt,
			    .cameraDistance = Locator::camera::has_value()
			                          ? glm::distance(transform.position, Locator::camera::value().GetOrigin())
			                          : 0.0f,
			    .hasPlayer = creature.owner != PlayerNames::NEUTRAL,
			    .handStillTurns = Locator::handSystem::has_value() ? Locator::handSystem::value().GetStillTurns() : 0u,
			    .heading =
			        lookup.AllOf<CreatureLocomotion>(entity) ? lookup.Get<const CreatureLocomotion>(entity).heading : 0.0f,
			    .feelings = FeelingsOf(mind),
			    .choosesOwnActivity = _idlePolicy,
			    .overlayPlaying = animation.gesture.animation.has_value(),
			};
			// A sit counting its time rests the body this turn, the turn it is asked to end included
			const bool sitCounting = creature_mind::SitCounting(mind.idle);
			const auto commands = creature_mind::Think(mind.idle, senses, random);
			Apply(commands, animation, eyes);
			Move(entity, commands);
			if (commands.move.has_value() && IsSubMove(commands.move->kind) && mind.idle.step < mind.idle.agenda.size())
			{
				StartSubMove(entity, *commands.move, mind.idle.agenda[mind.idle.step].seconds);
			}
			if (commands.releaseCast && Locator::magicSystem::has_value())
			{
				Locator::magicSystem::value().ReleaseCreatureCast(entity);
			}
			// The miracle is tried as the casting pose's loop begins; the rest of the agenda is no use without it
			if (commands.cast.has_value() && commands.cast->object.has_value() &&
			    !TryMiracle(entity, static_cast<MagicType>(commands.cast->magicType),
			                static_cast<entt::entity>(*commands.cast->object)))
			{
				mind.idle.step = mind.idle.agenda.size();
				mind.idle.stepStarted = false;
				mind.idle.gaveUp = true;
			}
			Order(lookup, entity, commands);
			TakeEffect(entity, commands, *mind.desires);
			// Eating charged the body for its action already, which the plan's end then does not
			if (commands.effect == creature_mind::Effect::Eat)
			{
				mind.satisfiedByEffect = true;
			}
			if (commands.effect == creature_mind::Effect::Completed)
			{
				CompleteByStep(entity, mind);
			}
			FollowAgenda(entity, mind);
			// Once it has taken what the hand gave it, its desires choose what to do with it
			if (commands.effect == creature_mind::Effect::TookFromHand && commands.effectObject.has_value())
			{
				UseGift(entity, static_cast<entt::entity>(*commands.effectObject));
			}
			// The script's turn over, the plan it gave is done with
			if (!mind.planActive && mind.idle.step >= mind.idle.agenda.size() && mind.planner.current.has_value() &&
			    mind.planner.current->desire == Desire::ObeyPlayer &&
			    mind.planner.current->action == creature_planner::k_FollowPlayerAction)
			{
				mind.planner.current.reset();
			}
			if (needs != nullptr)
			{
				needs->rest = creature_mind::IsUnconscious(mind.idle) ? CreatureNeeds::Rest::Unconscious
				              : creature_mind::IsAsleep(mind.idle)    ? CreatureNeeds::Rest::Asleep
				                                                      : CreatureNeeds::Rest::Awake;
				// A sit rests the body once a turn while it counts its time, after the body's own turn
				needs->sitting = sitCounting;
				if (needs->sitting && Locator::creaturePhysiologySystem::has_value())
				{
					Locator::creaturePhysiologySystem::value().Sit(entity);
				}
			}

			// Looking about, the head turns to the most interesting thing in sight, or ahead
			const auto lookAbout = [&]() {
				if (!candidates.has_value())
				{
					candidates = GatherCandidates(lookup);
				}
				const auto self = entt::to_integral(entity);
				std::vector<creature_look::Candidate> others;
				others.reserve(candidates->size());
				std::ranges::copy_if(*candidates, std::back_inserter(others),
				                     [self](const creature_look::Candidate& candidate) { return candidate.id != self; });
				const creature_look::Viewer viewer {
				    .position = transform.position,
				    .ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f)),
				    .size = creature.size,
				};
				mind.look = creature_look::LookAbout(mind.look, others, viewer, k_TurnsPerSecond);
				return mind.look.id.has_value() ? mind.look.point : creature_look::PointAhead(viewer);
			};
			// A step with something to look at has the head on it; one whose look gives nothing this turn, or that has
			// no look of its own, looks about. On the way to a point it looks about each turn, as its look there takes
			// what it watches in turn with the point and a sweep
			std::optional<glm::vec3> gazed;
			std::optional<glm::vec3> about;
			if (commands.gaze.has_value() && commands.gaze->goingTowards && commands.gaze->point.has_value())
			{
				about = lookAbout();
				gazed = WalkLookPoint(lookup, entity, mind, *commands.gaze->point, transform, senses.turnsPerSecond);
			}
			else if (commands.gaze.has_value())
			{
				gazed = GazePoint(lookup, entity, *commands.gaze,
				                  Locator::creatureLocomotionSystem::has_value() &&
				                      Locator::creatureLocomotionSystem::value().IsMoving(entity));
			}
			mind.lookingAbout = !gazed.has_value() && (commands.lookAbout || commands.gaze.has_value());
			if (gazed.has_value())
			{
				animation.lookAt = gazed;
			}
			else if (mind.lookingAbout)
			{
				animation.lookAt = about.has_value() ? *about : lookAbout();
			}
			else
			{
				animation.lookAt.reset();
			}
			// A script's focus is where its head looks, over anything else
			if (mind.scriptFocus.has_value())
			{
				animation.lookAt = mind.scriptFocus;
			}
			if (eyes != nullptr)
			{
				eyes->lookAt = animation.lookAt;
			}
		}();
	}
	for (const auto entity : passingOut)
	{
		if (Locator::creatureFightSystem::has_value())
		{
			Locator::creatureFightSystem::value().KnockOut(entity);
		}
	}
}

bool CreatureMindSystem::PlayAction(entt::entity creature, size_t animation, std::optional<bool> mirrored)
{
	auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return false;
	}
	// Played left to right or not by a coin the game tosses, unless the caller says
	const bool flipped = mirrored.has_value() ? *mirrored : game_random::GameRand(2) == 1;
	const auto played = creature_layers::PlayOnce(body->body, animation, flipped);
	if (played.has_value())
	{
		body->body = *played;
	}
	return played.has_value();
}

bool CreatureMindSystem::PlayGesture(entt::entity creature, size_t animation)
{
	auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature);
	if (body == nullptr)
	{
		return false;
	}
	const auto played = creature_layers::PlayGesture(body->gesture, animation);
	if (played.has_value())
	{
		body->gesture = *played;
	}
	return played.has_value();
}

void CreatureMindSystem::PullFace(entt::entity creature, size_t animation)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* body = registry.TryGet<CreatureAnimation>(creature))
	{
		body->face = creature_layers::PullFace(body->face, animation, k_ToldFaceMs, creature_face::Cue::Told);
	}
}

std::optional<creature_face::Request> CreatureMindSystem::ShowFeeling(entt::entity creature, creature_face::Cue cue)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr)
	{
		return std::nullopt;
	}
	const auto random = [](uint32_t range) { return game_random::GameRand(range); };
	const creature_mind::Senses senses {.feelings = FeelingsOf(*mind)};
	const auto face = creature_mind::PullFace(mind->idle, cue, senses, random);
	if (face.has_value())
	{
		body->face = creature_layers::PullFace(body->face, creature_face::AnimationOf(face->face), face->milliseconds, cue);
	}
	return face;
}

bool CreatureMindSystem::SitDown(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr || creature_layers::IsPlaying(body->body))
	{
		return false;
	}
	// A paused mind won't get it up again: it sits until told to stand
	if (mind->paused)
	{
		body->body =
		    *creature_layers::PlaySequence(body->body, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
		return true;
	}
	const auto random = [](uint32_t range) { return game_random::GameRand(range); };
	Abandon(*mind);
	creature_mind::Plan(mind->idle, creature_mind::Activity::Told, {creature_mind::SitDown(random)});
	return true;
}

void CreatureMindSystem::UpdateAttitudeFromFeedback(entt::entity creature, float feedback)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// Good feedback shows it its player wants compassion, bad anger, by how much, and it warms or cools to the player
	creature_perceived_desires::Increase(
	    mind->perceivedDesires, static_cast<int32_t>(feedback > 0.0f ? CreatureDesires::Compassion : CreatureDesires::Anger),
	    std::abs(feedback));
	mind->attitudeToPlayer = creature_feedback::AttitudeAfter(mind->attitudeToPlayer, feedback);
	mind->averageFeedback = creature_feedback::AverageAfter(mind->averageFeedback, feedback);
}

void CreatureMindSystem::ChangeDesireSource(entt::entity creature, uint32_t type, float amount)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind != nullptr && mind->desires.has_value())
	{
		creature_desires::ChangeSource(*mind->desires, type, amount);
	}
}

void CreatureMindSystem::ReceiveFeedback(entt::entity creature, float feedback)
{
	namespace sources = creature_desires::sources;
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	mind->secondsAlone = 0.0f;
	mind->lastFeedback = CreatureMindState::Feedback {.value = feedback, .activity = mind->idle.activity};
	// Stroked or slapped, however lightly, it first stops whatever it was doing: the action, the walk, its agenda and
	// the plan it carried out, what it learns from left as it was
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	mind->idle.agenda.clear();
	mind->idle.step = 0;
	mind->idle.stepStarted = false;
	mind->planActive = false;
	mind->planner.current.reset();
	// Too slight to count, it only looks at the player, idling about with it
	if (std::abs(feedback) <= creature_feedback::k_SlightFeedback)
	{
		std::ignore = ForceActivity(
		    creature, {.desire = creature_desires::Desire::IdleWithPlayer, .action = "LookAtCamera", .score = 1.0f});
		return;
	}
	// A stroke shows it its player wants compassion, a slap anger, by how hard
	UpdateAttitudeFromFeedback(creature, feedback);
	mind->feedbackSeconds = 0.0f;
	mind->feedbackWasStroke = feedback > 0.0f;
	LearnFromFeedback(creature, *mind, feedback);
	// It shows how it feels as soon as it is free, unless it is doing something again as it was stroked for
	mind->idle.showDesireSeconds = 0.0f;
	if (!mind->planActive && mind->idle.activity != creature_mind::Activity::ShowDesire)
	{
		mind->idle.agenda.resize(std::min(mind->idle.agenda.size(), mind->idle.step + (mind->idle.stepStarted ? 1 : 0)));
	}
}

void CreatureMindSystem::SetUpMind(entt::entity creature, CreatureType species, CreatureMindState& mind)
{
	if (!mind.desires.has_value())
	{
		MakeMind(species, mind);
	}
	if (!mind.learnt.has_value())
	{
		SetUpLearning(creature, mind);
	}
}

void CreatureMindSystem::StopWhatItWasDoing(entt::entity creature, CreatureMindState& mind, bool stopWalking)
{
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (stopWalking && Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
	mind.planActive = false;
	mind.planner.current.reset();
	// Its agenda cleared, the pass over its desires starts again, and nothing it does has achieved its aim yet
	if (mind.desires.has_value())
	{
		mind.desires->achieved = false;
		creature_planner::StartPass(mind.planner, creature_planner::PassDesires(*mind.desires));
	}
}

void CreatureMindSystem::FocusByScript(entt::entity creature, const glm::vec3& point)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// Taken under the script's control, it gives up what it was doing and obeys: it turns to face the point, its head
	// looking there from now on, as the plan of obeying its player, completed once the turn is over
	mind->scriptFocus = point;
	static_cast<void>(CarryOutForScript(creature, *mind, creature_mind::TurnToFacePoint(glm::vec2(point.x, point.z)), true));
}

void CreatureMindSystem::PrepareForScript(entt::entity creature, CreatureMindState& mind, bool stopWalking)
{
	if (!ecs::script_held::IsControlledByScript(creature))
	{
		ecs::script_held::SetControlledByScript(creature, true);
	}
	StopWhatItWasDoing(creature, mind, stopWalking);
	// A miracle it holds is let go of, and a sit or a casting pose it is in ends there and then: the end plays now, or
	// once the start has played
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	if (auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature); body != nullptr)
	{
		body->body = creature_layers::EndLoop(body->body);
	}
	mind.planner.current = creature_planner::Plan {.desire = Desire::ObeyPlayer,
	                                               .action = creature_planner::k_FollowPlayerAction,
	                                               .object = entt::to_integral(creature),
	                                               .goalUsefulness = creature_planner::k_ForcedScore,
	                                               .actionPriority = creature_planner::k_ForcedScore,
	                                               .priority = creature_planner::k_ForcedScore};
}

bool CreatureMindSystem::CarryOutForScript(entt::entity creature, CreatureMindState& mind,
                                           std::vector<creature_mind::Step> agenda, bool stopWalking)
{
	if (agenda.empty())
	{
		return false;
	}
	PrepareForScript(creature, mind, stopWalking);
	creature_mind::Plan(mind.idle, creature_mind::Activity::Told, std::move(agenda));
	// The agenda is the plan's: once over, it was carried out
	mind.planActive = true;
	mind.planSerial = mind.idle.serial;
	mind.agendaSeen = mind.idle.serial;
	mind.satisfiedByEffect = false;
	mind.completedByStep = false;
	return true;
}

void CreatureMindSystem::SetScriptPlay(entt::entity creature, uint32_t animation, uint32_t plays)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature); mind != nullptr)
	{
		mind->scriptPlay = {.animation = animation, .plays = plays};
	}
}

bool CreatureMindSystem::PlayForScript(entt::entity creature)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return false;
	}
	auto agenda = creature_script_play::Agenda(mind->scriptPlay);
	return agenda.has_value() && CarryOutForScript(creature, *mind, std::move(*agenda), true);
}

bool CreatureMindSystem::MoveForScript(entt::entity creature, const glm::vec3& point, float distance)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (mind == nullptr || transform == nullptr)
	{
		return false;
	}
	// Both legs go by the creature's height, as it is drawn
	const auto height = ecs::object::GetHeight(creature);
	auto agenda =
	    creature_mind::MoveForScript(glm::vec2(transform->position.x, transform->position.z), point, height, distance);
	// The game leaves the walk it was on to the new one; a walk step here only starts once the body is still, so the old
	// walk is stopped for the new one to start at once
	return CarryOutForScript(creature, *mind, std::move(agenda), true);
}

bool CreatureMindSystem::HasPlayed(entt::entity creature)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return true;
	}
	if (!creature_script_play::Played(mind->idle))
	{
		return false;
	}
	// Only being idle, what is left of its agenda is given up, so that it does not count as carried out
	auto& idle = mind->idle;
	if (idle.step < idle.agenda.size())
	{
		idle.agenda.resize(idle.step);
		idle.stepStarted = false;
		idle.gaveUp = true;
	}
	return true;
}

void CreatureMindSystem::AbandonAction(entt::entity creature)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	StopWhatItWasDoing(creature, *mind);
	// A cast it was making is let go of with it
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
}

void CreatureMindSystem::ReleasedByScript(entt::entity creature)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	mind->scriptFocus.reset();
	StopWhatItWasDoing(creature, *mind);
}

bool CreatureMindSystem::ForceAction(entt::entity creature, size_t animation, bool mirrored,
                                     std::optional<creature_face::Request> face, double interruptsAfter)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (body == nullptr || mind == nullptr || creature_mind::IsUnconscious(mind->idle))
	{
		return false;
	}
	// Not while it is only just into something
	if (Locator::creatureObjectActionSystem::has_value())
	{
		if (const auto progress = Locator::creatureObjectActionSystem::value().GetProgress(creature);
		    progress.has_value() && static_cast<double>(*progress) < interruptsAfter)
		{
			return false;
		}
	}
	// nor while its body's action, as far as the turns have played it, is no further through than the share
	if (creature_layers::IsPlaying(body->body) && Locator::creatureAnimationSystem::has_value())
	{
		const auto duration = Locator::creatureAnimationSystem::value().AnimationDuration(
		    creature, creature_layers::CurrentAnimation(body->body));
		if (duration.has_value() && *duration > 0.0f &&
		    !creature_feedback::MayInterrupt(body->body.timeMs, *duration, interruptsAfter))
		{
			return false;
		}
	}
	const auto played = creature_layers::PlayOnce(creature_layers::BodyAction {}, animation, mirrored);
	if (!played.has_value())
	{
		return false;
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	body->body = *played;
	if (face.has_value())
	{
		body->face =
		    creature_layers::PullFace(body->face, creature_face::AnimationOf(face->face), face->milliseconds, face->cue);
	}
	// What it was doing is over; it decides afresh once the action has played
	Abandon(*mind);
	mind->idle.agenda.resize(std::min(mind->idle.agenda.size(), mind->idle.step));
	mind->idle.stepStarted = false;
	return true;
}

bool CreatureMindSystem::Replan(entt::entity creature, creature_mind::Activity activity,
                                std::vector<creature_mind::Step> agenda)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	auto* body = registry.TryGet<CreatureAnimation>(creature);
	if (mind == nullptr || body == nullptr || agenda.empty())
	{
		return false;
	}
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	// Whatever it plays ends at once, as the game ends a creature's animations when it changes what it does
	body->body = {};
	if (auto* eyes = registry.TryGet<CreatureEyes>(creature))
	{
		ApplyEyes(eyes, creature_mind::Eyes::Normal);
	}
	// Stopping what it does lets go of a miracle it holds
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	Abandon(*mind);
	creature_mind::Plan(mind->idle, activity, std::move(agenda));
	return true;
}

bool CreatureMindSystem::PlansAction(entt::entity creature, std::string_view action)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	const auto* tables = GetTables();
	if (mind == nullptr || tables == nullptr || !mind->planner.current.has_value() ||
	    mind->planner.current->action >= tables->actions.size())
	{
		return false;
	}
	return tables->actions[mind->planner.current->action].name == action;
}

bool CreatureMindSystem::IsAvailableForReaction(entt::entity creature, uint8_t reaction)
{
	// The test reads the reactions' table, which comes with the game's data
	return Locator::infoConstants::has_value() && creature_reactions::IsAvailableForReaction(creature, reaction);
}

bool CreatureMindSystem::Sleep(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Sleep,
	              creature_mind::Sleep([](uint32_t range) { return game_random::GameRand(range); }));
}

bool CreatureMindSystem::Eat(entt::entity creature, std::optional<entt::entity> food)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (!food.has_value() && transform != nullptr)
	{
		if (const auto nearest = NearestFood(registry, creature, glm::vec2(transform->position.x, transform->position.z), {}))
		{
			food = nearest->first;
		}
	}
	if (!food.has_value() || !FoodValueOf(*food).has_value())
	{
		return false;
	}
	return Replan(creature, creature_mind::Activity::Eat, creature_mind::Eat(entt::to_integral(*food)));
}

bool CreatureMindSystem::Drink(entt::entity creature)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(creature);
	if (transform == nullptr)
	{
		return false;
	}
	const auto water = NearestWater(glm::vec2(transform->position.x, transform->position.z));
	return water.has_value() &&
	       Replan(creature, creature_mind::Activity::Drink, creature_mind::Drink(water->shore, water->water));
}

bool CreatureMindSystem::Poo(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Poo,
	              creature_mind::Poo([](uint32_t range) { return game_random::GameRand(range); }));
}

bool CreatureMindSystem::Puke(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Puke, creature_mind::Puke());
}

void CreatureMindSystem::EndMeal(entt::entity creature, uint32_t objectType, float foodValue, creature_eating::MealEnd end)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr || !mind->desires.has_value())
	{
		return;
	}
	auto* counts = mind->learnt.has_value() && objectType < mind->learnt->objectTypes.size()
	                   ? &mind->learnt->objectTypes.at(objectType)
	                   : nullptr;
	auto& desires = *mind->desires;
	if (end == creature_eating::MealEnd::Eaten)
	{
		if (counts != nullptr)
		{
			++counts->eaten;
		}
		if (Locator::creaturePhysiologySystem::has_value())
		{
			auto& hunger = desires[Desire::Hunger].value;
			hunger = Locator::creaturePhysiologySystem::value().HungerAfterMeal(creature, hunger, foodValue);
		}
		return;
	}
	// Not food, or poisoned: it is held back from hunger for a while and is sick; only what is not food teaches it so
	creature_desires::Suppress(desires, Desire::Hunger, k_MealRefusedSuppressSeconds, k_TurnsPerSecond);
	if (end == creature_eating::MealEnd::NotFood && counts != nullptr)
	{
		++counts->notFood;
	}
	Puke(creature);
}

bool CreatureMindSystem::Faint(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Faint, creature_mind::Faint());
}

void CreatureMindSystem::Wake(entt::entity creature)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature))
	{
		mind->idle.wakeWanted = true;
	}
}

void CreatureMindSystem::StandUp(entt::entity creature)
{
	if (auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature))
	{
		body->body = creature_layers::EndLoop(body->body);
	}
}

void CreatureMindSystem::FoughtFight(entt::entity creature, [[maybe_unused]] bool won)
{
	// Winning or losing, the fight is done. The game also has the loser learn from losing, which isn't known.
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind != nullptr && mind->desires.has_value())
	{
		Satisfied(creature, *mind->desires, k_FightAction);
	}
}
