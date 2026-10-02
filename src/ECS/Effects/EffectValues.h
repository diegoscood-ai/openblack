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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack
{
struct GEffectInfo;
} // namespace openblack

// The generic effect system the miracles hurt, heal and sway alignment with (Effect.cpp 0x524EF0..0x525910 and
// Object::ApplyEffect 0x637980). Wiki: docs/bw1-notes/magic.md.

namespace openblack::ecs::effects
{

/// EffectValues (0x40 bytes)
struct EffectValues
{
	enum Number : size_t
	{
		Burn,
		Crush,
		Hit,
		Heal,
		FlyAway,
		Alignment,
		Belief,

		_COUNT
	};
	/// +0x08 EffectNumbers: the GEffectInfo amounts
	std::array<float, _COUNT> numbers {};
	float radius {0.0f}; ///< +0x24 metres
	/// +0x28 the GameThing that applies it (a spell's creator); its player is the EffectValues' player
	entt::entity appliedBy {entt::null};
	bool appliedByCreature {false};
	/// +0x3C / GetPlayer 0x5254C0 (the applier's player)
	bool hasPlayer {false};
	PlayerNames player {PlayerNames::NEUTRAL};

	/// fn_005250A0 -> fn_005250D0: the 7 numbers and the radius of a GEffectInfo (GMagicEffectInfo's base)
	static EffectValues FromEffectInfo(const GEffectInfo& info);

	/// operator*= 0x525720 via fn_00525670 (skipped for 1): the 7 numbers, not the radius
	void Scale(float factor);

	/// EffectNumbers::IsDestructive 0x5258C0: burn, crush, hit or fly away above 0
	[[nodiscard]] bool IsDestructive() const;

	/// EffectValues::ApplyEffectToMapPos 0x525100: every available effect receiver of the 10 m map cells over pos +- R
	/// whose fire centre is within R + its fire radius and whose altitude is within its height + R gets ApplyEffect.
	/// No falloff. pos: x, z metres, y the altitude above the land. Returns the last object hit (entt::null: none).
	entt::entity ApplyEffectToMapPos(const glm::vec3& position);
};

/// Object::IsEffectReceiver (vt 0x774): 1 for Object (0x4029E0); Villager 0x751D70 refuses a heal when dead
[[nodiscard]] bool IsEffectReceiver(entt::entity object, const EffectValues& values);

/// Object::ApplyEffect 0x637980: crush and hit (x the info's defence multipliers) reduce the life, heal increases it,
/// a kill is DestroyedByEffect, a crush creates REACT_TO_OBJECT_CRUSHED, and the caster's alignment moves
/// (GAlignment::Update). Returns the original's "effectiveness": (1 - life0) / heal + life0 / damage.
float ApplyEffect(entt::entity object, EffectValues& values);

/// FireEffect::ConvertTemperatureToDamage 0x72EEC0: 0 below the combustion temperature Tc, else
/// (T - Tc) / Tc x defenceMultiplierBurn x 0.1
[[nodiscard]] float ConvertTemperatureToDamage(entt::entity object, float temperature);

/// Legacy (TownQueries is its last user): use ecs::map_cells, the ordered lists of the original.
/// The fixed list (+4) of one 10 m map cell. (aproximado) openblack's grid (ECS/MapProduction) puts a fixed object only
/// in the cells whose centre is within its bounding radius + 1 m, so a small tree away from a cell centre is in no cell
/// at all, while the original links every object into the cell of its position: here the list is the grid's plus every
/// fixed object whose position is in the cell, sorted by entity (the grid's sets have no order). Out of the grid: empty.
[[nodiscard]] std::vector<entt::entity> FixedObjectsInMapCell(int cellX, int cellZ);
/// Legacy (TownQueries is its last user): use ecs::map_cells::ObjectsInCell.
/// fn_00603500 / fn_007252D0 over one map cell: its fixed list, then its mobile list (+0, sorted by entity)
[[nodiscard]] std::vector<entt::entity> ObjectsInMapCell(int cellX, int cellZ);

} // namespace openblack::ecs::effects
