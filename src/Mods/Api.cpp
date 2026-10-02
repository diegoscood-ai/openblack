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

#include <fmt/format.h>
#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "3D/DayNightClock.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
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
#include "Switches.h"

namespace openblack::mods::api
{
namespace
{
std::vector<Interface>& InterfaceList()
{
	static std::vector<Interface> list;
	return list;
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
	return {"meshes", "magic", "object_tables", "object_fields", "switches"};
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

} // namespace openblack::mods::api
