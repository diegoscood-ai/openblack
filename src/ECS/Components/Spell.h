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
#include <glm/vec3.hpp>

#include "Enums.h"
#include "PSys/SpellLink.h"

namespace openblack::ecs::components
{

/// Spell +0xA0 `creator`: the GameThing that pays for and positions a spell (GameThing vt 0x58 MaintainSpell, vt 0x5C
/// UpdateSpellInfo). The original keeps a pointer; here the kind says which class it is.
struct SpellCreator
{
	enum class Kind : uint8_t
	{
		None,             ///< creator gone (ProcessMaintainRequest clears it)
		Player,           ///< a GPlayer (0x64C430 / 0x64C470): only the neutral player refills
		WorshipSpellIcon, ///< 0x77F6F0: the worship site's battery (src/Worship, M7)
		Creature,         ///< 0x4F8350: the creature's energy (M8)
		Thing,            ///< any other GameThing (0x56FED0: it pays everything)
	};
	Kind kind {Kind::None};
	PlayerNames player {PlayerNames::NEUTRAL}; ///< the player it belongs to (vt 0x1C GetPlayer)
	entt::entity entity {entt::null};          ///< the icon / creature / thing (entt::null for a player)
};

/// The spell's class (the GMagicInfo class that allocated it, vt 0x34 AllocSpell): which SpellOps run it
enum class SpellClass : uint8_t
{
	General,         ///< Spell 0x5FB450: fireball, lightning bolt, beam explosion (the behaviour is the PSys)
	Heal,            ///< SpellHeal 0x5FBD40
	Teleport,        ///< SpellTeleport 0x5FBDF0
	Forest,          ///< SpellForest 0x5FAD90
	Resource,        ///< SpellResource 0x5FAC20 (food, wood)
	StormAndTornado, ///< SpellStormAndTornado 0x5FBAB0
	Shield,          ///< SpellShield 0x5FBA70
	Water,           ///< SpellWater 0x5FAC70
	FlockFlying,     ///< SpellFlockFlying 0x723100
	FlockGround,     ///< SpellFlockGround 0x723180
	Creature,        ///< SpellCreature 0x5FA790

	_COUNT
};

/// Spell (0xEC bytes, vtable 0x9805B0; ctor 0x71FB40, fields zeroed by fn_0071FCF0). Positions are MapCoords as
/// metres: x and z on the map, y the altitude above the land (MapCoords +8).
struct Spell
{
	MagicType magicType {MagicType::None}; ///< +0xB4
	SpellClass spellClass {SpellClass::General};
	glm::vec3 position {0.0f};               ///< +0x14 the cast position, then each applied event's (y 0)
	uint32_t reaction {0};                   ///< +0x28 the Reaction it made (ECS/Effects/Reactions.h id), 0 none
	glm::vec3 movementDirection {1.0f, 0.0f, 0.0f}; ///< +0x2C the last applied event's velocity
	float chants {0.0f};                     ///< +0x38 the prayer power left in the spell (GetLife)
	float initialChants {0.0f};              ///< +0x3C the chants at the last SetChants (0x720FA0)
	bool closedDown {false};                 ///< +0x40 CoreCloseDown 0x720160
	bool isMyInterfaceCasting {false};       ///< +0x44 fn_007201F0
	bool isCreatureCasting {false};          ///< +0x48 creator->IsCreature()
	bool isHumanPlayerCasting {false};       ///< +0x4C the creator's player has +0x8E0 == 1
	float duration {-1.0f};                  ///< +0x50 seconds; < 0 no limit
	float manaPathPerTurn {0.0f};            ///< +0x54 CreateSpellPoint accumulators
	float manaPathPerEvent {0.0f};           ///< +0x58
	bool free {false};                       ///< +0x5C PayFor returns 1 without paying (setter 0x720820, no caller)
	psys::ProcessInfo processInfo {};        ///< +0x64 PSysProcessInfo
	SpellCreator creator {};                 ///< +0xA0
	PlayerNames player {PlayerNames::NEUTRAL}; ///< +0xA4
	bool hasPlayer {false};                  ///< +0xA4 != NULL (a null creator leaves it unset)
	bool castFromInterface {false};          ///< +0xA8 GInterfaceStatus set by DoPostCastThings (a hand cast)
	entt::entity seed {entt::null};          ///< +0xAC the SpellSeed that cast it
	uint32_t psys {0};                       ///< +0xB0 PSysInterface (psys::manager id), 0 none
	float age {0.0f};                        ///< +0xB8 seconds
	/// +0xBC the radius: set by InitWithPos 0x71FE50 (castData.magnitude; 40, 0x8CF300, with no cast data)
	float magnitude {1.0f};
	glm::vec3 originalCastPos {0.0f};        ///< +0xC0
	glm::vec3 castPos {0.0f};                ///< +0xCC follows the hand for in-hand spells
	glm::vec3 direction {0.0f};              ///< +0xD8 = processInfo.direction at init
	float strengthMultiplier {1.0f};         ///< +0xE4
	int maxObjectsToCreate {-1};             ///< vt 0x54C / 0x550 (SpellWithObjects keeps it)
};

} // namespace openblack::ecs::components
