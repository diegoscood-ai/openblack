/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Api.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <map>

#include <fmt/format.h>
#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "Audio/Audio.h"
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "GameClock.h"
#include "Game.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellCreator.h"
#include "Magic/MagicTables.h"
#include "Magic/Script/CHLSpells.h"
#include "Mod.h"
#include "ModLog.h"
#include "ModRegistry.h"
#include "Replacements.h"
#include "Resources/ResourceManager.h"
#include "Switches.h"

namespace openblack::mods::api
{
namespace
{
/// The bank names mods use: the SfxBank enumerators (BankTables.h)
std::string_view SoundBankName(audio::SfxBank bank)
{
	switch (bank)
	{
	case audio::SfxBank::InGame:
		return "InGame";
	case audio::SfxBank::Editor:
		return "Editor";
	case audio::SfxBank::Spells:
		return "Spells";
	case audio::SfxBank::Creature:
		return "Creature";
	case audio::SfxBank::ScriptSfx:
		return "ScriptSfx";
	case audio::SfxBank::HelpSprites:
		return "HelpSprites";
	case audio::SfxBank::Villagers:
		return "Villagers";
	case audio::SfxBank::VillagersBanter:
		return "VillagersBanter";
	case audio::SfxBank::SpellDialogue:
		return "SpellDialogue";
	case audio::SfxBank::Guidance:
		return "Guidance";
	default:
		return "";
	}
}

std::vector<Interface>& InterfaceList()
{
	static std::vector<Interface> list;
	return list;
}

/// The sound owner of each mod that has played something (mod id -> audio::NewOwner)
std::map<std::string, audio::Owner, std::less<>>& SoundOwners()
{
	static std::map<std::string, audio::Owner, std::less<>> owners;
	return owners;
}
} // namespace

void Log(const Mod& mod, int level, std::string_view text)
{
	log::Write(level >= 2 ? log::Level::Error : level == 1 ? log::Level::Warning : log::Level::Info, mod.GetInfo().id, text);
}

std::string Option(const Mod& mod, std::string_view option)
{
	return mod.GetChoice(option);
}

bool SetSwitch(Mod& mod, std::string_view name, double value)
{
	if (!Locator::mods::has_value())
	{
		return false;
	}
	return Locator::mods::value().SetRuntimeSwitch(mod, name, value);
}

std::optional<double> GetSwitch(std::string_view name)
{
	if (switches::Find(name) == nullptr)
	{
		return std::nullopt;
	}
	return switches::Get(name);
}

std::vector<SwitchInfo> Switches()
{
	std::vector<SwitchInfo> list;
	for (const auto& value : switches::All())
	{
		list.push_back({value.name, std::string(switches::TypeName(value.type)), std::string(switches::WhenName(value.when)),
		                value.description, value.min, value.max});
	}
	return list;
}

void Provide(const Mod& mod, Interface interface)
{
	auto& list = InterfaceList();
	interface.provider = mod.GetInfo().id;
	const auto it = std::ranges::find_if(list, [&interface](const Interface& each) {
		return each.name == interface.name && each.language == interface.language;
	});
	if (it != list.end())
	{
		log::Warning(mod.GetInfo().id, fmt::format("interface {} ({}) was offered by {} already: this one replaces it",
		                                           interface.name, interface.language, it->provider));
		*it = std::move(interface);
		return;
	}
	log::Info(mod.GetInfo().id, fmt::format("offers interface {} ({})", interface.name, interface.language));
	list.push_back(std::move(interface));
}

const Interface* FindInterface(std::string_view name, std::string_view language)
{
	const auto& list = InterfaceList();
	const auto it = std::ranges::find_if(
	    list, [name, language](const Interface& each) { return each.name == name && each.language == language; });
	return it != list.end() ? &*it : nullptr;
}

const std::vector<Interface>& Interfaces()
{
	return InterfaceList();
}

void Withdraw(const Mod& mod)
{
	std::erase_if(InterfaceList(), [&mod](const Interface& each) { return each.provider == mod.GetInfo().id; });
}

std::vector<std::pair<std::string, int64_t>> Enumeration(std::string_view which)
{
	std::vector<std::pair<std::string, int64_t>> values;
	if (which == "meshes")
	{
		for (size_t i = 0; i < k_MeshNames.size(); ++i)
		{
			values.emplace_back(k_MeshNames[i], static_cast<int64_t>(i));
		}
	}
	else if (which == "magic" && Locator::infoConstants::has_value())
	{
		// MagicType by the debug name of its effect in info.dat (GMagicInfo::GetInfoFromText 0x5FB3B0 reads the same)
		const auto& info = Locator::infoConstants::value();
		for (size_t i = 1; i < magic::k_MagicTypeCount && i < info.magicEffect.size(); ++i)
		{
			values.emplace_back(info.magicEffect[i].debugString.data(), static_cast<int64_t>(i));
		}
	}
	else if (which == "object_tables" || which == "object_fields")
	{
		const auto names = which == "object_tables" ? replace::ObjectTables() : replace::ObjectFields();
		for (size_t i = 0; i < names.size(); ++i)
		{
			values.emplace_back(std::string(names[i]), static_cast<int64_t>(i));
		}
	}
	else if (which == "sound_banks")
	{
		for (size_t i = 1; i < static_cast<size_t>(audio::SfxBank::_COUNT); ++i)
		{
			values.emplace_back(std::string(SoundBankName(static_cast<audio::SfxBank>(i))), static_cast<int64_t>(i));
		}
	}
	else if (which == "switches")
	{
		const auto& all = switches::All();
		for (size_t i = 0; i < all.size(); ++i)
		{
			values.emplace_back(all[i].name, static_cast<int64_t>(i));
		}
	}
	return values;
}

std::vector<std::string_view> Enumerations()
{
	return {"meshes", "magic", "object_tables", "object_fields", "switches", "sound_banks"};
}

uint32_t Turn()
{
	return Game::Instance() != nullptr ? Game::Instance()->GetTurn() : 0;
}

float Hour()
{
	return Game::Instance() != nullptr ? Game::Instance()->GetDayNightClock().GetScriptTime() : 0.0f;
}

std::optional<float> GroundHeight(float x, float z)
{
	if (!Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
}

glm::vec3 CameraPosition()
{
	return Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : glm::vec3(0.0f);
}

glm::vec3 CameraFocus()
{
	return Locator::camera::has_value() ? Locator::camera::value().GetFocus() : glm::vec3(0.0f);
}

void SetCamera(const glm::vec3& position, const glm::vec3& focus)
{
	if (Locator::camera::has_value())
	{
		Locator::camera::value().SetOrigin(position).SetFocus(focus);
	}
}

bool CastMiracle(std::string_view magic, const glm::vec3& position, float radius, float seconds)
{
	if (!Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value();
	int type = 0;
	if (const auto [end, error] = std::from_chars(magic.data(), magic.data() + magic.size(), type);
	    error != std::errc() || end != magic.data() + magic.size())
	{
		type = magic::GetInfoFromText(info, magic);
	}
	if (type <= 0 || type >= static_cast<int>(magic::k_MagicTypeCount))
	{
		return false;
	}
	const auto magicType = static_cast<MagicType>(type);
	// no time given: the player's timer (timerWhenPlayerCasting, 0x5FB7A0), as a hand cast lasts
	const float duration = seconds < 0.0f ? magic::GetTimerWhenPlayerCasting(info, magicType) : seconds;
	// SPELL_AT_POS's "from": 30 m above the target, as OPENBLACK_TEST_SPELL does (inferred: the challenge scripts cast
	// from the sky). With the class check (CanCast at the point, vt 0x30), as a script that asks for it
	const glm::vec3 from = position + glm::vec3(0.0f, 30.0f, 0.0f);
	const auto spell = magic::script::CastSpellAtPos(position, magicType, from, magic::creator::NeutralPlayer(), true,
	                                                 radius, duration, 0.0f, glm::vec3(0.0f));
	return spell != entt::null;
}

float TurnFraction()
{
	return game_clock::TurnFraction();
}

bool Paused()
{
	return game_clock::IsPaused();
}

float GameSpeed()
{
	return game_clock::Speed();
}

Cell CellAt(float x, float z)
{
	const auto cell = ecs::map_coords::CellOf(glm::vec2(x, z));
	return {cell.x, cell.y, ecs::map_coords::InBounds(cell)};
}

float Distance(float x1, float z1, float x2, float z2)
{
	return gutils::GetDistanceInMetres(glm::vec2(x1, z1), glm::vec2(x2, z2));
}

int32_t AngleBetween(float x1, float z1, float x2, float z2)
{
	return gutils::GetAngleFromXZ(glm::vec2(x1, z1), glm::vec2(x2, z2));
}

float AngleToRadians(int32_t angle)
{
	return gutils::ConvertGameAngleTo3D(angle);
}

int32_t RadiansToAngle(float radians)
{
	return static_cast<int32_t>(gutils::ConvertAngle3DToGame(radians));
}

std::pair<float, float> PointAtAngle(float x, float z, int32_t angle, float metres)
{
	const auto game = static_cast<uint16_t>(angle & gutils::k_GameAngleMask);
	return {x + gutils::GetXFromAngle(game, metres), z + gutils::GetZFromAngle(game, metres)};
}

namespace
{
std::optional<entt::id_type> MeshResource(std::string_view mesh)
{
	for (const auto& [name, index] : Enumeration("meshes"))
	{
		if (mesh == name || (mesh.starts_with('#') && mesh.substr(1) == std::to_string(index)))
		{
			return resources::HashIdentifier(static_cast<MeshId>(index));
		}
	}
	return std::nullopt;
}
} // namespace

std::optional<float> MeshRadius(std::string_view mesh, float scale)
{
	const auto id = MeshResource(mesh);
	if (!id || !ecs::object::MeshHalfExtents(*id))
	{
		return std::nullopt;
	}
	return ecs::object::MeshRadius2D(*id, scale);
}

std::optional<float> MeshHeight(std::string_view mesh, float scale)
{
	const auto id = MeshResource(mesh);
	if (!id || !ecs::object::MeshHalfExtents(*id))
	{
		return std::nullopt;
	}
	return ecs::object::MeshHeight(*id, scale);
}

bool PlaySound(const Mod& mod, std::string_view bankName, std::string_view sampleName, const glm::vec3* position)
{
	std::optional<audio::SfxBank> type;
	for (size_t i = 1; i < static_cast<size_t>(audio::SfxBank::_COUNT); ++i)
	{
		const auto each = static_cast<audio::SfxBank>(i);
		if (bankName == SoundBankName(each))
		{
			type = each;
		}
	}
	if (!type)
	{
		return false;
	}
	const auto bank = audio::Bank(*type);
	if (bank == audio::k_NoBank)
	{
		return false;
	}
	int number = 0;
	if (const auto [end, error] = std::from_chars(sampleName.data(), sampleName.data() + sampleName.size(), number);
	    error != std::errc() || end != sampleName.data() + sampleName.size())
	{
		const auto sample = audio::FindSample(bank, sampleName);
		if (!sample)
		{
			return false;
		}
		number = sample->number;
	}
	if (number <= 0)
	{
		return false;
	}
	auto& owners = SoundOwners();
	auto owner = owners.find(mod.GetInfo().id);
	if (owner == owners.end())
	{
		owner = owners.emplace(mod.GetInfo().id, audio::NewOwner()).first;
	}
	// a one-shot effect as the original plays them: mode 3, no loop, +0x10 false (audio: the camera woosh 0x45899B,
	// the pile's 3D sound 0x66D26A)
	constexpr int k_Mode = 3;
	constexpr int k_Loops = 0;
	if (position == nullptr)
	{
		audio::PlaySoundEffect(owner->second, number, k_Mode, k_Loops, false, false, bank);
	}
	else
	{
		audio::PlaySoundEffectAt(owner->second, *position, glm::vec3(0.0f), number, false, k_Mode, k_Loops, false, true, bank);
	}
	return true;
}

void StopSounds(const Mod& mod)
{
	auto& owners = SoundOwners();
	if (const auto owner = owners.find(mod.GetInfo().id); owner != owners.end())
	{
		audio::StopOwner(owner->second);
		owners.erase(owner);
	}
}

} // namespace openblack::mods::api
