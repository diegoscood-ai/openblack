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

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Common/HelpText.h"
#include "Creature/CreatureAttention.h"
#include "Creature/CreatureBodyActivity.h"
#include "Creature/CreatureCreation.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFart.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreatureOddAgendas.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureThrow.h"
#include "Creature/LocalPlayer.h"
#include "Creature/PerceivedDesires.h"
#include "CreatureMindSystemDetail.h"
#include "CreatureMindTownHelp.h"
#include "CreatureReactions.h"
#include "ECS/Abodes.h"
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
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureExplorationLand.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreaturePose.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/InTheAir.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Help/ToolTips.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "Physics/TurnRules.h"

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

/// How far from home a creature at a stage of growing up may go before it most wants to go home, from the game's
/// table of stages; the last stage's past its end, and none without the table
float GoHomeDistance(uint32_t developmentPhase)
{
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& phases = Locator::infoConstants::value().creatureDevelopmentPhaseEntry;
	return phases.at(std::min<size_t>(developmentPhase, phases.size() - 1)).goHomeDistance;
}

/// How far a creature stands from its home across the land, none without a home
std::optional<float> HomeDistance(const ecs::Registry& registry, entt::entity creature, const glm::vec3& position)
{
	const auto home = ecs::creature_home::HomeOf(registry, creature);
	if (!home.has_value())
	{
		return std::nullopt;
	}
	return glm::distance(glm::vec2(position.x, position.z), glm::vec2(home->x, home->z));
}

/// The sources that follow the creature's state: its body's, its mind's own (the wish to impress villagers among them)
/// and how far it is from home
std::optional<float> ReadSource(uint32_t type, const creature_desires::Desires& desires, const CreatureMindState& mind,
                                const creature_physiology::Needs& needs, bool night, std::optional<float> homeDistance)
{
	namespace sources = creature_desires::sources;
	if (const auto value = creature_physiology::SourceValue(type, needs, night))
	{
		return value;
	}
	switch (type)
	{
	case sources::k_ImpressFromSeeingVillagers:
		return desires.impressVillagers;
	case sources::k_GoHome:
	case sources::k_HangAroundAtHome:
		return creature_desires::HomeSource(type, homeDistance, GoHomeDistance(mind.developmentPhase));
	case sources::k_AttentionFromLoneliness:
		return creature_attention::Loneliness(mind.turnsNotLookedAt, game_clock::MsPerTurn());
	case sources::k_AttentionFromLackOfInteraction:
		return creature_attention::LackOfInteraction(mind.turnsSinceMadeToAct, game_clock::MsPerTurn());
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
	eyes->mode = look == creature_mind::Eyes::Closed   ? creature_eyes::Mode::Closed
	             : look == creature_mind::Eyes::Wide   ? creature_eyes::Mode::Wide
	             : look == creature_mind::Eyes::Stoned ? creature_eyes::Mode::Stoned
	                                                   : creature_eyes::Mode::Calm;
	// Stoned or wide eyes only change how the eyes look: the lids and the blinking stay as they were
	if (look == creature_mind::Eyes::Stoned || look == creature_mind::Eyes::Wide)
	{
		return;
	}
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
	if (commands.brakeToStop)
	{
		locomotion.BrakeToStop(creature);
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
		// A turn asked for again every turn is not started again while one is under way
		if (!move.whenFree || !locomotion.IsMoving(creature))
		{
			locomotion.TurnToFace(creature, move.point);
		}
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
	case Kind::ToThrowPosition:
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

/// Whether the creature that the creature's step waits to be still is on its way somewhere or turning. A thing that is
/// gone, or is not a creature, does not move.
bool WaitedForMoving(const ecs::Registry& registry, const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WaitForStill ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return false;
	}
	const auto object = static_cast<entt::entity>(*idle.agenda[idle.step].object);
	return registry.Valid(object) && registry.AllOf<Creature>(object) && Locator::creatureLocomotionSystem::has_value() &&
	       Locator::creatureLocomotionSystem::value().IsMoving(object);
}

/// The movements the mind oversees turn by turn, done or failed by what the creature's movement says
bool IsSubMove(creature_mind::Movement::Kind kind)
{
	using Kind = creature_mind::Movement::Kind;
	return kind == Kind::GoNearObject || kind == Kind::GetAwayFromObject || kind == Kind::TurnToFaceObject ||
	       kind == Kind::ToThrowPosition;
}

void Apply(const creature_mind::Commands& commands, CreatureAnimation& animation, CreatureEyes* eyes)
{
	if (commands.endSit || commands.endStaticLoop)
	{
		animation.body = creature_layers::EndLoop(animation.body);
	}
	if (commands.endDance)
	{
		animation.body = creature_layers::EndDance(animation.body);
	}
	if (commands.startDance.has_value())
	{
		if (auto body = creature_layers::PlayDance(animation.body, *commands.startDance))
		{
			animation.body = *body;
		}
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
	if (eyes != nullptr && commands.blinkIntervalMs.has_value())
	{
		eyes->blink.intervalMs = *commands.blinkIntervalMs;
	}
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

/// A fart where the poo would drop: a cloud of grey puffs blown out behind the creature, from where it stands, on the
/// game's synchronised random numbers. Its poo, its poo desire and its count of poos stay as they were. An evil
/// creature's fart makes those near it faint
void Fart(entt::entity creature)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const Creature>(creature);
	if (transform == nullptr || self == nullptr)
	{
		return;
	}
	const auto size = ecs::creature_pose::DrawnSize(registry, creature);
	const creature_fart::FloatRandom random = [](float bound) { return game_random::GameFloatRand(bound); };
	const auto velocities = creature_fart::PuffVelocities(BodyYaw(registry, creature, *transform), size, random);
	// Without a physics (a test's world) the numbers are drawn all the same
	if (Locator::dynamicsSystem::has_value())
	{
		auto& dynamics = Locator::dynamicsSystem::value();
		for (const auto& velocity : velocities)
		{
			dynamics.AddPuff(transform->position, velocity, creature_fart::PuffSize(size), creature_fart::k_PuffColour,
			                 openblack::physics::turn::PuffKind::Dust);
		}
	}
	// The fainting is a reaction of the creature's, for its own player, which grows from where it stands. Without the
	// reactions (a test's world) none is made
	if (creature_fart::MakesFaint(ecs::creature_pose::TargetMorph(registry, creature).evilGood) &&
	    Locator::reactionSystem::has_value())
	{
		effects::reactions::CreateReaction(creature, openblack::Reaction::Fainting, self->owner, false);
	}
}

/// What a step did to the body. `farts`: the poo's moment is a fart, the plan being the fart
void TakeEffect(entt::entity creature, const creature_mind::Commands& commands, creature_desires::Desires& desires, bool farts)
{
	using creature_mind::Effect;
	// A fart takes nothing from the body
	if (commands.effect == Effect::Poo && farts)
	{
		Fart(creature);
		return;
	}
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
	// Having looked another creature over, it notes when (ProcessTurn)
	case Effect::Inspected:
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
	// the drops of sick are the animation's: they come out each turn its puke plays (CreatureAnimationSystem)
	case Effect::Puke:
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
		// by its animation, as the game's discard: the put-down's lays it down where it stands, any other tosses it
		if (creature_object_actions::DiscardPutsDown(order.animation))
		{
			hands.PutDown(creature);
		}
		else
		{
			hands.Discard(creature);
		}
		break;
	case Kind::Eat:
		hands.EatHeld(creature);
		break;
	case Kind::Keep:
		hands.Keep(creature, order.animation);
		break;
	case Kind::Throw:
	case Kind::ThrowNearby:
	{
		// At a thing: at where it will be after the time to where it is, then after the time to there. A creature is
		// aimed at where its walk takes it, a villager ahead along its step, anything else at its middle where it is.
		const auto* thing = object.has_value() && registry.Valid(*object) ? registry.TryGet<const Transform>(*object) : nullptr;
		const auto* self = registry.TryGet<const Transform>(creature);
		if (thing != nullptr && self != nullptr)
		{
			const auto groundAt = [](glm::vec2 at) {
				return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
			};
			const auto height = object_measures::Height(registry, *object);
			creature_throw::ThrowAim aim;
			if (registry.AllOf<Creature>(*object))
			{
				// Its speed and heading as its walk keeps them; one whose walk has not started stands still
				const auto* walk = registry.TryGet<const CreatureLocomotion>(*object);
				const bool walking = walk != nullptr && walk->started;
				const float speed = walking ? walk->speed : 0.0f;
				const float heading = walking ? walk->heading : 0.0f;
				aim = creature_throw::AimAhead(self->position, thing->position, [&](float seconds) {
					return creature_throw::CreatureAfter(thing->position, heading, speed, height, seconds, groundAt);
				});
			}
			else
			{
				const auto groundThere = groundAt({thing->position.x, thing->position.z});
				const auto* hug = registry.AllOf<Villager>(*object) ? registry.TryGet<const WallHug>(*object) : nullptr;
				if (hug != nullptr)
				{
					// Led along its step unless its top state is a final one
					namespace villager = ecs::villager;
					const bool topFinal = villager::state_info::IsFinal(
					    villager::state_info::StateInfo(villager::GetState(*object, villager::Index::Top)));
					const auto msPerTurn = game_clock::MsPerTurn();
					const uint32_t turnsPerSecond = msPerTurn != 0 ? 1000u / msPerTurn : 0u;
					const auto middle = creature_throw::MiddleOf(thing->position, groundThere, height);
					aim = creature_throw::AimAhead(self->position, thing->position, [&](float seconds) {
						return creature_throw::VillagerAfter(middle, hug->step, topFinal, turnsPerSecond, seconds);
					});
				}
				else
				{
					aim = creature_throw::AimAtStillThing(self->position, thing->position, groundThere, height);
				}
			}
			hands.ThrowAtThing(creature, aim.point, aim.seconds);
			break;
		}
		// At a point in the air when it has a height, as the player's camera; otherwise at the ground there
		hands.Throw(creature, {point.x, order.pointHeight.value_or(point.y), point.z});
		break;
	}
	case Kind::TakeFromHand:
		if (!object.has_value() || !registry.Valid(*object) || !hands.TakeFromHand(creature, *object, order.mirrored))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::ThrowInStore:
	{
		// At the store's own place on the ground, timed by the distance along the ground as every creature throw is
		const auto* store = object.has_value() && registry.Valid(*object) ? registry.TryGet<const Transform>(*object) : nullptr;
		const auto* self = registry.TryGet<const Transform>(creature);
		if (store == nullptr || self == nullptr)
		{
			hands.Cancel(creature);
			break;
		}
		const glm::vec2 at {store->position.x, store->position.z};
		const auto height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
		const glm::vec3 into {at.x, height, at.y};
		const auto flight = creature_throw::ThrowFlightTime(creature_throw::GroundDistance(self->position, into));
		hands.ThrowTaking(creature, into, flight);
		break;
	}
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
	case Kind::Kick:
		if (!object.has_value() || !registry.Valid(*object) || !hands.Kick(creature, *object))
		{
			hands.Cancel(creature);
		}
		break;
	case Kind::PointAt:
		if (object.has_value() || order.seconds.has_value() || order.pointHeight.has_value())
		{
			// At a thing, wherever it is now and goes; or at a point in the air, as the camera
			const auto* at =
			    object.has_value() && registry.Valid(*object) ? registry.TryGet<const Transform>(*object) : nullptr;
			if (object.has_value() && at == nullptr)
			{
				hands.Cancel(creature);
				break;
			}
			const auto aim = at != nullptr ? at->position : glm::vec3 {point.x, order.pointHeight.value_or(point.y), point.z};
			hands.PointAtFor(creature, aim, order.seconds.value_or(creature_object_actions::k_PointSeconds), object);
			break;
		}
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
/// What it was set to do is forced on it as an order is: it chooses nothing for itself until its agenda is done
void MarkCommanded(entt::entity creature)
{
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature); mind != nullptr)
	{
		mind->commanded = true;
	}
}

/// Fainting from what its body lacks, the advisor says why first
void TellFainting(entt::entity creature)
{
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Fainting(creature);
	}
}

/// While it walks, the head does not look at what a step gives it to look at once that is this near
constexpr float k_NoGazeWhileWalkingWithin = 10.0f;

/// Where the player's hand nearest a creature is, none for a creature without a player or when there is no hand
std::optional<glm::vec3> NearestHand(const ecs::Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || transform == nullptr || !Locator::handSystem::has_value())
	{
		return std::nullopt;
	}
	std::optional<glm::vec3> nearest;
	for (const auto& hand : Locator::handSystem::value().GetPlayerHandPositions())
	{
		if (hand.has_value() &&
		    (!nearest.has_value() || glm::distance(*hand, transform->position) < glm::distance(*nearest, transform->position)))
		{
			nearest = hand;
		}
	}
	return nearest;
}

/// Where the player's hand nearest a creature is on the ground, only while its step watches the hand: where the hand
/// last had a place, so also while it has none. None for a creature without a player
std::optional<glm::vec2> HandPointOf(const ecs::Registry& registry, entt::entity creature, const CreatureMindState& mind)
{
	const auto& idle = mind.idle;
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchHand)
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL)
	{
		return std::nullopt;
	}
	return glm::vec2(mind.lastHandPoint.x, mind.lastHandPoint.z);
}

/// Whether a creature's movement is a turn on the spot, or a turn waiting for its action to end, rather than a way
/// somewhere
bool TurningOnTheSpot(const ecs::Registry& registry, entt::entity creature)
{
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	return self != nullptr && (self->motion == CreatureLocomotion::Motion::Turning || self->turnAfterAction.has_value());
}

/// Whether the thing a creature's step watches fly is still really in the air, only while its step watches a flying thing
bool WatchedThingInTheAir(const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchFlying ||
	    !idle.agenda[idle.step].object.has_value())
	{
		return false;
	}
	return ecs::ActuallyInTheAir(static_cast<entt::entity>(*idle.agenda[idle.step].object));
}

/// Where the head looks for a step that gives it something to look at, if anywhere this turn: the top of a thing (not
/// one it holds), or the thing itself, or the ground under it, the player's hand nearest it, the ground under a point, the
/// camera, or the ground at its feet. Walking, it does not look at a thing or a point nearer than ten (its feet it always
/// looks at). A creature without a player does not look at the camera or a hand.
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
	if (gaze.hand)
	{
		return NearestHand(registry, creature);
	}
	if (gaze.player)
	{
		// The hand when it moved much lately, else the camera
		const auto* mind = registry.TryGet<const CreatureMindState>(creature);
		if (mind != nullptr && mind->playerInterest.handMovement > creature_player_attention::k_WatchHandMovement)
		{
			return NearestHand(registry, creature);
		}
		return Locator::camera::has_value() ? std::optional(Locator::camera::value().GetOrigin()) : std::nullopt;
	}
	const auto* self = registry.TryGet<const Transform>(creature);
	if (gaze.feet)
	{
		if (self == nullptr)
		{
			return std::nullopt;
		}
		const auto feet =
		    creature_mind::FeetLookPoint(glm::vec2(self->position.x, self->position.z), BodyYaw(registry, creature, *self));
		const auto ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(feet) : self->position.y;
		return glm::vec3 {feet.x, ground, feet.y};
	}
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
	// The thing itself where it is, as up in the air
	if (gaze.object.has_value() && gaze.itself)
	{
		return *at;
	}
	if (gaze.object.has_value() && !gaze.bottom)
	{
		return glm::vec3 {flat.x, at->y + ecs::object::GetHeight(static_cast<entt::entity>(*gaze.object)), flat.y};
	}
	// On the ground under it
	const auto ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(flat) : at->y;
	return glm::vec3 {flat.x, ground, flat.y};
}

/// The point of focus of the camera of a creature's player, on the land, while its step follows the camera; and whether
/// the point is less than a sixth of a turn from the way its body faces, or in its own map cell. None for a creature
/// without a player.
std::pair<std::optional<glm::vec3>, bool> CameraFocusOf(const ecs::Registry& registry, entt::entity creature,
                                                        const creature_mind::IdleMind& idle)
{
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::FollowCamera ||
	    !Locator::camera::has_value())
	{
		return {std::nullopt, false};
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || transform == nullptr)
	{
		return {std::nullopt, false};
	}
	const auto focus = Locator::camera::value().GetFocus();
	const auto from = map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z));
	const auto to = map_coords::FromMetres(glm::vec2(focus.x, focus.z));
	// The way the body faces, as the game turns it: a quarter turn on from the map's
	const auto ahead = -(transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	const auto facing = static_cast<int32_t>(gutils::ConvertScawenAngleToGameAngle(
	    static_cast<float>(std::atan2(static_cast<double>(ahead.x), -static_cast<double>(ahead.z)))));
	// The game takes the difference of the two angles with no wrap at a full turn
	const auto apart = std::abs(facing - static_cast<int32_t>(gutils::GetAngleFromXZ(from, to)));
	const bool inView =
	    apart < gutils::k_GameAngleCircle / creature_look::k_BodyViewParts || map_coords::Cell(from) == map_coords::Cell(to);
	return {focus, inView};
}

/// The player's hand as a creature that follows the hand sees it, only while its step follows the hand. If the hand is off
/// the land, the last position of the hand counts.
std::optional<creature_player_attention::FollowHandView> FollowHandViewOf(const ecs::Registry& registry, entt::entity creature,
                                                                          const CreatureMindState& mind, bool bodyPlaying)
{
	const auto& idle = mind.idle;
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::FollowHand)
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	auto hand = NearestHand(registry, creature);
	if (!hand.has_value())
	{
		hand = mind.playerInterest.lastHand;
	}
	creature_player_attention::FollowHandView view;
	if (body == nullptr || body->owner == PlayerNames::NEUTRAL || !hand.has_value() || transform == nullptr || self == nullptr)
	{
		return view;
	}
	view.hasPlayer = true;
	const map_coords::MapCoords handAt {.x = map_coords::ToFixed(hand->x), .z = map_coords::ToFixed(hand->z)};
	const map_coords::MapCoords at {.x = map_coords::ToFixed(transform->position.x),
	                                .z = map_coords::ToFixed(transform->position.z)};
	view.handInMap = map_coords::InBounds(handAt);
	view.hand = {hand->x, hand->z};
	view.distance = gutils::GetDistanceInMetres(at, handAt);
	view.reach = creature_player_attention::FollowHandReach(object_measures::TwoDRadius(registry, creature));
	view.bodyPlaying = bodyPlaying;
	view.directionClose = creature_player_attention::IsDirectionClose(
	    at, handAt, creature_player_attention::GameAngleOf(creature_locomotion::DirectionOf(self->heading)));
	return view;
}

/// The player as a creature that watches it sees it, only while its step watches the player
std::optional<creature_player_attention::WatchPlayerView>
WatchPlayerViewOf(const ecs::Registry& registry, entt::entity creature, const CreatureMindState& mind, bool bodyPlaying)
{
	const auto& idle = mind.idle;
	if (idle.step >= idle.agenda.size() || idle.agenda[idle.step].kind != creature_mind::Step::Kind::WatchPlayer)
	{
		return std::nullopt;
	}
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* self = registry.TryGet<const CreatureLocomotion>(creature);
	creature_player_attention::WatchPlayerView view {
	    // TODO(creature-attention-actions): after a leash order to a place, the game makes the creature wait for orders for
	    // fifteen seconds. The creature decides nothing in that time. openblack does not have that wait yet. Thus the watch
	    // ends at once, as in the game at all other times.
	    .waitingForOrders = false,
	    .hasPlayer = body != nullptr && body->owner != PlayerNames::NEUTRAL,
	    .bodyPlaying = bodyPlaying,
	    .handMovement = mind.playerInterest.handMovement,
	};
	if (transform == nullptr || self == nullptr)
	{
		return view;
	}
	const auto targetOf = [transform, self](glm::vec3 point) {
		const auto offset = glm::vec2(point.x, point.z) - glm::vec2(transform->position.x, transform->position.z);
		return creature_player_attention::WatchPlayerView::Target {
		    .point = {point.x, point.z},
		    .distance = glm::distance(point, transform->position),
		    .turn = creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - self->heading),
		};
	};
	if (const auto hand = NearestHand(registry, creature); hand.has_value())
	{
		view.hand = targetOf(*hand);
	}
	if (Locator::camera::has_value())
	{
		view.camera = targetOf(Locator::camera::value().GetOrigin());
	}
	return view;
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
	const auto recently = creature_object_beliefs::MovingSeenRecently(game_clock::Turn(), mind.movingSeen.turn, turnsPerSecond);
	const auto look = creature_mind::LookWhileGoing(*mind.walkLook, numbers, from, to, BodyYaw(registry, creature, at),
	                                                mind.look.id.has_value(), turnsPerSecond, recently);
	if (look.forgetMovingSeen)
	{
		mind.movingSeen.weight = 0.0f;
	}
	using Kind = creature_mind::WalkLookAt::Kind;
	switch (look.kind)
	{
	case Kind::Nothing:
		break;
	case Kind::Point:
		return glm::vec3 {to.x, GroundAt(to, point.y), to.y};
	case Kind::Interesting:
		return mind.look.point;
	case Kind::MovingSeen:
		// Where it was seen, as high above the ground there as it stood
		return glm::vec3 {mind.movingSeen.at.x, GroundAt(mind.movingSeen.at, point.y) + mind.movingSeen.height,
		                  mind.movingSeen.at.y};
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

/// The creature heals itself: its life becomes full, and its wounds and blood age as if much time went by
void HealItself(ecs::Registry& registry, entt::entity creature)
{
	if (auto* needs = registry.TryGet<CreatureNeeds>(creature))
	{
		needs->needs.life = 1.0f;
	}
	if (Locator::creatureSkinSystem::has_value())
	{
		Locator::creatureSkinSystem::value().Heal(creature, creature_mind::k_HealItselfCounts);
	}
}

/// How interesting the creature finds its player this turn. The local player's hand is the player's; a creature of
/// another player, or of none, finds nothing of interest here.
float PlayerInterestNow(const Creature& body, CreatureMindState& mind)
{
	if (!Locator::handSystem::has_value() || body.owner != creature::LocalPlayer())
	{
		return creature_desire_sources::InterestNow(mind.playerInterest, std::nullopt, std::nullopt);
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& hands = Locator::handSystem::value();
	constexpr auto k_Hand = static_cast<size_t>(HandSystemInterface::Side::Left);
	// Off the land the hand has no place: it counts as where it last was
	auto hand = hands.GetPlayerHandPositions().at(k_Hand);
	if (!hand.has_value())
	{
		hand = mind.playerInterest.lastHand;
	}
	std::optional<float> heldRadius;
	if (const auto held = hands.GetHeldObject(); held.has_value() && registry.Valid(*held))
	{
		heldRadius = object_measures::TwoDRadius(registry, *held);
	}
	if (!hand.has_value())
	{
		return 0.0f;
	}
	return creature_desire_sources::InterestNow(mind.playerInterest, hand, heldRadius);
}

/// The rest of the agenda is no use: it gives it up
void GiveUpAgenda(creature_mind::IdleMind& idle)
{
	idle.step = idle.agenda.size();
	idle.stepStarted = false;
	idle.gaveUp = true;
}

/// The fight that a creature's agenda starts with another creature: whether it started. Without the fights, none does.
bool StartFightWith(entt::entity creature, entt::entity other)
{
	return Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().StartFight(creature, other) ==
	                                                        CreatureFightSystemInterface::StartResult::Started;
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
	// The fart's row, which tells a fart from a poo as the creature squats
	const auto fartAction =
	    lookTables != nullptr ? creature_mind_tables::FindAction(*lookTables, creature_fart::k_Action) : std::nullopt;

	// The camera of the player at this machine, which its own creature notes looking at it
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const auto localPlayer = creature::LocalPlayer();

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
			if (mind.scriptStage.has_value() && mind.pendingFile == nullptr)
			{
				TakeUpScriptStage(mind);
			}
			if (mind.scriptName.has_value() && mind.learnt.has_value())
			{
				mind.learnt->name = std::move(*mind.scriptName);
				mind.scriptName.reset();
			}
			if (!mind.scriptKnows.empty() && mind.learnt.has_value() && mind.pendingFile == nullptr)
			{
				const auto lessons = TakeUpScriptKnows(
				    mind.scriptKnows, GetTables(), creature_mind_tables::MiracleMultiplier(creature::InfoRow(creature.species)),
				    *mind.learnt);
				mind.scriptKnows.clear();
				// The advisor tells of what the script taught it, now that its mind has it
				for (const auto& lesson : lessons)
				{
					if (Locator::creatureHelpSystem::has_value())
					{
						Locator::creatureHelpSystem::value().Raise(entity, lesson);
					}
				}
			}
			++mind.turn;
			// It notes the region of the land where it stands, for its map of the places it went to, unless the hand
			// holds it
			if (!(Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == entity))
			{
				ecs::creature_exploration_land::Visit(entity, {transform.position.x, transform.position.z});
			}
			// How interesting it finds its player follows the hand, also while the hand holds it. The speed of the hand
			// follows the movement of the hand in the turn.
			const auto handBefore = mind.playerInterest.lastHand;
			creature_desire_sources::UpdateInterest(mind.playerInterest, PlayerInterestNow(creature, mind));
			const auto handMovement = handBefore.has_value() && mind.playerInterest.lastHand.has_value()
			                              ? *mind.playerInterest.lastHand - *handBefore
			                              : glm::vec3(0.0f);
			creature_player_attention::UpdateHandSpeed(mind.handSpeed, handMovement, k_TurnsPerSecond);
			// Where the hand nearest it last had a place is kept for the look at the hand
			mind.lastHandPoint = creature_player_attention::LastHandPoint(NearestHand(registry, entity), mind.lastHandPoint);
			// It forgets the things gone from the world, and its eyes sweep a few cells about it, even under a script's
			// control
			ForgetGoneThings(mind);
			SweepEyes(entity, mind, animation, transform);
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
			// Every turn it lives out of the hand, it notes whether its player's camera looks at it: only the player's
			// own creature can be, as only that player's camera is here
			const bool held =
			    Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == entity;
			if (!held)
			{
				const bool lookedAt =
				    camera != nullptr && creature.owner == localPlayer &&
				    creature_attention::IsLookingAt(camera->GetOrigin(), camera->GetFocus(), transform.position);
				creature_attention::NoteTurn(lookedAt, mind.turnsLookedAt, mind.turnsNotLookedAt);
				++mind.turnsSinceMadeToAct;
			}
			// Held by the player's hand its mind stands still, but for showing what it wants once it has turned to the
			// player
			if (held)
			{
				if (mind.showDesireOnHold)
				{
					ShowDesireOnHold(entity, mind, animation);
				}
				return;
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
			// Out of the hand, it looks at its emergencies before it decides: low life, a fire on it, a fire in its town
			if (!held)
			{
				StepEmergencies(entity, creature, mind, scripted, dominantRate);
			}
			// The asker of a joint action can make its mind stand still: its desires stand still, and it chooses
			// nothing, but it carries out the orders of the asker
			const bool demobilised = lookup.AllOf<CreatureDemobilised>(entity);
			if (!scripted && !demobilised)
			{
				const auto homeDistance = HomeDistance(registry, entity, transform.position);
				creature_desires::UpdateSources(*mind.desires, [&mind, &body, night, homeDistance](
				                                                   uint32_t type, const creature_desires::Desires& desires) {
					return ReadSource(type, desires, mind, body, night, homeDistance);
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
			// One still out cold from a faint does not faint again
			const bool fainting = needs != nullptr && needs->faint.has_value() && !mind.idle.outCold;
			if (fainting && Locator::creatureFightSystem::has_value())
			{
				if (mind.turn > k_TurnsBeforePassingOut)
				{
					TellFainting(entity);
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
				TellFainting(entity);
				creature_mind::Plan(mind.idle, creature_mind::Activity::Faint, creature_mind::Faint());
				// lying there is forced on it, as an order is
				mind.commanded = true;
				needs->faint.reset();
			}
			// A thing it uses gone, it stops what it was doing
			LoseGoneThings(entity, mind);
			// Out cold with nothing left to do, it comes to, and chooses nothing this turn
			const bool cameTo = creature_mind::ComesTo(mind.idle);
			// What it was ordered or forced to do is over once its agenda has no step left
			if (mind.idle.step >= mind.idle.agenda.size())
			{
				mind.commanded = false;
			}
			// Free to choose what to do next, it weighs its desires' plans
			if (!scripted && !demobilised && !cameTo && !mind.planActive && mind.idle.step >= mind.idle.agenda.size() &&
			    !(needs != nullptr && needs->faint.has_value()))
			{
				// A try at a miracle that fizzled, or what a stroke or slap left it to do, is what it does first
				ShowFizzle(entity, mind);
				DoAfterFeedback(entity, mind);
				// Given something by the hand, it decides what to do with what it holds, and plans nothing else this turn
				const bool usedGift = UseWhatItWasGiven(entity, mind);
				if (!usedGift && mind.idle.step >= mind.idle.agenda.size())
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
				// it looks as it is ordered to, choosing nothing for itself meanwhile
				mind.commanded = true;
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
			// So do the hands' animations on a thing, until they end
			const bool handling = Locator::creatureObjectActionSystem::has_value() &&
			                      Locator::creatureObjectActionSystem::value().GetProgress(entity).has_value();
			const auto [cameraFocus, cameraFocusAhead] = CameraFocusOf(lookup, entity, mind.idle);
			// The body is busy walking or turning, playing an action, acting on a thing or catching. (A fighter never
			// gets here: the mind leaves it alone above)
			const bool bodyBusy = creature_body::IsPerformingAction(
			    {.playing = creature_layers::IsPlaying(animation.body), .moving = moving, .handling = handling || catching});
			const creature_mind::Senses senses {
			    .seconds = k_TurnSeconds,
			    .turnsPerSecond = game_clock::MsPerTurn() != 0 ? 1000u / game_clock::MsPerTurn() : 0u,
			    .bodyBusy = bodyBusy,
			    .bodyLooping = creature_layers::IsLooping(animation.body),
			    .bodyPlaying = creature_layers::IsPlaying(animation.body),
			    .realSpeed = animation.realSpeed,
			    .bodyDancing = creature_layers::IsDancing(animation.body),
			    .holding = lookup.AllOf<CreatureHeldObject>(entity),
			    .heldMirrored =
			        lookup.AllOf<CreatureHeldObject>(entity) && lookup.Get<const CreatureHeldObject>(entity).mirrored,
			    .moving = moving,
			    .turning = TurningOnTheSpot(lookup, entity),
			    .subMove = SubMoveOf(entity),
			    .position = glm::vec2(transform.position.x, transform.position.z),
			    .rested = needs != nullptr && needs->rested,
			    .better = needs != nullptr && creature_mind::IsBetter(needs->needs.life, needs->needs.exhaustion),
			    .hands = HandsOf(entity),
			    .objectInMap = WaitedForInMap(lookup, mind.idle),
			    .objectMoving = WaitedForMoving(lookup, mind.idle),
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
			    .overlayPlaying = animation.gesture.animation.has_value(),
			    .cameraFocus = cameraFocus,
			    .cameraFocusAhead = cameraFocusAhead,
			    .hand = HandPointOf(lookup, entity, mind),
			    .followHand = FollowHandViewOf(lookup, entity, mind, bodyBusy),
			    .watchPlayer = WatchPlayerViewOf(lookup, entity, mind, bodyBusy),
			    .dance = DanceViewOf(entity, mind.idle),
			    .totemShare = mind_town_help::TotemShareOf(lookup, mind.idle),
			    .objectInTheAir = WatchedThingInTheAir(mind.idle),
			    .swap = SwapViewOf(entity),
			};
			// A sit counting its time rests the body this turn, the turn it is asked to end included
			const bool sitCounting = creature_mind::SitCounting(mind.idle);
			const auto commands = creature_mind::Think(mind.idle, senses, random);
			if (commands.dance.has_value())
			{
				CarryOutDance(entity, mind.idle, *commands.dance);
			}
			KeepDanceWithAgenda(entity, mind.idle);
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
			// Resting to get better, or once it has cast water on itself, its own fire stops
			if (commands.coolDown && Locator::fireSystem::has_value())
			{
				Locator::fireSystem::value().PutOut(entity);
			}
			if (commands.healItself)
			{
				HealItself(registry, entity);
			}
			// The miracle is tried as the casting pose's loop begins, at its place or else at its thing; the rest of the
			// agenda is no use without it
			if (commands.cast.has_value() &&
			    (commands.cast->place.has_value()
			         ? !TryMiracleAt(entity, static_cast<MagicType>(commands.cast->magicType), *commands.cast->place)
			         : commands.cast->object.has_value() &&
			               !TryMiracle(entity, static_cast<MagicType>(commands.cast->magicType),
			                           static_cast<entt::entity>(*commands.cast->object))))
			{
				GiveUpAgenda(mind.idle);
			}
			// A joint action: when the other creature refuses to be its partner, the rest of the agenda is no use
			if (commands.requestPartner.has_value() &&
			    !RequestPartner(entity, static_cast<entt::entity>(commands.requestPartner->creature),
			                    commands.requestPartner->kind))
			{
				GiveUpAgenda(mind.idle);
			}
			// A fight: the duel between the two starts now, and the rest of the agenda is no use when it can't
			if (commands.fight.has_value() && !StartFightWith(entity, static_cast<entt::entity>(*commands.fight)))
			{
				GiveUpAgenda(mind.idle);
			}
			if (commands.order.has_value() && commands.order->step != nullptr)
			{
				TakeOrder(static_cast<entt::entity>(commands.order->partner), entity, *commands.order->step);
			}
			if (commands.demobilise.has_value())
			{
				SetDemobilised(static_cast<entt::entity>(commands.demobilise->first), commands.demobilise->second);
			}
			// A joint action with a town ends in a desire about the town: its plan takes the place of the agenda. The
			// action stops as a failure, and so the joint action stops at once for the partner too, before the partner
			// takes up the desire of its own step.
			if (commands.applyDesire.has_value())
			{
				EndJointAction(entity);
				std::ignore =
				    ForceDesireOn(entity, commands.applyDesire->desire, static_cast<entt::entity>(commands.applyDesire->thing));
			}
			Order(lookup, entity, commands);
			mind_town_help::Apply(lookup, commands);
			StepSwap(entity, commands);
			// A rock broken in two at once (a rock, the bonfire, a fragment: whatever answers it is a rock); anything
			// else can't be, and the rest of the agenda is no use
			if (commands.shatter.has_value())
			{
				const auto rock = static_cast<entt::entity>(*commands.shatter);
				if (lookup.Valid(rock) && ecs::object_physics::IsRock(rock))
				{
					static_cast<void>(ecs::object_physics::SplitRockAsItGoes(rock));
				}
				else
				{
					mind.idle.step = mind.idle.agenda.size();
					mind.idle.stepStarted = false;
					mind.idle.gaveUp = true;
				}
			}
			// A building or a totem mended by a fifth of its life (a totem on its town centre passes the mending on to
			// the centre), and the tree used up mending it gone
			if (commands.repair.has_value() && lookup.Valid(static_cast<entt::entity>(*commands.repair)))
			{
				const auto mended = static_cast<entt::entity>(*commands.repair);
				if (lookup.AllOf<Abode>(mended))
				{
					static_cast<void>(ecs::abodes::IncreaseLife(mended, creature_mind::k_RepairLife));
				}
				else if (lookup.AllOf<VillageTotem>(mended))
				{
					static_cast<void>(ecs::life::IncreaseLife(mended, creature_mind::k_RepairLife));
				}
			}
			if (commands.useUp.has_value() && lookup.Valid(static_cast<entt::entity>(*commands.useUp)))
			{
				ecs::ToBeDeleted(static_cast<entt::entity>(*commands.useUp));
			}
			// The poo's moment is a fart when the plan carried out is the fart
			const bool farts = commands.effect == creature_mind::Effect::Poo &&
			                   creature_fart::Farts(mind_detail::CurrentAction(mind, lookTables), fartAction);
			TakeEffect(entity, commands, *mind.desires, farts);
			// It faints, and those near it react as to a death. No player is known for it, as for a villager's death
			if (commands.fainted)
			{
				ecs::effects::reactions::CreateReaction(entity, openblack::Reaction::ReactToDeath, std::nullopt, false);
			}
			// From this step on, the plan acts on the creature itself rather than on the thing it chose
			if (commands.clearObjectToActOn)
			{
				creature_planner::ActOnItself(mind.planner, static_cast<uint32_t>(entt::to_integral(entity)));
			}
			// From this step on, the plan uses the creature itself rather than the thing it chose to use
			if (commands.clearObjectToUse)
			{
				creature_planner::UseItself(mind.planner, static_cast<uint32_t>(entt::to_integral(entity)));
			}
			// Eating charged the body for its action already, which the plan's end then does not
			if (commands.effect == creature_mind::Effect::Eat)
			{
				mind.satisfiedByEffect = true;
			}
			if (commands.effect == creature_mind::Effect::Completed)
			{
				CompleteByStep(entity, mind);
			}
			// Having looked another creature over, it notes when, and waits a while before it does again
			if (commands.effect == creature_mind::Effect::Inspected && commands.effectObject.has_value())
			{
				if (auto* inspected = mind.beliefs.Find(static_cast<entt::entity>(*commands.effectObject)))
				{
					inspected->inspected = ecs::effects::reactions::Turn();
				}
			}
			FollowAgenda(entity, mind);
			// The script's turn over, the plan it gave is done with
			if (!mind.planActive && mind.idle.step >= mind.idle.agenda.size() && mind.planner.current.has_value() &&
			    mind.planner.current->desire == Desire::ObeyPlayer &&
			    mind.planner.current->action == creature_planner::k_FollowPlayerAction)
			{
				creature_planner::StopPlan(mind.planner);
			}
			if (needs != nullptr)
			{
				needs->rest = creature_mind::IsUnconscious(mind.idle) ? CreatureNeeds::Rest::Unconscious
				              : creature_mind::IsAsleep(mind.idle)    ? CreatureNeeds::Rest::Asleep
				              : creature_mind::IsResting(mind.idle)   ? CreatureNeeds::Rest::Resting
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
			// A thing it was told to look at: where the thing stands, until the thing is gone, which lets the focus go
			if (mind.scriptFocusThing.has_value())
			{
				const auto thing = *mind.scriptFocusThing;
				const auto* at = lookup.Valid(thing) ? lookup.TryGet<const Transform>(thing) : nullptr;
				if (at != nullptr)
				{
					animation.lookAt = at->position;
				}
				else
				{
					mind.scriptFocusThing.reset();
				}
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
	// A joint action stops for both creatures when either stops what it does for it
	KeepJointActions();
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
	// The scripts read how strongly it was last rewarded or punished
	mind->interactionMagnitude = feedback;
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

void CreatureMindSystem::HandTookHold(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// Its body stops what it does: its side of a duel ends, it stops pointing, then it stops and turns on the spot
	// towards the player's eye, from where its body is drawn
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().EndOwnFight(creature);
	}
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().StopPointing(creature);
	}
	if (Locator::creatureLocomotionSystem::has_value() && Locator::camera::has_value())
	{
		const auto drawn = ecs::creature_pose::BetweenTurns(registry, creature);
		const auto* transform = registry.TryGet<const Transform>(creature);
		if (drawn.has_value() || transform != nullptr)
		{
			const auto body = drawn.has_value() ? drawn->position : transform->position;
			const auto heading = creature_locomotion::HeldTurnHeading(body, Locator::camera::value().GetOrigin());
			static_cast<void>(Locator::creatureLocomotionSystem::value().TurnToHeading(creature, heading));
		}
	}
	mind->showDesireOnHold = true;
}

void CreatureMindSystem::ShowDesireOnHold(entt::entity creature, CreatureMindState& mind, CreatureAnimation& animation)
{
	// It waits only for the action its body plays to end, not for its turn to the player
	if (creature_layers::IsPlaying(animation.body))
	{
		return;
	}
	mind.showDesireOnHold = false;
	std::optional<Desire> credited;
	if (mind.learnt.has_value())
	{
		if (const auto best = creature_learning::BestContext(mind.learnt->contexts); best.has_value())
		{
			credited = mind.learnt->contexts.at(*best).desire;
		}
	}
	const auto planned =
	    mind.planActive && mind.planner.current.has_value() ? std::optional(mind.planner.current->desire) : std::nullopt;
	const auto desire = creature_help::DesireShownOnHold(credited, planned);
	if (!desire.has_value())
	{
		return;
	}
	PlayAction(creature, *creature_desires::EmoteFor(*desire));
	// The help says what it wants, even though the hand is on it
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().Raise(
		    creature,
		    {.kind = creature_help::Kind::CurrentDesire, .parameter = static_cast<uint32_t>(*desire), .forced = true});
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
	mind->lastFeedback = CreatureMindState::Feedback {.value = feedback, .activity = mind->idle.activity};
	// Stroked or slapped, however lightly, it first stops whatever it was doing: the action, the walk, its agenda and
	// the plan it carried out, what it learns from left as it was
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Cancel(creature);
	}
	// as an action finished unsuccessfully: a walk brakes to a stop
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().BrakeToStop(creature);
	}
	// a dance it dances stops
	if (auto* body = registry.TryGet<CreatureAnimation>(creature); body != nullptr)
	{
		body->body = creature_layers::EndDance(body->body);
	}
	mind->idle.agenda.clear();
	mind->idle.step = 0;
	mind->idle.stepStarted = false;
	mind->planActive = false;
	creature_planner::StopPlan(mind->planner);
	// A creature that learns nothing from it has done with it
	if (std::as_const(registry).AllOf<CreatureLearningOff>(creature))
	{
		return;
	}
	// Too slight to count, it only looks at the player, idling about with it, and the hand offers to focus on it
	if (creature_feedback::IsSlightFeedback(feedback))
	{
		std::ignore = ForceActivity(
		    creature, {.desire = creature_desires::Desire::IdleWithPlayer, .action = "LookAtCamera", .score = 1.0f});
		help::tooltips::Force(helptext::k_ToolTipFocusCreature, 1.0f);
		return;
	}
	// A stroke shows it its player wants compassion, a slap anger, by how hard
	UpdateAttitudeFromFeedback(creature, feedback);
	LearnFromFeedback(creature, *mind, feedback);
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
	// On a walk, it brakes along its route to a stop
	if (stopWalking && Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().BrakeToStop(creature);
	}
	creature_mind::Plan(mind.idle, creature_mind::Activity::None, {});
	mind.planActive = false;
	creature_planner::StopPlan(mind.planner);
	// An emergency it was in is over, and so is an order of its player's it carried out
	mind.emergencyRunning = false;
	mind.commanded = false;
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
	mind->scriptFocusThing.reset();
	static_cast<void>(CarryOutForScript(creature, *mind, creature_mind::TurnToFacePoint(glm::vec2(point.x, point.z)), true));
}

void CreatureMindSystem::FocusOnThingByScript(entt::entity creature, entt::entity thing)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	// As with a point: under the script's control it gives up what it was doing and obeys, turning to face the thing
	// and holding still two seconds once it does, its head on the thing from now on
	mind->scriptFocusThing = thing;
	mind->scriptFocus.reset();
	static_cast<void>(
	    CarryOutForScript(creature, *mind, creature_mind::FaceThingForScript(static_cast<uint32_t>(thing)), true));
	// and it comes to know the thing, as it does a thing it sees
	ComeAcross(creature, *mind, thing);
}

void CreatureMindSystem::ReleaseFocusByScript(entt::entity creature)
{
	// Only the focus goes: the creature stays under the script's control, doing what it was doing
	if (auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature); mind != nullptr)
	{
		mind->scriptFocus.reset();
		mind->scriptFocusThing.reset();
	}
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
	return CarryOutForScript(creature, *mind, creature_script_play::Agenda(mind->scriptPlay), true);
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
	// Unlike every other thing a script has it do, the walk it is on is not stopped: the legs take it up, so a move sent
	// again each turn to the same point carries the walk on, and one to a new point turns the walk there
	const bool walking =
	    Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature);
	const bool done = CarryOutForScript(creature, *mind, std::move(agenda), false);
	// For the debug tools, outside the registry
	NoteNative(CreatureNative::MoveGameThing,
	           {.object = static_cast<uint32_t>(entt::to_integral(creature)),
	            .point = point,
	            .done = done,
	            .note = walking ? "sent while walking: the walk under way is taken up" : "sent while still",
	            .turn = static_cast<uint32_t>(game_clock::Turn())});
	return done;
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

void CreatureMindSystem::ForceFinish(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<CreatureMindState>(creature))
	{
		return;
	}
	// The action that its body plays ends at two times the usual speed
	if (auto* body = registry.TryGet<CreatureAnimation>(creature); body != nullptr)
	{
		body->body = creature_layers::Hurry(body->body);
	}
	// What it was doing stops as a failure: it stops where it is, drops what it does with a thing, forgets its agenda and
	// lets go of a cast
	AbandonAction(creature);
}

void CreatureMindSystem::NoteNative(CreatureNative native, const CreatureNativeCall& call)
{
	auto& last = _lastNatives.at(static_cast<size_t>(native));
	const auto calls = last.has_value() ? last->calls : 0;
	last = call;
	last->calls = calls + 1;
}

void CreatureMindSystem::ChangeDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	const auto* body = mind != nullptr ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return;
	}
	SetUpMind(creature, body->species, *mind);
	if (!mind->desires.has_value())
	{
		return;
	}
	// Its stage of growing up decides which desires are active before the script has its say, not after
	if (mind->desiresPhase != mind->developmentPhase)
	{
		creature_desires::ActivateForPhase(*mind->desires, Phases(), mind->developmentPhase);
		mind->desiresPhase = mind->developmentPhase;
	}
	change(*mind->desires);
}

void CreatureMindSystem::ReleasedByScript(entt::entity creature)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(creature);
	if (mind == nullptr)
	{
		return;
	}
	mind->scriptFocus.reset();
	mind->scriptFocusThing.reset();
	StopWhatItWasDoing(creature, *mind);
	// The same stop as a script's start: a miracle it holds is let go of, and a sit or a casting pose it is in ends there
	// and then
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ReleaseCreatureCast(creature);
	}
	if (auto* body = Locator::entitiesRegistry::value().TryGet<CreatureAnimation>(creature); body != nullptr)
	{
		body->body = creature_layers::EndLoop(body->body);
	}
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
	// The face is pulled even when the body is too far into something to take the action
	if (face.has_value())
	{
		body->face =
		    creature_layers::PullFace(body->face, creature_face::AnimationOf(face->face), face->milliseconds, face->cue);
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
	// What it was doing ends as a forced plan ends it: a walk brakes to a stop
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().BrakeToStop(creature);
	}
	// Whatever it plays ends at once, as the game ends a creature's animations when it changes what it does; but the end
	// of an action the leash pulled it away from plays out, the new plan's steps waiting for it
	const bool ending = (body->body.kind == creature_layers::BodyAction::Kind::Sequence ||
	                     body->body.kind == creature_layers::BodyAction::Kind::Dance) &&
	                    body->body.endWanted && creature_layers::IsPlaying(body->body);
	if (!(mind->leash.bodyEndPlaysOut && ending))
	{
		body->body = {};
	}
	mind->leash.bodyEndPlaysOut = false;
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
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Transform>(creature))
	{
		return false;
	}
	// At the place where the drinking action drinks, walking to within its height
	const auto place = DrinkPlaceOf(creature);
	return place.has_value() && Replan(creature, creature_mind::Activity::Drink,
	                                   creature_drink::Drink(*place, object_measures::Height(registry, creature)));
}

bool CreatureMindSystem::Poo(entt::entity creature)
{
	return Replan(creature, creature_mind::Activity::Poo,
	              creature_mind::Poo([](uint32_t range) { return game_random::GameRand(range); }));
}

bool CreatureMindSystem::Puke(entt::entity creature)
{
	// Sick, it does nothing else of its own accord until it is done
	const bool sick = Replan(creature, creature_mind::Activity::Puke, creature_mind::Puke());
	MarkCommanded(creature);
	return sick;
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
	// Not food, poisoned, or a meal that may not be eaten: it is held back from hunger for a while and is sick; only what
	// is not food teaches it so
	creature_desires::Suppress(desires, Desire::Hunger, k_MealRefusedSuppressSeconds, k_TurnsPerSecond);
	if (end == creature_eating::MealEnd::NotFood && counts != nullptr)
	{
		++counts->notFood;
	}
	Puke(creature);
}

bool CreatureMindSystem::Faint(entt::entity creature)
{
	// Fainted, it does nothing else of its own accord until it comes round
	const bool fainted = Replan(creature, creature_mind::Activity::Faint, creature_mind::Faint());
	MarkCommanded(creature);
	return fainted;
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

void CreatureMindSystem::MoveToStage(entt::entity creature, uint32_t stage)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = registry.Valid(creature) ? registry.TryGet<CreatureMindState>(creature) : nullptr;
	if (mind == nullptr)
	{
		return;
	}
	mind->developmentPhase = stage;
	// The turns it has spent at its stage start again
	if (mind->learnt.has_value())
	{
		mind->learnt->developmentTimer = 0;
	}
	// Its desires are set for the stage at once, even for the stage it is at already; a mind yet to think sets them up
	// for the stage when it first does
	if (mind->desires.has_value())
	{
		creature_desires::ActivateForPhase(*mind->desires, Phases(), stage);
		mind->desiresPhase = stage;
	}
	else
	{
		mind->desiresPhase.reset();
	}
	// The mind file a creature is made from, or given, is the creature's mind at once, so the stage comes after it: a
	// mind that has yet to take up its file puts this stage over the file's once it has
	const auto* body = registry.TryGet<const Creature>(creature);
	const bool fileWaits = mind->pendingFile != nullptr || (!mind->resourceChecked && body != nullptr && body->mind != 0);
	if (fileWaits)
	{
		mind->scriptStage = stage;
	}
	else
	{
		mind->scriptStage.reset();
	}
	GiveUpPlan(creature);
}

void CreatureMindSystem::TakeUpScriptStage(CreatureMindState& mind)
{
	const auto stage = *mind.scriptStage;
	mind.scriptStage.reset();
	mind.developmentPhase = stage;
	if (mind.learnt.has_value())
	{
		mind.learnt->developmentTimer = 0;
	}
	if (mind.desires.has_value())
	{
		creature_desires::ActivateForPhase(*mind.desires, Phases(), stage);
		mind.desiresPhase = stage;
	}
	else
	{
		mind.desiresPhase.reset();
	}
}
