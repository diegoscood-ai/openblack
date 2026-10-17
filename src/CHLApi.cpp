/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLApi.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/CameraEdits.h"
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Audio.h"
#include "Audio/Engine/SamplePlay.h"
#include "Audio/GameMusic.h"
#include "Audio/Services/Confirmation.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Audio/Services/ScriptSound.h"
#include "Camera/Camera.h"
#include "Camera/CameraShake.h"
#include "Camera/FieldOfView.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScriptCamera.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/LeashScript.h"
#include "Debug/TraceLog.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BallArchetype.h"
#include "ECS/Archetypes/BonfireArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/ScriptMarkerArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandClicked.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/InfluenceRing.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Sky.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CreatureRemoval.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Events/DrawListEvents.h"
#include "ECS/Events/LandEvents.h"
#include "ECS/Events/Publish.h"
#include "ECS/FeatureBuild.h"
#include "ECS/Flocks.h"
#include "ECS/Life.h"
#include "ECS/LivingAngle.h"
#include "ECS/LivingWalkPath.h"
#include "ECS/MapCells.h"
#include "ECS/MissionaryBoat.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/PlayerCreature.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHeld.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/ScriptPopulate.h"
#include "ECS/ScriptSetPosition.h"
#include "ECS/ScriptTimer.h"
#include "ECS/ScriptTypes.h"
#include "ECS/SeaCells.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "ECS/Systems/VortexSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownBelief.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownAggression.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/VillagerScriptRules.h"
#include "ECS/VillagerSpeed.h"
#include "EngineConfig.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Graphics/Moon.h"
#include "Hand/HandClickRules.h"
#include "Help/HelpProfile.h"
#include "Help/HelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Help/ScriptControl.h"
#include "Help/ScriptSpirits.h"
#include "Help/SpiritsRuntime.h"
#include "InfoConstants.h"
#include "Input/HandDemo.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/Script/CHLFire.h"
#include "Magic/Script/CHLInfluence.h"
#include "Magic/Script/CHLSpells.h"
#include "Magic/Script/CHLWeather.h"
#include "Magic/Script/CHLWorship.h"
#include "Magic/Script/ScriptPlayer.h"
#include "Magic/Spells/SpellCreature.h"
#include "Particles/PSysManager.h"
#include "ScriptHeaders/ScriptChallengeSnapshots.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "ScriptHeaders/ScriptNameLists.h"
#include "ScriptHeaders/ScriptPropertyRules.h"
#include "ScriptHeaders/ScriptRandom.h"
#include "Video/FallingSpellVideo.h"
#include "Video/VideoPlayer.h"
#include "Worship/Citadel.h"
#include "Worship/LeashPosts.h"
#include "Worship/SpellDispenser.h"
#include "Worship/TempleLeash.h"

namespace openblack::chlapi
{

using namespace openblack::ecs::archetypes;

using openblack::Locator;
using openblack::MobileStaticInfo;
using openblack::ecs::components::Transform;
using openblack::ecs::systems::HandSystemInterface;
using openblack::lhvm::DataType;
using openblack::lhvm::VMValue;
using openblack::script::ObjectType;

#define CREATE_FUNCTION_BINDING(NAME, STACKIN, STACKOUT, FUNCTION)       \
	{                                                                    \
		_functionsTable.emplace_back(FUNCTION, STACKIN, STACKOUT, NAME); \
	}

const std::vector<lhvm::NativeFunction>& CHLApi::GetFunctionsTable()
{
	return _functionsTable;
}

/// OPENBLACK_SCRIPT_THING_TRACE=1 (openblack only): MOVE_GAME_THING, SET_SCRIPT_STATE, SET_SCRIPT_ULONG and a PLAYED
/// that is true write a line, to follow a script that drives its things (Land 1's FollowUs)
bool ScriptThingTrace()
{
	static const bool on = [] {
		const char* env = std::getenv("OPENBLACK_SCRIPT_THING_TRACE");
		return env != nullptr && std::string_view(env) != "" && std::string_view(env) != "0";
	}();
	return debug_env::TraceLogs(on, "scripting");
}

void CHLApi::NotImplemented(std::optional<int32_t> detail)
{
	if (!_stubCalls.Record(_currentNative, detail))
	{
		return;
	}
	const auto name = _currentNative < _functionsTable.size() ? _functionsTable[_currentNative].name : "?";
	if (detail.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script native {} ({}) not implemented for {}; later calls are counted",
		                    name, _currentNative, *detail);
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Script native {} ({}) not implemented; later calls are counted", name,
		                    _currentNative);
	}
}

void CHLApi::LogStubCalls() const
{
	const auto entries = _stubCalls.Entries();
	if (entries.empty())
	{
		return;
	}
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "Unimplemented script natives called: {} calls of {} natives",
	                   _stubCalls.Total(), entries.size());
	for (const auto& entry : entries)
	{
		const auto name = entry.native < _functionsTable.size() ? _functionsTable[entry.native].name : "?";
		if (entry.detail.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "  {} calls of {} ({}) for {}", entry.calls, name, entry.native,
			                   *entry.detail);
		}
		else
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "  {} calls of {} ({})", entry.calls, name, entry.native);
		}
	}
}

namespace
{

/// The native being run isn't written yet, or not for this case of it. The scripts call some unwritten natives every
/// frame (GAME_THING_CLICKED: 37k lines), so each is logged on its first call only
void NotImplemented(std::optional<int32_t> detail = std::nullopt)
{
	Locator::chlapi::value().NotImplemented(detail);
}

/// The objects the scripts hold (Locator::scriptObjects)
ecs::systems::ScriptObjectsSystemInterface& ScriptObjects()
{
	if (!Locator::scriptObjects::has_value())
	{
		std::fputs("chlapi: no script objects service in the locator (Locator::scriptObjects)\n", stderr);
		std::abort();
	}
	return Locator::scriptObjects::value();
}

/// The physics, which keeps the scripts' hit record (Locator::dynamicsSystem)
ecs::systems::DynamicsSystemInterface& Dynamics()
{
	if (!Locator::dynamicsSystem::has_value())
	{
		std::fputs("chlapi: no physics in the locator (Locator::dynamicsSystem)\n", stderr);
		std::abort();
	}
	return Locator::dynamicsSystem::value();
}

} // namespace

/// The script VM as the game's script control asks it (task number, current task's script type, a task's script type,
/// stop tasks of a type)
help::script_control::Vm ScriptVm()
{
	help::script_control::Vm vm;
	vm.taskNumber = []() { return Locator::vm::value().GetCurrentTaskNumber(); };
	vm.currentTaskType = []() { return static_cast<uint32_t>(Locator::vm::value().GetCurrentTaskScriptType()); };
	vm.taskType = [](uint32_t task) { return static_cast<uint32_t>(Locator::vm::value().GetTaskScriptType(task)); };
	vm.stopTasksOfType = [](uint32_t mask) { Locator::vm::value().StopTasksOfType(static_cast<lhvm::ScriptType>(mask)); };
	vm.pushFloat = [](float value) { Locator::vm::value().Pushf(value); };
	vm.startScript = [](std::string_view name, uint32_t mask) {
		Locator::vm::value().StartScript(std::string(name), static_cast<lhvm::ScriptType>(mask));
	};
	return vm;
}

bool CHLApi::StartHelpScript(std::string_view name)
{
	if (!Locator::dialogueControlSystem::has_value())
	{
		std::fputs("chlapi: no dialogue control in the locator (Locator::dialogueControlSystem)\n", stderr);
		std::abort();
	}
	return help::Get() != nullptr &&
	       help::script_control::StartHelpScript(Locator::dialogueControlSystem::value(), name, ScriptVm());
}

/// The check of the script camera opcodes: no camera mode -> "Script camera has been removed!"; a mode other than the
/// script camera -> "We are in the wrong camera mode!" (SET_CAMERA_POSITION says nothing). Either way the opcode does
/// nothing. openblack always has the player's mode, so only the second can happen. The "Script moving camera in
/// citadel" note of 003/004/287 is not ported: the citadel never has a script mode
bool ScriptCameraMode(const char* opcode)
{
	// A dual camera on top of the script mode (START_DUAL_CAMERA) is the current mode: the opcode does nothing
	if (script_camera::ScriptModeCurrent())
	{
		return true;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "{}: We are in the wrong camera mode!", opcode);
	return false;
}

namespace
{
constexpr std::string_view k_ObjectNoLongerValid = "Object no longer valid";

/// The original's "<opcode>: Object no longer valid" (`message`, its wording for the opcode): an error the first time the
/// opcode logs it on this land, a debug line after that, so that a script that keeps asking about a thing gone does not
/// fill the log
void ObjectNoLongerValid(std::string_view opcode, std::string_view message = k_ObjectNoLongerValid)
{
	if (Locator::entitiesRegistry::value().Context().objectNoLongerValid.First(opcode))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: {}", opcode, message);
	}
	else
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "{}: {}", opcode, message);
	}
}

/// The vortices between the lands (Locator::vortexSystem); stops with a message when there are none
ecs::systems::VortexSystemInterface& TheVortexSystem()
{
	if (!Locator::vortexSystem::has_value())
	{
		std::fputs("scripts: no vortex service in the locator (Locator::vortexSystem)\n", stderr);
		std::abort();
	}
	return Locator::vortexSystem::value();
}

/// The answer to the start-of-game question (Locator::tutorialSkipSystem); stops with a message when there is none
const ecs::systems::TutorialSkipSystemInterface& TheTutorialSkipSystem()
{
	if (!Locator::tutorialSkipSystem::has_value())
	{
		std::fputs("scripts: no start-of-game answer in the locator (Locator::tutorialSkipSystem)\n", stderr);
		std::abort();
	}
	return Locator::tutorialSkipSystem::value();
}

/// The player profiles (Locator::playerProfileSystem); stops with a message when there are none
const ecs::systems::PlayerProfileSystemInterface& ThePlayerProfileSystem()
{
	if (!Locator::playerProfileSystem::has_value())
	{
		std::fputs("scripts: no player profiles in the locator (Locator::playerProfileSystem)\n", stderr);
		std::abort();
	}
	return Locator::playerProfileSystem::value();
}

/// The sky, whose moon keeps how it last showed (Locator::skySystem); stops with a message when there is none
const ecs::systems::SkySystemInterface& TheSkySystem()
{
	if (!Locator::skySystem::has_value())
	{
		std::fputs("scripts: no sky in the locator (Locator::skySystem)\n", stderr);
		std::abort();
	}
	return Locator::skySystem::value();
}

/// The help texts' spoken lines (Locator::helpSpeechSystem); stops with a message when there are none
ecs::systems::HelpSpeechSystemInterface& TheHelpSpeechSystem()
{
	if (!Locator::helpSpeechSystem::has_value())
	{
		std::fputs("scripts: no help speech in the locator (Locator::helpSpeechSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpSpeechSystem::value();
}

/// The help texts (Locator::helpTextSystem); stops with a message when there are none
ecs::systems::HelpTextSystemInterface& TheHelpTextSystem()
{
	if (!Locator::helpTextSystem::has_value())
	{
		std::fputs("scripts: no help texts in the locator (Locator::helpTextSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpTextSystem::value();
}

/// The opening's specials (Locator::introSystem); stops with a message when there are none
ecs::systems::IntroSystemInterface& TheIntroSystem()
{
	if (!Locator::introSystem::has_value())
	{
		std::fputs("scripts: no opening specials in the locator (Locator::introSystem)\n", stderr);
		std::abort();
	}
	return Locator::introSystem::value();
}

/// The villagers drawn in high detail (Locator::highDetailSystem); stops with a message when there are none
ecs::systems::HighDetailSystemInterface& TheHighDetailSystem()
{
	if (!Locator::highDetailSystem::has_value())
	{
		std::fputs("scripts: no high-detail villagers in the locator (Locator::highDetailSystem)\n", stderr);
		std::abort();
	}
	return Locator::highDetailSystem::value();
}

/// What the player at this computer last clicked, kept on their hand; none without a hand
ecs::components::HandClicked* LocalHandClicked()
{
	if (!Locator::handSystem::has_value())
	{
		return nullptr;
	}
	const auto hand = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& registry = Locator::entitiesRegistry::value();
	if (hand == entt::null || !registry.Valid(hand))
	{
		return nullptr;
	}
	return registry.TryGet<ecs::components::HandClicked>(hand);
}
} // namespace

/// The script's thing for an object id: the thing, or nullopt with the opcode's own message (the callers print
/// different strings: "Thing no longer valid", "Thing not found!", "Object no longer valid").
/// (approximate) As MusicThing: 0 is null and a valid entity stands for a live thing (the original looks the id up in
/// its script table)
std::optional<entt::entity> ScriptThing(uint32_t object, const char* opcode, const char* message)
{
	const auto entity = static_cast<entt::entity>(object);
	if (object != 0 && Locator::entitiesRegistry::value().Valid(entity))
	{
		return entity;
	}
	if (message == k_ObjectNoLongerValid)
	{
		ObjectNoLongerValid(opcode);
	}
	else
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: {}", opcode, message);
	}
	return std::nullopt;
}

/// ScriptThing for the camera opcodes: "Thing no longer valid"
std::optional<entt::entity> CameraThing(uint32_t object, const char* opcode)
{
	return ScriptThing(object, opcode, "Thing no longer valid");
}

/// The thing a leash opcode names: none for 0 or a thing no longer there. The original's messages for it are empty in
/// the shipped game, so nothing is logged
std::optional<entt::entity> LeashThing(uint32_t object)
{
	const auto entity = static_cast<entt::entity>(object);
	if (object != 0 && Locator::entitiesRegistry::value().Valid(entity))
	{
		return entity;
	}
	return std::nullopt;
}

/// Whether a thing is a creature, looked up without making any storage
bool IsLeashCreature(entt::entity entity)
{
	return Locator::entitiesRegistry::value().AllOf<ecs::components::Creature>(entity);
}

/// The creature a leash opcode names: none when the thing is missing or not a creature
std::optional<entt::entity> LeashCreature(uint32_t object)
{
	const auto thing = LeashThing(object);
	return thing.has_value() && IsLeashCreature(*thing) ? thing : std::nullopt;
}

std::unordered_set<std::string> GetUniqueWords(const std::string& strings)
{
	std::unordered_set<std::string> result;
	std::istringstream iss(strings);
	std::string word;
	while (std::getline(iss, word, ' '))
	{
		result.insert(word);
	}
	return result;
}

glm::vec3 PopVec()
{
	auto& lhvm = Locator::vm::value();
	const auto z = lhvm.Popf();
	const auto y = lhvm.Popf();
	const auto x = lhvm.Popf();
	return {x, y, z};
}

void PushVec(const glm::vec3& vec)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushv(vec.x);
	lhvm.Pushv(vec.y);
	lhvm.Pushv(vec.z);
}

std::string PopString()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.GetString(lhvm.Pop().intVal);
}

std::vector<float> PopVarArg(const int32_t argc)
{
	std::vector<float> vals;
	vals.resize(argc);
	auto& lhvm = Locator::vm::value();
	for (int i = argc - 1; i >= 0; i--)
	{
		vals[i] = lhvm.Popf();
	}
	return vals;
}

/// The ground position of a script vector: the map coordinates keep y relative to the land, and after the creation
/// the physics editor step resets it to 0, so everything but a marker stands on the land (the CHL vectors come with
/// y = 0)
glm::vec3 OnGround(glm::vec3 position)
{
	if (Locator::terrainSystem::has_value())
	{
		position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	return position;
}

/// The script's create switch, from CREATE 027 (angle 0, scale 1) and CREATE_WITH_ANGLE_AND_SCALE 252. The common step
/// after it rebuilds the 3D object's matrix at the altitude of pos + relY with only its own Y angle and scale (no X/Z
/// tilt), which the archetypes below already give.
/// @return entt::null when nothing is created (the script then gets 0)
entt::entity CreateScriptObject(const ObjectType type, uint32_t subtype, const glm::vec3& given, float yAngleRadians,
                                float scale)
{
	const auto& info = Locator::infoConstants::value();
	// sub_type 5000 is only for the types that ignore it
	if (subtype == 5000 && type != ObjectType::Timer && type != ObjectType::SpellDispenser && type != ObjectType::Whale &&
	    type != ObjectType::Ark && type != ObjectType::Marker && type != ObjectType::Ball && type != ObjectType::Poo &&
	    type != ObjectType::Scaffold)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHL CREATE: trying to create an invalid sub_type");
		return entt::null;
	}
	// The vector becomes the map's coordinates once, before the types: x and z on the map's fixed point, as every type
	// with a place takes them; the land under that point
	const glm::vec3 position(map_coords::Quantise(given.x), given.y, map_coords::Quantise(given.z));
	const auto ground = OnGround(position);
	switch (type)
	{
	case ObjectType::Marker: // a script marker at the vector as given
		return ScriptMarkerArchetype::Create(position);
	case ObjectType::Feature: // (pos, feature info, angle, scale)
		if (subtype < info.feature.size())
		{
			return FeatureArchetype::Create(ground, static_cast<FeatureInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Villager:      // (pos, info, info.grownUpAge + 1): an adult
	case ObjectType::VillagerChild: // (pos, info, 10): a child
		if (subtype < info.villager.size())
		{
			// made at its age, given the same age again, then held by the script (IN_SCRIPT)
			return ecs::villager_script_rules::CreateScriptVillager(
			    type == ObjectType::VillagerChild, info.villager.at(subtype).grownUpAge,
			    [&ground, subtype](uint32_t age) {
				    return VillagerArchetype::Create(ground, ground, static_cast<VillagerInfo>(subtype), age, false);
			    },
			    [](entt::entity villager, uint32_t age) { Locator::livingActionSystem::value().VillagerSetAge(villager, age); },
			    [](entt::entity villager, VillagerStates state) {
				    ecs::script_held::SetLivingScriptState(villager, static_cast<uint32_t>(state));
			    });
		}
		break;
	case ObjectType::Animal: // (pos, animal info, 0, 0), birds too
	case ObjectType::Bird:
		if (subtype < info.animal.size())
		{
			return AnimalArchetype::Create(ground, static_cast<AnimalInfo>(subtype), 0, 0);
		}
		break;
	case ObjectType::MobileStatic:
	case ObjectType::Rock:
		if (subtype >= info.mobileStatic.size())
		{
			break;
		}
		if (subtype == static_cast<uint32_t>(MobileStaticInfo::SingingStoneBase))
		{
			// a base only, without angle or scale
			return MobileStaticArchetype::Create(ground, MobileStaticInfo::SingingStoneBase, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		if (subtype == static_cast<uint32_t>(MobileStaticInfo::StreetLantern) || subtype == 59)
		{
			// a street lantern, the same as the map script's CREATE_STREET_LANTERN (59 is a country lantern)
			return StreetLanternArchetype::Create(ground, static_cast<MobileStaticInfo>(subtype));
		}
		// (pos, info, 0, 0, angle, scale): a rock, a mobile static, or for info 8 a bonfire
		return MobileStaticArchetype::CreateFromInfo(ground, static_cast<MobileStaticInfo>(subtype), 0.0f, yAngleRadians,
		                                             scale);
	case ObjectType::MobileObject: // (pos, mobile object info, 0, angle, scale)
		if (subtype < info.mobileObject.size())
		{
			return MobileObjectArchetype::Create(ground, static_cast<MobileObjectInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Poo: // mobile object info 5
		return MobileObjectArchetype::Create(ground, MobileObjectInfo::LumpOfPoo, yAngleRadians, scale);
	case ObjectType::Ark: // mobile object info 23
		return MobileObjectArchetype::Create(ground, MobileObjectInfo::Ark, yAngleRadians, scale);
	case ObjectType::Tree: // (pos, info, no forest, scale, angle, scale)
		if (subtype < info.tree.size())
		{
			return TreeArchetype::Create(0, ground, static_cast<TreeInfo>(subtype), true, yAngleRadians, scale, scale);
		}
		break;
	case ObjectType::AnimatedStatic: // (pos, animated static info, angle, scale)
		if (subtype < info.animatedStatic.size())
		{
			return AnimatedStaticArchetype::Create(ground, static_cast<AnimatedStaticInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Whale: // mobile object info 24, the shark
		return SharkArchetype::Create(ground, yAngleRadians, scale);
	case ObjectType::PuzzleGame: // (pos, sub_type, angle in 2048ths of a turn, scale)
		return openblack::ecs::CreatePuzzleGame(position, static_cast<script::PuzzleGameType>(subtype), yAngleRadians, scale);
	case ObjectType::Abode: // "Invalid create type" in the original too
	case ObjectType::Town:
	case ObjectType::Dance:
	case ObjectType::Flock:
	case ObjectType::InfluenceRing:
	case ObjectType::Citadel:
	case ObjectType::WorshipSite:
	case ObjectType::SpellSeed:
	case ObjectType::Mist:
	case ObjectType::Field:
	case ObjectType::ComputerPlayer:
	case ObjectType::TotemStatue:
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHL CREATE: invalid create type {}", static_cast<int>(type));
		return entt::null;
	case ObjectType::WeatherThing:
		return magic::script::CreateWeatherThing(subtype, position); // Magic/Script/CHLWeather.cpp
	// the miracle cases (Magic/Script/CHLWorship.cpp)
	case ObjectType::OneShotSpell:
		return magic::script::CreateOneShotSpell(subtype, position);
	case ObjectType::OneShotSpellInHand:
		return magic::script::CreateOneShotSpellInHand(subtype);
	case ObjectType::SpellDispenser:
		return magic::script::CreateSpellDispenser(subtype, position, yAngleRadians, scale);
	case ObjectType::Scaffold: // (pos, scaffold info, angle, scale)
		// no town, no owner (inferred); the sub-type is not read (5000 is accepted above). Its value comes from the scale
		// (1.0 -> 3)
		return ecs::scaffolds::Create(map_coords::FromWorld(ground), entt::null, entt::null, yAngleRadians, scale);
	case ObjectType::Timer: // the sub-type is the seconds
		return ecs::script_timer::Create(static_cast<float>(subtype));
	case ObjectType::Vortex: // (pos, sub_type, 50.0): In / Out / Volcano
		return TheVortexSystem().Create(position, static_cast<VortexType>(subtype));
	case ObjectType::Ball: // (pos, ball info): the sub-type is not read; the nearest town, however far, takes it
		return BallArchetype::CreateForNearestTown(ground);
	default:
		// TODO: Reward, Creature, DeadTree, Store, Totem, Highlight
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for type {}", static_cast<int>(type));
		return entt::null;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHL CREATE: invalid sub_type {} for type {}", subtype,
	                    static_cast<int>(type));
	return entt::null;
}

VMValue Pop(DataType& type)
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Pop(type);
}

VMValue Pop()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Pop();
}

float Popf()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Popf();
}

void Push(VMValue value, DataType type)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Push(value, type);
}

void Pushf(float value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushf(value);
}

void Pushv(float value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushv(value);
}

void Pushi(int32_t value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushi(value);
}

void Pusho(uint32_t value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pusho(value);
}

void Pushb(bool value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushb(value);
}

CHLApi::CHLApi()
{
	_functionsTable.reserve(464);
	InitFunctionsTable0();
	InitFunctionsTable1();
	InitFunctionsTable2();
	InitFunctionsTable3();
	InitFunctionsTable4();
}

void None() {} // 000 NONE

void SetCameraPosition() // 001 SET_CAMERA_POSITION
{
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetPosition(position);
		// the script camera moved: the draw list is rebuilt on the next drawn frame
		ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
	}
}

void SetCameraFocus() // 002 SET_CAMERA_FOCUS
{
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetFocus(position);
		// the script camera moved: the draw list is rebuilt on the next drawn frame
		ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
	}
}

void MoveCameraPosition() // 003 MOVE_CAMERA_POSITION
{
	// in seconds of the wall clock
	const auto time = Popf();
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::MovePosition(position, time);
	}
}

void MoveCameraFocus() // 004 MOVE_CAMERA_FOCUS
{
	const auto time = Popf();
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::MoveFocus(position, time);
	}
}

void GetCameraPosition() // 005 GET_CAMERA_POSITION
{
	auto& camera = Locator::camera::value();
	const auto position = camera.GetOrigin();
	PushVec(position);
}

void GetCameraFocus() // 006 GET_CAMERA_FOCUS
{
	auto& camera = Locator::camera::value();
	const auto focus = camera.GetFocus();
	PushVec(focus);
}

/// The help spirit of a popped SCRIPT_SPIRIT_TYPE (the local player's alignment: inferred, openblack's local player is
/// PLAYER_ONE; the random draw uses game_random's local stream)
int32_t ScriptHelpSpirit(int32_t scriptSpirit)
{
	const auto discrete = help::script_spirits::DiscreteAlignment(ecs::effects::alignment::Get(PlayerNames::PLAYER_ONE));
	return help::script_spirits::HelpSpiritOf(scriptSpirit, discrete,
	                                          []() { return static_cast<uint32_t>(audio::tags::RandomSample(0, 100)); });
}

/// The advisors' AdvisorSpiritController (Locator::advisorSystem); nullptr before they are loaded (openblack only)
help::spirits::AdvisorSpiritController* SpiritControl()
{
	if (!Locator::advisorSystem::has_value() || !Locator::advisorSystem::value().IsLoaded())
	{
		return nullptr;
	}
	return &Locator::advisorSystem::value().GetController();
}

/// The running task is a help script
bool IsHelpTask()
{
	return static_cast<uint32_t>(Locator::vm::value().GetCurrentTaskScriptType()) ==
	       static_cast<uint32_t>(lhvm::ScriptType::Help);
}

/// "Invalid Y" then "Invalid X" for a value below 0 (NaN too) or above 1; the opcode goes on
void CheckScreenFractions(const char* opcode, float x, float y)
{
	if (!help::script_spirits::IsScreenFraction(y))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Invalid Y", opcode);
	}
	if (!help::script_spirits::IsScreenFraction(x))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Invalid X", opcode);
	}
}

/// ScriptThing for the spirit opcodes: the object, or 0 with "Object no longer valid" (approximate, as MusicThing: 0
/// is null and a valid entity stands for a live thing)
uint32_t SpiritThing(uint32_t object, const char* opcode)
{
	if (object != 0 && Locator::entitiesRegistry::value().Valid(static_cast<entt::entity>(object)))
	{
		return object;
	}
	ObjectNoLongerValid(opcode);
	return 0;
}

void SpiritEject() // 007 SPIRIT_EJECT
{
	// POP the spirit -> AdvisorSpiritController::SpiritEject(t, help script): a help script's spirit appears, any other is
	// ejected
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritEject(spirit, IsHelpTask());
	}
}

void SpiritHome() // 008 SPIRIT_HOME
{
	// POP the spirit -> HelpSystem::SpiritHome(t, help script): a help script's spirit vanishes; any other flies home
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->SpiritHome(spirit, IsHelpTask() ? 1 : 0);
	}
}

void SpiritPointPos() // 009 SPIRIT_POINT_POS
{
	// POP in_world (the raw dword), the position, the spirit -> the spirit points there (eject, then point mode 1 with
	// 8.0 / 5.0)
	const auto inWorld = Pop().intVal != 0;
	const auto position = PopVec();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritPointPosition(spirit, position, inWorld);
	}
}

void SpiritPointGameThing() // 010 SPIRIT_POINT_GAME_THING
{
	// POP in_world, the object, the spirit (converted before the test); no object -> "Object no longer valid"; else the
	// spirit points at it (re-sent every turn)
	const auto inWorld = Pop().intVal != 0;
	const auto target = Pop().uintVal;
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	const auto object = SpiritThing(target, "SPIRIT_POINT_GAME_THING");
	if (auto* control = SpiritControl(); control != nullptr && object != 0)
	{
		control->SpiritPointObject(spirit, object, inWorld);
	}
}

void GameThingFieldOfView() // 011 GAME_THING_FIELD_OF_VIEW
{
	// A multiplayer game pushes 1 without popping (not in openblack). POP the thing (none -> "Object no longer valid",
	// 0); the drawn camera's screen test of its bounding sphere (an object) or of its point (Camera/FieldOfView.h)
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		ObjectNoLongerValid("GAME_THING_FIELD_OF_VIEW");
		Pushb(false);
		return;
	}
	Pushb(field_of_view::ThingInView(entity));
}

void PosFieldOfView() // 012 POS_FIELD_OF_VIEW
{
	// POP the vector; inside the temple 0, else whether the point is on the screen of the drawn camera
	// (Camera/FieldOfView.h)
	const auto position = PopVec();
	Pushb(field_of_view::PosInView(position));
}

// A script string widened as the original's ANSI code page conversion (up to 2047 characters).
// (approximate) byte by byte, the same as code page 1252 except for 0x80..0x9F, and without the length limit
std::u16string WidenScriptString(const std::string& text)
{
	std::u16string wide;
	wide.reserve(text.size());
	for (const char c : text)
	{
		wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(c)));
	}
	return wide;
}

void RunText() // 013 RUN_TEXT
{
	const auto withInteraction = Pop().intVal;
	const auto textID = static_cast<uint32_t>(Pop().intVal);
	const auto singleLine = static_cast<bool>(Pop().intVal);
	// an invalid text is reported by the help system, which shows text 0
	static_cast<void>(TheHelpTextSystem().RunText(singleLine, textID, withInteraction));
}

void TempText() // 014 TEMP_TEXT
{
	const auto withInteraction = Pop().intVal;
	const auto string = PopString();
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* dialogue = TheHelpTextSystem().GetDialogue(); dialogue != nullptr)
	{
		dialogue->TempText(singleLine, WidenScriptString(string), withInteraction);
	}
}

void TextRead() // 015 TEXT_READ
{
	// whether the text was read, pushed as a bool (type 6)
	Pushb(TheHelpTextSystem().IsTextRead());
}

void GameThingClicked() // 016 GAME_THING_CLICKED
{
	// (pending) a multiplayer game: "This is not multiplayer friendly yet!" and true, without the pop: openblack has no
	// multiplayer game
	const auto object = Pop().uintVal;
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(thing))
	{
		ObjectNoLongerValid("GAME_THING_CLICKED");
		Pushb(false);
		return;
	}
	auto* last = LocalHandClicked();
	// the last object tapped with the action button == the thing
	const bool clicked = last != nullptr && hand_click::IsThingClicked(*last, thing);
	// a scroll (not a "did you know") of the challenges 0x38, 0x3B, 0x3C, 0x3D: the click is cleared, the game saved
	// at once (pending: no save rooms), then it is tapped again
	if (clicked && ecs::script_highlight::IsHighlight(thing) && !ecs::script_highlight::IsDidYouKnow(thing) &&
	    ecs::script_highlight::SavesGameWhenClicked(ecs::script_highlight::ScriptIdOf(thing)))
	{
		hand_click::ClearThing(*last);
		hand_click::ClickThing(*last, thing, game_clock::Turn());
	}
	Pushb(clicked); // pushed as a bool (type 6)
}

/// The creature minds, which the game always makes: a native that reaches them without them stops with a clear message
ecs::systems::CreatureMindSystemInterface& CreatureMinds(const char* native)
{
	if (!Locator::creatureMindSystem::has_value())
	{
		std::fprintf(stderr, "%s: no creature minds in the locator (Locator::creatureMindSystem)\n", native);
		std::abort();
	}
	return Locator::creatureMindSystem::value();
}

/// SET_SCRIPT_STATE on a creature: it plays what the script last gave it. The static action 0, which has no start, is
/// not played
void PlayForScript(entt::entity creature)
{
	if (!CreatureMinds("SET_SCRIPT_STATE").PlayForScript(creature))
	{
		NotImplemented();
	}
}

void SetScriptState() // 017 SET_SCRIPT_STATE
{
	// the state (first pop), then the object
	const auto state = Pop().intVal;
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// none -> "Object no longer valid"
	if (object == 0 || !registry.Valid(entity))
	{
		ObjectNoLongerValid("SET_SCRIPT_STATE");
		return;
	}
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_SCRIPT_STATE {} {}", object, state);
	}
	// a script container: the state and each living member's script state, no drowning test; a creature member plays
	// what the script gave it, as a creature does
	if (ecs::script_containers::IsContainer(entity))
	{
		ecs::script_containers::ForEachMember(entity, [&registry, state](entt::entity member) {
			if (registry.AllOf<ecs::components::Creature>(member))
			{
				PlayForScript(member);
			}
			else
			{
				ecs::script_held::SetLivingScriptState(member, static_cast<uint32_t>(state));
			}
			return false;
		});
		return;
	}
	// a living thing that is not drowning -> its script state; else "Object not living for set state"
	if (registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity) && !ecs::IsDrowning(entity))
	{
		ecs::script_held::SetLivingScriptState(entity, static_cast<uint32_t>(state));
		return;
	}
	// a creature, whatever the state, plays what the script gave it (SET_SCRIPT_ULONG)
	if (registry.AllOf<ecs::components::Creature>(entity) && !ecs::IsDrowning(entity))
	{
		PlayForScript(entity);
		return;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCRIPT_STATE: Object not living for set state");
}

void SetScriptStatePos() // 018 SET_SCRIPT_STATE_POS
{
	// const auto position = PopVec();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetScriptFloat() // 019 SET_SCRIPT_FLOAT
{
	// const auto value = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetScriptUlong() // 020 SET_SCRIPT_ULONG
{
	// the times (first pop), the clip, then the object
	const auto loop = static_cast<uint32_t>(Pop().intVal);
	const auto animation = static_cast<uint32_t>(Pop().intVal);
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// none -> "Object no longer valid"
	if (object == 0 || !registry.Valid(entity))
	{
		ObjectNoLongerValid("SET_SCRIPT_ULONG");
		return;
	}
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_SCRIPT_ULONG {} clip {} times {}", object, animation, loop);
	}
	// a script container: the clip and times on each villager member
	if (ecs::script_containers::IsContainer(entity))
	{
		ecs::script_containers::ForEachMember(entity, [&registry, animation, loop](entt::entity member) {
			if (registry.AllOf<ecs::components::Villager>(member))
			{
				Locator::livingActionSystem::value().VillagerSetScriptAnimation(member, static_cast<AnimId>(animation), loop);
			}
			return false;
		});
		return;
	}
	// a villager -> its script clip and times
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		Locator::livingActionSystem::value().VillagerSetScriptAnimation(entity, static_cast<AnimId>(animation), loop);
		return;
	}
	// a creature -> its script clip and times
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		CreatureMinds("SET_SCRIPT_ULONG").SetScriptPlay(entity, animation, loop);
		return;
	}
	// anything else: "setting the state of something neither a creature nor a villager"
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"),
	                    "SET_SCRIPT_ULONG: setting the state of something neither a creature nor a villager");
}

/// The creatures' bodies, which hold the needs among the creature properties; none without the service
ecs::systems::CreaturePhysiologySystemInterface* CreaturePhysiology()
{
	return Locator::creaturePhysiologySystem::has_value() ? &Locator::creaturePhysiologySystem::value() : nullptr;
}

/// A creature property a creature has no value for: one of its needs without a body to hold it
void LogCreatureWithoutBody(const char* native)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: the creature has no body for its needs{}", native,
	                    Locator::creaturePhysiologySystem::has_value() ? "" : " (no creature physiology system)");
}

void GetProperty() // 021 GET_PROPERTY
{
	// the object, then the property
	const auto object = Pop().uintVal;
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	const auto entity = static_cast<entt::entity>(object);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "GET_PROPERTY: Thing no longer valid");
		Pushf(0.0f);
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::Health:
	{
		// a game object's life, 0..1 (ecs::life): a villager's, a creature's, an animal's, a building's or a rock's. A
		// container (a town, a flock, a forest that is not a big forest), a dance, a mist, a spell or an influence ring is
		// not an object: "Object not an object", "Cannot Get Property" and 0. A pile of food or wood (the hand's too) has
		// its amount over the most its kind holds; a simple pot has an object's life
		using script::property_rules::HealthKind;
		const auto* pot = registry.TryGet<ecs::components::Pot>(entity);
		const auto& potInfos = Locator::infoConstants::value().pot;
		const auto kind = script::property_rules::HealthKindOf({
		    .container = registry.AnyOf<ecs::components::Town, ecs::components::Flock>(entity),
		    .forest = registry.AllOf<ecs::components::Forest>(entity),
		    .bigForest = registry.AllOf<ecs::components::BigForest>(entity),
		    .dance = registry.AllOf<ecs::components::Dance>(entity),
		    .mist = registry.AllOf<ecs::components::Mist>(entity),
		    .spell = registry.AllOf<ecs::components::Spell>(entity),
		    .influenceRing = registry.AllOf<ecs::components::InfluenceRing>(entity),
		    .pileOfFoodOrWood = pot != nullptr && potInfos.at(static_cast<size_t>(pot->type)).potType != PotType::Pot,
		});
		if (kind == HealthKind::NotAnObject)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_PROPERTY: Object not an object");
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_PROPERTY: Cannot Get Property {}", static_cast<int>(prop));
		}
		float life = 0.0f;
		if (kind == HealthKind::Object)
		{
			life = ecs::life::LifeOf(entity);
		}
		else if (kind == HealthKind::Pile)
		{
			life = script::property_rules::PileLife(pot->amount, potInfos.at(static_cast<size_t>(pot->type)).maxAmountInPot);
		}
		Pushf(script::property_rules::HealthProperty(kind, life));
		return;
	}
	case script::ObjectPropertyType::Scale:
		// the object's scale: the Transform's uniform scale. (pending) a villager or an animal only here
		if (registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity))
		{
			Pushf(registry.Get<const Transform>(entity).scale.x);
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Speed:
		// ecs::living::SpeedProperty (the physics, the dead living things)
		if (const auto speed = ecs::living::SpeedProperty(entity); speed.has_value())
		{
			Pushf(*speed);
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Age:
		// a living thing: its age as a float; else "Not used on non living objects" and 0
		if (const auto age = ecs::living::AgeProperty(entity); age.has_value())
		{
			Pushf(*age);
			return;
		}
		if (registry.AllOf<ecs::components::Creature>(entity))
		{
			NotImplemented(static_cast<int32_t>(prop)); // TODO: creatures
			Pushf(0.0f);
			return;
		}
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_PROPERTY: Not used on non living objects");
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Flying:
		// whether the object has a physics object (the same flag the original's in-the-air test reads), asleep resting
		// proxies included
		Pushb(openblack::Locator::dynamicsSystem::value().Find(entity) != nullptr);
		return;
	case script::ObjectPropertyType::Drowning: // whether it is drowning
		Pushb(openblack::ecs::IsDrowning(entity));
		return;
	case script::ObjectPropertyType::YPos:
		// the altitude of any thing, as a float. (pending) only the highlights here
		if (ecs::script_highlight::IsHighlight(entity))
		{
			Pushf(ecs::script_highlight::GetYPos(entity));
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::BuiltPercentage: // a building's built percentage, else 1
		if (const auto percent = openblack::ecs::abodes::GetBuiltPercentage(entity); percent.has_value())
		{
			Pushf(*percent);
			return;
		}
		// a totem statue: its own, as it keeps it
		if (Locator::villageTotemSystem::has_value())
		{
			if (const auto percent = Locator::villageTotemSystem::value().GetBuiltPercentage(entity); percent.has_value())
			{
				Pushf(*percent);
				return;
			}
		}
		if (const auto percent = openblack::ecs::feature_build::GetBuiltPercentage(entity); percent.has_value())
		{
			Pushf(*percent);
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(1.0f);
		return;
	case script::ObjectPropertyType::Strength:
	case script::ObjectPropertyType::Alignment:
		// a creature's strength or alignment (ecs::player_creature::GetCreatureProperty). (pending) anything else: a
		// spell's or a spell seed's strength, the alignment of the player a thing belongs to
		if (const auto value =
		        ecs::player_creature::GetCreatureProperty(std::as_const(registry), CreaturePhysiology(), entity, prop);
		    value.has_value())
		{
			Pushf(*value);
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::CreatureWarmth:
	case script::ObjectPropertyType::CreatureFatness:
	case script::ObjectPropertyType::CreatureEnergy:
	case script::ObjectPropertyType::CreatureItchiness:
	case script::ObjectPropertyType::CreatureAmountOfPoo:
	case script::ObjectPropertyType::CreatureExhaustion:
	case script::ObjectPropertyType::CreatureDehydration:
	case script::ObjectPropertyType::CreatureFightHealth:
	case script::ObjectPropertyType::CreatureMinSize:
	case script::ObjectPropertyType::CreatureMaxSize:
		// a creature's body: its need, fatness, fight health, or the size its small or big spell takes it to; anything
		// else: "Object not a creature" and 0
		if (const auto value =
		        ecs::player_creature::GetCreatureProperty(std::as_const(registry), CreaturePhysiology(), entity, prop);
		    value.has_value())
		{
			Pushf(*value);
			return;
		}
		if (registry.AllOf<ecs::components::Creature>(entity))
		{
			LogCreatureWithoutBody("GET_PROPERTY");
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_PROPERTY: Object not a creature");
		}
		Pushf(0.0f);
		return;
	case script::ObjectPropertyType::Height:
		// a totem statue: the share it was last set to, from none to all, not a height. (pending) a puzzle totem's
		// and every other thing's height, as the other properties not ported
		if (Locator::villageTotemSystem::has_value())
		{
			if (const auto share = Locator::villageTotemSystem::value().GetShare(entity); share.has_value())
			{
				Pushf(*share);
				return;
			}
		}
		NotImplemented(static_cast<int32_t>(prop));
		Pushi(0);
		return;
	default:
		// TODO(Daniels118): implement the other properties
		NotImplemented(static_cast<int32_t>(prop));
		Pushi(0);
		return;
	}
}

void SetProperty() // 022 SET_PROPERTY
{
	// the value, the object, the property
	const auto val = Popf();
	const auto object = Pop().uintVal;
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// a villager or an animal (the original's living cast; the script holds them as Villager / Animal entities)
	const bool living =
	    object != 0 && registry.Valid(entity) && registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity);
	switch (prop)
	{
	case script::ObjectPropertyType::Health:
		// a totem statue: its life, as given, with no clamp. (pending) every other object
		if (object != 0 && registry.Valid(entity) && Locator::villageTotemSystem::has_value() &&
		    Locator::villageTotemSystem::value().SetLifeFromScript(entity, val))
		{
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::Scale:
		// the object's scale (and its matrix when it changes): the Transform's uniform scale; a creature's drawn and own
		// size (ecs::player_creature::SetCreatureScale). (pending) a villager, an animal or a creature only here
		if (living)
		{
			registry.Get<Transform>(entity).scale = glm::vec3(val);
			return;
		}
		if (object != 0 && registry.Valid(entity) && ecs::player_creature::SetCreatureScale(registry, entity, val))
		{
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::Height:
		// a totem statue: its town's share of its people at worship (its own without a town), as given, with no clamp;
		// a creature's own size from the height (ecs::player_creature::SetCreatureHeight). (pending) a puzzle totem and
		// every other thing
		if (object != 0 && registry.Valid(entity) && Locator::villageTotemSystem::has_value() &&
		    Locator::villageTotemSystem::value().SetShareFromScript(entity, val))
		{
			return;
		}
		if (object != 0 && registry.Valid(entity) && ecs::player_creature::SetCreatureHeight(registry, entity, val))
		{
			return;
		}
		// anything else as the other properties not ported
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::Speed:
		// the speed in metres: a villager's (no per-villager factor; it stays, the state speed skips a villager controlled by
		// a script), the wall hug's for an animal
		if (living && registry.AllOf<ecs::components::Villager>(entity))
		{
			ecs::SetVillagerSpeedInMetres(entity, val);
			return;
		}
		if (living)
		{
			ecs::animal_ai::SetSpeedInMetres(entity, val);
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::Age:
		// a living thing -> its age (truncated): a villager's (the meshes when grownUpAge is crossed, the scale for the age),
		// an animal's; else "Cannot Set Property"
		if (living && registry.AllOf<ecs::components::Villager>(entity))
		{
			Locator::livingActionSystem::value().VillagerSetAge(entity,
			                                                    static_cast<uint32_t>(openblack::map_coords::FtoL(val)));
			return;
		}
		if (living)
		{
			ecs::animal_ai::SetAge(entity, static_cast<uint32_t>(openblack::map_coords::FtoL(val)));
			return;
		}
		if (object != 0 && registry.Valid(entity) && registry.AllOf<ecs::components::Creature>(entity))
		{
			NotImplemented(static_cast<int32_t>(prop)); // TODO: creatures
			return;
		}
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_PROPERTY: Cannot Set Property {}", static_cast<int>(prop));
		return;
	case script::ObjectPropertyType::Flying:
	case script::ObjectPropertyType::Drowning:
	case script::ObjectPropertyType::Moving:
		// "Cannot Set Property %d", nothing changes
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_PROPERTY: Cannot Set Property {}", static_cast<int>(prop));
		return;
	case script::ObjectPropertyType::YPos:
		// a script highlight -> its draw height; then, for any thing, its altitude = val and its 3D object moved there.
		// (pending) only the highlights here
		if (object != 0 && ecs::script_highlight::IsHighlight(static_cast<entt::entity>(object)))
		{
			ecs::script_highlight::SetYPos(static_cast<entt::entity>(object), val);
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::BuiltPercentage:
		// a building -> its built percentage (the features and the totem statues here); anything else -> "Cannot Set
		// Property"
		if (object == 0 || !(openblack::ecs::abodes::SetBuiltPercentage(entity, val) ||
		                     (registry.Valid(entity) && Locator::villageTotemSystem::has_value() &&
		                      Locator::villageTotemSystem::value().SetBuiltPercentageFromScript(entity, val)) ||
		                     openblack::ecs::feature_build::SetBuiltPercentage(entity, val)))
		{
			NotImplemented(static_cast<int32_t>(prop));
		}
		return;
	case script::ObjectPropertyType::Strength:
	case script::ObjectPropertyType::Alignment:
		// a creature's strength or alignment, as given (ecs::player_creature::SetCreatureProperty); no thing: "Object no
		// longer valid". (pending) anything else: a spell's or a spell seed's strength, a reward's alignment
		if (object == 0 || !registry.Valid(entity))
		{
			ObjectNoLongerValid("SET_PROPERTY");
			return;
		}
		if (ecs::player_creature::SetCreatureProperty(registry, CreaturePhysiology(), entity, prop, val))
		{
			return;
		}
		NotImplemented(static_cast<int32_t>(prop));
		return;
	case script::ObjectPropertyType::CreatureWarmth:
	case script::ObjectPropertyType::CreatureFatness:
	case script::ObjectPropertyType::CreatureEnergy:
	case script::ObjectPropertyType::CreatureItchiness:
	case script::ObjectPropertyType::CreatureAmountOfPoo:
	case script::ObjectPropertyType::CreatureExhaustion:
	case script::ObjectPropertyType::CreatureDehydration:
	case script::ObjectPropertyType::CreatureFightHealth:
	case script::ObjectPropertyType::CreatureMinSize:
	case script::ObjectPropertyType::CreatureMaxSize:
		// a creature's need, fatness, fight health, smallest or largest size, the warmth kept to -1..1 and the energy to
		// 0..1; no thing: "Object no longer valid"; anything else: "Object not a creature"
		if (object == 0 || !registry.Valid(entity))
		{
			ObjectNoLongerValid("SET_PROPERTY");
			return;
		}
		if (ecs::player_creature::SetCreatureProperty(registry, CreaturePhysiology(), entity, prop, val))
		{
			return;
		}
		if (registry.AllOf<ecs::components::Creature>(entity))
		{
			LogCreatureWithoutBody("SET_PROPERTY");
		}
		else
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_PROPERTY: Object not a creature");
		}
		return;
	default:
		// TODO(Daniels118): implement the other properties
		NotImplemented(static_cast<int32_t>(prop));
		return;
	}
}

void GetPosition() // 023 GET_POSITION
{
	const auto objId = Pop().uintVal;

	// the altitude of pos + relY, the object's world position; (0, 0, 0) for a lost object
	glm::vec3 position(0.0f);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(objId);
	if (objId != 0 && registry.Valid(entity))
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (const auto* flock = registry.TryGet<const ecs::components::Flock>(entity); flock != nullptr)
		{
			// a flock: its first member's position, else the flock's own domain centre
			position = flock->domainCentre;
			if (!flock->members.empty() && registry.Valid(flock->members.front()))
			{
				if (const auto* leader = registry.TryGet<const Transform>(flock->members.front()); leader != nullptr)
				{
					position = leader->position;
				}
			}
		}
		else if (registry.AllOf<ecs::components::Villager>(entity) && ecs::villager::AreWeThereAtDestination(entity, 0.0f))
		{
			// a wall hugger that is not a creature and is already there: its destination instead of its position. Its height
			// is the land's + the destination's (approximate: the land under it, openblack's WallHug keeps only x / z;
			// GET_DISTANCE ignores y). TODO: the animals' (AnimalBrain goal)
			const auto dest = *ecs::villager::GetDestPos(entity);
			position = OnGround(glm::vec3(dest.x, 0.0f, dest.y));
		}
		else if (transform != nullptr)
		{
			position = transform->position;
		}
	}

	PushVec(position);
}

namespace
{
/// The hands (Locator::handSystem); stops with a message when there are none
const HandSystemInterface& TheHands(const char* native)
{
	if (!Locator::handSystem::has_value())
	{
		std::fprintf(stderr, "%s: no hands in the locator (Locator::handSystem)\n", native);
		std::abort();
	}
	return Locator::handSystem::value();
}

/// The physics (Locator::dynamicsSystem); stops with a message when there is none
ecs::systems::DynamicsSystemInterface& ThePhysics(const char* native)
{
	if (!Locator::dynamicsSystem::has_value())
	{
		std::fprintf(stderr, "%s: no physics in the locator (Locator::dynamicsSystem)\n", native);
		std::abort();
	}
	return Locator::dynamicsSystem::value();
}

/// The creatures' locomotion (Locator::creatureLocomotionSystem); stops with a message when there is none
ecs::systems::CreatureLocomotionSystemInterface& TheCreatureLocomotion(const char* native)
{
	if (!Locator::creatureLocomotionSystem::has_value())
	{
		std::fprintf(stderr, "%s: no creature locomotion in the locator (Locator::creatureLocomotionSystem)\n", native);
		std::abort();
	}
	return Locator::creatureLocomotionSystem::value();
}
} // namespace

void SetPosition() // 024 SET_POSITION
{
	// z, y, x, then the thing. The refusals are silent in the original; openblack logs them at debug level
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	namespace set_position = ecs::script_set_position;
	set_position::Facts facts {.valid = object != 0 && registry.Valid(entity)};
	if (!facts.valid)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "SET_POSITION: Thing no longer valid");
		return;
	}
	const auto held = TheHands("SET_POSITION").GetHeldObject();
	facts.inHand = held.has_value() && *held == entity;
	// in the physics, as GET_PROPERTY's Flying reads it
	facts.flying = ThePhysics("SET_POSITION").Find(entity) != nullptr;
	facts.scriptMarker = registry.AllOf<ecs::components::ScriptMarker>(entity);
	facts.object = !facts.scriptMarker && ecs::script_type::IsObject(entity);
	// an object of a kind openblack never puts in the map cells counts as in the map
	facts.inMap =
	    !ecs::map_cells::IsFiledInMapCells(ecs::map_cells::KindOf(entity), registry.AllOf<ecs::components::Shark>(entity)) ||
	    ecs::map_cells::IsObjectInMap(entity);
	facts.creature = registry.AllOf<ecs::components::Creature>(entity);
	facts.living = registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(entity);
	facts.controlledByScript = ecs::script_held::IsControlledByScript(entity);
	if (facts.inHand)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "SET_POSITION: Setting object in hand to a position");
	}
	if (facts.flying)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "SET_POSITION: Trying to set position - Object is flying");
	}
	if (!facts.inHand && !facts.flying && facts.object && !facts.inMap)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "SET_POSITION: Trying to set position - Object not in map");
	}
	const auto plan = set_position::PlanFor(facts);
	// the point in the map's units (x and z truncated); on the ground there unless it keeps its height
	auto coords = map_coords::FromWorld(position);
	if (!plan.keepsHeight)
	{
		coords.altitude = 0.0f;
	}
	const auto world = map_coords::ToWorld(coords);
	switch (plan.move)
	{
	case set_position::Move::Refused:
		return;
	case set_position::Move::Creature:
		TheCreatureLocomotion("SET_POSITION").Place(entity, world);
		return;
	case set_position::Move::Copy:
		// only its position: a flock's is its domain centre
		if (auto* flock = registry.TryGet<ecs::components::Flock>(entity); flock != nullptr)
		{
			flock->domainCentre = world;
		}
		else if (auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
		{
			transform->position = world;
		}
		return;
	case set_position::Move::Object:
		ecs::map_cells::MoveMapObject(entity, world);
		// its matrix built again from its Y angle and its scale
		if (auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
		{
			transform->rotation = set_position::UprightRotation(transform->rotation);
			transform->scale = glm::vec3(transform->scale.x);
		}
		// the position at the start of the turn becomes the new one: drawn there from the next frame, no slide
		ecs::NotifyTeleported(entity);
		if (plan.intoScriptState)
		{
			ecs::script_held::SetLivingScriptState(entity, 4); // IN_SCRIPT
		}
		return;
	}
}

void GetDistance() // 025 GET_DISTANCE
{
	// the two vectors to gutils::GetDistance = hypotenuse(dx, dz): x and z only, y is ignored; under 0.5 it is 0
	// (villager_script_rules::ScriptDistance)
	const auto p1 = PopVec();
	const auto p0 = PopVec();
	Pushf(ecs::villager_script_rules::ScriptDistance(p0, p1));
}

namespace
{
/// CALL / CALL_NEAR for creatures: the first creature with GetDistanceInMetres(point, its map coords) <= 1.0 (CALL: no
/// type or sub-type test) / <= r and its sub-type == subtype (CALL_NEAR). (approximate) openblack has no creature
/// list: the registry's order. (pending, creature) the sub-type is taken as the creature's species
/// (ecs::script_type::SubtypeOf)
entt::entity FindCreatureForScript(const map_coords::MapCoords& coords, uint32_t subtype, std::optional<float> radius)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	registry.Each<const ecs::components::Creature>([&](entt::entity creature, const ecs::components::Creature& data) {
		if (found != entt::null || (radius.has_value() && static_cast<uint32_t>(data.species) != subtype))
		{
			return;
		}
		if (gutils::GetDistanceInMetres(coords, ecs::object::MapCoordsOf(creature)) <= radius.value_or(1.0f))
		{
			found = creature;
		}
	});
	return found;
}

/// CALL / CALL_NEAR after their pops (radius: CALL_NEAR's, nullopt for CALL):
/// - type 1..41, else "Invalid type=%d" and 0;
/// - the point as map coords to the type's find function (one for CALL, one for CALL_NEAR); none for NONE, MARKER,
///   DANCE, FLOCK, INFLUENCE_RING and WEATHER_THING: "No find function for type=%d" and 0;
/// - found: added as a script thing; else the warning "Thing not found" and 0.
/// The original's script error and warning messages are empty in the shipped game: it prints none of these. openblack
/// keeps them as its own diagnostics, "Thing not found" at debug level, since the scripts' waiting loops
/// (FollowUs L53071..53087) ask every turn until the thing exists
entt::entity FindForScript(int32_t type, uint32_t subtype, const glm::vec3& position, std::optional<float> radius,
                           bool excludingScripted)
{
	if (type <= 0 || type >= 0x2A)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid type={}", type);
		return entt::null;
	}
	const auto scriptType = static_cast<ObjectType>(type);
	const auto coords = map_coords::FromWorld(position);
	entt::entity found = entt::null;
	switch (scriptType)
	{
	case ObjectType::Marker:
	case ObjectType::Dance:
	case ObjectType::Flock:
	case ObjectType::InfluenceRing:
	case ObjectType::WeatherThing:
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "No find function for type={}", type);
		return entt::null;
	case ObjectType::Town:
		// the nearest town within r (10.0 for CALL). The type, the sub-type, the filter and excludingScripted are not used
		found = ecs::map_cells::GetNearestTown(coords, radius.value_or(10.0f));
		break;
	case ObjectType::Creature:
		found = FindCreatureForScript(coords, subtype, radius); // excludingScripted is not used either
		break;
	default:
	{
		// the nearest accepted thing of the map cells of the square, not cut at r. The filter: the type and sub-type for
		// CALL; for CALL_NEAR also GetDistanceInMetres(point, the thing or its totem) <= r; things in a script are rejected
		// when excludingScripted
		const float searchRadius = radius.value_or(1.0f);
		found = ecs::map_cells::FindNearForScript(
		    coords,
		    [&](entt::entity thing) {
			    if (excludingScripted && ecs::script_held::IsInScript(thing))
			    {
				    return false;
			    }
			    if (!ecs::script_type::Matches(thing, scriptType, subtype))
			    {
				    return false;
			    }
			    return !radius.has_value() ||
			           gutils::GetDistanceInMetres(coords, ecs::map_cells::ScriptDistancePoint(thing)) <= searchRadius;
		    },
		    searchRadius);
		break;
	}
	}
	if (found == entt::null)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "Thing not found");
		return entt::null;
	}
	ScriptObjects().Register(found, false);
	return found;
}
} // namespace

void Call() // 026 CALL
{
	// pops excludingScripted, the point (z, y, x), the sub-type, then the type
	const auto excludingScripted = Pop().intVal != 0;
	const auto position = PopVec();
	const auto subtype = static_cast<uint32_t>(Pop().intVal);
	const auto type = Pop().intVal;
	const auto thing = FindForScript(type, subtype, position, std::nullopt, excludingScripted);
	// pushed as an object. (known limitation) CHLApi's entity-as-id convention: entity 0 would read as "not found"
	// (THING_VALID tests objId != 0), as everywhere else in CHLApi
	Pusho(thing == entt::null ? 0 : static_cast<uint32_t>(thing));
}

void Create() // 027 CREATE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);

	// types 1..41 only, angle 0, scale 1; 0 when nothing is made
	const auto object = type > ObjectType::None && type <= ObjectType::AnimatedStatic
	                        ? CreateScriptObject(type, subtype, position, 0.0f, 1.0f)
	                        : entt::null;
	// added as a thing the script created
	ScriptObjects().Register(object, true);

	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void Random() // 028 RANDOM
{
	// the first POP is max, the second min; ((max - min) + 1) as a float, a synced GameFloatRand of it, + min, truncated
	// towards 0 and pushed back as a float: a whole number in [min, max + 1)
	const auto max = Popf();
	const auto min = Popf();
	const float span = max - min;
	const float range = span + 1.0f;
	const float drawn = game_random::GameFloatRand(range);
	const float sum = drawn + min;
	Pushf(static_cast<float>(static_cast<int32_t>(sum)));
}

void DllGettime() // 029 DLL_GETTIME
{
	// The game's own table has no handler here; the script library DLL puts its own in, LHVM::PushElaspedTime (the VM
	// tick count, one a game turn outside the citadel, x 0.1f)
	Locator::vm::value().PushElaspedTime();
}

/// The camera's and the game speed's owners (Locator::scriptControlSystem); the game stops with the native's name when
/// it is missing, or for the camera's natives (`needsCamera`) when the game's camera is
ecs::systems::ScriptControlSystemInterface* ScriptControl(const char* native, bool needsCamera = false)
{
	if (Locator::scriptControlSystem::has_value() && (!needsCamera || Locator::camera::has_value()))
	{
		return &Locator::scriptControlSystem::value();
	}
	std::fprintf(stderr, "chlapi: %s: no script control%s in the locator (Locator::scriptControlSystem)\n", native,
	             needsCamera ? " or no camera" : "");
	std::abort();
}

void StartCameraControl() // 030 START_CAMERA_CONTROL
{
	// Help/ScriptControl.cpp. Inside the citadel (game_clock::IsInsideCitadel) no camera mode. Outside, the script
	// control creates the script camera mode unless the current mode cannot be left (Camera/ScriptCamera.h)
	const bool insideCitadel = openblack::game_clock::IsInsideCitadel();
	auto& cameraControl = help::script_control::GetCameraControl();
	auto* control = ScriptControl(__func__, true);
	const bool granted =
	    control != nullptr &&
	    help::script_control::StartCameraControl(cameraControl, *control, Locator::camera::value(), ScriptVm(), insideCitadel);
	Pushb(granted);
}

void EndCameraControl() // 031 END_CAMERA_CONTROL
{
	// Help/ScriptControl.cpp: the camera is given back when this task has it; its camera part (the script mode deleted,
	// the player's mode from where the camera is, the FOV back to 70 degrees in 0.5 s) is the script control's
	auto* control = ScriptControl(__func__, true);
	if (control != nullptr &&
	    help::script_control::EndCameraControl(help::script_control::GetCameraControl(), *control, Locator::camera::value(),
	                                           audio::GetScriptAudioState(), ScriptVm()))
	{
		// the camera given back: the draw list is rebuilt on the next drawn frame
		ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
	}
}

void SetWidescreen() // 032 SET_WIDESCREEN
{
	// The help system's wide screen; the bars slide in HelpSystemInfo.wideScreenTime seconds (2.0). Only the task that
	// holds it or any when none does (Help/ScriptControl.cpp); the help system's hook moves the bars (Game.cpp)
	const auto on = static_cast<int32_t>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		help::script_control::SetWideScreen(*helpSystem, on, ScriptVm());
	}
	else
	{
		// openblack only (the original always has a help system; here a VM without a HelpSystem, e.g. tools): the bars move
		// without any owner. 2.0 is the default of CinemaBars::_transitionSeconds (no source)
		const float time =
		    Locator::infoConstants::has_value() ? Locator::infoConstants::value().helpSystem.wideScreenTime : 2.0f;
		Locator::cinematicDirectorSystem::value().SetWideScreen(on != 0, time);
		audio::SetScriptWideScreen(on != 0);
	}
}

void MoveGameThing() // 033 MOVE_GAME_THING
{
	// The pops: the radius (only the creature's), z, y, x, the object
	const auto radius = Popf();
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// none -> "Thing no longer valid"
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "MOVE_GAME_THING: Thing no longer valid");
		return;
	}
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "MOVE_GAME_THING {} to ({:.2f}, {:.2f}, {:.2f}) radius {:.2f}", object,
		                   position.x, position.y, position.z, radius);
	}
	// the map coords of pos: x / z of the point, y kept above the land (the walks use x / z)
	const glm::vec2 goal(position.x, position.z);
	// a creature in the map is taken under the script's control and walks there; out of it, nothing
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		if (ecs::map_cells::IsObjectInMap(entity))
		{
			CreatureMinds("MOVE_GAME_THING").MoveForScript(entity, position, radius);
		}
		return;
	}
	// a living thing: only when it is in the map and not drowning, else nothing (VillagerScriptMoveTo)
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		Locator::livingActionSystem::value().VillagerScriptMoveTo(entity, goal);
		return;
	}
	if (registry.AllOf<ecs::components::Animal>(entity))
	{
		// The same living branch for an animal: there already -> SetScriptState(IN_SCRIPT 4), else SetupMoveToPos(pos,
		// IN_SCRIPT 4) (animal_ai::ScriptMoveTo). (approximate) "in the map" as "not in the hand" (IN_HAND)
		const auto* brain = registry.TryGet<const ecs::components::AnimalBrain>(entity);
		if (brain == nullptr || ecs::IsDrowning(entity) ||
		    static_cast<ecs::animal_ai::AnimalState>(brain->topState) == ecs::animal_ai::AnimalState::InHand)
		{
			return;
		}
		ecs::animal_ai::ScriptMoveTo(entity, goal);
		return;
	}
	// a flock -> Flock::SetDomainCentrePos(coords): its tail's destination (a villager's or an animal's) and the flock's
	// domain centre (ECS/Flocks.h)
	if (registry.AllOf<ecs::components::Flock>(entity))
	{
		ecs::flocks::SetDomainCentrePos(entity, position);
		return;
	}
	// a weather thing or a computer player would move too (the latter with 60.0); openblack has neither kind of thing.
	// anything else: "Jonty - Thing must be living to move it!", then its position = coords, so its world point is the
	// vector itself. (approximate) only the Transform moves: openblack's derived data (static meshes, physics) is not told
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "MOVE_GAME_THING: Jonty - Thing must be living to move it!");
	if (auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
	{
		transform->position = position;
		ecs::NotifyTeleported(entity);
	}
}

void SetFocus() // 034 SET_FOCUS
{
	namespace set_position = ecs::script_set_position;
	// z, y, x, then the thing
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_FOCUS: Thing no longer valid");
		return;
	}
	// an object that is not living turns about Y to face the point, an instant snap: nothing when it already has that
	// angle; else it takes it, upright or with its tilt kept, and goes back in at the head of its cells' lists.
	// (inferred) openblack keeps no Y angle apart from the turn, so the turn it would get is compared instead
	const auto turnObject = [&registry, position](entt::entity thing) {
		auto* transform = registry.TryGet<Transform>(thing);
		if (transform == nullptr)
		{
			return;
		}
		const auto rotation =
		    set_position::FocusRotation(transform->rotation, set_position::FocusYAngle(transform->position, position),
		                                ecs::script_type::KeepsTiltWhenTurned(thing));
		if (!rotation.has_value())
		{
			// (pending) a tilted mobile static or mobile object: openblack keeps no x and z angles to turn it with
			NotImplemented(static_cast<int32_t>(ecs::script_type::TypeOf(thing)));
			return;
		}
		if (*rotation == transform->rotation)
		{
			return;
		}
		transform->rotation = *rotation;
		ecs::map_cells::OnAnglesOrScaleChanged(thing);
		registry.SetDirty();
	};
	// a script container: the focus on each member, without taking it under script control
	if (ecs::script_containers::IsContainer(entity))
	{
		ecs::script_containers::ForEachMember(entity, [position, &turnObject](entt::entity member) {
			// a creature member takes itself under script control as it turns to the point
			if (Locator::entitiesRegistry::value().AllOf<ecs::components::Creature>(member))
			{
				CreatureMinds("SET_FOCUS").FocusByScript(member, position);
			}
			else if (!ecs::living::SetFocus(member, position))
			{
				turnObject(member);
			}
			return false;
		});
		return;
	}
	// a thing that is not an object (a script marker...) cannot face anything: an error, nothing done
	if (registry.AllOf<ecs::components::ScriptMarker>(entity) || !ecs::script_type::IsObject(entity))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "SET_FOCUS: Thing must be living to face position (type {})",
		                    static_cast<int32_t>(ecs::script_type::TypeOf(entity)));
		return;
	}
	// a creature is put under script control first, then stops what it was doing and turns to face the point
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		ecs::script_held::SetControlledByScript(entity, true);
		CreatureMinds("SET_FOCUS").FocusByScript(entity, position);
		return;
	}
	// an instant snap of the yaw, nothing keeps the focus (villagers and animals: living::SetFocus)
	if (ecs::living::SetFocus(entity, position))
	{
		if (ScriptThingTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_FOCUS {} ({}, {}, {})", object, position.x, position.y,
			                   position.z);
		}
		return;
	}
	turnObject(entity);
}

void HasCameraArrived() // 035 HAS_CAMERA_ARRIVED
{
	// (1 in a network game, not ported) the current camera mode's arrived test: the script mode's, or the player's with
	// the same squared distance 0.001 to the destinations: here the player's Camera zoomers (Zoomer3; (inferred) the
	// original has one camera for both modes)
	if (script_camera::HasMode()) // the script mode, or a dual camera
	{
		Pushb(script_camera::ScriptArrived());
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto dp = camera.GetOrigin() - camera.GetOrigin(Camera::Interpolation::Target);
	const auto df = camera.GetFocus() - camera.GetFocus(Camera::Interpolation::Target);
	Pushb(glm::dot(dp, dp) < script_camera::k_ArrivedDistanceSquared &&
	      glm::dot(df, df) < script_camera::k_ArrivedDistanceSquared);
}

void FlockCreate() // 036 FLOCK_CREATE
{
	// the position (z, y, x), a flock added as a thing the script created (ECS/ScriptContainers.h). (pending) the
	// script name of the create
	const auto position = PopVec();
	const auto flock = ecs::script_containers::CreateFlock(position);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "FLOCK_CREATE {} ({}, {}, {})", static_cast<uint32_t>(flock), position.x,
		                   position.y, position.z);
	}
	Pusho(static_cast<uint32_t>(flock));
}

void FlockAttach() // 037 FLOCK_ATTACH
{
	// the leader flag, the target, then the obj (ECS/ScriptContainers.h)
	const auto asLeader = Pop().intVal != 0;
	const auto target = Pop().uintVal;
	const auto object = Pop().uintVal;
	const auto pushed = ecs::script_containers::Attach(object, target, asLeader);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "FLOCK_ATTACH {} to {} leader {} -> {}", object, target, asLeader, pushed);
	}
	Pusho(pushed);
}

void FlockDetach() // 038 FLOCK_DETACH
{
	// the container (first pop), then the obj id (ECS/ScriptContainers.h)
	const auto container = Pop().uintVal;
	const auto object = Pop().uintVal;
	const auto pushed = ecs::script_containers::Detach(container, object);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "FLOCK_DETACH {} from {} -> {}", object, container, pushed);
	}
	Pusho(pushed);
}

void FlockDisband() // 039 FLOCK_DISBAND
{
	// ECS/ScriptContainers.h
	ecs::script_containers::Disband(Pop().uintVal);
}

void IdSize() // 040 ID_SIZE
{
	// the members as a float; 0 with "Cannot Find Flock/Dance/Town Size"
	Pushf(ecs::script_containers::Size(Pop().uintVal).value_or(0.0f));
}

void FlockMember() // 041 FLOCK_MEMBER
{
	// the flock, then the thing: whether the living is one of the flock's members; not a flock or not a living gives
	// false
	const auto flock = static_cast<entt::entity>(Pop().uintVal);
	const auto thing = static_cast<entt::entity>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	const auto valid = [&registry](entt::entity e) { return e != entt::entity {0} && registry.Valid(e); };
	if (!valid(flock) || !registry.AllOf<ecs::components::Flock>(flock) || !valid(thing) ||
	    !registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(thing))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "FLOCK_MEMBER: Invalid Flock or Thing");
		Pushb(false);
		return;
	}
	const auto members = ecs::flocks::MembersFromHead(flock);
	Pushb(std::ranges::find(members, thing) != members.end());
}

void GetHandPosition() // 042 GET_HAND_POSITION
{
	const auto handEntity = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& handTransform = Locator::entitiesRegistry::value().Get<Transform>(handEntity);

	PushVec(handTransform.position);
}

void PlaySoundEffect() // 043 PLAY_SOUND_EFFECT
{
	// six POPs (withPos, z, y, x, bank, sample), then audio::script_sound
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	audio::script_sound::PlaySoundEffect(sample, bank, position, withPosition);
}

/// The script's thing for the music functions: the object, or nullopt with the original's "Thing no longer valid"
/// (approximate: the original looks the id up in its script table, range 1..511; here 0 is null as in Pusho and a
/// valid entity stands for a live thing)
std::optional<audio::ThingId> MusicThing(uint32_t objId)
{
	if (objId != 0 && Locator::entitiesRegistry::value().Valid(static_cast<entt::entity>(objId)))
	{
		return objId;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Thing no longer valid");
	return std::nullopt;
}

void StartMusic() // 044 START_MUSIC
{
	const auto music = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "START_MUSIC({})", music);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		gameMusic->ScriptStartMusic(music);
	}
}

void StopMusic() // 045 STOP_MUSIC
{
	// the script music set to none
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "STOP_MUSIC()");
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		gameMusic->ScriptStopMusic();
	}
}

void AttachMusic() // 046 ATTACH_MUSIC
{
	// the thing is popped first, then the type
	const auto target = MusicThing(Pop().uintVal);
	const auto music = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "ATTACH_MUSIC({}, {})", music, target.value_or(0));
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		gameMusic->ScriptAttachMusic(music, target);
	}
}

void DetachMusic() // 047 DETACH_MUSIC
{
	// the thing's music is removed
	const auto object = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->RemoveThingMusic(*object);
	}
}

void ObjectDelete() // 048 OBJECT_DELETE
{
	// the mode, then the object; nothing for a thing no longer there
	const auto mode = Pop().intVal;
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == 0 || !registry.Valid(entity))
	{
		return;
	}
	// a container (town, flock, dance) is disbanded; no script slot is freed here, on any path (a slot is let go through
	// its references). (approximate) here before the puzzle test and the mode's action, where the original disbands after
	// them: openblack destroys a deleted container at once (the original only marks it) and has no container's own
	// deletion yet, which empties a flock before its disband. Only mode 0 on a container tells the two orders apart
	if (ecs::script_containers::IsContainer(entity))
	{
		ecs::script_containers::Disband(object);
	}
	// a puzzle game: removed from the script and reset, whatever the mode; never deleted here
	if (registry.AllOf<ecs::components::PuzzleGame>(entity))
	{
		NotImplemented(); // (pending)
		return;
	}
	const bool creature = registry.AllOf<ecs::components::Creature>(entity);
	// A creature leaves the game through its own removal, which lets go of its leash, fights, hand and lists
	const auto deleteNow = [entity, creature]() {
		if (creature)
		{
			ecs::creature_removal::RemoveFromGame(entity);
		}
		else
		{
			ecs::ToBeDeleted(entity);
		}
	};
	// the modes past 0 act on objects only: a town, a flock, a timer and the other things that are not objects stay
	using ecs::script_type::ScriptDelete;
	switch (ecs::script_type::ScriptDeleteOf(mode, ecs::script_type::IsObject(entity), creature,
	                                         registry.AllOf<ecs::components::CitadelHeart>(entity)))
	{
	case ScriptDelete::Delete:           // (pending: villager::Delete when it exists)
	case ScriptDelete::BreakUpAndDelete: // the mesh broken up (15.0, 3.0) (pending: not drawn)
		deleteNow();
		break;
	case ScriptDelete::GhostAndDelete:
		// a 500 ms ghost of its mesh in the landscape fade list (pending: the ghost is not drawn)
		ecs::ToBeDeleted(entity);
		break;
	case ScriptDelete::Fizzle:
		// the creature fizzles out of sight over two seconds for good, and is taken out of the game once it has; it is
		// not deleted here
		if (Locator::creatureFizzSystem::has_value())
		{
			Locator::creatureFizzSystem::value().SetFizz(entity, 1.0f, 2.0f, true);
		}
		break;
	case ScriptDelete::CitadelHeart:
		// a heart being destroyed has its model broken up (80.0, 3.0) and stays; any other starts its destruction
		if (!Locator::templeDestructionSystem::has_value())
		{
			std::fputs("chlapi: no temple destruction in the locator (Locator::templeDestructionSystem)\n", stderr);
			std::abort();
		}
		Locator::templeDestructionSystem::value().DeleteByScript(entity);
		break;
	case ScriptDelete::Nothing: // only removed from the script, the thing stays
		break;
	}
}

void FocusFollow() // 049 FOCUS_FOLLOW
{
	// POP the thing (none -> "Thing no longer valid") -> the script camera's focus follows it: the path dropped, and the
	// focus heads for the thing every frame
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusFollow(*thing);
	}
}

void PositionFollow() // 050 POSITION_FOLLOW
{
	// POP the thing -> the camera's position follows it every frame, from the heading and pitch the camera has now, at
	// the thing's viewing distance (its height x 8); with "behind" (on in the script mode) the heading is 0, relative to
	// a wall hugger's angle. The path is kept
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::PositionFollow(*thing);
	}
}

void CallNear() // 051 CALL_NEAR
{
	// pops excludingScripted, the radius, the point (z, y, x), the sub-type, then the type
	const auto excludingScripted = Pop().intVal != 0;
	const auto radius = Popf();
	const auto position = PopVec();
	const auto subtype = static_cast<uint32_t>(Pop().intVal);
	const auto type = Pop().intVal;
	const auto thing = FindForScript(type, subtype, position, radius, excludingScripted);
	Pusho(thing == entt::null ? 0 : static_cast<uint32_t>(thing)); // pushed as an object
}

void SpecialEffectPosition() // 052 SPECIAL_EFFECT_POSITION
{
	// a spot visual with the given duration
	const auto duration = Popf();
	const auto position = PopVec();
	const auto effect = Pop().intVal;
	const auto object = Locator::particleSystem::value().StartSpotVisual(static_cast<SpotVisualType>(effect), position,
	                                                                     psys::manager::SpotVisualTurns(duration), entt::null);
	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void SpecialEffectObject() // 053 SPECIAL_EFFECT_OBJECT
{
	// a spot visual at the object's position when made, with the given duration (it does not follow it); its loss
	// closes the effect
	const auto duration = Popf();
	const auto target = static_cast<entt::entity>(Pop().uintVal);
	const auto effect = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(target) ? registry.TryGet<const Transform>(target) : nullptr;
	entt::entity object = entt::null;
	if (transform != nullptr)
	{
		object = Locator::particleSystem::value().StartSpotVisual(static_cast<SpotVisualType>(effect), transform->position,
		                                                          psys::manager::SpotVisualTurns(duration), target);
	}
	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void DanceCreate() // 054 DANCE_CREATE
{
	// pops the time, the point (z, y, x), the dance's row, then what it is danced about (which may be nothing)
	const auto time = Popf();
	const auto position = PopVec();
	const auto type = Pop().intVal;
	const auto objectId = Pop().uintVal;
	const auto object = static_cast<entt::entity>(objectId);
	const auto owner = objectId != 0 && Locator::entitiesRegistry::value().Valid(object) ? object : entt::null;
	// the point made a map position as the game makes one from a point (x and z to 16.16, the height above the land
	// under it kept as its altitude); the time truncated to whole turns; the dance is made by a script, so it never
	// takes the camera over
	const auto dance = Locator::danceSystem::value().Create(static_cast<DanceInfo>(type), map_coords::FromWorld(position),
	                                                        owner, static_cast<uint32_t>(map_coords::FtoL(time)), true);
	if (dance == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "DANCE_CREATE: Dance not created");
		Pusho(0);
		return;
	}
	// added as a thing the script created
	ScriptObjects().Register(dance, true);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "DANCE_CREATE {} row {} about {} ({}, {}, {}) for {}",
		                   static_cast<uint32_t>(dance), type, objectId, position.x, position.y, position.z, time);
	}
	Pusho(static_cast<uint32_t>(dance));
}

void CallIn() // 055 CALL_IN
{
	// excluding (first pop), the container, the sub-type, the type
	const auto excludingScripted = Pop().intVal != 0;
	const auto container = static_cast<entt::entity>(Pop().uintVal);
	const auto subtype = static_cast<uint32_t>(Pop().intVal);
	const auto type = static_cast<script::ObjectType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	if (container != entt::null && registry.Valid(container))
	{
		found = ecs::script_containers::Find(container, type, subtype, excludingScripted);
	}
	if (found == entt::null)
	{
		// a warning, push 0
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "CALL_IN: Cannot find in");
		Pusho(0);
		return;
	}
	// added as a found script thing
	ScriptObjects().Register(found, false);
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "CALL_IN {} in {} -> {}", static_cast<int>(type),
		                   static_cast<uint32_t>(container), static_cast<uint32_t>(found));
	}
	Pusho(static_cast<uint32_t>(found));
}

void ChangeInnerOuterProperties() // 056 CHANGE_INNER_OUTER_PROPERTIES
{
	// calm (first pop), outer, inner, the obj (ECS/ScriptContainers.h)
	const auto calm = Popf();
	const auto outer = Popf();
	const auto inner = Popf();
	const auto object = Pop().uintVal;
	ecs::script_containers::ChangeInnerOuter(object, inner, outer, calm);
}

/// What a script gave for a challenge's record, taken off its stack: every value, however many arguments it gave its
/// reminder script, so that nothing is left behind for the script's next calls
script::challenge_snapshots::Snapshot PopChallengeSnapshot(script::challenge_snapshots::Call call)
{
	auto& lhvm = Locator::vm::value();
	const auto snapshot =
	    script::challenge_snapshots::Read(call, {
	                                                .pop =
	                                                    [&lhvm]() {
		                                                    script::challenge_snapshots::Value value;
		                                                    value.value = lhvm.Pop(value.type);
		                                                    return value;
	                                                    },
	                                                .text = [&lhvm](uint32_t offset) { return lhvm.GetString(offset); },
	                                            });
	if (snapshot.tooManyArguments)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Too many arguments for a challenge's reminder script: {}",
		                    snapshot.arguments.size());
	}
	return snapshot;
}

/// The player's record of a challenge is kept up to date from a script's snapshot. A snapshot of a challenge already
/// recorded moves the local player's alignment at once (not through the turn's pending change). The things a record
/// holds keep a script reference. The challenge's picture is not taken yet.
void KeepChallengeRecord(const script::challenge_snapshots::Snapshot& snapshot, script::challenge_snapshots::Call call)
{
	const ecs::challenge_records::References references {
	    .add =
	        [](entt::entity thing) {
		        if (static_cast<uint32_t>(thing) != 0)
		        {
			        ScriptObjects().AddReference(thing);
		        }
	        },
	    .remove =
	        [](entt::entity thing) {
		        if (static_cast<uint32_t>(thing) != 0)
		        {
			        ScriptObjects().RemoveReference(thing);
		        }
	        },
	};
	const auto outcome =
	    ecs::challenge_records::Record(Locator::entitiesRegistry::value(), snapshot, call, game_clock::Turn(), references);
	if (!outcome.alignment.has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	const auto change =
	    ecs::challenge_records::AlignmentChange(ecs::effects::alignment::Get(player), *outcome.alignment,
	                                            Locator::infoConstants::value().player.maxScriptAlignmentChange);
	ecs::effects::alignment::AddClamped(player, change);
}

void Snapshot() // 057 SNAPSHOT
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Start),
	                    script::challenge_snapshots::Call::Start);
}

void GetAlignment() // 058 GET_ALIGNMENT
{
	// the player's alignment value
	const auto player = Pop().intVal;
	Pushf(
	    ecs::effects::alignment::Get(static_cast<PlayerNames>(std::clamp(player, 0, static_cast<int>(PlayerNames::NEUTRAL)))));
}

void SetAlignment() // 059 SET_ALIGNMENT
{
	// the player first, then the value; out of -1..1 it is an error and nothing happens, otherwise AddClamped: the value
	// is ADDED to the player's alignment (clamped), despite the name
	const auto player = Pop().intVal;
	const auto value = Pop().floatVal;
	if (value < -1.0f || value > 1.0f)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ALIGNMENT: Alignment out of range ({})", value);
		return;
	}
	ecs::effects::alignment::AddClamped(static_cast<PlayerNames>(std::clamp(player, 0, static_cast<int>(PlayerNames::NEUTRAL))),
	                                    value);
}

void InfluenceObject() // 060 INFLUENCE_OBJECT
{
	magic::script::InfluenceObject(); // Magic/Script/CHLInfluence.cpp
}

void InfluencePosition() // 061 INFLUENCE_POSITION
{
	magic::script::InfluencePosition(); // Magic/Script/CHLInfluence.cpp
}

void GetInfluence() // 062 GET_INFLUENCE
{
	magic::script::GetInfluence(); // Magic/Script/CHLInfluence.cpp
}

void SetInterfaceInteraction() // 063 SET_INTERFACE_INTERACTION
{
	// the level (Help/InterfaceInteraction.h)
	help::interface_interaction::Set(Pop().intVal);
}

void Played() // 064 PLAYED
{
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == 0 || !registry.Valid(entity))
	{
		// "Thing no longer valid" -> 1
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "PLAYED: Thing no longer valid");
		Pushb(true);
		return;
	}
	// a creature: whether it has played what it was doing, giving up the rest of its agenda when it only idles
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		const bool played = CreatureMinds("PLAYED").HasPlayed(entity);
		if (played && ScriptThingTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "PLAYED {}: true", object);
		}
		Pushb(played);
		return;
	}
	if (registry.AllOf<openblack::ecs::components::PuzzleGame>(entity))
	{
		// a puzzle game: whether it was played
		Pushb(openblack::ecs::IsPuzzleGamePlayed(entity));
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		// a villager: whether its script animation is complete
		const bool complete = Locator::livingActionSystem::value().VillagerHasPlayedScriptAnimation(entity);
		if (complete && ScriptThingTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "PLAYED {}: true", object);
		}
		Pushb(complete);
		return;
	}
	if (registry.AllOf<ecs::components::Animal>(entity))
	{
		// TODO(Daniels118): any other living thing: its final state == 4 IN_SCRIPT
		NotImplemented();
		Pushb(false);
		return;
	}
	// a weather thing would answer whether it is done: openblack has no weather things. Anything else: "Thing not
	// living" and 1
	SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "PLAYED: Thing not living");
	Pushb(true);
}

void RandomUlong() // 065 RANDOM_ULONG
{
	// the first POP is high, the second low; a synced draw from low to high, both included, pushed as an int
	const auto high = Pop().uintVal;
	const auto low = Pop().uintVal;
	Pushi(static_cast<int32_t>(script::random::WholeNumberBetween(low, high, game_random::Service())));
}

void SetGamespeed() // 066 SET_GAMESPEED
{
	// Help/ScriptControl.cpp
	const auto speed = Popf();
	if (const auto* control = ScriptControl(__func__); control != nullptr)
	{
		help::script_control::SetGameSpeed(*control, ScriptVm(), speed);
	}
}

void CallInNear() // 067 CALL_IN_NEAR
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto pos = PopVec();
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void OverrideStateAnimation() // 068 OVERRIDE_STATE_ANIMATION
{
	// the clip, then the thing
	const auto clip = Pop().intVal;
	const auto object = Pop().uintVal;
	// clip <= 0 or >= 441: "Invalid animation forced", and it goes on
	if (ecs::living::IsInvalidForcedClip(clip))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OVERRIDE_STATE_ANIMATION: Invalid animation forced {}", clip);
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !registry.Valid(entity))
	{
		return; // no thing, nothing said
	}
	// the clip's entry in the animation list, entry 0 out of [0, count)
	const int32_t index = ecs::living::ForcedClipIndex(clip);
	// a changed anim is set on the 3D object with its cycle time 0. (not ported) the remind record for the script, read
	// only when a play animation resumes. The clip lasts until the next state animation is set
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		Locator::livingActionSystem::value().VillagerOverrideAnimation(entity, index);
		return;
	}
	if (registry.AllOf<ecs::components::Animal>(entity))
	{
		ecs::SetAnimalAnim(entity, index, true);
		return;
	}
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		NotImplemented(); // TODO: creatures
		return;
	}
	// not a living thing
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OVERRIDE_STATE_ANIMATION: Thing must be living");
}

void CreatureCreateRelativeToCreature() // 069 CREATURE_CREATE_RELATIVE_TO_CREATURE
{
	// const auto type = Pop().intVal;
	// const auto position = PopVec();
	// const auto scale = Popf();
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void CreatureLearnEverything() // 070 CREATURE_LEARN_EVERYTHING
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSetKnowsAction() // 071 CREATURE_SET_KNOWS_ACTION
{
	// POP whether it knows, the action, the kind of action (0 an ordinary skill, 1 a miracle by its magic type), then the
	// creature. No thing: "No creature for script"; anything but a creature: "No script for creature" and nothing changed
	const auto knows = Pop().intVal != 0;
	const auto action = Pop().uintVal;
	const auto kind = Pop().uintVal;
	const auto object = Pop().uintVal;
	const auto thing = ScriptThing(object, "CREATURE_SET_KNOWS_ACTION", "No creature for script");
	if (thing.has_value() && !Locator::creatureMindSystem::value().SetKnowsAction(*thing, kind, action, knows))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATURE_SET_KNOWS_ACTION: No script for creature");
	}
}

void CreatureSetAgendaPriority() // 072 CREATURE_SET_AGENDA_PRIORITY
{
	// POP the priority, then the creature: what it carries out counts as that pressing. Nothing is said for a thing that is
	// not a creature
	const auto priority = Popf();
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	if (object != 0 && Locator::entitiesRegistry::value().Valid(entity))
	{
		Locator::creatureMindSystem::value().SetAgendaPriority(entity, priority);
	}
}

void CreatureTurnOffAllDesires() // 073 CREATURE_TURN_OFF_ALL_DESIRES
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureLearnDistinctionAboutActivityObject() // 074 CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureDoAction() // 075 CREATURE_DO_ACTION
{
	// the thing it is done with (first pop, 0 for none), the thing it is done to, the action's row, the creature
	const auto withObject = Pop().uintVal;
	const auto targetObject = Pop().uintVal;
	const auto action = Pop().intVal;
	const auto creatureObject = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = [&registry](uint32_t object) -> std::optional<entt::entity> {
		const auto entity = static_cast<entt::entity>(object);
		return object != 0 && registry.Valid(entity) ? std::optional(entity) : std::nullopt;
	};
	const auto creature = thing(creatureObject);
	const auto target = thing(targetObject);
	if (!creature.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATURE_DO_ACTION: No creature for script");
	}
	if (!target.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATURE_DO_ACTION: No object-to-act-on for script");
	}
	if (!creature.has_value() || !target.has_value())
	{
		return;
	}
	if (!registry.AllOf<ecs::components::Creature>(*creature))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATURE_DO_ACTION: No creature for script");
		return;
	}
	// the script takes control of it first, then it carries the action out
	ecs::script_held::SetControlledByScript(*creature, true);
	if (!CreatureMinds("CREATURE_DO_ACTION")
	         .ScriptDoAction(*creature, static_cast<uint32_t>(action), *target, thing(withObject)))
	{
		// an action openblack's creatures cannot carry out yet
		NotImplemented();
	}
}

void InCreatureHand() // 076 IN_CREATURE_HAND
{
	// const auto creature = Pop().uintVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void CreatureSetDesireValue() // 077 CREATURE_SET_DESIRE_VALUE
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSetDesireActivated78() // 078 CREATURE_SET_DESIRE_ACTIVATED
{
	// const auto active = Pop().intVal;
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSetDesireActivated79() // 079 CREATURE_SET_DESIRE_ACTIVATED
{
	// const auto active = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureSetDesireMaximum() // 080 CREATURE_SET_DESIRE_MAXIMUM
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ConvertCameraPosition() // 081 CONVERT_CAMERA_POSITION
{
	// the camera "Cam%d" of camera.edt, its position
	const auto cameraEnum = Pop().intVal;
	const auto camera = camera_edits::FindCamera(cameraEnum);
	const auto position =
	    camera.has_value() ? glm::vec3(camera->position[0], camera->position[1], camera->position[2]) : glm::vec3(0.0f);
	Pushv(position.x);
	Pushv(position.y);
	Pushv(position.z);
}

void ConvertCameraFocus() // 082 CONVERT_CAMERA_FOCUS
{
	// the camera "Cam%d" of camera.edt, its focus
	const auto cameraEnum = Pop().intVal;
	const auto camera = camera_edits::FindCamera(cameraEnum);
	const auto focus = camera.has_value() ? glm::vec3(camera->focus[0], camera->focus[1], camera->focus[2]) : glm::vec3(0.0f);
	Pushv(focus.x);
	Pushv(focus.y);
	Pushv(focus.z);
}

void CreatureSetPlayer() // 083 CREATURE_SET_PLAYER
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartCountdownTimer() // 084 START_COUNTDOWN_TIMER
{
	// ECS/ScriptTimer.h script_countdown
	const auto timeout = Popf();
	if (!ecs::script_countdown::Start(timeout))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "START_COUNTDOWN_TIMER: Invalid time for timer");
	}
}

void CreatureInitialiseNumTimesPerformedAction() // 085 CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureGetNumTimesActionPerformed() // 086 CREATURE_GET_NUM_TIMES_ACTION_PERFORMED
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void RemoveCountdownTimer() // 087 REMOVE_COUNTDOWN_TIMER
{
	ecs::script_countdown::Remove(); // the countdown timer is cleared
}

void GetObjectDropped() // 088 GET_OBJECT_DROPPED
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void ClearDroppedByObject() // 089 CLEAR_DROPPED_BY_OBJECT
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreateReaction() // 090 CREATE_REACTION
{
	// const auto reaction = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void RemoveReaction() // 091 REMOVE_REACTION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetCountdownTimer() // 092 GET_COUNTDOWN_TIMER
{
	// the remaining whole seconds
	Pushf(ecs::script_countdown::RemainingSeconds());
}

void StartDualCamera() // 093 START_DUAL_CAMERA
{
	// POP b then a; either missing -> "Thing invalid for dual cam" and nothing; else a two-things camera mode on top of
	// whatever mode is current (no mode nor citadel check): the camera looks at the two things' middle from their
	// distance apart (Camera/ScriptCamera.h)
	const auto objectB = Pop().uintVal;
	const auto objectA = Pop().uintVal;
	const auto b = CameraThing(objectB, __func__);
	const auto a = CameraThing(objectA, __func__);
	if (!a.has_value() || !b.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Thing invalid for dual cam", __func__);
		return;
	}
	const auto& camera = Locator::camera::value();
	script_camera::StartDual(*a, *b, camera.GetOriginZoomer(), camera.GetFocusZoomer());
}

void UpdateDualCamera() // 094 UPDATE_DUAL_CAMERA
{
	// POP b then a; either missing -> "Thing invalid for dual cam" and nothing; else when the current mode is a
	// two-things camera its things are set (a point camera becomes a two things one)
	const auto objectB = Pop().uintVal;
	const auto objectA = Pop().uintVal;
	const auto b = CameraThing(objectB, __func__);
	const auto a = CameraThing(objectA, __func__);
	if (!a.has_value() || !b.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Thing invalid for dual cam", __func__);
		return;
	}
	script_camera::UpdateDual(*a, *b);
}

void ReleaseDualCamera() // 095 RELEASE_DUAL_CAMERA
{
	// a current two-things camera mode is deleted and popped: the mode under it (the script mode) moves the camera again,
	// its seconds from 0
	script_camera::ReleaseDual();
}

void SetCreatureHelp() // 096 SET_CREATURE_HELP
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetTargetObject() // 097 GET_TARGET_OBJECT
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void CreatureDesireIs() // 098 CREATURE_DESIRE_IS
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void CountdownTimerExists() // 099 COUNTDOWN_TIMER_EXISTS
{
	Pushb(ecs::script_countdown::Exists()); // whether the countdown timer exists
}

void LookGameThing() // 100 LOOK_GAME_THING
{
	// POP the object, the spirit; no object -> "Object no longer valid"; else the spirit looks at it (the per-turn
	// refresh points, original bug kept)
	const auto target = Pop().uintVal;
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	const auto object = SpiritThing(target, "LOOK_GAME_THING");
	if (auto* control = SpiritControl(); control != nullptr && object != 0)
	{
		control->SpiritLookObject(spirit, object);
	}
}

void GetObjectDestination() // 101 GET_OBJECT_DESTINATION
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureForceFinish() // 102 CREATURE_FORCE_FINISH
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void HideCountdownTimer() // 103 HIDE_COUNTDOWN_TIMER
{
	ecs::script_countdown::SetShown(false); // the countdown timer is hidden
}

void GetActionTextForObject() // 104 GET_ACTION_TEXT_FOR_OBJECT
{
	// pops nothing and always pushes the help text 828
	Pushi(828);
}

void CreateDualCameraWithPoint() // 105 CREATE_DUAL_CAMERA_WITH_POINT
{
	// POP the point (z, y, x) then the thing; a thing-and-point camera mode with no check: a thing that is not there
	// leaves a mode that moves nothing (its update would read through null in the original) until the turn's check of
	// the stacked modes drops it
	const auto point = PopVec();
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	const auto& camera = Locator::camera::value();
	script_camera::StartDualWithPoint(thing.value_or(entt::null), point, camera.GetOriginZoomer(), camera.GetFocusZoomer());
}

void SetCameraToFaceObject() // 106 SET_CAMERA_TO_FACE_OBJECT
{
	// POP the distance, then the thing; with the script mode: the focus on the thing (half its height up), the position
	// `distance` away along its facing and 0.1 rad up; then the script camera's position and focus are set (both drop
	// the path and the follows)
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	if (!ScriptCameraMode(__func__))
	{
		return;
	}
	// (inferred) no thing: the original says "no object to face" and then reads through the null pointer; here nothing
	// more is done
	const auto thing = object != 0 ? static_cast<entt::entity>(object) : entt::null;
	if (const auto points = script_camera::FaceObject(thing, distance); points.has_value())
	{
		script_camera::SetPosition(points->position);
		script_camera::SetFocus(points->focus);
		// the script camera moved: the draw list is rebuilt on the next drawn frame
		ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
	}
}

void MoveCameraToFaceObject() // 107 MOVE_CAMERA_TO_FACE_OBJECT
{
	// POP the time, the distance, then the thing; with the script mode, the same points as 106, the script camera's
	// position and focus moved there in that time (seconds of the wall clock)
	const auto time = Popf();
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	if (!ScriptCameraMode(__func__))
	{
		return;
	}
	const auto thing = object != 0 ? static_cast<entt::entity>(object) : entt::null; // (inferred) as in 106
	if (const auto points = script_camera::FaceObject(thing, distance); points.has_value())
	{
		script_camera::MovePosition(points->position, time);
		script_camera::MoveFocus(points->focus, time);
	}
}

void GetMoonPercentage() // 108 GET_MOON_PERCENTAGE
{
	// How full the moon was the last time it showed: 0 at the full moon, 1 at a new moon, and 1 before it first shows
	const auto* moon = Locator::entitiesRegistry::value().TryGet<const ecs::components::Moon>(TheSkySystem().GetMoon());
	Pushf(graphics::moon::ScriptPercentage(moon != nullptr ? moon->phase : 0.0f));
}

void PopulateContainer() // 109 POPULATE_CONTAINER
{
	// the subtype, the type, the quantity and the container, in that order (ECS/ScriptPopulate.h)
	namespace populate = ecs::script_populate;
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;
	const auto count = populate::CountOf(Popf());
	const auto container = static_cast<entt::entity>(Pop().uintVal);
	if (!populate::IsValidType(type))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Invalid type={}", type);
		// the game leaves a value on the stack here although the statement gives none back
		Pusho(0);
		return;
	}
	const auto place = ecs::script_containers::PlaceOf(container);
	if (container == entt::null || container == static_cast<entt::entity>(0) || !place.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Container not valid");
		return;
	}
	// about the container's place on the land, its own height above the land added
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({place->x, place->z}) : 0.0f;
	const glm::vec3 centre(place->x, ground + place->y, place->z);
	const float spread = populate::SpreadOf(count);
	const auto floatRand = [](float x) { return game_random::GameFloatRand(x); };
	auto& registry = Locator::entitiesRegistry::value();
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto at = populate::PlaceOf(centre, spread, floatRand);
		const auto thing = CreateScriptObject(static_cast<ObjectType>(type), static_cast<uint32_t>(subtype), at, 0.0f, 1.0f);
		if (thing == entt::null || !registry.Valid(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Could not create thing for populate");
			return;
		}
		if (!registry.AnyOf<ecs::components::Animal, ecs::components::Villager, ecs::components::Creature>(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Created Thing Not Living");
			ecs::ToBeDeleted(thing);
			return;
		}
		// added as a thing the script created, then into its container
		ScriptObjects().Register(thing, true);
		if (!ecs::script_containers::TakePopulated(container, thing))
		{
			return;
		}
	}
}

void AddReference() // 110 ADD_REFERENCE
{
	// the script reference is incremented (the original pushes nothing; the binding's one output is kept as the object)
	const auto objId = Pop().uintVal;
	if (objId != 0)
	{
		ScriptObjects().AddReference(static_cast<entt::entity>(objId));
	}
	Pusho(objId);
}

void RemoveReference() // 111 REMOVE_REFERENCE
{
	// the script reference is decremented
	const auto objId = Pop().uintVal;
	if (objId != 0)
	{
		ScriptObjects().RemoveReference(static_cast<entt::entity>(objId));
	}
	Pusho(objId);
}

void SetGameTime() // 112 SET_GAME_TIME
{
	// the visual time forced from the script time
	const auto time = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_GAME_TIME({})", time);
	Game::SetTime(time);
}

void GetGameTime() // 113 GET_GAME_TIME
{
	// the visual time mapped back to script time
	Pushf(Locator::dayNightClock::value().Clock().GetScriptTime());
}

void GetRealTime() // 114 GET_REAL_TIME
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetRealDay115() // 115 GET_REAL_DAY
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetRealDay116() // 116 GET_REAL_DAY
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetRealMonth() // 117 GET_REAL_MONTH
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetRealYear() // 118 GET_REAL_YEAR
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void RunCameraPath() // 119 RUN_CAMERA_PATH
{
	// the script camera runs the path camera.edt "Track%d" (3D/CameraEdits.h, 3D/CameraTrack.h)
	const auto path = static_cast<int32_t>(Pop().intVal);
	if (ScriptCameraMode(__func__))
	{
		script_camera::RunPath(path);
	}
}

/// The dialogue's owner (Locator::dialogueControlSystem); the game stops with the native's name when it is missing
ecs::systems::DialogueControlSystemInterface* DialogueControl(const char* native)
{
	if (Locator::dialogueControlSystem::has_value())
	{
		return &Locator::dialogueControlSystem::value();
	}
	std::fprintf(stderr, "chlapi: %s: no dialogue control in the locator (Locator::dialogueControlSystem)\n", native);
	std::abort();
}

void StartDialogue() // 120 START_DIALOGUE
{
	// Help/ScriptControl.cpp; the advisors going home are help system hooks (not ported). Without a HelpSystem
	// (openblack only, the original always has one): false
	auto* helpSystem = help::Get();
	auto* dialogue = helpSystem != nullptr ? DialogueControl(__func__) : nullptr;
	Pushb(dialogue != nullptr && help::script_control::StartDialogue(*helpSystem, *dialogue, ScriptVm()));
}

void EndDialogue() // 121 END_DIALOGUE
{
	// Help/ScriptControl.cpp: only for the task that has the dialogue (nothing without a HelpSystem: openblack only)
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		if (auto* dialogue = DialogueControl(__func__); dialogue != nullptr)
		{
			help::script_control::EndDialogue(*helpSystem, *dialogue, audio::GetScriptAudioState(), ScriptVm());
		}
	}
}

void IsDialogueReady() // 122 IS_DIALOGUE_READY
{
	// whether the dialogue is not controlled, a bool (type 6). Without a HelpSystem (openblack only, no original
	// equivalent): true, nothing controls the dialogue
	const auto* helpSystem = help::Get();
	const auto* dialogue = helpSystem != nullptr ? DialogueControl(__func__) : nullptr;
	Pushb(dialogue == nullptr || help::script_control::IsSpiritReady(*dialogue, helpSystem->GetWideScreenOwner()));
}

void ChangeWeatherProperties() // 123 CHANGE_WEATHER_PROPERTIES
{
	magic::script::ChangeWeatherProperties(); // Magic/Script/CHLWeather.cpp
}

void ChangeLightningProperties() // 124 CHANGE_LIGHTNING_PROPERTIES
{
	magic::script::ChangeLightningProperties(); // Magic/Script/CHLWeather.cpp
}

void ChangeTimeFadeProperties() // 125 CHANGE_TIME_FADE_PROPERTIES
{
	magic::script::ChangeTimeFadeProperties(); // Magic/Script/CHLWeather.cpp
}

void ChangeCloudProperties() // 126 CHANGE_CLOUD_PROPERTIES
{
	magic::script::ChangeCloudProperties(); // Magic/Script/CHLWeather.cpp
}

void SetHeadingAndSpeed() // 127 SET_HEADING_AND_SPEED
{
	// const auto speed = Popf();
	// const auto position = PopVec();
	// const auto unk0 = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartGameSpeed() // 128 START_GAME_SPEED
{
	// Help/ScriptControl.cpp
	if (auto* control = ScriptControl(__func__); control != nullptr)
	{
		help::script_control::StartGameSpeed(*control, ScriptVm());
	}
}

void EndGameSpeed() // 129 END_GAME_SPEED
{
	// Help/ScriptControl.cpp
	if (auto* control = ScriptControl(__func__); control != nullptr)
	{
		help::script_control::EndGameSpeed(*control, ScriptVm());
	}
}

void BuildBuilding() // 130 BUILD_BUILDING
{
	// POP desire, then z, y, x (the first popped is the desire); the town forces the building of what it planned at the
	// map coords of the point, with desire x 5.0
	const auto desire = Popf();
	const auto position = PopVec();
	openblack::ecs::building_sites::ForceBuildingOfPlannedAtPos(openblack::map_coords::FromWorld(position), desire * 5.0f);
}

void SetAffectedByWind() // 131 SET_AFFECTED_BY_WIND
{
	// const auto object = Pop().uintVal;
	// const auto enabled = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void WidescreenTransistionFinished() // 132 WIDESCREEN_TRANSISTION_FINISHED
{
	Pushb(Locator::cinematicDirectorSystem::value().IsWideScreenTransitionFinished());
}

void GetResource() // 133 GET_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void AddResource() // 134 ADD_RESOURCE
{
	// POP the thing, POP the amount (truncated), POP the RESOURCE_TYPE (raw). No thing -> "No thing for resource", not
	// an object -> "Not object for resource", both push 0; else the object's AddResource(type, amount, not IS, not
	// poisoned, no pos) and PUSH what it took as a float. (pending) a villager's own resources in object_resources
	// (Land 1's builders, L52628..52703)
	const auto object = Pop().uintVal;
	const auto amount = static_cast<uint32_t>(openblack::map_coords::FtoL(Popf()));
	const auto type = static_cast<ResourceType>(Pop().intVal);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ADD_RESOURCE: No thing for resource");
		Pushf(0.0f);
		return;
	}
	// (approximate) every entity but a town is an object here (the towns are things with a position, not objects)
	if (registry.AllOf<openblack::ecs::components::Town>(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ADD_RESOURCE: Not object for resource");
		Pushf(0.0f);
		return;
	}
	const auto added = openblack::ecs::object_resources::AddResource(entity, type, amount);
	Pushf(static_cast<float>(added));
}

void RemoveResource() // 135 REMOVE_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto quantity = Popf();
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetTargetRelativePos() // 136 GET_TARGET_RELATIVE_POS
{
	// const auto angle = Popf();
	// const auto distance = Popf();
	// const auto to = PopVec();
	// const auto from = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void StopPointing() // 137 STOP_POINTING
{
	// POP the spirit -> it stops pointing
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritStopPointing(spirit);
	}
}

void StopLooking() // 138 STOP_LOOKING
{
	// POP the spirit -> it stops looking
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritStopLooking(spirit);
	}
}

void LookAtPosition() // 139 LOOK_AT_POSITION
{
	// POP the position, the spirit -> it looks at the position
	const auto position = PopVec();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritLookAtPosition(spirit, position);
	}
}

void PlaySpiritAnim() // 140 PLAY_SPIRIT_ANIM
{
	// POP the time, the anim (raw), y, x, the spirit; anim outside 0..80 -> "Invalid enum", then "Invalid Y" /
	// "Invalid X", none of them stops it; then the spirit plays the anim at (x, y) for that time
	const auto time = Popf();
	const auto anim = Pop().intVal;
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (anim < 0 || anim > static_cast<int32_t>(help::spirits::anim::k_Last))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "PLAY_SPIRIT_ANIM: Invalid enum");
	}
	CheckScreenFractions("PLAY_SPIRIT_ANIM", x, y);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritPlayAnim(spirit, x, y, static_cast<uint32_t>(anim), time);
	}
}

void CallInNotNear() // 141 CALL_IN_NOT_NEAR
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto pos = PopVec();
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetCameraZone() // 142 SET_CAMERA_ZONE
{
	// POP the string; the camera exclusion file reset, then ".\Data\Zones\%s" loaded and the force field on
	// (cameraZoneSystem). It limits the player's camera, which openblack does not read yet; the script camera is not
	// affected
	const auto zone = PopString();
	if (!Locator::cameraZoneSystem::has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CAMERA_ZONE {}: no camera zone service", zone);
		return;
	}
	Locator::cameraZoneSystem::value().SetZones(zone);
}

void GetObjectState() // 143 GET_OBJECT_STATE
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void RevealCountdownTimer() // 144 REVEAL_COUNTDOWN_TIMER
{
	ecs::script_countdown::SetShown(true); // the countdown timer is shown
}

void SetTimerTime() // 145 SET_TIMER_TIME
{
	// POP the time, then the thing (none -> "Object no longer valid"). A script timer starts again from this turn; a
	// spell dispenser takes it as its period; anything else is "Invalid script thing"
	const auto time = Popf();
	const auto timer = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(timer);
	if (timer == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		ObjectNoLongerValid("SET_TIMER_TIME");
		return;
	}
	if (ecs::script_timer::SetTime(entity, time))
	{
		return;
	}
	if (worship::dispenser::IsDispenser(entity))
	{
		// the seconds as whole game turns, as the game's clock counts them
		const auto turns = static_cast<uint32_t>(game_clock::TicksForSeconds(time));
		if (!Locator::magicSystem::has_value())
		{
			std::fputs("chlapi: SET_TIMER_TIME: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
			std::abort();
		}
		Locator::magicSystem::value().SetDispenserTurns(entity, turns);
		return;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TIMER_TIME: Invalid script thing");
}

void CreateTimer() // 146 CREATE_TIMER
{
	// POP the seconds; a script timer added as a thing the script created and PUSHed (0 and "Thing not created" when
	// nothing was made). (not ported) the debug name of the script that created it
	const auto timeout = Popf();
	const auto timer = ecs::script_timer::Create(timeout);
	if (timer == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_TIMER: Thing not created");
		Pusho(0);
		return;
	}
	ScriptObjects().Register(timer, true);
	Pusho(static_cast<uint32_t>(timer));
}

void GetTimerTimeRemaining() // 147 GET_TIMER_TIME_REMAINING
{
	// none -> "Object no longer valid" and 0.0; not a timer -> "Invalid script thing" and 0.0; else the seconds left of
	// the game turns, 0 once out
	const auto timer = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(timer);
	if (timer == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		ObjectNoLongerValid("GET_TIMER_TIME_REMAINING");
		Pushf(0.0f);
		return;
	}
	const auto remaining = ecs::script_timer::Remaining(entity);
	if (!remaining)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TIMER_TIME_REMAINING: Invalid script thing");
	}
	Pushf(remaining.value_or(0.0f));
}

void GetTimerTimeSinceSet() // 148 GET_TIMER_TIME_SINCE_SET
{
	// none or not a timer -> the error and FLT_MAX; else the seconds since it was set
	const auto timer = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(timer);
	if (timer == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		ObjectNoLongerValid("GET_TIMER_TIME_SINCE_SET");
		Pushf(std::numeric_limits<float>::max());
		return;
	}
	const auto since = ecs::script_timer::SinceSet(entity);
	if (!since)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TIMER_TIME_SINCE_SET: Invalid script thing");
	}
	Pushf(since.value_or(std::numeric_limits<float>::max()));
}

void MoveMusic() // 149 MOVE_MUSIC
{
	// "to" is popped first, then "from"; both must be valid
	const auto to = MusicThing(Pop().uintVal);
	const auto from = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && from && to)
	{
		gameMusic->MoveThingMusic(*from, *to);
	}
}

void GetInclusionDistance() // 150 GET_INCLUSION_DISTANCE
{
	// the camera's inclusion distance as a float. (approximate) the player camera's update, which writes it, is not
	// ported: it stays at its start value FLT_MAX
	Pushf(player_camera::Get().inclusionDistance);
}

void GetLandHeight() // 151 GET_LAND_HEIGHT
{
	const auto position = PopVec();

	// -10 over the sea (altitude 0), off the map or without a block
	const auto& island = Locator::terrainSystem::value();
	Pushf(openblack::ecs::sea_cells::ScriptLandHeight(island, position));
}

void LoadMap() // 152 LOAD_MAP
{
	// the land's path; the land changes inside the call, and the calling script goes on, on the new land
	const auto path = PopString();
	ecs::events::Publish(ecs::events::LandChangeRequested {.path = path});
}

void StopAllScriptsExcluding() // 153 STOP_ALL_SCRIPTS_EXCLUDING
{
	const auto scriptNames = PopString();

	const auto names = GetUniqueWords(scriptNames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&names](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return !names.contains(name);
	});
}

void StopAllScriptsInFilesExcluding() // 154 STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto sourceFilenames = PopString();

	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&sourceFilenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return !script::name_lists::HoldsFile(sourceFilenames, filename);
	});
}

void StopScript() // 155 STOP_SCRIPT
{
	const auto scriptName = PopString();
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&scriptName](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return name == scriptName;
	});
}

void ClearClickedObject() // 156 CLEAR_CLICKED_OBJECT
{
	// the clicked object is cleared
	if (auto* clicked = LocalHandClicked())
	{
		hand_click::ClearThing(*clicked);
	}
}

void ClearClickedPosition() // 157 CLEAR_CLICKED_POSITION
{
	// the clicked position is cleared (its turn stays)
	if (auto* clicked = LocalHandClicked())
	{
		hand_click::ClearPlace(*clicked);
	}
}

void PositionClicked() // 158 POSITION_CLICKED
{
	// the radius, then the position; (pending) a multiplayer game logs "This is not multiplayer friendly yet!" and
	// pushes true. Then whether the last clicked land point is within the radius
	const auto radius = Popf();
	const auto position = PopVec();
	const auto* clicked = LocalHandClicked();
	// Along the ground: the position's height doesn't count
	const map_coords::MapCoords at {.x = map_coords::ToFixed(position.x), .z = map_coords::ToFixed(position.z)};
	Pushb(clicked != nullptr && hand_click::IsPlaceClicked(*clicked, at, radius));
}

void ReleaseFromScript() // 159 RELEASE_FROM_SCRIPT
{
	// none -> "Thing not valid". Controlled by a script -> released into the game: no longer controlled by the script,
	// never deleted, its music removed, then by SCRIPT_OBJECT_TYPE (37: nothing more)
	const auto object = Pop().uintVal;
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "RELEASE_FROM_SCRIPT: Thing not valid");
		return;
	}
	// (a highlight: DidYouKnow (CHL L2610..) releases its sign at once and it stays when the task ends; the containers'
	// members, the villagers and the animals by their types: ECS/ScriptHeld.h)
	ScriptObjects().ReleaseFromScript(thing);
}

void GetObjectHandIsOver() // 160 GET_OBJECT_HAND_IS_OVER
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void IdPoisonedSize() // 161 ID_POISONED_SIZE
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void IsPoisoned() // 162 IS_POISONED
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void CallPoisonedIn() // 163 CALL_POISONED_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void CallNotPoisonedIn() // 164 CALL_NOT_POISONED_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SpiritPlayed() // 165 SPIRIT_PLAYED
{
	// POP the spirit; push whether it is not playing an anim, a bool (type 6). Without the control (openblack only):
	// true, nothing plays
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	const auto* control = SpiritControl();
	Pushb(control == nullptr || !control->SpiritPlayingAnim(spirit));
}

void ClingSpirit() // 166 CLING_SPIRIT
{
	// POP y, x, the spirit; "Invalid Y" / "Invalid X" (not stopping); the spirit clings at (x, y)
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions("CLING_SPIRIT", x, y);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritCling(spirit, x, y);
	}
}

void FlySpirit() // 167 FLY_SPIRIT
{
	// POP y, x, the spirit; "Invalid Y" / "Invalid X" (not stopping); the spirit flies to (x, y)
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions("FLY_SPIRIT", x, y);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritFly(spirit, x, y);
	}
}

void SetIdMoveable() // 168 SET_ID_MOVEABLE
{
	// the object, then the bool; no object: "Thing not valid"; else its not-moveable flag = !moveable
	const auto object = Pop().uintVal;
	const auto moveable = Pop().intVal != 0;
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ID_MOVEABLE: Thing not valid");
		return;
	}
	ecs::object_flags::SetMoveable(entity, moveable);
}

void SetIdPickupable() // 169 SET_ID_PICKUPABLE
{
	// as 168, with its not-pickupable flag = !pickupable
	const auto object = Pop().uintVal;
	const auto pickupable = Pop().intVal != 0;
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ID_PICKUPABLE: Thing not valid");
		return;
	}
	ecs::object_flags::SetPickupable(entity, pickupable);
}

void IsOnFire() // 170 IS_ON_FIRE
{
	magic::script::IsOnFire(); // Magic/Script/CHLFire.cpp
}

void IsFireNear() // 171 IS_FIRE_NEAR
{
	magic::script::IsFireNear(); // Magic/Script/CHLFire.cpp
}

void StopScriptsInFiles() // 172 STOP_SCRIPTS_IN_FILES
{
	const auto sourceFilenames = PopString();

	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&filenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return filenames.contains(filename);
	});
}

void SetPoisoned() // 173 SET_POISONED
{
	// const auto obj = Pop().uintVal;
	// const auto poisoned = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetTemperature() // 174 SET_TEMPERATURE
{
	magic::script::SetTemperature(); // Magic/Script/CHLFire.cpp
}

void SetOnFire() // 175 SET_ON_FIRE
{
	magic::script::SetOnFire(); // Magic/Script/CHLFire.cpp
}

void SetTarget() // 176 SET_TARGET
{
	// const auto time = Popf();
	// const auto position = PopVec();
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void WalkPath() // 177 WALK_PATH
{
	const auto valTo = Popf();
	const auto valFrom = Popf();
	const auto cameraEnum = Pop().intVal;
	const auto forward = Pop().intVal != 0;
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || !registry.AllOf<ecs::components::Transform>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "WALK_PATH: Thing not valid");
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		// a living thing -> it walks the track in IN_SCRIPT 4 from, to, forward (ECS/LivingWalkPath.h)
		const bool started = ecs::living::StartWalkPath(object, cameraEnum, VillagerStates::InScript, valFrom, valTo, forward);
		if (ScriptThingTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "WALK_PATH {} track {} from {} to {} forward {} -> {}",
			                   static_cast<uint32_t>(object), cameraEnum, valFrom, valTo, forward, started);
		}
		return;
	}
	if (registry.AnyOf<ecs::components::Animal, ecs::components::Creature>(object))
	{
		// (pending) an animal's or a creature's state 28 (the same living walk)
		NotImplemented();
		return;
	}
	if (!registry.AnyOf<ecs::components::MobileObject, ecs::components::Shark>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "WALK_PATH: Thing is invalid for move path");
		return;
	}
	static_cast<void>(Locator::walkPathSystem::value().Start(object, cameraEnum, forward, valFrom, valTo));
}

void FocusAndPositionFollow() // 178 FOCUS_AND_POSITION_FOLLOW
{
	// POP the distance, then the thing: as POSITION_FOLLOW with that distance and the heading kept (no "behind" reset);
	// the focus follows the same thing unless FOCUS_FOLLOW gave another
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusAndPositionFollow(*thing, distance);
	}
}

void GetWalkPathPercentage() // 179 GET_WALK_PATH_PERCENTAGE
{
	// 1.0 for anything but a living thing (a shark's path is not read)
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(object) &&
	    registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(object))
	{
		// the walk's current / duration (ECS/LivingWalkPath.h). (openblack) 0 without a path: the original reads through
		// null
		Pushf(ecs::living::GetWalkPathPercentage(object).value_or(0.0f));
		return;
	}
	Pushf(1.0f);
}

void CameraProperties() // 180 CAMERA_PROPERTIES
{
	// POP behind (raw, tested != 0), the angle (degrees, to radians), the speed, the distance; with the script mode the
	// follow's distance, speed (its time factor: 0 places at once), behind and angle are set
	const bool behind = Pop().uintVal != 0;
	const auto angle = Popf() * script_camera::k_DegreesToRadians;
	const auto speed = Popf();
	const auto distance = Popf();
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetFollowProperties(distance, speed, angle, behind);
	}
}

void EnableDisableMusic() // 181 ENABLE_DISABLE_MUSIC
{
	// the thing is popped first, then the switch (stored as it is)
	const auto object = MusicThing(Pop().uintVal);
	const auto enable = Pop().intVal;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->EnableThingMusic(*object, enable);
	}
}

void GetMusicObjDistance() // 182 GET_MUSIC_OBJ_DISTANCE
{
	// 0 for an invalid thing, else its music's distance
	const auto source = MusicThing(Pop().uintVal);
	float distance = 0.0f;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && source)
	{
		distance = gameMusic->ThingMusicDistance(*source);
	}
	Pushf(distance);
}

void GetMusicEnumDistance() // 183 GET_MUSIC_ENUM_DISTANCE
{
	// with an invalid type the original pushes twice (0, then the play distance)
	const auto type = Pop().intVal;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		for (const float value : gameMusic->ScriptMusicTypeDistances(type))
		{
			Pushf(value);
		}
	}
	else
	{
		Pushf(0.0f); // (not in the original: no GameMusic before game_music::Start; one value keeps the stack)
	}
}

void SetMusicPlayPosition() // 184 SET_MUSIC_PLAY_POSITION
{
	// z, y and x are popped first, then the thing
	const auto position = PopVec();
	const auto object = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->SetPlayPosition(*object, position);
	}
}

void AttachObjectLeashToObject() // 185 ATTACH_OBJECT_LEASH_TO_OBJECT
{
	// two pops: the thing to tie to, then the creature; either order works (Creature/LeashScript.h)
	const auto thing = Pop().uintVal;
	const auto creature = Pop().uintVal;
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::AttachToThing(Locator::leashSystem::value(), LeashThing(creature), LeashThing(thing),
		                                      IsLeashCreature);
	}
}

void AttachObjectLeashToHand() // 186 ATTACH_OBJECT_LEASH_TO_HAND
{
	const auto creature = Pop().uintVal;
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::AttachToHand(Locator::leashSystem::value(), LeashCreature(creature));
	}
}

void DetachObjectLeash() // 187 DETACH_OBJECT_LEASH
{
	const auto creature = Pop().uintVal;
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::Detach(Locator::leashSystem::value(), LeashCreature(creature));
	}
}

void SetCreatureOnlyDesire() // 188 SET_CREATURE_ONLY_DESIRE
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCreatureOnlyDesireOff() // 189 SET_CREATURE_ONLY_DESIRE_OFF
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void RestartMusic() // 190 RESTART_MUSIC
{
	// the thing's music is restarted
	const auto object = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->RestartThingMusic(*object);
	}
}

void MusicPlayed191() // 191 MUSIC_PLAYED
{
	// true for an invalid thing, else whether its music is finished (a bool, type 6)
	const auto object = MusicThing(Pop().uintVal);
	bool finished = true;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		finished = gameMusic->IsThingMusicFinished(*object) != 0;
	}
	Pushb(finished);
}

void IsOfType() // 192 IS_OF_TYPE
{
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void ClearHitObject() // 193 CLEAR_HIT_OBJECT
{
	// the physics forgets the last thing hit and what hit it
	Dynamics().RecordHit(entt::null, entt::null);
}

void GameThingHit() // 194 GAME_THING_HIT
{
	// pops the object: whether it is the thing the physics last recorded as hit. An object gone is false, with "Object
	// no longer valid" (a message the shipped game prints nothing for)
	const auto object = Pop().uintVal;
	const auto thing = ScriptThing(object, "GAME_THING_HIT", "Object no longer valid");
	Pushb(thing.has_value() && *thing == Dynamics().GetHitObject());
}

void SpellAtThing() // 195 SPELL_AT_THING
{
	magic::script::SpellAtThing(); // Magic/Script/CHLSpells.cpp
}

void SpellAtPos() // 196 SPELL_AT_POS
{
	magic::script::SpellAtPos(); // Magic/Script/CHLSpells.cpp
}

void CallPlayerCreature() // 197 CALL_PLAYER_CREATURE
{
	// the player's primary creature, the one they lead, handed to the script as a found thing; none is 0
	const auto scriptPlayer = Popf();
	PlayerNames player {};
	std::optional<entt::entity> creature;
	if (Locator::playerSystem::has_value() && magic::ScriptPlayerToGamePlayer(map_coords::FtoL(scriptPlayer), player))
	{
		creature = Locator::playerSystem::value().GetPrimaryCreature(player);
	}
	if (!creature.has_value())
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "CALL_PLAYER_CREATURE: no creature of player {}", scriptPlayer);
		Pusho(0);
		return;
	}
	ScriptObjects().Register(*creature, false);
	Pusho(static_cast<uint32_t>(*creature));
}

void GetSlowestSpeed() // 198 GET_SLOWEST_SPEED
{
	// const auto flock = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetObjectHeld199() // 199 GET_OBJECT_HELD
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void HelpSystemOn() // 200 HELP_SYSTEM_ON
{
	// whether the help system is on, a bool (type 6). Without a HelpSystem (openblack only: the original always has
	// one): false
	const auto* helpSystem = help::Get();
	Pushb(helpSystem != nullptr && helpSystem->IsHelpSystemOn());
}

void ShakeCamera() // 201 SHAKE_CAMERA
{
	// POP the seconds, the amplitude, the radius and the point (z, y, x) -> a camera shake: the drawn camera shakes
	// while it is within the radius of the point, less and less (Camera/CameraShake.h)
	const auto seconds = Popf();
	const auto amplitude = Popf();
	const auto radius = Popf();
	const auto position = PopVec();
	camera_shake::StartCameraShake(position, radius, amplitude, seconds);
}

void SetAnimationModify() // 202 SET_ANIMATION_MODIFY
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetAviSequence() // 203 SET_AVI_SEQUENCE
{
	// POP the sequence (raw, compared with 1 and 2), then on (raw, != 0) -> the film started or stopped. It returns at
	// once
	const auto sequence = Pop().intVal;
	const bool on = Pop().uintVal != 0;
	if (!on)
	{
		// stop: 2 -> the falling spell video ends; anything else nothing
		if (sequence == 2)
		{
			video::GetFallingSpell().End(); // nothing without FallingSpellVideo
		}
		return;
	}
	if (sequence == 1)
	{
		// start 1: "data\intro.bik" full screen (the tip video cleared, the game paused and its pause kept, the wide screen
		// on; the player object is made even when the file does not open), then the film's first 60 s: the pause given back
		// after 58 s and the picture faded out up to 60 s; and the black fade gone at once.
		// The film is played by video:: (Video/VideoPlayer.h); the player object exists even when the file does not open,
		// so the fade is always cleared
		auto& player = video::Get();
		const auto& fileSystem = Locator::filesystem::value();
		player.Play(fileSystem.FindPath("Data/intro.bik"));
		player.ScheduleIntro();
		Locator::cinematicDirectorSystem::value().FadeBackToNormal(0.0f);
		return;
	}
	if (sequence == 2)
	{
		// start 2: the falling spell video (Video/FallingSpellVideo.h: nothing without the local player's creature; else mode
		// 2, "data\spells\fall\fall.bik" full screen with no fade of its own, the falling spell updated each frame) and the
		// black fade gone at once, also without a creature
		video::GetFallingSpell().KickOff();
		Locator::cinematicDirectorSystem::value().FadeBackToNormal(0.0f);
	}
}

void PlayGesture() // 204 PLAY_GESTURE
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void DevFunction() // 205 DEV_FUNCTION
{
	const auto function = Pop().intVal;
	if (!Locator::leashSystem::has_value() ||
	    !ecs::player_creature::DevFunction(Locator::leashSystem::value(), function, PlayerNames::PLAYER_ONE))
	{
		NotImplemented(function);
	}
}

void HasMouseWheel() // 206 HAS_MOUSE_WHEEL
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void NumMouseButtons() // 207 NUM_MOUSE_BUTTONS
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetCreatureDevStage() // 208 SET_CREATURE_DEV_STAGE
{
	const auto stage = Pop().intVal;
	const auto object = Pop().uintVal;
	if (const auto thing = ScriptThing(object, "SET_CREATURE_DEV_STAGE", "Thing not found!"))
	{
		ecs::player_creature::SetDevelopmentStage(Locator::entitiesRegistry::value(), *thing, stage);
	}
}

void SetFixedCamRotation() // 209 SET_FIXED_CAM_ROTATION
{
	// POP the point (z, y, x) then "on"; only with the player's camera mode current ("Wrong camera mode" otherwise): the
	// player's camera then turns about that point, or no longer (Camera/PlayerCameraScript.h; not read by
	// DefaultWorldCameraModel yet)
	const auto point = PopVec();
	const bool on = Pop().uintVal != 0;
	if (script_camera::HasMode())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Wrong camera mode", __func__);
		return;
	}
	player_camera::ForceRotateAboutPoint(on ? std::optional<glm::vec3>(point) : std::nullopt);
}

void SwapCreature() // 210 SWAP_CREATURE
{
	// const auto toCreature = Pop().uintVal;
	// const auto fromCreature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetArena() // 211 GET_ARENA
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void GetFootballPitch() // 212 GET_FOOTBALL_PITCH
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void StopAllGames() // 213 STOP_ALL_GAMES
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void AttachToGame() // 214 ATTACH_TO_GAME
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void DetachFromGame() // 215 DETACH_FROM_GAME
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void DetachUndefinedFromGame() // 216 DETACH_UNDEFINED_FROM_GAME
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetOnlyForScripts() // 217 SET_ONLY_FOR_SCRIPTS
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartMatchWithReferee() // 218 START_MATCH_WITH_REFEREE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameTeamSize() // 219 GAME_TEAM_SIZE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameType() // 220 GAME_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void GameSubType() // 221 GAME_SUB_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void IsLeashed() // 222 IS_LEASHED
{
	const auto object = Pop().uintVal;
	Pushb(Locator::leashSystem::has_value() &&
	      creature_leash::script::IsLeashed(Locator::leashSystem::value(), LeashCreature(object)));
}

void SetCreatureHome() // 223 SET_CREATURE_HOME
{
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	const auto thing = ScriptThing(object, "SET_CREATURE_HOME", "Thing not found!");
	if (thing.has_value() && Locator::leashSystem::has_value())
	{
		ecs::player_creature::SetHome(Locator::leashSystem::value(), Locator::entitiesRegistry::value(), *thing,
		                              OnGround(ecs::player_creature::HomeOnGround(position, 0.0f)));
	}
}

namespace
{
/// An object of the physics' hit record as a script is given it: none is 0, else it is added as a found script thing.
/// It is read back as kept, with no second test that it is still there
void PushHitRecord(entt::entity object)
{
	if (object == entt::null)
	{
		Pusho(0);
		return;
	}
	ScriptObjects().Register(object, false);
	Pusho(static_cast<uint32_t>(object));
}
} // namespace

void GetHitObject() // 224 GET_HIT_OBJECT
{
	// the last thing a body in the physics hit
	PushHitRecord(Dynamics().GetHitObject());
}

void GetObjectWhichHit() // 225 GET_OBJECT_WHICH_HIT
{
	// what hit it
	PushHitRecord(Dynamics().GetObjectWhichHit());
}

void GetNearestTownOfPlayer() // 226 GET_NEAREST_TOWN_OF_PLAYER
{
	// five POPs: the radius, the player (a float, truncated), then the position's z, y and x; the script player to the
	// game's player, the map coords of the point, and the nearest of only that player's towns within r. None: "Did not
	// find town" and 0; else the town, added as a found script thing
	const auto radius = Popf();
	const auto scriptPlayer = Popf();
	const auto position = PopVec();
	PlayerNames player = PlayerNames::NEUTRAL;
	// (openblack) a player out of 0..7 has no player (the original reads through null): no town. Map coords x, z only
	// (FromMetres: the altitude is not read)
	const auto town =
	    magic::ScriptPlayerToGamePlayer(map_coords::FtoL(scriptPlayer), player)
	        ? ecs::map_cells::FindPlayerTownAtPos(map_coords::FromMetres(glm::vec2(position.x, position.z)), radius, player)
	        : entt::entity {entt::null};
	if (town == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_NEAREST_TOWN_OF_PLAYER: Did not find town");
		Pusho(0);
		return;
	}
	ScriptObjects().Register(town, false);
	Pusho(static_cast<uint32_t>(town));
}

void SpellAtPoint() // 227 SPELL_AT_POINT
{
	magic::script::SpellAtPoint(); // Magic/Script/CHLSpells.cpp
}

void SetAttackOwnTown() // 228 SET_ATTACK_OWN_TOWN
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void IsFighting() // 229 IS_FIGHTING
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetMagicRadius() // 230 SET_MAGIC_RADIUS
{
	// const auto radius = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void TempTextWithNumber() // 231 TEMP_TEXT_WITH_NUMBER
{
	const auto withInteraction = Pop().intVal;
	const auto value = Popf();
	const auto format = PopString();
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* dialogue = TheHelpTextSystem().GetDialogue(); dialogue != nullptr)
	{
		dialogue->TempTextWithNumber(singleLine, WidenScriptString(format), value, withInteraction);
	}
}

void RunTextWithNumber() // 232 RUN_TEXT_WITH_NUMBER
{
	const auto withInteraction = Pop().intVal;
	const auto number = Popf();
	const auto textID = static_cast<uint32_t>(Pop().intVal);
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* dialogue = TheHelpTextSystem().GetDialogue(); dialogue != nullptr)
	{
		dialogue->RunTextWithNumber(singleLine, textID, number, withInteraction);
	}
}

void CreatureSpellReversion() // 233 CREATURE_SPELL_REVERSION
{
	// POP the creature, then the flag: whether the spells on it put it back as it was once they wear off. No thing:
	// "Thing not found!"; anything but a creature: "Thing not creature!" and nothing changed
	const auto object = Pop().uintVal;
	const bool reversion = Pop().intVal != 0;
	if (const auto thing = ScriptThing(object, "CREATURE_SPELL_REVERSION", "Thing not found!"))
	{
		if (!magic::spell_creature::SetReversion(*thing, reversion))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATURE_SPELL_REVERSION: Thing not creature!");
		}
	}
}

void GetDesire() // 234 GET_DESIRE
{
	// ecs::town_desire::ScriptGetDesire: POP the desire; out of [0, 17) -> "Invalid desire" and PUSH 0 without the
	// second POP (literal: the object stays on the stack); else POP the object, PUSH the town's raw desire (0 if it is
	// not a town)
	const auto desire = Pop().intVal;
	std::vector<std::string> errors;
	const float value = ecs::town_desire::ScriptGetDesire(
	    desire, [] { return static_cast<entt::entity>(Pop().uintVal); }, &errors);
	for (const auto& error : errors)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_DESIRE: {}", error);
	}
	Pushf(value);
}

void GetEventsPerSecond() // 235 GET_EVENTS_PER_SECOND
{
	// POP the HELP_EVENT_TYPE (1..48, else "Invalid event" and 0.0); the triggers per second of its help accumulator
	// (Help/HelpProfile.h)
	const auto type = Pop().intVal;
	const auto value = help_profile::EventsPerSecond(type);
	if (!value)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_EVENTS_PER_SECOND: Invalid event {}", type);
	}
	Pushf(value.value_or(0.0f));
}

void GetTimeSince() // 236 GET_TIME_SINCE
{
	// as 235, the time since the event was last used
	const auto type = Pop().intVal;
	const auto value = help_profile::TimeSince(type);
	if (!value)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TIME_SINCE: Invalid event {}", type);
	}
	Pushf(value.value_or(0.0f));
}

void GetTotalEvents() // 237 GET_TOTAL_EVENTS
{
	// as 235, the accumulator's count: the turns in which the event happened (TeachRotate 25, TeachPitch 28, TeachZoom /
	// TrackZoomUsage 29, DoubleClicking 30 / 31)
	const auto type = Pop().intVal;
	const auto value = help_profile::TotalEvents(type);
	if (!value)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TOTAL_EVENTS: Invalid event {}", type);
	}
	Pushf(value.value_or(0.0f));
}

void UpdateSnapshot() // 238 UPDATE_SNAPSHOT
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Update),
	                    script::challenge_snapshots::Call::Update);
}

/// The reward types the scripts may give
constexpr int32_t k_LastRewardType = 60;

/// A reward chest of a kind for the player at this computer, given to a town or none: on the land, or from the sky
uint32_t GiveReward(int32_t type, glm::vec3 position, entt::entity town, bool fromSky)
{
	if (type < 1 || type > k_LastRewardType)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_REWARD: invalid type {}", type);
		return 0;
	}
	if (!Locator::rewardSystem::has_value())
	{
		return 0;
	}
	const auto player =
	    Locator::playerSystem::has_value() ? std::optional(Locator::playerSystem::value().GetLocalPlayer()) : std::nullopt;
	const auto chest =
	    Locator::rewardSystem::value().Create(position, static_cast<RewardObjectInfo>(type), player, town, fromSky);
	// added as a thing the script created
	ScriptObjects().Register(chest, true);
	return static_cast<uint32_t>(chest);
}

void CreateReward() // 239 CREATE_REWARD
{
	const auto fromSky = Pop().intVal != 0;
	const auto position = PopVec();
	const auto type = Pop().intVal;
	Pusho(GiveReward(type, position, entt::null, fromSky));
}

void CreateRewardInTown() // 240 CREATE_REWARD_IN_TOWN
{
	const auto fromSky = Pop().intVal != 0;
	const auto position = PopVec();
	const auto town = static_cast<entt::entity>(Pop().uintVal);
	const auto type = Pop().intVal;
	// The town must be one. (pending) the original names the town as the script's creation here, not the chest
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(town) || !registry.AllOf<ecs::components::Town>(town))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_REWARD_IN_TOWN: not a town");
		Pusho(0);
		return;
	}
	Pusho(GiveReward(type, position, town, fromSky));
}

void SetFade() // 241 SET_FADE
{
	// a screen fade to the colour (every argument truncated; colour 0..255, time in seconds)
	const auto time = Popf();
	const auto blue = Popf();
	const auto green = Popf();
	const auto red = Popf();
	const auto channel = [](float value) { return static_cast<uint8_t>(static_cast<int>(value)); };
	Locator::cinematicDirectorSystem::value().FadeTo(channel(red), channel(green), channel(blue), time);
}

void SetFadeIn() // 242 SET_FADE_IN
{
	// the screen fades back to normal
	const auto duration = Popf();
	Locator::cinematicDirectorSystem::value().FadeBackToNormal(duration);
}

void FadeFinished() // 243 FADE_FINISHED
{
	// no fade in progress
	Pushb(Locator::cinematicDirectorSystem::value().IsFadeFinished());
}

void SetPlayerMagic() // 244 SET_PLAYER_MAGIC
{
	magic::script::SetPlayerMagic(); // Magic/Script/CHLSpells.cpp
}

void HasPlayerMagic() // 245 HAS_PLAYER_MAGIC
{
	magic::script::HasPlayerMagic(); // Magic/Script/CHLSpells.cpp
}

void SpiritSpeaks() // 246 SPIRIT_SPEAKS
{
	// POP the text, then the SCRIPT_SPIRIT_TYPE; the help spirit of it (the local player's alignment: inferred,
	// openblack's local player is PLAYER_ONE; the random draw uses game_random's local stream); text 0 past 6974; push
	// whether the spirit who talks the text is this one (type 6)
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	Pushb(help::script_spirits::SpiritWhoTalks(TheHelpTextSystem().GetNarrator(text)) == spirit);
}

void BeliefForPlayer() // 247 BELIEF_FOR_PLAYER
{
	// the player (script player 0 is the neutral slot, n is game player n - 1), then the thing; none of either gives 0
	const auto scriptPlayer = map_coords::FtoL(Popf());
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	PlayerNames player {};
	if (object == 0 || !registry.Valid(entity) || !magic::ScriptPlayerToGamePlayer(scriptPlayer, player))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "BELIEF_FOR_PLAYER: no thing or no player");
		Pushf(0.0f);
		return;
	}
	// a town: its belief in the player
	if (registry.AllOf<ecs::components::Town>(entity))
	{
		Pushf(script::property_rules::BeliefForPlayer(true, ecs::town_belief::GetBeliefInPlayer(entity, player), std::nullopt,
		                                              player));
		return;
	}
	// anything else believes wholly in its own player: a villager's is its town's (none without a town), a creature's
	// its owner
	std::optional<PlayerNames> owner;
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		owner = ecs::villager::GetPlayerOf(entity);
	}
	else if (const auto* creature = registry.TryGet<const ecs::components::Creature>(entity))
	{
		owner = creature->owner;
	}
	else
	{
		// (pending) the other kinds' own player
		NotImplemented();
		Pushf(0.0f);
		return;
	}
	Pushf(script::property_rules::BeliefForPlayer(false, std::nullopt, owner, player));
}

void GetHelp() // 248 GET_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetLeashWorks() // 249 SET_LEASH_WORKS
{
	// two pops: the creature, then the value, taken as it is (set when not zero)
	const auto creature = Pop().uintVal;
	const auto value = Pop().intVal;
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::SetWorks(Locator::leashSystem::value(), LeashCreature(creature), value);
	}
}

void LoadMyCreature() // 250 LOAD_MY_CREATURE
{
	// the profile's creature, at the map cell of the point (its height is not used); nothing when the player has one
	const auto position = PopVec();
	ecs::player_creature::LoadMyCreature(glm::vec2(position.x, position.z));
}

void ObjectRelativeBelief() // 251 OBJECT_RELATIVE_BELIEF
{
	// const auto belief = Popf();
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreateWithAngleAndScale() // 252 CREATE_WITH_ANGLE_AND_SCALE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);
	const auto scale = Popf();
	const auto angle = Popf();

	// the angle is in degrees
	const entt::entity object = type > ObjectType::None && type <= ObjectType::AnimatedStatic
	                                ? CreateScriptObject(type, subtype, position, glm::radians(angle), scale)
	                                : entt::null;
	// added as a thing the script created
	ScriptObjects().Register(object, true);

	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void SetHelpSystem() // 253 SET_HELP_SYSTEM
{
	// the help system's on flag = the popped value as it is
	const auto on = Pop().intVal;
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->SetHelpOn(static_cast<uint32_t>(on));
	}
}

void SetVirtualInfluence() // 254 SET_VIRTUAL_INFLUENCE
{
	// the player, then on or off: any value but 0 is on. Off, the player's hand loses what it kept of their influence
	// past the border
	const auto scriptPlayer = Popf();
	const bool enabled = Pop().intVal != 0;
	PlayerNames player {};
	if (Locator::influenceSystem::has_value() && magic::ScriptPlayerToGamePlayer(map_coords::FtoL(scriptPlayer), player))
	{
		Locator::influenceSystem::value().SetVirtualInfluence(player, enabled);
	}
}

void SetActive() // 255 SET_ACTIVE
{
	const auto object = Pop().uintVal;
	const auto active = static_cast<bool>(Pop().intVal);
	// a script highlight first -> activated or not
	if (object != 0 && ecs::script_highlight::IsHighlight(static_cast<entt::entity>(object)))
	{
		ecs::script_highlight::SetActivated(static_cast<entt::entity>(object), active);
		return;
	}
	// a spell dispenser, through the miracles
	if (object != 0 && worship::dispenser::IsDispenser(static_cast<entt::entity>(object)))
	{
		if (!Locator::magicSystem::has_value())
		{
			std::fputs("chlapi: SET_ACTIVE: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
			std::abort();
		}
		Locator::magicSystem::value().SetDispenserActive(static_cast<entt::entity>(object), active);
		return;
	}
	// a scaffold: active -> its building is forced; inactive: nothing
	if (object != 0 && ecs::scaffolds::IsScaffold(static_cast<entt::entity>(object)))
	{
		if (active)
		{
			ecs::scaffolds::ForceBuildBuilding(static_cast<entt::entity>(object), std::nullopt);
		}
		return;
	}
	// anything else, or no thing: only an error, as the original
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(static_cast<entt::entity>(object)))
	{
		ObjectNoLongerValid("SET_ACTIVE");
		return;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ACTIVE: Invalid object type");
}

void ThingValid() // 256 THING_VALID
{
	const auto objId = Pop().uintVal;
	// TODO(Daniels118): is this the right way?
	bool valid = false;
	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		valid = registry.Valid(static_cast<entt::entity>(objId));
	}
	Pushb(valid);
}

void VortexFadeOut() // 257 VORTEX_FADE_OUT
{
	// the thing; a vortex -> it starts fading out, else "Thing not vortex" / "vortex fade out failed" (the original's
	// script error message is empty in the shipped game)
	const auto vortex = Pop().uintVal;
	if (const auto thing = ScriptThing(vortex, "VORTEX_FADE_OUT", "vortex fade out failed"))
	{
		TheVortexSystem().StartFadeOut(*thing); // nothing for a thing that is not a vortex
	}
}

void RemoveReactionOfType() // 258 REMOVE_REACTION_OF_TYPE
{
	// const auto reaction = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureLearnEverythingExcluding() // 259 CREATURE_LEARN_EVERYTHING_EXCLUDING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void PlayedPercentage() // 260 PLAYED_PERCENTAGE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void ObjectCastByObject() // 261 OBJECT_CAST_BY_OBJECT
{
	// const auto caster = Pop().uintVal;
	// const auto spellInstance = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void IsWindMagicAtPos() // 262 IS_WIND_MAGIC_AT_POS
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void CreateMist() // 263 CREATE_MIST
{
	// const auto heightRatio = Popf();
	// const auto transparency = Popf();
	// const auto b = Popf();
	// const auto g = Popf();
	// const auto r = Popf();
	// const auto scale = Popf();
	// const auto pos = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetMistFade() // 264 SET_MIST_FADE
{
	// const auto duration = Popf();
	// const auto endTransparency = Popf();
	// const auto startTransparency = Popf();
	// const auto endScale = Popf();
	// const auto startScale = Popf();
	// const auto mist = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetObjectFade() // 265 GET_OBJECT_FADE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void PlayHandDemo() // 266 PLAY_HAND_DEMO
{
	// pops withoutHandModify, then the wait-for-trigger flag, then the demo's name (Input/HandDemo.h: its playback for
	// the running task)
	const auto withoutHandModify = Pop().intVal != 0;
	const auto waitTrigger = Pop().intVal != 0;
	const auto name = PopString();
	hand_demo::Play(name, Locator::vm::value().GetCurrentTaskNumber(), waitTrigger, withoutHandModify);
}

void IsPlayingHandDemo() // 267 IS_PLAYING_HAND_DEMO
{
	// the negation of whether the demo plays back, so the scripts' `CALL 267; JZ loop` wait until the demo has finished
	Pushb(!hand_demo::IsPlaying(0));
}

void GetArsePosition() // 268 GET_ARSE_POSITION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void IsLeashedToObject() // 269 IS_LEASHED_TO_OBJECT
{
	// two pops: the thing, then the creature; either order works, as for ATTACH_OBJECT_LEASH_TO_OBJECT
	const auto target = Pop().uintVal;
	const auto object = Pop().uintVal;
	Pushb(Locator::leashSystem::has_value() &&
	      creature_leash::script::IsLeashedToThing(Locator::leashSystem::value(), LeashThing(object), LeashThing(target),
	                                               IsLeashCreature));
}

void GetInteractionMagnitude() // 270 GET_INTERACTION_MAGNITUDE
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void IsCreatureAvailable() // 271 IS_CREATURE_AVAILABLE
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void CreateHighlight() // 272 CREATE_HIGHLIGHT
{
	// pops the challenge id, the point (z, y, x) and the script highlight info row
	const auto challenge = Pop().uintVal;
	const auto position = PopVec();
	const auto row = Pop().uintVal;
	// a highlight at the map coords of the point, with the info, the challenge, 0.0 and 1.0
	const auto thing = ecs::script_highlight::Create(position, row, challenge, 0.0f, 1.0f);
	if (thing == entt::null)
	{
		// "Highlight not created" and 0. (openblack) also for a row past the four
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATE_HIGHLIGHT: Highlight not created (row {})", row);
		Pusho(0);
		return;
	}
	ScriptObjects().Register(thing, true); // a thing the script created
	Pusho(static_cast<uint32_t>(thing));
	// (pending) the debug name of the script that created it
}

void GetObjectHeld273() // 273 GET_OBJECT_HELD
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void GetActionCount() // 274 GET_ACTION_COUNT
{
	// const auto creature = Pop().uintVal;
	// const auto action = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GetObjectLeashType() // 275 GET_OBJECT_LEASH_TYPE
{
	// the leash picked at the temple of the creature's player, as the scripts number it, -1 for none; 0 when the thing
	// is no creature or its player has no temple heart
	const auto object = Pop().uintVal;
	const auto creature = LeashCreature(object);
	const auto pick = creature.has_value()
	                      ? worship::temple_leash::PickOf(
	                            Locator::entitiesRegistry::value().Get<const ecs::components::Creature>(*creature).owner)
	                      : std::nullopt;
	Pushi(worship::leash_posts::ScriptLeashType(pick));
}

void SetFocusFollow() // 276 SET_FOCUS_FOLLOW
{
	// the same code as FOCUS_FOLLOW (the script camera's focus follows the thing, no placing at once)
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusFollow(*thing);
	}
}

void SetPositionFollow() // 277 SET_POSITION_FOLLOW
{
	// POP the thing -> the position follows it, and the focus and the position are placed at once on the follow's
	// points (the mode's seconds at 2: then followed at the time factor's pace)
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::PositionFollow(*thing);
		script_camera::PlaceFollowNow();
	}
}

void SetFocusAndPositionFollow() // 278 SET_FOCUS_AND_POSITION_FOLLOW
{
	// POP the distance, then the thing -> as FOCUS_AND_POSITION_FOLLOW, placed at once
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusAndPositionFollow(*thing, distance);
		script_camera::PlaceFollowNow();
	}
}

void SetCameraLens() // 279 SET_CAMERA_LENS
{
	// the FOV set to 70 degrees in x: the argument is the TIME and the lens goes back to the default (copied as the
	// original does; its one use is SET_CAMERA_LENS(0))
	const auto time = Popf();
	script_camera::SetFov(script_camera::k_DefaultFov, time);
}

void MoveCameraLens() // 280 MOVE_CAMERA_LENS
{
	// the FOV set to lens in t (degrees, seconds of game time)
	const auto time = Popf();
	const auto lens = Popf();
	script_camera::SetFov(lens * script_camera::k_DegreesToRadians, time);
}

void CreatureReaction() // 281 CREATURE_REACTION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureInDevScript() // 282 CREATURE_IN_DEV_SCRIPT
{
	const auto object = Pop().uintVal;
	const auto inDevScript = Pop().intVal != 0;
	if (const auto thing = ScriptThing(object, "CREATURE_IN_DEV_SCRIPT", "Thing not found!"))
	{
		ecs::player_creature::SetInDevScript(Locator::entitiesRegistry::value(), *thing, inDevScript);
	}
}

void StoreCameraDetails() // 283 STORE_CAMERA_DETAILS
{
	// the drawn camera's position and focus are stored (not the FOV)
	const auto& camera = Locator::camera::value();
	auto& state = script_camera::Get();
	state.storedPosition = camera.GetOrigin();
	state.storedFocus = camera.GetFocus();
}

void RestoreCameraDetails() // 284 RESTORE_CAMERA_DETAILS
{
	// the camera's position and focus set to the stored ones, whatever the mode
	const auto& state = script_camera::Get();
	script_camera::SetPositionAndFocus(state.storedPosition, state.storedFocus);
	if (!script_camera::Active())
	{
		// (inferred) the player's mode: openblack's player Camera has its own zoomers (not script_camera's), so it is set
		// here too
		Locator::camera::value().SetOrigin(state.storedPosition).SetFocus(state.storedFocus);
	}
	// the camera moved: the draw list is rebuilt on the next drawn frame
	ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
}

void StartAngleSound285() // 285 START_ANGLE_SOUND
{
	// POP, then the confirmation sound on the camera's turn (audio/Services/Confirmation.h)
	const auto enable = Pop().intVal != 0;
	audio::confirmation::StartAngleSound(enable);
}

void SetCameraPosFocLens() // 286 SET_CAMERA_POS_FOC_LENS
{
	// the camera's position and focus set, and the FOV set to lens at once with the lens NOT turned into radians (copied
	// as the original does, no map uses it)
	const auto lens = Popf();
	const auto focus = PopVec();
	const auto position = PopVec();
	script_camera::SetPositionAndFocus(position, focus);
	if (!script_camera::Active())
	{
		Locator::camera::value().SetOrigin(position).SetFocus(focus); // (inferred) as in 284
	}
	script_camera::SetFov(lens, 0.0f);
	// the camera moved: the draw list is rebuilt on the next drawn frame
	ecs::events::Publish(ecs::events::DrawListRebuildRequested {1});
}

void MoveCameraPosFocLens() // 287 MOVE_CAMERA_POS_FOC_LENS
{
	// the script camera's position and focus moved in t and the FOV set to lens in t, the lens not in radians (copied,
	// no map uses it)
	const auto time = Popf();
	const auto lens = Popf();
	const auto focus = PopVec();
	const auto position = PopVec();
	if (ScriptCameraMode(__func__))
	{
		script_camera::MovePosition(position, time);
		script_camera::MoveFocus(focus, time);
		script_camera::SetFov(lens, time);
	}
}

void GameTimeOnOff() // 288 GAME_TIME_ON_OFF
{
	// the visual time scale set to on ? 1 : 0
	const auto enable = Pop().intVal != 0;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "GAME_TIME_ON_OFF({})", enable);
	Locator::dayNightClock::value().Clock().SetRunning(enable);
}

void MoveGameTime() // 289 MOVE_GAME_TIME
{
	// the visual time slides to the hour in `duration` seconds of game time
	const auto duration = Popf();
	const auto hourOfTheDay = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "MOVE_GAME_TIME({}, {})", hourOfTheDay, duration);
	Locator::dayNightClock::value().Clock().MoveScriptTime(hourOfTheDay, duration);
}

void SetHighGraphicsDetail() // 290 SET_HIGH_GRAPHICS_DETAIL
{
	// POP the object, then the bool; no thing: "Thing not found!". The SuperVillager of the intro family
	// (ecs/SuperVillager.h)
	const auto object = Pop().uintVal;
	const auto enable = Pop().intVal != 0;
	if (const auto thing = ScriptThing(object, "SET_HIGH_GRAPHICS_DETAIL", "Thing not found!"); thing.has_value())
	{
		if (enable)
		{
			TheHighDetailSystem().Make(*thing);
		}
		else
		{
			TheHighDetailSystem().Release(*thing);
		}
	}
}

void SetSkeleton() // 291 SET_SKELETON
{
	// const auto object = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void IsSkeleton() // 292 IS_SKELETON
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void PlayerSpellCastTime() // 293 PLAYER_SPELL_CAST_TIME
{
	magic::script::PlayerSpellCastTime(); // Magic/Script/CHLSpells.cpp
}

void PlayerSpellLastCast() // 294 PLAYER_SPELL_LAST_CAST
{
	magic::script::PlayerSpellLastCast(); // Magic/Script/CHLSpells.cpp
}

void GetLastSpellCastPos() // 295 GET_LAST_SPELL_CAST_POS
{
	magic::script::GetLastSpellCastPos(); // Magic/Script/CHLSpells.cpp
}

void AddSpotVisualTargetPos() // 296 ADD_SPOT_VISUAL_TARGET_POS
{
	// const auto position = PopVec();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void AddSpotVisualTargetObject() // 297 ADD_SPOT_VISUAL_TARGET_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetIndestructable() // 298 SET_INDESTRUCTABLE
{
	// POP the object, POP the flag; a script container hands it to its type's callback (TODO: openblack has none),
	// anything else sets or clears its indestructible flag
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	const auto indestructible = (Pop().intVal & 1) != 0;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_INDESTRUCTABLE: Thing not valid");
		return;
	}
	if (indestructible)
	{
		registry.AssignOrReplace<openblack::ecs::components::Indestructible>(object);
	}
	else if (registry.AllOf<openblack::ecs::components::Indestructible>(object))
	{
		registry.Remove<openblack::ecs::components::Indestructible>(object);
	}
}

void SetGraphicsClipping() // 299 SET_GRAPHICS_CLIPPING
{
	// Only the first argument counts: whether the camera clips close
	Pop();
	const auto close = Pop().uintVal != 0;
	Locator::cinematicDirectorSystem::value().SetCloseClipping(close);
}

void SpiritAppear() // 300 SPIRIT_APPEAR
{
	// POP the spirit -> ejected as a help script's spirit: it appears
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		control->SpiritEject(spirit, true);
	}
}

void SpiritDisappear() // 301 SPIRIT_DISAPPEAR
{
	// POP the spirit -> sent home as a help script's spirit: it vanishes
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->SpiritHome(spirit, 1);
	}
}

void SetFocusOnObject() // 302 SET_FOCUS_ON_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ReleaseObjectFocus() // 303 RELEASE_OBJECT_FOCUS
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ImmersionExists() // 304 IMMERSION_EXISTS
{
	// whether a force-feedback mouse is in use: openblack has none, and the game answers false without one
	Pushb(false);
}

void SetDrawLeash() // 305 SET_DRAW_LEASH
{
	// the draw leash setting = the popped value as it is
	help::script_control::GetCameraControl().drawLeash = Pop().intVal;
}

void SetDrawHighlight() // 306 SET_DRAW_HIGHLIGHT
{
	// the draw highlight setting = the popped value as it is
	help::script_control::GetCameraControl().drawHighlight = Pop().intVal;
}

void SetOpenClose() // 307 SET_OPEN_CLOSE
{
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	// The script's word is kept as it is: 1 opens, 0 closes
	const auto open = static_cast<int32_t>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::entity {0} || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_OPEN_CLOSE: thing not found");
		return;
	}
	if (!Locator::animatedStaticSystem::has_value() || !Locator::animatedStaticSystem::value().SetOpenState(object, open))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_OPEN_CLOSE: thing must be an animated static");
	}
}

void SetIntroBuilding() // 308 SET_INTRO_BUILDING
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CreatureForceFriends() // 309 CREATURE_FORCE_FRIENDS
{
	// POP one creature, the other, then whether to befriend them. Either missing: "Thing not found!"; each thing that is
	// not a creature: "Thing not creature!"; and nothing changed. Two creatures with it on: each added once to the
	// other's friends (player_creature::ForceFriends); off: nothing.
	// (approximate) As ScriptThing, written inline so the message is printed once: 0 is null and a valid entity stands
	// for a live thing (the original looks each id up in its script table)
	const auto firstObject = Pop().uintVal;
	const auto secondObject = Pop().uintVal;
	const auto enable = Pop().intVal != 0;
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto first = static_cast<entt::entity>(firstObject);
	const auto second = static_cast<entt::entity>(secondObject);
	if (firstObject == 0 || !registry.Valid(first) || secondObject == 0 || !registry.Valid(second))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATURE_FORCE_FRIENDS: Thing not found!");
		return;
	}
	for (const auto thing : {first, second})
	{
		if (!registry.AllOf<ecs::components::Creature>(thing))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATURE_FORCE_FRIENDS: Thing not creature!");
		}
	}
	ecs::player_creature::ForceFriends(Locator::entitiesRegistry::value(), first, second, enable);
}

void MoveComputerPlayerPosition() // 310 MOVE_COMPUTER_PLAYER_POSITION
{
	// const auto withFixedHeight = static_cast<bool>(Pop().intVal);
	// const auto speed = Popf();
	// const auto position = PopVec();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void EnableDisableComputerPlayer311() // 311 ENABLE_DISABLE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetComputerPlayerPosition() // 312 GET_COMPUTER_PLAYER_POSITION
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SetComputerPlayerPosition() // 313 SET_COMPUTER_PLAYER_POSITION
{
	// const auto withFixedHeight = static_cast<bool>(Pop().intVal);
	// const auto position = PopVec();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetStoredCameraPosition() // 314 GET_STORED_CAMERA_POSITION
{
	// the stored camera position
	PushVec(script_camera::Get().storedPosition);
}

void GetStoredCameraFocus() // 315 GET_STORED_CAMERA_FOCUS
{
	// the stored camera focus
	PushVec(script_camera::Get().storedFocus);
}

void CallNearInState() // 316 CALL_NEAR_IN_STATE
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto position = PopVec();
	// const auto state = Pop().intVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetCreatureSound() // 317 SET_CREATURE_SOUND
{
	// the script's creature sound setting = the value as it is
	audio::GetScriptAudioState().creatureSound = Pop().intVal;
}

void CreatureInteractingWith() // 318 CREATURE_INTERACTING_WITH
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetSunDraw() // 319 SET_SUN_DRAW
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ObjectInfoBits() // 320 OBJECT_INFO_BITS
{
	// What the gate stones laid in a plinth are worth: ape 1, tiger 2, cow 4. Asked of anything else, the original
	// reports the error and pushes no answer at all, so the script's next pop finds whatever lies below.
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::entity {0} || !registry.Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OBJECT_INFO_BITS: thing not valid");
		return;
	}
	const auto value = Locator::animatedStaticSystem::has_value()
	                       ? Locator::animatedStaticSystem::value().GateStoneValue(object)
	                       : std::nullopt;
	if (!value.has_value())
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "OBJECT_INFO_BITS: thing must be an animated static");
		return;
	}
	Pushf(static_cast<float>(*value));
}

void SetHurtByFire() // 321 SET_HURT_BY_FIRE
{
	magic::script::SetHurtByFire(); // Magic/Script/CHLFire.cpp
}

void ConfinedObject() // 322 CONFINED_OBJECT
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ClearConfinedObject() // 323 CLEAR_CONFINED_OBJECT
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetObjectFlock() // 324 GET_OBJECT_FLOCK
{
	// const auto member = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetPlayerBelief() // 325 SET_PLAYER_BELIEF
{
	// const auto belief = Popf();
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void PlayJcSpecial() // 326 PLAY_JC_SPECIAL
{
	// on the value, 0..15: 0, 1, 2, 4, 5, 6 -> the special of that number, 3 a script GFX object, 14 / 15 the camera
	// bookmarks on / off
	const auto feature = static_cast<uint32_t>(Pop().intVal); // unsigned: a negative value is above 15
	switch (feature)
	{
	case 0: // the light onto the Son and the debug camera (the intro specials, Locator::introSystem)
	case 1:
	case 2:
	case 4: // the intro hand
	case 5:
		TheIntroSystem().Play(static_cast<int32_t>(feature));
		break;
	case 3: // a script GFX object: (pending) in no script of the game
		NotImplemented();
		break;
	case 6:
		// the missionaries' boat, in mode 0
		openblack::ecs::missionary_boat::Create(0);
		break;
	case 14:                                            // the bookmarks on
		if (Locator::cameraBookmarkSystem::has_value()) // (openblack guard) none in the CHL tests and tools
		{
			Locator::cameraBookmarkSystem::value().SetEnabled(true);
		}
		break;
	case 15: // the bookmarks off. (FollowUs only, L50058)
		if (Locator::cameraBookmarkSystem::has_value())
		{
			Locator::cameraBookmarkSystem::value().SetEnabled(false);
		}
		break;
	default: // 7..13 and above 15: nothing (FollowUs's 18 at L52177)
		break;
	}
}

void IsPlayingJcSpecial() // 327 IS_PLAYING_JC_SPECIAL
{
	// the value truncated; 1, except 13 -> whether that special has finished (the pick-up clip's wrap: never, it is one
	// shot), pushed as a bool (type 6). No script of the game calls it
	const auto feature = static_cast<int32_t>(Popf());
	Pushb(feature == 13 ? TheIntroSystem().HasLiftFinished() : true);
}

void VortexParameters() // 328 VORTEX_PARAMETERS
{
	// last argument first: the flock, b, a, the position, the town, the vortex. Town or vortex missing -> "Thing not
	// valid"; else the vortex's town and its flock parameters (the map coords of position, a, b, flock): the Out's
	// (docs/wiki/world/vortex.md#bringing-them-out-the-out-vortex)
	const auto flock = Pop().uintVal;
	const auto b = Popf();
	const auto a = Popf();
	const auto position = PopVec();
	const auto town = Pop().uintVal;
	const auto vortex = Pop().uintVal;
	const auto townThing = ScriptThing(town, "VORTEX_PARAMETERS", "Thing not valid");
	const auto vortexThing = ScriptThing(vortex, "VORTEX_PARAMETERS", "Thing not valid");
	if (!townThing || !vortexThing)
	{
		return;
	}
	// (approximate) no flock cast: a valid entity stands for it
	const auto flockEntity = static_cast<entt::entity>(flock);
	const auto flockThing = flock != 0 && Locator::entitiesRegistry::value().Valid(flockEntity) ? flockEntity : entt::null;
	TheVortexSystem().SetParameters(*vortexThing, *townThing, position, a, b, flockThing);
}

void LoadCreature() // 329 LOAD_CREATURE
{
	// a player's creature of the script's species from a mind file, at the point (its height is not used)
	const auto position = PopVec();
	const auto scriptPlayer = Popf();
	const auto mindFilename = PopString();
	const auto type = Pop().intVal;
	PlayerNames player {};
	if (magic::ScriptPlayerToGamePlayer(map_coords::FtoL(scriptPlayer), player))
	{
		ecs::player_creature::ScriptLoadCreature(type, mindFilename, player, glm::vec2(position.x, position.z));
	}
}

void IsSpellCharging() // 330 IS_SPELL_CHARGING
{
	magic::script::IsSpellCharging(); // Magic/Script/CHLWorship.cpp
}

void IsThatSpellCharging() // 331 IS_THAT_SPELL_CHARGING
{
	magic::script::IsThatSpellCharging(); // Magic/Script/CHLWorship.cpp
}

void OpposingCreature() // 332 OPPOSING_CREATURE
{
	// const auto god = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void FlockWithinLimits() // 333 FLOCK_WITHIN_LIMITS
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void HighlightProperties() // 334 HIGHLIGHT_PROPERTIES
{
	// pops the DYK_CATEGORY, the text, then the thing
	const auto category = Pop().uintVal;
	const auto text = Pop().uintVal;
	const auto object = Pop().uintVal;
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !ecs::script_highlight::IsHighlight(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "HIGHLIGHT_PROPERTIES: Thing not valid");
		return;
	}
	// the highlight's script id (text, category)
	ecs::script_highlight::SetScriptId(thing, text, static_cast<DykCategory>(category));
}

void LastMusicLine() // 335 LAST_MUSIC_LINE
{
	// without audio the original runs TEXT_READ instead
	const auto line = Popf();
	std::optional<bool> reached;
	{
		const auto lock = audio::game_music::Lock();
		if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
		{
			reached = gameMusic->ScriptLastMusicLine(line);
		}
	}
	if (reached)
	{
		Pushb(*reached);
	}
	else
	{
		TextRead();
	}
}

void HandDemoTrigger() // 336 HAND_DEMO_TRIGGER
{
	// the script's pending trigger, cleared as it is read (Input/HandDemo.h)
	Pushb(hand_demo::ConsumeTrigger());
}

void GetBellyPosition() // 337 GET_BELLY_POSITION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SetCreatureCreedProperties() // 338 SET_CREATURE_CREED_PROPERTIES
{
	// const auto time = Popf();
	// const auto power = Popf();
	// const auto scale = Popf();
	// const auto handGlow = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameThingCanViewCamera() // 339 GAME_THING_CAN_VIEW_CAMERA
{
	// const auto degrees = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GamePlaySaySoundEffect() // 340 GAME_PLAY_SAY_SOUND_EFFECT
{
	// six POPs (withPos, the point, the text, alt), then the text is said on the first voice, or the second for alt,
	// from the point only with withPos (Locator::helpSpeechSystem over audio::voices::Say)
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto alt = Pop().intVal != 0;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "GAME_PLAY_SAY_SOUND_EFFECT({}, {}, ({}, {}, {}), {})", alt, text, position.x,
	                    position.y, position.z, withPosition);
	TheHelpSpeechSystem().Say(text, alt ? audio::SpeechVoice::Second : audio::SpeechVoice::First,
	                          withPosition ? std::optional(position) : std::nullopt);
}

void SetTownDesireBoost() // 341 SET_TOWN_DESIRE_BOOST
{
	// POP the boost, the desire, the thing; a town and desire < 17 and -1 <= boost <= 1 -> the town's boost of that
	// desire and its desires re-sorted (ecs::town_desire::ScriptSetTownDesireBoost)
	const auto boost = Popf();
	const auto desire = Pop().intVal;
	const auto object = Pop().uintVal;
	std::vector<std::string> errors;
	ecs::town_desire::ScriptSetTownDesireBoost(static_cast<entt::entity>(object), desire, boost, &errors);
	for (const auto& error : errors)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_TOWN_DESIRE_BOOST: {}", error);
	}
}

void IsLockedInteraction() // 342 IS_LOCKED_INTERACTION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetCreatureName() // 343 SET_CREATURE_NAME
{
	// POP the text id, then the creature. No thing: nothing; anything but a creature: "Thing not creature!" and nothing
	// changed; a creature takes the help text of the id as its name (text 0 for an id out of range)
	const auto textId = Pop().uintVal;
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = static_cast<entt::entity>(object);
	if (object == 0 || !registry.Valid(thing))
	{
		return;
	}
	if (!ecs::player_creature::SetName(registry, thing, helptext::Get(textId)))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CREATURE_NAME: Thing not creature!");
	}
}

void ComputerPlayerReady() // 344 COMPUTER_PLAYER_READY
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void EnableDisableComputerPlayer345() // 345 ENABLE_DISABLE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// const auto pause = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ClearActorMind() // 346 CLEAR_ACTOR_MIND
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void EnterExitCitadel() // 347 ENTER_EXIT_CITADEL
{
	const auto enter = static_cast<bool>(Pop().intVal);
	if (Locator::temple::has_value())
	{
		auto& temple = Locator::temple::value();
		const auto active = temple.Active();
		if (enter != active)
		{
			if (enter)
			{
				temple.Activate();
			}
			else
			{
				temple.Deactivate();
			}
		}
	}
	else
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "No temple");
	}
}

void StartAngleSound348() // 348 START_ANGLE_SOUND
{
	// the second "START_ANGLE_SOUND", the pitch one: the confirmation sound on the camera's tilt
	const auto enable = Pop().intVal != 0;
	audio::confirmation::StartPitchSound(enable);
}

void ThingJcSpecial() // 349 THING_JC_SPECIAL
{
	// POP the object, the feature, then the bool; no thing: "Object no longer valid". The flags of a SuperVillager
	// (ecs/SuperVillager.h)
	const auto object = Pop().uintVal;
	const auto feature = Pop().intVal;
	const auto enable = Pop().intVal != 0;
	if (const auto thing = ScriptThing(object, "THING_JC_SPECIAL", "Object no longer valid"); thing.has_value())
	{
		TheHighDetailSystem().Order(*thing, static_cast<ecs::high_detail_rules::ThingSpecial>(feature), enable);
	}
}

void MusicPlayed350() // 350 MUSIC_PLAYED
{
	// the music playing != type (a bool, type 6); without audio the original runs TEXT_READ instead
	const auto music = Pop().intVal;
	std::optional<bool> played;
	{
		const auto lock = audio::game_music::Lock();
		if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
		{
			played = gameMusic->ScriptMusicPlayed(music);
		}
	}
	if (played)
	{
		Pushb(*played);
	}
	else
	{
		TextRead();
	}
}

void UpdateSnapshotPicture() // 351 UPDATE_SNAPSHOT_PICTURE
{
	KeepChallengeRecord(PopChallengeSnapshot(script::challenge_snapshots::Call::Picture),
	                    script::challenge_snapshots::Call::Picture);
}

void StopScriptsInFilesExcluding() // 352 STOP_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto scriptNames = PopString();
	const auto sourceFilenames = PopString();

	const auto names = GetUniqueWords(scriptNames);
	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&names, &filenames](const std::string& name, const std::string& filename) -> bool {
		return filenames.contains(filename) && !names.contains(name);
	});
}

void CreateRandomVillagerOfTribe() // 353 CREATE_RANDOM_VILLAGER_OF_TRIBE
{
	// const auto position = PopVec();
	// const auto tribe = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void ToggleLeash() // 354 TOGGLE_LEASH
{
	// the script player (a float, truncated) presses the leash key for their creature
	const auto scriptPlayer = map_coords::FtoL(Popf());
	if (Locator::leashSystem::has_value())
	{
		creature_leash::script::Toggle(Locator::leashSystem::value(), scriptPlayer);
	}
}

void GameSetMana() // 355 GAME_SET_MANA
{
	magic::script::GameSetMana(); // Magic/Script/CHLWorship.cpp
}

void SetMagicProperties() // 356 SET_MAGIC_PROPERTIES
{
	magic::script::SetMagicProperties(); // Magic/Script/CHLWorship.cpp
}

void SetGameSound() // 357 SET_GAME_SOUND
{
	// false -> every sample stopped and the game sound off (only the dialogue banks HelpSprites / Villagers play); true
	// -> on
	const auto enable = static_cast<bool>(Pop().intVal);
	audio::SetGameSound(enable);
}

void SexIsMale() // 358 SEX_IS_MALE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

/// GET_FIRST_HELP and GET_LAST_HELP do the same: POP the object (none -> "Object no longer valid" and 0) and PUSH, as a
/// float, the first help text about it. A spell dispenser gives its miracle's. (not ported) any other object gives the
/// first help text of its own info row, and a thing that is not an object 0 without a message
void PushFirstHelpText(const char* opcode)
{
	const auto object = Pop().uintVal;
	const auto thing = ScriptThing(object, opcode, "Object no longer valid");
	if (!thing.has_value())
	{
		Pushf(0.0f);
		return;
	}
	if (worship::dispenser::IsDispenser(*thing))
	{
		if (!Locator::infoConstants::has_value())
		{
			std::fprintf(stderr, "chlapi: %s: no info constants in the locator (Locator::infoConstants)\n", opcode);
			std::abort();
		}
		const auto magicType = Locator::entitiesRegistry::value().Get<const ecs::components::SpellDispenser>(*thing).magicType;
		Pushf(magic::GetDispenserHelpText(Locator::infoConstants::value(), magicType));
		return;
	}
	NotImplemented();
	Pushf(0.0f);
}

void GetFirstHelp() // 359 GET_FIRST_HELP
{
	PushFirstHelpText("GET_FIRST_HELP");
}

void GetLastHelp() // 360 GET_LAST_HELP
{
	PushFirstHelpText("GET_LAST_HELP");
}

void IsActive() // 361 IS_ACTIVE
{
	// a highlight's and a dispenser's own active flag (the one SET_ACTIVE writes); a reward's opened flag is set only
	// when it is tapped open, which openblack's rewards cannot be yet; every other thing is never active
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "IS_ACTIVE: Object no longer valid");
		Pushb(false);
		return;
	}
	if (ecs::script_highlight::IsHighlight(entity))
	{
		Pushb(ecs::script_highlight::IsActive(entity));
		return;
	}
	if (const auto* dispenser = registry.TryGet<const ecs::components::SpellDispenser>(entity))
	{
		Pushb(dispenser->timer.active);
		return;
	}
	Pushb(false);
}

void SetBookmarkPosition() // 362 SET_BOOKMARK_POSITION
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetScaffoldProperties() // 363 SET_SCAFFOLD_PROPERTIES
{
	// POP destroy, size (a float), type (stored raw), the object. (not verified) whether the VM gives `type` as an int
	// or as float bits
	const bool destroy = (Pop().intVal & 1) != 0;
	const auto size = Popf();
	const auto type = static_cast<int32_t>(Pop().intVal);
	const auto object = Pop().uintVal;
	// none -> "Object no longer valid!"
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		ObjectNoLongerValid("SET_SCAFFOLD_PROPERTIES", "Object no longer valid!");
		return;
	}
	// not a scaffold -> "Thing must be scaffold", and nothing more (the next scaffold test fails)
	if (!ecs::scaffolds::IsScaffold(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCAFFOLD_PROPERTIES: Thing must be scaffold");
		return;
	}
	// its type, its value = int(size), its destroy flag = destroy & 1
	ecs::scaffolds::SetScaffoldProperties(entity, type, size, destroy);
}

void SetComputerPlayerPersonality() // 364 SET_COMPUTER_PLAYER_PERSONALITY
{
	// const auto probability = Popf();
	// const auto aspect = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetComputerPlayerSuppression() // 365 SET_COMPUTER_PLAYER_SUPPRESSION
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ForceComputerPlayerAction() // 366 FORCE_COMPUTER_PLAYER_ACTION
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// const auto action = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void QueueComputerPlayerAction() // 367 QUEUE_COMPUTER_PLAYER_ACTION
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// const auto action = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetTownWithId() // 368 GET_TOWN_WITH_ID
{
	// the id is the float cut toward 0; the first town with that script number, the players in order and then the
	// neutral one, comes into the script as a found thing (not one it created); no such town gives 0
	const auto id = map_coords::FtoL(Popf());
	const auto town = ecs::town_queries::FindTownWithID(static_cast<uint32_t>(id));
	if (town == entt::null)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "GET_TOWN_WITH_ID: no town with id {}", id);
		Pusho(0);
		return;
	}
	ScriptObjects().Register(town, false);
	Pusho(static_cast<uint32_t>(town));
}

void SetDisciple() // 369 SET_DISCIPLE
{
	// const auto withSound = static_cast<bool>(Pop().intVal);
	// const auto discipleType = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ReleaseComputerPlayer() // 370 RELEASE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetComputerPlayerSpeed() // 371 SET_COMPUTER_PLAYER_SPEED
{
	// const auto speed = Popf();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetFocusFollowComputerPlayer() // 372 SET_FOCUS_FOLLOW_COMPUTER_PLAYER
{
	// POP the player (a float, truncated), the script player to the game's player, the mode checks, then the path
	// dropped, no focus thing and the focus follows the computer player's hand. Not ported: openblack has no computer
	// players
	[[maybe_unused]] const auto player = Popf();
	NotImplemented();
}

void SetPositionFollowComputerPlayer() // 373 SET_POSITION_FOLLOW_COMPUTER_PLAYER
{
	// as 372 for the position: the path dropped, no follow thing and the position follows the player. Not ported
	[[maybe_unused]] const auto player = Popf();
	NotImplemented();
}

void CallComputerPlayer() // 374 CALL_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void CallBuildingInTown() // 375 CALL_BUILDING_IN_TOWN
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void SetCanBuildWorshipsite() // 376 SET_CAN_BUILD_WORSHIPSITE
{
	magic::script::SetCanBuildWorshipsite(); // Magic/Script/CHLWorship.cpp
}

void GetFacingCameraPosition() // 377 GET_FACING_CAMERA_POSITION
{
	// the drawn camera's position + d * the camera's forward vector (taken as the unit vector from the drawn position to
	// the drawn focus: inferred)
	const auto distance = Popf();
	const auto& camera = Locator::camera::value();
	const auto origin = camera.GetOrigin();
	const auto toFocus = camera.GetFocus() - origin;
	const float length = glm::length(toFocus);
	PushVec(length > 0.0f ? origin + toFocus * (distance / length) : origin);
}

void SetComputerPlayerAttitude() // 378 SET_COMPUTER_PLAYER_ATTITUDE
{
	// const auto attitude = Popf();
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetComputerPlayerAttitude() // 379 GET_COMPUTER_PLAYER_ATTITUDE
{
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void LoadComputerPlayerPersonality() // 380 LOAD_COMPUTER_PLAYER_PERSONALITY
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SaveComputerPlayerPersonality() // 381 SAVE_COMPUTER_PLAYER_PERSONALITY
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetPlayerAlly() // 382 SET_PLAYER_ALLY
{
	// const auto percentage = Popf();
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void CallFlying() // 383 CALL_FLYING
{
	// const auto excluding = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto position = PopVec();
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void SetObjectFadeIn() // 384 SET_OBJECT_FADE_IN
{
	// the seconds, then the object. A creature drops out of sight at once, with the teleport's energise, and fizzes
	// back in over the seconds; the game fades nothing else in and says so
	const auto seconds = Popf();
	const auto object = Pop().uintVal;
	const auto thing = ScriptThing(object, __func__, "Object dead man!");
	if (!thing.has_value())
	{
		return;
	}
	if (!Locator::entitiesRegistry::value().AllOf<ecs::components::Creature>(*thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Not implemented yet - TODO JC", __func__);
		return;
	}
	if (Locator::creatureFizzSystem::has_value())
	{
		auto& fizz = Locator::creatureFizzSystem::value();
		fizz.SetFizz(*thing, 1.0f, 0.0f, false);
		fizz.SetFizz(*thing, 0.0f, seconds, false);
	}
}

void IsAffectedBySpell() // 385 IS_AFFECTED_BY_SPELL
{
	// pops two values, the object last (pending: what the first one is)
	// const auto first = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetMagicInObject() // 386 SET_MAGIC_IN_OBJECT
{
	magic::script::SetMagicInObject(); // Magic/Script/CHLWorship.cpp
}

void IdAdultSize() // 387 ID_ADULT_SIZE
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void ObjectCapacity() // 388 OBJECT_CAPACITY
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void ObjectAdultCapacity() // 389 OBJECT_ADULT_CAPACITY
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetCreatureAutoFighting() // 390 SET_CREATURE_AUTO_FIGHTING
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void IsAutoFighting() // 391 IS_AUTO_FIGHTING
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void SetCreatureQueueFightMove() // 392 SET_CREATURE_QUEUE_FIGHT_MOVE
{
	// const auto move = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCreatureQueueFightSpell() // 393 SET_CREATURE_QUEUE_FIGHT_SPELL
{
	// const auto spell = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCreatureQueueFightStep() // 394 SET_CREATURE_QUEUE_FIGHT_STEP
{
	// const auto step = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetCreatureFightAction() // 395 GET_CREATURE_FIGHT_ACTION
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void CreatureFightQueueHits() // 396 CREATURE_FIGHT_QUEUE_HITS
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SquareRoot() // 397 SQUARE_ROOT
{
	const auto value = Popf();
	auto root = 0.0f;
	if (value > 0.0f)
	{
		root = std::sqrt(value);
	}
	Pushf(root);
}

void GetPlayerAlly() // 398 GET_PLAYER_ALLY
{
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SetPlayerWindResistance() // 399 SET_PLAYER_WIND_RESISTANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void GetPlayerWindResistance() // 400 GET_PLAYER_WIND_RESISTANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushi(0);
}

void PauseUnpauseClimateSystem() // 401 PAUSE_UNPAUSE_CLIMATE_SYSTEM
{
	magic::script::PauseUnpauseClimateSystem(); // Magic/Script/CHLWeather.cpp
}

void PauseUnpauseStormCreationInClimateSystem() // 402 PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM
{
	magic::script::PauseUnpauseStormCreationInClimateSystem(); // Magic/Script/CHLWeather.cpp
}

void GetManaForSpell() // 403 GET_MANA_FOR_SPELL
{
	magic::script::GetManaForSpell(); // Magic/Script/CHLSpells.cpp
}

void KillStormsInArea() // 404 KILL_STORMS_IN_AREA
{
	magic::script::KillStormsInArea(); // Magic/Script/CHLWeather.cpp
}

void InsideTemple() // 405 INSIDE_TEMPLE
{
	// whether we are inside the citadel, a bool (type 6)
	Pushb(openblack::game_clock::IsInsideCitadel());
}

void RestartObject() // 406 RESTART_OBJECT
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetGameTimeProperties() // 407 SET_GAME_TIME_PROPERTIES
{
	// the visual time cycle (duration, percentage night, percentage change)
	const auto percentageChange = Popf();
	const auto percentageNight = Popf();
	const auto duration = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_GAME_TIME_PROPERTIES({}, {}, {})", duration, percentageNight,
	                   percentageChange);
	Locator::dayNightClock::value().Clock().SetCycle(duration, percentageNight, percentageChange);
}

void ResetGameTimeProperties() // 408 RESET_GAME_TIME_PROPERTIES
{
	Locator::dayNightClock::value().Clock().SetCycle(DayNightClock::k_DefaultDuration, DayNightClock::k_DefaultNight,
	                                                 DayNightClock::k_DefaultChange);
}

void SoundExists() // 409 SOUND_EXISTS
{
	// whether the sound is installed, pushed as a bool (type 6)
	Pushb(audio::SoundExists());
}

void GetTownWorshipDeaths() // 410 GET_TOWN_WORSHIP_DEATHS
{
	magic::script::GetTownWorshipDeaths(); // Magic/Script/CHLWorship.cpp
}

void GameClearDialogue() // 411 GAME_CLEAR_DIALOGUE
{
	// the help system's text cleared (the voices go on, docs/wiki/audio/voices.md#narration-and-reading-time)
	TheHelpTextSystem().ClearAllText();
}

void GameCloseDialogue() // 412 GAME_CLOSE_DIALOGUE
{
	// the help text closed and the help system's text cleared
	TheHelpTextSystem().CloseDialogue();
}

void GetHandState() // 413 GET_HAND_STATE
{
	// the interface's hand state of the last turn (HandSystemInterface)
	Pushi(Locator::handSystem::value().GetInterfaceHandState());
}

void SetInterfaceCitadel() // 414 SET_INTERFACE_CITADEL
{
	// the interface citadel setting = POP() (the raw value); its only reader is the citadel entrance's tap check
	openblack::worship::citadel::SetInterfaceCitadel(Pop().uintVal);
}

void MapScriptFunction() // 415 MAP_SCRIPT_FUNCTION
{
	// const auto command = PopString();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void WithinRotation() // 416 WITHIN_ROTATION
{
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetPlayerTownTotal() // 417 GET_PLAYER_TOWN_TOTAL
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void SpiritScreenPoint() // 418 SPIRIT_SCREEN_POINT
{
	// POP y, x, the spirit; "Invalid Y" / "Invalid X" (not stopping); the pixel (int(W x), int(H y)) of the screen's
	// width and height -> the spirit points at it
	const auto y = Popf();
	const auto x = Popf();
	const auto spirit = ScriptHelpSpirit(Pop().intVal);
	CheckScreenFractions("SPIRIT_SCREEN_POINT", x, y);
	if (auto* control = SpiritControl(); control != nullptr)
	{
		const auto& screen = control->GetScreen();
		const glm::ivec2 pixel(static_cast<int32_t>(static_cast<float>(screen.width) * x),
		                       static_cast<int32_t>(static_cast<float>(screen.height) * y));
		control->SpiritScreenPoint(spirit, pixel);
	}
}

void KeyDown() // 419 KEY_DOWN
{
	// const auto key = Pop().intVal;
	// TODO(Daniels118): implement this (translate key to physical key code)
	NotImplemented();
	Pushb(false);
}

void SetFightExit() // 420 SET_FIGHT_EXIT
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetObjectClicked() // 421 GET_OBJECT_CLICKED
{
	// the thing last tapped with the hand, the same memory GAME_THING_CLICKED reads (none once forgotten, cleared or
	// gone), comes into the script as a found thing; none gives 0
	const auto* clicked = LocalHandClicked();
	const auto thing = clicked != nullptr ? clicked->thing : entt::null;
	if (thing == entt::null || !Locator::entitiesRegistry::value().Valid(thing))
	{
		Pusho(0);
		return;
	}
	ScriptObjects().Register(thing, false);
	Pusho(static_cast<uint32_t>(thing));
}

void GetMana() // 422 GET_MANA
{
	magic::script::GetMana(); // Magic/Script/CHLWorship.cpp
}

void ClearPlayerSpellCharging() // 423 CLEAR_PLAYER_SPELL_CHARGING
{
	magic::script::ClearPlayerSpellCharging(); // Magic/Script/CHLWorship.cpp
}

void StopSoundEffect() // 424 STOP_SOUND_EFFECT
{
	// POPs bank, sample (or a HELP_TEXT with isSay), isSay (the scripts of the shipped game always pass 0)
	const auto bank = Pop().intVal;
	const auto id = Pop().uintVal;
	const auto isSay = Pop().intVal != 0;
	audio::script_sound::StopSoundEffect(isSay, id, bank);
}

void GetTotemStatue() // 425 GET_TOTEM_STATUE
{
	// the object: a town centre's totem, pushed as an object; anything else (a town too) 0, with no message
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	std::optional<entt::entity> totem;
	if (object != 0 && Locator::entitiesRegistry::value().Valid(entity) && Locator::villageTotemSystem::has_value())
	{
		totem = Locator::villageTotemSystem::value().TotemOfTownCentre(entity);
	}
	Pusho(totem.has_value() ? static_cast<uint32_t>(*totem) : 0);
}

void SetSetOnFire() // 426 SET_SET_ON_FIRE
{
	magic::script::SetSetOnFire(); // Magic/Script/CHLFire.cpp
}

void SetLandBalance() // 427 SET_LAND_BALANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetObjectBeliefScale() // 428 SET_OBJECT_BELIEF_SCALE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StartImmersion() // 429 START_IMMERSION
{
	// const auto effect = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StopImmersion() // 430 STOP_IMMERSION
{
	// const auto effect = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void StopAllImmersion() // 431 STOP_ALL_IMMERSION
{
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetCreatureInTemple() // 432 SET_CREATURE_IN_TEMPLE
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameDrawText() // 433 GAME_DRAW_TEXT
{
	// const auto fade = Popf();
	// const auto size = Popf();
	// const auto height = Popf();
	// const auto width = Popf();
	// const auto down = Popf();
	// const auto across = Popf();
	// const auto textID = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GameDrawTempText() // 434 GAME_DRAW_TEMP_TEXT
{
	// const auto fade = Popf();
	// const auto size = Popf();
	// const auto height = Popf();
	// const auto width = Popf();
	// const auto down = Popf();
	// const auto across = Popf();
	// const auto string = PopString();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void FadeAllDrawText() // 435 FADE_ALL_DRAW_TEXT
{
	// const auto time = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetDrawTextColour() // 436 SET_DRAW_TEXT_COLOUR
{
	// const auto blue = Popf();
	// const auto green = Popf();
	// const auto red = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetClippingWindow() // 437 SET_CLIPPING_WINDOW
{
	// const auto time = Popf();
	// const auto height = Popf();
	// const auto width = Popf();
	// const auto down = Popf();
	// const auto across = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void ClearClippingWindow() // 438 CLEAR_CLIPPING_WINDOW
{
	// const auto time = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SaveGameInSlot() // 439 SAVE_GAME_IN_SLOT
{
	// const auto slot = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void SetObjectCarrying() // 440 SET_OBJECT_CARRYING
{
	// const auto carriedObj = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void PosValidForCreature() // 441 POS_VALID_FOR_CREATURE
{
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushb(false);
}

void GetTimeSinceObjectAttacked() // 442 GET_TIME_SINCE_OBJECT_ATTACKED
{
	// POP the town, POP the script's player. The seconds since that player last attacked the town, while the town
	// still holds more than a little against them (ecs::town_aggression::SecondsSinceAttacked); the largest float when
	// it is no town, no player, or never attacked by them
	const auto object = Pop().uintVal;
	const auto scriptPlayer = Popf();
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	using openblack::ecs::components::Town;
	const auto* town = object != 0 && registry.Valid(entity) ? registry.TryGet<const Town>(entity) : nullptr;
	PlayerNames player {};
	if (town == nullptr || !magic::ScriptPlayerToGamePlayer(openblack::map_coords::FtoL(scriptPlayer), player))
	{
		Pushf(std::numeric_limits<float>::max());
		return;
	}
	Pushf(openblack::ecs::town_aggression::SecondsSinceAttacked(town->aggression, player, openblack::game_clock::Turn(),
	                                                            openblack::game_clock::MsPerTurn()));
}

void GetTownAndVillagerHealthTotal() // 443 GET_TOWN_AND_VILLAGER_HEALTH_TOTAL
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GameAddForBuilding() // 444 GAME_ADD_FOR_BUILDING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented();
}

void EnableDisableAlignmentMusic() // 445 ENABLE_DISABLE_ALIGNMENT_MUSIC
{
	// the alignment music setting = the value as it is
	const auto enable = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "ENABLE_DISABLE_ALIGNMENT_MUSIC({})", enable);
	audio::GetScriptAudioState().alignmentMusic = enable;
}

void GetDeadLiving() // 446 GET_DEAD_LIVING
{
	// const auto radius = Popf();
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void AttachSoundTag() // 447 ATTACH_SOUND_TAG
{
	// POPs the object (looked up at once), bank, sample, threeD
	const auto object = MusicThing(Pop().uintVal);
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	const auto threeD = Pop().intVal != 0;
	if (object)
	{
		audio::script_sound::AttachSoundTag(threeD, sample, bank, static_cast<entt::entity>(*object));
	}
}

void DetachSoundTag() // 448 DETACH_SOUND_TAG
{
	// POPs the object (looked up at once), bank, sample
	const auto object = MusicThing(Pop().uintVal);
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	if (object)
	{
		audio::script_sound::DetachSoundTag(sample, bank, static_cast<entt::entity>(*object));
	}
}

void GetSacrificeTotal() // 449 GET_SACRIFICE_TOTAL
{
	// const auto worshipSite = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushf(0.0f);
}

void GameSoundPlaying() // 450 GAME_SOUND_PLAYING
{
	// POPs bank, sample; pushes whether it is playing as a bool (type 6)
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	Pushb(audio::script_sound::GameSoundPlaying(sample, bank));
}

void GetTemplePosition() // 451 GET_TEMPLE_POSITION
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureAutoscale() // 452 CREATURE_AUTOSCALE
{
	// POP the factor, the creature, then whether it is on. A creature then follows the local player's creature's size
	// each turn (player_creature::Autoscale); no thing, or anything but a creature: "Thing should be valid!" and nothing
	// changed
	const auto factor = Popf();
	const auto object = Pop().uintVal;
	const auto enable = Pop().intVal != 0;
	const auto thing = ScriptThing(object, "CREATURE_AUTOSCALE", "Thing should be valid!");
	if (thing.has_value() && !ecs::player_creature::SetAutoscale(Locator::entitiesRegistry::value(), *thing, enable, factor))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CREATURE_AUTOSCALE: Thing should be valid!");
	}
}

void GetSpellIconInTemple() // 453 GET_SPELL_ICON_IN_TEMPLE
{
	magic::script::GetSpellIconInTemple(); // Magic/Script/CHLWorship.cpp
}

void GameClearComputerPlayerActions() // 454 GAME_CLEAR_COMPUTER_PLAYER_ACTIONS
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
}

void GetFirstInContainer() // 455 GET_FIRST_IN_CONTAINER
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void GetNextInContainer() // 456 GET_NEXT_IN_CONTAINER
{
	// const auto after = Pop().uintVal;
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented();
	Pusho(0);
}

void GetTempleEntrancePosition() // 457 GET_TEMPLE_ENTRANCE_POSITION
{
	// const auto height = Popf();
	// const auto radius = Popf();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SaySoundEffectPlaying() // 458 SAY_SOUND_EFFECT_PLAYING
{
	// POP the text, then alt; push whether the first voice, or the second for alt, still says it (type 6)
	// (Locator::helpSpeechSystem over audio::voices::IsSaying)
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto alt = Pop().intVal != 0;
	Pushb(TheHelpSpeechSystem().IsSaying(text, alt ? audio::SpeechVoice::Second : audio::SpeechVoice::First));
}

void SetHandDemoKeys() // 459 SET_HAND_DEMO_KEYS
{
	// The original's handler is empty: it does nothing and pops nothing (no script calls it). openblack's VM drops the
	// declared argument when a native pops nothing (LHVM Opcode05Sys), which the original would leave on the stack
}

// The three push a bit of the game's tutorial skip flags as a boolean (type 6); the bits are set at each new game by
// the SkipBox answer (Game::Run) and kept by the start-of-game answer's service (Locator::tutorialSkipSystem). SetupLand1
// turns them into IsSkippingToCreatureSelect, IsSkippingCreatureGuide and IsKeepingOldCreature, which LandControl1 reads
// (docs/wiki/scripts/land-change-and-saving.md#what-the-script-does-with-the-answer).
void CanSkipTutorial() // 460 CAN_SKIP_TUTORIAL
{
	// bit 23
	Pushb(TheTutorialSkipSystem().Get().skipTutorial);
}

void CanSkipCreatureTraining() // 461 CAN_SKIP_CREATURE_TRAINING
{
	// bit 24
	Pushb(TheTutorialSkipSystem().Get().skipCreatureTraining);
}

void IsKeepingOldCreature() // 462 IS_KEEPING_OLD_CREATURE
{
	// bit 25
	Pushb(TheTutorialSkipSystem().Get().keepOldCreature);
}

void CurrentProfileHasCreature() // 463 CURRENT_PROFILE_HAS_CREATURE
{
	// Whether the profile's mind file exists in Scripts/CreatureMind. The profile's file is the creature-file setting
	Pushb(ThePlayerProfileSystem().CurrentProfileHasCreature());
}

void CHLApi::InitFunctionsTable0()
{
	CREATE_FUNCTION_BINDING("NONE", 0, 0, None);
	CREATE_FUNCTION_BINDING("SET_CAMERA_POSITION", 3, 0, SetCameraPosition);
	CREATE_FUNCTION_BINDING("SET_CAMERA_FOCUS", 3, 0, SetCameraFocus);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_POSITION", 4, 0, MoveCameraPosition);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_FOCUS", 4, 0, MoveCameraFocus);
	CREATE_FUNCTION_BINDING("GET_CAMERA_POSITION", 0, 3, GetCameraPosition);
	CREATE_FUNCTION_BINDING("GET_CAMERA_FOCUS", 0, 3, GetCameraFocus);
	CREATE_FUNCTION_BINDING("SPIRIT_EJECT", 1, 0, SpiritEject);
	CREATE_FUNCTION_BINDING("SPIRIT_HOME", 1, 0, SpiritHome);
	CREATE_FUNCTION_BINDING("SPIRIT_POINT_POS", 5, 0, SpiritPointPos);
	CREATE_FUNCTION_BINDING("SPIRIT_POINT_GAME_THING", 3, 0, SpiritPointGameThing);
	CREATE_FUNCTION_BINDING("GAME_THING_FIELD_OF_VIEW", 1, 1, GameThingFieldOfView);
	CREATE_FUNCTION_BINDING("POS_FIELD_OF_VIEW", 3, 1, PosFieldOfView);
	CREATE_FUNCTION_BINDING("RUN_TEXT", 3, 0, RunText);
	CREATE_FUNCTION_BINDING("TEMP_TEXT", 3, 0, TempText);
	CREATE_FUNCTION_BINDING("TEXT_READ", 0, 1, TextRead);
	CREATE_FUNCTION_BINDING("GAME_THING_CLICKED", 1, 1, GameThingClicked);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_STATE", 2, 0, SetScriptState);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_STATE_POS", 4, 0, SetScriptStatePos);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_FLOAT", 2, 0, SetScriptFloat);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_ULONG", 3, 0, SetScriptUlong);
	CREATE_FUNCTION_BINDING("GET_PROPERTY", 2, 1, GetProperty);
	CREATE_FUNCTION_BINDING("SET_PROPERTY", 3, 0, SetProperty);
	CREATE_FUNCTION_BINDING("GET_POSITION", 1, 3, GetPosition);
	CREATE_FUNCTION_BINDING("SET_POSITION", 4, 0, SetPosition);
	CREATE_FUNCTION_BINDING("GET_DISTANCE", 6, 1, GetDistance);
	CREATE_FUNCTION_BINDING("CALL", 6, 1, Call);
	CREATE_FUNCTION_BINDING("CREATE", 5, 1, Create);
	CREATE_FUNCTION_BINDING("RANDOM", 2, 1, Random);
	CREATE_FUNCTION_BINDING("DLL_GETTIME", 0, 1, DllGettime);
	CREATE_FUNCTION_BINDING("START_CAMERA_CONTROL", 0, 1, StartCameraControl);
	CREATE_FUNCTION_BINDING("END_CAMERA_CONTROL", 0, 0, EndCameraControl);
	CREATE_FUNCTION_BINDING("SET_WIDESCREEN", 1, 0, SetWidescreen);
	CREATE_FUNCTION_BINDING("MOVE_GAME_THING", 5, 0, MoveGameThing);
	CREATE_FUNCTION_BINDING("SET_FOCUS", 4, 0, SetFocus);
	CREATE_FUNCTION_BINDING("HAS_CAMERA_ARRIVED", 0, 1, HasCameraArrived);
	CREATE_FUNCTION_BINDING("FLOCK_CREATE", 3, 1, FlockCreate);
	CREATE_FUNCTION_BINDING("FLOCK_ATTACH", 3, 1, FlockAttach);
	CREATE_FUNCTION_BINDING("FLOCK_DETACH", 2, 1, FlockDetach);
	CREATE_FUNCTION_BINDING("FLOCK_DISBAND", 1, 0, FlockDisband);
	CREATE_FUNCTION_BINDING("ID_SIZE", 1, 1, IdSize);
	CREATE_FUNCTION_BINDING("FLOCK_MEMBER", 2, 1, FlockMember);
	CREATE_FUNCTION_BINDING("GET_HAND_POSITION", 0, 3, GetHandPosition);
	CREATE_FUNCTION_BINDING("PLAY_SOUND_EFFECT", 6, 0, PlaySoundEffect);
	CREATE_FUNCTION_BINDING("START_MUSIC", 1, 0, StartMusic);
	CREATE_FUNCTION_BINDING("STOP_MUSIC", 0, 0, StopMusic);
	CREATE_FUNCTION_BINDING("ATTACH_MUSIC", 2, 0, AttachMusic);
	CREATE_FUNCTION_BINDING("DETACH_MUSIC", 1, 0, DetachMusic);
	CREATE_FUNCTION_BINDING("OBJECT_DELETE", 2, 0, ObjectDelete);
	CREATE_FUNCTION_BINDING("FOCUS_FOLLOW", 1, 0, FocusFollow);
	CREATE_FUNCTION_BINDING("POSITION_FOLLOW", 1, 0, PositionFollow);
	CREATE_FUNCTION_BINDING("CALL_NEAR", 7, 1, CallNear);
	CREATE_FUNCTION_BINDING("SPECIAL_EFFECT_POSITION", 5, 1, SpecialEffectPosition);
	CREATE_FUNCTION_BINDING("SPECIAL_EFFECT_OBJECT", 3, 1, SpecialEffectObject);
	CREATE_FUNCTION_BINDING("DANCE_CREATE", 6, 1, DanceCreate);
	CREATE_FUNCTION_BINDING("CALL_IN", 4, 1, CallIn);
	CREATE_FUNCTION_BINDING("CHANGE_INNER_OUTER_PROPERTIES", 4, 0, ChangeInnerOuterProperties);
	CREATE_FUNCTION_BINDING("SNAPSHOT", -1, 0, Snapshot);
	CREATE_FUNCTION_BINDING("GET_ALIGNMENT", 1, 1, GetAlignment);
	CREATE_FUNCTION_BINDING("SET_ALIGNMENT", 2, 0, SetAlignment);
	CREATE_FUNCTION_BINDING("INFLUENCE_OBJECT", 4, 1, InfluenceObject);
	CREATE_FUNCTION_BINDING("INFLUENCE_POSITION", 6, 1, InfluencePosition);
	CREATE_FUNCTION_BINDING("GET_INFLUENCE", 5, 1, GetInfluence);
	CREATE_FUNCTION_BINDING("SET_INTERFACE_INTERACTION", 1, 0, SetInterfaceInteraction);
	CREATE_FUNCTION_BINDING("PLAYED", 1, 1, Played);
	CREATE_FUNCTION_BINDING("RANDOM_ULONG", 2, 1, RandomUlong);
	CREATE_FUNCTION_BINDING("SET_GAMESPEED", 1, 0, SetGamespeed);
	CREATE_FUNCTION_BINDING("CALL_IN_NEAR", 8, 1, CallInNear);
	CREATE_FUNCTION_BINDING("OVERRIDE_STATE_ANIMATION", 2, 0, OverrideStateAnimation);
	CREATE_FUNCTION_BINDING("CREATURE_CREATE_RELATIVE_TO_CREATURE", 6, 1, CreatureCreateRelativeToCreature);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_EVERYTHING", 1, 0, CreatureLearnEverything);
	CREATE_FUNCTION_BINDING("CREATURE_SET_KNOWS_ACTION", 4, 0, CreatureSetKnowsAction);
	CREATE_FUNCTION_BINDING("CREATURE_SET_AGENDA_PRIORITY", 2, 0, CreatureSetAgendaPriority);
	CREATE_FUNCTION_BINDING("CREATURE_TURN_OFF_ALL_DESIRES", 1, 0, CreatureTurnOffAllDesires);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT", 4, 0,
	                        CreatureLearnDistinctionAboutActivityObject);
	CREATE_FUNCTION_BINDING("CREATURE_DO_ACTION", 4, 0, CreatureDoAction);
	CREATE_FUNCTION_BINDING("IN_CREATURE_HAND", 2, 1, InCreatureHand);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_VALUE", 3, 0, CreatureSetDesireValue);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_ACTIVATED", 3, 0, CreatureSetDesireActivated78);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_ACTIVATED", 2, 0, CreatureSetDesireActivated79);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_MAXIMUM", 3, 0, CreatureSetDesireMaximum);
	CREATE_FUNCTION_BINDING("CONVERT_CAMERA_POSITION", 1, 3, ConvertCameraPosition);
	CREATE_FUNCTION_BINDING("CONVERT_CAMERA_FOCUS", 1, 3, ConvertCameraFocus);
	CREATE_FUNCTION_BINDING("CREATURE_SET_PLAYER", 1, 0, CreatureSetPlayer);
	CREATE_FUNCTION_BINDING("START_COUNTDOWN_TIMER", 1, 0, StartCountdownTimer);
	CREATE_FUNCTION_BINDING("CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION", 2, 0, CreatureInitialiseNumTimesPerformedAction);
	CREATE_FUNCTION_BINDING("CREATURE_GET_NUM_TIMES_ACTION_PERFORMED", 2, 1, CreatureGetNumTimesActionPerformed);
	CREATE_FUNCTION_BINDING("REMOVE_COUNTDOWN_TIMER", 0, 0, RemoveCountdownTimer);
	CREATE_FUNCTION_BINDING("GET_OBJECT_DROPPED", 1, 1, GetObjectDropped);
	CREATE_FUNCTION_BINDING("CLEAR_DROPPED_BY_OBJECT", 1, 0, ClearDroppedByObject);
	CREATE_FUNCTION_BINDING("CREATE_REACTION", 2, 0, CreateReaction);
	CREATE_FUNCTION_BINDING("REMOVE_REACTION", 1, 0, RemoveReaction);
	CREATE_FUNCTION_BINDING("GET_COUNTDOWN_TIMER", 0, 1, GetCountdownTimer);
	CREATE_FUNCTION_BINDING("START_DUAL_CAMERA", 2, 0, StartDualCamera);
	CREATE_FUNCTION_BINDING("UPDATE_DUAL_CAMERA", 2, 0, UpdateDualCamera);
	CREATE_FUNCTION_BINDING("RELEASE_DUAL_CAMERA", 0, 0, ReleaseDualCamera);
	CREATE_FUNCTION_BINDING("SET_CREATURE_HELP", 1, 0, SetCreatureHelp);
	CREATE_FUNCTION_BINDING("GET_TARGET_OBJECT", 1, 1, GetTargetObject);
	CREATE_FUNCTION_BINDING("CREATURE_DESIRE_IS", 2, 1, CreatureDesireIs);
	CREATE_FUNCTION_BINDING("COUNTDOWN_TIMER_EXISTS", 0, 1, CountdownTimerExists);
	CREATE_FUNCTION_BINDING("LOOK_GAME_THING", 2, 0, LookGameThing);
	CREATE_FUNCTION_BINDING("GET_OBJECT_DESTINATION", 1, 3, GetObjectDestination);
	CREATE_FUNCTION_BINDING("CREATURE_FORCE_FINISH", 1, 0, CreatureForceFinish);
	CREATE_FUNCTION_BINDING("HIDE_COUNTDOWN_TIMER", 0, 0, HideCountdownTimer);
	CREATE_FUNCTION_BINDING("GET_ACTION_TEXT_FOR_OBJECT", 0, 1, GetActionTextForObject);
	CREATE_FUNCTION_BINDING("CREATE_DUAL_CAMERA_WITH_POINT", 4, 0, CreateDualCameraWithPoint);
	CREATE_FUNCTION_BINDING("SET_CAMERA_TO_FACE_OBJECT", 2, 0, SetCameraToFaceObject);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_TO_FACE_OBJECT", 3, 0, MoveCameraToFaceObject);
}

void CHLApi::InitFunctionsTable1()
{
	CREATE_FUNCTION_BINDING("GET_MOON_PERCENTAGE", 0, 1, GetMoonPercentage);
	CREATE_FUNCTION_BINDING("POPULATE_CONTAINER", 4, 0, PopulateContainer);
	CREATE_FUNCTION_BINDING("ADD_REFERENCE", 1, 1, AddReference);
	CREATE_FUNCTION_BINDING("REMOVE_REFERENCE", 1, 1, RemoveReference);
	CREATE_FUNCTION_BINDING("SET_GAME_TIME", 1, 0, SetGameTime);
	CREATE_FUNCTION_BINDING("GET_GAME_TIME", 0, 1, GetGameTime);
	CREATE_FUNCTION_BINDING("GET_REAL_TIME", 0, 1, GetRealTime);
	CREATE_FUNCTION_BINDING("GET_REAL_DAY", 0, 1, GetRealDay115);
	CREATE_FUNCTION_BINDING("GET_REAL_DAY", 0, 1, GetRealDay116);
	CREATE_FUNCTION_BINDING("GET_REAL_MONTH", 0, 1, GetRealMonth);
	CREATE_FUNCTION_BINDING("GET_REAL_YEAR", 0, 1, GetRealYear);
	CREATE_FUNCTION_BINDING("RUN_CAMERA_PATH", 1, 0, RunCameraPath);
	CREATE_FUNCTION_BINDING("START_DIALOGUE", 0, 1, StartDialogue);
	CREATE_FUNCTION_BINDING("END_DIALOGUE", 0, 0, EndDialogue);
	CREATE_FUNCTION_BINDING("IS_DIALOGUE_READY", 0, 1, IsDialogueReady);
	CREATE_FUNCTION_BINDING("CHANGE_WEATHER_PROPERTIES", 6, 0, ChangeWeatherProperties);
	CREATE_FUNCTION_BINDING("CHANGE_LIGHTNING_PROPERTIES", 5, 0, ChangeLightningProperties);
	CREATE_FUNCTION_BINDING("CHANGE_TIME_FADE_PROPERTIES", 3, 0, ChangeTimeFadeProperties);
	CREATE_FUNCTION_BINDING("CHANGE_CLOUD_PROPERTIES", 4, 0, ChangeCloudProperties);
	CREATE_FUNCTION_BINDING("SET_HEADING_AND_SPEED", 5, 0, SetHeadingAndSpeed);
	CREATE_FUNCTION_BINDING("START_GAME_SPEED", 0, 0, StartGameSpeed);
	CREATE_FUNCTION_BINDING("END_GAME_SPEED", 0, 0, EndGameSpeed);
	CREATE_FUNCTION_BINDING("BUILD_BUILDING", 4, 0, BuildBuilding);
	CREATE_FUNCTION_BINDING("SET_AFFECTED_BY_WIND", 2, 0, SetAffectedByWind);
	CREATE_FUNCTION_BINDING("WIDESCREEN_TRANSISTION_FINISHED", 0, 1, WidescreenTransistionFinished);
	CREATE_FUNCTION_BINDING("GET_RESOURCE", 2, 1, GetResource);
	CREATE_FUNCTION_BINDING("ADD_RESOURCE", 3, 1, AddResource);
	CREATE_FUNCTION_BINDING("REMOVE_RESOURCE", 3, 1, RemoveResource);
	CREATE_FUNCTION_BINDING("GET_TARGET_RELATIVE_POS", 8, 3, GetTargetRelativePos);
	CREATE_FUNCTION_BINDING("STOP_POINTING", 1, 0, StopPointing);
	CREATE_FUNCTION_BINDING("STOP_LOOKING", 1, 0, StopLooking);
	CREATE_FUNCTION_BINDING("LOOK_AT_POSITION", 4, 0, LookAtPosition);
	CREATE_FUNCTION_BINDING("PLAY_SPIRIT_ANIM", 5, 0, PlaySpiritAnim);
	CREATE_FUNCTION_BINDING("CALL_IN_NOT_NEAR", 8, 1, CallInNotNear);
	CREATE_FUNCTION_BINDING("SET_CAMERA_ZONE", 1, 0, SetCameraZone);
	CREATE_FUNCTION_BINDING("GET_OBJECT_STATE", 1, 1, GetObjectState);
	CREATE_FUNCTION_BINDING("REVEAL_COUNTDOWN_TIMER", 0, 0, RevealCountdownTimer);
	CREATE_FUNCTION_BINDING("SET_TIMER_TIME", 2, 0, SetTimerTime);
	CREATE_FUNCTION_BINDING("CREATE_TIMER", 1, 1, CreateTimer);
	CREATE_FUNCTION_BINDING("GET_TIMER_TIME_REMAINING", 1, 1, GetTimerTimeRemaining);
	CREATE_FUNCTION_BINDING("GET_TIMER_TIME_SINCE_SET", 1, 1, GetTimerTimeSinceSet);
	CREATE_FUNCTION_BINDING("MOVE_MUSIC", 2, 0, MoveMusic);
	CREATE_FUNCTION_BINDING("GET_INCLUSION_DISTANCE", 0, 1, GetInclusionDistance);
	CREATE_FUNCTION_BINDING("GET_LAND_HEIGHT", 3, 1, GetLandHeight);
	CREATE_FUNCTION_BINDING("LOAD_MAP", 1, 0, LoadMap);
	CREATE_FUNCTION_BINDING("STOP_ALL_SCRIPTS_EXCLUDING", 1, 0, StopAllScriptsExcluding);
	CREATE_FUNCTION_BINDING("STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING", 1, 0, StopAllScriptsInFilesExcluding);
	CREATE_FUNCTION_BINDING("STOP_SCRIPT", 1, 0, StopScript);
	CREATE_FUNCTION_BINDING("CLEAR_CLICKED_OBJECT", 0, 0, ClearClickedObject);
	CREATE_FUNCTION_BINDING("CLEAR_CLICKED_POSITION", 0, 0, ClearClickedPosition);
	CREATE_FUNCTION_BINDING("POSITION_CLICKED", 4, 1, PositionClicked);
	CREATE_FUNCTION_BINDING("RELEASE_FROM_SCRIPT", 1, 0, ReleaseFromScript);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HAND_IS_OVER", 0, 1, GetObjectHandIsOver);
	CREATE_FUNCTION_BINDING("ID_POISONED_SIZE", 1, 1, IdPoisonedSize);
	CREATE_FUNCTION_BINDING("IS_POISONED", 1, 1, IsPoisoned);
	CREATE_FUNCTION_BINDING("CALL_POISONED_IN", 4, 1, CallPoisonedIn);
	CREATE_FUNCTION_BINDING("CALL_NOT_POISONED_IN", 4, 1, CallNotPoisonedIn);
	CREATE_FUNCTION_BINDING("SPIRIT_PLAYED", 1, 1, SpiritPlayed);
	CREATE_FUNCTION_BINDING("CLING_SPIRIT", 3, 0, ClingSpirit);
	CREATE_FUNCTION_BINDING("FLY_SPIRIT", 3, 0, FlySpirit);
	CREATE_FUNCTION_BINDING("SET_ID_MOVEABLE", 2, 0, SetIdMoveable);
	CREATE_FUNCTION_BINDING("SET_ID_PICKUPABLE", 2, 0, SetIdPickupable);
	CREATE_FUNCTION_BINDING("IS_ON_FIRE", 1, 1, IsOnFire);
	CREATE_FUNCTION_BINDING("IS_FIRE_NEAR", 4, 1, IsFireNear);
	CREATE_FUNCTION_BINDING("STOP_SCRIPTS_IN_FILES", 1, 0, StopScriptsInFiles);
	CREATE_FUNCTION_BINDING("SET_POISONED", 2, 0, SetPoisoned);
	CREATE_FUNCTION_BINDING("SET_TEMPERATURE", 2, 0, SetTemperature);
	CREATE_FUNCTION_BINDING("SET_ON_FIRE", 3, 0, SetOnFire);
	CREATE_FUNCTION_BINDING("SET_TARGET", 5, 0, SetTarget);
	CREATE_FUNCTION_BINDING("WALK_PATH", 5, 0, WalkPath);
	CREATE_FUNCTION_BINDING("FOCUS_AND_POSITION_FOLLOW", 2, 0, FocusAndPositionFollow);
	CREATE_FUNCTION_BINDING("GET_WALK_PATH_PERCENTAGE", 1, 1, GetWalkPathPercentage);
	CREATE_FUNCTION_BINDING("CAMERA_PROPERTIES", 4, 0, CameraProperties);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_MUSIC", 2, 0, EnableDisableMusic);
	CREATE_FUNCTION_BINDING("GET_MUSIC_OBJ_DISTANCE", 1, 1, GetMusicObjDistance);
	CREATE_FUNCTION_BINDING("GET_MUSIC_ENUM_DISTANCE", 1, 1, GetMusicEnumDistance);
	CREATE_FUNCTION_BINDING("SET_MUSIC_PLAY_POSITION", 4, 0, SetMusicPlayPosition);
	CREATE_FUNCTION_BINDING("ATTACH_OBJECT_LEASH_TO_OBJECT", 2, 0, AttachObjectLeashToObject);
	CREATE_FUNCTION_BINDING("ATTACH_OBJECT_LEASH_TO_HAND", 1, 0, AttachObjectLeashToHand);
	CREATE_FUNCTION_BINDING("DETACH_OBJECT_LEASH", 1, 0, DetachObjectLeash);
	CREATE_FUNCTION_BINDING("SET_CREATURE_ONLY_DESIRE", 3, 0, SetCreatureOnlyDesire);
	CREATE_FUNCTION_BINDING("SET_CREATURE_ONLY_DESIRE_OFF", 1, 0, SetCreatureOnlyDesireOff);
	CREATE_FUNCTION_BINDING("RESTART_MUSIC", 1, 0, RestartMusic);
	CREATE_FUNCTION_BINDING("MUSIC_PLAYED", 1, 1, MusicPlayed191);
	CREATE_FUNCTION_BINDING("IS_OF_TYPE", 3, 1, IsOfType);
	CREATE_FUNCTION_BINDING("CLEAR_HIT_OBJECT", 0, 0, ClearHitObject);
	CREATE_FUNCTION_BINDING("GAME_THING_HIT", 1, 1, GameThingHit);
	CREATE_FUNCTION_BINDING("SPELL_AT_THING", 8, 1, SpellAtThing);
	CREATE_FUNCTION_BINDING("SPELL_AT_POS", 10, 1, SpellAtPos);
	CREATE_FUNCTION_BINDING("CALL_PLAYER_CREATURE", 1, 1, CallPlayerCreature);
	CREATE_FUNCTION_BINDING("GET_SLOWEST_SPEED", 1, 1, GetSlowestSpeed);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HELD", 0, 1, GetObjectHeld199);
}

void CHLApi::InitFunctionsTable2()
{
	CREATE_FUNCTION_BINDING("HELP_SYSTEM_ON", 0, 1, HelpSystemOn);
	CREATE_FUNCTION_BINDING("SHAKE_CAMERA", 6, 0, ShakeCamera);
	CREATE_FUNCTION_BINDING("SET_ANIMATION_MODIFY", 2, 0, SetAnimationModify);
	CREATE_FUNCTION_BINDING("SET_AVI_SEQUENCE", 2, 0, SetAviSequence);
	CREATE_FUNCTION_BINDING("PLAY_GESTURE", 5, 0, PlayGesture);
	CREATE_FUNCTION_BINDING("DEV_FUNCTION", 1, 0, DevFunction);
	CREATE_FUNCTION_BINDING("HAS_MOUSE_WHEEL", 0, 1, HasMouseWheel);
	CREATE_FUNCTION_BINDING("NUM_MOUSE_BUTTONS", 0, 1, NumMouseButtons);
	CREATE_FUNCTION_BINDING("SET_CREATURE_DEV_STAGE", 2, 0, SetCreatureDevStage);
	CREATE_FUNCTION_BINDING("SET_FIXED_CAM_ROTATION", 4, 0, SetFixedCamRotation);
	CREATE_FUNCTION_BINDING("SWAP_CREATURE", 2, 0, SwapCreature);
	CREATE_FUNCTION_BINDING("GET_ARENA", 5, 1, GetArena);
	CREATE_FUNCTION_BINDING("GET_FOOTBALL_PITCH", 1, 1, GetFootballPitch);
	CREATE_FUNCTION_BINDING("STOP_ALL_GAMES", 1, 0, StopAllGames);
	CREATE_FUNCTION_BINDING("ATTACH_TO_GAME", 3, 0, AttachToGame);
	CREATE_FUNCTION_BINDING("DETACH_FROM_GAME", 3, 0, DetachFromGame);
	CREATE_FUNCTION_BINDING("DETACH_UNDEFINED_FROM_GAME", 2, 0, DetachUndefinedFromGame);
	CREATE_FUNCTION_BINDING("SET_ONLY_FOR_SCRIPTS", 2, 0, SetOnlyForScripts);
	CREATE_FUNCTION_BINDING("START_MATCH_WITH_REFEREE", 2, 0, StartMatchWithReferee);
	CREATE_FUNCTION_BINDING("GAME_TEAM_SIZE", 2, 0, GameTeamSize);
	CREATE_FUNCTION_BINDING("GAME_TYPE", 1, 1, GameType);
	CREATE_FUNCTION_BINDING("GAME_SUB_TYPE", 1, 1, GameSubType);
	CREATE_FUNCTION_BINDING("IS_LEASHED", 1, 1, IsLeashed);
	CREATE_FUNCTION_BINDING("SET_CREATURE_HOME", 4, 0, SetCreatureHome);
	CREATE_FUNCTION_BINDING("GET_HIT_OBJECT", 0, 1, GetHitObject);
	CREATE_FUNCTION_BINDING("GET_OBJECT_WHICH_HIT", 0, 1, GetObjectWhichHit);
	CREATE_FUNCTION_BINDING("GET_NEAREST_TOWN_OF_PLAYER", 5, 1, GetNearestTownOfPlayer);
	CREATE_FUNCTION_BINDING("SPELL_AT_POINT", 5, 1, SpellAtPoint);
	CREATE_FUNCTION_BINDING("SET_ATTACK_OWN_TOWN", 2, 0, SetAttackOwnTown);
	CREATE_FUNCTION_BINDING("IS_FIGHTING", 1, 1, IsFighting);
	CREATE_FUNCTION_BINDING("SET_MAGIC_RADIUS", 2, 0, SetMagicRadius);
	CREATE_FUNCTION_BINDING("TEMP_TEXT_WITH_NUMBER", 4, 0, TempTextWithNumber);
	CREATE_FUNCTION_BINDING("RUN_TEXT_WITH_NUMBER", 4, 0, RunTextWithNumber);
	CREATE_FUNCTION_BINDING("CREATURE_SPELL_REVERSION", 2, 0, CreatureSpellReversion);
	CREATE_FUNCTION_BINDING("GET_DESIRE", 2, 1, GetDesire);
	CREATE_FUNCTION_BINDING("GET_EVENTS_PER_SECOND", 1, 1, GetEventsPerSecond);
	CREATE_FUNCTION_BINDING("GET_TIME_SINCE", 1, 1, GetTimeSince);
	CREATE_FUNCTION_BINDING("GET_TOTAL_EVENTS", 1, 1, GetTotalEvents);
	CREATE_FUNCTION_BINDING("UPDATE_SNAPSHOT", -1, 0, UpdateSnapshot);
	CREATE_FUNCTION_BINDING("CREATE_REWARD", 5, 1, CreateReward);
	CREATE_FUNCTION_BINDING("CREATE_REWARD_IN_TOWN", 6, 1, CreateRewardInTown);
	CREATE_FUNCTION_BINDING("SET_FADE", 4, 0, SetFade);
	CREATE_FUNCTION_BINDING("SET_FADE_IN", 1, 0, SetFadeIn);
	CREATE_FUNCTION_BINDING("FADE_FINISHED", 0, 1, FadeFinished);
	CREATE_FUNCTION_BINDING("SET_PLAYER_MAGIC", 3, 0, SetPlayerMagic);
	CREATE_FUNCTION_BINDING("HAS_PLAYER_MAGIC", 2, 1, HasPlayerMagic);
	CREATE_FUNCTION_BINDING("SPIRIT_SPEAKS", 2, 1, SpiritSpeaks);
	CREATE_FUNCTION_BINDING("BELIEF_FOR_PLAYER", 2, 1, BeliefForPlayer);
	CREATE_FUNCTION_BINDING("GET_HELP", 1, 1, GetHelp);
	CREATE_FUNCTION_BINDING("SET_LEASH_WORKS", 2, 0, SetLeashWorks);
	CREATE_FUNCTION_BINDING("LOAD_MY_CREATURE", 3, 0, LoadMyCreature);
	CREATE_FUNCTION_BINDING("OBJECT_RELATIVE_BELIEF", 3, 0, ObjectRelativeBelief);
	CREATE_FUNCTION_BINDING("CREATE_WITH_ANGLE_AND_SCALE", 7, 1, CreateWithAngleAndScale);
	CREATE_FUNCTION_BINDING("SET_HELP_SYSTEM", 1, 0, SetHelpSystem);
	CREATE_FUNCTION_BINDING("SET_VIRTUAL_INFLUENCE", 2, 0, SetVirtualInfluence);
	CREATE_FUNCTION_BINDING("SET_ACTIVE", 2, 0, SetActive);
	CREATE_FUNCTION_BINDING("THING_VALID", 1, 1, ThingValid);
	CREATE_FUNCTION_BINDING("VORTEX_FADE_OUT", 1, 0, VortexFadeOut);
	CREATE_FUNCTION_BINDING("REMOVE_REACTION_OF_TYPE", 2, 0, RemoveReactionOfType);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_EVERYTHING_EXCLUDING", 2, 0, CreatureLearnEverythingExcluding);
	CREATE_FUNCTION_BINDING("PLAYED_PERCENTAGE", 1, 1, PlayedPercentage);
	CREATE_FUNCTION_BINDING("OBJECT_CAST_BY_OBJECT", 2, 1, ObjectCastByObject);
	CREATE_FUNCTION_BINDING("IS_WIND_MAGIC_AT_POS", 1, 1, IsWindMagicAtPos);
	CREATE_FUNCTION_BINDING("CREATE_MIST", 9, 1, CreateMist);
	CREATE_FUNCTION_BINDING("SET_MIST_FADE", 6, 0, SetMistFade);
	CREATE_FUNCTION_BINDING("GET_OBJECT_FADE", 1, 1, GetObjectFade);
	CREATE_FUNCTION_BINDING("PLAY_HAND_DEMO", 3, 0, PlayHandDemo);
	CREATE_FUNCTION_BINDING("IS_PLAYING_HAND_DEMO", 0, 1, IsPlayingHandDemo);
	CREATE_FUNCTION_BINDING("GET_ARSE_POSITION", 1, 3, GetArsePosition);
	CREATE_FUNCTION_BINDING("IS_LEASHED_TO_OBJECT", 2, 1, IsLeashedToObject);
	CREATE_FUNCTION_BINDING("GET_INTERACTION_MAGNITUDE", 1, 1, GetInteractionMagnitude);
	CREATE_FUNCTION_BINDING("IS_CREATURE_AVAILABLE", 1, 1, IsCreatureAvailable);
	CREATE_FUNCTION_BINDING("CREATE_HIGHLIGHT", 5, 1, CreateHighlight);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HELD", 1, 1, GetObjectHeld273);
	CREATE_FUNCTION_BINDING("GET_ACTION_COUNT", 2, 1, GetActionCount);
	CREATE_FUNCTION_BINDING("GET_OBJECT_LEASH_TYPE", 1, 1, GetObjectLeashType);
	CREATE_FUNCTION_BINDING("SET_FOCUS_FOLLOW", 1, 0, SetFocusFollow);
	CREATE_FUNCTION_BINDING("SET_POSITION_FOLLOW", 1, 0, SetPositionFollow);
	CREATE_FUNCTION_BINDING("SET_FOCUS_AND_POSITION_FOLLOW", 2, 0, SetFocusAndPositionFollow);
	CREATE_FUNCTION_BINDING("SET_CAMERA_LENS", 1, 0, SetCameraLens);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_LENS", 2, 0, MoveCameraLens);
	CREATE_FUNCTION_BINDING("CREATURE_REACTION", 2, 0, CreatureReaction);
	CREATE_FUNCTION_BINDING("CREATURE_IN_DEV_SCRIPT", 2, 0, CreatureInDevScript);
	CREATE_FUNCTION_BINDING("STORE_CAMERA_DETAILS", 0, 0, StoreCameraDetails);
	CREATE_FUNCTION_BINDING("RESTORE_CAMERA_DETAILS", 0, 0, RestoreCameraDetails);
	CREATE_FUNCTION_BINDING("START_ANGLE_SOUND", 1, 0, StartAngleSound285);
	CREATE_FUNCTION_BINDING("SET_CAMERA_POS_FOC_LENS", 7, 0, SetCameraPosFocLens);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_POS_FOC_LENS", 8, 0, MoveCameraPosFocLens);
	CREATE_FUNCTION_BINDING("GAME_TIME_ON_OFF", 1, 0, GameTimeOnOff);
	CREATE_FUNCTION_BINDING("MOVE_GAME_TIME", 2, 0, MoveGameTime);
	CREATE_FUNCTION_BINDING("SET_HIGH_GRAPHICS_DETAIL", 2, 0, SetHighGraphicsDetail);
	CREATE_FUNCTION_BINDING("SET_SKELETON", 2, 0, SetSkeleton);
	CREATE_FUNCTION_BINDING("IS_SKELETON", 1, 1, IsSkeleton);
	CREATE_FUNCTION_BINDING("PLAYER_SPELL_CAST_TIME", 1, 1, PlayerSpellCastTime);
	CREATE_FUNCTION_BINDING("PLAYER_SPELL_LAST_CAST", 1, 1, PlayerSpellLastCast);
	CREATE_FUNCTION_BINDING("GET_LAST_SPELL_CAST_POS", 1, 3, GetLastSpellCastPos);
	CREATE_FUNCTION_BINDING("ADD_SPOT_VISUAL_TARGET_POS", 4, 0, AddSpotVisualTargetPos);
	CREATE_FUNCTION_BINDING("ADD_SPOT_VISUAL_TARGET_OBJECT", 2, 0, AddSpotVisualTargetObject);
	CREATE_FUNCTION_BINDING("SET_INDESTRUCTABLE", 2, 0, SetIndestructable);
	CREATE_FUNCTION_BINDING("SET_GRAPHICS_CLIPPING", 2, 0, SetGraphicsClipping);
}

void CHLApi::InitFunctionsTable3()
{
	CREATE_FUNCTION_BINDING("SPIRIT_APPEAR", 1, 0, SpiritAppear);
	CREATE_FUNCTION_BINDING("SPIRIT_DISAPPEAR", 1, 0, SpiritDisappear);
	CREATE_FUNCTION_BINDING("SET_FOCUS_ON_OBJECT", 2, 0, SetFocusOnObject);
	CREATE_FUNCTION_BINDING("RELEASE_OBJECT_FOCUS", 1, 0, ReleaseObjectFocus);
	CREATE_FUNCTION_BINDING("IMMERSION_EXISTS", 0, 1, ImmersionExists);
	CREATE_FUNCTION_BINDING("SET_DRAW_LEASH", 1, 0, SetDrawLeash);
	CREATE_FUNCTION_BINDING("SET_DRAW_HIGHLIGHT", 1, 0, SetDrawHighlight);
	CREATE_FUNCTION_BINDING("SET_OPEN_CLOSE", 2, 0, SetOpenClose);
	CREATE_FUNCTION_BINDING("SET_INTRO_BUILDING", 1, 0, SetIntroBuilding);
	CREATE_FUNCTION_BINDING("CREATURE_FORCE_FRIENDS", 3, 0, CreatureForceFriends);
	CREATE_FUNCTION_BINDING("MOVE_COMPUTER_PLAYER_POSITION", 6, 0, MoveComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_COMPUTER_PLAYER", 2, 0, EnableDisableComputerPlayer311);
	CREATE_FUNCTION_BINDING("GET_COMPUTER_PLAYER_POSITION", 1, 3, GetComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_POSITION", 5, 0, SetComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("GET_STORED_CAMERA_POSITION", 0, 3, GetStoredCameraPosition);
	CREATE_FUNCTION_BINDING("GET_STORED_CAMERA_FOCUS", 0, 3, GetStoredCameraFocus);
	CREATE_FUNCTION_BINDING("CALL_NEAR_IN_STATE", 8, 1, CallNearInState);
	CREATE_FUNCTION_BINDING("SET_CREATURE_SOUND", 1, 0, SetCreatureSound);
	CREATE_FUNCTION_BINDING("CREATURE_INTERACTING_WITH", 2, 1, CreatureInteractingWith);
	CREATE_FUNCTION_BINDING("SET_SUN_DRAW", 1, 0, SetSunDraw);
	CREATE_FUNCTION_BINDING("OBJECT_INFO_BITS", 1, 1, ObjectInfoBits);
	CREATE_FUNCTION_BINDING("SET_HURT_BY_FIRE", 2, 0, SetHurtByFire);
	CREATE_FUNCTION_BINDING("CONFINED_OBJECT", 5, 0, ConfinedObject);
	CREATE_FUNCTION_BINDING("CLEAR_CONFINED_OBJECT", 1, 0, ClearConfinedObject);
	CREATE_FUNCTION_BINDING("GET_OBJECT_FLOCK", 1, 1, GetObjectFlock);
	CREATE_FUNCTION_BINDING("SET_PLAYER_BELIEF", 3, 0, SetPlayerBelief);
	CREATE_FUNCTION_BINDING("PLAY_JC_SPECIAL", 1, 0, PlayJcSpecial);
	CREATE_FUNCTION_BINDING("IS_PLAYING_JC_SPECIAL", 1, 1, IsPlayingJcSpecial);
	CREATE_FUNCTION_BINDING("VORTEX_PARAMETERS", 8, 0, VortexParameters);
	CREATE_FUNCTION_BINDING("LOAD_CREATURE", 6, 0, LoadCreature);
	CREATE_FUNCTION_BINDING("IS_SPELL_CHARGING", 1, 1, IsSpellCharging);
	CREATE_FUNCTION_BINDING("IS_THAT_SPELL_CHARGING", 2, 1, IsThatSpellCharging);
	CREATE_FUNCTION_BINDING("OPPOSING_CREATURE", 1, 1, OpposingCreature);
	CREATE_FUNCTION_BINDING("FLOCK_WITHIN_LIMITS", 1, 1, FlockWithinLimits);
	CREATE_FUNCTION_BINDING("HIGHLIGHT_PROPERTIES", 3, 0, HighlightProperties);
	CREATE_FUNCTION_BINDING("LAST_MUSIC_LINE", 1, 1, LastMusicLine);
	CREATE_FUNCTION_BINDING("HAND_DEMO_TRIGGER", 0, 1, HandDemoTrigger);
	CREATE_FUNCTION_BINDING("GET_BELLY_POSITION", 1, 3, GetBellyPosition);
	CREATE_FUNCTION_BINDING("SET_CREATURE_CREED_PROPERTIES", 5, 0, SetCreatureCreedProperties);
	CREATE_FUNCTION_BINDING("GAME_THING_CAN_VIEW_CAMERA", 2, 1, GameThingCanViewCamera);
	CREATE_FUNCTION_BINDING("GAME_PLAY_SAY_SOUND_EFFECT", 6, 0, GamePlaySaySoundEffect);
	CREATE_FUNCTION_BINDING("SET_TOWN_DESIRE_BOOST", 3, 0, SetTownDesireBoost);
	CREATE_FUNCTION_BINDING("IS_LOCKED_INTERACTION", 1, 1, IsLockedInteraction);
	CREATE_FUNCTION_BINDING("SET_CREATURE_NAME", 2, 0, SetCreatureName);
	CREATE_FUNCTION_BINDING("COMPUTER_PLAYER_READY", 1, 1, ComputerPlayerReady);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_COMPUTER_PLAYER", 2, 0, EnableDisableComputerPlayer345);
	CREATE_FUNCTION_BINDING("CLEAR_ACTOR_MIND", 1, 0, ClearActorMind);
	CREATE_FUNCTION_BINDING("ENTER_EXIT_CITADEL", 1, 0, EnterExitCitadel);
	CREATE_FUNCTION_BINDING("START_ANGLE_SOUND", 1, 0, StartAngleSound348);
	CREATE_FUNCTION_BINDING("THING_JC_SPECIAL", 3, 0, ThingJcSpecial);
	CREATE_FUNCTION_BINDING("MUSIC_PLAYED", 1, 1, MusicPlayed350);
	CREATE_FUNCTION_BINDING("UPDATE_SNAPSHOT_PICTURE", 11, 0, UpdateSnapshotPicture);
	CREATE_FUNCTION_BINDING("STOP_SCRIPTS_IN_FILES_EXCLUDING", 2, 0, StopScriptsInFilesExcluding);
	CREATE_FUNCTION_BINDING("CREATE_RANDOM_VILLAGER_OF_TRIBE", 4, 1, CreateRandomVillagerOfTribe);
	CREATE_FUNCTION_BINDING("TOGGLE_LEASH", 1, 0, ToggleLeash);
	CREATE_FUNCTION_BINDING("GAME_SET_MANA", 2, 0, GameSetMana);
	CREATE_FUNCTION_BINDING("SET_MAGIC_PROPERTIES", 3, 0, SetMagicProperties);
	CREATE_FUNCTION_BINDING("SET_GAME_SOUND", 1, 0, SetGameSound);
	CREATE_FUNCTION_BINDING("SEX_IS_MALE", 1, 1, SexIsMale);
	CREATE_FUNCTION_BINDING("GET_FIRST_HELP", 1, 1, GetFirstHelp);
	CREATE_FUNCTION_BINDING("GET_LAST_HELP", 1, 1, GetLastHelp);
	CREATE_FUNCTION_BINDING("IS_ACTIVE", 1, 1, IsActive);
	CREATE_FUNCTION_BINDING("SET_BOOKMARK_POSITION", 4, 0, SetBookmarkPosition);
	CREATE_FUNCTION_BINDING("SET_SCAFFOLD_PROPERTIES", 4, 0, SetScaffoldProperties);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_PERSONALITY", 3, 0, SetComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_SUPPRESSION", 3, 0, SetComputerPlayerSuppression);
	CREATE_FUNCTION_BINDING("FORCE_COMPUTER_PLAYER_ACTION", 4, 0, ForceComputerPlayerAction);
	CREATE_FUNCTION_BINDING("QUEUE_COMPUTER_PLAYER_ACTION", 4, 0, QueueComputerPlayerAction);
	CREATE_FUNCTION_BINDING("GET_TOWN_WITH_ID", 1, 1, GetTownWithId);
	CREATE_FUNCTION_BINDING("SET_DISCIPLE", 3, 0, SetDisciple);
	CREATE_FUNCTION_BINDING("RELEASE_COMPUTER_PLAYER", 1, 0, ReleaseComputerPlayer);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_SPEED", 2, 0, SetComputerPlayerSpeed);
	CREATE_FUNCTION_BINDING("SET_FOCUS_FOLLOW_COMPUTER_PLAYER", 1, 0, SetFocusFollowComputerPlayer);
	CREATE_FUNCTION_BINDING("SET_POSITION_FOLLOW_COMPUTER_PLAYER", 1, 0, SetPositionFollowComputerPlayer);
	CREATE_FUNCTION_BINDING("CALL_COMPUTER_PLAYER", 1, 1, CallComputerPlayer);
	CREATE_FUNCTION_BINDING("CALL_BUILDING_IN_TOWN", 4, 1, CallBuildingInTown);
	CREATE_FUNCTION_BINDING("SET_CAN_BUILD_WORSHIPSITE", 2, 0, SetCanBuildWorshipsite);
	CREATE_FUNCTION_BINDING("GET_FACING_CAMERA_POSITION", 1, 3, GetFacingCameraPosition);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_ATTITUDE", 3, 0, SetComputerPlayerAttitude);
	CREATE_FUNCTION_BINDING("GET_COMPUTER_PLAYER_ATTITUDE", 2, 1, GetComputerPlayerAttitude);
	CREATE_FUNCTION_BINDING("LOAD_COMPUTER_PLAYER_PERSONALITY", 2, 0, LoadComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SAVE_COMPUTER_PLAYER_PERSONALITY", 2, 0, SaveComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SET_PLAYER_ALLY", 3, 0, SetPlayerAlly);
	CREATE_FUNCTION_BINDING("CALL_FLYING", 7, 1, CallFlying);
	CREATE_FUNCTION_BINDING("SET_OBJECT_FADE_IN", 2, 0, SetObjectFadeIn);
	CREATE_FUNCTION_BINDING("IS_AFFECTED_BY_SPELL", 2, 1, IsAffectedBySpell);
	CREATE_FUNCTION_BINDING("SET_MAGIC_IN_OBJECT", 3, 0, SetMagicInObject);
	CREATE_FUNCTION_BINDING("ID_ADULT_SIZE", 1, 1, IdAdultSize);
	CREATE_FUNCTION_BINDING("OBJECT_CAPACITY", 1, 1, ObjectCapacity);
	CREATE_FUNCTION_BINDING("OBJECT_ADULT_CAPACITY", 1, 1, ObjectAdultCapacity);
	CREATE_FUNCTION_BINDING("SET_CREATURE_AUTO_FIGHTING", 2, 0, SetCreatureAutoFighting);
	CREATE_FUNCTION_BINDING("IS_AUTO_FIGHTING", 1, 1, IsAutoFighting);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_MOVE", 2, 0, SetCreatureQueueFightMove);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_SPELL", 2, 0, SetCreatureQueueFightSpell);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_STEP", 2, 0, SetCreatureQueueFightStep);
	CREATE_FUNCTION_BINDING("GET_CREATURE_FIGHT_ACTION", 1, 1, GetCreatureFightAction);
	CREATE_FUNCTION_BINDING("CREATURE_FIGHT_QUEUE_HITS", 1, 1, CreatureFightQueueHits);
	CREATE_FUNCTION_BINDING("SQUARE_ROOT", 1, 1, SquareRoot);
	CREATE_FUNCTION_BINDING("GET_PLAYER_ALLY", 2, 1, GetPlayerAlly);
	CREATE_FUNCTION_BINDING("SET_PLAYER_WIND_RESISTANCE", 2, 1, SetPlayerWindResistance);
}

void CHLApi::InitFunctionsTable4()
{
	CREATE_FUNCTION_BINDING("GET_PLAYER_WIND_RESISTANCE", 2, 1, GetPlayerWindResistance);
	CREATE_FUNCTION_BINDING("PAUSE_UNPAUSE_CLIMATE_SYSTEM", 1, 0, PauseUnpauseClimateSystem);
	CREATE_FUNCTION_BINDING("PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM", 1, 0, PauseUnpauseStormCreationInClimateSystem);
	CREATE_FUNCTION_BINDING("GET_MANA_FOR_SPELL", 1, 1, GetManaForSpell);
	CREATE_FUNCTION_BINDING("KILL_STORMS_IN_AREA", 4, 0, KillStormsInArea);
	CREATE_FUNCTION_BINDING("INSIDE_TEMPLE", 0, 1, InsideTemple);
	CREATE_FUNCTION_BINDING("RESTART_OBJECT", 1, 0, RestartObject);
	CREATE_FUNCTION_BINDING("SET_GAME_TIME_PROPERTIES", 3, 0, SetGameTimeProperties);
	CREATE_FUNCTION_BINDING("RESET_GAME_TIME_PROPERTIES", 0, 0, ResetGameTimeProperties);
	CREATE_FUNCTION_BINDING("SOUND_EXISTS", 0, 1, SoundExists);
	CREATE_FUNCTION_BINDING("GET_TOWN_WORSHIP_DEATHS", 1, 1, GetTownWorshipDeaths);
	CREATE_FUNCTION_BINDING("GAME_CLEAR_DIALOGUE", 0, 0, GameClearDialogue);
	CREATE_FUNCTION_BINDING("GAME_CLOSE_DIALOGUE", 0, 0, GameCloseDialogue);
	CREATE_FUNCTION_BINDING("GET_HAND_STATE", 0, 1, GetHandState);
	CREATE_FUNCTION_BINDING("SET_INTERFACE_CITADEL", 1, 0, SetInterfaceCitadel);
	CREATE_FUNCTION_BINDING("MAP_SCRIPT_FUNCTION", 1, 0, MapScriptFunction);
	CREATE_FUNCTION_BINDING("WITHIN_ROTATION", 0, 1, WithinRotation);
	CREATE_FUNCTION_BINDING("GET_PLAYER_TOWN_TOTAL", 1, 1, GetPlayerTownTotal);
	CREATE_FUNCTION_BINDING("SPIRIT_SCREEN_POINT", 3, 0, SpiritScreenPoint);
	CREATE_FUNCTION_BINDING("KEY_DOWN", 1, 1, KeyDown);
	CREATE_FUNCTION_BINDING("SET_FIGHT_EXIT", 1, 0, SetFightExit);
	CREATE_FUNCTION_BINDING("GET_OBJECT_CLICKED", 0, 1, GetObjectClicked);
	CREATE_FUNCTION_BINDING("GET_MANA", 1, 1, GetMana);
	CREATE_FUNCTION_BINDING("CLEAR_PLAYER_SPELL_CHARGING", 1, 0, ClearPlayerSpellCharging);
	CREATE_FUNCTION_BINDING("STOP_SOUND_EFFECT", 3, 0, StopSoundEffect);
	CREATE_FUNCTION_BINDING("GET_TOTEM_STATUE", 1, 1, GetTotemStatue);
	CREATE_FUNCTION_BINDING("SET_SET_ON_FIRE", 2, 0, SetSetOnFire);
	CREATE_FUNCTION_BINDING("SET_LAND_BALANCE", 2, 0, SetLandBalance);
	CREATE_FUNCTION_BINDING("SET_OBJECT_BELIEF_SCALE", 2, 0, SetObjectBeliefScale);
	CREATE_FUNCTION_BINDING("START_IMMERSION", 1, 0, StartImmersion);
	CREATE_FUNCTION_BINDING("STOP_IMMERSION", 1, 0, StopImmersion);
	CREATE_FUNCTION_BINDING("STOP_ALL_IMMERSION", 0, 0, StopAllImmersion);
	CREATE_FUNCTION_BINDING("SET_CREATURE_IN_TEMPLE", 1, 0, SetCreatureInTemple);
	CREATE_FUNCTION_BINDING("GAME_DRAW_TEXT", 7, 0, GameDrawText);
	CREATE_FUNCTION_BINDING("GAME_DRAW_TEMP_TEXT", 7, 0, GameDrawTempText);
	CREATE_FUNCTION_BINDING("FADE_ALL_DRAW_TEXT", 1, 0, FadeAllDrawText);
	CREATE_FUNCTION_BINDING("SET_DRAW_TEXT_COLOUR", 3, 0, SetDrawTextColour);
	CREATE_FUNCTION_BINDING("SET_CLIPPING_WINDOW", 5, 0, SetClippingWindow);
	CREATE_FUNCTION_BINDING("CLEAR_CLIPPING_WINDOW", 1, 0, ClearClippingWindow);
	CREATE_FUNCTION_BINDING("SAVE_GAME_IN_SLOT", 1, 0, SaveGameInSlot);
	CREATE_FUNCTION_BINDING("SET_OBJECT_CARRYING", 2, 0, SetObjectCarrying);
	CREATE_FUNCTION_BINDING("POS_VALID_FOR_CREATURE", 3, 1, PosValidForCreature);
	CREATE_FUNCTION_BINDING("GET_TIME_SINCE_OBJECT_ATTACKED", 2, 1, GetTimeSinceObjectAttacked);
	CREATE_FUNCTION_BINDING("GET_TOWN_AND_VILLAGER_HEALTH_TOTAL", 1, 1, GetTownAndVillagerHealthTotal);
	CREATE_FUNCTION_BINDING("GAME_ADD_FOR_BUILDING", 2, 0, GameAddForBuilding);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_ALIGNMENT_MUSIC", 1, 0, EnableDisableAlignmentMusic);
	CREATE_FUNCTION_BINDING("GET_DEAD_LIVING", 4, 1, GetDeadLiving);
	CREATE_FUNCTION_BINDING("ATTACH_SOUND_TAG", 4, 0, AttachSoundTag);
	CREATE_FUNCTION_BINDING("DETACH_SOUND_TAG", 3, 0, DetachSoundTag);
	CREATE_FUNCTION_BINDING("GET_SACRIFICE_TOTAL", 1, 1, GetSacrificeTotal);
	CREATE_FUNCTION_BINDING("GAME_SOUND_PLAYING", 2, 1, GameSoundPlaying);
	CREATE_FUNCTION_BINDING("GET_TEMPLE_POSITION", 1, 3, GetTemplePosition);
	CREATE_FUNCTION_BINDING("CREATURE_AUTOSCALE", 3, 0, CreatureAutoscale);
	CREATE_FUNCTION_BINDING("GET_SPELL_ICON_IN_TEMPLE", 2, 1, GetSpellIconInTemple);
	CREATE_FUNCTION_BINDING("GAME_CLEAR_COMPUTER_PLAYER_ACTIONS", 1, 0, GameClearComputerPlayerActions);
	CREATE_FUNCTION_BINDING("GET_FIRST_IN_CONTAINER", 1, 1, GetFirstInContainer);
	CREATE_FUNCTION_BINDING("GET_NEXT_IN_CONTAINER", 2, 1, GetNextInContainer);
	CREATE_FUNCTION_BINDING("GET_TEMPLE_ENTRANCE_POSITION", 3, 3, GetTempleEntrancePosition);
	CREATE_FUNCTION_BINDING("SAY_SOUND_EFFECT_PLAYING", 2, 1, SaySoundEffectPlaying);
	CREATE_FUNCTION_BINDING("SET_HAND_DEMO_KEYS", 1, 0, SetHandDemoKeys);
	CREATE_FUNCTION_BINDING("CAN_SKIP_TUTORIAL", 0, 1, CanSkipTutorial);
	CREATE_FUNCTION_BINDING("CAN_SKIP_CREATURE_TRAINING", 0, 1, CanSkipCreatureTraining);
	CREATE_FUNCTION_BINDING("IS_KEEPING_OLD_CREATURE", 0, 1, IsKeepingOldCreature);
	CREATE_FUNCTION_BINDING("CURRENT_PROFILE_HAS_CREATURE", 0, 1, CurrentProfileHasCreature);
}

} // namespace openblack::chlapi
