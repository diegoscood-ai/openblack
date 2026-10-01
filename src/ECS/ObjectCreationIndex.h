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

#include <string>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{
namespace components
{
/// Object +0x3C: the value of the creation counter when the object was made (Object::Object 0x636520)
struct ObjectCreationIndex
{
	uint32_t value;
};
} // namespace components

/// The original's object creation counter (GData +0x18 = g_game+0x205A48; research dev\tmp_dis\anim\creation_index.md):
/// every Object-derived thing takes the next number when it is made (abodes, trees, features, rocks, villagers,
/// animals, pots, the creature, lanterns, and side objects openblack doesn't make: town desire flags, script highlights,
/// spell icons, citadel parts...). Towns, forests, mists, footpaths, streams, planned buildings and the hand don't
/// count. It only goes up; ClearMap (GData::Reset 0x510750) sets it to 0 when a land is loaded, and on a fresh boot the
/// first land starts at 2 (HelpSystem::Create made two HelpSpirits before it). Villager::SetSpeed reads it.
namespace object_index
{
/// A land is loaded: back to 0, or to 2 on the first land of the session
void OnLoadMap();
/// Gives the entity the next index
void Assign(entt::entity entity);
/// Takes numbers for objects openblack doesn't create
void Skip(uint32_t count);
/// The entity's index, or none (-1)
[[nodiscard]] int64_t Of(entt::entity entity);

/// openblack only (mods): while one of these is alive, Assign gives indices from a separate range (k_ModBase up) and
/// the original's counter does not move, so objects a mod adds (that the original never makes) don't shift the
/// creation order of the original's objects (Villager::SetSpeed, the animals' and the forest's orderings).
class ModScope
{
public:
	explicit ModScope(bool active = true);
	~ModScope();
	ModScope(const ModScope&) = delete;
	ModScope& operator=(const ModScope&) = delete;

private:
	bool _active;
};
/// The first index of the mods' range
constexpr uint32_t k_ModBase = 0x40000000u;
/// An entity made inside a ModScope
[[nodiscard]] bool IsModObject(entt::entity entity);

/// CREATE_NEW_TOWN_SPELL / the town centre's spell icons (TownCentreSpellIcons, at most 6): a town's distinct spell
/// seeds, and whether its centre exists; each new seed of a town with a centre makes an icon
void AddTownSpell(uint32_t town, const std::string& spell);
void OnTownCentre(uint32_t town);
} // namespace object_index

} // namespace openblack::ecs
