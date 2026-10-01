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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// SpellIcon (a MultiMapFixed, 0x110 bytes; ctor 0x725FF0): the floating hand icon (GSpellIconInfo.meshId 203
/// BuildingVillageCentreSpellHand) with the seed's SpellSeedGraphic above it. Both kinds below have it.
struct SpellIcon
{
	uint8_t infoIndex {0};                        ///< +0x28 GSpellIconInfo: 0 "Spell Icon" (site), 1 "TownSpell Icon"
	SpellSeedType seedType {SpellSeedType::None}; ///< +0x80 GSpellSeedInfo
	PlayerNames player {PlayerNames::NEUTRAL};    ///< GameThing::GetPlayer (a site icon asks its site)
	entt::entity graphic {entt::null};            ///< +0x7C SpellSeedGraphic (Create3DSpellObject 0x726210)
	entt::entity chargeRing {entt::null};         ///< +0x88 TChargingData: mesh 561 MSH_S_PULSE_IN
};

/// WorshipSpellIcon (0x140 bytes; Create 0x77F2B0 -> ctor 0x77F140, SetToZero 0x77F1F0)
struct WorshipSpellIcon
{
	entt::entity site {entt::null};    ///< +0x118
	int16_t removeTimer {0};           ///< +0x114 (fn_0077FF10 sets 1000; no caller: vestigial)
	float savedScale {0.0f};           ///< +0x11C
	bool charging {false};             ///< +0x120
	int powerUp {-1};                  ///< +0x124 the power-up level being charged (-1 = the base)
	PlayerNames chargingFor {PlayerNames::NEUTRAL}; ///< +0x128 the GInterfaceStatus (the hand) that charges it
	bool hasChargingInterface {false}; ///< +0x128 != NULL
	std::vector<entt::entity> seeds;   ///< +0x12C / +0x130 the SpellSeeds made from it (fn_0077F780: at the head)
	float chantStore {0.0f};           ///< +0x134 the charge
	uint32_t chargeStartTurn {0};      ///< +0x138
	int16_t slot {0};                  ///< +0x13C the icon slot 10..15 (placement index = slot - 10)
};

/// TownCentreSpellIcon (a TownSpellIcon, 0x128 bytes; fn_00748CB0 -> fn_00748BF0 / fn_00748A70)
struct TownCentreSpellIcon
{
	entt::entity town {entt::null};       ///< TownSpellIcon +0x114
	entt::entity townCentre {entt::null}; ///< +0x118
	uint8_t slot {0};                     ///< the town centre's icons[6] index (TownCentre +0xD0)
	std::array<bool, 3> powerUps {};      ///< +0x11C int[3]: the power-up levels the town holds (SetPULevel 0x748EB0)
};

/// TownCentre +0xD0 `TownCentreSpellIcon* icons[6]` (on the town centre's Abode entity)
struct TownCentreIcons
{
	std::array<entt::entity, 6> icons {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
};

/// SpellSeedGraphic (SpellIcon.cpp; Create 0x726F60, list 0xD9D3D0): the seed's mesh floating above an icon or inside
/// a one-shot orb, with the seed's holder effect (GSpellSeedInfo.holderParticle) and its power-up band
struct SpellSeedGraphic
{
	SpellSeedType seedType {SpellSeedType::None};
	PlayerNames player {PlayerNames::NEUTRAL}; ///< GameThing::GetPlayer (vt 0x1C); the mesh (Game3DObject) is +0x2C
	float scale {1.0f};                        ///< +0x54
	/// +0x58: the power-up band's size (DrawSpellGraphic 0x51A70C: band scale = 0.2 x +0x58 x +0x54); 1 (fn_00726F10),
	/// 0.5 on a worship icon (UpdateGraphicsWithPULevels 0x77F320). Not an alpha: nothing else reads it.
	float bandScale {1.0f};
	bool autoUpdate {true};                    ///< +0x5C: the holder PSys is stepped by the list every turn
	int powerUp {-1};                          ///< +0x60 (SetPowerUpType 0x727060; the band when != -1)
	uint32_t psys {0};                         ///< +0x50 the holder effect (psys::manager)
	entt::entity band {entt::null};            ///< +0x30 the power-up band (CreatePUBand 0x727080)
	std::vector<entt::entity> extraBands;      ///< the band drawn again for PU 1, 2 (the loop 0x51A3D4 draws pu + 1)
	glm::vec3 point {0.0f};                    ///< +0x64 the point given (fn_007270E0): the bands' centre
	glm::vec3 meshPosition {0.0f};             ///< +0x14 (MapCoords) = point + unknown0x150 x scale: the mesh
	glm::vec3 effectPosition {0.0f};           ///< point + unknown0x154 x scale: the holder effect
	float spin {0.0f};                         ///< +0x3C the mesh's y angle (+2 rad/s, DrawSpellGraphic 0x519B20)
	/// +0x34 the creature spell phials' frame, 0..32 at -15 a second (0x519B89, frame_anim::SpellIconFrame); 0 from
	/// fn_00726F10 (0x726F4B)
	float uvPhase {0.0f};
	float bandSpin {0.0f};                     ///< +0x44 the bands' angle (+10.3 rad/s [0xBE8E94], 0x51A2EA)
	float bandSpin2 {0.0f};                    ///< +0x40 (+1 rad/s [0xBE8E90], 0x51A305; no reader found)
};

} // namespace openblack::ecs::components
