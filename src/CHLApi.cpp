/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLApi.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <mutex>
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

#include "3D/CameraTracks.h"
#include "3D/DayNightClock.h"
#include "PSys/PSysManager.h"
#include "3D/LandIslandInterface.h"
#include "3D/ScreenFade.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Audio.h"
#include "Audio/Services/GameMusic.h"
#include "Audio/Services/ScriptAudioState.h"
#include "Audio/LH/SamplePlay.h"
#include "Audio/Services/ScriptSound.h"
#include "Camera/Camera.h"
#include "Camera/CameraShake.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/AnimatedStaticArchetype.h"
#include "ECS/Archetypes/BonfireArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MarkerArchetype.h"
#include "ECS/Archetypes/MobileObjectArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/SharkArchetype.h"
#include "ECS/Archetypes/StreetLanternArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Shark.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Villager.h"
#include "ECS/FeatureBuild.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MobileWalkPaths.h"
#include "ECS/PetitNavire.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerDrowning.h"
#include "EngineConfig.h"
#include "Enums.h"
#include "Game.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Script/CHLInfluence.h"
#include "Magic/Script/CHLFire.h"
#include "Magic/Script/CHLSpells.h"
#include "Magic/Script/CHLWeather.h"
#include "Magic/Script/CHLWorship.h"
#include "ECS/Effects/Alignment.h"
#include "ScriptHeaders/ScriptEnums.h"

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

/// The scripts call some unimplemented functions every frame (GAME_THING_CLICKED: 37k lines): logged once per function
/// OPENBLACK_SCRIPT_THING_TRACE=1 (openblack only): MOVE_GAME_THING, SET_SCRIPT_STATE, SET_SCRIPT_ULONG and a PLAYED
/// that is true write a line, to follow a script that drives its things (Land 1's FollowUs)
bool ScriptThingTrace()
{
	static const bool on = [] {
		const char* env = std::getenv("OPENBLACK_SCRIPT_THING_TRACE");
		return env != nullptr && std::string_view(env) != "" && std::string_view(env) != "0";
	}();
	return on;
}

void NotImplemented(const char* function)
{
	static std::mutex mutex;
	static std::unordered_set<std::string> warned;
	const std::lock_guard lock(mutex);
	if (warned.insert(function).second)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHLApi Function {}() not implemented (logged once).", function);
	}
}

/// The script VM as GScript asks it (ScriptDLL::TaskNumber 0x6F69F0, GetCurrentTaskScriptType 0x6F6A90, GetScriptType
/// 0x6F6C50, StopTasksOfType 0x6F68F0)
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

/// **Not original**: true while the task running now is the land's opening one and mod game.skip-intro has "free start"
/// on (EngineConfig::skipIntroFreeStart, Help/ScriptControl.h). The opcodes that would take the opening away from the
/// player (its camera, the wide screen, the fades and the script's music) then do nothing, and its waits answer at once
bool FreeStart()
{
	return help::script_control::IsFreeStartTask(help::script_control::GetCameraControl(),
	                                             Locator::vm::value().GetCurrentTaskNumber());
}

/// The check of GScript's camera opcodes (0x6ECAA0 and the others): no camera mode -> "Script camera has been
/// removed!" (0xC0C0CC); a mode other than CameraModeScript (__RTDynamicCast to 0x9CE188) -> "We are in the wrong
/// camera mode!" (0xC0C0EC; SET_CAMERA_POSITION 0x6EC8F0 says nothing). Either way the opcode does nothing. openblack
/// always has the player's mode, so only the second can happen. The "Script moving camera in citadel" note (0xC0C110) of
/// 003/004/287 is not ported: the citadel never has a script mode (StartCameraControl 0x6ECD33)
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

/// GScript::GetScriptGameThing 0x70D220 for the camera opcodes: the thing, or nullopt with the original's "Thing no
/// longer valid" (0xC0C258). (aproximado) As MusicThing: 0 is null and a valid entity stands for a live thing (the
/// original looks the id up in its script table 0xD967F8)
std::optional<entt::entity> CameraThing(uint32_t object, const char* opcode)
{
	const auto entity = static_cast<entt::entity>(object);
	if (object != 0 && Locator::entitiesRegistry::value().Valid(entity))
	{
		return entity;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: Thing no longer valid", opcode);
	return std::nullopt;
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

/// The ground position of a script vector: MapCoords::Set 0x603340 keeps y relative to the land, and after the
/// creation PhysicsEditorCreate (GameThingWithPos 0x401980, MobileStatic 0x55D720, Bonfire...) resets it to 0, so
/// everything but a marker stands on the land (the CHL vectors come with y = 0)
glm::vec3 OnGround(glm::vec3 position)
{
	if (Locator::terrainSystem::has_value())
	{
		position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	return position;
}

/// GScript's create switch fn_006F11A0 (table 0x6F1A70), from CREATE 027 (angle 0, scale 1) and
/// CREATE_WITH_ANGLE_AND_SCALE 252. The common step after it (0x6F1591-0x6F1A42) rebuilds the 3D object's matrix at
/// GetAltitude(pos) + relY with only its own Y angle and scale (no X/Z tilt), which the archetypes below already give.
/// @return entt::null when nothing is created (the script then gets 0)
entt::entity CreateScriptObject(const ObjectType type, uint32_t subtype, const glm::vec3& position, float yAngleRadians,
                                float scale)
{
	const auto& info = Locator::infoConstants::value();
	// 0x6F11BD: sub_type 5000 is only for the types that ignore it
	if (subtype == 5000 && type != ObjectType::Timer && type != ObjectType::SpellDispenser && type != ObjectType::Whale &&
	    type != ObjectType::Ark && type != ObjectType::Marker && type != ObjectType::Ball && type != ObjectType::Poo &&
	    type != ObjectType::Scaffold)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHL CREATE: trying to create an invalid sub_type");
		return entt::null;
	}
	const auto ground = OnGround(position);
	switch (type)
	{
	case ObjectType::Marker: // fn_0070D8D0: a ScriptMarker at the vector as given
		return MarkerArchetype::Create(position);
	case ObjectType::Feature: // fn_00527350(pos, GFeatureInfo, angle, scale, 1, 0)
		if (subtype < info.feature.size())
		{
			return FeatureArchetype::Create(ground, static_cast<FeatureInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Villager:      // Villager::Create 0x74FBE0(pos, info, info.grownUpAge + 1): an adult
	case ObjectType::VillagerChild: // Villager::Create(pos, info, 10): a child
		if (subtype < info.villager.size())
		{
			const auto age = type == ObjectType::Villager ? info.villager.at(subtype).grownUpAge + 1 : 10u;
			return VillagerArchetype::Create(ground, ground, static_cast<VillagerInfo>(subtype), age, false);
		}
		break;
	case ObjectType::Animal: // fn_00419C20(pos, GAnimalInfo, 0, 0), birds too
	case ObjectType::Bird:
		if (subtype < info.animal.size())
		{
			return AnimalArchetype::Create(ground, static_cast<AnimalInfo>(subtype), 0, 0);
		}
		break;
	case ObjectType::MobileStatic: // 0x6F141E
	case ObjectType::Rock:
		if (subtype >= info.mobileStatic.size())
		{
			break;
		}
		if (subtype == static_cast<uint32_t>(MobileStaticInfo::SingingStoneBase))
		{
			// fn_00609340(pos, info): a GBaseOnly, without angle or scale
			return MobileStaticArchetype::Create(ground, MobileStaticInfo::SingingStoneBase, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		}
		if (subtype == static_cast<uint32_t>(MobileStaticInfo::StreetLantern) || subtype == 59)
		{
			// GStreetLantern::Create 0x7346E0(pos, info), the same as the map script's CREATE_STREET_LANTERN (59 is a
			// country lantern)
			return StreetLanternArchetype::Create(ground, static_cast<MobileStaticInfo>(subtype));
		}
		// fn_00608770(pos, info, 0, 0, angle, scale): a Rock (info +0x128 == 2), a MobileStatic, or for info 8 a Bonfire
		return MobileStaticArchetype::CreateFromInfo(ground, static_cast<MobileStaticInfo>(subtype), 0.0f, yAngleRadians,
		                                             scale);
	case ObjectType::MobileObject: // 0x607000(pos, GMobileObjectInfo, 0, angle, scale)
		if (subtype < info.mobileObject.size())
		{
			return MobileObjectArchetype::Create(ground, static_cast<MobileObjectInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Poo: // 0x607000 with GMobileObjectInfo 5
		return MobileObjectArchetype::Create(ground, MobileObjectInfo::LumpOfPoo, yAngleRadians, scale);
	case ObjectType::Ark: // 0x607000 with GMobileObjectInfo 23
		return MobileObjectArchetype::Create(ground, MobileObjectInfo::Ark, yAngleRadians, scale);
	case ObjectType::Tree: // Tree::Create 0x749EE0(pos, info, no forest, scale, angle, scale)
		if (subtype < info.tree.size())
		{
			return TreeArchetype::Create(0, ground, static_cast<TreeInfo>(subtype), true, yAngleRadians, scale, scale);
		}
		break;
	case ObjectType::AnimatedStatic: // 0x421F50(pos, GAnimatedStaticInfo, angle, scale, 1, 0)
		if (subtype < info.animatedStatic.size())
		{
			return AnimatedStaticArchetype::Create(ground, static_cast<AnimatedStaticInfo>(subtype), yAngleRadians, scale);
		}
		break;
	case ObjectType::Whale: // 0x6F1747: Whale::Create 0x774C50(pos, &GMobileObjectInfo[24], 0, angle, scale), the shark
		return SharkArchetype::Create(ground, yAngleRadians, scale);
	case ObjectType::PuzzleGame: // 0x6F184C: fn_006D6680(pos, sub_type, ftol(angle x 2048 / 2 pi), scale)
		return openblack::ecs::CreatePuzzleGame(position, static_cast<script::PuzzleGameType>(subtype), yAngleRadians, scale);
	case ObjectType::Abode: // "Invalid create type" (0x6F191D) in the original too
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
	// GScript 0x6F1010's miracle cases (Magic/Script/CHLWorship.cpp)
	case ObjectType::OneShotSpell:
		return magic::script::CreateOneShotSpell(subtype, position);
	case ObjectType::OneShotSpellInHand:
		return magic::script::CreateOneShotSpellInHand(subtype);
	case ObjectType::SpellDispenser:
		return magic::script::CreateSpellDispenser(subtype, position, yAngleRadians, scale);
	default:
		// TODO: Reward, Creature, DeadTree, Store, Timer, Vortex, Ball, Totem, Highlight, Scaffold
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for type {}", static_cast<int>(type));
		return entt::null;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "CHL CREATE: invalid sub_type {} for type {}", subtype, static_cast<int>(type));
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
	// GScript::SetCameraPosition 0x6EC8F0 -> CameraModeScript::SetCameraPosition 0x461370
	const auto position = PopVec();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": the opening does not move the player's camera
	}
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetPosition(position);
	}
}

void SetCameraFocus() // 002 SET_CAMERA_FOCUS
{
	// GScript::SetCameraFocus 0x6EC9A0 -> CameraModeScript::SetCameraFocus 0x4612B0
	const auto position = PopVec();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetFocus(position);
	}
}

void MoveCameraPosition() // 003 MOVE_CAMERA_POSITION
{
	// GScript::MoveCameraPosition 0x6ECAA0 -> CameraModeScript::MoveCameraPosition 0x4616F0 (seconds of the wall clock)
	const auto time = Popf();
	const auto position = PopVec();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	if (ScriptCameraMode(__func__))
	{
		script_camera::MovePosition(position, time);
	}
}

void MoveCameraFocus() // 004 MOVE_CAMERA_FOCUS
{
	// GScript::MoveCameraFocus 0x6ECBA0 -> CameraModeScript::MoveCameraFocus 0x461430
	const auto time = Popf();
	const auto position = PopVec();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
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

void SpiritEject() // 007 SPIRIT_EJECT
{
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SpiritHome() // 008 SPIRIT_HOME
{
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SpiritPointPos() // 009 SPIRIT_POINT_POS
{
	// const auto inWorld = static_cast<bool>(Pop().intVal);
	// const auto position = PopVec();
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SpiritPointGameThing() // 010 SPIRIT_POINT_GAME_THING
{
	// const auto inWorld = static_cast<bool>(Pop().intVal);
	// const auto target = Pop().uintVal;
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GameThingFieldOfView() // 011 GAME_THING_FIELD_OF_VIEW
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void PosFieldOfView() // 012 POS_FIELD_OF_VIEW
{
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

// CHAR2WCHAR 0x8300A0 of a script string: MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, s, -1, buffer, 0x7FF).
// (approximated) byte by byte, the same as CP_ACP 1252 except for 0x80..0x9F, and without the 0x7FF limit
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
	// GScript::RunText 0x6F7D60
	const auto withInteraction = Pop().intVal;
	const auto textID = static_cast<uint32_t>(Pop().intVal);
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->RunText(singleLine, textID, withInteraction);
	}
}

void TempText() // 014 TEMP_TEXT
{
	// GScript::TempText 0x6F7E40
	const auto withInteraction = Pop().intVal;
	const auto string = PopString();
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->TempText(singleLine, WidenScriptString(string), withInteraction);
	}
}

void TextRead() // 015 TEXT_READ
{
	// GScript::TextRead 0x6F8260: HelpSystem::IsTextRead, pushed as a bool (type 6)
	const auto* helpSystem = help::Get();
	Pushb(helpSystem != nullptr && helpSystem->IsTextRead());
}

void GameThingClicked() // 016 GAME_THING_CLICKED
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetScriptState() // 017 SET_SCRIPT_STATE
{
	// GScript::SetScriptState 0x6F8370: the state (first pop), then the object
	const auto state = Pop().intVal;
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// 0x6F8396: GetScriptGameThing; none -> "Object no longer valid" (0xC0D428)
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCRIPT_STATE: Object no longer valid");
		return;
	}
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_SCRIPT_STATE {} {}", object, state);
	}
	// 0x6F83B9: IsScriptContainer (vt +0x3F8) -> g_game +0x250090 +0x24 = state and the type's loop function (table
	// 0xC0C73C by GetScriptObjectType vt +0x4E8, with 0x6F8280). TODO: openblack has no script containers
	// 0x6F841D: dynamic_cast<Living*> and not IsDrowning (vt +0x17C) -> GScript::SetScriptState(living, state) 0x6F82E0;
	// else "Object not living for set state" (0xC0D440)
	if (registry.AllOf<ecs::components::Villager>(entity) && !ecs::IsDrowning(entity))
	{
		ecs::villager::SetScriptState(entity, static_cast<VillagerStates>(static_cast<uint8_t>(state)));
		return;
	}
	if (registry.AnyOf<ecs::components::Animal, ecs::components::Creature>(entity))
	{
		// 0x6F82EA: a creature's (fn_0047B140 / fn_004F6E30 / fn_004F6F10 with its +0x128C clip) and an animal's
		// (StorePreviousState, exit, entry, SetAnim(1), +0x58 = 0 on the animal state machine). TODO: not ported
		NotImplemented("SetScriptState (animal or creature)");
		return;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCRIPT_STATE: Object not living for set state");
}

void SetScriptStatePos() // 018 SET_SCRIPT_STATE_POS
{
	// const auto position = PopVec();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetScriptFloat() // 019 SET_SCRIPT_FLOAT
{
	// const auto value = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetScriptUlong() // 020 SET_SCRIPT_ULONG
{
	// GScript::SetScriptUlong 0x6F8770: the times (first pop, edi), the clip (ebx), then the object
	const auto loop = static_cast<uint32_t>(Pop().intVal);
	const auto animation = static_cast<uint32_t>(Pop().intVal);
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// 0x6F87A9: GetScriptGameThing; none -> "Object no longer valid" (0xC0D428)
	if (object == 0 || !registry.Valid(entity))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_SCRIPT_ULONG: Object no longer valid");
		return;
	}
	if (ScriptThingTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_SCRIPT_ULONG {} clip {} times {}", object, animation, loop);
	}
	// 0x6F87CD: IsScriptContainer (vt +0x3F8) -> g_game +0x250090 +0x24 = clip, +0x28 = times and the type's loop
	// function (0xC0C73C, with 0x6F8730). TODO: openblack has no script containers
	// 0x6F8841: dynamic_cast<Villager*> -> +0x120 = times, +0x11C = clip
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		ecs::villager::SetScriptAnimation(entity, animation, loop);
		return;
	}
	// 0x6F886D: dynamic_cast<Creature*> -> +0x1290 = times, +0x128C = clip. TODO(M8, creature)
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		NotImplemented("SetScriptUlong (creature)");
		return;
	}
	// 0x6F8879: "setting the state of something neither a creature nor a villager" (0xC0D484)
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"),
	                    "SET_SCRIPT_ULONG: setting the state of something neither a creature nor a villager");
}

void GetProperty() // 021 GET_PROPERTY
{
	// GScript::GetProperty 0x70DAE0: the object, then the property; switch table 0x70E78C on property - 1
	const auto object = Pop().uintVal;
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	const auto entity = static_cast<entt::entity>(object);
	if (object == 0 || !Locator::entitiesRegistry::value().Valid(entity))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "GET_PROPERTY: Thing no longer valid");
		Pushf(0.0f);
		return;
	}
	switch (prop)
	{
	case script::ObjectPropertyType::Flying:
		// 0x70DCF8: bit 6 (0x40) of +0x24 = the object has a PhysicsObject (Object::IsActuallyInTheAir 0x639410 reads it
		// the same way), asleep resting proxies included
		Pushb(openblack::ecs::physics::PhysicsObjects::Find(entity) != nullptr);
		return;
	case script::ObjectPropertyType::Drowning: // 0x70DD0A: IsDrowning (vt +0x17C)
		Pushb(openblack::ecs::IsDrowning(entity));
		return;
	case script::ObjectPropertyType::BuiltPercentage: // 0x70E1A9: a MultiMapFixed's GetPercentBuilt, else 1
		if (const auto percent = openblack::ecs::feature_build::GetBuiltPercentage(entity); percent.has_value())
		{
			Pushf(*percent);
			return;
		}
		NotImplemented(__func__);
		Pushf(1.0f);
		return;
	default:
		// TODO(Daniels118): implement the other properties
		NotImplemented(__func__);
		Pushi(0);
		return;
	}
}

void SetProperty() // 022 SET_PROPERTY
{
	// GScript::SetProperty 0x70F380: the value, the object, the property -> fn_0070E820 (switch table 0x70F2BC)
	[[maybe_unused]] const auto val = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
	const auto prop = static_cast<script::ObjectPropertyType>(Pop().intVal);
	switch (prop)
	{
	case script::ObjectPropertyType::Flying:
	case script::ObjectPropertyType::Drowning:
	case script::ObjectPropertyType::Moving:
		// 0x70F2CC..0x70F2D4 -> 0x70F294: "Cannot Set Property %d", nothing changes
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_PROPERTY: Cannot Set Property {}", static_cast<int>(prop));
		return;
	case script::ObjectPropertyType::BuiltPercentage:
		// 0x70EC69: a MultiMapFixed -> fn_0052EDD0 (the Features here); anything else -> 0x70F294
		if (object == 0 || !openblack::ecs::feature_build::SetBuiltPercentage(static_cast<entt::entity>(object), val))
		{
			NotImplemented(__func__);
		}
		return;
	default:
		// TODO(Daniels118): implement the other properties
		NotImplemented(__func__);
		return;
	}
}

void GetPosition() // 023 GET_POSITION
{
	const auto objId = Pop().uintVal;

	// GScript::GetPosition 0x6F88A0: GetAltitude(pos) + relY, the object's world position; (0, 0, 0) for a lost object
	glm::vec3 position(0.0f);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(objId);
	if (objId != 0 && registry.Valid(entity))
	{
		const auto* transform = registry.TryGet<const Transform>(entity);
		if (const auto* flock = registry.TryGet<const ecs::components::Flock>(entity); flock != nullptr)
		{
			// 0x6F8927: IsFlock (vt +0x3EC): its first member's Pos (0x6F8931..0x6F893F), else Flock::GetFlockPos 0x530570,
			// which with no first member is the flock's own +0x14 (the domain centre)
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
			// 0x6F8977..0x6F89AF: IsMobileWallHug (vt +0x408) and not a creature (vt +0x34): AreWeThere(0) 0x60AD40 -> its
			// destination (+0x80) instead of Pos. Its height is GetAltitude + the destination's +0x88 (aproximado: the land
			// under it, openblack's WallHug keeps only x / z; GET_DISTANCE ignores y). TODO: the animals' (AnimalBrain goal)
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

void SetPosition() // 024 SET_POSITION
{
	auto position = PopVec();
	const auto objId = Pop().uintVal;

	if (objId != 0)
	{
		const auto& island = Locator::terrainSystem::value();
		position.y = island.GetHeightAt(glm::vec2(position.x, position.z));
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(objId));
		if (transform != nullptr)
		{
			transform->position = position;
		}
	}
}

void GetDistance() // 025 GET_DISTANCE
{
	// GScript::GetDistance 0x6F8CA0: the two vectors (0x6F8CB1..0x6F8D07) to GUtils::GetDistance(LHPoint, LHPoint)
	// 0x74CDE0 = hypotenuse(dx, dz): x and z only, y is ignored (0x74CDE8..0x74CDF3); under 0.5 (0x8AA3B4, 0x6F8D4B) it
	// is 0
	const auto p1 = PopVec();
	const auto p0 = PopVec();
	const float distance = gutils::GetDistance(p0, p1);
	Pushf(distance < 0.5f ? 0.0f : distance);
}

void Call() // 026 CALL
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto position = PopVec();
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void Create() // 027 CREATE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);

	// GScript::CreateThing 0x6F1B20: types 1..41 only, angle 0, scale 1; 0 when nothing is made
	const auto object = type > ObjectType::None && type <= ObjectType::AnimatedStatic
	                        ? CreateScriptObject(type, subtype, position, 0.0f, 1.0f)
	                        : entt::null;
	// 0x6F1BCD: AddScriptGameThing(thing, 1), a thing the script created
	ecs::script_held::AddScriptThing(object, true);

	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void Random() // 028 RANDOM
{
	const auto max = Popf();
	const auto min = Popf();
	const float random = min + (max - min) * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	Pushf(random);
}

void DllGettime() // 029 DLL_GETTIME
{
	// TODO(Daniels118): need a way to access Game::GetTurn()
	// Pushf(static_cast<float>(_turnCount) / 10.0f); // TODO(Daniels118): should it be divided by 10 or not?
}

void StartCameraControl() // 030 START_CAMERA_CONTROL
{
	// GScript::StartCameraControl 0x6ECCA0 (Help/ScriptControl.cpp). Inside the citadel: g_game+0x205A28 == 1 (inferred:
	// openblack's temple interior being active stands for it), no camera mode. Outside, fn_00461140 (0x6ECCBA) creates
	// the script camera mode unless GCamera::CantExitCurrentMode 0x441B70 (Camera/ScriptCamera.h)
	const bool insideCitadel = Locator::temple::has_value() && Locator::temple::value().Active();
	auto& cameraControl = help::script_control::GetCameraControl();
	bool cameraTaken = false;
	if (!insideCitadel)
	{
		const auto& camera = Locator::camera::value();
		cameraTaken = script_camera::BeginFrom(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	}
	const bool granted =
	    help::script_control::StartCameraControl(cameraControl, ScriptVm(), insideCitadel, cameraTaken);
	// Not original (mod game.skip-intro, "free start"): the first task that takes the camera after a new game is the
	// land's opening (CreatureDevSeeHome, or CreaturesInGlade with the other answers). It is still granted, so the
	// script's `loop { START_CAMERA_CONTROL }` goes through and releases as usual, but from here until it gives the
	// camera back FreeStart() drops what it does to the player
	if (granted && cameraControl.freeStartArmed && Locator::config::value().skipIntroFreeStart)
	{
		cameraControl.freeStartArmed = false;
		cameraControl.freeStartTask = cameraControl.owner;
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "Mod game.skip-intro: free start, the opening is task {}",
		                   cameraControl.owner);
	}
	Pushb(granted);
}

void EndCameraControl() // 031 END_CAMERA_CONTROL
{
	// GScript::EndCameraControl 0x6ECEF0 (Help/ScriptControl.cpp): fn_006ECD70 when this task has the camera; its camera
	// part (the script mode deleted, the player's mode from where the camera is, the FOV back to 70 degrees in 0.5 s)
	// is script_camera::End
	if (help::script_control::EndCameraControl(help::script_control::GetCameraControl(), audio::GetScriptAudioState(),
	                                           ScriptVm()))
	{
		script_camera::End();
	}
}

void SetWidescreen() // 032 SET_WIDESCREEN
{
	// 0x6F7BF0 -> HelpSystem::SetWideScreen 0x5C6AD0; the bars slide in HelpSystemInfo.wideScreenTime seconds (2.0)
	// GScript::SetWideScreen: only the task that holds it (HelpSystem+0x45EC) or any when none does
	// (Help/ScriptControl.cpp); HelpSystem's hook moves the bars (Game.cpp)
	const auto on = static_cast<int32_t>(Pop().intVal);
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": no bars over the opening
	}
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		help::script_control::SetWideScreen(*helpSystem, on, ScriptVm());
	}
	else
	{
		// openblack only (no original equivalent: GScript always has g_game+0x25005C; here a VM without a HelpSystem,
		// e.g. tools): the bars move without any owner. 2.0 is the default of ScreenFade::_wideTime (no source)
		const float time =
		    Locator::infoConstants::has_value() ? Locator::infoConstants::value().helpSystem.wideScreenTime : 2.0f;
		Game::Instance()->GetScreenFade().SetWideScreen(on != 0, time);
		audio::SetScriptWideScreen(on != 0);
	}
}

void MoveGameThing() // 033 MOVE_GAME_THING
{
	// GScript::MoveGameThing 0x6F8E80. The pops (0x6F8E91..0x6F8EE9): the radius (only the creature's), z, y, x, the
	// object
	const auto radius = Popf();
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = static_cast<entt::entity>(object);
	// 0x6F8EEF: GetScriptGameThing 0x70D220; none -> "Thing no longer valid" (0xC0C258)
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
	// MapCoords(pos) 0x603160 (MapCoords::Set 0x603340): x / z of the point, y kept above the land (the walks use x / z)
	const glm::vec2 goal(position.x, position.z);
	// 0x6F8F15: IsCreature (vt +0x34)
	if (registry.AllOf<ecs::components::Creature>(entity))
	{
		// dynamic_cast<Creature*> (0x6F8F2B; none -> "no creature for script" 0xC0D598), IsObjectInMap (vt +0x178) ->
		// fn_004F6B60(pos, radius) (0x6F8F60): Creature::PrepareCreatureForScriptedAction 0x4F6A90 and the sub-actions of
		// its CreatureSubActionAgenda (AddSubAction 0x4FF240). TODO(M8, creature): openblack has no creature AI
		NotImplemented("MoveGameThing (creature)");
		return;
	}
	// 0x6F8F70: IsLiving (vt +0x3C4): IsObjectInMap (vt +0x178, 0x6F8F7E) and not IsDrowning (vt +0x17C, 0x6F8F90),
	// else nothing (0x6F907E)
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		if (!ecs::villager::IsObjectInMap(entity) || ecs::IsDrowning(entity))
		{
			return;
		}
		// 0x6F8FB7: AreWeThere(coords, 0.0) (vt +0x85C, 0x60AD60) == 0 -> Living::SetupMoveToPos(coords, 4 IN_SCRIPT)
		// 0x5F2830 (0x6F8FCA); else GScript::SetScriptState(this, 4 IN_SCRIPT) 0x6F82E0 (0x6F8FD7)
		if (!ecs::villager::AreWeThere(entity, goal, 0.0f))
		{
			ecs::villager::SetupMoveToPos(entity, goal, VillagerStates::InScript);
		}
		else
		{
			ecs::villager::SetScriptState(entity, VillagerStates::InScript);
		}
		return;
	}
	if (registry.AllOf<ecs::components::Animal>(entity))
	{
		// The same Living branch for an animal (GScript 0x6F8F6C), ported by the animals session: there already
		// (AreWeThere) -> SetScriptState(IN_SCRIPT 4), else SetupMoveToPos(pos, IN_SCRIPT 4) (animal_ai::ScriptMoveTo).
		// (aproximado) IsObjectInMap as "not in the hand" (IN_HAND)
		const auto* brain = registry.TryGet<const ecs::components::AnimalBrain>(entity);
		if (brain == nullptr || ecs::IsDrowning(entity) ||
		    static_cast<ecs::animal_ai::AnimalState>(brain->topState) == ecs::animal_ai::AnimalState::InHand)
		{
			return;
		}
		ecs::animal_ai::ScriptMoveTo(entity, goal);
		return;
	}
	// 0x6F8FEA: IsFlock (vt +0x3EC) -> Flock::SetDomainCentrePos(coords) 0x52FC20: its first member's destination (+0x80,
	// 0x52FC2C..0x52FC53) and the flock's +0x14
	if (auto* flock = registry.TryGet<ecs::components::Flock>(entity); flock != nullptr)
	{
		if (!flock->members.empty() && registry.Valid(flock->members.front()))
		{
			if (auto* brain = registry.TryGet<ecs::components::AnimalBrain>(flock->members.front()); brain != nullptr)
			{
				brain->goal = goal;
			}
		}
		flock->domainCentre = position;
		return;
	}
	// 0x6F9015: IsWeather (vt +0x3FC) -> fn_00774550(pos) (its system +0x78: +0x5C = pos); 0x6F9036: IsComputerPlayer
	// (vt +0x4B8) -> fn_00658510(pos, 60.0). openblack has neither kind of thing.
	// 0x6F9058: anything else: "Jonty - Thing must be living to move it!" (0xC0D56C), then SetPos(coords) (vt +0xFC,
	// GameThingWithPos::SetPos 0x401940: Pos = coords, so its world point is the vector itself). (aproximado) only the
	// Transform moves: openblack's derived data (static meshes, physics) is not told
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "MOVE_GAME_THING: Jonty - Thing must be living to move it!");
	if (auto* transform = registry.TryGet<Transform>(entity); transform != nullptr)
	{
		transform->position = position;
	}
}

void SetFocus() // 034 SET_FOCUS
{
	// const auto position = PopVec();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void HasCameraArrived() // 035 HAS_CAMERA_ARRIVED
{
	if (FreeStart())
	{
		// Mod game.skip-intro, "free start": the opening's camera is not moved at all, so it has always arrived. Without
		// this the script would wait here for ever
		Pushb(true);
		return;
	}
	// GScript::HasCameraArrived 0x6ED170 (1 in a network game, not ported) -> GCamera::Arrived 0x443050: the current
	// mode's vt+0x34. CameraModeScript::Arrived 0x461B40; the player's CameraModeNew3 keeps CameraMode::Arrived 0x441700
	// (its vtable 0x8C7BFC +0x34), the same squared distance 0.001 to the destinations: here the player's Camera
	// zoomers (Zoomer3d, GCamera +0x118 / +0x88; (inferido) the original has one GCamera for both modes)
	if (script_camera::HasMode()) // the script mode, or a dual camera (CameraMode::Arrived 0x441700)
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
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void FlockAttach() // 037 FLOCK_ATTACH
{
	// const auto asLeader = static_cast<bool>(Pop().intVal);
	// const auto flock = Pop().uintVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void FlockDetach() // 038 FLOCK_DETACH
{
	// const auto flock = Pop().uintVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void FlockDisband() // 039 FLOCK_DISBAND
{
	// const auto flock = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IdSize() // 040 ID_SIZE
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void FlockMember() // 041 FLOCK_MEMBER
{
	// const auto flock = Pop().uintVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetHandPosition() // 042 GET_HAND_POSITION
{
	const auto handEntity = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& handTransform = Locator::entitiesRegistry::value().Get<Transform>(handEntity);

	PushVec(handTransform.position);
}

void PlaySoundEffect() // 043 PLAY_SOUND_EFFECT
{
	// GScript::PlaySoundEffect 0x70F7F0: six POPs (withPos, z, y, x, bank, sample), then audio::script_sound
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	audio::script_sound::PlaySoundEffect(sample, bank, position, withPosition);
}

/// GScript::GetScriptGameThing 0x70D220 for the music functions: the object, or nullopt with the original's
/// "Thing no longer valid" (0xC0C258) (approximated: the original looks the id up in its script table 0xD967F8, range
/// 1..0x1FF; here 0 is null as in Pusho and a valid entity stands for a live thing)
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
	// GScript::StartMusic 0x70FB20
	const auto music = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "START_MUSIC({})", music);
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": the opening plays no music of its own
	}
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		gameMusic->ScriptStartMusic(music);
	}
}

void StopMusic() // 045 STOP_MUSIC
{
	// 0x70FB90: StartScriptMusic(0)
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "STOP_MUSIC()");
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": it started none, so it stops none
	}
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		gameMusic->ScriptStopMusic();
	}
}

void AttachMusic() // 046 ATTACH_MUSIC
{
	// 0x70FBF0: the thing is popped first, then the type
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
	// 0x70FC60: RemoveThingMusic
	const auto object = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->RemoveThingMusic(*object);
	}
}

void ObjectDelete() // 048 OBJECT_DELETE
{
	// const auto withFade = Pop().intVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void FocusFollow() // 049 FOCUS_FOLLOW
{
	// GScript::FocusFollow 0x6EDF30: POP the thing (GetScriptGameThing 0x70D220; none -> "Thing no longer valid") ->
	// CameraModeScript::SetCameraFocus(thing) 0x4619B0: the path dropped, and the focus heads for the thing every frame
	// (CameraModeFollow::Update 0x44C160)
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusFollow(*thing);
	}
}

void PositionFollow() // 050 POSITION_FOLLOW
{
	// GScript::PositionFollow 0x6EDE70: POP the thing -> CameraModeFollow::Set(thing) 0x44BA00: the position follows it
	// every frame, from the heading and pitch the camera has now, at GetThingViewingDistance (its height x 8); with
	// "behind" (on in the script mode) the heading is 0, relative to a MobileWallHug's angle. The path is kept
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::PositionFollow(*thing);
	}
}

void CallNear() // 051 CALL_NEAR
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto position = PopVec();
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SpecialEffectPosition() // 052 SPECIAL_EFFECT_POSITION
{
	// GScript::SpecialEffectPosition 0x70C330 -> GParticleContainer::CreateSpotVisualWithSpecifiedDuration 0x63E580
	const auto duration = Popf();
	const auto position = PopVec();
	const auto effect = Pop().intVal;
	const auto object = psys::manager::CreateSpotVisual(effect, position, duration, entt::null);
	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void SpecialEffectObject() // 053 SPECIAL_EFFECT_OBJECT
{
	// GScript::SpecialEffectObject 0x70C460: at the object, following it; its loss closes the effect
	const auto duration = Popf();
	const auto target = static_cast<entt::entity>(Pop().uintVal);
	const auto effect = Pop().intVal;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(target) ? registry.TryGet<const Transform>(target) : nullptr;
	entt::entity object = entt::null;
	if (transform != nullptr)
	{
		object = psys::manager::CreateSpotVisual(effect, transform->position, duration, target);
	}
	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void DanceCreate() // 054 DANCE_CREATE
{
	// const auto duration = Popf();
	// const auto position = PopVec();
	// const auto type = Pop().intVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CallIn() // 055 CALL_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void ChangeInnerOuterProperties() // 056 CHANGE_INNER_OUTER_PROPERTIES
{
	// const auto calm = Popf();
	// const auto outer = Popf();
	// const auto inner = Popf();
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void Snapshot() // 057 SNAPSHOT
{
	// const auto challengeId = Pop().intVal;
	// const auto argc = Pop().intVal;
	// const auto argv = PopVarArg(argc);
	// const auto reminderScript = PopString();
	// const auto titleStrID = Pop().intVal;
	// const auto alignment = Popf();
	// const auto success = Popf();
	// const auto focus = PopVec();
	// const auto position = PopVec();
	// const auto quest = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetAlignment() // 058 GET_ALIGNMENT
{
	// GScript::GetAlignment 0x6F9A60: the player's GAlignment value (GPlayer::GetAlignmentValue 0x64D6A0)
	const auto player = Pop().intVal;
	Pushf(ecs::effects::alignment::Get(static_cast<PlayerNames>(std::clamp(player, 0, static_cast<int>(PlayerNames::NEUTRAL)))));
}

void SetAlignment() // 059 SET_ALIGNMENT
{
	// GScript::SetAlignment 0x6F99C0: the player first, then the value; out of -1..1 it is an error and nothing
	// happens, otherwise CrudeUpdate: the value is ADDED to the player's alignment (clamped), despite the name
	const auto player = Pop().intVal;
	const auto value = Pop().floatVal;
	if (value < -1.0f || value > 1.0f)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_ALIGNMENT: Alignment out of range ({})", value);
		return;
	}
	ecs::effects::alignment::CrudeUpdate(static_cast<PlayerNames>(std::clamp(player, 0, static_cast<int>(PlayerNames::NEUTRAL))), value);
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
	// const auto level = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void Played() // 064 PLAYED
{
	// GScript::Played 0x6F9DC0
	const auto object = Pop().uintVal;
	const auto entity = static_cast<entt::entity>(object);
	auto& registry = Locator::entitiesRegistry::value();
	if (object == 0 || !registry.Valid(entity))
	{
		// 0x6F9DE1: "Thing no longer valid" -> 1
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "PLAYED: Thing no longer valid");
		Pushb(true);
		return;
	}
	if (registry.AllOf<openblack::ecs::components::PuzzleGame>(entity))
	{
		// 0x6F9F0B: IsPuzzleGame -> fn_006D66E0
		Pushb(openblack::ecs::IsPuzzleGamePlayed(entity));
		return;
	}
	if (registry.AllOf<ecs::components::Villager>(entity))
	{
		// 0x6F9E83: IsLiving (vt +0x3C4); 0x6F9E9C: dynamic_cast<Villager*> -> IsScriptAnimationComplete 0x7689D0 (0x6F9EAA)
		const bool complete = ecs::villager::IsScriptAnimationComplete(entity);
		if (complete && ScriptThingTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "PLAYED {}: true", object);
		}
		Pushb(complete);
		return;
	}
	if (registry.AnyOf<ecs::components::Animal, ecs::components::Creature>(entity))
	{
		// TODO(Daniels118): IsCreature -> the creature's plan (0x6F9DF4..0x6F9E7E); any other Living: GetFinalState
		// (vt +0xB04) == 4 IN_SCRIPT (0x6F9EC4..0x6F9ED8)
		NotImplemented(__func__);
		Pushb(false);
		return;
	}
	// IsWeather (vt +0x3FC) -> +0x78 == 0 (0x6F9EF0): openblack has no weather things. Anything else (0x6F9F31):
	// "Thing not living" and 1
	SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "PLAYED: Thing not living");
	Pushb(true);
}

void RandomUlong() // 065 RANDOM_ULONG
{
	// const auto max = Pop().intVal;
	// const auto min = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void SetGamespeed() // 066 SET_GAMESPEED
{
	// const auto speed = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
	Pusho(0);
}

void OverrideStateAnimation() // 068 OVERRIDE_STATE_ANIMATION
{
	// const auto animType = Pop().intVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureCreateRelativeToCreature() // 069 CREATURE_CREATE_RELATIVE_TO_CREATURE
{
	// const auto type = Pop().intVal;
	// const auto position = PopVec();
	// const auto scale = Popf();
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CreatureLearnEverything() // 070 CREATURE_LEARN_EVERYTHING
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetKnowsAction() // 071 CREATURE_SET_KNOWS_ACTION
{
	// const auto knows = Pop().intVal;
	// const auto action = Pop().intVal;
	// const auto typeOfAction = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetAgendaPriority() // 072 CREATURE_SET_AGENDA_PRIORITY
{
	// const auto priority = Popf();
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureTurnOffAllDesires() // 073 CREATURE_TURN_OFF_ALL_DESIRES
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureLearnDistinctionAboutActivityObject() // 074 CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureDoAction() // 075 CREATURE_DO_ACTION
{
	// const auto withObject = Pop().uintVal;
	// const auto target = Pop().uintVal;
	// const auto unk1 = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void InCreatureHand() // 076 IN_CREATURE_HAND
{
	// const auto creature = Pop().uintVal;
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void CreatureSetDesireValue() // 077 CREATURE_SET_DESIRE_VALUE
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetDesireActivated78() // 078 CREATURE_SET_DESIRE_ACTIVATED
{
	// const auto active = Pop().intVal;
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetDesireActivated79() // 079 CREATURE_SET_DESIRE_ACTIVATED
{
	// const auto active = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureSetDesireMaximum() // 080 CREATURE_SET_DESIRE_MAXIMUM
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ConvertCameraPosition() // 081 CONVERT_CAMERA_POSITION
{
	// GScript::ConvertCameraPosition 0x6ED200: LoadCameraFromHD 0x446FE0 ("Cam%d" of camera.edt), its position
	const auto cameraEnum = Pop().intVal;
	const auto camera = LoadCameraBin(cameraEnum);
	const auto position = camera.has_value() ? camera->position : glm::vec3(0.0f);
	Pushv(position.x);
	Pushv(position.y);
	Pushv(position.z);
}

void ConvertCameraFocus() // 082 CONVERT_CAMERA_FOCUS
{
	// GScript::ConvertCameraFocus 0x6ED270: LoadCameraFromHD 0x446FE0 ("Cam%d" of camera.edt), its focus
	const auto cameraEnum = Pop().intVal;
	const auto camera = LoadCameraBin(cameraEnum);
	const auto focus = camera.has_value() ? camera->focus : glm::vec3(0.0f);
	Pushv(focus.x);
	Pushv(focus.y);
	Pushv(focus.z);
}

void CreatureSetPlayer() // 083 CREATURE_SET_PLAYER
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StartCountdownTimer() // 084 START_COUNTDOWN_TIMER
{
	// const auto timeout = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureInitialiseNumTimesPerformedAction() // 085 CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureGetNumTimesActionPerformed() // 086 CREATURE_GET_NUM_TIMES_ACTION_PERFORMED
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void RemoveCountdownTimer() // 087 REMOVE_COUNTDOWN_TIMER
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetObjectDropped() // 088 GET_OBJECT_DROPPED
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void ClearDroppedByObject() // 089 CLEAR_DROPPED_BY_OBJECT
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreateReaction() // 090 CREATE_REACTION
{
	// const auto reaction = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void RemoveReaction() // 091 REMOVE_REACTION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetCountdownTimer() // 092 GET_COUNTDOWN_TIMER
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void StartDualCamera() // 093 START_DUAL_CAMERA
{
	// GScript::StartDualCamera 0x6ED2E0: POP b then a (GetScriptGameThing 0x70D220 each); either missing -> "Thing invalid
	// for dual cam" (0xC0C1EC) and nothing; else new(0x30) CameraModeTwoObjects(camera, a, b) 0x461BB0 on top of
	// whatever mode is current (no mode nor citadel check): the camera looks at the two things' middle from their
	// distance apart (Camera/ScriptCamera.h)
	const auto objectB = Pop().uintVal;
	const auto objectA = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
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
	// GScript::UpdateDualCamera 0x6ED370: POP b then a; either missing -> "Thing invalid for dual cam" and nothing; else
	// when the current mode is a CameraModeTwoObjects (0x6ED3F0) SetObjects(a, b) 0x461C90 (a point camera becomes a two
	// things one)
	const auto objectB = Pop().uintVal;
	const auto objectA = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
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
	// GScript::ReleaseDualCamera 0x6ED410: a current CameraModeTwoObjects is deleted (vt+0x30) and popped (PopViewMode
	// 0x441C50): the mode under it (the script mode) moves the camera again, its seconds from 0
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	script_camera::ReleaseDual();
}

void SetCreatureHelp() // 096 SET_CREATURE_HELP
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetTargetObject() // 097 GET_TARGET_OBJECT
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CreatureDesireIs() // 098 CREATURE_DESIRE_IS
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void CountdownTimerExists() // 099 COUNTDOWN_TIMER_EXISTS
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void LookGameThing() // 100 LOOK_GAME_THING
{
	// const auto target = Pop().uintVal;
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetObjectDestination() // 101 GET_OBJECT_DESTINATION
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureForceFinish() // 102 CREATURE_FORCE_FINISH
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void HideCountdownTimer() // 103 HIDE_COUNTDOWN_TIMER
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetActionTextForObject() // 104 GET_ACTION_TEXT_FOR_OBJECT
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void CreateDualCameraWithPoint() // 105 CREATE_DUAL_CAMERA_WITH_POINT
{
	// GScript::CreateDualCameraWithPoint 0x6ED460: POP the point (z, y, x) then the thing; new(0x30) fn_00461CB0(camera,
	// thing, &point) with no check: a thing that is not there leaves a mode that moves nothing (its Update would read
	// through null in the original) until the turn's CheckStackedModesForValidity drops it (IsStillValid 0x461D90)
	const auto point = PopVec();
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	const auto thing = CameraThing(object, __func__);
	const auto& camera = Locator::camera::value();
	script_camera::StartDualWithPoint(thing.value_or(entt::null), point, camera.GetOriginZoomer(), camera.GetFocusZoomer());
}

void SetCameraToFaceObject() // 106 SET_CAMERA_TO_FACE_OBJECT
{
	// GScript::SetCameraToFaceObject 0x6ED500: POP the distance, then the thing (GetScriptGameThing 0x70D220); with the
	// script mode, fn_006ED710(thing, distance, &position, &focus): the focus on the thing (half its height up), the
	// position `distance` away along its facing (GetFacingDirection vt +0x4EC) and 0.1 rad up; then
	// CameraModeScript::SetCameraPosition 0x461370 and SetCameraFocus 0x4612B0 (both drop the path and the follows)
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	if (!ScriptCameraMode(__func__))
	{
		return;
	}
	// (inferido) no thing: fn_006ED710 says "no object to face" (0xC0C218) and then reads through the null pointer;
	// here nothing more is done
	const auto thing = object != 0 ? static_cast<entt::entity>(object) : entt::null;
	if (const auto points = script_camera::FaceObject(thing, distance); points.has_value())
	{
		script_camera::SetPosition(points->position);
		script_camera::SetFocus(points->focus);
	}
}

void MoveCameraToFaceObject() // 107 MOVE_CAMERA_TO_FACE_OBJECT
{
	// GScript::MoveCameraToFaceObject 0x6ED600: POP the time, the distance, then the thing; with the script mode,
	// fn_006ED710 (as 106) and CameraModeScript::MoveCameraPosition 0x4616F0 / MoveCameraFocus 0x461430 in that time
	// (seconds of the wall clock)
	const auto time = Popf();
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	if (!ScriptCameraMode(__func__))
	{
		return;
	}
	const auto thing = object != 0 ? static_cast<entt::entity>(object) : entt::null; // (inferido) as in 106
	if (const auto points = script_camera::FaceObject(thing, distance); points.has_value())
	{
		script_camera::MovePosition(points->position, time);
		script_camera::MoveFocus(points->focus, time);
	}
}

void GetMoonPercentage() // 108 GET_MOON_PERCENTAGE
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void PopulateContainer() // 109 POPULATE_CONTAINER
{
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// const auto quantity = Popf();
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void AddReference() // 110 ADD_REFERENCE
{
	// GScript::AddReference 0x6FA450: IncrementScriptReference (the original pushes nothing; the binding's one output
	// is kept as the object)
	const auto objId = Pop().uintVal;
	if (objId != 0)
	{
		ecs::script_held::IncrementReference(static_cast<entt::entity>(objId));
	}
	Pusho(objId);
}

void RemoveReference() // 111 REMOVE_REFERENCE
{
	// GScript::RemoveReference 0x6FA470: DecrementScriptReference
	const auto objId = Pop().uintVal;
	if (objId != 0)
	{
		ecs::script_held::DecrementReference(static_cast<entt::entity>(objId));
	}
	Pusho(objId);
}

void SetGameTime() // 112 SET_GAME_TIME
{
	// GScript::SetGameTime 0x710E20: ForceVisualTime(script -> visual time)
	const auto time = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_GAME_TIME({})", time);
	Game::Instance()->SetTime(time);
}

void GetGameTime() // 113 GET_GAME_TIME
{
	// GScript::GetGameTime 0x710E60: the visual time mapped back to script time
	Pushf(Game::Instance()->GetDayNightClock().GetScriptTime());
}

void GetRealTime() // 114 GET_REAL_TIME
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetRealDay115() // 115 GET_REAL_DAY
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetRealDay116() // 116 GET_REAL_DAY
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetRealMonth() // 117 GET_REAL_MONTH
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetRealYear() // 118 GET_REAL_YEAR
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void RunCameraPath() // 119 RUN_CAMERA_PATH
{
	// GScript::RunCameraPath 0x6ED7F0 -> CameraModeScript::RunPath fn_00461A80 (camera.edt "Track%d", 3D/CameraTracks.h)
	const auto path = static_cast<int32_t>(Pop().intVal);
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	if (ScriptCameraMode(__func__))
	{
		script_camera::RunPath(path);
	}
}

void StartDialogue() // 120 START_DIALOGUE
{
	// GScript::StartDialogue 0x710690 (Help/ScriptControl.cpp); the advisors going home are HelpSystem hooks (not ported).
	// Without a HelpSystem (openblack only, the original always has g_game+0x25005C): false
	auto* helpSystem = help::Get();
	Pushb(helpSystem != nullptr && help::script_control::StartDialogue(*helpSystem, ScriptVm()));
}

void EndDialogue() // 121 END_DIALOGUE
{
	// GScript::EndDialogue 0x710780 (Help/ScriptControl.cpp): only for the task that has the dialogue (nothing without a
	// HelpSystem: openblack only)
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		help::script_control::EndDialogue(*helpSystem, audio::GetScriptAudioState(), ScriptVm());
	}
}

void IsDialogueReady() // 122 IS_DIALOGUE_READY
{
	// GScript::IsSpiritReady 0x710830: !HelpSystem::IsDialogueControlled 0x5C6740, a bool (type 6). Without a
	// HelpSystem (openblack only, no original equivalent): true, nothing controls the dialogue
	const auto* helpSystem = help::Get();
	Pushb(helpSystem == nullptr || help::script_control::IsSpiritReady(*helpSystem));
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
	NotImplemented(__func__);
}

void StartGameSpeed() // 128 START_GAME_SPEED
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void EndGameSpeed() // 129 END_GAME_SPEED
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void BuildBuilding() // 130 BUILD_BUILDING
{
	// const auto desire = Popf();
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetAffectedByWind() // 131 SET_AFFECTED_BY_WIND
{
	// const auto object = Pop().uintVal;
	// const auto enabled = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void WidescreenTransistionFinished() // 132 WIDESCREEN_TRANSISTION_FINISHED
{
	Pushb(Game::Instance()->GetScreenFade().IsWideScreenTransitionFinished());
}

void GetResource() // 133 GET_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void AddResource() // 134 ADD_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto quantity = Popf();
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void RemoveResource() // 135 REMOVE_RESOURCE
{
	// const auto container = Pop().uintVal;
	// const auto quantity = Popf();
	// const auto resource = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetTargetRelativePos() // 136 GET_TARGET_RELATIVE_POS
{
	// const auto angle = Popf();
	// const auto distance = Popf();
	// const auto to = PopVec();
	// const auto from = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void StopPointing() // 137 STOP_POINTING
{
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StopLooking() // 138 STOP_LOOKING
{
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void LookAtPosition() // 139 LOOK_AT_POSITION
{
	// const auto position = PopVec();
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void PlaySpiritAnim() // 140 PLAY_SPIRIT_ANIM
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
	Pusho(0);
}

void SetCameraZone() // 142 SET_CAMERA_ZONE
{
	// GScript::SetCameraZone 0x6ED890: POP the string; CameraExclusion::ResetExclusionFile(1), then ".\Data\Zones\%s"
	// (CameraExclusion::LoadExclusionFile 0x455370) and the force field on (Camera/PlayerCameraScript.h). It limits the
	// player's camera (CameraModeNew3), which openblack does not read yet; the script camera is not affected
	const auto zone = PopString();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": the opening does not shut the player's camera in (user, 2026-10-02)
	}
	player_camera::SetCameraZone(zone);
}

void GetObjectState() // 143 GET_OBJECT_STATE
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void RevealCountdownTimer() // 144 REVEAL_COUNTDOWN_TIMER
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetTimerTime() // 145 SET_TIMER_TIME
{
	const auto time = Popf();
	const auto timer = Pop().uintVal;
	// 0x711280 also takes a spell dispenser (its period); the timers themselves are not ported
	if (timer == 0 || !magic::script::SetDispenserTimerTime(static_cast<entt::entity>(timer), time))
	{
		NotImplemented(__func__);
	}
}

void CreateTimer() // 146 CREATE_TIMER
{
	// const auto timeout = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetTimerTimeRemaining() // 147 GET_TIMER_TIME_REMAINING
{
	// const auto timer = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetTimerTimeSinceSet() // 148 GET_TIMER_TIME_SINCE_SET
{
	// const auto timer = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void MoveMusic() // 149 MOVE_MUSIC
{
	// 0x70FCA0: "to" is popped first, then "from"; both must be valid (0x70FCEE..0x70FCF4)
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
	// GScript::GetInclusionDistance 0x6ED990: PUSH [0xC5E13C] as a float. (aproximado) CameraModeNew3::Update, which
	// writes it (0x45FBFD / 0x45FCD9 / 0x45FCE1), is not ported: it stays at its start value FLT_MAX (0x4548D0)
	Pushf(player_camera::Get().inclusionDistance);
}

void GetLandHeight() // 151 GET_LAND_HEIGHT
{
	const auto position = PopVec();

	// GScript::GetLandHeight 0x6FB1F0: -10 over the sea (altitude 0), off the map or without a block
	const auto& island = Locator::terrainSystem::value();
	Pushf(openblack::ecs::sea_cells::ScriptLandHeight(island, position));
}

void LoadMap() // 152 LOAD_MAP
{
	// const auto path = PopString();

	// auto& fileSystem = Locator::filesystem::value();
	// auto mapPath = fileSystem.GetGamePath() / path;
	// TODO(Daniels118): LoadMap(mapPath);
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

	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&filenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return !filenames.contains(filename);
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
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ClearClickedPosition() // 157 CLEAR_CLICKED_POSITION
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void PositionClicked() // 158 POSITION_CLICKED
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void ReleaseFromScript() // 159 RELEASE_FROM_SCRIPT
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetObjectHandIsOver() // 160 GET_OBJECT_HAND_IS_OVER
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void IdPoisonedSize() // 161 ID_POISONED_SIZE
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void IsPoisoned() // 162 IS_POISONED
{
	// const auto obj = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void CallPoisonedIn() // 163 CALL_POISONED_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CallNotPoisonedIn() // 164 CALL_NOT_POISONED_IN
{
	// const auto excludingScripted = static_cast<bool>(Pop().intVal);
	// const auto container = Pop().uintVal;
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SpiritPlayed() // 165 SPIRIT_PLAYED
{
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void ClingSpirit() // 166 CLING_SPIRIT
{
	// const auto yPercent = Popf();
	// const auto xPercent = Popf();
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void FlySpirit() // 167 FLY_SPIRIT
{
	// const auto yPercent = Popf();
	// const auto xPercent = Popf();
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetIdMoveable() // 168 SET_ID_MOVEABLE
{
	// const auto obj = Pop().uintVal;
	// const auto moveable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetIdPickupable() // 169 SET_ID_PICKUPABLE
{
	// const auto obj = Pop().uintVal;
	// const auto pickupable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void WalkPath() // 177 WALK_PATH
{
	// GScript::WalkPath 0x6FBB50
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
	if (registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(object))
	{
		// IsLiving: 0x5EE100 gives the Living its own DataPath (+0xAC), with a step from the speed (+0x5A) and the
		// focus way's length (+0x0C), and a footpath (docs/bw1-notes/camera-tracks.md). Not ported.
		NotImplemented(__func__);
		return;
	}
	if (!registry.AnyOf<ecs::components::MobileObject, ecs::components::Shark>(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "WALK_PATH: Thing is invalid for move path"); // 0x6FBC2E
		return;
	}
	ecs::StartMobileWalkPath(object, cameraEnum, forward, valFrom, valTo); // fn_006076C0
}

void FocusAndPositionFollow() // 178 FOCUS_AND_POSITION_FOLLOW
{
	// GScript::FocusAndPositionFollow 0x6EDDA0: POP the distance, then the thing -> fn_0044BA90(thing, distance): as
	// POSITION_FOLLOW with that distance and the heading kept (no "behind" reset); the focus follows the same thing
	// (GetFocusThing 0x4611F0) unless FOCUS_FOLLOW gave another
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusAndPositionFollow(*thing, distance);
	}
}

void GetWalkPathPercentage() // 179 GET_WALK_PATH_PERCENTAGE
{
	// GScript::GetWalkPathPercentage 0x6FBC50: 1.0 for anything but a Living (a shark's DataPath is not read)
	const auto object = static_cast<entt::entity>(Pop().uintVal);
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(object) &&
	    registry.AnyOf<ecs::components::Villager, ecs::components::Animal, ecs::components::Creature>(object))
	{
		// Living::GetWalkPathPercentage 0x5EE520: +0xAC current / duration; the Living walk path is not ported
		NotImplemented(__func__);
		Pushf(0.0f);
		return;
	}
	Pushf(1.0f);
}

void CameraProperties() // 180 CAMERA_PROPERTIES
{
	// GScript::CameraProperties 0x6EDFF0: POP behind (raw, tested != 0 at 0x44C7E6), the angle (degrees, x 0.0174533
	// [0x92B20C]), the speed, the distance; with the script mode +0x14 = distance, +0x18 = speed (the follow's time
	// factor: 0 places at once), +0x1C = behind, +0x0C = angle, +0x20 = 0 (0x6EE0C1..0x6EE0D9)
	const bool behind = Pop().uintVal != 0;
	const auto angle = Popf() * script_camera::k_DegreesToRadians;
	const auto speed = Popf();
	const auto distance = Popf();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	if (ScriptCameraMode(__func__))
	{
		script_camera::SetFollowProperties(distance, speed, angle, behind);
	}
}

void EnableDisableMusic() // 181 ENABLE_DISABLE_MUSIC
{
	// 0x70FD10: the thing is popped first, then the switch (stored as it is, fn_004298A0)
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
	// 0x70FD70: 0 for an invalid thing (0x70FDA1), else fn_004293B0
	const auto source = MusicThing(Pop().uintVal);
	float distance = 0.0f;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && source)
	{
		distance = gameMusic->GetMusicObjDistance(*source);
	}
	Pushf(distance);
}

void GetMusicEnumDistance() // 183 GET_MUSIC_ENUM_DISTANCE
{
	// 0x70FDE0: with an invalid type the original pushes twice (0, then GetPlayDistance)
	const auto type = Pop().intVal;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
	{
		for (const float value : gameMusic->ScriptGetMusicEnumDistance(type))
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
	// 0x70FE60: z, y and x are popped first (the LHPoint at 0x70FEDB), then the thing
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
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void AttachObjectLeashToHand() // 186 ATTACH_OBJECT_LEASH_TO_HAND
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void DetachObjectLeash() // 187 DETACH_OBJECT_LEASH
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureOnlyDesire() // 188 SET_CREATURE_ONLY_DESIRE
{
	// const auto value = Popf();
	// const auto desire = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureOnlyDesireOff() // 189 SET_CREATURE_ONLY_DESIRE_OFF
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void RestartMusic() // 190 RESTART_MUSIC
{
	// 0x70FF00: RestartMusicThing
	const auto object = MusicThing(Pop().uintVal);
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		gameMusic->RestartMusicThing(*object);
	}
}

void MusicPlayed191() // 191 MUSIC_PLAYED
{
	// 0x70FF40: true for an invalid thing (0x70FF71), else IsMusicThingFinished (a bool, VMType 6)
	const auto object = MusicThing(Pop().uintVal);
	bool finished = true;
	const auto lock = audio::game_music::Lock();
	if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr && object)
	{
		finished = gameMusic->IsMusicThingFinished(*object) != 0;
	}
	Pushb(finished);
}

void IsOfType() // 192 IS_OF_TYPE
{
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void ClearHitObject() // 193 CLEAR_HIT_OBJECT
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GameThingHit() // 194 GAME_THING_HIT
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
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
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetSlowestSpeed() // 198 GET_SLOWEST_SPEED
{
	// const auto flock = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetObjectHeld199() // 199 GET_OBJECT_HELD
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void HelpSystemOn() // 200 HELP_SYSTEM_ON
{
	// GScript::HelpSystemOn 0x6FBFD0: HelpSystem+0x45F8 && +0x45F4 != 0, a bool (type 6). Without a HelpSystem
	// (openblack only: the original always has g_game+0x25005C): false
	const auto* helpSystem = help::Get();
	Pushb(helpSystem != nullptr && helpSystem->IsHelpSystemOn());
}

void ShakeCamera() // 201 SHAKE_CAMERA
{
	// GScript::ShakeCamera 0x6EE0F0: POP the seconds, the amplitude, the radius and the point (z, y, x) ->
	// PSysGlobal::StartCameraShake(point, radius, amplitude, seconds) 0x68F400 -> LH3DCameraChecker::Create 0x821050:
	// the drawn camera shakes while it is within the radius of the point, less and less (Camera/CameraShake.h)
	const auto seconds = Popf();
	const auto amplitude = Popf();
	const auto radius = Popf();
	const auto position = PopVec();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	camera_shake::StartCameraShake(position, radius, amplitude, seconds);
}

void SetAnimationModify() // 202 SET_ANIMATION_MODIFY
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetAviSequence() // 203 SET_AVI_SEQUENCE
{
	// GScript::SetAviSequence 0x6FC050: POP the sequence (raw, compared with 1 and 2), then on (raw, != 0) ->
	// PSysGlobal::StartAVISequence 0x68F450 / StopAVISequence 0x68F4F0. It returns at once
	const auto sequence = Pop().intVal;
	const bool on = Pop().uintVal != 0;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": the opening plays no film
	}
	if (!on)
	{
		// StopAVISequence 0x68F4F0: 2 -> GGame::EndFallingSpellVideo 0x553A10; anything else nothing
		if (sequence == 2)
		{
			NotImplemented("SetAviSequence (EndFallingSpellVideo)");
		}
		return;
	}
	if (sequence == 1)
	{
		// StartAVISequence(1) 0x68F477..0x68F4E9: PlayFullScreenMovie("data\intro.bik", 0) 0x54D920 (the tip video
		// cleared, the pause kept in VideoPreviousPause and PauseGame(1), the wide screen on, the Bink opened; the
		// player object is made even when the file does not open, 0x54AC05), then g+0x25018C = 58 x fps,
		// g+0x250190 = 60 x fps (the film's first 60 s: the pause given back after 58 s and the picture faded out up to
		// 60 s, Process3dEngine 0x54DB27..0x54DB7F), g+0x250530 = 1, and GScript::SetupScreenFadeBackToNormal(0)
		// 0x6EBB00: the black fade gone at once.
		// (aproximado) openblack plays no Bink film: as if it had ended at once (the pause and the wide screen as they
		// were, GGame::FinishedVideo 0x54D8D0), only the fade is cleared
		NotImplemented("SetAviSequence (data\\intro.bik)");
		Game::Instance()->GetScreenFade().FadeBackToNormal(0.0f);
		return;
	}
	if (sequence == 2)
	{
		// StartAVISequence(2) 0x68F459..0x68F471: GGame::KickOffFallingSpellVideo 0x5539A0 (not ported) and
		// SetupScreenFadeBackToNormal(0)
		NotImplemented("SetAviSequence (KickOffFallingSpellVideo)");
		Game::Instance()->GetScreenFade().FadeBackToNormal(0.0f);
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
	NotImplemented(__func__);
}

void DevFunction() // 205 DEV_FUNCTION
{
	// const auto func = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void HasMouseWheel() // 206 HAS_MOUSE_WHEEL
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void NumMouseButtons() // 207 NUM_MOUSE_BUTTONS
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetCreatureDevStage() // 208 SET_CREATURE_DEV_STAGE
{
	// const auto stage = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetFixedCamRotation() // 209 SET_FIXED_CAM_ROTATION
{
	// GScript::SetFixedCamRotation 0x6EE1A0: POP the point (z, y, x) then "on"; only with the player's CameraModeNew3
	// current ("Wrong camera mode" 0xC0C29C otherwise): ForceRotateAboutPoint(on ? &point : 0) (vt+0x5C, 0x457330), the
	// player's camera then turns about that point (Camera/PlayerCameraScript.h; not read by DefaultWorldCameraModel yet)
	const auto point = PopVec();
	const bool on = Pop().uintVal != 0;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": the opening does not fix the player's camera (user, 2026-10-02)
	}
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
	NotImplemented(__func__);
}

void GetArena() // 211 GET_ARENA
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetFootballPitch() // 212 GET_FOOTBALL_PITCH
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void StopAllGames() // 213 STOP_ALL_GAMES
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void AttachToGame() // 214 ATTACH_TO_GAME
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void DetachFromGame() // 215 DETACH_FROM_GAME
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void DetachUndefinedFromGame() // 216 DETACH_UNDEFINED_FROM_GAME
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetOnlyForScripts() // 217 SET_ONLY_FOR_SCRIPTS
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StartMatchWithReferee() // 218 START_MATCH_WITH_REFEREE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GameTeamSize() // 219 GAME_TEAM_SIZE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GameType() // 220 GAME_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void GameSubType() // 221 GAME_SUB_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void IsLeashed() // 222 IS_LEASHED
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetCreatureHome() // 223 SET_CREATURE_HOME
{
	// const auto position = PopVec();
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetHitObject() // 224 GET_HIT_OBJECT
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetObjectWhichHit() // 225 GET_OBJECT_WHICH_HIT
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetNearestTownOfPlayer() // 226 GET_NEAREST_TOWN_OF_PLAYER
{
	// const auto unk4 = Pop().intVal;
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
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
	NotImplemented(__func__);
}

void IsFighting() // 229 IS_FIGHTING
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetMagicRadius() // 230 SET_MAGIC_RADIUS
{
	// const auto radius = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void TempTextWithNumber() // 231 TEMP_TEXT_WITH_NUMBER
{
	// GScript::TempTextWithNumber 0x6F7F50
	const auto withInteraction = Pop().intVal;
	const auto value = Popf();
	const auto format = PopString();
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->TempTextWithNumber(singleLine, WidenScriptString(format), value, withInteraction);
	}
}

void RunTextWithNumber() // 232 RUN_TEXT_WITH_NUMBER
{
	// GScript::RunTextWithNumber 0x6F7C70
	const auto withInteraction = Pop().intVal;
	const auto number = Popf();
	const auto textID = static_cast<uint32_t>(Pop().intVal);
	const auto singleLine = static_cast<bool>(Pop().intVal);
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->RunTextWithNumber(singleLine, textID, number, withInteraction);
	}
}

void CreatureSpellReversion() // 233 CREATURE_SPELL_REVERSION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetDesire() // 234 GET_DESIRE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetEventsPerSecond() // 235 GET_EVENTS_PER_SECOND
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetTimeSince() // 236 GET_TIME_SINCE
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetTotalEvents() // 237 GET_TOTAL_EVENTS
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void UpdateSnapshot() // 238 UPDATE_SNAPSHOT
{
	// const auto challengeId = Pop().intVal;
	// const auto argc = Pop().intVal;
	// const auto argv = PopVarArg(argc);
	// const auto reminderScript = PopString();
	// const auto titleStrID = Pop().intVal;
	// const auto alignment = Popf();
	// const auto success = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreateReward() // 239 CREATE_REWARD
{
	// const auto fromSky = static_cast<bool>(Pop().intVal);
	// const auto position = PopVec();
	// const auto reward = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CreateRewardInTown() // 240 CREATE_REWARD_IN_TOWN
{
	// const auto fromSky = static_cast<bool>(Pop().intVal);
	// const auto position = PopVec();
	// const auto town = Pop().uintVal;
	// const auto reward = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetFade() // 241 SET_FADE
{
	// 0x6FCD70 -> SetupScreenFadeTo 0x6EBA90 (every argument truncated; colour 0..255, time in seconds)
	const auto time = Popf();
	const auto blue = Popf();
	const auto green = Popf();
	const auto red = Popf();
	const auto channel = [](float value) { return static_cast<uint8_t>(static_cast<int>(value)); };
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": the opening does not black out the screen
	}
	Game::Instance()->GetScreenFade().FadeTo(channel(red), channel(green), channel(blue), time);
}

void SetFadeIn() // 242 SET_FADE_IN
{
	// 0x6FCE00 -> SetupScreenFadeBackToNormal 0x6EBB00
	const auto duration = Popf();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start": it faded nothing out, so there is nothing to fade back in
	}
	Game::Instance()->GetScreenFade().FadeBackToNormal(duration);
}

void FadeFinished() // 243 FADE_FINISHED
{
	// 0x6FCE40: no fade in progress
	Pushb(Game::Instance()->GetScreenFade().IsFinished());
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
	// GScript::SpiritSpeaks 0x710C40: POP the text, then the SCRIPT_SPIRIT_TYPE; ConvertScriptSpiritToHelpSpirit 0x710350
	// (the local player's alignment: inferred, openblack's local player is PLAYER_ONE; LocalRand: openblack's generator);
	// text 0 past 6974 (0x710C6E); push HelpSystem::GetSpiritWhoTalks 0x5C6E20 == the spirit (type 6)
	auto text = static_cast<uint32_t>(Pop().intVal);
	const auto type = Pop().intVal;
	const int discrete = audio::DiscreteAlignment(ecs::effects::alignment::Get(PlayerNames::PLAYER_ONE));
	const auto spirit =
	    help::ConvertScriptSpiritToHelpSpirit(type, discrete, []() { return audio::tags::RandomSample(0, 100); });
	if (text >= helptext::k_TextCount)
	{
		text = 0;
	}
	Pushb(help::SpiritWhoTalks(helptext::GetEntry(text).narrator) == spirit);
}

void BeliefForPlayer() // 247 BELIEF_FOR_PLAYER
{
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetHelp() // 248 GET_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetLeashWorks() // 249 SET_LEASH_WORKS
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void LoadMyCreature() // 250 LOAD_MY_CREATURE
{
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ObjectRelativeBelief() // 251 OBJECT_RELATIVE_BELIEF
{
	// const auto belief = Popf();
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreateWithAngleAndScale() // 252 CREATE_WITH_ANGLE_AND_SCALE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);
	const auto scale = Popf();
	const auto angle = Popf();

	// GScript::CreateWithAngleAndScale 0x6F2E10: the angle is in degrees (x 0.0174533)
	const entt::entity object = type > ObjectType::None && type <= ObjectType::AnimatedStatic
	                                ? CreateScriptObject(type, subtype, position, glm::radians(angle), scale)
	                                : entt::null;
	// 0x6F2F01: AddScriptGameThing(thing, 1), a thing the script created
	ecs::script_held::AddScriptThing(object, true);

	Pusho(object == entt::null ? 0 : static_cast<uint32_t>(object));
}

void SetHelpSystem() // 253 SET_HELP_SYSTEM
{
	// GScript::SetHelpSystem 0x6FC020: HelpSystem+0x45F8 = the popped value as it is (0x6FC03D)
	const auto on = Pop().intVal;
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->SetHelpOn(static_cast<uint32_t>(on));
	}
}

void SetVirtualInfluence() // 254 SET_VIRTUAL_INFLUENCE
{
	// const auto player = Popf();
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetActive() // 255 SET_ACTIVE
{
	const auto object = Pop().uintVal;
	const auto active = static_cast<bool>(Pop().intVal);
	// GameThing vt 0x1C0 SetActive: only the spell dispensers are ported (Magic/Script/CHLWorship.cpp)
	if (object == 0 || !magic::script::SetDispenserActive(static_cast<entt::entity>(object), active))
	{
		NotImplemented(__func__);
	}
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
	// const auto vortex = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void RemoveReactionOfType() // 258 REMOVE_REACTION_OF_TYPE
{
	// const auto reaction = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureLearnEverythingExcluding() // 259 CREATURE_LEARN_EVERYTHING_EXCLUDING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void PlayedPercentage() // 260 PLAYED_PERCENTAGE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void ObjectCastByObject() // 261 OBJECT_CAST_BY_OBJECT
{
	// const auto caster = Pop().uintVal;
	// const auto spellInstance = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void IsWindMagicAtPos() // 262 IS_WIND_MAGIC_AT_POS
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void GetObjectFade() // 265 GET_OBJECT_FADE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void PlayHandDemo() // 266 PLAY_HAND_DEMO
{
	// const auto withoutHandModify = static_cast<bool>(Pop().intVal);
	// const auto withPause = static_cast<bool>(Pop().intVal);
	// const auto string = PopString();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsPlayingHandDemo() // 267 IS_PLAYING_HAND_DEMO
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetArsePosition() // 268 GET_ARSE_POSITION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void IsLeashedToObject() // 269 IS_LEASHED_TO_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetInteractionMagnitude() // 270 GET_INTERACTION_MAGNITUDE
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void IsCreatureAvailable() // 271 IS_CREATURE_AVAILABLE
{
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void CreateHighlight() // 272 CREATE_HIGHLIGHT
{
	// const auto challengeID = Pop().intVal;
	// const auto position = PopVec();
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetObjectHeld273() // 273 GET_OBJECT_HELD
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetActionCount() // 274 GET_ACTION_COUNT
{
	// const auto creature = Pop().uintVal;
	// const auto action = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetObjectLeashType() // 275 GET_OBJECT_LEASH_TYPE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void SetFocusFollow() // 276 SET_FOCUS_FOLLOW
{
	// GScript::SetFocusFollow 0x6EDB40: the same code as FOCUS_FOLLOW 0x6EDF30 (CameraModeScript::SetCameraFocus(thing)
	// 0x4619B0, no placing at once)
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusFollow(*thing);
	}
}

void SetPositionFollow() // 277 SET_POSITION_FOLLOW
{
	// GScript::SetPositionFollow 0x6EDA80: POP the thing -> CameraModeFollow::Set(thing) 0x44BA00 and fn_0044BB30: the
	// focus and the position placed at once on the follow's points (the mode's seconds at 2: then followed at the
	// time factor's pace)
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::PositionFollow(*thing);
		script_camera::PlaceFollowNow();
	}
}

void SetFocusAndPositionFollow() // 278 SET_FOCUS_AND_POSITION_FOLLOW
{
	// GScript::SetFocusAndPositionFollow 0x6ED9B0: POP the distance, then the thing -> fn_0044BA90(thing, distance) and
	// fn_0044BB30 (placed at once)
	const auto distance = Popf();
	const auto object = Pop().uintVal;
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	const auto thing = CameraThing(object, __func__);
	if (thing.has_value() && ScriptCameraMode(__func__))
	{
		script_camera::FocusAndPositionFollow(*thing, distance);
		script_camera::PlaceFollowNow();
	}
}

void SetCameraLens() // 279 SET_CAMERA_LENS
{
	// GScript::SetCameraLens 0x6EE2E0: GCamera::SetCameraFov(70 degrees, x) (0x6EE302..0x6EE325): the argument is the
	// TIME and the lens goes back to the default (copied as the original does; its one use is SET_CAMERA_LENS(0))
	const auto time = Popf();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	script_camera::SetFov(script_camera::k_DefaultFov, time);
}

void MoveCameraLens() // 280 MOVE_CAMERA_LENS
{
	// GScript::MoveCameraLens 0x6EE280: GCamera::SetCameraFov(lens * 0.0174533, t) (degrees, seconds of game time)
	const auto time = Popf();
	const auto lens = Popf();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	script_camera::SetFov(lens * script_camera::k_DegreesToRadians, time);
}

void CreatureReaction() // 281 CREATURE_REACTION
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureInDevScript() // 282 CREATURE_IN_DEV_SCRIPT
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StoreCameraDetails() // 283 STORE_CAMERA_DETAILS
{
	// GScript::StoreCameraDetails 0x6EE330: GScript +0x54 = LH3DTech::g_camera, +0x60 = the drawn focus (not the FOV)
	const auto& camera = Locator::camera::value();
	auto& state = script_camera::Get();
	state.storedPosition = camera.GetOrigin();
	state.storedFocus = camera.GetFocus();
}

void RestoreCameraDetails() // 284 RESTORE_CAMERA_DETAILS
{
	// GScript::RestoreCameraDetails 0x6EE390: GCamera::SetPositionAndFocus 0x4438C0 with them, whatever the mode
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	const auto& state = script_camera::Get();
	script_camera::SetPositionAndFocus(state.storedPosition, state.storedFocus);
	if (!script_camera::Active())
	{
		// (inferido) the player's mode: openblack's player Camera has its own zoomers (not script_camera's), so it is set
		// here too
		Locator::camera::value().SetOrigin(state.storedPosition).SetFocus(state.storedFocus);
	}
}

void StartAngleSound285() // 285 START_ANGLE_SOUND
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCameraPosFocLens() // 286 SET_CAMERA_POS_FOC_LENS
{
	// GScript::SetCameraPosFocLens 0x6EE3C0: GCamera::SetPositionAndFocus 0x4438C0 and SetCameraFov(lens, 0) with the lens
	// NOT turned into radians (0x6EE480; copied as the original does, no map uses it)
	const auto lens = Popf();
	const auto focus = PopVec();
	const auto position = PopVec();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	script_camera::SetPositionAndFocus(position, focus);
	if (!script_camera::Active())
	{
		Locator::camera::value().SetOrigin(position).SetFocus(focus); // (inferido) as in 284
	}
	script_camera::SetFov(lens, 0.0f);
}

void MoveCameraPosFocLens() // 287 MOVE_CAMERA_POS_FOC_LENS
{
	// GScript::MoveCameraPosFocLens 0x6EE4B0: CameraModeScript::MoveCameraPosition / MoveCameraFocus in t and
	// SetCameraFov(lens, t), the lens not in radians (0x6EE613; copied, no map uses it)
	const auto time = Popf();
	const auto lens = Popf();
	const auto focus = PopVec();
	const auto position = PopVec();
	if (FreeStart())
	{
		return; // mod game.skip-intro, "free start"
	}
	if (ScriptCameraMode(__func__))
	{
		script_camera::MovePosition(position, time);
		script_camera::MoveFocus(focus, time);
		script_camera::SetFov(lens, time);
	}
}

void GameTimeOnOff() // 288 GAME_TIME_ON_OFF
{
	// GScript::GameTimeOnOff 0x710E90: SetVisualTimeScale(on ? 1 : 0)
	const auto enable = Pop().intVal != 0;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "GAME_TIME_ON_OFF({})", enable);
	Game::Instance()->GetDayNightClock().SetScale(enable ? 1.0f : 0.0f);
}

void MoveGameTime() // 289 MOVE_GAME_TIME
{
	// GScript::MoveGameTime 0x710EC0: the visual time slides to the hour in `duration` seconds of game time
	const auto duration = Popf();
	const auto hourOfTheDay = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "MOVE_GAME_TIME({}, {})", hourOfTheDay, duration);
	Game::Instance()->GetDayNightClock().MoveScriptTime(hourOfTheDay, duration);
}

void SetHighGraphicsDetail() // 290 SET_HIGH_GRAPHICS_DETAIL
{
	// const auto object = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetSkeleton() // 291 SET_SKELETON
{
	// const auto object = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsSkeleton() // 292 IS_SKELETON
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void AddSpotVisualTargetObject() // 297 ADD_SPOT_VISUAL_TARGET_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetIndestructable() // 298 SET_INDESTRUCTABLE
{
	// GScript::SetIndestructable 0x6FDE20: POP the object (GetScriptGameThing), POP the flag; a script container
	// (IsScriptContainer vt +0x3F8) hands it to its type's callback (table 0xC0C73C, TODO: openblack has none), anything
	// else sets or clears Flags +0x24 bit 0x4000 (0x6FDEB3)
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
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SpiritAppear() // 300 SPIRIT_APPEAR
{
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SpiritDisappear() // 301 SPIRIT_DISAPPEAR
{
	// const auto spirit = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetFocusOnObject() // 302 SET_FOCUS_ON_OBJECT
{
	// const auto target = Pop().uintVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ReleaseObjectFocus() // 303 RELEASE_OBJECT_FOCUS
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ImmersionExists() // 304 IMMERSION_EXISTS
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetDrawLeash() // 305 SET_DRAW_LEASH
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetDrawHighlight() // 306 SET_DRAW_HIGHLIGHT
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetOpenClose() // 307 SET_OPEN_CLOSE
{
	// const auto object = Pop().uintVal;
	// const auto open = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetIntroBuilding() // 308 SET_INTRO_BUILDING
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CreatureForceFriends() // 309 CREATURE_FORCE_FRIENDS
{
	// const auto targetCreature = Pop().uintVal;
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void MoveComputerPlayerPosition() // 310 MOVE_COMPUTER_PLAYER_POSITION
{
	// const auto withFixedHeight = static_cast<bool>(Pop().intVal);
	// const auto speed = Popf();
	// const auto position = PopVec();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void EnableDisableComputerPlayer311() // 311 ENABLE_DISABLE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetComputerPlayerPosition() // 312 GET_COMPUTER_PLAYER_POSITION
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void GetStoredCameraPosition() // 314 GET_STORED_CAMERA_POSITION
{
	// GScript::GetStoredCameraPosition 0x6EE630: GScript +0x54
	PushVec(script_camera::Get().storedPosition);
}

void GetStoredCameraFocus() // 315 GET_STORED_CAMERA_FOCUS
{
	// GScript::GetStoredCameraFocus 0x6EE6A0: GScript +0x60
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
	NotImplemented(__func__);
	Pusho(0);
}

void SetCreatureSound() // 317 SET_CREATURE_SOUND
{
	// GScript::SetCreatureSound 0x710020: GScript+0x84 = the value as it is (0x71003D)
	audio::GetScriptAudioState().creatureSound = Pop().intVal;
}

void CreatureInteractingWith() // 318 CREATURE_INTERACTING_WITH
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetSunDraw() // 319 SET_SUN_DRAW
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ObjectInfoBits() // 320 OBJECT_INFO_BITS
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
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
	NotImplemented(__func__);
}

void ClearConfinedObject() // 323 CLEAR_CONFINED_OBJECT
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetObjectFlock() // 324 GET_OBJECT_FLOCK
{
	// const auto member = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetPlayerBelief() // 325 SET_PLAYER_BELIEF
{
	// const auto belief = Popf();
	// const auto player = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void PlayJcSpecial() // 326 PLAY_JC_SPECIAL
{
	// GScript::PlayJCSpecial 0x708ED0 (table 0x708F74 on the value, 0..15): 0, 1, 2, 4, 5, 6 -> fn_005DF9C0(value),
	// 3 a ScriptGFX object (0x828DB0), 14 / 15 [0x9CD384] = 1 / 0
	const auto feature = Pop().intVal;
	if (feature == 6)
	{
		// fn_005DF9C0 case 6 (0x5DFBF8): new PetitNavire(0), the missionaries' boat
		openblack::ecs::petit_navire::Create(0);
		return;
	}
	// TODO(Daniels118): the other specials
	NotImplemented(__func__);
}

void IsPlayingJcSpecial() // 327 IS_PLAYING_JC_SPECIAL
{
	// GScript::IsPlayingJCSpecial 0x708FC0: ftol of the value; 1, except 13 -> [0xD19C94], which only the hand intro
	// (fn_005DF640, special 4, not ported) sets: 0 here
	const auto feature = static_cast<int32_t>(Popf());
	Pushb(feature != 13);
}

void VortexParameters() // 328 VORTEX_PARAMETERS
{
	// const auto flock = Pop().uintVal;
	// const auto radius = Popf();
	// const auto distance = Popf();
	// const auto position = PopVec();
	// const auto town = Pop().uintVal;
	// const auto vortex = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void LoadCreature() // 329 LOAD_CREATURE
{
	// const auto position = PopVec();
	// const auto player = Popf();
	// const auto mindFilename = PopString();
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
	Pushi(0);
}

void FlockWithinLimits() // 333 FLOCK_WITHIN_LIMITS
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void HighlightProperties() // 334 HIGHLIGHT_PROPERTIES
{
	// const auto category = Pop().intVal;
	// const auto text = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void LastMusicLine() // 335 LAST_MUSIC_LINE
{
	// 0x710050: without audio the original runs TEXT_READ instead (0x7100A6)
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
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetBellyPosition() // 337 GET_BELLY_POSITION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void GameThingCanViewCamera() // 339 GAME_THING_CAN_VIEW_CAMERA
{
	// const auto degrees = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GamePlaySaySoundEffect() // 340 GAME_PLAY_SAY_SOUND_EFFECT
{
	// GScript::GamePlaySaySoundEffect 0x70F9B0: six POPs (withPos, the point, the text, alt), then
	// SaySoundEffect 0x70F8E0(text, withPos, alt, &point) (audio::voices::Say)
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto alt = Pop().intVal != 0;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "GAME_PLAY_SAY_SOUND_EFFECT({}, {}, ({}, {}, {}), {})", alt, text,
	                    position.x, position.y, position.z, withPosition);
	audio::voices::Say(text, withPosition, alt, position);
}

void SetTownDesireBoost() // 341 SET_TOWN_DESIRE_BOOST
{
	// const auto boost = Popf();
	// const auto desire = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsLockedInteraction() // 342 IS_LOCKED_INTERACTION
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetCreatureName() // 343 SET_CREATURE_NAME
{
	// const auto textID = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ComputerPlayerReady() // 344 COMPUTER_PLAYER_READY
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void EnableDisableComputerPlayer345() // 345 ENABLE_DISABLE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// const auto pause = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ClearActorMind() // 346 CLEAR_ACTOR_MIND
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ThingJcSpecial() // 349 THING_JC_SPECIAL
{
	// const auto target = Pop().uintVal;
	// const auto feature = Pop().intVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void MusicPlayed350() // 350 MUSIC_PLAYED
{
	// 0x70FBA0: GAudio+0x28 != type (a bool, VMType 6); without audio the original runs TEXT_READ instead (0x70FBE4)
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
	// const auto challengeID = Pop().intVal;
	// const auto takingPicture = static_cast<bool>(Pop().intVal);
	// const auto titleStrID = Pop().intVal;
	// const auto alignment = Popf();
	// const auto success = Popf();
	// const auto focus = PopVec();
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
	Pusho(0);
}

void ToggleLeash() // 354 TOGGLE_LEASH
{
	// const auto player = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	// GScript::SetGameSound 0x7100B0: false -> LHSampleStopAll (fn_004287D0) and GScript+0x90 = 1 (only the dialogue
	// banks HelpSprites / Villagers play, GAudio::PlaySoundEffect 0x429F7C); true -> +0x90 = 0
	const auto enable = static_cast<bool>(Pop().intVal);
	audio::SetGameSound(enable);
}

void SexIsMale() // 358 SEX_IS_MALE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetFirstHelp() // 359 GET_FIRST_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetLastHelp() // 360 GET_LAST_HELP
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void IsActive() // 361 IS_ACTIVE
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetBookmarkPosition() // 362 SET_BOOKMARK_POSITION
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetScaffoldProperties() // 363 SET_SCAFFOLD_PROPERTIES
{
	// const auto destroy = static_cast<bool>(Pop().intVal);
	// const auto size = Popf();
	// const auto type = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetComputerPlayerPersonality() // 364 SET_COMPUTER_PLAYER_PERSONALITY
{
	// const auto probability = Popf();
	// const auto aspect = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetComputerPlayerSuppression() // 365 SET_COMPUTER_PLAYER_SUPPRESSION
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ForceComputerPlayerAction() // 366 FORCE_COMPUTER_PLAYER_ACTION
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// const auto action = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void QueueComputerPlayerAction() // 367 QUEUE_COMPUTER_PLAYER_ACTION
{
	// const auto obj2 = Pop().uintVal;
	// const auto obj1 = Pop().uintVal;
	// const auto action = PopString();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetTownWithId() // 368 GET_TOWN_WITH_ID
{
	// const auto id = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetDisciple() // 369 SET_DISCIPLE
{
	// const auto withSound = static_cast<bool>(Pop().intVal);
	// const auto discipleType = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ReleaseComputerPlayer() // 370 RELEASE_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetComputerPlayerSpeed() // 371 SET_COMPUTER_PLAYER_SPEED
{
	// const auto speed = Popf();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetFocusFollowComputerPlayer() // 372 SET_FOCUS_FOLLOW_COMPUTER_PLAYER
{
	// GScript 0x6EDC00: POP the player (a float, ftol), ConvertScriptPlayerToGamePlayer 0x6EB9A0, the mode checks, then
	// fn_004619F0: the path dropped, SetCameraFocus(0) 0x4619B0 and +0x50 = the player (CameraModeFollow::Update follows
	// GComputerPlayer::GetHandPos 0x657FE0). Not ported: openblack has no computer players
	[[maybe_unused]] const auto player = Popf();
	NotImplemented(__func__);
}

void SetPositionFollowComputerPlayer() // 373 SET_POSITION_FOLLOW_COMPUTER_PLAYER
{
	// GScript 0x6EDCD0: as 372 with fn_00461A10: the path dropped, Set(0) 0x44BA00 and +0x54 = the player. Not ported
	[[maybe_unused]] const auto player = Popf();
	NotImplemented(__func__);
}

void CallComputerPlayer() // 374 CALL_COMPUTER_PLAYER
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void CallBuildingInTown() // 375 CALL_BUILDING_IN_TOWN
{
	// const auto unk3 = Pop().intVal;
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void SetCanBuildWorshipsite() // 376 SET_CAN_BUILD_WORSHIPSITE
{
	magic::script::SetCanBuildWorshipsite(); // Magic/Script/CHLWorship.cpp
}

void GetFacingCameraPosition() // 377 GET_FACING_CAMERA_POSITION
{
	// GScript::GetFacingCameraPosition 0x6EE710: LH3DTech::g_camera + d * the camera's forward vector (0xEA1DD4..0xEA1DDC;
	// taken as the unit vector from the drawn position to the drawn focus: inferido)
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
	NotImplemented(__func__);
}

void GetComputerPlayerAttitude() // 379 GET_COMPUTER_PLAYER_ATTITUDE
{
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void LoadComputerPlayerPersonality() // 380 LOAD_COMPUTER_PLAYER_PERSONALITY
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SaveComputerPlayerPersonality() // 381 SAVE_COMPUTER_PLAYER_PERSONALITY
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetPlayerAlly() // 382 SET_PLAYER_ALLY
{
	// const auto percentage = Popf();
	// const auto player2 = Popf();
	// const auto player1 = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void CallFlying() // 383 CALL_FLYING
{
	// const auto excluding = static_cast<bool>(Pop().intVal);
	// const auto radius = Popf();
	// const auto position = PopVec();
	// const auto subtype = Pop().intVal;
	// const auto type = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void SetObjectFadeIn() // 384 SET_OBJECT_FADE_IN
{
	// const auto time = Popf();
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsAffectedBySpell() // 385 IS_AFFECTED_BY_SPELL
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
	Pushf(0.0f);
}

void ObjectCapacity() // 388 OBJECT_CAPACITY
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void ObjectAdultCapacity() // 389 OBJECT_ADULT_CAPACITY
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetCreatureAutoFighting() // 390 SET_CREATURE_AUTO_FIGHTING
{
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void IsAutoFighting() // 391 IS_AUTO_FIGHTING
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void SetCreatureQueueFightMove() // 392 SET_CREATURE_QUEUE_FIGHT_MOVE
{
	// const auto move = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureQueueFightSpell() // 393 SET_CREATURE_QUEUE_FIGHT_SPELL
{
	// const auto spell = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureQueueFightStep() // 394 SET_CREATURE_QUEUE_FIGHT_STEP
{
	// const auto step = Pop().intVal;
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetCreatureFightAction() // 395 GET_CREATURE_FIGHT_ACTION
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void CreatureFightQueueHits() // 396 CREATURE_FIGHT_QUEUE_HITS
{
	// const auto creature = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SetPlayerWindResistance() // 399 SET_PLAYER_WIND_RESISTANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void GetPlayerWindResistance() // 400 GET_PLAYER_WIND_RESISTANCE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void RestartObject() // 406 RESTART_OBJECT
{
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetGameTimeProperties() // 407 SET_GAME_TIME_PROPERTIES
{
	// GScript::SetGameTimeProperties 0x7114B0: SetVisualTimeCycle(duration, percentage night, percentage change)
	const auto percentageChange = Popf();
	const auto percentageNight = Popf();
	const auto duration = Popf();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SET_GAME_TIME_PROPERTIES({}, {}, {})", duration, percentageNight, percentageChange);
	Game::Instance()->GetDayNightClock().SetCycle(duration, percentageNight, percentageChange);
}

void ResetGameTimeProperties() // 408 RESET_GAME_TIME_PROPERTIES
{
	// GScript::ResetGameTimeProperties 0x711520
	Game::Instance()->GetDayNightClock().SetCycle(DayNightClock::k_DefaultDuration, DayNightClock::k_DefaultNight,
	                                              DayNightClock::k_DefaultChange);
}

void SoundExists() // 409 SOUND_EXISTS
{
	// GScript::SoundExists 0x710100: GAudio::IsInstalled 0x426D30 (LHWaveIsInstalled), pushed as a bool (VMType 6)
	Pushb(audio::SoundExists());
}

void GetTownWorshipDeaths() // 410 GET_TOWN_WORSHIP_DEATHS
{
	magic::script::GetTownWorshipDeaths(); // Magic/Script/CHLWorship.cpp
}

void GameClearDialogue() // 411 GAME_CLEAR_DIALOGUE
{
	// GScript::GameClearDialogue 0x6FF6F0: HelpSystem::ClearAllText (the voices go on, voices.md §2.4)
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->ClearDialogue();
	}
}

void GameCloseDialogue() // 412 GAME_CLOSE_DIALOGUE
{
	// GScript::GameCloseDialogue 0x6FF700: HelpText fn_005CB010 and HelpSystem::ClearAllText
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->CloseDialogue();
	}
}

void GetHandState() // 413 GET_HAND_STATE
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushi(0);
}

void SetInterfaceCitadel() // 414 SET_INTERFACE_CITADEL
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void MapScriptFunction() // 415 MAP_SCRIPT_FUNCTION
{
	// const auto command = PopString();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void WithinRotation() // 416 WITHIN_ROTATION
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetPlayerTownTotal() // 417 GET_PLAYER_TOWN_TOTAL
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void SpiritScreenPoint() // 418 SPIRIT_SCREEN_POINT
{
	// const auto unk2 = Pop().intVal;
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void KeyDown() // 419 KEY_DOWN
{
	// const auto key = Pop().intVal;
	// TODO(Daniels118): implement this (translate key to physical key code)
	NotImplemented(__func__);
	Pushb(false);
}

void SetFightExit() // 420 SET_FIGHT_EXIT
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetObjectClicked() // 421 GET_OBJECT_CLICKED
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
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
	// GScript::StopSoundEffect 0x70FA50: POPs bank, sample (or a HELP_TEXT with isSay), isSay (the scripts of W120 always
	// pass 0)
	const auto bank = Pop().intVal;
	const auto id = Pop().uintVal;
	const auto isSay = Pop().intVal != 0;
	audio::script_sound::StopSoundEffect(isSay, id, bank);
}

void GetTotemStatue() // 425 GET_TOTEM_STATUE
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
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
	NotImplemented(__func__);
}

void SetObjectBeliefScale() // 428 SET_OBJECT_BELIEF_SCALE
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StartImmersion() // 429 START_IMMERSION
{
	// const auto effect = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StopImmersion() // 430 STOP_IMMERSION
{
	// const auto effect = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void StopAllImmersion() // 431 STOP_ALL_IMMERSION
{
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetCreatureInTemple() // 432 SET_CREATURE_IN_TEMPLE
{
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
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
	NotImplemented(__func__);
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
	NotImplemented(__func__);
}

void FadeAllDrawText() // 435 FADE_ALL_DRAW_TEXT
{
	// const auto time = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetDrawTextColour() // 436 SET_DRAW_TEXT_COLOUR
{
	// const auto blue = Popf();
	// const auto green = Popf();
	// const auto red = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetClippingWindow() // 437 SET_CLIPPING_WINDOW
{
	// const auto time = Popf();
	// const auto height = Popf();
	// const auto width = Popf();
	// const auto down = Popf();
	// const auto across = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void ClearClippingWindow() // 438 CLEAR_CLIPPING_WINDOW
{
	// const auto time = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SaveGameInSlot() // 439 SAVE_GAME_IN_SLOT
{
	// const auto slot = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void SetObjectCarrying() // 440 SET_OBJECT_CARRYING
{
	// const auto carriedObj = Pop().intVal;
	// const auto object = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void PosValidForCreature() // 441 POS_VALID_FOR_CREATURE
{
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
}

void GetTimeSinceObjectAttacked() // 442 GET_TIME_SINCE_OBJECT_ATTACKED
{
	// const auto town = Pop().uintVal;
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GetTownAndVillagerHealthTotal() // 443 GET_TOWN_AND_VILLAGER_HEALTH_TOTAL
{
	// const auto town = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GameAddForBuilding() // 444 GAME_ADD_FOR_BUILDING
{
	// const auto unk1 = Pop().intVal;
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void EnableDisableAlignmentMusic() // 445 ENABLE_DISABLE_ALIGNMENT_MUSIC
{
	// 0x710120: GScript+0x94 = the value as it is (0x71013D)
	const auto enable = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "ENABLE_DISABLE_ALIGNMENT_MUSIC({})", enable);
	audio::GetScriptAudioState().alignmentMusic = enable;
}

void GetDeadLiving() // 446 GET_DEAD_LIVING
{
	// const auto radius = Popf();
	// const auto position = PopVec();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void AttachSoundTag() // 447 ATTACH_SOUND_TAG
{
	// GScript::AttachSoundTag 0x710150: POPs the object (GetScriptGameThing 0x70D220 at once), bank, sample, threeD
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
	// GScript::DetachSoundTag 0x7101D0: POPs the object (GetScriptGameThing 0x70D220 at once), bank, sample
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
	NotImplemented(__func__);
	Pushf(0.0f);
}

void GameSoundPlaying() // 450 GAME_SOUND_PLAYING
{
	// GScript::IsSoundPlaying 0x710230: POPs bank, sample; pushes fn_0042A280(sample, sample, bank) as a bool (VMType 6)
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	Pushb(audio::script_sound::GameSoundPlaying(sample, bank));
}

void GetTemplePosition() // 451 GET_TEMPLE_POSITION
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureAutoscale() // 452 CREATURE_AUTOSCALE
{
	// const auto size = Popf();
	// const auto creature = Pop().uintVal;
	// const auto enable = static_cast<bool>(Pop().intVal);
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetSpellIconInTemple() // 453 GET_SPELL_ICON_IN_TEMPLE
{
	magic::script::GetSpellIconInTemple(); // Magic/Script/CHLWorship.cpp
}

void GameClearComputerPlayerActions() // 454 GAME_CLEAR_COMPUTER_PLAYER_ACTIONS
{
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

void GetFirstInContainer() // 455 GET_FIRST_IN_CONTAINER
{
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetNextInContainer() // 456 GET_NEXT_IN_CONTAINER
{
	// const auto after = Pop().uintVal;
	// const auto container = Pop().uintVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pusho(0);
}

void GetTempleEntrancePosition() // 457 GET_TEMPLE_ENTRANCE_POSITION
{
	// const auto height = Popf();
	// const auto radius = Popf();
	// const auto player = Popf();
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SaySoundEffectPlaying() // 458 SAY_SOUND_EFFECT_PLAYING
{
	// GScript::SaySoundEffectPlaying 0x710280: POP the text, then alt; push audio::voices::IsSaying (type 6)
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto alt = Pop().intVal != 0;
	Pushb(audio::voices::IsSaying(alt, text));
}

void SetHandDemoKeys() // 459 SET_HAND_DEMO_KEYS
{
	// const auto unk0 = Pop().intVal;
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
}

// The three push a bit of g_game+0x14 as a boolean (VMType 6); the bits are set at each new game by the SkipBox answer
// (Game::Run, GGame::OnNewGame 0x55395B). SetupLand1 turns them into IsSkippingToCreatureSelect, IsSkippingCreatureGuide
// and IsKeepingOldCreature, which LandControl1 reads (docs/bw1-notes/map-loading.md).
void CanSkipTutorial() // 460 CAN_SKIP_TUTORIAL
{
	// GScript::CanSkipTutorial 0x6FFEF0: (g_game+0x14 >> 23) & 1
	Pushb(Game::Instance()->GetTutorialSkipFlags().canSkipTutorial);
}

void CanSkipCreatureTraining() // 461 CAN_SKIP_CREATURE_TRAINING
{
	// GScript::CanSkipCreatureTraining 0x6FFF10: (g_game+0x14 >> 24) & 1
	Pushb(Game::Instance()->GetTutorialSkipFlags().canSkipCreatureTraining);
}

void IsKeepingOldCreature() // 462 IS_KEEPING_OLD_CREATURE
{
	// GScript::IsKeepingOldCreature 0x6FFF30: (g_game+0x14 >> 25) & 1
	Pushb(Game::Instance()->GetTutorialSkipFlags().isKeepingOldCreature);
}

void CurrentProfileHasCreature() // 463 CURRENT_PROFILE_HAS_CREATURE
{
	// The original reads the player's profile, which openblack does not have. SetupLand1 ands it with bit 25
	// (challenge.chl 25432..25434), so without an answer the SkipBox's fourth answer could never take effect: mod
	// game.skip-intro answers it for the profile when that is the answer it gave (EngineConfig::skipTutorialChoice = 3)
	if (Game::Instance()->GetTutorialSkipFlags().isKeepingOldCreature)
	{
		Pushb(true);
		return;
	}
	// TODO(Daniels118): implement this
	NotImplemented(__func__);
	Pushb(false);
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
	CREATE_FUNCTION_BINDING("GET_ACTION_TEXT_FOR_OBJECT", 1, 1, GetActionTextForObject);
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
	CREATE_FUNCTION_BINDING("IS_AFFECTED_BY_SPELL", 1, 1, IsAffectedBySpell);
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
