/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRunner.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

#include <LHVM.h>
#include <MindFile.h>
#include <SDL_events.h>
#include <bgfx/bgfx.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/FlatLand.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/GameMusic.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Audio/Services/Voices.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Camera/TempleCameraModel.h"
#include "Common/HelpText.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMode.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/LeashOwnership.h"
#include "Creature/LeashRules.h"
#include "Creature/LocalPlayer.h"
#include "Debug/GesturesModel.h"
#include "Debug/MiraclesCaster.h"
#include "Debug/MiraclesModel.h"
#include "Debug/TestbedDemoPoints.h"
#include "Debug/TestbedLandTimeline.h"
#include "Debug/WeatherModel.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/FieldArchetype.h"
#include "ECS/Archetypes/FishFarmArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/ChallengePicture.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/PuzzleTotem.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Footpaths.h"
#include "ECS/HeldApply.h"
#include "ECS/Life.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/PuzzleTotemHand.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ResourceStores.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/FootprintSystemInterface.h"
#include "ECS/Systems/GestureEventsInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/InputStateInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RenderingSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Vortex.h"
#include "ECS/VortexSave.h"
#include "ECS/Weather/Storms.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Gui/CreatureCaveScreen.h"
#include "Gui/GameInterface.h"
#include "Gui/TattooEditorDialog.h"
#include "Help/DialogueText.h"
#include "Help/HelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Help/Spirits.h"
#include "Help/SpiritsRuntime.h"
#include "Input/FixedMouse.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GameCursor.h"
#include "Input/GamePackets.h"
#include "Input/HandDemo.h"
#include "Input/RealInput.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/GestureTemplates.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Magic/VortexRules.h"
#include "Particles/ParticleTypes.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Windowing/WindowingInterface.h"
#include "Worship/Citadel.h"
#include "Worship/WorshipPercentage.h"

#include "../Profiler.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using openblack::ecs::archetypes::CreatureArchetype;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::CreatureReactionsOff;
using openblack::ecs::components::Transform;

namespace
{
/// A cast through the hand: the button goes down this long after the seed is in the hand, and a throw lets go this long
/// after it went down
constexpr float k_HandPressWait = 0.2f;
constexpr float k_HandThrowSeconds = 0.4f;
/// The hand looks at its point from this high over itself, and at a creature this high over its feet
constexpr float k_HandEyeHeight = 20.0f;
constexpr float k_HandAimHeight = 2.0f;

std::string_view HandResultName(ecs::systems::MagicSystemInterface::HandResult result)
{
	using HandResult = ecs::systems::MagicSystemInterface::HandResult;
	switch (result)
	{
	case HandResult::None:
		return "nothing";
	case HandResult::TookMiracle:
		return "took a miracle";
	case HandResult::Readied:
		return "armed";
	case HandResult::Cast:
		return "cast";
	case HandResult::CastHeld:
		return "cast, held";
	case HandResult::NotReady:
		return "not ready";
	case HandResult::CantCastThere:
		return "can't cast there";
	case HandResult::Released:
		return "let go";
	case HandResult::Discarded:
		return "dropped";
	case HandResult::NoCircle:
		return "no circle drawn";
	case HandResult::PoweredUp:
		return "powered up";
	}
	return "";
}

using Kind = Command::Kind;
using Pace = ecs::systems::CreatureLocomotionSystemInterface::Pace;

/// The lines of the log of commands kept
constexpr size_t k_LogLines = 8;
/// A creature follows another this far behind, for a creature of size 1
constexpr float k_FollowDistance = 25.0f;
/// The size of a creature whose scenario doesn't give one
constexpr float k_DefaultSize = 1.0f;
/// The least the overview takes in either way of the middle of what it frames
constexpr float k_MinOverviewHalfSize = 25.0f;
/// The live results of a benchmark are summed up again every so many frames
constexpr uint32_t k_FramesPerSummary = 30;
/// What the store of a crowd's town starts with
constexpr uint32_t k_CrowdFood = 2000;
constexpr uint32_t k_CrowdWood = 2000;
/// The town a scenario's buildings and fields belong to, its id kept clear of the crowds' towns, and what its buildings
/// hold
constexpr uint32_t k_ScenarioTownId = 900;
constexpr uint32_t k_ScenarioTownFood = 500;
constexpr uint32_t k_ScenarioTownWood = 500;
/// The hand's place on the screen is logged for this long after a button changes
constexpr float k_HandWatchSeconds = 0.5f;
/// The hand taking hold of a fireball gives up after this long
constexpr float k_FireBallTakeSeconds = 1.5f;
/// A double click on a creature gives up when the hand hasn't come over it after this long
constexpr float k_DoubleClickAimSeconds = 2.0f;
/// It presses the Action button this many frames after it is on the ball, and lets go after this long: well inside the
/// grab's 225 ms wait for a tap, or well past it to have the ball placed in the hand
constexpr uint32_t k_FireBallPressFrame = 2;
constexpr float k_FireBallTapSeconds = 0.1f;
constexpr float k_FireBallHoldSeconds = 0.4f;
/// The land the scenario's vortices levelled is logged once they are all gone, or after this long if one is still there
constexpr float k_VortexLandLogSeconds = 30.0f;

/// The scenario's town, a Celtic town of the player's in the middle of the map, made when first needed
uint32_t ScenarioTown(glm::vec2 middle)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& towns = registry.Context().towns;
	if (const auto found = towns.find(k_ScenarioTownId); found != towns.end())
	{
		if (registry.Valid(found->second))
		{
			return k_ScenarioTownId;
		}
		towns.erase(found);
	}
	const glm::vec3 position {middle.x, Locator::terrainSystem::value().GetHeightAt(middle), middle.y};
	ecs::archetypes::TownArchetype::Create(static_cast<int>(k_ScenarioTownId), position, PlayerNames::PLAYER_ONE,
	                                       Tribe::CELTIC);
	return k_ScenarioTownId;
}

/// Calls one of the scripts' natives by its name, as a script's call does: `push` puts its arguments on the scripts'
/// stack, the last on top, as many as the native takes, and it gives back as many values as `results`. Empty when it
/// was called, else why it was not
std::string CallScriptNative(std::string_view name, int32_t arguments, const std::function<void(lhvm::LHVM&)>& push,
                             uint32_t results = 0)
{
	if (!Locator::chlapi::has_value() || !Locator::vm::has_value())
	{
		return "the scripts have no VM";
	}
	const auto& table = Locator::chlapi::value().GetFunctionsTable();
	// A native of that name taking that many values: two natives may share a name
	const auto native = std::ranges::find_if(table, [name, arguments](const lhvm::NativeFunction& function) {
		return function.name == name && function.stackIn == arguments;
	});
	if (native == table.end() || native->impl == nullptr || native->stackIn != arguments || native->stackOut != results)
	{
		return fmt::format("no native {} that takes {} values and gives {}", name, arguments, results);
	}
	push(Locator::vm::value());
	native->impl();
	return {};
}

/// A script's SET_FOCUS on the creature at the point, or its RELEASE_FROM_SCRIPT, through the natives themselves, so
/// that the run goes through the native's creature branch as well as the creature's mind
std::string GiveScriptCommand(entt::entity creature, Kind kind, glm::vec2 point)
{
	const auto object = static_cast<uint32_t>(entt::to_integral(creature));
	std::string failed;
	if (kind == Kind::ScriptFocus)
	{
		// The thing first, then the point's x, y and z, which the native takes off the stack the other way round
		const glm::vec3 at {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y};
		failed = CallScriptNative("SET_FOCUS", 4, [object, at](lhvm::LHVM& vm) {
			vm.Pusho(object);
			vm.Pushv(at.x);
			vm.Pushv(at.y);
			vm.Pushv(at.z);
		});
	}
	else
	{
		failed = CallScriptNative("RELEASE_FROM_SCRIPT", 1, [object](lhvm::LHVM& vm) { vm.Pusho(object); });
	}
	if (!failed.empty())
	{
		return failed;
	}
	const bool controlled = ecs::script_held::IsControlledByScript(creature);
	// the native refuses a thing it no longer finds (an id of 0 among them) by logging only: say so here
	if (kind == Kind::ScriptFocus && !controlled)
	{
		return "not taken: the focus left it in its own control";
	}
	return controlled ? "under the script's control" : "in its own control";
}

/// A script's MOVE_GAME_THING of the creature to the point within the radius, through the native itself, so that the
/// run goes through the native's creature branch as well as the creature's mind
std::string GiveScriptMove(entt::entity creature, glm::vec2 point, float radius)
{
	const auto object = static_cast<uint32_t>(entt::to_integral(creature));
	// The thing, the point's x, y and z, then the radius, which the native takes off the stack the other way round
	const glm::vec3 at {point.x, Locator::terrainSystem::value().GetHeightAt(point), point.y};
	const auto failed = CallScriptNative("MOVE_GAME_THING", 5, [object, at, radius](lhvm::LHVM& vm) {
		vm.Pusho(object);
		vm.Pushv(at.x);
		vm.Pushv(at.y);
		vm.Pushv(at.z);
		vm.Pushf(radius);
	});
	if (!failed.empty())
	{
		return failed;
	}
	return ecs::script_held::IsControlledByScript(creature) ? "under the script's control" : "not taken: in its own control";
}

/// The first temple on the land, the scenario's first, if there is one
std::optional<entt::entity> FirstTemple()
{
	std::optional<entt::entity> first;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const ecs::components::Temple>([&first](entt::entity temple, const ecs::components::Temple&) {
		    if (!first.has_value() || entt::to_integral(temple) < entt::to_integral(*first))
		    {
			    first = temple;
		    }
	    });
	return first;
}

/// A script's CREATURE_DO_ACTION of an action's row on the creature, done to the thing with another thing or with
/// nothing, through the native itself, so that the run goes through the native as well as the creature's mind
std::string GiveScriptDoAction(entt::entity creature, std::optional<entt::entity> target, size_t action,
                               std::optional<entt::entity> with)
{
	if (!target.has_value())
	{
		return "no such object";
	}
	const auto object = static_cast<uint32_t>(entt::to_integral(creature));
	const auto thing = static_cast<uint32_t>(entt::to_integral(*target));
	const auto used = with.has_value() ? static_cast<uint32_t>(entt::to_integral(*with)) : 0u;
	const auto row = static_cast<int32_t>(action);
	// The creature, the row, the thing, then what to do it with (nothing when 0), which the native takes off the stack
	// the other way round
	const auto failed = CallScriptNative("CREATURE_DO_ACTION", 4, [object, row, thing, used](lhvm::LHVM& vm) {
		vm.Pusho(object);
		vm.Pushi(row);
		vm.Pusho(thing);
		vm.Pusho(used);
	});
	if (!failed.empty())
	{
		return failed;
	}
	return ecs::script_held::IsControlledByScript(creature) ? "under the script's control" : "not taken: in its own control";
}

/// A script's desire, reaction, creature-from-creature or speed native on the creature, through the natives themselves,
/// as Land 1's guide and Ogre scripts call them
std::string GiveGuideOgreNativeTo(entt::entity creature, const Command& command, glm::vec2 point,
                                  std::optional<entt::entity>& made)
{
	const auto object = static_cast<uint32_t>(entt::to_integral(creature));
	const auto value = static_cast<int32_t>(command.value);
	const auto amount = command.amount;
	switch (command.kind)
	{
	case Kind::ScriptDesiresOff:
	{
		const auto target = static_cast<uint32_t>(entt::to_integral(made.value_or(creature)));
		const auto failed = CallScriptNative("CREATURE_SET_DESIRE_ACTIVATED", 2, [target](lhvm::LHVM& vm) {
			vm.Pusho(target);
			vm.Pushi(0);
		});
		return failed.empty() ? "every desire off" : failed;
	}
	case Kind::ScriptDesire:
	{
		const auto target = static_cast<uint32_t>(entt::to_integral(made.value_or(creature)));
		auto failed = CallScriptNative("CREATURE_SET_DESIRE_ACTIVATED", 3, [target, value](lhvm::LHVM& vm) {
			vm.Pusho(target);
			vm.Pushi(value);
			vm.Pushi(1);
		});
		for (const auto* name : {"CREATURE_SET_DESIRE_VALUE", "CREATURE_SET_DESIRE_MAXIMUM"})
		{
			if (failed.empty())
			{
				failed = CallScriptNative(name, 3, [target, value, amount](lhvm::LHVM& vm) {
					vm.Pusho(target);
					vm.Pushi(value);
					vm.Pushf(amount);
				});
			}
		}
		return failed.empty() ? fmt::format("desire {} on, at {:.2f} and no more", value, static_cast<double>(amount)) : failed;
	}
	case Kind::ScriptReaction:
	{
		const auto failed = CallScriptNative("CREATE_REACTION", 2, [object, value](lhvm::LHVM& vm) {
			vm.Pusho(object);
			vm.Pushi(value);
		});
		return failed.empty() ? fmt::format("reaction {} made", value) : failed;
	}
	case Kind::ScriptCreateFromCreature:
	{
		// The creature, the scale, the place's x, y and z, then the species, which the native takes off the stack the
		// other way round; it gives back the new creature
		const auto failed = CallScriptNative(
		    "CREATURE_CREATE_RELATIVE_TO_CREATURE", 6,
		    [object, amount, point, value](lhvm::LHVM& vm) {
			    vm.Pusho(object);
			    vm.Pushf(amount);
			    vm.Pushv(point.x);
			    vm.Pushv(0.0f);
			    vm.Pushv(point.y);
			    vm.Pushi(value);
		    },
		    1);
		if (!failed.empty())
		{
			return failed;
		}
		const auto id = Locator::vm::value().Pop().uintVal;
		const auto& registry = Locator::entitiesRegistry::value();
		const auto entity = static_cast<entt::entity>(id);
		const auto* body =
		    id != 0 && registry.Valid(entity) ? registry.TryGet<const ecs::components::Creature>(entity) : nullptr;
		if (body == nullptr)
		{
			return "made nothing";
		}
		made = entity;
		return fmt::format("made creature {} of species {} for player {}, size {:.2f}, strength {:.2f}", id,
		                   static_cast<int>(body->species), static_cast<int>(body->owner), static_cast<double>(body->size),
		                   static_cast<double>(body->strength));
	}
	case Kind::ScriptSpeed:
	{
		const auto speed = static_cast<int32_t>(script::ObjectPropertyType::Speed);
		const auto scale = static_cast<int32_t>(script::ObjectPropertyType::Scale);
		auto failed = CallScriptNative("SET_PROPERTY", 3, [object, speed, amount](lhvm::LHVM& vm) {
			vm.Pushi(speed);
			vm.Pusho(object);
			vm.Pushf(amount);
		});
		if (!failed.empty())
		{
			return failed;
		}
		failed = CallScriptNative(
		    "GET_PROPERTY", 2,
		    [object, scale](lhvm::LHVM& vm) {
			    vm.Pushi(scale);
			    vm.Pusho(object);
		    },
		    1);
		if (!failed.empty())
		{
			return failed;
		}
		const auto shown = Locator::vm::value().Pop().floatVal;
		const auto* locomotion = Locator::entitiesRegistry::value().TryGet<const ecs::components::CreatureLocomotion>(creature);
		return fmt::format("speed share {:.2f}, scale {:.2f}",
		                   static_cast<double>(locomotion != nullptr ? locomotion->fraction : 0.0f),
		                   static_cast<double>(shown));
	}
	default:
		return {};
	}
}

/// A script's play on the creature, SET_SCRIPT_ULONG of the action and how many times then SET_SCRIPT_STATE, or its
/// PLAYED, through the natives themselves, so that the run goes through the natives' creature branches as well as the
/// creature's mind
std::string GiveScriptPlay(entt::entity creature, const Command& command)
{
	const auto object = static_cast<uint32_t>(entt::to_integral(creature));
	if (command.kind == Kind::ScriptPlayed)
	{
		// The thing, and the answer back
		const auto failed = CallScriptNative(
		    "PLAYED", 1, [object](lhvm::LHVM& vm) { vm.Pusho(object); }, 1);
		if (!failed.empty())
		{
			return failed;
		}
		lhvm::DataType type {};
		const auto value = Locator::vm::value().Pop(type);
		return value.intVal != 0 ? "true" : "false";
	}
	// The thing, the action and the times, which the native takes off the stack the other way round
	const auto animation = static_cast<int32_t>(command.value);
	const auto plays = static_cast<int32_t>(command.plays);
	auto failed = CallScriptNative("SET_SCRIPT_ULONG", 3, [object, animation, plays](lhvm::LHVM& vm) {
		vm.Pusho(object);
		vm.Pushi(animation);
		vm.Pushi(plays);
	});
	if (failed.empty())
	{
		// The thing, then a state the creature's play does not look at
		constexpr int32_t k_InScriptState = 200;
		failed = CallScriptNative("SET_SCRIPT_STATE", 2, [object](lhvm::LHVM& vm) {
			vm.Pusho(object);
			vm.Pushi(k_InScriptState);
		});
	}
	if (!failed.empty())
	{
		return failed;
	}
	return ecs::script_held::IsControlledByScript(creature) ? "under the script's control" : "not taken: in its own control";
}

/// A script's GET_PROPERTY of HEALTH on the thing, through the native: the value it gives back, as the script reads
/// it, and as which type
std::string ScriptHealth(entt::entity thing)
{
	const auto object = static_cast<uint32_t>(entt::to_integral(thing));
	// The property first, then the thing, which the native takes off the stack the other way round
	const auto failed = CallScriptNative(
	    "GET_PROPERTY", 2,
	    [object](lhvm::LHVM& vm) {
		    vm.Pushi(static_cast<int32_t>(script::ObjectPropertyType::Health));
		    vm.Pusho(object);
	    },
	    1);
	if (!failed.empty())
	{
		return failed;
	}
	lhvm::DataType type {};
	const auto value = Locator::vm::value().Pop(type);
	return type == lhvm::DataType::Float ? fmt::format("{:.3f}", value.floatVal)
	                                     : fmt::format("{} (not a float)", value.intVal);
}

/// A script's GET_TOTEM_STATUE of a town centre, then its SET_PROPERTY of a property to a value on that totem, through the
/// natives: what was done
std::string ScriptTotemProperty(entt::entity townCentre, int32_t property, float value)
{
	const auto centre = static_cast<uint32_t>(entt::to_integral(townCentre));
	auto failed = CallScriptNative(
	    "GET_TOTEM_STATUE", 1, [centre](lhvm::LHVM& vm) { vm.Pusho(centre); }, 1);
	if (!failed.empty())
	{
		return failed;
	}
	lhvm::DataType type {};
	const auto totem = Locator::vm::value().Pop(type).uintVal;
	if (totem == 0)
	{
		return "no totem";
	}
	// The property, the thing, then the value, which the native takes off the stack the other way round
	failed = CallScriptNative("SET_PROPERTY", 3, [property, totem, value](lhvm::LHVM& vm) {
		vm.Pushi(property);
		vm.Pusho(totem);
		vm.Pushf(value);
	});
	return failed.empty() ? fmt::format("set to {:.3f}", value) : failed;
}

/// A script's FLOCK_CREATE at a point and its POPULATE_CONTAINER of that flock, through the natives: the flock and how
/// many members it has then, as the scripts' ID_SIZE reads it
std::string ScriptPopulate(glm::vec3 at, int32_t type, uint32_t subtype, float quantity)
{
	// the point's x, y and z, which the native takes off the stack the other way round
	auto failed = CallScriptNative(
	    "FLOCK_CREATE", 3,
	    [at](lhvm::LHVM& vm) {
		    vm.Pushf(at.x);
		    vm.Pushf(at.y);
		    vm.Pushf(at.z);
	    },
	    1);
	if (!failed.empty())
	{
		return failed;
	}
	lhvm::DataType type_ {};
	const auto flock = Locator::vm::value().Pop(type_).uintVal;
	// the container, the quantity, the type and then the subtype, which the native takes off the other way round
	failed = CallScriptNative("POPULATE_CONTAINER", 4, [flock, quantity, type, subtype](lhvm::LHVM& vm) {
		vm.Pusho(flock);
		vm.Pushf(quantity);
		vm.Pushi(type);
		vm.Pushi(static_cast<int32_t>(subtype));
	});
	if (!failed.empty())
	{
		return failed;
	}
	const auto size = ecs::script_containers::Size(flock);
	return fmt::format("flock {} with {} members", flock, size.has_value() ? static_cast<int>(*size) : -1);
}

/// A script's CREATE at a point through the native: what it made, and for a creature its owner and whether it takes up
/// reactions
std::string ScriptCreate(glm::vec3 at, int32_t type, uint32_t subtype)
{
	// the type, the subtype, then the point's x, y and z, which the native takes off the stack the other way round
	const auto failed = CallScriptNative(
	    "CREATE", 5,
	    [at, type, subtype](lhvm::LHVM& vm) {
		    vm.Pushi(type);
		    vm.Pushi(static_cast<int32_t>(subtype));
		    vm.Pushf(at.x);
		    vm.Pushf(at.y);
		    vm.Pushf(at.z);
	    },
	    1);
	if (!failed.empty())
	{
		return failed;
	}
	lhvm::DataType made {};
	const auto object = Locator::vm::value().Pop(made).uintVal;
	if (object == 0)
	{
		return "nothing made";
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto thing = static_cast<entt::entity>(object);
	const auto* creature = registry.Valid(thing) ? registry.TryGet<const Creature>(thing) : nullptr;
	if (creature == nullptr)
	{
		return fmt::format("thing {}", object);
	}
	return fmt::format("creature {} of player {}, reactions {}", object, static_cast<int>(creature->owner),
	                   registry.AllOf<CreatureReactionsOff>(thing) ? "off" : "on");
}

/// A script's snapshot of the testbed's challenge through the native, SNAPSHOT (0), UPDATE_SNAPSHOT (1) or
/// UPDATE_SNAPSHOT_PICTURE (2), with the command's success and alignment: then every challenge record, and the local
/// player's alignment
std::string ScriptChallengeSnapshot(const Command& command)
{
	constexpr int32_t k_Challenge = 90;
	constexpr int32_t k_Title = 2456;
	// The reminder script is the string at the start of the scripts' data, and its one argument a number
	constexpr int32_t k_ReminderScript = 0;
	constexpr int32_t k_Argument = 7;
	constexpr glm::vec3 k_Position {10.0f, 20.0f, 30.0f};
	constexpr glm::vec3 k_Focus {40.0f, 50.0f, 60.0f};
	const float success = command.amount;
	const float alignment = command.alignment;
	const auto pushVector = [](lhvm::LHVM& vm, glm::vec3 v) {
		vm.Pushv(v.x);
		vm.Pushv(v.y);
		vm.Pushv(v.z);
	};
	// Every value in the order a script pushes it, which the native takes off the stack the other way round
	std::string failed;
	if (command.value == 0)
	{
		failed = CallScriptNative("SNAPSHOT", -1, [&](lhvm::LHVM& vm) {
			vm.Pushb(false);
			pushVector(vm, k_Position);
			pushVector(vm, k_Focus);
			vm.Pushf(success);
			vm.Pushf(alignment);
			vm.Pushi(k_Title);
			vm.Pushi(k_ReminderScript);
			vm.Pushi(k_Argument);
			vm.Pushi(1);
			vm.Pushi(k_Challenge);
		});
	}
	else if (command.value == 1)
	{
		failed = CallScriptNative("UPDATE_SNAPSHOT", -1, [&](lhvm::LHVM& vm) {
			vm.Pushf(success);
			vm.Pushf(alignment);
			vm.Pushi(k_Title);
			vm.Pushi(k_ReminderScript);
			vm.Pushi(k_Argument);
			vm.Pushi(1);
			vm.Pushi(k_Challenge);
		});
	}
	else
	{
		failed = CallScriptNative("UPDATE_SNAPSHOT_PICTURE", 11, [&](lhvm::LHVM& vm) {
			pushVector(vm, k_Position);
			pushVector(vm, k_Focus);
			vm.Pushf(success);
			vm.Pushf(alignment);
			vm.Pushi(k_Title);
			vm.Pushi(1);
			vm.Pushi(k_Challenge);
		});
	}
	if (!failed.empty())
	{
		return failed;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	std::string out;
	for (const auto entity : ecs::challenge_records::InOrder(registry))
	{
		const auto& record = registry.Get<ecs::components::ChallengeRecord>(entity);
		const auto* picture = registry.TryGet<ecs::components::ChallengePicture>(entity);
		out += fmt::format("challenge {} title {} success {:.3f} alignment {:.3f} reminder \"{}\" with {} arguments, "
		                   "pictures asked {} taken {}; ",
		                   record.challenge, record.title, record.success, record.alignment, record.reminderScript,
		                   record.arguments.size(), picture != nullptr ? picture->asked : 0u,
		                   picture != nullptr ? picture->taken : 0u);
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	return out + fmt::format("player's alignment {:.5f}", ecs::effects::alignment::Get(player));
}

/// A script's OBJECT_DELETE of the player's temple's heart with the temple's explosion (mode 3), through the native: what
/// came of it
std::string ScriptDeleteTemple(PlayerNames player)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto heart = worship::citadel::HeartOf(worship::citadel::Of(player));
	if (heart == entt::null)
	{
		return "the player has no temple";
	}
	const bool wasDestroying = registry.Get<const ecs::components::Temple>(heart).destroying;
	// The thing first, then the mode, which the native takes off the stack the other way round
	constexpr int32_t k_TempleExplode = 3;
	const auto object = static_cast<uint32_t>(entt::to_integral(heart));
	const auto failed = CallScriptNative("OBJECT_DELETE", 2, [object](lhvm::LHVM& vm) {
		vm.Pusho(object);
		vm.Pushi(k_TempleExplode);
	});
	if (!failed.empty())
	{
		return failed;
	}
	return wasDestroying ? "its model broken up, its destruction goes on" : "its destruction starts";
}

/// A script's OBJECT_DELETE of a thing, plainly deleted (mode 0), through the native: what came of it
std::string ScriptDelete(entt::entity thing)
{
	// The thing first, then the mode, which the native takes off the stack the other way round
	constexpr int32_t k_PlainDelete = 0;
	const auto object = static_cast<uint32_t>(entt::to_integral(thing));
	const auto failed = CallScriptNative("OBJECT_DELETE", 2, [object](lhvm::LHVM& vm) {
		vm.Pusho(object);
		vm.Pushi(k_PlainDelete);
	});
	return failed.empty() ? "deleted, it leaves the map on the next turn" : failed;
}

/// A script's SNAPSHOT of a challenge through the native, with no reminder script and no arguments, and where the camera
/// was and looked when its picture was taken (none: zeros)
std::string ScriptSnapshot(uint32_t challenge, uint32_t title, float success, float alignment, bool quest,
                           const std::optional<std::pair<glm::vec3, glm::vec3>>& view)
{
	// The native takes off the challenge first, then the count of arguments, the reminder script, the title, the
	// alignment, the success, the focus and the position (z first) and whether it is a quest
	return CallScriptNative("SNAPSHOT", -1, [=](lhvm::LHVM& vm) {
		vm.Pushi(quest ? 1 : 0);
		const auto [position, focus] = view.value_or(std::make_pair(glm::vec3(0.0f), glm::vec3(0.0f)));
		const std::array<glm::vec3, 2> points {position, focus};
		for (const auto& point : points)
		{
			vm.Pushf(point.x);
			vm.Pushf(point.y);
			vm.Pushf(point.z);
		}
		vm.Pushf(success);
		vm.Pushf(alignment);
		vm.Pushi(static_cast<int32_t>(title));
		vm.Pushi(-1); // no reminder script: a string past the script's data reads as none
		vm.Pushi(0);
		vm.Pushi(static_cast<int32_t>(challenge));
	});
}

/// A script's CREATE_REWARD of a type at a point, through the native: the object it gives back, as the script reads it
std::string ScriptReward(glm::vec3 at, int32_t type, bool fromSky)
{
	// The type first, then the point's x, y and z, then whether it falls, which the native takes off the stack the
	// other way round
	const auto failed = CallScriptNative(
	    "CREATE_REWARD", 5,
	    [at, type, fromSky](lhvm::LHVM& vm) {
		    vm.Pushi(type);
		    vm.Pushv(at.x);
		    vm.Pushv(at.y);
		    vm.Pushv(at.z);
		    vm.Pushi(fromSky ? 1 : 0);
	    },
	    1);
	if (!failed.empty())
	{
		return failed;
	}
	const auto object = Locator::vm::value().Pop().uintVal;
	return object != 0 ? fmt::format("object {}", object) : "nothing";
}

/// A script's GET_PROPERTY of a property of a thing, through the native: the number it gives back
float ScriptProperty(uint32_t object, script::ObjectPropertyType property)
{
	// The property first, then the thing, which the native takes off the stack the other way round
	const auto failed = CallScriptNative(
	    "GET_PROPERTY", 2,
	    [object, property](lhvm::LHVM& vm) {
		    vm.Pushi(static_cast<int32_t>(property));
		    vm.Pusho(object);
	    },
	    1);
	return failed.empty() ? Locator::vm::value().Pop().floatVal : 0.0f;
}

/// A puzzle totem's HEIGHT and MAX_HEIGHT as a script reads them
std::string PuzzleTotemHeights(uint32_t object)
{
	return fmt::format("HEIGHT {:.0f} of MAX_HEIGHT {:.0f}", ScriptProperty(object, script::ObjectPropertyType::Height),
	                   ScriptProperty(object, script::ObjectPropertyType::MaxHeight));
}

/// A script's CREATE_WITH_ANGLE_AND_SCALE of a totem at a point, then its HEIGHT set to that part of its MAX_HEIGHT and,
/// when asked, its SET_ID_PICKUPABLE false, through the natives: what was made
std::string ScriptPuzzleTotem(glm::vec3 at, uint32_t info, float angle, float scale, float heightShare, bool unpickupable)
{
	// The angle, the scale, the type, the sub-type and the point's x, y and z, which the native takes off the stack the
	// other way round
	auto failed = CallScriptNative(
	    "CREATE_WITH_ANGLE_AND_SCALE", 7,
	    [at, info, angle, scale](lhvm::LHVM& vm) {
		    vm.Pushf(angle);
		    vm.Pushf(scale);
		    vm.Pushi(static_cast<int32_t>(script::ObjectType::Totem));
		    vm.Pushi(static_cast<int32_t>(info));
		    vm.Pushv(at.x);
		    vm.Pushv(at.y);
		    vm.Pushv(at.z);
	    },
	    1);
	if (!failed.empty())
	{
		return failed;
	}
	const auto totem = Locator::vm::value().Pop().uintVal;
	if (totem == 0)
	{
		return "nothing";
	}
	const float height = ScriptProperty(totem, script::ObjectPropertyType::MaxHeight) * heightShare;
	// The property, the thing, then the value
	failed = CallScriptNative("SET_PROPERTY", 3, [totem, height](lhvm::LHVM& vm) {
		vm.Pushi(static_cast<int32_t>(script::ObjectPropertyType::Height));
		vm.Pusho(totem);
		vm.Pushf(height);
	});
	if (failed.empty() && unpickupable)
	{
		// Whether it may be picked up, then the thing
		failed = CallScriptNative("SET_ID_PICKUPABLE", 2, [totem](lhvm::LHVM& vm) {
			vm.Pushi(0);
			vm.Pusho(totem);
		});
	}
	return failed.empty() ? fmt::format("object {}, {}", totem, PuzzleTotemHeights(totem)) : failed;
}

/// A script's DANCE_CREATE of a dance at a point, around a thing or none, for a time, through the native: the object it
/// gives back, as the script reads it
std::string ScriptDance(uint32_t around, int32_t type, glm::vec3 at, float time, uint32_t& made)
{
	// The thing first, then the dance, the point's x, y and z and the time, which the native takes off the stack the
	// other way round
	const auto failed = CallScriptNative(
	    "DANCE_CREATE", 6,
	    [around, type, at, time](lhvm::LHVM& vm) {
		    vm.Pusho(around);
		    vm.Pushi(type);
		    vm.Pushv(at.x);
		    vm.Pushv(at.y);
		    vm.Pushv(at.z);
		    vm.Pushf(time);
	    },
	    1);
	if (!failed.empty())
	{
		return failed;
	}
	const auto object = Locator::vm::value().Pop().uintVal;
	made = object;
	return object != 0 ? fmt::format("object {}", object) : "nothing";
}

/// A script's FLOCK_ATTACH of a thing to a container, not as its leader, through the native: what it gives back
std::string ScriptAttach(uint32_t object, uint32_t container)
{
	// The thing first, then the container, then whether it leads, which the native takes off the stack the other way
	// round
	const auto failed = CallScriptNative(
	    "FLOCK_ATTACH", 3,
	    [object, container](lhvm::LHVM& vm) {
		    vm.Pusho(object);
		    vm.Pusho(container);
		    vm.Pushi(0);
	    },
	    1);
	if (!failed.empty())
	{
		return failed;
	}
	return fmt::format("{}", Locator::vm::value().Pop().uintVal);
}

/// A script's WALK_PATH of the thing along a track of the game's cameras, through the native: whether it is on the list
/// of things walking a track then
std::string ScriptWalkPath(entt::entity thing, const Command& command)
{
	const auto object = static_cast<uint32_t>(entt::to_integral(thing));
	const auto forward = command.pathBackwards ? 0 : 1;
	const auto track = static_cast<int32_t>(command.value);
	const auto from = command.pathFrom;
	const auto to = command.pathTo;
	// The thing first, then which way, the track, and the shares it starts and stops at, which the native takes off the
	// stack the other way round
	const auto failed = CallScriptNative("WALK_PATH", 5, [object, forward, track, from, to](lhvm::LHVM& vm) {
		vm.Pusho(object);
		vm.Pushi(forward);
		vm.Pushi(track);
		vm.Pushf(from);
		vm.Pushf(to);
	});
	if (!failed.empty())
	{
		return failed;
	}
	if (!Locator::walkPathSystem::has_value())
	{
		return "the game has no walk paths";
	}
	const auto& walking = Locator::walkPathSystem::value().Walking();
	if (std::ranges::find(walking, thing) == walking.end())
	{
		return "not walking: no such track, or it walks no track this way";
	}
	return "walking";
}

/// A script's SET_INTERFACE_INTERACTION of a level, through the native: empty when it was called, else why not
std::string ScriptInterfaceLevel(int32_t level)
{
	return CallScriptNative("SET_INTERFACE_INTERACTION", 1, [level](lhvm::LHVM& vm) { vm.Pushi(level); });
}

/// The profiler's stages as benchmarks name them: the drawing of the reflection and of the main pass told apart, and
/// everything from drawing the scene on counted as rendering
std::vector<benchmark::StageInfo> BenchmarkStages()
{
	static const std::vector<std::string> k_Names = [] {
		std::vector<std::string> names;
		for (size_t i = 0; i < Profiler::k_StageNames.size(); ++i)
		{
			const auto stage = static_cast<Profiler::Stage>(i);
			std::string name(Profiler::k_StageNames.at(i));
			if (stage > Profiler::Stage::ReflectionPass && stage <= Profiler::Stage::ReflectionDrawSprites)
			{
				name = "Reflection " + name;
			}
			else if (stage > Profiler::Stage::MainPass && stage <= Profiler::Stage::MainPassDrawSprites)
			{
				name = "Main " + name;
			}
			names.push_back(std::move(name));
		}
		return names;
	}();
	std::vector<benchmark::StageInfo> stages;
	for (size_t i = 0; i < k_Names.size(); ++i)
	{
		// the scene's draw up to the renderer's frame, and the draw's preparation; every other stage is update work
		const auto stage = static_cast<Profiler::Stage>(i);
		const bool render = (stage >= Profiler::Stage::SceneDraw && stage <= Profiler::Stage::RendererFrame) ||
		                    (stage >= Profiler::Stage::PreDraw && stage <= Profiler::Stage::DrawUpload);
		stages.push_back({.name = k_Names.at(i), .render = render});
	}
	return stages;
}

/// The kind of build a benchmark's results were measured in
#ifdef NDEBUG
constexpr std::string_view k_Build = "optimised";
#else
constexpr std::string_view k_Build = "debug";
#endif

double Milliseconds(std::chrono::system_clock::duration duration)
{
	return std::chrono::duration<double, std::milli>(duration).count();
}

/// The time a stage took in a frame: its last run, if it ran within the frame, as the profiler's own summary counts it
double StageMilliseconds(const Profiler::Entry& entry, const Profiler::Scope& stage)
{
	if (!stage.finalized || stage.start < entry.frameStart || stage.end > entry.frameEnd)
	{
		return 0.0;
	}
	return Milliseconds(stage.end - stage.start);
}

std::string_view MoveResultName(ecs::systems::CreatureLocomotionSystemInterface::MoveResult result)
{
	using MoveResult = ecs::systems::CreatureLocomotionSystemInterface::MoveResult;
	switch (result)
	{
	case MoveResult::InvalidDestination:
		return "nowhere to stand there";
	case MoveResult::Busy:
		return "it can't walk";
	case MoveResult::Started:
	default:
		return "on its way";
	}
}

/// Where a creature faces on the land; its mesh looks back along +z
glm::vec2 AheadOf(const Transform& transform)
{
	const auto ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f));
	return {ahead.x, ahead.z};
}

/// The debug windows leave the mouse alone while a scenario drives it, so that the real pointer resting on one of them
/// doesn't take the hand's place
void KeepDebugWindowsOffTheMouse(bool off)
{
	if (ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}
	auto& io = ImGui::GetIO();
	io.ConfigFlags = off ? (io.ConfigFlags | ImGuiConfigFlags_NoMouse) : (io.ConfigFlags & ~ImGuiConfigFlags_NoMouse);
}

/// Whether a command sends its creature somewhere, or has it face, throw or point somewhere
bool HasPoint(Kind kind)
{
	return kind == Kind::WalkTo || kind == Kind::RunTo || kind == Kind::FleeFrom || kind == Kind::TurnToFace ||
	       kind == Kind::ThrowAt || kind == Kind::PointAt;
}

/// The task the scenario's cinema bars are held by: no script's task has this number
constexpr uint32_t k_ScenarioWideScreenTask = 0xFFFFFFFFu;

/// Whether a command is the player's alone, given before a creature is looked for
bool IsPlayerCommand(Kind kind)
{
	return kind == Kind::HoldSeed || kind == Kind::DrawGesture || kind == Kind::SummonSeed || kind == Kind::PressKey ||
	       kind == Kind::HandTakeFireBall || kind == Kind::HandTake;
}

std::string_view Started(bool started)
{
	return started ? "started" : "can't";
}

/// The hand lets go of the creature it holds, and how it was treated goes to the creature's mind at the next turn, as
/// the hand's own letting go sends it
void LetGoOfCreature(ecs::systems::CreatureHandSystemInterface& hand)
{
	if (const auto given = hand.Release())
	{
		game_packets::Packet packet {.type = game_packets::Type::CreatureFeedback, .object = given->creature};
		packet.data[0] = given->feedback;
		game_packets::Push(packet);
	}
}

/// The camera's fields of view across and up and down, in radians
glm::vec2 FieldsOfView(const Camera& camera)
{
	const auto horizontal = camera.GetHorizontalFieldOfView();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	return {horizontal, 2.0f * std::atan(std::tan(horizontal * 0.5f) / std::max(aspect, 0.1f))};
}

/// Why the player's leash was refused, for the readout
std::string RefusedText(const ecs::systems::LeashSystemInterface& leashes, PlayerNames player)
{
	const auto refused = leashes.LastRefusal(player);
	return refused.has_value() ? fmt::format("refused: {}", creature_leash::Describe(refused->why)) : "can't";
}

/// The hour of the day jumps, as a script sets it: the clock goes on from there and the sky follows at once
void SetHour(float hour)
{
	if (!Locator::dayNightClock::has_value())
	{
		return;
	}
	auto& clock = Locator::dayNightClock::value().Clock();
	clock.SetScriptTime(hour);
	if (Locator::skySystem::has_value())
	{
		Locator::skySystem::value().SetTime(clock.GetScriptTime());
	}
}

/// A villager's, animal's or building's life and a villager's or animal's poison as the scenario gives them. An animal
/// of no life is killed as an effect that takes its last life kills it; a bird is first put down on the land, so that
/// it dies there rather than falls
void SetLifeAndPoison(entt::entity entity, const ObjectSetup& object)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A building is hurt down to it, as anything that hurts it does, which opens a site for its repair
	if (entity != entt::null && registry.Valid(entity) && registry.AllOf<ecs::components::Abode>(entity) &&
	    object.life.has_value())
	{
		static_cast<void>(ecs::abodes::ReduceLife(entity, 1.0f - std::clamp(*object.life, 0.0f, 1.0f), std::nullopt));
		return;
	}
	if (entity == entt::null || !registry.Valid(entity) ||
	    !registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity))
	{
		return;
	}
	if (object.life.has_value())
	{
		ecs::life::SetLife(entity, std::clamp(*object.life, 0.0f, 1.0f));
	}
	if (const auto* animal = registry.TryGet<const ecs::components::Animal>(entity);
	    animal != nullptr && object.life.has_value() && *object.life <= 0.0f)
	{
		auto& transform = registry.Get<ecs::components::Transform>(entity);
		if (ecs::animal_ai::IsDoveClass(animal->type) && Locator::terrainSystem::has_value())
		{
			transform.position.y =
			    Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
		}
		ecs::animal_ai::DestroyedByEffect(entity);
	}
	if (object.poisoned)
	{
		ecs::life::SetPoisoned(entity, true);
	}
}

/// An animal made hungry: its hunger counter at its kind's full, as its needs leave it once its hunger has had its
/// turns. Its brain is made now if it has none yet, as the first reaction offered to it makes it
void MakeHungry(entt::entity entity, const ObjectSetup& object)
{
	if (!object.hungry || entity == entt::null)
	{
		return;
	}
	auto* brain = ecs::animal_ai::detail::BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	const auto& animal = Locator::entitiesRegistry::value().Get<const ecs::components::Animal>(entity);
	brain->hunger = static_cast<int16_t>(ecs::animal_ai::detail::InfoOf(animal).hunger);
}

/// A villager given a script's speed is the script's own from the start: created by it and held in one of its
/// variables
void TakeIntoScript(entt::entity entity, const ObjectSetup& object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!object.scriptSpeed.has_value() || entity == entt::null || !registry.Valid(entity) ||
	    !registry.AllOf<ecs::components::Villager>(entity))
	{
		return;
	}
	ecs::script_held::AddScriptThing(entity, true);
	ecs::script_held::IncrementReference(entity);
}

/// A script's SET_PROPERTY of SPEED on the villager, through the native: in metres a turn as the scripts give it, the
/// whole speed x 10 / 65536, which the game turns back into the same whole speed. Empty when it was called
std::string ScriptSpeed(entt::entity villager, uint16_t whole)
{
	const auto object = static_cast<uint32_t>(entt::to_integral(villager));
	const float metres = static_cast<float>(whole) * 10.0f / 65536.0f;
	// The property, the thing, then the value, which the native takes off the stack the other way round
	return CallScriptNative("SET_PROPERTY", 3, [object, metres](lhvm::LHVM& vm) {
		vm.Pushi(static_cast<int32_t>(script::ObjectPropertyType::Speed));
		vm.Pusho(object);
		vm.Pushf(metres);
	});
}

glm::ivec2 WindowSize()
{
	return Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(1);
}

/// The records of the game's hand demo of that name, read through the resources as its playback reads them; none when
/// it cannot be read
std::vector<hand_demo::Record> ReadHandDemo(std::string_view name)
{
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return {};
	}
	try
	{
		return hand_demo::ReadRecords(name);
	}
	catch (const std::exception&)
	{
		return {};
	}
}

/// Whether the mouse's own events are dropped before the game sees them, as in the test runs
bool MouseEventsDropped()
{
	return input::IgnoreRealInput() || input::FixedMouse().has_value();
}

/// While the tattoo editor's dialog is open it takes the mouse, as the game hands it the window's events: the scenario's
/// pointer is handed to it the same way
bool ToTattooEditor(gui::GameInterface* interface, const SDL_Event& event)
{
	if (interface == nullptr || !interface->GetTattooEditor().IsOpen() || !Locator::windowing::has_value())
	{
		return false;
	}
	return interface->ProcessEvent(event, static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()));
}

/// The pointer's button going down or up where it is, as the window's mouse event, for the tattoo editor's dialog;
/// whether the open dialog took it
bool ToTattooEditor(gui::GameInterface* interface, glm::ivec2 position, size_t button, bool down)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
	event.button.button = static_cast<uint8_t>(button);
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	event.button.x = position.x;
	event.button.y = position.y;
	return ToTattooEditor(interface, event);
}

/// Sends a move of the pointer to where it is, by so much, with the buttons held, as the mouse does, for the camera's
/// drags; the hand follows the fixed cursor instead
void PushMotion(gui::GameInterface* interface, glm::ivec2 position, uint32_t buttons, glm::ivec2 moved)
{
	SDL_Event event {};
	event.type = SDL_MOUSEMOTION;
	event.motion.windowID = Locator::windowing::has_value() ? Locator::windowing::value().GetID() : 0;
	event.motion.state = buttons;
	event.motion.x = position.x;
	event.motion.y = position.y;
	event.motion.xrel = moved.x;
	event.motion.yrel = moved.y;
	SDL_PushEvent(&event);
	// The test runs drop the window's own mouse events, so the open editor is handed this one directly
	if (MouseEventsDropped())
	{
		ToTattooEditor(interface, event);
	}
}
} // namespace

void Runner::BeginRun(const Scenario& scenario)
{
	_scenario = &scenario;
	_running = true;
	_seconds = 0.0f;
	_timeline = {};
	_handLightColour.reset();
	_vortices.clear();
	_vortexLandLogged = false;
	_creatures.clear();
	_objects.clear();
	_throwsDue.clear();
	for (const auto& setup : scenario.throws)
	{
		_throwsDue.emplace_back(setup.delaySeconds);
	}
	_presses.clear();
	_particles.clear();
	_miracles.clear();
	_nextMiracleLog = 0.0f;
	_nextBuildingLog = 0.0f;
	_walks.clear();
	_started.clear();
	_log.clear();
	_sweep.reset();
	_handWatchSeconds = 0.0f;
	_holdPointer = false;
	_handHoldPoint.reset();
	_fireBallTake.reset();
	_lastDance = 0;
	_shot.reset();
	_crowdCreatures.clear();
	_village = {};
	_crowdNext = 0;
	_crowdAbodes.clear();
	_crowdEntities.clear();
	_crowdProgress = {};
	_settledFrames = 0;
	_recorder.reset();
	_liveResults = {};
	_framesSinceSummary = 0;
	_measured = false;
}

void Runner::StartOnLand(std::vector<const Scenario*> chapters)
{
	Stop();
	std::erase(chapters, nullptr);
	_chapters = std::move(chapters);
	_chapter = 0;
	_landDoneLogged = false;
	_onLand = !_chapters.empty();
	if (_onLand)
	{
		BeginChapter(0);
	}
}

void Runner::BeginChapter(size_t chapter)
{
	const auto& scenario = *_chapters.at(chapter);
	_chapter = chapter;
	// The log of the chapters before it is kept, as one land's story
	auto log = std::move(_log);
	BeginRun(scenario);
	_log = std::move(log);
	// The land as the game loaded it: nothing is set out, and the points are map coordinates
	_land = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	_middle = testbed_land_timeline::Origin(true, glm::vec2(0.0f));
	RefreshLandCreature();
	Log(fmt::format("Land timeline {} of {}: {}", chapter + 1, _chapters.size(), scenario.name));
}

void Runner::RefreshLandCreature()
{
	// The player's creature, once they have one: the only creature a land timeline's commands act on
	std::optional<entt::entity> led;
	if (Locator::leashSystem::has_value())
	{
		led = Locator::leashSystem::value().PlayersCreature(openblack::creature::LocalPlayer());
	}
	_creatures.assign(1, led.value_or(entt::null));
}

void Runner::NextLandChapter()
{
	if (const auto next = testbed_land_timeline::NextChapter(_chapter, _chapters.size(), _timeline.done))
	{
		BeginChapter(*next);
	}
	else if (_timeline.done && !_landDoneLogged)
	{
		_landDoneLogged = true;
		Log(fmt::format("{:.1f}s: the land timeline is done", _seconds));
	}
}

void Runner::Start(const Scenario& scenario)
{
	Stop();
	_onLand = false;
	_chapters.clear();
	_chapter = 0;
	// One written for a land plays only there, as a land entry's timeline= plays it
	if (scenario.onLand)
	{
		_scenario = &scenario;
		_running = false;
		Log(fmt::format("Problem: {} is played on a land, through a land entry's timeline=, not on the testbed", scenario.id));
		return;
	}
	// The camera is the player's again before the scenario frames it, and the cave closes
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	if (Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().IsOpen())
	{
		Locator::creatureCaveSystem::value().Close();
	}
	BeginRun(scenario);
	if (scenario.crowd.has_value())
	{
		const auto& crowd = *scenario.crowd;
		if (crowd.kind == Crowd::Kind::Creatures)
		{
			_crowdCreatures = LayOutCreatures(crowd.count, crowd.seed);
			_crowdProgress.total = _crowdCreatures.size();
		}
		else
		{
			_village = LayOutVillagers(crowd.count, crowd.seed);
			_crowdProgress.total = _village.towns.size() + _village.abodes.size() + _village.villagers.size();
		}
		_recorder = std::make_unique<benchmark::Recorder>(BenchmarkStages(), _benchmark.frames);
	}

	// The scenario's hand demo is read before the land is made: one recorded with its camera close to the ground is
	// played over a lower plane
	_demoRecords.clear();
	_demoStarted = false;
	auto planeAltitude = flat_land::k_Altitude;
	if (const auto& demo = scenario.fixtures.handDemo; demo.has_value())
	{
		_demoRecords = ReadHandDemo(demo->name);
		if (_demoRecords.empty())
		{
			Log(fmt::format("Problem: the hand demo {} could not be read", demo->name));
		}
		planeAltitude =
		    demo->planeAltitude.value_or(testbed_demo::PlaneAltitudeUnder(_demoRecords).value_or(flat_land::k_Altitude));
	}

	// A scenario that writes into the game's folder runs only on a copy of the game's data: checked before anything is
	// changed, so that nothing is left to undo
	if (!MayRunHere(scenario))
	{
		_running = false;
		return;
	}

	// Every scenario starts at the normal speed, as it did when a new land reset it: a land now keeps the speed, as the
	// original does, so a speed left by the last scenario or the window is put back here
	_host.SetGameSpeed(1.0f);
	// A fresh testbed: the last one's creatures, objects, footprints, weather and scripts all go with it
	_host.LoadTestbed(planeAltitude);
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		_running = false;
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	_middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	_land = &land;
	_presses = DemoPresses(planeAltitude);

	SetUpEnvironment(scenario.environment);
	// The testbed's lake has a water point at the middle of its south edge, as a land's script marks its places to
	// drink, in the scenes that ask for it. The creature drinks there.
	if (scenario.environment.lakeWaterPoint)
	{
		const glm::vec2 drinkAt {flat_land::k_LakeCentre.x,
		                         flat_land::k_LakeCentre.y - flat_land::k_LakeHalfExtent.y + (flat_land::k_CellSize * 0.5f)};
		auto& registry = Locator::entitiesRegistry::value();
		const auto number = static_cast<uint32_t>(registry.Size<ecs::components::DrinkWaypoint>());
		registry.Assign<ecs::components::DrinkWaypoint>(registry.Create(),
		                                                glm::vec3 {drinkAt.x, land.GetHeightAt(drinkAt), drinkAt.y}, number);
	}
	PlaceObjects(scenario, _middle);
	PlaceFootpaths(scenario, _middle);
	RunLandScript(scenario, _middle);
	PlaceBirds(scenario, _middle);
	if (scenario.townBelief.has_value())
	{
		SetTownBelief(*scenario.townBelief);
	}
	PlaceBareTowns(scenario, _middle);
	PlaceCreatures(scenario, _middle);
	PlaceDispensers(scenario);
	PlaceFixtures(scenario);
	// The hand held still over a point of the land, as a player holds the mouse there: the cursor is kept over it
	if (scenario.hand.has_value() && Locator::inputState::has_value() && Locator::windowing::has_value())
	{
		const auto at = MapPoint(_middle, scenario.hand->offset);
		_handHoldPoint = glm::vec3(at.x, land.GetHeightAt(at), at.y);
		_holdPointer = true;
	}
	for (const auto& miracle : scenario.miracles)
	{
		_miracles.push_back({.nextAt = miracle.delaySeconds, .letGoAt = std::nullopt, .spell = entt::null});
	}
	for (size_t i = 0; i < scenario.particles.size(); ++i)
	{
		_particles.push_back({StartParticle(i), 0.0f});
	}
	Frame(scenario.framing.shot, scenario.framing.creature, scenario.framing.distance);
	Log(fmt::format("Started {}", scenario.name));

	// What the scenario sets up that this tree has no way to yet
	std::vector<std::string_view> missing;
	if (scenario.tribalPower.has_value())
	{
		missing.emplace_back("tribal power");
	}
	if (scenario.environment.prayer.has_value())
	{
		missing.emplace_back("prayer power");
	}
	if (std::ranges::any_of(scenario.objects, [](const ObjectSetup& object) { return object.worshipAt.has_value(); }))
	{
		missing.emplace_back("villagers' walks to worship through the stones");
	}
	if (std::ranges::any_of(scenario.miracles, [](const MiracleCast& miracle) { return miracle.fromWorship; }))
	{
		missing.emplace_back("seeds summoned from worship into the hand");
	}
	if (!missing.empty())
	{
		Log(fmt::format("not in this tree yet: {}", fmt::join(missing, ", ")));
	}
	// The camera's drags and the wheel come as the mouse's own events, which the test runs drop
	if (MouseEventsDropped() && std::ranges::any_of(scenario.commands, [](const Command& command) {
		    return command.kind == Kind::PointerSweep || command.kind == Kind::WheelTurn;
	    }))
	{
		Log("Problem: the mouse's events are dropped while the real input is ignored or the mouse is fixed; the "
		    "camera won't see this scenario's drags and wheel");
	}
}

void Runner::Stop()
{
	if (!_running)
	{
		return;
	}
	_running = false;
	// Its held miracles stop, and the hand is the mouse's again
	if (Locator::magicSystem::has_value())
	{
		auto& magic = Locator::magicSystem::value();
		if (std::ranges::any_of(_miracles, [](const RunningMiracle& m) { return m.pressAt || m.releaseAt; }))
		{
			magic.ReleaseAction();
			magic.DiscardHeldSeed();
		}
		magic.DriveHand(std::nullopt);
		for (const auto& miracle : _miracles)
		{
			if (miracle.letGoAt.has_value())
			{
				Locator::magicSystem::value().CloseDown(miracle.spell);
			}
		}
	}
	_miracles.clear();
	// The hand lets go of a creature a scenario held it to
	if (Locator::creatureHandSystem::has_value() &&
	    (Locator::creatureHandSystem::value().IsHeldByCommand() ||
	     (_handGrabbed && Locator::creatureHandSystem::value().GetCreature().has_value())))
	{
		LetGoOfCreature(Locator::creatureHandSystem::value());
	}
	_handGrabbed = false;
	_scriptMade.reset();
	_textShown = false;
	RestoreSettings();
	Log("Stopped");
}

void Runner::RestoreSettings()
{
	// The mouse is the player's again
	ReleasePointer();
	// The footpaths' debug lines as they were
	if (const auto before = std::exchange(_drawFootpathsBefore, std::nullopt);
	    before.has_value() && Locator::config::has_value())
	{
		Locator::config::value().drawFootpaths = *before;
	}
	// The scenario's cinema bars go, as a script's go when its task stops
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->ReleaseWideScreenOf(k_ScenarioWideScreenTask);
	}
	// A double click not given yet is dropped, and the camera keys held go with the other keys
	_doubleClick.reset();
	_cameraKeys.reset();
	// The keys it left down are let go, as the window lets go of a real key
	for (const auto key : _keys.TakeAll())
	{
		GiveKey(key, false);
	}
	// The interface level it set is put back as it was
	if (const auto before = std::exchange(_interfaceLevelBefore, std::nullopt); before.has_value())
	{
		help::interface_interaction::Set(*before);
	}
	// And its screen fade, at once
	if (_faded && Locator::cinematicDirectorSystem::has_value())
	{
		Locator::cinematicDirectorSystem::value().FadeBackToNormal(0.0f);
	}
	_faded = false;
	// Its particle effects die away
	if (Locator::particleSystem::has_value())
	{
		for (const auto& particle : _particles)
		{
			Locator::particleSystem::value().CloseDown(particle.effect);
		}
	}
	_particles.clear();
	// The texts are the player's to click again, and the camera the player's
	_clickTexts = false;
	_followed.reset();
	_taskWaitAt.reset();
	// A land timeline set up no environment, so it has none to put back: the land's clock and creatures are the game's
	if (_onLand)
	{
		return;
	}
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		physiology.SetTimeScale(1.0f);
		physiology.SetFaintingEnabled(true);
	}
	if (Locator::footprintSystem::has_value())
	{
		Locator::footprintSystem::value().SetAprilFoolsOverride(std::nullopt);
	}
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().SetAngerStartsFights(true);
	}
	if (Locator::dayNightClock::has_value())
	{
		Locator::dayNightClock::value().Clock().SetRunning(true);
	}
}

void Runner::SetIslandWeather(Weather kind)
{
	if (kind == Weather::Clear || !Locator::weatherSystem::has_value())
	{
		return;
	}
	namespace window = debug::weather_window;
	// Rain and thunderstorms as the Weather window's presets; snow and blizzards as cold rainless storms that snow
	window::StormSettings settings;
	switch (kind)
	{
	case Weather::Rain:
		window::ApplyPreset(window::k_Presets.at(2), settings);
		break;
	case Weather::Thunderstorm:
		window::ApplyPreset(window::k_Presets.at(3), settings);
		break;
	case Weather::Snow:
	case Weather::Blizzard:
		settings.rain = 0;
		settings.overcast = 100;
		settings.temperature = static_cast<int8_t>(kind == Weather::Snow ? -5 : -10);
		settings.windStrength = kind == Weather::Snow ? 5 : 30;
		break;
	case Weather::Clear:
		break;
	}
	auto storm = window::IslandStorm(settings);
	if (kind == Weather::Snow || kind == Weather::Blizzard)
	{
		storm.weather.snow = static_cast<int8_t>(kind == Weather::Snow ? 60 : 100);
	}
	Locator::weatherSystem::value().SetStormCreationEnabled(false);
	weather::storms::Create(storm);
	Log(fmt::format("Weather over the island: {}", Name(kind)));
}

void Runner::PlaceByHand(const testbed_fixtures::Fixtures& fixtures)
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto middle = (land.GetExtent().minimum + land.GetExtent().maximum) * 0.5f;
	for (const auto& problem : testbed_fixtures::Problems(fixtures))
	{
		Log(fmt::format("Problem: {}", problem));
	}
	testbed_fixtures::Place(
	    fixtures, middle, {}, [this](size_t index) { return ObjectAt(index); },
	    [this](std::string line) { Log(std::move(line)); });
}

std::vector<glm::vec2> Runner::DemoPresses(uint8_t planeAltitude)
{
	std::vector<glm::vec2> points;
	if (_demoRecords.empty())
	{
		return points;
	}
	// The hand's ray as the game casts it: the camera's field of view across, the window's shape, the mouse to a pixel
	const testbed_demo::Lens lens {
	    .xFovDegrees = Locator::config::has_value() ? Locator::config::value().cameraXFov : 70.0f,
	    .aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f,
	};
	const auto height = testbed_demo::PlaneHeight(planeAltitude);
	for (const auto& press : testbed_demo::Presses(_demoRecords))
	{
		auto onScreen = press;
		onScreen.mouse = testbed_demo::RoundToPixel(press.mouse, WindowSize());
		const auto point = testbed_demo::PointOnPlane(onScreen, lens, height);
		if (!point.has_value())
		{
			// The presses after one that misses the land are not counted, so that a press's number stays its own
			Log(fmt::format("Problem: the hand demo's press {} does not reach the land", points.size()));
			break;
		}
		points.emplace_back(point->x, point->z);
		Log(fmt::format("Hand demo press {} at ({:.1f}, {:.1f})", points.size() - 1, point->x, point->z));
	}
	return points;
}

void Runner::StartHandDemo()
{
	if (_scenario == nullptr || _demoStarted || !_scenario->fixtures.handDemo.has_value())
	{
		return;
	}
	const auto& demo = *_scenario->fixtures.handDemo;
	if (_seconds < demo.startSeconds)
	{
		return;
	}
	_demoStarted = true;
	// The demo moves the camera, the cursor and the buttons from here, as the tutorial plays it
	ReleaseCamera();
	ReleasePointer();
	const auto played = hand_demo::Play(demo.name, 0, false, false);
	Log(played ? fmt::format("Hand demo {} playing", demo.name)
	           : fmt::format("Problem: the hand demo {} did not start", demo.name));
}

bool Runner::MayRunHere(const Scenario& scenario)
{
	// The copy of the game's data carries its marker
	if (scenario.fixtures.writesGameData)
	{
		const bool onCopy =
		    Locator::filesystem::has_value() && Locator::filesystem::value().Exists(Locator::filesystem::value().GetGamePath() /
		                                                                            testbed_fixtures::k_GameDataCopyMarker);
		if (!onCopy)
		{
			Log(fmt::format("Problem: {} writes into the game's folder, so it runs only on a copy of the game's data (one "
			                "with {} in it)",
			                scenario.id, testbed_fixtures::k_GameDataCopyMarker));
			return false;
		}
	}
	return true;
}

void Runner::PlaceFixtures(const Scenario& scenario)
{
	const auto& fixtures = scenario.fixtures;
	if (testbed_fixtures::Empty(fixtures))
	{
		return;
	}
	for (const auto& problem : testbed_fixtures::Problems(
	         fixtures, fixtures.handDemo.has_value() ? std::optional<size_t>(_presses.size()) : std::nullopt))
	{
		Log(fmt::format("Problem: {}", problem));
	}
	// The creature fixtures are the scenario's creatures too, numbered after its own, so that its commands reach them
	testbed_fixtures::Place(
	    fixtures, _middle, _presses, [this](size_t index) { return ObjectAt(index); },
	    [this](std::string line) { Log(std::move(line)); },
	    [this](entt::entity creature) {
		    _creatures.push_back(creature);
		    _started.push_back(false);
	    });
}

void Runner::SetUpEnvironment(const Environment& environment)
{
	SetHour(environment.hour);
	// The footpaths drawn as the debug menu's box draws them, put back as they were when the scenario stops
	if (environment.drawFootpaths && Locator::config::has_value())
	{
		_drawFootpathsBefore = std::exchange(Locator::config::value().drawFootpaths, true);
	}
	if (environment.detailLevel.has_value() && Locator::config::has_value())
	{
		Locator::config::value().detailLevel = *environment.detailLevel;
		if (Locator::rendereringSystem::has_value())
		{
			Locator::rendereringSystem::value().SetLayoutDirty();
		}
	}
	if (Locator::dayNightClock::has_value())
	{
		Locator::dayNightClock::value().Clock().SetRunning(environment.clockRuns);
	}
	SetIslandWeather(environment.weather);
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		physiology.SetTimeScale(environment.bodyTimeScale);
		physiology.SetFaintingEnabled(environment.fainting);
	}
	if (Locator::footprintSystem::has_value())
	{
		Locator::footprintSystem::value().SetAprilFoolsOverride(environment.aprilFools);
	}
	if (Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().SetAngerStartsFights(environment.angerStartsFights);
	}
	if (Locator::alignmentSystem::has_value())
	{
		Locator::alignmentSystem::value().SetPlayerAlignment(PlayerNames::PLAYER_ONE,
		                                                     environment.playerAlignment.value_or(0.0f));
	}
	// The cursor, and so the hand, put at a place on the screen; held there until the scenario stops, as no mouse moves
	// it in a test run
	if (environment.cursor.has_value() && Locator::inputState::has_value() && Locator::windowing::has_value())
	{
		const auto size = WindowSize();
		_pointer.MoveTo(glm::ivec2(*environment.cursor * glm::vec2(size)), size);
		// The debug windows still take the mouse, so the scenario can be stopped
		_holdPointer = true;
	}
}

void Runner::SetTownBelief(float belief)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(k_ScenarioTownId);
	if (found == towns.end() || !registry.Valid(found->second))
	{
		Log("Problem: the scenario's town's belief is set, but none of its objects makes the town");
		return;
	}
	auto& town = registry.Get<ecs::components::Town>(found->second);
	ecs::town_belief::SetBeliefInPlayer(town, PlayerNames::PLAYER_ONE, belief);
	Log(fmt::format("The scenario's town believes {:.2f} in the player",
	                ecs::town_belief::GetBeliefInPlayer(town.belief, PlayerNames::PLAYER_ONE)));
}

void Runner::PlaceBareTowns(const Scenario& scenario, glm::vec2 middle)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < scenario.bareTowns.size(); ++i)
	{
		const auto& bare = scenario.bareTowns[i];
		const auto at = middle + bare.offset;
		const glm::vec3 position {at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y};
		// The game's own archetype, as a land script's CREATE_TOWN makes one: a town of the player's with no members
		const auto town = ecs::archetypes::TownArchetype::Create(static_cast<int>(k_ScenarioTownId + 1 + i), position,
		                                                         PlayerNames::PLAYER_ONE, Tribe::CELTIC);
		Log(fmt::format("placed bare town {} at ({:.1f}, {:.1f})", i, at.x, at.y));
		if (bare.belief.has_value() && registry.Valid(town))
		{
			auto& data = registry.Get<ecs::components::Town>(town);
			ecs::town_belief::SetBeliefInPlayer(data, PlayerNames::PLAYER_ONE, *bare.belief);
			Log(fmt::format("Bare town {} believes {:.2f} in the player", i,
			                ecs::town_belief::GetBeliefInPlayer(data.belief, PlayerNames::PLAYER_ONE)));
		}
	}
}

void Runner::PlaceBirds(const Scenario& scenario, glm::vec2 middle)
{
	if (!Locator::animalSystem::has_value())
	{
		return;
	}
	// As a land script makes them: the flock numbered, then its birds spread about its home
	auto& animals = Locator::animalSystem::value();
	constexpr float k_Spread = 8.0f;
	for (size_t i = 0; i < scenario.birdFlocks.size(); ++i)
	{
		const auto& setup = scenario.birdFlocks[i];
		const auto home = MapPoint(middle, setup.offset);
		const auto flock = animals.CreateScriptFlock(static_cast<int32_t>(i + 1), home, home, setup.reach, setup.flockDistance);
		for (uint32_t bird = 0; bird < setup.count; ++bird)
		{
			const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(bird) / static_cast<float>(setup.count);
			const glm::vec2 at = home + glm::vec2(std::cos(angle), std::sin(angle)) * k_Spread;
			animals.CreateBird(setup.kind, at, 0, flock);
		}
	}
}

void Runner::PlaceFootpaths(const Scenario& scenario, glm::vec2 middle)
{
	using ecs::components::Footpath;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	// A node at a place of the map, drawn on the land
	const auto nodeAt = [&land](Footpath& footpath, const map_coords::MapCoords& place) {
		const glm::vec2 ground {map_coords::ToMetres(place.x), map_coords::ToMetres(place.z)};
		footpath.nodes.push_back({glm::vec3(ground.x, land.GetHeightAt(ground), ground.y), place, 1, footpath.nextNodeId++});
	};
	for (const auto& setup : scenario.footpaths)
	{
		// a new footpath at the head of the game's list, its nodes in the order given (the first at the list's head)
		const auto entity = ecs::footpaths::Create();
		auto& footpath = registry.Get<Footpath>(entity);
		if (setup.startsAt.has_value() && *setup.startsAt < _objects.size() && registry.Valid(_objects[*setup.startsAt]))
		{
			nodeAt(footpath, ecs::object::MapCoordsOf(_objects[*setup.startsAt]));
		}
		for (const auto offset : setup.nodes)
		{
			nodeAt(footpath, map_coords::FromMetres(MapPoint(middle, offset)));
		}
		// each object it leads from holds it in its link, as a land's links give them
		for (const auto index : setup.leadsFrom)
		{
			if (index < _objects.size() && registry.Valid(_objects[index]))
			{
				ecs::footpaths::AddFootpath(_objects[index], entity);
			}
		}
	}
}

void Runner::RunLandScript(const Scenario& scenario, glm::vec2 middle)
{
	if (scenario.landScript.empty())
	{
		return;
	}
	// every place of the lines is from the middle of the map, as a vortex's lines are from where it opens
	lhscriptx::Script::SetPositionOffset(map_coords::FromMetres(middle));
	try
	{
		lhscriptx::Script script;
		script.Load(std::string(scenario.landScript));
	}
	catch (const std::exception& e)
	{
		Log(fmt::format("Problem: the scenario's land script stopped ({})", e.what()));
	}
	lhscriptx::Script::SetPositionOffset(std::nullopt);
}

void Runner::PlaceObjects(const Scenario& scenario, glm::vec2 middle)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	for (const auto& object : scenario.objects)
	{
		const auto point = MapPoint(middle, object.offset);
		const glm::vec3 position {point.x, land.GetHeightAt(point), point.y};
		const auto yaw = glm::radians(object.yawDegrees);
		_objects.push_back(std::visit(
		    [&]<typename T>(T type) -> entt::entity {
			    if constexpr (std::is_same_v<T, MobileObjectInfo>)
			    {
				    if (type == MobileObjectInfo::Whale)
				    {
					    // A shark, as a script's CREATE of a whale makes it
					    return ecs::archetypes::SharkArchetype::Create(position, yaw, object.scale);
				    }
				    return ecs::archetypes::MobileObjectArchetype::Create(position, type, yaw, object.scale);
			    }
			    else if constexpr (std::is_same_v<T, TreeInfo>)
			    {
				    return ecs::archetypes::TreeArchetype::Create(object.forest, position, type, true, yaw,
				                                                  object.fullSize.value_or(object.scale), object.scale);
			    }
			    else if constexpr (std::is_same_v<T, VillagerInfo>)
			    {
				    constexpr uint32_t k_AdultAge = 30;
				    const auto villager = ecs::archetypes::VillagerArchetype::Create(position, position, type, k_AdultAge);
				    if (object.joinTown)
				    {
					    const auto town = Locator::entitiesRegistry::value().Context().towns.at(ScenarioTown(middle));
					    ecs::town_villagers::AddVillagerToTown(town, villager);
				    }
				    return villager;
			    }
			    else if constexpr (std::is_same_v<T, PotInfo>)
			    {
				    return ecs::archetypes::PotArchetype::Create(position, yaw, type, object.amount);
			    }
			    else if constexpr (std::is_same_v<T, AbodeInfo>)
			    {
				    const auto abode = ecs::archetypes::AbodeArchetype::Create(
				        ScenarioTown(middle), position, type, yaw, object.scale, object.storedFood.value_or(k_ScenarioTownFood),
				        object.storedWood.value_or(k_ScenarioTownWood), object.underConstruction);
				    if (object.underConstruction)
				    {
					    // Its building site in the town, as a plan being built has, and drawn as built so far
					    const auto town = Locator::entitiesRegistry::value().Context().towns.at(ScenarioTown(middle));
					    ecs::building_sites::AddBuildingSite(town, abode);
					    ecs::abodes::SetPercentBuilt(abode, 0.0f);
				    }
				    return abode;
			    }
			    else if constexpr (std::is_same_v<T, FieldTypeInfo>)
			    {
				    const auto field =
				        ecs::archetypes::FieldArchetype::Create(static_cast<int>(ScenarioTown(middle)), position, type, yaw);
				    if (object.ripeCrop && field != entt::null)
				    {
					    auto& crop = Locator::entitiesRegistry::value().Get<ecs::components::Field>(field);
					    crop.crops = ecs::components::Field::k_TimesToSow;
					    crop.growth = ecs::components::Field::k_AgeRecolt;
					    crop.food = ecs::components::Field::k_TotalFood;
				    }
				    return field;
			    }
			    else if constexpr (std::is_same_v<T, AnimalInfo>)
			    {
				    // An animal of no town, with a flock of its own and its age by chance; it faces as it is made
				    return ecs::archetypes::AnimalArchetype::Create(position, type, 0, 0);
			    }
			    else if constexpr (std::is_same_v<T, MobileStaticInfo>)
			    {
				    return ecs::archetypes::MobileStaticArchetype::Create(position, type, 0.0f, 0.0f, yaw, 0.0f, object.scale);
			    }
			    else if constexpr (std::is_same_v<T, AnimatedStaticInfo>)
			    {
				    return ecs::archetypes::AnimatedStaticArchetype::Create(position, type, yaw, object.scale);
			    }
			    else if constexpr (std::is_same_v<T, FishFarmInfo>)
			    {
				    return ecs::archetypes::FishFarmArchetype::Create(position, static_cast<uint32_t>(type));
			    }
			    else
			    {
				    return ecs::archetypes::FeatureArchetype::Create(position, type, yaw, object.scale);
			    }
		    },
		    object.type));
		SetLifeAndPoison(_objects.back(), object);
		MakeHungry(_objects.back(), object);
		TakeIntoScript(_objects.back(), object);
		if (object.immovable)
		{
			ecs::object_flags::SetMoveable(_objects.back(), false);
		}
	}
	// the villagers that live in an abode of the scenario's move in, once both are out
	for (size_t i = 0; i < scenario.objects.size() && i < _objects.size(); ++i)
	{
		const auto& home = scenario.objects[i].livesIn;
		if (home.has_value() && *home < _objects.size())
		{
			ecs::abode_villagers::AddVillagerToAbode(_objects[*home], _objects[i]);
		}
	}
}

void Runner::PlaceCreatures(const Scenario& scenario, glm::vec2 middle)
{
	const auto& land = Locator::terrainSystem::value();
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& setup : scenario.creatures)
	{
		const auto point = MapPoint(middle, setup.offset);
		const glm::vec3 position {point.x, land.GetHeightAt(point), point.y};
		auto body = CreatureArchetype::StartBody(setup.species);
		body.alignment = setup.alignment.value_or(body.alignment);
		body.fatness = setup.fatness.value_or(body.fatness);
		body.strength = setup.strength.value_or(body.strength);
		const auto size = setup.size.value_or(k_DefaultSize);
		const auto entity =
		    CreatureArchetype::Create(position, setup.owner, setup.species, 0, glm::radians(setup.facingDegrees), size, body);
		if (auto* mind = registry.TryGet<CreatureMindState>(entity))
		{
			mind->paused = setup.pauseMind;
			if (setup.phase.has_value())
			{
				mind->developmentPhase = *setup.phase;
			}
		}
		if (!setup.mindFile.empty())
		{
			LoadMindFile(entity, setup.mindFile);
		}
		if (Locator::leashSystem::has_value())
		{
			// Made for trying things out, it knows every leash, as creatures made by the original's debug tools do
			auto& leashes = Locator::leashSystem::value();
			for (const auto type : creature_leash::k_Types)
			{
				leashes.SetKnown(entity, type, true);
			}
		}
		if (Locator::creatureSkinSystem::has_value())
		{
			auto& skins = Locator::creatureSkinSystem::value();
			for (size_t slot = 0; slot < setup.tattoos.size(); ++slot)
			{
				skins.SetTattoo(entity, slot, setup.tattoos[slot]);
			}
			for (const auto& wound : setup.wounds)
			{
				skins.AddWound(entity, wound);
			}
			for (const auto& drop : setup.blood)
			{
				skins.AddBlood(entity, drop);
			}
		}
		_creatures.push_back(entity);
		_started.push_back(false);
	}
	LeadFirstCreatures(scenario);
}

void Runner::LeadFirstCreatures(const Scenario& scenario)
{
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& leashes = Locator::leashSystem::value();
	const auto count = std::min(scenario.creatures.size(), _creatures.size());
	// The game makes each new creature its owner's, the newest winning (LeashSystemInterface::ClaimOnArrival). The
	// testbed's scenarios are written for one rule of their own instead, applied once, after all of them are made: an
	// owner's first creature is the one they lead, and the others aren't. One marked not leashable is passed over
	if (scenario.firstCreatureLeads)
	{
		std::vector<PlayerNames> owners;
		for (size_t i = 0; i < count; ++i)
		{
			const auto& setup = scenario.creatures[i];
			if (setup.leashable == std::optional(false) || std::ranges::find(owners, setup.owner) != owners.end())
			{
				continue;
			}
			owners.push_back(setup.owner);
			if (creature_leash::CanLead(setup.owner))
			{
				leashes.SetLeashable(_creatures[i], true);
			}
		}
	}
	// What a creature is given has the last word, in the order they were made
	for (size_t i = 0; i < count; ++i)
	{
		if (const auto& leashable = scenario.creatures[i].leashable; leashable.has_value())
		{
			leashes.SetLeashable(_creatures[i], *leashable);
		}
	}
}

void Runner::LeadFirstCrowdCreatures()
{
	if (_scenario == nullptr || !_scenario->firstCreatureLeads || _crowdCreatures.empty() || !Locator::leashSystem::has_value())
	{
		return;
	}
	std::vector<PlayerNames> owners;
	for (const auto& setup : _scenario->creatures)
	{
		owners.push_back(setup.owner);
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	auto& leashes = Locator::leashSystem::value();
	for (const auto entity : _crowdEntities)
	{
		const auto* body = registry.Valid(entity) ? registry.TryGet<const Creature>(entity) : nullptr;
		if (body == nullptr || std::ranges::find(owners, body->owner) != owners.end())
		{
			continue;
		}
		owners.push_back(body->owner);
		if (creature_leash::CanLead(body->owner))
		{
			leashes.SetLeashable(entity, true);
		}
	}
}

void Runner::PlaceDispensers(const Scenario& scenario)
{
	// The testbed lays out no dispenser grid of its own, so there is none to take away for a scenario that wants none
	if (!Locator::magicSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& magic = Locator::magicSystem::value();
	for (const auto& dispenser : scenario.dispensers)
	{
		const auto point = MapPoint(_middle, dispenser.offset);
		const float ground = Locator::terrainSystem::value().GetHeightAt(point);
		if (dispenser.bubbleHeight.has_value())
		{
			magic.CreateOneOffSeedFor({point.x, ground + *dispenser.bubbleHeight, point.y}, dispenser.type);
		}
		else
		{
			const auto made = magic.CreateDispenser({point.x, ground, point.y}, dispenser.type, 0.0f);
			if (dispenser.charged && made != entt::null)
			{
				magic.ChargeDispenser(made);
			}
		}
	}
}

std::optional<entt::entity> Runner::CreatureAt(size_t index) const
{
	if (index >= _creatures.size() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto entity = _creatures[index];
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Creature>(entity))
	{
		return std::nullopt;
	}
	return entity;
}

void Runner::ApplyStates()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (size_t i = 0; i < _creatures.size(); ++i)
	{
		const auto entity = CreatureAt(i);
		if (!entity.has_value())
		{
			continue;
		}
		// a creature fixture has no setup of its own to hold
		if (i >= _scenario->creatures.size())
		{
			continue;
		}
		const auto& setup = _scenario->creatures[i];
		auto* needs = registry.TryGet<CreatureNeeds>(*entity);
		auto* mind = registry.TryGet<CreatureMindState>(*entity);
		const auto* creature = registry.TryGet<const Creature>(*entity);
		// The body and the mind start from the species' tables on their first turn, over anything set before
		if (needs == nullptr || mind == nullptr || creature == nullptr || !needs->started || !mind->desires.has_value())
		{
			continue;
		}
		if (_started[i] && !setup.hold)
		{
			continue;
		}
		auto overrides = setup.needs;
		if (_started[i])
		{
			// Its age is only set as it starts, so it still grows older
			overrides.age.reset();
		}
		Apply(overrides, needs->needs, creature->size);
		Apply(setup.desires, *mind->desires);
		if (!_started[i] && !setup.desires.empty())
		{
			// Its first thought was had with the species' desires; it thinks again with the scenario's
			mind->idle = {};
		}
		_started[i] = true;
	}
}

std::optional<entt::entity> Runner::ObjectAt(size_t index) const
{
	if (index >= _objects.size() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::entitiesRegistry::value().Valid(_objects[index]))
	{
		return std::nullopt;
	}
	return _objects[index];
}

std::string Runner::GiveObjectCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureObjectActionSystem::has_value())
	{
		return "no hands";
	}
	auto& hands = Locator::creatureObjectActionSystem::value();
	const auto& land = Locator::terrainSystem::value();
	const auto point = MapPoint(_middle, command.point);
	const glm::vec3 onLand {point.x, land.GetHeightAt(point), point.y};
	const auto object = ThingOf(command);
	switch (command.kind)
	{
	case Kind::PickUp:
		return object.has_value() ? std::string(Started(hands.PickUp(creature, *object))) : "it is gone";
	case Kind::PutDown:
		return std::string(Started(hands.PutDown(creature)));
	case Kind::Discard:
		return std::string(Started(hands.Discard(creature)));
	case Kind::Lob:
		return std::string(Started(hands.Lob(creature)));
	case Kind::EatHeld:
		return std::string(Started(hands.EatHeld(creature)));
	case Kind::Examine:
		return std::string(Started(hands.Keep(creature, creature_object_actions::k_FirstKeepAnimation + command.value)));
	case Kind::ThrowAt:
		// At about the height of a creature's middle
		return std::string(Started(hands.Throw(creature, onLand + glm::vec3(0.0f, CreatureHeight(1.0f) * 0.5f, 0.0f))));
	case Kind::KnockDown:
		return object.has_value() ? std::string(Started(hands.Destroy(creature, *object))) : "it is gone";
	case Kind::Kick:
		return object.has_value() ? std::string(Started(hands.Kick(creature, *object))) : "it is gone";
	case Kind::PointAt:
		return std::string(Started(hands.PointAt(creature, onLand)));
	default:
		return {};
	}
}

std::string Runner::GiveGuideOgreNative(entt::entity creature, const Command& command, glm::vec2 point)
{
	return GiveGuideOgreNativeTo(creature, command, point, _scriptMade);
}

std::string Runner::GiveHandCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureHandSystem::has_value())
	{
		return "no hand";
	}
	auto& hand = Locator::creatureHandSystem::value();
	// While the hand holds one creature, a command on another is refused
	if (const auto held = hand.GetCreature(); command.kind != Kind::HandLetGo && held.has_value() && *held != creature)
	{
		return "refused: the hand holds another creature";
	}
	switch (command.kind)
	{
	case Kind::HandGrab:
		_handGrabbed = hand.Grab(creature) || _handGrabbed;
		return hand.GetCreature() == creature ? "held" : "can't";
	case Kind::HandStroke:
		return hand.Stroke(creature, static_cast<creature_feedback::BodyPart>(command.bodyPart)) ? "stroked" : "busy";
	case Kind::HandSlap:
		return hand.Slap(creature, command.slapHeight, command.gentle, command.sweepsRight) ? "slapped" : "busy";
	case Kind::HandLetGo:
		if (hand.GetCreature() == creature)
		{
			const auto sum = hand.GetFeedbackSum();
			LetGoOfCreature(hand);
			return fmt::format("{:+.2f}", sum);
		}
		return "not held";
	default:
		return {};
	}
}

std::string Runner::GiveLeashCommand(entt::entity creature, const Command& command)
{
	if (!Locator::leashSystem::has_value())
	{
		return "no leashes";
	}
	auto& leashes = Locator::leashSystem::value();
	switch (command.kind)
	{
	case Kind::LeashState:
		return leashes.IsLeashed(creature) ? fmt::format("leashed ({})", static_cast<int>(leashes.TypeOf(creature)))
		                                   : "not leashed";
	case Kind::PutOnLeash:
		// It must know the learning leash before any, and the leash itself
		leashes.SetKnown(creature, LeashType::Rope, true);
		leashes.SetKnown(creature, command.leash, true);
		if (leashes.IsLeashed(creature))
		{
			return leashes.ChangeType(creature, command.leash) ? "changed" : "can't";
		}
		return leashes.PutOn(creature, command.leash) ? "on" : RefusedText(leashes, command.player);
	case Kind::MakeLeashable:
		return leashes.SetLeashable(creature, true) ? "leashable" : RefusedText(leashes, command.player);
	case Kind::HandTapLeash:
		// As the player's Action button tapping it does
		return leashes.TapCreature(command.player, creature) ? "on" : RefusedText(leashes, command.player);
	case Kind::LeashShake:
	{
		// The game shakes a leash off with a scribble drawn with the empty hand
		auto scribble = command;
		scribble.value = magic::gestures::k_Scribble;
		return DrawGesture(scribble);
	}
	case Kind::LeashKey:
	{
		// As the player pressing the key does, through the same call the controls make
		constexpr std::array k_Keys {creature_leash::LeashKey::Leash, creature_leash::LeashKey::PreviousLeash,
		                             creature_leash::LeashKey::NextLeash};
		const auto key = k_Keys.at(command.value);
		const auto before = leashes.TypeOf(creature);
		if (!leashes.PressKey(command.player, key))
		{
			return RefusedText(leashes, command.player);
		}
		// The keys that step the leash send it, and it changes at the next turn
		if (key != creature_leash::LeashKey::Leash)
		{
			return "sent";
		}
		const auto after = leashes.TypeOf(creature);
		return after == LeashType::None ? "off" : before == after ? "same" : creature_leash::Name(after);
	}
	case Kind::TieLeash:
		if (const auto object = ThingOf(command))
		{
			return leashes.TieTo(creature, *object) ? "tied" : "can't";
		}
		return "it is gone";
	case Kind::UntieLeash:
		leashes.UntieToHand(creature);
		return {};
	case Kind::TakeOffLeash:
		leashes.TakeOff(creature);
		return {};
	case Kind::ConfineToHome:
		leashes.ConfineToHome(creature, command.radius);
		return {};
	default:
		return {};
	}
}

std::string Runner::GivePlayerCommand(const Command& command)
{
	switch (command.kind)
	{
	case Kind::PressKey:
		if (!Locator::gameActionSystem::has_value())
		{
			return "no keys";
		}
		Locator::gameActionSystem::value().QueuePress(static_cast<input::BindableActionMap>(command.value));
		return {};
	case Kind::HoldSeed:
	{
		// As a bubble's seed comes into the hand, the way the Magic window's Running tab puts one there: at its base level
		if (!Locator::magicSystem::has_value())
		{
			return "no miracles";
		}
		const auto seed = Locator::magicSystem::value().GiveSeedToHand(
		    command.player, static_cast<SpellSeedType>(command.value), debug::miracles::k_BasePowerUpLevel, 1.0f);
		return seed == entt::null ? "the hand can't take it" : fmt::format("seed {} in the hand", static_cast<uint32_t>(seed));
	}
	case Kind::SummonSeed:
	{
		// As the miracle selection asks the player's worship for it: the best icon of the seed charges it from the
		// prayer power, and the seed comes into the hand once it is full
		auto* icons = Locator::magicSystem::has_value() ? magic::gestures::GetIconProvider() : nullptr;
		const auto seed = static_cast<int>(command.value);
		if (icons == nullptr)
		{
			return "the player has no worship";
		}
		if (!icons->IconValidForRequest(seed))
		{
			return "the player's worship can't give it";
		}
		icons->RequestSpell(seed);
		return "asked for";
	}
	case Kind::DrawGesture:
		return DrawGesture(command);
	case Kind::HandTakeFireBall:
		return StartFireBallTake(command);
	case Kind::HandTake:
	{
		const auto object = ThingOf(command);
		if (!object.has_value() || !Locator::handGrabSystem::has_value())
		{
			return "nothing to take";
		}
		return Locator::handGrabSystem::value().TakeIntoHand(*object) ? "taken into the hand" : "the hand can't take it";
	}
	default:
		return {};
	}
}

std::string Runner::StartFireBallTake(const Command& command)
{
	if (!Locator::inputState::has_value() || !Locator::windowing::has_value() || !Locator::camera::has_value() ||
	    !Locator::handSystem::has_value() || !Locator::magicSystem::has_value())
	{
		return "no hand";
	}
	if (_fireBallTake.has_value())
	{
		return "the hand is still taking one";
	}
	FireBallTake take;
	take.holdPastGrab = command.value == 1;
	// a seed in the hand takes the ball in by resting over it, with no button
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (const auto held = Locator::handSystem::value().GetHeldObject(); held.has_value() && registry.Valid(*held))
	{
		if (const auto* seed = registry.TryGet<const ecs::components::SpellSeed>(*held); seed != nullptr)
		{
			take.feed = true;
			take.seed = *held;
			take.seedPower = seed->power;
		}
	}
	if (!_pointer.IsTaken())
	{
		_pointer.Take();
	}
	KeepDebugWindowsOffTheMouse(true);
	_fireBallTake = take;
	return take.feed           ? fmt::format("the seed rests over the ball, its power {:.3f}", take.seedPower)
	       : take.holdPastGrab ? std::string("the empty hand holds the ball past the grab's wait")
	                           : std::string("the empty hand clicks on the ball");
}

void Runner::UpdateFireBallTake(float seconds)
{
	if (!_fireBallTake.has_value() || !Locator::camera::has_value() || !Locator::handSystem::has_value())
	{
		return;
	}
	auto& take = *_fireBallTake;
	take.seconds += seconds;
	++take.frames;
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	// the ball: the newest the hand can see (the list keeps the newest first), kept once found
	if (take.ball == entt::null)
	{
		for (const auto ball : magic::fireball::All())
		{
			if (const auto* component = registry.TryGet<const ecs::components::MagicFireBall>(ball);
			    component != nullptr && component->handTarget.has_value())
			{
				take.ball = ball;
				take.frames = 0;
				break;
			}
		}
	}
	const bool gone = take.ball != entt::null && !registry.Valid(take.ball);
	if (gone || take.seconds > k_FireBallTakeSeconds)
	{
		if (take.pressedAt.has_value() && !take.released)
		{
			_pointer.Press(testbed_pointer::ButtonOf(3), false);
		}
		const auto held = Locator::handSystem::value().GetHeldObject();
		const auto* seed =
		    held.has_value() && registry.Valid(*held) ? registry.TryGet<const ecs::components::SpellSeed>(*held) : nullptr;
		std::string line;
		if (!gone)
		{
			line = take.ball == entt::null ? "no fireball the hand can see" : "it slipped through the hand";
		}
		else if (take.feed)
		{
			line = seed != nullptr && *held == take.seed
			           ? fmt::format("the seed took it in, power {:.3f} (was {:.3f})", seed->power, take.seedPower)
			           : std::string("the ball went, and the seed is no longer in the hand");
		}
		else
		{
			line = seed != nullptr && seed->seedType == SpellSeedType::Fire
			           ? fmt::format("caught it: a fire seed in the hand, {}, {:.0f} chants",
			                         seed->ready ? "ready" : "not ready", seed->chantStore)
			           : std::string("the ball went, and nothing is in the hand");
		}
		line = fmt::format("{:.2f}s: HandTakeFireBall: {}", _seconds, line);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
		_fireBallTake.reset();
		return;
	}
	if (take.ball == entt::null)
	{
		return;
	}
	// the pointer on the ball where the hand sees it, else on the ball itself
	const auto& ball = registry.Get<const ecs::components::MagicFireBall>(take.ball);
	const auto point = ball.handTarget.value_or(registry.Get<const ecs::components::Transform>(take.ball).position);
	const auto size = WindowSize();
	glm::vec3 screen {0.0f};
	if (Locator::camera::value().ProjectWorldToScreen(point, {0.0f, 0.0f, size.x, size.y}, screen))
	{
		_pointer.MoveTo(glm::ivec2(glm::round(glm::vec2(screen))), size);
	}
	if (take.feed)
	{
		return;
	}
	// the empty hand: the button down on the ball, and up again inside the grab's wait or past it
	if (!take.pressedAt.has_value() && take.frames >= k_FireBallPressFrame)
	{
		_pointer.Press(testbed_pointer::ButtonOf(3), true);
		take.pressedAt = take.seconds;
		_handWatchSeconds = k_HandWatchSeconds;
	}
	else if (take.pressedAt.has_value() && !take.released &&
	         take.seconds - *take.pressedAt >= (take.holdPastGrab ? k_FireBallHoldSeconds : k_FireBallTapSeconds))
	{
		_pointer.Press(testbed_pointer::ButtonOf(3), false);
		take.released = true;
		_handWatchSeconds = k_HandWatchSeconds;
	}
}

std::string Runner::DrawGesture(const Command& command)
{
	namespace gestures = magic::gestures;
	if (!Locator::inputState::has_value() || !Locator::windowing::has_value() || !Locator::magicSystem::has_value())
	{
		return "no mouse";
	}
	if (_drawing.has_value())
	{
		return "the hand is still drawing";
	}
	const auto gesture = static_cast<gestures::Gesture>(command.value);
	const auto name = debug::gestures_window::GestureName(gesture);
	const auto size = WindowSize();
	auto drawing = testbed_gesture::StartDrawing(testbed_gesture::StrokeOf(gestures::Templates(), gesture, size), gesture);
	if (!drawing.has_value())
	{
		return fmt::format("no template of {}", name);
	}
	if (!_pointer.IsTaken())
	{
		_pointer.Take();
	}
	KeepDebugWindowsOffTheMouse(true);
	// The stroke reaches the hand through the fixed cursor alone: no mouse motion goes out, so no drag of the camera can
	// wipe the gesture as it is drawn
	_pointer.MoveTo(drawing->pixels.front(), size);
	if (drawing->holdAction)
	{
		// The hand is given the button at the end of this frame's update, and the stroke starts the next frame
		_pointer.Press(testbed_pointer::ButtonOf(3), true);
	}
	_gestureCooldown = gestures::State().cooldown;
	auto line = fmt::format("{}, {} mouse messages{}", name, drawing->pixels.size(),
	                        drawing->holdAction ? " with the Action button held" : "");
	_drawing = std::move(drawing);
	return line;
}

void Runner::UpdateDrawing(float seconds)
{
	namespace gestures = magic::gestures;
	// The frame after a drawing ends is watched too: the gesture may be taken as the button is let go
	if ((!_drawing.has_value() && !_watchGestureOnce) || !Locator::magicSystem::has_value())
	{
		return;
	}
	const auto& state = gestures::State();
	if (testbed_gesture::TookAGesture(_gestureCooldown, state.cooldown))
	{
		// A sizing circle is kept as the circle; the last match may have been tried against other gestures since
		const auto taken = state.circlePending ? state.circleGesture : state.result.gesture;
		auto line = fmt::format("{:.2f}s: recognised {} (template {}{})", _seconds, debug::gestures_window::GestureName(taken),
		                        state.result.templateIndex, state.result.reversed ? ", mirrored" : "");
		if (state.circlePending)
		{
			line += fmt::format(": a circle at ({:.1f}, {:.1f}, {:.1f}), size {:.1f}", state.circlePosition.x,
			                    state.circlePosition.y, state.circlePosition.z, state.circleSize);
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
	}
	_gestureCooldown = state.cooldown;
	if (!_drawing.has_value())
	{
		_watchGestureOnce = false;
		return;
	}
	const auto step = testbed_gesture::Step(*_drawing, seconds, gestures::sampling::PlayingStroke());
	if (step.startStroke)
	{
		gestures::sampling::PlayStroke(_drawing->pixels);
	}
	if (step.pointer.has_value())
	{
		_pointer.MoveTo(*step.pointer, WindowSize());
	}
	if (step.releaseAction)
	{
		_pointer.Press(testbed_pointer::ButtonOf(3), false);
	}
	if (step.done)
	{
		const auto line = fmt::format("{:.2f}s: drawn {}", _seconds, debug::gestures_window::GestureName(_drawing->gesture));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
		_drawing.reset();
		_watchGestureOnce = true;
	}
}

void Runner::StopDrawing()
{
	if (!_drawing.has_value())
	{
		return;
	}
	_drawing.reset();
	_watchGestureOnce = false;
	// An empty stroke ends the one playing
	if (Locator::magicSystem::has_value() && magic::gestures::sampling::PlayingStroke())
	{
		magic::gestures::sampling::PlayStroke({});
	}
}

std::string Runner::GiveVillagerCommand(const Command& command)
{
	const auto object = ObjectAt(command.object);
	if (!object.has_value())
	{
		return "it is gone";
	}
	if (command.kind == Kind::VillagerWalkTo)
	{
		// The walk the villagers' own errands set up, which goes round what stands in its way; once there it stands, as
		// a script's villager does
		const auto goal = MapPoint(_middle, command.point);
		return std::string(Started(ecs::villager::SetupMoveToWithHug(*object, goal, VillagerStates::InScript) == 1));
	}
	const auto store = ObjectAt(command.value);
	if (!store.has_value())
	{
		return "the store is gone";
	}
	// As when the player's throw lands it in the store: the store takes its resource and it is deleted at once
	return ecs::resource_stores::DeleteObjectAndTakeResource(
	           *store, *object, ecs::held_apply::ThrowerInterface(openblack::creature::LocalPlayer()))
	           ? "taken"
	           : "not taken";
}

std::string Runner::GiveFightCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureFightSystem::has_value())
	{
		return "no fights";
	}
	auto& fights = Locator::creatureFightSystem::value();
	switch (command.kind)
	{
	case Kind::StartFight:
		if (const auto opponent = CreatureAt(command.value))
		{
			constexpr std::array<std::string_view, 5> k_Results {"started", "no opponent", "busy", "too weak", "refused"};
			return std::string(k_Results.at(static_cast<size_t>(fights.StartFight(creature, *opponent))));
		}
		return "it is gone";
	case Kind::FightBlow:
	{
		constexpr std::array k_Bands {creature_fight::Band::High, creature_fight::Band::Mid, creature_fight::Band::Low};
		// As a click held for the charge, then let go
		const bool queued = fights.QueueMove(creature, creature_fight::AttackMove(k_Bands.at(command.value)), true);
		fights.ReleaseCharge(creature, command.chargeMs);
		return queued ? fmt::format("{:.0f} ms", command.chargeMs) : "not fighting";
	}
	case Kind::FightBlock:
		return fights.QueueMove(creature, creature_fight::BlockMove(), true) ? "" : "not fighting";
	case Kind::FightStep:
		return fights.QueueMove(creature, creature_fight::StepMove(static_cast<creature_fight::Step>(command.value)), true)
		           ? ""
		           : "not fighting";
	case Kind::FightSpecial:
		return fights.QueueMove(creature, {.kind = creature_fight::Move::Kind::Special}, true) ? "" : "not fighting";
	case Kind::FightAuto:
		fights.SetAutoFighting(creature, command.value != 0);
		return fights.IsFighting(creature) ? "" : "not fighting";
	case Kind::FightCast:
		return fights.QueueMove(
		           creature, {.kind = creature_fight::Move::Kind::Spell, .value = static_cast<uint32_t>(command.value)}, true)
		           ? ""
		           : "not fighting";
	case Kind::KnockOut:
		fights.KnockOut(creature);
		return {};
	case Kind::BringRound:
		fights.Resurrect(creature);
		return {};
	case Kind::TieLeashToCreature:
		if (const auto other = CreatureAt(command.value); other.has_value() && Locator::leashSystem::has_value())
		{
			return Locator::leashSystem::value().TieTo(creature, *other) ? "tied" : "can't";
		}
		return "it is gone";
	default:
		return {};
	}
}

bool Runner::IsFree(size_t creature) const
{
	const auto entity = CreatureAt(creature);
	if (!entity.has_value())
	{
		// A creature that is gone holds nothing up
		return true;
	}
	if (Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(*entity))
	{
		return false;
	}
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const CreatureAnimation>(*entity);
	return animation == nullptr || !creature_layers::IsPlaying(animation->body);
}

bool Runner::IsWalkedRound(size_t object) const
{
	const auto entity = ObjectAt(object);
	if (!entity.has_value())
	{
		// A thing that is gone holds nothing up
		return true;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return std::ranges::any_of(_objects, [&registry, &entity](entt::entity walker) {
		const auto* hugged = registry.Valid(walker) && registry.AllOf<ecs::components::MoveStateOrbitTag>(walker)
		                         ? registry.TryGet<const ecs::components::WallHugObjectReference>(walker)
		                         : nullptr;
		return hugged != nullptr && hugged->entity == *entity;
	});
}

void Runner::ChangeLand(std::string_view landScript)
{
	Log(fmt::format("{:.1f}s: {} {}", _seconds, Name(Kind::ChangeLand), landScript));
	// The testbed goes with the land, so the scenario ends here, as when another land is loaded over it; a land
	// timeline carries on on the new land
	if (!_onLand)
	{
		_running = false;
		_shot.reset();
		RestoreSettings();
	}
	const bool loaded = _host.ChangeLand(landScript);
	Log(loaded ? fmt::format("Changed to the land of {}", landScript)
	           : fmt::format("Problem: the land of {} could not be loaded", landScript));
	// The speed the new land runs at: a land change keeps it
	Log(fmt::format("The game speed is {:.2f}", 1.0f / _host.GetGameSpeed()));
}

void Runner::Give(const Command& command)
{
	if (IsPlayerCommand(command.kind))
	{
		const auto result = GivePlayerCommand(command);
		Log(fmt::format("{:.1f}s: {}{}{}", _seconds, Name(command.kind), result.empty() ? "" : ": ", result));
		return;
	}
	if (IsPointerCommand(command.kind))
	{
		const auto result = GivePointerCommand(command);
		const auto line = fmt::format("{:.2f}s: {}: {}", _seconds, Name(command.kind), result);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
		Log(line);
		return;
	}
	if (command.kind == Kind::WaitGlobal || command.kind == Kind::WaitTask || command.kind == Kind::WaitText ||
	    command.kind == Kind::WaitCamera || command.kind == Kind::SetGlobal || command.kind == Kind::StartScript ||
	    command.kind == Kind::LogGlobals || command.kind == Kind::ClickTexts)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveScriptsCommand(command)));
		return;
	}
	if (command.kind == Kind::CameraTo || command.kind == Kind::CameraFollow || command.kind == Kind::MoveThing ||
	    command.kind == Kind::HandOver || command.kind == Kind::HandDrop)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveLandCommand(command)));
		return;
	}
	if (command.kind == Kind::SetHour)
	{
		SetHour(command.hour);
		Log(fmt::format("{:.1f}s: the hour is {:.1f}", _seconds, command.hour));
		return;
	}
	if (command.kind == Kind::Advisor)
	{
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), GiveAdvisorCommand(command)));
		return;
	}
	if (command.kind == Kind::SetOpenClose || command.kind == Kind::LayGateStone)
	{
		GiveSceneryCommand(command);
		return;
	}
	if (command.kind == Kind::CreateVortex || command.kind == Kind::VortexFadeIn || command.kind == Kind::VortexFadeOut ||
	    command.kind == Kind::VortexCrossingFile)
	{
		GiveVortexCommand(command);
		return;
	}
	if (command.kind == Kind::EndGuidanceWaits)
	{
		audio::guidance::EndWaits();
		Log(fmt::format("{:.1f}s: the guidance's waits are over", _seconds));
		return;
	}
	if (command.kind == Kind::OpenTempleRooms)
	{
		// The same opening as the temple debug window's button
		const bool temple = Locator::temple::has_value();
		if (temple)
		{
			Locator::temple::value().OpenRooms();
		}
		Log(fmt::format("{:.1f}s: {}", _seconds, temple ? "the temple's rooms are open" : "no temple, no rooms opened"));
		return;
	}
	if (command.kind == Kind::SetAlignment)
	{
		if (Locator::alignmentSystem::has_value())
		{
			Locator::alignmentSystem::value().SetPlayerAlignment(command.player, command.alignment);
		}
		Log(fmt::format("{:.1f}s: the alignment is {:.2f}", _seconds, command.alignment));
		return;
	}
	if (command.kind == Kind::SpiritOut || command.kind == Kind::SpiritHome)
	{
		const bool out = command.kind == Kind::SpiritOut;
		const bool loaded = Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded();
		if (loaded)
		{
			const auto dude = static_cast<int>(command.value);
			if (out)
			{
				Locator::advisorSystem::value().GetController().Eject(dude);
			}
			else
			{
				Locator::advisorSystem::value().GetController().Home(dude);
			}
		}
		Log(fmt::format("{:.1f}s: the {} spirit {}", _seconds,
		                command.value == static_cast<size_t>(help::spirits::k_GoodDude) ? "good" : "evil",
		                !loaded ? "is not there" : (out ? "flies out" : "flies home")));
		return;
	}
	if (command.kind == Kind::SpiritSay)
	{
		// As a script's text in the spirit's voice: its line of the advisors' voices goes through the texts' own voice path,
		// which stops both spirits' lines and has this one say it
		const bool good = command.value == static_cast<size_t>(help::spirits::k_GoodDude);
		const bool voices = Locator::helpTextSystem::has_value();
		if (voices)
		{
			static_cast<void>(
			    audio::voices::RunTextVoice(good ? helptext::k_NarratorGoodSpirit : helptext::k_NarratorEvilSpirit,
			                                {.bank = audio::SfxBank::HelpSprites, .sample = command.spiritLine}));
		}
		Log(fmt::format("{:.1f}s: the {} spirit {} line {}", _seconds, good ? "good" : "evil",
		                voices ? "says" : "has no voice to say", command.spiritLine));
		return;
	}
	if (command.kind == Kind::CreateWorshipSite)
	{
		// As the map script's command, on the player's temple: the site is built once the town has a building site of it
		const auto citadel = worship::citadel::Of(command.player);
		std::string_view result = "the player has no temple";
		if (citadel != entt::null)
		{
			const auto tribe = static_cast<Tribe>(command.value);
			const auto site = worship::citadel::CreateBuiltWorshipSite(citadel, tribe);
			// with no town of the player's of that tribe the site is still built, but no town joins it
			if (site == entt::null)
			{
				result = worship::citadel::FindTribeWorshipSite(citadel, tribe) == entt::null
				             ? "the temple has no site of that tribe"
				             : "built, but the player has no town of that tribe";
			}
			else
			{
				result = ecs::abodes::IsBuilt(site) ? "built" : "not built, the town has no building site of it";
			}
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), result));
		return;
	}
	if (command.kind == Kind::WideScreen)
	{
		// As a script's wide screen: refused while another task holds the bars
		auto* helpSystem = help::Get();
		std::string_view result = "there is no help system";
		if (helpSystem != nullptr)
		{
			const auto owner = helpSystem->GetWideScreenOwner();
			if (owner != 0 && owner != k_ScenarioWideScreenTask)
			{
				result = "a script holds them";
			}
			else
			{
				helpSystem->SetWideScreen(command.value != 0 ? 1 : 0, k_ScenarioWideScreenTask);
				result = command.value != 0 ? "the cinema bars come on" : "the cinema bars go";
			}
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), result));
		return;
	}
	if (command.kind == Kind::KeyDown || command.kind == Kind::KeyUp)
	{
		const bool down = command.kind == Kind::KeyDown;
		const auto key = static_cast<uint32_t>(command.value);
		const bool changed = down ? _keys.Down(key) : _keys.Up(key);
		if (changed)
		{
			GiveKey(key, down);
		}
		// with a text shown, whether it still waits for a click: a click, or the dialogue's key, ends the wait
		std::string wait;
		if (_textShown && Locator::helpTextSystem::has_value())
		{
			const auto* dialogue = std::as_const(Locator::helpTextSystem::value()).GetDialogue();
			wait = fmt::format("; the text waits for a click: {}",
			                   dialogue != nullptr && dialogue->IsWaitingForClick() ? "yes" : "no");
		}
		Log(fmt::format("{:.1f}s: key {} {}{}", _seconds, key, !changed ? "unchanged" : (down ? "down" : "up"), wait));
		return;
	}
	if (command.kind == Kind::ScriptChallengeSnapshot)
	{
		Log(fmt::format("{:.1f}s: challenge snapshot {}: {}", _seconds, command.value, ScriptChallengeSnapshot(command)));
		return;
	}
	if (command.kind == Kind::ScriptDeleteTemple)
	{
		Log(fmt::format("{:.1f}s: temple of player {} deleted with its explosion: {}", _seconds,
		                static_cast<int>(command.player), ScriptDeleteTemple(command.player)));
		return;
	}
	if (command.kind == Kind::ScriptSnapshot)
	{
		// With a point other than the map's middle, the camera is put 50 above it and 70 along z from it, looking at
		// it, and the snapshot is taken from there
		std::optional<std::pair<glm::vec3, glm::vec3>> view;
		if (command.point != glm::vec2(0.0f) && Locator::terrainSystem::has_value())
		{
			const auto at = MapPoint(_middle, command.point);
			const auto focus = glm::vec3(at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y);
			const auto position = focus + glm::vec3(0.0f, 50.0f, 70.0f);
			Locator::camera::value().SetOrigin(position).SetFocus(focus);
			view = std::make_pair(position, focus);
		}
		const auto failed = ScriptSnapshot(static_cast<uint32_t>(command.value), command.subtype, command.amount,
		                                   command.alignment, command.quest, view);
		Log(fmt::format("{:.1f}s: snapshot of challenge {}{}", _seconds, command.value, failed.empty() ? "" : ": " + failed));
		return;
	}
	if (command.kind == Kind::LookAtTempleScroll)
	{
		const bool inside = Locator::temple::has_value() && Locator::temple::value().Active();
		if (inside)
		{
			Locator::temple::value().LookAtScroll(command.value);
		}
		Log(fmt::format("{:.1f}s: {} {}", _seconds,
		                inside ? "the temple's camera looks at scroll" : "not in the temple, no scroll", command.value));
		return;
	}
	if (command.kind == Kind::ReadTip)
	{
		// The tip goes on the read lists as a tap on its sign puts it there, without the sign and its bubble
		const auto text = static_cast<uint32_t>(command.value);
		const auto category = static_cast<uint32_t>(std::lround(command.amount));
		const bool read = Locator::scriptState::has_value();
		if (read)
		{
			ecs::script_highlight::did_you_know_read::MarkRead(text, static_cast<DykCategory>(category));
		}
		Log(fmt::format("{:.1f}s: tip {} of category {} {}", _seconds, text, category, read ? "read" : "not read"));
		return;
	}
	if (command.kind == Kind::LeaveTemple)
	{
		const bool inside = Locator::temple::has_value() && Locator::temple::value().Active();
		if (inside)
		{
			Locator::temple::value().RequestLeave();
		}
		Log(fmt::format("{:.1f}s: {}", _seconds, inside ? "leaving the temple" : "not in the temple"));
		return;
	}
	if (command.kind == Kind::ScriptPopulate)
	{
		// the flock made at the point with no height of its own above the land, as a marker on the land gives it
		const auto point = MapPoint(_middle, command.point);
		Log(fmt::format("{:.1f}s: populate with {} of type {} subtype {}: {}", _seconds, command.amount, command.value,
		                command.subtype,
		                ScriptPopulate(glm::vec3(point.x, 0.0f, point.y), static_cast<int32_t>(command.value), command.subtype,
		                               command.amount)));
		return;
	}
	if (command.kind == Kind::ScriptCreate)
	{
		// at the point with no height of its own above the land, as a marker on the land gives it
		const auto point = MapPoint(_middle, command.point);
		Log(fmt::format("{:.1f}s: create of type {} subtype {}: {}", _seconds, command.value, command.subtype,
		                ScriptCreate(glm::vec3(point.x, 0.0f, point.y), static_cast<int32_t>(command.value), command.subtype)));
		return;
	}
	if (command.kind == Kind::ScriptReward)
	{
		const auto point = MapPoint(_middle, command.point);
		const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
		Log(fmt::format(
		    "{:.1f}s: reward {} {}: {}", _seconds, command.value, command.fromSky ? "from the sky" : "on the land",
		    ScriptReward(glm::vec3(point.x, ground, point.y), static_cast<int32_t>(command.value), command.fromSky)));
		return;
	}
	if (command.kind == Kind::ScriptPuzzleTotem)
	{
		const auto point = MapPoint(_middle, command.point);
		const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
		Log(fmt::format("{:.1f}s: puzzle totem {}: {}", _seconds, command.value,
		                ScriptPuzzleTotem(glm::vec3(point.x, ground, point.y), static_cast<uint32_t>(command.value),
		                                  command.angleDegrees, command.amount, command.built, command.unpickupable)));
		return;
	}
	if (command.kind == Kind::PuzzleTotemHeights)
	{
		std::vector<entt::entity> totems;
		std::as_const(Locator::entitiesRegistry::value())
		    .Each<const ecs::components::PuzzleTotem>(
		        [&totems](entt::entity totem, const ecs::components::PuzzleTotem&) { totems.push_back(totem); });
		std::ranges::sort(totems);
		for (const auto totem : totems)
		{
			const auto object = static_cast<uint32_t>(entt::to_integral(totem));
			Log(fmt::format("{:.1f}s: puzzle totem {}: {}{}", _seconds, object, PuzzleTotemHeights(object),
			                ecs::puzzle_totem_hand::IsHeld(totem) ? ", held by the hand" : ""));
		}
		return;
	}
	if (command.kind == Kind::ScriptDance)
	{
		const auto point = MapPoint(_middle, command.point);
		const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
		const auto around = command.danceAround.has_value() ? ObjectAt(*command.danceAround) : std::nullopt;
		Log(fmt::format("{:.1f}s: dance {}{}: {}", _seconds, command.value,
		                around.has_value() ? fmt::format(" around object {}", *command.danceAround) : "",
		                ScriptDance(around.has_value() ? static_cast<uint32_t>(entt::to_integral(*around)) : 0,
		                            static_cast<int32_t>(command.value), glm::vec3(point.x, ground, point.y), command.danceTime,
		                            _lastDance)));
		return;
	}
	if (command.kind == Kind::ScriptInterfaceLevel)
	{
		if (!_interfaceLevelBefore.has_value())
		{
			_interfaceLevelBefore = help::interface_interaction::GetLevel();
		}
		const auto failed = ScriptInterfaceLevel(static_cast<int32_t>(command.value));
		Log(fmt::format("{:.1f}s: interface level {}: {}", _seconds, command.value,
		                failed.empty() ? fmt::format("now {}", help::interface_interaction::GetLevel()) : failed));
		return;
	}
	if (command.kind == Kind::ScriptDanceAttach)
	{
		const auto object = ObjectAt(command.object);
		Log(fmt::format("{:.1f}s: attach object {} to dance {}: {}", _seconds, command.object, _lastDance,
		                object.has_value() ? ScriptAttach(static_cast<uint32_t>(entt::to_integral(*object)), _lastDance)
		                                   : "it is gone"));
		return;
	}
	if (command.kind == Kind::OpenTattooEditor)
	{
		const bool hasInterface = _host.GetInterface() != nullptr;
		const bool opened = hasInterface && gui::OpenTattooEditorOnCreature();
		Log(fmt::format("{:.1f}s: open tattoo editor: {}", _seconds,
		                !hasInterface ? "the game has no interface"
		                : opened      ? "opened"
		                              : "can't (no cave creature, or it is open)"));
		return;
	}
	if (command.kind == Kind::AbodeBuilt)
	{
		// As building work leaves it
		const auto object = ThingOf(command);
		const bool building = object.has_value() && ecs::abodes::IsDrawBuilding(*object);
		if (building)
		{
			ecs::abodes::SetPercentBuilt(*object, command.built);
		}
		Log(fmt::format("{:.1f}s: object {} built {:.2f}{}", _seconds, command.object, command.built,
		                building ? "" : ": it is no abode being built"));
		return;
	}
	if (command.kind == Kind::TownGainsBelief)
	{
		// As a gift to the town's store gives it: pending belief, which the town's next fold adds
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		const auto found = towns.find(k_ScenarioTownId);
		const bool made = found != towns.end() && registry.Valid(found->second);
		if (made)
		{
			ecs::town_stores::AddToBelief(found->second, command.player, command.amount, entt::null, false, 0);
		}
		Log(fmt::format("{:.1f}s: the scenario's town gains belief {:.3f} in player {}{}", _seconds, command.amount,
		                static_cast<int>(command.player), made ? "" : ": none of the scenario's objects makes the town"));
		return;
	}
	if (command.kind == Kind::SetWorshipShare)
	{
		// As the hand letting go of the town's totem sets it, through the totems' slot
		auto& registry = Locator::entitiesRegistry::value();
		const auto& towns = registry.Context().towns;
		const auto found = towns.find(k_ScenarioTownId);
		const bool made = found != towns.end() && registry.Valid(found->second);
		if (made)
		{
			if (Locator::villageTotemSystem::has_value())
			{
				Locator::villageTotemSystem::value().SetTownShare(found->second, command.amount);
			}
			else
			{
				worship::percentage::SetWorshipPercentage(found->second, command.amount);
			}
		}
		// without a worship site the town's share stays 0
		Log(made ? fmt::format("{:.1f}s: the scenario's town's worship share set to {:.2f}: it is {:.2f}", _seconds,
		                       command.amount, worship::percentage::GetWorshipPercentage(found->second))
		         : fmt::format("{:.1f}s: no worship share set: none of the scenario's objects makes the town", _seconds));
		return;
	}
	if (command.kind == Kind::TempleBuilt)
	{
		auto& registry = Locator::entitiesRegistry::value();
		size_t built = 0;
		registry.Each<const ecs::components::Temple, ecs::components::CitadelPartBuild>(
		    [&](const ecs::components::Temple& temple, ecs::components::CitadelPartBuild& part) {
			    if (temple.owner == command.player)
			    {
				    part.percentBuilt = command.built;
				    ++built;
			    }
		    },
		    entt::exclude<ecs::components::Unavailable>);
		Log(fmt::format("{:.1f}s: temple of player {} built {:.2f}{}", _seconds, static_cast<int>(command.player),
		                command.built, built == 0 ? ": the player has no temple" : ""));
		return;
	}
	if (command.kind == Kind::ScriptHealth)
	{
		const auto object = ThingOf(command);
		Log(fmt::format("{:.1f}s: object {} health: {}", _seconds, command.object,
		                object.has_value() ? ScriptHealth(*object) : "no such object"));
		return;
	}
	if (command.kind == Kind::ScriptDelete)
	{
		const auto object = ThingOf(command);
		Log(fmt::format("{:.1f}s: object {} script delete: {}", _seconds, command.object,
		                object.has_value() ? ScriptDelete(*object) : "it is gone"));
		return;
	}
	if (command.kind == Kind::ScriptTotemProperty)
	{
		const auto object = ObjectAt(command.object);
		Log(fmt::format("{:.1f}s: the totem of object {}, property {}: {}", _seconds, command.object, command.value,
		                object.has_value() ? ScriptTotemProperty(*object, static_cast<int32_t>(command.value), command.amount)
		                                   : "no such object"));
		return;
	}
	if (command.kind == Kind::ScriptWalkPath)
	{
		const auto object = ObjectAt(command.object);
		Log(fmt::format("{:.1f}s: object {} walks track {} from {:.3f} to {:.3f}{}: {}", _seconds, command.object,
		                command.value, command.pathFrom, command.pathTo, command.pathBackwards ? " backwards" : "",
		                object.has_value() ? ScriptWalkPath(*object, command) : "no such object"));
		return;
	}
	if (command.kind == Kind::HighDetail)
	{
		// As the script's SET_HIGH_GRAPHICS_DETAIL, through the slot
		const auto object = ThingOf(command);
		const bool slot = Locator::highDetailSystem::has_value();
		if (object.has_value() && slot)
		{
			if (command.value != 0)
			{
				Locator::highDetailSystem::value().Make(*object);
			}
			else
			{
				Locator::highDetailSystem::value().Release(*object);
			}
		}
		Log(fmt::format("{:.1f}s: object {} {}: {}", _seconds, command.object,
		                command.value != 0 ? "in high detail" : "back to normal",
		                !object.has_value() ? "no such object" : (slot ? "done" : "no high-detail system")));
		return;
	}
	if (command.kind == Kind::ScriptFade)
	{
		// As the script's SET_FADE, or SET_FADE_IN with value 1
		const bool director = Locator::cinematicDirectorSystem::has_value();
		if (director)
		{
			auto& cinematic = Locator::cinematicDirectorSystem::value();
			if (command.value == 0)
			{
				cinematic.FadeTo(static_cast<uint8_t>(command.fadeColour >> 16), static_cast<uint8_t>(command.fadeColour >> 8),
				                 static_cast<uint8_t>(command.fadeColour), command.fadeSeconds);
			}
			else
			{
				cinematic.FadeBackToNormal(command.fadeSeconds);
			}
			_faded = true;
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, command.value == 0 ? "fade to the colour" : "fade back to normal",
		                director ? "started" : "no cinematic director"));
		return;
	}
	if (command.kind == Kind::TapHighlight)
	{
		// As the interface's tap by the local player, on the script highlight nearest the point
		const auto point = MapPoint(_middle, command.point);
		const auto& registry = Locator::entitiesRegistry::value();
		entt::entity nearest = entt::null;
		float nearestDistance = std::numeric_limits<float>::max();
		for (const auto thing : ecs::script_highlight::All())
		{
			if (!registry.Valid(thing) || !registry.AllOf<Transform>(thing))
			{
				continue;
			}
			const auto& position = registry.Get<Transform>(thing).position;
			const float distance = glm::distance(glm::vec2(position.x, position.z), point);
			if (distance < nearestDistance)
			{
				nearestDistance = distance;
				nearest = thing;
			}
		}
		std::string result = "there is no highlight";
		if (nearest != entt::null)
		{
			static_cast<void>(ecs::script_highlight::InterfaceTap(nearest, true));
			const bool shown = Locator::tipBubbleSystem::has_value() && Locator::tipBubbleSystem::value().GetSign() == nearest;
			result = shown ? fmt::format("the tip bubble shows text {}", Locator::tipBubbleSystem::value().GetText())
			               : std::string("no tip bubble shows");
		}
		Log(fmt::format("{:.1f}s: {}: {}", _seconds, Name(command.kind), result));
		return;
	}
	if (command.kind == Kind::ScriptMusic)
	{
		const auto type = static_cast<int32_t>(command.value);
		const auto failed = CallScriptNative("START_MUSIC", 1, [type](lhvm::LHVM& vm) { vm.Pushi(type); });
		Log(fmt::format("{:.1f}s: script music {}: {}", _seconds, type, failed.empty() ? "started" : failed));
		return;
	}
	if (command.kind == Kind::MusicState)
	{
		const auto lock = audio::game_music::Lock();
		const auto* music = audio::game_music::Get();
		const auto& script = audio::GetScriptAudioState();
		Log(fmt::format("{:.1f}s: script music {}, line {}, beat {}", _seconds, music != nullptr ? music->GetScriptType() : -1,
		                script.musicLine.load(), script.musicBeat.load()));
		return;
	}
	if (command.kind == Kind::ShowText)
	{
		// As the script's RUN_TEXT, not on a single line, waiting for the player's click
		const bool shown = Locator::helpTextSystem::has_value() && Locator::helpTextSystem::value().IsStarted();
		if (shown)
		{
			static_cast<void>(Locator::helpTextSystem::value().RunText(false, static_cast<uint32_t>(command.value), 1));
		}
		_textShown = _textShown || shown;
		Log(fmt::format("{:.1f}s: text {} {}", _seconds, command.value, shown ? "shown" : "not shown, no help system"));
		return;
	}
	if (command.kind == Kind::PlayFilm)
	{
		if (command.value == 0)
		{
			// As the script's SET_AVI_SEQUENCE(1): the intro film with its schedule, and the black fade gone at once
			// A missing file only leaves the film not playing
			const auto& fileSystem = Locator::filesystem::value();
			if (fileSystem.Exists("Data/intro.bik"))
			{
				auto& player = video::Get();
				player.Play(fileSystem.FindPath("Data/intro.bik"));
				player.ScheduleIntro();
			}
			if (Locator::cinematicDirectorSystem::has_value())
			{
				Locator::cinematicDirectorSystem::value().FadeBackToNormal(0.0f);
			}
		}
		else
		{
			// As the script's SET_AVI_SEQUENCE(2), whether or not the player has a creature: the falling spell's film, and the
			// black fade gone at once
			video::GetFallingSpell().Start();
			if (Locator::cinematicDirectorSystem::has_value())
			{
				Locator::cinematicDirectorSystem::value().FadeBackToNormal(0.0f);
			}
		}
		Log(fmt::format("{:.1f}s: the {} film {}", _seconds, command.value == 0 ? "intro" : "falling spell",
		                video::IsPlaying() ? "plays" : "is not playing"));
		return;
	}
	if (command.kind == Kind::ChangeLand)
	{
		ChangeLand(command.landScript);
		return;
	}
	if (command.kind == Kind::SetGameSpeed)
	{
		// The host takes the speed the other way round, as a multiplier where 2 is half speed
		_host.SetGameSpeed(1.0f / command.gameSpeed);
		Log(fmt::format("{:.1f}s: the game speed is {:.2f}", _seconds, 1.0f / _host.GetGameSpeed()));
		return;
	}
	if (command.kind == Kind::VillagerWalkTo || command.kind == Kind::StoreTakes)
	{
		const auto result = GiveVillagerCommand(command);
		Log(fmt::format("{:.1f}s: object {} {}{}{}", _seconds, command.object, Name(command.kind), result.empty() ? "" : ": ",
		                result));
		return;
	}
	const auto entity = CreatureAt(command.creature);
	// a creature fixture, numbered after the scenario's own creatures, has no label; a land's is the player's
	const auto who = _onLand ? std::string("the player's creature")
	                 : command.creature < _scenario->creatures.size() && !_scenario->creatures[command.creature].label.empty()
	                     ? std::string(_scenario->creatures[command.creature].label)
	                     : fmt::format("creature {}", command.creature);
	// One made and since taken out of the game is told nothing
	if (!entity.has_value() && command.creature < _creatures.size())
	{
		Log(fmt::format("{:.1f}s: {} {}: it is gone", _seconds, who, Name(command.kind)));
		return;
	}
	if (!entity.has_value() || !Locator::creatureLocomotionSystem::has_value() || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	// Lying out cold, it does nothing it is told until it comes round
	if (const auto* needs = Locator::entitiesRegistry::value().TryGet<const CreatureNeeds>(*entity);
	    needs != nullptr && needs->rest == CreatureNeeds::Rest::Unconscious && command.kind != Kind::BringRound)
	{
		Log(fmt::format("{:.1f}s: {} {}: out cold", _seconds, who, Name(command.kind)));
		return;
	}
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	auto& minds = Locator::creatureMindSystem::value();
	const auto point = MapPoint(_middle, command.point);
	std::string result;
	switch (command.kind)
	{
	case Kind::WalkTo:
	case Kind::RunTo:
		result =
		    MoveResultName(locomotion.MoveTo(*entity, point, command.kind == Kind::RunTo ? Pace::Run : Pace::Walk, 0.0f, 1.0f));
		break;
	case Kind::SwapMinds:
		if (const auto other = CreatureAt(command.value))
		{
			result = minds.SwapMinds(*entity, *other) ? "swapped" : "can't";
		}
		else
		{
			result = "no such creature";
		}
		break;
	case Kind::Follow:
		if (const auto leader = CreatureAt(command.value))
		{
			const auto size = Locator::entitiesRegistry::value().Get<Creature>(*entity).size;
			result = MoveResultName(locomotion.Follow(*entity, *leader, k_FollowDistance * std::max(size, 0.5f), Pace::Walk));
		}
		break;
	case Kind::FleeFrom:
		result = MoveResultName(locomotion.FleeFrom(*entity, point));
		break;
	case Kind::TurnToFace:
		result = locomotion.TurnToFace(*entity, point) ? "turning" : "can't";
		break;
	case Kind::FaceCamera:
		result = locomotion.TurnToFace(*entity, glm::xz(Locator::camera::value().GetOrigin())) ? "turning" : "can't";
		break;
	case Kind::Stop:
		locomotion.Stop(*entity);
		break;
	case Kind::PlayAction:
		result = minds.PlayAction(*entity, command.value, command.mirrored) ? "playing" : "busy";
		break;
	case Kind::PlayGesture:
		result = minds.PlayGesture(*entity, command.value) ? "playing" : "busy";
		break;
	case Kind::PullFace:
		minds.PullFace(*entity, command.value);
		break;
	case Kind::ShowFeeling:
		if (const auto face = minds.ShowFeeling(*entity, static_cast<creature_face::Cue>(command.value)))
		{
			result = creature_face::Name(face->face);
		}
		break;
	case Kind::SitDown:
		result = minds.SitDown(*entity) ? "sitting" : "busy";
		break;
	case Kind::StandUp:
		minds.StandUp(*entity);
		break;
	case Kind::Sleep:
		result = minds.Sleep(*entity) ? "started" : "can't";
		break;
	case Kind::Wake:
		minds.Wake(*entity);
		break;
	case Kind::Eat:
		result = minds.Eat(*entity, std::nullopt) ? "started" : "nothing to eat";
		break;
	case Kind::Drink:
		result = minds.Drink(*entity) ? "started" : "no water near";
		break;
	case Kind::RemoveCreature:
		result = ecs::creature_removal::RemoveFromGame(*entity).has_value() ? "removed" : "can't";
		break;
	case Kind::Poo:
		result = minds.Poo(*entity) ? "started" : "can't";
		break;
	case Kind::Puke:
		result = minds.Puke(*entity) ? "started" : "can't";
		break;
	case Kind::Faint:
		result = minds.Faint(*entity) ? "started" : "can't";
		break;
	case Kind::Stroke:
	case Kind::Slap:
		// As a whole session of the hand on the creature would, a full reward or punishment as the hand lets go
		minds.ReceiveFeedback(*entity, command.kind == Kind::Stroke ? 1.0f : -1.0f);
		break;
	case Kind::PickUp:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Lob:
	case Kind::EatHeld:
	case Kind::Examine:
	case Kind::ThrowAt:
	case Kind::KnockDown:
	case Kind::Kick:
	case Kind::PointAt:
		result = GiveObjectCommand(*entity, command);
		break;
	case Kind::HandGrab:
	case Kind::HandStroke:
	case Kind::HandSlap:
	case Kind::HandLetGo:
		result = GiveHandCommand(*entity, command);
		break;
	case Kind::ScriptDesiresOff:
	case Kind::ScriptDesire:
	case Kind::ScriptReaction:
	case Kind::ScriptCreateFromCreature:
	case Kind::ScriptSpeed:
		result = GiveGuideOgreNative(*entity, command, point);
		break;
	case Kind::PutOnLeash:
	case Kind::TieLeash:
	case Kind::UntieLeash:
	case Kind::TakeOffLeash:
	case Kind::ConfineToHome:
	case Kind::MakeLeashable:
	case Kind::HandTapLeash:
	case Kind::LeashKey:
	case Kind::LeashShake:
	case Kind::LeashState:
		result = GiveLeashCommand(*entity, command);
		break;
	case Kind::StartFight:
	case Kind::FightBlow:
	case Kind::FightBlock:
	case Kind::FightStep:
	case Kind::FightSpecial:
	case Kind::FightAuto:
	case Kind::FightCast:
	case Kind::KnockOut:
	case Kind::BringRound:
	case Kind::TieLeashToCreature:
		result = GiveFightCommand(*entity, command);
		break;
	case Kind::CreatureKey:
	case Kind::DoubleClick:
	case Kind::CameraKeys:
	case Kind::ClearCameraView:
	case Kind::LeaveCreatureMode:
	case Kind::OpenCreatureCave:
	case Kind::ApplyTattoo:
	case Kind::RemoveTattoo:
		result = GiveCreatureModeCommand(*entity, command);
		break;
	case Kind::HandGive:
		result = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().GiveHeldTo(*entity)
		             ? "given"
		             : "the hand gives nothing";
		break;
	case Kind::SetHour:
	case Kind::HoldSeed:
	case Kind::DrawGesture:
	case Kind::SummonSeed:
	case Kind::PressKey:
	case Kind::HandTakeFireBall:
	case Kind::HandTake:
	case Kind::SetAlignment:
	case Kind::EndGuidanceWaits:
	case Kind::OpenTempleRooms:
	case Kind::ChangeLand:
	case Kind::VillagerWalkTo:
	case Kind::StoreTakes:
	case Kind::SetGameSpeed:
	case Kind::SetOpenClose:
	case Kind::LayGateStone:
	case Kind::SpiritOut:
	case Kind::SpiritHome:
	case Kind::CreateWorshipSite:
	case Kind::WideScreen:
	case Kind::ShowText:
	case Kind::PlayFilm:
	case Kind::CreateVortex:
	case Kind::VortexFadeIn:
	case Kind::VortexFadeOut:
	case Kind::VortexCrossingFile:
	case Kind::ScriptFade:
	case Kind::HighDetail:
	case Kind::KeyDown:
	case Kind::KeyUp:
	case Kind::ScriptHealth:
	case Kind::ScriptTotemProperty:
	case Kind::ScriptReward:
	case Kind::ScriptDance:
	case Kind::TapHighlight:
	case Kind::ScriptDanceAttach:
	case Kind::ScriptInterfaceLevel:
	case Kind::TempleBuilt:
	case Kind::OpenTattooEditor:
	case Kind::Advisor:
	case Kind::AbodeBuilt:
	case Kind::LeaveTemple:
	case Kind::ScriptPopulate:
	case Kind::SpiritSay:
	case Kind::ScriptMusic:
	case Kind::MusicState:
	case Kind::TownGainsBelief:
	case Kind::ScriptDeleteTemple:
	case Kind::ScriptWalkPath:
	case Kind::ScriptChallengeSnapshot:
	case Kind::ScriptDelete:
	case Kind::ScriptSnapshot:
	case Kind::ScriptPuzzleTotem:
	case Kind::PuzzleTotemHeights:
	case Kind::ReadTip:
	case Kind::LookAtTempleScroll:
	case Kind::ScriptCreate:
	case Kind::SetWorshipShare:
	case Kind::WaitGlobal:
	case Kind::WaitTask:
	case Kind::WaitText:
	case Kind::WaitCamera:
	case Kind::SetGlobal:
	case Kind::StartScript:
	case Kind::LogGlobals:
	case Kind::ClickTexts:
	case Kind::CameraTo:
	case Kind::CameraFollow:
	case Kind::MoveThing:
	case Kind::HandOver:
	case Kind::HandDrop:
	// The mouse commands are given before a creature is looked for
	case Kind::PointerTo:
	case Kind::PointerPress:
	case Kind::PointerRelease:
	case Kind::PointerSweep:
	case Kind::WheelTurn:
	case Kind::PointerDoubleClick:
		break;
	case Kind::SetDesire:
	case Kind::SetPhase:
	case Kind::RewardIf:
		result = TeachMind(*entity, command);
		break;
	case Kind::SetCreatureSize:
	{
		// As the creature spawner's size slider: kept within the sizes a creature may have, and drawn at it
		auto& registry = Locator::entitiesRegistry::value();
		auto& body = registry.Get<Creature>(*entity);
		body.size = creature_morph::ClampScale(command.amount);
		registry.Get<Transform>(*entity).scale = glm::vec3(CreatureArchetype::DrawnScale(body.species, body.size));
		registry.SetDirty();
		result = fmt::format("size {:.3f}", body.size);
		break;
	}
	case Kind::SeeSkill:
		minds.SeeSkill(Locator::entitiesRegistry::value().Get<Transform>(*entity).position, command.value);
		break;
	case Kind::SeeMiracle:
		minds.SeeMiracle(Locator::entitiesRegistry::value().Get<Transform>(*entity).position, command.value);
		break;
	case Kind::KnowMiracle:
		minds.KnowMiracle(*entity, command.value);
		break;
	case Kind::SetAlight:
	{
		// With a value of 1, the scenario's object catches fire in place of the creature
		const auto burning = command.value == 1 ? ObjectAt(command.object) : std::optional(*entity);
		if (Locator::fireSystem::has_value() && burning.has_value())
		{
			Locator::fireSystem::value().SetOnFire(*burning, 0.0f);
		}
		result = Locator::fireSystem::has_value() && burning.has_value() ? "alight" : "no fire";
		break;
	}
	case Kind::CastMiracle:
	{
		const auto target = command.atCreature.has_value() ? CreatureAt(*command.atCreature) : ObjectAt(command.object);
		result = !target.has_value()                                                       ? "nothing to cast at"
		         : minds.TellCast(*entity, static_cast<MagicType>(command.value), *target) ? ""
		                                                                                   : "can't";
		break;
	}
	case Kind::PlayerDid:
	{
		const auto& land = Locator::terrainSystem::value();
		minds.PlayerDid(command.player, command.value, glm::vec3(point.x, land.GetHeightAt(point), point.y), std::nullopt);
		break;
	}
	case Kind::ScriptFocus:
	case Kind::ScriptRelease:
		result = GiveScriptCommand(*entity, command.kind, point);
		break;
	case Kind::ScriptPlay:
	case Kind::ScriptPlayed:
		result = GiveScriptPlay(*entity, command);
		break;
	case Kind::ScriptMove:
		result = GiveScriptMove(*entity, point, command.radius);
		break;
	case Kind::ScriptDoAction:
		result = GiveScriptDoAction(*entity,
		                            command.atCreature.has_value() ? CreatureAt(*command.atCreature)
		                            : command.onTemple             ? FirstTemple()
		                                                           : ObjectAt(command.object),
		                            command.value, command.with.has_value() ? ObjectAt(*command.with) : std::nullopt);
		break;
	}
	Log(fmt::format("{:.1f}s: {} {}{}{}", _seconds, who, Name(command.kind), result.empty() ? "" : ": ", result));
}

std::optional<entt::entity> Runner::FindThing(const testbed_land_timeline::ThingNear& pick) const
{
	namespace timeline = testbed_land_timeline;
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto point = MapPoint(_middle, pick.point);
	// only the things near enough are asked for their kind, as the scripts' search asks only those in its square
	std::vector<timeline::ThingFacts> things;
	registry.Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		const glm::vec2 at {transform.position.x, transform.position.z};
		if (!timeline::WithinReach(at, pick, point))
		{
			return;
		}
		const auto type = ecs::script_type::TypeOf(entity);
		const bool any = pick.subtype == ecs::script_type::k_AnySubtype;
		things.push_back({
		    .id = static_cast<uint32_t>(entt::to_integral(entity)),
		    .type = type,
		    .subtype = type == pick.type && !any ? ecs::script_type::SubtypeOf(entity) : ecs::script_type::k_NoSubtype,
		    .at = at,
		});
	});
	const auto found = timeline::Nearest(things, pick, point);
	if (!found.has_value())
	{
		return std::nullopt;
	}
	return static_cast<entt::entity>(*found);
}

std::optional<entt::entity> Runner::ThingOf(const Command& command) const
{
	return command.thing.has_value() ? FindThing(*command.thing) : ObjectAt(command.object);
}

std::optional<entt::entity> Runner::OtherThingOf(const Command& command) const
{
	return command.other.has_value() ? FindThing(*command.other) : ObjectAt(command.value);
}

std::string Runner::GiveLandCommand(const Command& command)
{
	const auto ground = [](glm::vec2 at) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
	};
	switch (command.kind)
	{
	case Kind::CameraTo:
	{
		if (!Locator::camera::has_value())
		{
			return "no camera";
		}
		_followed.reset();
		const auto placement = Placed(command.eye, command.look, _middle, ground);
		Locator::camera::value().SetOrigin(placement.origin).SetFocus(placement.focus);
		return fmt::format("at ({:.1f}, {:.1f}, {:.1f}) looking at ({:.1f}, {:.1f}, {:.1f})", placement.origin.x,
		                   placement.origin.y, placement.origin.z, placement.focus.x, placement.focus.y, placement.focus.z);
	}
	case Kind::CameraFollow:
	{
		if (command.value == 1)
		{
			_followed.reset();
			return "let go";
		}
		_followed = command.thing.has_value() ? FindThing(*command.thing) : CreatureAt(command.creature);
		_followDistance = command.amount;
		return _followed.has_value() ? fmt::format("thing {}", entt::to_integral(*_followed)) : "nothing to follow";
	}
	case Kind::MoveThing:
	{
		const auto thing = command.thing.has_value() ? FindThing(*command.thing) : CreatureAt(command.creature);
		if (!thing.has_value())
		{
			return "nothing to move";
		}
		// the thing, then the point's x, y and z, which the native takes off the stack the other way round
		const auto point = MapPoint(_middle, command.point);
		const glm::vec3 at {point.x, ground(point), point.y};
		const auto object = static_cast<uint32_t>(entt::to_integral(*thing));
		const auto failed = CallScriptNative("SET_POSITION", 4, [object, at](lhvm::LHVM& vm) {
			vm.Pusho(object);
			vm.Pushf(at.x);
			vm.Pushf(at.y);
			vm.Pushf(at.z);
		});
		if (!failed.empty())
		{
			return failed;
		}
		const auto& position = Locator::entitiesRegistry::value().Get<Transform>(*thing).position;
		return fmt::format("thing {} now at ({:.1f}, {:.1f})", object, position.x, position.z);
	}
	case Kind::HandOver:
	{
		if (command.value == 1)
		{
			_handHoldPoint.reset();
			_holdPointer = false;
			return "the pointer let go";
		}
		if (!Locator::inputState::has_value() || !Locator::windowing::has_value())
		{
			return "no pointer";
		}
		const auto point = MapPoint(_middle, command.point);
		_handHoldPoint = glm::vec3(point.x, ground(point), point.y);
		_holdPointer = true;
		return fmt::format("over ({:.1f}, {:.1f})", point.x, point.y);
	}
	case Kind::HandDrop:
	{
		if (!Locator::handGrabSystem::has_value())
		{
			return "no hand";
		}
		auto& grab = Locator::handGrabSystem::value();
		const auto held = grab.GetHeld();
		if (!held.has_value())
		{
			return "the hand holds nothing";
		}
		grab.ForceDrop();
		return fmt::format("thing {} let go", entt::to_integral(*held));
	}
	default:
		return {};
	}
}

void Runner::FollowThing()
{
	if (!_followed.has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(*_followed) || !registry.AllOf<Transform>(*_followed))
	{
		_followed.reset();
		Log(fmt::format("{:.1f}s: the camera's thing is gone; the camera is the player's", _seconds));
		return;
	}
	// a creature by its height, anything else as a small creature
	constexpr float k_ThingSize = 0.5f;
	const auto* creature = registry.TryGet<const Creature>(*_followed);
	const auto height = CreatureHeight(creature != nullptr ? creature->size : k_ThingSize);
	const auto placement = Follow(registry.Get<const Transform>(*_followed).position, height, _followDistance);
	Locator::camera::value().SetOrigin(placement.origin).SetFocus(placement.focus);
}

bool Runner::WaitIsOver(const Command& command)
{
	namespace timeline = testbed_land_timeline;
	switch (command.kind)
	{
	case Kind::WaitGlobal:
	{
		if (!Locator::vm::has_value())
		{
			return false;
		}
		const auto& globals = Locator::vm::value().GetVariables();
		const auto index = timeline::GlobalIndex(globals, command.global);
		return index.has_value() && timeline::Holds(command.compare, timeline::NumberOf(globals[*index]), command.amount);
	}
	case Kind::WaitTask:
	{
		const bool running =
		    Locator::vm::has_value() && timeline::ScriptRunning(Locator::vm::value().GetTasks(), command.script);
		// a new wait has seen no task of its script yet
		if (_taskWaitAt != _timeline.next)
		{
			_taskWaitAt = _timeline.next;
			_taskSeenRunning = false;
		}
		_taskSeenRunning = _taskSeenRunning || running;
		return timeline::TaskWaitHolds(static_cast<timeline::TaskWait>(command.value), running, _taskSeenRunning);
	}
	case Kind::WaitText:
	{
		const auto* dialogue =
		    Locator::helpTextSystem::has_value() ? std::as_const(Locator::helpTextSystem::value()).GetDialogue() : nullptr;
		return dialogue != nullptr && timeline::TextHolds(dialogue->GetTexts(), dialogue->IsWaitingForClick(),
		                                                  static_cast<uint32_t>(command.value), command.read);
	}
	case Kind::WaitCamera:
		return Locator::camera::has_value() &&
		       timeline::CameraWithin(Locator::camera::value().GetOrigin(), GroundPoint(command.point), command.radius);
	default:
		return true;
	}
}

glm::vec3 Runner::GroundPoint(glm::vec2 point) const
{
	const auto at = MapPoint(_middle, point);
	const float height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
	return {at.x, height, at.y};
}

std::string Runner::GiveScriptsCommand(const Command& command)
{
	namespace timeline = testbed_land_timeline;
	if (command.kind == Kind::ClickTexts)
	{
		_clickTexts = command.value != 0;
		return _clickTexts ? "the texts are clicked on" : "the texts are the player's to click";
	}
	if (command.kind == Kind::WaitText)
	{
		return fmt::format("text {} {}", command.value, command.read ? "read" : "shown");
	}
	if (command.kind == Kind::WaitCamera)
	{
		const auto point = GroundPoint(command.point);
		return fmt::format("within {:.1f} of ({:.1f}, {:.1f}, {:.1f})", command.radius, point.x, point.y, point.z);
	}
	if (!Locator::vm::has_value())
	{
		return "the scripts have no VM";
	}
	auto& vm = Locator::vm::value();
	const auto& globals = vm.GetVariables();
	const auto valueOf = [&globals](std::string_view name) {
		const auto index = timeline::GlobalIndex(globals, name);
		return index.has_value() ? fmt::format("{}={}", name, timeline::NumberOf(globals[*index]))
		                         : fmt::format("{}: no such global", name);
	};
	switch (command.kind)
	{
	case Kind::WaitGlobal:
		return fmt::format("{} {} {}", valueOf(command.global), timeline::Name(command.compare), command.amount);
	case Kind::WaitTask:
	{
		constexpr std::array<std::string_view, timeline::k_TaskWaits> k_Waits {"runs", "does not run", "has finished"};
		return fmt::format("{} {}", command.script, k_Waits.at(command.value));
	}
	case Kind::SetGlobal:
	{
		const auto index = timeline::GlobalIndex(globals, command.global);
		if (!index.has_value())
		{
			return fmt::format("{}: no such global", command.global);
		}
		vm.SetVariable(static_cast<uint32_t>(*index), timeline::ValueOf(globals[*index].type, command.amount));
		return valueOf(command.global);
	}
	case Kind::StartScript:
	{
		// As the LHVM viewer's Start: only a script that takes no parameters, since a task started from outside has none
		// to give it
		const auto& scripts = vm.GetScripts();
		const auto script = std::ranges::find(scripts, command.script, &lhvm::VMScript::name);
		if (script == scripts.end())
		{
			return fmt::format("no script {}", command.script);
		}
		if (script->parameterCount != 0)
		{
			return fmt::format("{} takes {} parameters", command.script, script->parameterCount);
		}
		const auto task = vm.StartScript(std::string(command.script), lhvm::ScriptType::All);
		return task != 0 ? fmt::format("{} started as task {}", command.script, task)
		                 : fmt::format("{} not started", command.script);
	}
	case Kind::LogGlobals:
	{
		std::vector<std::string> lines;
		for (const auto name : timeline::Names(command.global))
		{
			lines.push_back(valueOf(name));
		}
		for (const auto name : timeline::Names(command.script))
		{
			lines.push_back(fmt::format("{} {}", name, timeline::ScriptRunning(vm.GetTasks(), name) ? "runs" : "does not run"));
		}
		return fmt::format("{}", fmt::join(lines, ", "));
	}
	default:
		return {};
	}
}

void Runner::ClickWaitingText()
{
	if (!_clickTexts || !Locator::helpTextSystem::has_value())
	{
		return;
	}
	// The dialogue itself refuses the click until the text has been shown long enough, as it refuses the player's
	auto* dialogue = Locator::helpTextSystem::value().GetDialogue();
	if (dialogue == nullptr || !dialogue->IsWaitingForClick())
	{
		return;
	}
	dialogue->ProcessClick(true);
	if (!dialogue->IsWaitingForClick())
	{
		Log(fmt::format("{:.1f}s: text {} clicked on", _seconds, dialogue->GetCurrentText()));
	}
}

std::string Runner::GiveAdvisorCommand(const Command& command) const
{
	if (!Locator::advisorSystem::has_value())
	{
		return "no advisors";
	}
	auto& control = Locator::advisorSystem::value().GetController();
	// The scripts name the good advisor 1 and the evil one 2
	const int32_t type = command.value == 0 ? 1 : 2;
	const auto who = command.value == 0 ? "the good advisor" : "the evil advisor";
	using Action = Command::AdvisorAction;
	switch (command.advisor)
	{
	case Action::Out:
		control.SpiritEject(type, false);
		return fmt::format("{} comes out", who);
	case Action::Appear:
		control.SpiritEject(type, true);
		return fmt::format("{} appears", who);
	case Action::Home:
		control.SpiritHome(type, false);
		return fmt::format("{} goes home", who);
	case Action::Vanish:
		control.SpiritHome(type, true);
		return fmt::format("{} vanishes", who);
	case Action::Cling:
		control.SpiritCling(type, command.point.x, command.point.y);
		return fmt::format("{} clings at ({:.2f}, {:.2f})", who, command.point.x, command.point.y);
	case Action::Fly:
		control.SpiritFly(type, command.point.x, command.point.y);
		return fmt::format("{} flies to ({:.2f}, {:.2f})", who, command.point.x, command.point.y);
	case Action::PointOnScreen:
	{
		const auto& screen = control.GetScreen();
		control.SpiritScreenPoint(type, glm::ivec2(static_cast<int32_t>(static_cast<float>(screen.width) * command.point.x),
		                                           static_cast<int32_t>(static_cast<float>(screen.height) * command.point.y)));
		return fmt::format("{} points at ({:.2f}, {:.2f}) on the screen", who, command.point.x, command.point.y);
	}
	case Action::PointAtLand:
	case Action::LookAtLand:
	{
		const auto at = MapPoint(_middle, command.point);
		const float height = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : 0.0f;
		const glm::vec3 position(at.x, height, at.y);
		if (command.advisor == Action::LookAtLand)
		{
			control.SpiritLookAtPosition(type, position);
			return fmt::format("{} looks at ({:.0f}, {:.0f})", who, at.x, at.y);
		}
		control.SpiritPointPosition(type, position, command.gentle);
		return fmt::format("{} points at ({:.0f}, {:.0f}){}", who, at.x, at.y, command.gentle ? " out in the world" : "");
	}
	case Action::PlayAnim:
		control.SpiritPlayAnim(type, command.point.x, command.point.y, command.anim, command.amount);
		return fmt::format("{} plays {}", who, help::spirits::AnimName(command.anim));
	case Action::Feel:
		control.Dude(type == 1 ? help::spirits::k_GoodDude : help::spirits::k_EvilDude).SetEmotion(command.anim, 1.0f);
		return fmt::format("{} feels {}", who, help::spirits::EmotionName(command.anim));
	}
	return "";
}

void Runner::GiveSceneryCommand(const Command& command)
{
	const auto object = ThingOf(command);
	if (!object.has_value() || !Locator::animatedStaticSystem::has_value())
	{
		Log(fmt::format("{:.1f}s: no such object", _seconds));
		return;
	}
	auto& scenery = Locator::animatedStaticSystem::value();
	if (command.kind == Kind::SetOpenClose)
	{
		const bool done = scenery.SetOpenState(*object, static_cast<int32_t>(command.value));
		Log(fmt::format("{:.1f}s: {} object {}{}", _seconds, command.value == 1 ? "opened" : "closed", command.object,
		                done ? "" : ", which isn't animated scenery"));
		return;
	}
	// As the hand gives it: laid in the plinth, the stone is used up, leaving its ghost
	const auto plinth = OtherThingOf(command);
	if (plinth.has_value() && scenery.LayGateStone(*plinth, *object))
	{
		ecs::object_ghosts::Add(*object);
		ecs::ToBeDeleted(*object);
		Log(fmt::format("{:.1f}s: object {} laid in the plinth, now worth {}", _seconds, command.object,
		                scenery.GateStoneValue(*plinth).value_or(0)));
		return;
	}
	Log(fmt::format("{:.1f}s: object {} isn't taken by object {}", _seconds, command.object, command.value));
}

void Runner::GiveKey(uint32_t key, bool down) const
{
	if (!Locator::gameActionSystem::has_value())
	{
		return;
	}
	SDL_Event event {};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.scancode = static_cast<SDL_Scancode>(key);
	Locator::gameActionSystem::value().ProcessEvent(event);
}

void Runner::LetGoOfCameraKeys()
{
	if (!_cameraKeys.has_value())
	{
		return;
	}
	// The cursor key first, then Shift or Ctrl, as a player's fingers come off them
	for (const auto key : std::views::reverse(_cameraKeys->keys))
	{
		if (_keys.Up(static_cast<uint32_t>(key)))
		{
			GiveKey(static_cast<uint32_t>(key), false);
		}
	}
	_cameraKeys.reset();
}

void Runner::UpdateCreatureModeInput(float seconds)
{
	if (_cameraKeys.has_value() && _seconds >= _cameraKeys->until)
	{
		LetGoOfCameraKeys();
		const bool active = Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive();
		Log(fmt::format("{:.1f}s: camera keys let go{}", _seconds, active ? "" : ", not in Creature Mode"));
	}
	if (!_doubleClick.has_value() || !Locator::gameActionSystem::has_value() || !Locator::windowing::has_value() ||
	    !Locator::camera::has_value() || !Locator::creatureHandSystem::has_value())
	{
		return;
	}
	auto& click = *_doubleClick;
	const auto windowId = Locator::windowing::value().GetID();
	const auto position = _pointer.GetPosition();
	if (click.down)
	{
		// The button comes up the frame after; Creature Mode took the double click as it went down
		Locator::gameActionSystem::value().ProcessEvent(DoubleClickEvent(false, position.x, position.y, windowId));
		const auto followed =
		    Locator::creatureModeSystem::has_value() ? Locator::creatureModeSystem::value().GetCreature() : std::nullopt;
		Log(fmt::format("{:.1f}s: double clicked entity {}: {}", _seconds, static_cast<uint32_t>(click.creature),
		                followed.has_value() ? fmt::format("following entity {}", static_cast<uint32_t>(*followed))
		                                     : std::string("the camera is the player's")));
		_doubleClick.reset();
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	click.seconds += seconds;
	if (!registry.Valid(click.creature) || click.seconds > k_DoubleClickAimSeconds)
	{
		Log(fmt::format("{:.1f}s: DoubleClick: the hand never came over entity {}", _seconds,
		                static_cast<uint32_t>(click.creature)));
		_doubleClick.reset();
		return;
	}
	// The hand saw the creature under the cursor in its last update, which Creature Mode reads this frame
	if (Locator::creatureHandSystem::value().CreatureUnderHand() == click.creature)
	{
		Locator::gameActionSystem::value().ProcessEvent(DoubleClickEvent(true, position.x, position.y, windowId));
		click.down = true;
		return;
	}
	// The cursor over the creature's middle, wherever it and the camera have gone
	const auto& transform = registry.Get<const Transform>(click.creature);
	const float height = creature_mode::CreatureHeight(ecs::creature_pose::DrawnSize(registry, click.creature));
	const auto size = WindowSize();
	glm::vec3 screen {0.0f};
	if (Locator::camera::value().ProjectWorldToScreen(transform.position + glm::vec3(0.0f, height * 0.5f, 0.0f),
	                                                  {0.0f, 0.0f, size.x, size.y}, screen))
	{
		_pointer.MoveTo(glm::ivec2(glm::round(glm::vec2(screen))), size);
	}
}

void Runner::GiveVortexCommand(const Command& command)
{
	if (!Locator::vortexSystem::has_value())
	{
		Log(fmt::format("{:.1f}s: {}: there is no vortex system", _seconds, Name(command.kind)));
		return;
	}
	auto& slot = Locator::vortexSystem::value();
	if (command.kind == Kind::CreateVortex)
	{
		// As a script's CREATE, on the land at the point
		const auto point = MapPoint(_middle, command.point);
		const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
		constexpr std::array<VortexType, 3> k_Types {VortexType::In, VortexType::Out, VortexType::Volcano};
		constexpr std::array<std::string_view, 3> k_TypeNames {"in", "out", "volcano"};
		const auto which = std::min<size_t>(command.value, k_Types.size() - 1);
		const auto made = slot.Create(glm::vec3(point.x, ground, point.y), k_Types.at(which));
		_vortices.push_back({.entity = made, .point = point});
		Log(fmt::format("{:.1f}s: vortex {} ({}) {}", _seconds, _vortices.size() - 1, k_TypeNames.at(which),
		                made == entt::null ? "not made" : "made"));
		return;
	}
	const auto entity = command.value < _vortices.size() ? _vortices[command.value].entity : entt::null;
	if (command.kind == Kind::VortexCrossingFile)
	{
		// In place of the reader the way into the land opened as it was made, before its first emission
		auto* vortex = entity != entt::null
		                   ? Locator::entitiesRegistry::value().TryGet<ecs::components::LandscapeVortex>(entity)
		                   : nullptr;
		if (vortex == nullptr || vortex->type != VortexType::Out)
		{
			Log(fmt::format("{:.1f}s: vortex {} is not a way into a land", _seconds, command.value));
			return;
		}
		vortex->reader = std::make_shared<ecs::vortex_save::Reader>(ecs::vortex_save::ReaderFrom(command.crossingFile));
		Log(fmt::format("{:.1f}s: vortex {} reads a crossing file of {} lines{}", _seconds, command.value,
		                vortex->reader->lines.size(), vortex->reader->lastLineEnded ? "" : ", the last with no new line"));
		return;
	}
	bool done = false;
	if (command.kind == Kind::VortexFadeIn)
	{
		// The fade in has no slot of its own: the vortex module's, as the debug window's
		done = entity != entt::null && Locator::entitiesRegistry::value().Valid(entity);
		ecs::vortex::StartFadeIn(entity);
	}
	else
	{
		done = entity != entt::null && slot.StartFadeOut(entity);
	}
	Log(fmt::format("{:.1f}s: vortex {} {}", _seconds, command.value,
	                !done ? "is not there" : (command.kind == Kind::VortexFadeIn ? "fades in" : "fades out")));
}

void Runner::LogVortexLand()
{
	if (_vortexLandLogged || _vortices.size() < 2 || !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto there = [&registry](const MadeVortex& made) {
		return made.entity != entt::null && registry.Valid(made.entity) &&
		       registry.AllOf<ecs::components::LandscapeVortex>(made.entity);
	};
	if (std::ranges::any_of(_vortices, there))
	{
		if (_seconds < k_VortexLandLogSeconds)
		{
			return;
		}
		for (size_t i = 0; i < _vortices.size(); ++i)
		{
			if (there(_vortices[i]))
			{
				Log(fmt::format("{:.1f}s: vortex {} is still there", _seconds, i));
			}
		}
	}
	_vortexLandLogged = true;
	// The cells both levelled squares take in, by the cells round each vortex's own
	const auto first = openblack::vortex::CentreCell(_vortices[0].point);
	const auto second = openblack::vortex::CentreCell(_vortices[1].point);
	const auto low = glm::max(first, second) - openblack::vortex::k_LevelHalfSide;
	const auto high = glm::min(first, second) + openblack::vortex::k_LevelHalfSide;
	if (low.x > high.x || low.y > high.y)
	{
		Log(fmt::format("{:.1f}s: the vortices' squares share no cells", _seconds));
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	for (int x = low.x; x <= high.x; ++x)
	{
		std::vector<int> heights;
		for (int z = low.y; z <= high.y; ++z)
		{
			const bool on = x >= 0 && z >= 0 && x < flat_land::k_CellsPerSide && z < flat_land::k_CellsPerSide &&
			                land.HasBlockAt(glm::u16vec2(x, z));
			heights.push_back(on ? land.GetCellAltitude(land.GetCell(glm::u16vec2(x, z))) & 0xFF : 0);
		}
		Log(fmt::format("{:.1f}s: the shared cells at x {}, z {} to {}: {}", _seconds, x, low.y, high.y,
		                fmt::join(heights, " ")));
	}
}

std::string Runner::GiveCreatureModeCommand(entt::entity creature, const Command& command)
{
	if (!Locator::creatureModeSystem::has_value() || !Locator::creatureCaveSystem::has_value())
	{
		return "no creature mode";
	}
	auto& mode = Locator::creatureModeSystem::value();
	auto& cave = Locator::creatureCaveSystem::value();
	const auto following = [&mode] {
		const auto followed = mode.GetCreature();
		return followed.has_value() ? fmt::format("following entity {}", static_cast<uint32_t>(*followed))
		                            : std::string("the camera is the player's");
	};
	switch (command.kind)
	{
	case Kind::CreatureKey:
		mode.PressCreatureKey();
		return following();
	case Kind::DoubleClick:
	{
		// The cursor goes over the creature; once the hand sees it there, the double click is given to the game's
		// actions, as the window gives a player's, and Creature Mode reads it there
		if (!Locator::inputState::has_value() || !Locator::windowing::has_value() || !Locator::camera::has_value() ||
		    !Locator::gameActionSystem::has_value() || !Locator::creatureHandSystem::has_value())
		{
			return "no mouse";
		}
		if (_doubleClick.has_value())
		{
			return "still double clicking";
		}
		if (!_pointer.IsTaken())
		{
			_pointer.Take();
		}
		KeepDebugWindowsOffTheMouse(true);
		_doubleClick = CreatureDoubleClick {.creature = creature};
		return fmt::format("the cursor goes over entity {}", static_cast<uint32_t>(creature));
	}
	case Kind::CameraKeys:
	{
		// The keys go down in the game's actions as the window gives real keys, and Creature Mode's camera reads them
		// each frame until they are let go
		if (!Locator::gameActionSystem::has_value())
		{
			return "no keys";
		}
		const auto keys = CameraKeysOf(Locator::gameActionSystem::value().GetKeyBindings(), command.value, command.ctrl);
		if (!keys.has_value())
		{
			return "the keys aren't bound";
		}
		LetGoOfCameraKeys();
		for (const auto key : *keys)
		{
			if (_keys.Down(static_cast<uint32_t>(key)))
			{
				GiveKey(static_cast<uint32_t>(key), true);
			}
		}
		_cameraKeys = HeldCameraKeys {.keys = *keys, .until = _seconds + command.amount};
		return fmt::format("{} and {} held for {:.1f}s{}", SDL_GetScancodeName(keys->at(0)), SDL_GetScancodeName(keys->at(1)),
		                   command.amount, mode.IsActive() ? "" : ", not in Creature Mode");
	}
	case Kind::ClearCameraView:
		mode.ClearView();
		if (const auto view = mode.GetView(); view.has_value())
		{
			return fmt::format("heading {:.2f}, pitch {:.2f}", view->yaw, view->pitch);
		}
		return "not in Creature Mode";
	case Kind::LeaveCreatureMode:
		mode.Leave();
		return following();
	case Kind::OpenCreatureCave:
		cave.Open();
		cave.GetScreen().page = static_cast<creature_cave::Page>(command.value);
		cave.GetScreen().pageRequested = true;
		return cave.GetCreature().has_value() ? "" : "the player has no creature";
	case Kind::ApplyTattoo:
		return cave.ApplyTattoo(static_cast<uint8_t>(command.bodyPart), static_cast<uint8_t>(command.value),
		                        glm::u8vec3(180, 30, 30))
		           ? ""
		           : "can't";
	case Kind::RemoveTattoo:
		return cave.RemoveTattoo(static_cast<uint8_t>(command.bodyPart)) ? "" : "nothing there";
	default:
		return {};
	}
}

std::string Runner::TeachMind(entt::entity entity, const Command& command)
{
	auto* mind = Locator::entitiesRegistry::value().TryGet<CreatureMindState>(entity);
	if (mind == nullptr)
	{
		return "no mind";
	}
	switch (command.kind)
	{
	case Kind::SetDesire:
		if (mind->desires.has_value() && command.value < creature_desires::k_DesireCount)
		{
			auto& state = mind->desires->desires.at(command.value);
			state.activated = true;
			state.suppressedTurns = 0;
			state.value = command.amount * std::max(state.max, 0.0f);
			return fmt::format("{} {:.2f}", creature_desires::Name(static_cast<creature_desires::Desire>(command.value)),
			                   state.value);
		}
		return "no desires yet";
	case Kind::SetPhase:
		mind->developmentPhase = static_cast<uint32_t>(command.value);
		return fmt::format("stage {}", command.value);
	case Kind::RewardIf:
		// From now on each thing it does to something is judged as soon as it is done
		mind->trainer = static_cast<uint32_t>(command.value);
		return fmt::format("trained: stroked for a {}, slapped for anything else",
		                   creature_tree::BeliefName(static_cast<uint32_t>(command.value)));
	default:
		return {};
	}
}

void Runner::LoadMindFile(entt::entity entity, std::string_view name)
{
	constexpr std::string_view k_Game = "game:";
	// The file last opened with the spawner's file dialog isn't kept in this tree, so it is always the game's own mind
	// of that name, which it falls back to
	constexpr std::string_view k_Chosen = "chosen:";
	std::string file;
	if (name.starts_with(k_Game))
	{
		file = name.substr(k_Game.size());
	}
	else if (name.starts_with(k_Chosen))
	{
		file = name.substr(k_Chosen.size());
	}
	if (file.empty() || !Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		Log(fmt::format("mind file {}: {}", name, creaturemind::ResultToStr(creaturemind::MindResult::ErrCantOpen)));
		return;
	}
	// Through the resource cache, by the same name the spawner's list loads it with
	const auto path = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true) / file;
	auto& minds = Locator::resources::value().GetCreatureMinds();
	const auto id = minds.Load(file, resources::CreatureMindLoader::FromDiskTag {}, path).first->first;
	const auto handle = minds.Handle(id);
	const auto result = handle ? handle->result : creaturemind::MindResult::ErrCantOpen;
	if (handle && handle->Loaded() && Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().LoadMind(entity, std::make_shared<const creaturemind::MindFileData>(handle->data));
	}
	Log(fmt::format("mind file {}: {}", file, creaturemind::ResultToStr(result)));
}

void Runner::Frame(Shot shot, size_t creature, float distance)
{
	_shot = shot;
	_shotCreature = creature;
	_shotDistance = distance;
	UpdateCamera();
}

void Runner::LogHandLight()
{
	if (_scenario == nullptr || !_scenario->logHandLight || !Locator::dayNightClock::has_value())
	{
		return;
	}
	// The table the renderer built last, which the night lights read this frame
	const auto reading = ReadHandLight(land_light::CurrentTable().GetLandColour());
	if (_handLightColour == reading.landColour)
	{
		return;
	}
	_handLightColour = reading.landColour;
	Log(fmt::format("{:.2f}s: {}", _seconds, Describe(reading, Locator::dayNightClock::value().Clock().GetScriptTime())));
}

void Runner::LogSunGlare()
{
	if (_scenario == nullptr || !_scenario->logSunGlare || !Locator::skySystem::has_value() ||
	    !Locator::terrainSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	// As the renderer looks for the land in the way: from the camera's origin, with the near clip it draws with
	const auto& camera = Locator::camera::value();
	const auto origin = camera.GetOrigin();
	const auto& land = Locator::terrainSystem::value();
	const auto reading =
	    ReadSunGlare(Locator::skySystem::value().GetTime(), origin,
	                 camera.GetDrawnNearClip(Locator::config::value().cameraNearClip), [&](const glm::vec3& point) {
		                 glm::vec2 hit(0.0f);
		                 return land.RayCast(origin, point, hit, origin);
	                 });
	auto line = reading.has_value() ? Describe(*reading) : std::string("the sun is down");
	if (_sunGlareLine == line)
	{
		return;
	}
	Log(fmt::format("{:.2f}s: {}", _seconds, line));
	_sunGlareLine = std::move(line);
}

void Runner::UpdateCamera()
{
	if (!_shot.has_value() || _scenario == nullptr || !Locator::terrainSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	// Creature Mode takes the camera from the scenario's shot for good
	if (Locator::creatureModeSystem::has_value() && Locator::creatureModeSystem::value().IsActive())
	{
		_shot.reset();
		return;
	}
	auto& camera = Locator::camera::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	std::optional<CameraPlacement> placement;
	switch (*_shot)
	{
	case Shot::Testbed:
		// As the testbed leaves it: 120 units south of the middle and 60 up, looking at the middle
		placement = CameraPlacement {
		    .origin = {_middle.x, land.GetHeightAt(_middle) + 60.0f, _middle.y - 120.0f},
		    .focus = {_middle.x, land.GetHeightAt(_middle), _middle.y},
		};
		break;
	case Shot::Overview:
	{
		std::vector<glm::vec2> points;
		// the scenario's own creatures only: a creature fixture is framed through the shot's points
		for (size_t i = 0; i < std::min(_creatures.size(), _scenario->creatures.size()); ++i)
		{
			if (const auto entity = CreatureAt(i))
			{
				points.push_back(glm::xz(registry.Get<Transform>(*entity).position));
			}
		}
		for (const auto& object : _scenario->objects)
		{
			points.push_back(MapPoint(_middle, object.offset));
		}
		for (const auto& particle : _scenario->particles)
		{
			points.push_back(MapPoint(_middle, particle.offset));
		}
		for (const auto& extra : _scenario->framing.include)
		{
			points.push_back(MapPoint(_middle, extra));
		}
		// And everywhere the creatures are sent
		for (const auto& command : _scenario->commands)
		{
			if (HasPoint(command.kind))
			{
				points.push_back(MapPoint(_middle, command.point));
			}
		}
		const auto bounds = BoundsOf(points, glm::vec2(k_MinOverviewHalfSize));
		placement = Overview({bounds.centre.x, land.GetHeightAt(bounds.centre), bounds.centre.y}, bounds.halfSize,
		                     FieldsOfView(camera), _shotDistance);
		break;
	}
	case Shot::Placed:
	{
		const auto eye = MapPoint(_middle, {_scenario->framing.eye.x, _scenario->framing.eye.z});
		const auto look = MapPoint(_middle, {_scenario->framing.look.x, _scenario->framing.look.z});
		placement = CameraPlacement {
		    .origin = {eye.x, land.GetHeightAt(eye) + _scenario->framing.eye.y, eye.y},
		    .focus = {look.x, land.GetHeightAt(look) + _scenario->framing.look.y, look.y},
		};
		break;
	}
	case Shot::Follow:
	case Shot::Head:
		if (const auto entity = CreatureAt(_shotCreature))
		{
			const auto& transform = registry.Get<Transform>(*entity);
			const auto height = CreatureHeight(registry.Get<Creature>(*entity).size);
			placement = *_shot == Shot::Follow ? Follow(transform.position, height, _shotDistance)
			                                   : Head(transform.position, AheadOf(transform), height, _shotDistance);
		}
		break;
	}
	if (placement.has_value())
	{
		camera.SetOrigin(placement->origin).SetFocus(placement->focus);
	}
	// A shot of everything is taken once; following and close ups keep up with the creature
	if (*_shot == Shot::Overview || *_shot == Shot::Testbed || *_shot == Shot::Placed)
	{
		_shot.reset();
	}
}

void Runner::Update(float seconds)
{
	if (_scenario == nullptr)
	{
		return;
	}
	UpdateCamera();
	if (!_running)
	{
		return;
	}
	// Another land was loaded over the testbed: a new land, or the scenario's creatures or crowd gone with the registry
	const auto gone = [](entt::entity entity) { return !Locator::entitiesRegistry::value().Valid(entity); };
	const auto* land = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	if (_onLand)
	{
		// A land timeline carries on through a change of land, with the player's creature as it now is
		if (land != _land)
		{
			_land = land;
			Log(fmt::format("{:.1f}s: the land changed; the timeline carries on", _seconds));
		}
		RefreshLandCreature();
	}
	else if (land != _land ||
	         (!_creatures.empty() && std::ranges::none_of(std::views::iota(size_t {0}, _creatures.size()),
	                                                      [this](size_t i) { return CreatureAt(i).has_value(); })) ||
	         (!_crowdEntities.empty() && gone(_crowdEntities.front())))
	{
		_running = false;
		_shot.reset();
		RestoreSettings();
		Log("Its creatures are gone");
		return;
	}
	_seconds += seconds;
	LogMiracles();
	LogBuildings();
	StartHandDemo();
	Measure();
	SpawnCrowd();
	UpdateParticles(seconds);
	UpdateMiracles(seconds);
	UpdateVillagerWalks();
	UpdateThrows();
	ApplyStates();
	HoldHand();
	UpdateFireBallTake(seconds);
	UpdatePointer(seconds);
	UpdateDrawing(seconds);
	UpdateCreatureModeInput(seconds);
	ClickWaitingText();
	FollowThing();
	// Before this frame's commands: the land's colour is the one built for the hour the clock still shows
	LogHandLight();
	LogSunGlare();
	LogVortexLand();
	const auto due = Advance(_timeline, _scenario->commands, _scenario->repeatFrom, seconds, [this](const Command& command) {
		// A command to the hand waits for the gesture it is drawing
		if (command.kind == Kind::HoldSeed || command.kind == Kind::DrawGesture || command.kind == Kind::SummonSeed)
		{
			return !_drawing.has_value();
		}
		// A store waits for a villager to walk round what it takes
		if (command.kind == Kind::StoreTakes)
		{
			return IsWalkedRound(command.object);
		}
		// A wait on the scripts waits for what it names
		if (command.kind == Kind::WaitGlobal || command.kind == Kind::WaitTask || command.kind == Kind::WaitText ||
		    command.kind == Kind::WaitCamera)
		{
			return WaitIsOver(command);
		}
		return IsFree(command.creature);
	});
	for (const auto index : due)
	{
		// A land change ends the scenario, and the commands due after it with it
		if (!_running)
		{
			break;
		}
		Give(_scenario->commands[index]);
	}
	if (_onLand)
	{
		NextLandChapter();
	}
	// The buttons held reach the hand this frame: this runs after the game has copied the mouse's own buttons into
	// what the hand reads, and before the hand reads them
	_pointer.WriteButtons();
}

uint32_t Runner::StartParticle(size_t index) const
{
	if (!Locator::particleSystem::has_value() || !Locator::terrainSystem::has_value())
	{
		return ecs::systems::ParticleSystemInterface::k_NoEffect;
	}
	const auto& particle = _scenario->particles.at(index);
	const auto point = MapPoint(_middle, particle.offset);
	const glm::vec3 position {point.x, Locator::terrainSystem::value().GetHeightAt(point) + particle.height, point.y};
	auto& effects = Locator::particleSystem::value();
	// A type plays its own particle file, as the debug window's spawner starts one
	const auto file = particle.file.empty() ? particles::ParticleTypeFile(particle.type) : particle.file;
	if (file.empty())
	{
		return ecs::systems::ParticleSystemInterface::k_NoEffect;
	}
	const auto effect = effects.Start(file, position, particle.magnitude);
	if (effect == ecs::systems::ParticleSystemInterface::k_NoEffect)
	{
		return effect;
	}
	effects.SetPlayer(effect, particle.player);
	effects.SetDrawPath(effect, particle.path);
	if (particle.targetsCreatures)
	{
		// those still about
		for (size_t i = 0; i < _creatures.size(); ++i)
		{
			if (const auto creature = CreatureAt(i))
			{
				effects.AddTarget(effect, *creature);
			}
		}
	}
	if (particle.targetObject.has_value())
	{
		if (const auto object = ObjectAt(*particle.targetObject))
		{
			effects.AddTarget(effect, *object);
		}
	}
	return effect;
}

void Runner::UpdateParticles(float seconds)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	auto& effects = Locator::particleSystem::value();
	for (size_t i = 0; i < _particles.size(); ++i)
	{
		auto& running = _particles.at(i);
		const auto restart = _scenario->particles.at(i).restartSeconds;
		running.seconds += seconds;
		// An effect that has ended, or whose time is up, starts again
		if (effects.Find(running.effect) == nullptr || (restart > 0.0f && running.seconds >= restart))
		{
			effects.CloseDown(running.effect);
			running = {StartParticle(i), 0.0f};
		}
	}
}

void Runner::UpdateMiracles(float seconds)
{
	if (!Locator::magicSystem::has_value())
	{
		return;
	}
	auto& magic = Locator::magicSystem::value();
	for (size_t i = 0; i < _miracles.size(); ++i)
	{
		auto& miracle = _miracles.at(i);
		const auto& setup = _scenario->miracles.at(i);
		if (setup.byHand)
		{
			// The button goes down once the hand is over the point, and comes up after the hold, the hand moving meanwhile
			if (miracle.pressAt.has_value() && _seconds >= *miracle.pressAt)
			{
				magic.PressAction();
				Log(fmt::format("Pressed: {}", HandResultName(magic.GetLastHandResult())));
				miracle.pressAt.reset();
				// A storm or a shield has its circle drawn while the button is down, round the point
				if (setup.circleRadius.has_value() && Locator::gestureEvents::has_value() &&
				    Locator::terrainSystem::has_value())
				{
					const auto point2 = MapPoint(_middle, setup.point);
					Locator::gestureEvents::value().Inject(
					    {.kind = ecs::systems::GestureEvent::Kind::Circle,
					     .gesture = GestureType::Circle,
					     .centre = {point2.x, Locator::terrainSystem::value().GetHeightAt(point2), point2.y},
					     .radius = *setup.circleRadius,
					     .powerUpLevel = -1});
				}
				miracle.releaseAt = _seconds + setup.holdSeconds.value_or(k_HandThrowSeconds);
			}
			if (miracle.releaseAt.has_value())
			{
				miracle.hand += setup.throwVelocity * seconds;
				magic.DriveHand(HandFrameFor(i, miracle.hand));
				if (_seconds >= *miracle.releaseAt)
				{
					magic.ReleaseAction();
					Log(fmt::format("Let go: {}", HandResultName(magic.GetLastHandResult())));
					miracle.releaseAt.reset();
					if (setup.sweepSeconds.has_value())
					{
						miracle.sweepUntil = _seconds + *setup.sweepSeconds;
					}
					else
					{
						magic.DriveHand(std::nullopt);
					}
					// A seed kept in the hand is dropped, for the next go
					if (magic.IsHandBusy())
					{
						magic.DiscardHeldSeed();
					}
				}
			}
			if (miracle.sweepUntil.has_value())
			{
				// The hand sweeps on over the land as the flock pours out of it
				miracle.hand += setup.throwVelocity * seconds;
				magic.DriveHand(HandFrameFor(i, miracle.hand));
				if (_seconds >= *miracle.sweepUntil)
				{
					miracle.sweepUntil.reset();
					magic.DriveHand(std::nullopt);
				}
			}
			if (miracle.nextAt.has_value() && _seconds >= *miracle.nextAt)
			{
				StartHandCast(i);
				miracle.nextAt =
				    setup.repeatSeconds.has_value() ? std::optional(*miracle.nextAt + *setup.repeatSeconds) : std::nullopt;
			}
			continue;
		}
		if (miracle.letGoAt.has_value() && _seconds >= *miracle.letGoAt)
		{
			Locator::magicSystem::value().CloseDown(miracle.spell);
			miracle.letGoAt.reset();
		}
		if (miracle.breakAt.has_value() && _seconds >= *miracle.breakAt)
		{
			miracle.breakAt.reset();
			if (debug::miracles::BreakShield(miracle.spell))
			{
				Log("Shield broken");
			}
		}
		if (!miracle.nextAt.has_value() || _seconds < *miracle.nextAt)
		{
			continue;
		}
		miracle.spell = CastMiracle(i);
		Log(fmt::format("Cast {}{}", static_cast<int>(setup.type), miracle.spell == entt::null ? ", which failed" : ""));
		if (setup.holdSeconds.has_value() && miracle.spell != entt::null)
		{
			miracle.letGoAt = _seconds + *setup.holdSeconds;
		}
		if (setup.breakSeconds.has_value() && miracle.spell != entt::null)
		{
			miracle.breakAt = _seconds + *setup.breakSeconds;
		}
		miracle.nextAt = setup.repeatSeconds.has_value() ? std::optional(*miracle.nextAt + *setup.repeatSeconds) : std::nullopt;
	}
}

void Runner::UpdateVillagerWalks()
{
	auto& registry = Locator::entitiesRegistry::value();
	_walks.resize(_objects.size());
	for (size_t i = 0; i < _objects.size() && i < _scenario->objects.size(); ++i)
	{
		const auto& setup = _scenario->objects.at(i);
		auto& walk = _walks.at(i);
		const auto entity = _objects.at(i);
		if (!registry.Valid(entity) || !registry.AllOf<ecs::components::LivingAction, ecs::components::WallHug>(entity))
		{
			continue;
		}
		if (setup.walkTo.has_value())
		{
			if (!walk.nextAt.has_value() && walk.outwards)
			{
				walk.nextAt = setup.walkAfterSeconds;
			}
			if (walk.nextAt.has_value() && _seconds >= *walk.nextAt)
			{
				const auto goal = MapPoint(_middle, walk.outwards ? *setup.walkTo : setup.offset);
				if (setup.scriptSpeed.has_value())
				{
					const auto failed = ScriptSpeed(entity, *setup.scriptSpeed);
					Log(fmt::format("Villager {} speed {}{}", i, *setup.scriptSpeed, failed.empty() ? "" : ": " + failed));
				}
				ecs::villager::SetupMoveToWithHug(entity, goal, setup.walkFinal);
				Log(fmt::format("Villager {} walks {}", i, walk.outwards ? "out" : "back"));
				walk.outwards = !walk.outwards;
				walk.nextAt =
				    setup.walkRepeatSeconds.has_value() ? std::optional(*walk.nextAt + *setup.walkRepeatSeconds) : std::nullopt;
				if (!setup.walkRepeatSeconds.has_value())
				{
					walk.outwards = false;
				}
			}
		}
		if (setup.dropOnStoneSeconds.has_value() && !walk.dropped && _seconds >= *setup.dropOnStoneSeconds &&
		    Locator::teleportSystem::has_value())
		{
			walk.dropped = true;
			// The stone nearest the villager
			const auto here = registry.Get<const Transform>(entity).position;
			// A villager jumps from its own player's stones only: one without a town is given one of the player's
			auto& villager = registry.Get<ecs::components::Villager>(entity);
			if (villager.town == entt::null || !registry.Valid(villager.town))
			{
				villager.town = registry.Context().towns.at(ScenarioTown(_middle));
			}
			std::optional<entt::entity> nearest;
			float best = std::numeric_limits<float>::max();
			registry.Each<const ecs::components::TeleportStone, const Transform>(
			    [&](entt::entity stone, const ecs::components::TeleportStone&, const Transform& transform) {
				    const float d = glm::distance(transform.position, here);
				    if (d < best)
				    {
					    best = d;
					    nearest = stone;
				    }
			    });
			// The testbed's hand is the first player's; whose stone it is is asked first, as the hand's own drop does
			auto& stones = Locator::teleportSystem::value();
			const bool jumped = nearest.has_value() && stones.CanDropOnStone(entity, *nearest) &&
			                    stones.DropOnStone(entity, *nearest, PlayerNames::PLAYER_ONE);
			Log(fmt::format("Villager {} dropped on a stone: {}", i, jumped ? "jumped" : "stayed"));
		}
	}
}

std::optional<ecs::systems::MagicSystemInterface::HandFrame> Runner::HandFrameFor(size_t index, glm::vec3 hand) const
{
	const auto& setup = _scenario->miracles.at(index);
	const auto& land = Locator::terrainSystem::value();
	const auto point2 = MapPoint(_middle, setup.point);
	glm::vec3 point {point2.x, land.GetHeightAt(point2), point2.y};
	glm::vec3 aim = point;
	if (setup.target == MiracleCast::Target::Creature)
	{
		const auto creature = CreatureAt(setup.creature);
		if (!creature.has_value())
		{
			return std::nullopt;
		}
		point = Locator::entitiesRegistry::value().Get<Transform>(*creature).position;
		// At the middle of its body
		aim = point + glm::vec3(0.0f, k_HandAimHeight, 0.0f);
	}
	// Looking down at it from over the hand
	const auto eye = hand + glm::vec3(0.0f, k_HandEyeHeight, 0.0f);
	const auto toward = aim - eye;
	const auto direction = glm::length(toward) > 0.0f ? glm::normalize(toward) : glm::vec3(0.0f, -1.0f, 0.0f);
	return ecs::systems::MagicSystemInterface::HandFrame {
	    .handPosition = hand,
	    .point = point,
	    .rayOrigin = eye,
	    .rayDirection = direction,
	    .cameraForward = direction,
	    .overWorld = true,
	};
}

void Runner::StartHandCast(size_t index)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& setup = _scenario->miracles.at(index);
	auto& miracle = _miracles.at(index);
	const auto& info = Locator::infoConstants::value();
	auto& magic = Locator::magicSystem::value();
	if (magic.IsHandBusy())
	{
		magic.DiscardHeldSeed();
	}
	const auto seed = magic::GetFirstSpellSeedForMagicType(info, setup.type);
	if (seed == SpellSeedType::None)
	{
		return;
	}
	if (setup.casterAlignment.has_value() && Locator::alignmentSystem::has_value())
	{
		Locator::alignmentSystem::value().SetPlayerAlignment(PlayerNames::PLAYER_ONE, *setup.casterAlignment);
	}
	const auto level = magic::GetPowerUpGesture(magic::GetSpellSeedInfo(info, seed), setup.type).level;
	const auto held = setup.fromWorship ? magic.SummonSeed(PlayerNames::PLAYER_ONE, seed, level)
	                                    : magic.GiveSeedToHand(PlayerNames::PLAYER_ONE, seed, level, 1.0f);
	if (held == entt::null)
	{
		Log("The hand couldn't take the seed");
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	const auto hand2 = MapPoint(_middle, setup.handOffset);
	miracle.hand = {hand2.x, land.GetHeightAt(hand2) + setup.handHeight, hand2.y};
	magic.DriveHand(HandFrameFor(index, miracle.hand));
	// A seed from worship waits until it is ready
	const float wait = setup.holdBeforePress.has_value() ? *setup.holdBeforePress
	                   : setup.fromWorship               ? info.spellSystem.delayBeforeSeedActive + k_HandPressWait
	                                                     : k_HandPressWait;
	miracle.pressAt = _seconds + wait;
	Log(fmt::format("Seed of {} in the hand{}", static_cast<int>(setup.type), setup.fromWorship ? ", from worship" : ""));
}

entt::entity Runner::CastMiracle(size_t index)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& setup = _scenario->miracles.at(index);
	const auto& info = Locator::infoConstants::value();
	const auto& land = Locator::terrainSystem::value();
	auto& magic = Locator::magicSystem::value();
	// What a seed of it in the hand would cast: a sizing miracle at its usual size, the fire seed at 1, its effect's
	// prayer power and the player's timer, or none for a miracle without one
	constexpr float k_FireSeedMagnitude = 1.0f;
	constexpr float k_NoTimeLimit = -1.0f;
	const auto seed = magic::GetFirstSpellSeedForMagicType(info, setup.type);
	float size = 1.0f;
	if (const auto* radius = magic::GetMagicInfoAs<GMagicRadiusSpellInfo>(info, setup.type))
	{
		size = radius->radiusForNormalCost;
	}
	const float timer = magic::GetTimerWhenPlayerCasting(info, setup.type);
	const magic::SpellCastData cast {
	    .magnitude = seed == SpellSeedType::Fire ? k_FireSeedMagnitude : size,
	    .chants = magic::GetMagicEffectInfo(info, setup.type).initialChants,
	    .duration = timer < 0.0f ? k_NoTimeLimit : timer,
	    .maxObjectsToCreate = -1,
	};
	std::optional<entt::entity> target;
	const auto point2 = MapPoint(_middle, setup.point);
	glm::vec3 point {point2.x, land.GetHeightAt(point2), point2.y};
	if (setup.target == MiracleCast::Target::Creature)
	{
		target = CreatureAt(setup.creature);
		if (!target.has_value())
		{
			return entt::null;
		}
		point = Locator::entitiesRegistry::value().Get<Transform>(*target).position;
	}
	const auto hand2 = MapPoint(_middle, setup.handOffset);
	const glm::vec3 hand {hand2.x, land.GetHeightAt(hand2) + setup.handHeight, hand2.y};
	const auto toward = point - hand;
	const particles::ProcessInfo process {
	    .handPos = hand,
	    .cameraForward = glm::length(toward) > 0.0f ? glm::normalize(toward) : glm::vec3(0.0f, 0.0f, 1.0f),
	    .direction = setup.throwVelocity,
	};
	return target.has_value() ? magic.CastOnObject(setup.type, setup.player, *target, cast, process)
	                          : magic.CastAtPoint(setup.type, setup.player, point, cast, process);
}

void Runner::LogMiracles()
{
	if (!_scenario->logMiraclesEvery.has_value() || _seconds < _nextMiracleLog || !Locator::terrainSystem::has_value())
	{
		return;
	}
	_nextMiracleLog = _seconds + *_scenario->logMiraclesEvery;
	const auto& land = Locator::terrainSystem::value();
	Locator::entitiesRegistry::value().Each<const ecs::components::Spell>(
	    [&](entt::entity, const ecs::components::Spell& spell) {
		    const glm::vec2 xz(spell.position.x, spell.position.z);
		    Log(fmt::format("{:.1f} s: miracle {} at ({:.2f}, {:.2f}), land {:.2f}", _seconds,
		                    static_cast<int>(spell.magicType), xz.x - _middle.x, xz.y - _middle.y, land.GetHeightAt(xz)));
	    });
}

void Runner::LogBuildings()
{
	if (!_scenario->logBuildingsEvery.has_value() || _seconds < _nextBuildingLog)
	{
		return;
	}
	_nextBuildingLog = _seconds + *_scenario->logBuildingsEvery;
	const auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const ecs::components::Abode>([&](entt::entity abode, const ecs::components::Abode& building) {
		const auto* site = registry.Valid(building.buildingSite)
		                       ? registry.TryGet<const ecs::components::BuildingSite>(building.buildingSite)
		                       : nullptr;
		Log(fmt::format("{:.1f} s: building {} life {:.3f}, drawn {:.3f} repaired{}", _seconds, static_cast<int>(abode),
		                ecs::life::LifeOf(abode), ecs::abodes::GetPercentForDrawBuilding(abode),
		                site == nullptr ? std::string()
		                                : fmt::format(", {} site, {} builders", site->isRepairSite ? "repair" : "building",
		                                              site->builders.size())));
	});
}

void Runner::UpdateThrows()
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::entitiesRegistry::has_value() ||
	    !Locator::terrainSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& dynamics = Locator::dynamicsSystem::value();
	for (size_t i = 0; i < _throwsDue.size() && i < _scenario->throws.size(); ++i)
	{
		auto& due = _throwsDue.at(i);
		if (!due.has_value() || _seconds < *due)
		{
			continue;
		}
		const auto& setup = _scenario->throws.at(i);
		const auto object = setup.object < _objects.size() ? _objects.at(setup.object) : entt::null;
		// What broke or went is thrown no more
		if (!registry.Valid(object) || !registry.AllOf<Transform>(object))
		{
			due.reset();
			continue;
		}
		// Still flying from the last throw: it is thrown once it lands
		if (dynamics.IsFlying(object))
		{
			continue;
		}
		auto& transform = registry.Get<Transform>(object);
		const auto point =
		    setup.from.has_value() ? MapPoint(_middle, *setup.from) : glm::vec2(transform.position.x, transform.position.z);
		transform.position = {point.x, Locator::terrainSystem::value().GetHeightAt(point) + setup.height, point.y};
		registry.SetDirty();
		// Let go as the player's hand lets go of a thing, with no spin
		if (!ecs::physics::from_hand::InitialisePhysicsFromHand(object, setup.velocity, false,
		                                                        openblack::creature::LocalPlayer())
		         .has_value())
		{
			Log(fmt::format("{:.1f}s: object {} wasn't thrown", _seconds, setup.object));
			// a thing a script made immovable gets no body: it hangs where it was let go, as from the hand
			if (ecs::physics::from_hand::HangsWhereLetGo(object))
			{
				ecs::physics::from_hand::LetGoWithoutBody(object);
			}
		}
		due = setup.repeatSeconds.has_value() ? std::optional(*due + *setup.repeatSeconds) : std::nullopt;
	}
}

std::string Runner::GivePointerCommand(const Command& command)
{
	if (!Locator::inputState::has_value() || !Locator::windowing::has_value())
	{
		return "no mouse";
	}
	const auto size = WindowSize();
	if (!_pointer.IsTaken())
	{
		// The pointer holds still where the cursor is from now on, until the scenario moves it
		_pointer.Take();
		_pointer.MoveTo(_pointer.GetPosition(), size);
	}
	KeepDebugWindowsOffTheMouse(true);
	// The camera's drags and the wheel come as the mouse's own events, which are dropped in the test runs
	const std::string_view dropped = MouseEventsDropped() ? " (the mouse's events are dropped here)" : "";
	switch (command.kind)
	{
	case Kind::PointerTo:
	{
		const auto to = testbed_pointer::PixelAtShare(command.point, size);
		const auto moved = to - _pointer.GetPosition();
		_pointer.MoveTo(to, size);
		PushMotion(_host.GetInterface(), to, _pointer.ButtonMask(), moved);
		break;
	}
	case Kind::PointerPress:
	case Kind::PointerRelease:
		// The open tattoo editor's dialog takes the button, as the game's takes the mouse, and the hand doesn't see it;
		// otherwise the hand is given the buttons at the end of this frame's update
		if (!ToTattooEditor(_host.GetInterface(), _pointer.GetPosition(), command.value, command.kind == Kind::PointerPress))
		{
			_pointer.Press(testbed_pointer::ButtonOf(command.value), command.kind == Kind::PointerPress);
			// Inside the temple the rooms read the game's actions, which the window's button events make
			if (Locator::temple::has_value() && Locator::temple::value().Active() && Locator::gameActionSystem::has_value() &&
			    Locator::windowing::has_value())
			{
				const auto position = _pointer.GetPosition();
				Locator::gameActionSystem::value().ProcessEvent(ButtonEvent(command.kind == Kind::PointerPress, command.value,
				                                                            position.x, position.y,
				                                                            Locator::windowing::value().GetID()));
			}
		}
		_handWatchSeconds = k_HandWatchSeconds;
		break;
	case Kind::PointerDoubleClick:
		// the hand takes the double click in its update this frame, with the buttons given to it, as the game hands it
		// the mouse's own
		if (_pointer.DoubleClickDown(Locator::temple::has_value() && Locator::temple::value().Active()) &&
		    Locator::handSystem::has_value())
		{
			Locator::handSystem::value().ActionDoubleClick();
		}
		_handWatchSeconds = k_HandWatchSeconds;
		break;
	case Kind::PointerSweep:
		_sweep = testbed_pointer::StartSweep(command.point, command.amount, size);
		return HandOnScreen() + std::string(dropped);
	case Kind::WheelTurn:
	{
		SDL_Event event {};
		event.type = SDL_MOUSEWHEEL;
		event.wheel.windowID = Locator::windowing::value().GetID();
		event.wheel.y = static_cast<int32_t>(command.value) * (command.ctrl ? -1 : 1);
		event.wheel.preciseY = static_cast<float>(event.wheel.y);
		SDL_PushEvent(&event);
		return HandOnScreen() + std::string(dropped);
	}
	default:
		break;
	}
	return HandOnScreen();
}

void Runner::HoldHand()
{
	// The pointer's own moves, a sweep or a gesture being drawn, take it while they last
	if (!_handHoldPoint.has_value() || _sweep.has_value() || _drawing.has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto size = WindowSize();
	glm::vec3 screen {0.0f};
	if (Locator::camera::value().ProjectWorldToScreen(*_handHoldPoint, {0.0f, 0.0f, size.x, size.y}, screen))
	{
		_pointer.MoveTo(glm::ivec2(glm::round(glm::vec2(screen))), size);
	}
}

void Runner::UpdatePointer(float seconds)
{
	if (!_pointer.IsTaken())
	{
		return;
	}
	// Once the commands are done, the buttons let go and the gesture drawn, the mouse is the player's again
	if (!_holdPointer && !_pointer.AnyHeld() && !_sweep.has_value() && !_drawing.has_value() && !_fireBallTake.has_value() &&
	    !_doubleClick.has_value() && _timeline.done && _handWatchSeconds <= 0.0f)
	{
		ReleasePointer();
		return;
	}
	if (_handWatchSeconds > 0.0f || _sweep.has_value())
	{
		_handWatchSeconds -= seconds;
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Testbed {:.3f}s: {}", _seconds, HandOnScreen());
	}
	if (_sweep.has_value())
	{
		const auto size = WindowSize();
		const auto step = testbed_pointer::Advance(*_sweep, seconds);
		auto position = _pointer.GetPosition();
		// While the camera turns with the mouse its pointer is held, and only the movement comes through
		const bool frozen = Locator::gameActionSystem::has_value() && Locator::gameActionSystem::value().IsCursorFrozen();
		if (!frozen)
		{
			position = testbed_pointer::ClampToWindow(position + step.moved, size);
		}
		_pointer.MoveTo(position, size);
		PushMotion(_host.GetInterface(), position, _pointer.ButtonMask(), step.moved);
		if (step.done)
		{
			_sweep.reset();
			const auto line = fmt::format("{:.2f}s: moved: {}", _seconds, HandOnScreen());
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed {}", line);
			Log(line);
		}
	}
}

void Runner::ReleasePointer()
{
	StopDrawing();
	_sweep.reset();
	_handWatchSeconds = 0.0f;
	_holdPointer = false;
	_handHoldPoint.reset();
	if (_pointer.IsTaken() && Locator::inputState::has_value())
	{
		_pointer.Release();
		KeepDebugWindowsOffTheMouse(false);
	}
}

namespace
{
/// A temple room's submesh's name, as the temple loads the room's mesh
std::string SubMeshName(TempleRoom room, uint32_t subMesh)
{
	constexpr std::array<std::string_view, 7> k_RoomMeshes {"challenge", "creature", "credits", "main",
	                                                        "multi",     "options",  "savegame"};
	if (static_cast<size_t>(room) >= k_RoomMeshes.size() || !Locator::resources::has_value())
	{
		return {};
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto name = fmt::format("temple/interior/{}_l3d", k_RoomMeshes.at(static_cast<size_t>(room)));
	const auto id = entt::hashed_string(name.c_str()).value();
	if (!meshes.Contains(id) || subMesh >= meshes.Handle(id)->GetSubMeshes().size())
	{
		return {};
	}
	return meshes.Handle(id)->GetSubMeshes()[subMesh]->GetName();
}
} // namespace

std::string Runner::HandOnScreen() const
{
	if (!Locator::handSystem::has_value() || !Locator::camera::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	const auto cursor = input::GameCursor();
	const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(hand);
	if (transform == nullptr)
	{
		return {};
	}
	const auto& position = transform->position;
	const auto size = glm::vec2(WindowSize());
	const auto& camera = Locator::camera::value();
	glm::vec3 screen {0.0f};
	camera.ProjectWorldToScreen(position, {0.0f, 0.0f, size.x, size.y}, screen);
	const auto cues = camera.GetModel().GetHandCues();
	constexpr std::array<std::string_view, 4> k_DragModes {"pan", "edge rotate", "pitch", "pitch from the top"};
	const auto drag = !cues.dragging              ? std::string_view("none")
	                  : cues.dragMode.has_value() ? k_DragModes.at(static_cast<size_t>(*cues.dragMode))
	                                              : std::string_view("undecided");
	const bool held = Locator::gameActionSystem::has_value() && Locator::gameActionSystem::value().IsCursorFrozen();
	// the village totem the hand holds, and the share it holds it at
	std::string totem;
	std::as_const(Locator::entitiesRegistry::value())
	    .Each<const ecs::components::VillageTotem>([&totem](entt::entity, const ecs::components::VillageTotem& statue) {
		    if (statue.gripped && totem.empty())
		    {
			    totem = fmt::format(", holding the totem at share {:.3f}", statue.held);
		    }
	    });
	// inside the temple, the room and submesh under the cursor
	std::string inTemple;
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		const auto hit = Locator::temple::value().GetCursorHit();
		inTemple = !hit.has_value()           ? ", temple: nothing"
		           : hit->subMesh.has_value() ? fmt::format(", temple: room {} submesh {} {}", static_cast<int>(hit->room),
		                                                    *hit->subMesh, SubMeshName(hit->room, *hit->subMesh))
		                                      : ", temple: the floor";
		if (const auto* model = dynamic_cast<const TempleCameraModel*>(&camera.GetModel()); model != nullptr)
		{
			inTemple +=
			    fmt::format(", looking at a submesh {}, zoom {:.3f}", model->IsLookingAtSubMesh(), model->GetSubMeshZoom());
		}
	}
	return fmt::format("cursor ({}, {}), hand ({:.0f}, {:.0f}) at ({:.1f}, {:.1f}, {:.1f}), {:.1f} from the camera, scale "
	                   "{:.3f}, hints {:#x}, drag {}, camera heading {:.3f} pitch {:.3f}{}{}{}",
	                   cursor.x, cursor.y, screen.x, screen.y, position.x, position.y, position.z,
	                   glm::distance(position, camera.GetOrigin()), transform->scale.y, cues.tricons, drag,
	                   camera.GetRotation().y, camera.GetRotation().x, held ? ", held" : "", totem, inTemple);
}

void Runner::Log(std::string line)
{
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Testbed: {}", line);
	_log.push_back(std::move(line));
	while (_log.size() > k_LogLines)
	{
		_log.pop_front();
	}
}

std::optional<CrowdProgress> Runner::GetCrowdProgress() const
{
	if (_scenario == nullptr || !_scenario->crowd.has_value())
	{
		return std::nullopt;
	}
	return _crowdProgress;
}

std::span<const benchmark::StageInfo> Runner::GetStages() const
{
	if (_recorder == nullptr)
	{
		return {};
	}
	return _recorder->Stages();
}

uint32_t Runner::GetWarmUpLeft() const
{
	return _settledFrames >= _benchmark.warmUpFrames ? 0 : _benchmark.warmUpFrames - _settledFrames;
}

void Runner::SpawnCrowd()
{
	if (_scenario == nullptr || !_scenario->crowd.has_value() || _crowdProgress.Done() ||
	    !Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto start = std::chrono::steady_clock::now();
	const auto batch = std::min(_scenario->crowd->perFrame, _crowdProgress.total - _crowdNext);
	for (size_t i = 0; i < batch; ++i)
	{
		SpawnCrowdMember(_crowdNext++);
	}
	_crowdProgress.spawned = _crowdNext;
	_crowdProgress.spawnMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	++_crowdProgress.spawnFrames;
	if (_crowdProgress.Done())
	{
		Log(fmt::format("Spawned {} in {:.0f} ms over {} frames", _crowdProgress.total, _crowdProgress.spawnMs,
		                _crowdProgress.spawnFrames));
		LeadFirstCrowdCreatures();
	}
}

void Runner::SpawnCrowdMember(size_t index)
{
	const auto& land = Locator::terrainSystem::value();
	const auto onLand = [&land, this](glm::vec2 offset) {
		const auto point = MapPoint(_middle, offset);
		return glm::vec3(point.x, land.GetHeightAt(point), point.y);
	};
	if (!_crowdCreatures.empty())
	{
		const auto& member = _crowdCreatures.at(index);
		// a new creature: its body is the one its draws make
		_crowdEntities.push_back(CreatureArchetype::Create(onLand(member.offset), member.owner, member.species, 0,
		                                                   glm::radians(member.facingDegrees), k_DefaultSize));
		return;
	}
	// The towns first, then their homes and stores, then the villagers who live in them
	const auto towns = _village.towns.size();
	const auto abodes = _village.abodes.size();
	if (index < towns)
	{
		const auto& town = _village.towns.at(index);
		_crowdEntities.push_back(
		    ecs::archetypes::TownArchetype::Create(static_cast<int>(index), onLand(town.offset), town.owner, town.tribe));
		return;
	}
	if (index < towns + abodes)
	{
		const auto& abode = _village.abodes.at(index - towns);
		_crowdAbodes.push_back(ecs::archetypes::AbodeArchetype::Create(static_cast<uint32_t>(abode.town), onLand(abode.offset),
		                                                               abode.type, glm::radians(abode.yawDegrees), 1.0f,
		                                                               k_CrowdFood, k_CrowdWood));
		return;
	}
	const auto& member = _village.villagers.at(index - towns - abodes);
	const auto& home = _village.abodes.at(member.abode);
	const auto entity =
	    ecs::archetypes::VillagerArchetype::Create(onLand(home.offset), onLand(member.offset), member.type, member.age);
	_crowdEntities.push_back(entity);
	// It lives in the home laid out for it, rather than the first in its town with room; the home's town becomes its
	// town
	auto& registry = Locator::entitiesRegistry::value();
	const auto abode = _crowdAbodes.at(member.abode);
	if (abode != entt::null && registry.Valid(abode))
	{
		ecs::abode_villagers::AddVillagerToAbode(abode, entity);
	}
}

void Runner::Measure()
{
	if (_recorder == nullptr || !_crowdProgress.Done() || !Locator::profiler::has_value())
	{
		return;
	}
	if (_settledFrames < _benchmark.warmUpFrames)
	{
		++_settledFrames;
		return;
	}
	// The last frame, whole: it ended as this one started
	const auto& profiler = Locator::profiler::value();
	const auto& entry = profiler.GetEntries().at(profiler.GetEntryIndex(-1));
	const auto frame = entry.frameEnd - entry.frameStart;
	const auto& draw = entry.stages.at(static_cast<size_t>(Profiler::Stage::SceneDraw));
	const bool drawn = draw.finalized && draw.start >= entry.frameStart && draw.start <= entry.frameEnd;
	const auto update = drawn ? draw.start - entry.frameStart : frame;
	std::array<float, static_cast<size_t>(Profiler::Stage::_count)> stages {};
	for (size_t i = 0; i < stages.size(); ++i)
	{
		stages.at(i) = static_cast<float>(StageMilliseconds(entry, entry.stages.at(i)));
	}
	// What the renderer last drew, the frame before
	const auto* renderStats = bgfx::getStats();
	_recorder->Add(static_cast<float>(Milliseconds(frame)), static_cast<float>(Milliseconds(update)),
	               static_cast<float>(Milliseconds(frame - update)), stages,
	               renderStats != nullptr ? static_cast<float>(renderStats->numDraw) : 0.0f);
	if (++_framesSinceSummary >= k_FramesPerSummary || _recorder->Count() == _recorder->Capacity())
	{
		_framesSinceSummary = 0;
		_liveResults = _recorder->Summarise();
	}
	if (!_measured && _recorder->Count() >= _recorder->Capacity())
	{
		_measured = true;
		_liveResults = _recorder->Summarise();
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Benchmark {}: frame mean {:.2f} ms p95 {:.2f} max {:.2f}", _scenario->id,
		                   _liveResults.frame.average, _liveResults.frame.p95, _liveResults.frame.max);
		if (_benchmark.resultsBase.has_value())
		{
			const auto written = SaveResults();
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Benchmark {}: results {}", _scenario->id,
			                   written.has_value() ? written->generic_string() : "not written");
		}
		if (_benchmark.quitWhenMeasured)
		{
			_host.RequestQuit();
		}
	}
}

std::vector<std::pair<std::string, size_t>> Runner::EntityCounts() const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return {};
	}
	auto& registry = Locator::entitiesRegistry::value();
	return {
	    {"placed", registry.Size<Transform>()},
	    {"creatures", registry.Size<Creature>()},
	    {"villagers", registry.Size<ecs::components::Villager>()},
	    {"abodes", registry.Size<ecs::components::Abode>()},
	    {"towns", registry.Size<ecs::components::Town>()},
	};
}

// Tool output, not game data: written only when the command line gave --benchmark-out, to that path alone, so a game
// run without the flag never writes a file here
std::optional<std::filesystem::path> Runner::SaveResults()
{
	if (!_benchmark.resultsBase.has_value() || _scenario == nullptr || _recorder == nullptr || _recorder->Count() == 0)
	{
		return std::nullopt;
	}
	benchmark::RunInfo run {
	    .scenarioId = std::string(_scenario->id),
	    .scenarioName = std::string(_scenario->name),
	    .build = std::string(k_Build),
	    .crowd = _crowdProgress.total,
	    .spawnMs = _crowdProgress.spawnMs,
	    .spawnFrames = _crowdProgress.spawnFrames,
	    .warmUpFrames = _benchmark.warmUpFrames,
	    .entityCounts = EntityCounts(),
	};
	if (Locator::windowing::has_value())
	{
		const auto size = Locator::windowing::value().GetSize();
		run.width = static_cast<uint32_t>(size.x);
		run.height = static_cast<uint32_t>(size.y);
	}
	const auto& base = *_benchmark.resultsBase;
	std::error_code error;
	if (base.has_parent_path())
	{
		std::filesystem::create_directories(base.parent_path(), error);
	}
	const auto files = benchmark::ResultPaths(base);
	std::ofstream json(files.json, std::ios::binary);
	std::ofstream csv(files.csv, std::ios::binary);
	if (!json || !csv)
	{
		Log(fmt::format("Couldn't write {}", files.json.generic_string()));
		return std::nullopt;
	}
	const auto results = _recorder->Summarise();
	json << benchmark::ToJson(run, results, _recorder->Stages());
	csv << benchmark::ToCsv(run, results, _recorder->Stages());
	Log(fmt::format("Saved {}", files.json.generic_string()));
	return files.json;
}
