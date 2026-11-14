/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/entity/entity.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack
{
struct GAbodeInfo;
}

namespace openblack::ecs::archetypes
{
class AbodeArchetype
{
public:
	/// A storage pit's piles that are there already (a loaded game's own): its five wood piles in order, then its food
	/// pile
	struct StoragePitPiles
	{
		std::array<entt::entity, 5> wood {entt::null, entt::null, entt::null, entt::null, entt::null};
		entt::entity food {entt::null};
	};

	/// A field's row makes a field instead (FieldArchetype::Create, a wheat field with a fence of the town's tribe),
	/// unless it is a loaded one (ownParts false), whose load makes its field from its record
	/// underConstruction: the abode is a plan being built: not counted by TownStats, built 0%, and none of the
	/// MakeFunctional parts (storage pit, creche, graveyard, the town centre's totem and icons) until abodes::Built. The
	/// script's abodes are whole (false). nearestTownWhenNone: an abode given no town joins the nearest one, as the land
	/// script's do; a puzzle totem (false) stays of no town
	/// piles: a storage pit takes them rather than making its own and filling them with the amounts
	/// ownParts: a whole town centre is made functional (its totem, its icons, its town's centre and its belief
	/// symbols), and a workshop makes its sign and its wood pile and, whole, joins its town's list; a loaded one (false)
	/// does none of those, as its load makes each of them from its own record
	static entt::entity Create(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians, float scale,
	                           uint32_t foodAmount, uint32_t woodAmount, bool underConstruction = false,
	                           bool nearestTownWhenNone = true, const StoragePitPiles* piles = nullptr, bool ownParts = true);
	/// The abode of a town already found (or of no town, Abode::k_NoTown) made as an abode of its row's class, a field's
	/// row included: what Create does for every row but a field's, and the abode part of a field (FieldArchetype)
	static entt::entity CreatePlain(uint32_t townId, const glm::vec3& position, AbodeInfo type, float yAngleRadians,
	                                float scale, uint32_t foodAmount, uint32_t woodAmount, bool underConstruction,
	                                const StoragePitPiles* piles = nullptr, bool ownParts = true);
	/// The town centre made functional: its totem (the plinth and its hand, only when the centre has none yet) and the
	/// spell icons (worship::town_centre::MakeFunctional)
	static void MakeTownCentreFunctional(entt::entity townCentre, const GAbodeInfo& info, float yAngleRadians, float scale);
	/// Whether a building of the type and tribe drapes over the land: the graveyard, the storage pit, the workshop, the
	/// citadel, the creche, the football pitch, the town centre and the field, and the Celtic, Japanese, Indian, Norse
	/// and Tibetan wonders
	[[nodiscard]] static bool MorphsWithTerrain(AbodeType type, Tribe tribe);
	/// The height an abode that does not drape over the land is set at: `ground` (the land under its origin) plus the
	/// lowest of the land under the four corners of its mesh box relative to it, never above the origin's, and no
	/// deeper than max(0.2 x radius2D, 0.8) (radius2D: its scale x the larger half-extent of its box in x or z)
	[[nodiscard]] static float SunkFoundationHeight(float ground, const std::array<float, 4>& cornerGrounds, float radius2D);
	AbodeArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
