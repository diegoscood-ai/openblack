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

#include <entt/entity/entity.hpp>

#include "ECS/Components/Spell.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// SpellSeed (Object, 0xA0 bytes, vtable 0x981FC0): the miracle in the hand. Ctors 0x727FF0 (from a worship icon) and
/// 0x7280A0 (from a seed info), common init fn_00728140. The entity also has a Transform and the seed's mesh
/// (GSpellSeedInfo.mesh, scale = info.scale).
struct SpellSeed
{
	/// +0x54: bit 0 cleared on cast / set in hand; bit 1 = don't keep it when it comes into the hand
	uint8_t flags {0};
	SpellSeedType seedType {SpellSeedType::None}; ///< +0x58 GSpellSeedInfo*
	entt::entity icon {entt::null};               ///< +0x5C the WorshipSpellIcon that charged it (M7)
	entt::entity spell {entt::null};              ///< +0x60 the spell cast from it
	bool inInterface {false};                     ///< +0x64 GInterfaceStatus (the local player's hand)
	SpellCreator creator {};                      ///< +0x68 AllocSpell's creator: the icon, or the interface's player
	int powerUp {-1};                             ///< +0x6C POWER_UP_TYPE (-1 = the base)
	bool hasCast {false};                         ///< +0x70
	bool fromOneShot {false};                     ///< +0x72 made by OneOffSpellSeed::CreateSpellIntoHand
	float chantStore {0.0f};                      ///< +0x74 the charge (SetChantStore 0x729A50)
	float chantStoreCopy {0.0f};                  ///< +0x78
	float storedChants {0.0f};                    ///< +0x7C the last spell's chants (-1 = none)
	int storedMaxObjects {-1};                    ///< +0x80
	float storedAge {0.0f};                       ///< +0x84
	float castMultiplier {1.0f};                  ///< +0x88 scales initialChants and the duration
	float psysPower {1.0f};                       ///< +0x8C multiplies the spell's strength
	bool ready {false};                           ///< +0x90 active: held long enough (or at once)
	int turnsInHand {0};                          ///< +0x94
	MagicType lastMagic {MagicType::None};        ///< +0x98 the magic of the last cast
};

} // namespace openblack::ecs::components
