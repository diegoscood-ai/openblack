/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>

namespace openblack::mods
{
class Mod;
}

/// The simplified functions mods use, one C++ core that the three languages translate: mod.json (data), Lua
/// (Lua/LuaHost.cpp, the `ob` table) and native libraries (Native/NativeHost.cpp, include/openblack/mod_api.h). A new
/// function goes here first, then gets a line in each of the two translations (docs/bw1-notes/mod-library.md,
/// "Referencia de la API").
///
/// Every function is safe to call at any time: before a land is loaded the world ones answer "nothing" (nullopt,
/// false). They only use the public API of each engine area (agreed with their owners): world height LandIsland,
/// spells magic::script::CastSpellAtPos (by the rules of the game, as SPELL_AT_POS), camera, clock, switches.
namespace openblack::mods::api
{

// ---- the mod itself

void Log(const Mod& mod, int level, std::string_view text); ///< level 0 info, 1 warning, 2 error
/// The current choice of one of its options ("" if none)
[[nodiscard]] std::string Option(const Mod& mod, std::string_view option);
/// An engine switch while the mod is active (Switches.h). False if there is no such switch
bool SetSwitch(Mod& mod, std::string_view name, double value);
/// The current value of an engine switch, nullopt if there is none
[[nodiscard]] std::optional<double> GetSwitch(std::string_view name);
/// Every engine switch: name, type ("bool", "int", "float"), when ("live", "map", "restart"), description
struct SwitchInfo
{
	std::string name;
	std::string type;
	std::string when;
	std::string description;
	double min {0.0};
	double max {0.0};
};
[[nodiscard]] std::vector<SwitchInfo> Switches();

// ---- interfaces between mods (library mods)

/// What a mod publishes for others: an opaque pointer (a C function table, a Lua table reference...) and the
/// language it is for
struct Interface
{
	std::string name;     ///< e.g. "foliage.v1"
	std::string provider; ///< mod id
	std::string language; ///< "native" or "lua"
	const void* table {nullptr};
	size_t size {0};
};
/// Publishes an interface (a second one with the same name and language replaces the first, with a warning)
void Provide(const Mod& mod, Interface interface);
[[nodiscard]] const Interface* FindInterface(std::string_view name, std::string_view language);
[[nodiscard]] const std::vector<Interface>& Interfaces();
/// Every interface of that mod goes (it stops)
void Withdraw(const Mod& mod);

// ---- enumerations

/// name -> value: "meshes" (k_MeshNames), "magic" (info.dat names of MagicType, after the data loads),
/// "object_tables" and "object_fields" (Replacements.h), "switches"
[[nodiscard]] std::vector<std::pair<std::string, int64_t>> Enumeration(std::string_view which);
[[nodiscard]] std::vector<std::string_view> Enumerations();

// ---- the game

/// Game turns since the game started (10 per second)
[[nodiscard]] uint32_t Turn();
/// The hour of the day on the land's clock (0-24)
[[nodiscard]] float Hour();
/// The landscape's height at x, z (world metres); nullopt when no land is loaded
[[nodiscard]] std::optional<float> GroundHeight(float x, float z);
[[nodiscard]] glm::vec3 CameraPosition();
[[nodiscard]] glm::vec3 CameraFocus();
void SetCamera(const glm::vec3& position, const glm::vec3& focus);
/// A miracle cast at a point by the neutral player, as the script's SPELL_AT_POS does (GScript::CastSpellAtPos
/// 0x70BD60, its checks included): `magic` is its info.dat name ("FIREBALL", "MAGIC_TYPE_FIREBALL"...) or number.
/// False if there is no such miracle, no land, or the miracle refused
bool CastMiracle(std::string_view magic, const glm::vec3& position, float radius, float seconds);

} // namespace openblack::mods::api
