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

#include <optional>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// Resources put down on the land: Pot::AddResourceToPos 0x66F270 and the pile helpers it uses. Shared by the hand (a
// hand pot let go of) and the miracles (SpellResource's grains). Wiki: docs/bw1-notes/magic.md, "Comida y madera".

namespace openblack::ecs::pot_resource
{

/// The GInterfaceStatus* argument: the interface that puts the resource down (NULL for the neutral / script player)
struct Dropper
{
	bool hasInterface {false};                  ///< IS != NULL
	PlayerNames player {PlayerNames::PLAYER_ONE}; ///< IS->GetPlayer()
	bool isMyInterface {false};                 ///< IS == g_game.MyInterfaceStatus()
};

/// Pot::AddResourceToPos 0x66F270 (pos, IS, type, amount, poisoned, speedUp): out of bounds nothing. The 3x3 cells
/// around pos (GUtils::Spiral) are searched, fixed objects then mobile ones, and every same-resource store or pot whose
/// fire centre is within Get2DRadius x GetRadiusMultiplierForApplyingPotToPos of pos takes what it accepts. What is
/// left, on land, makes a new MagicFood / MagicWood pile (fn_005FA8B0) with the pile sound, SetPoisoned(poisoned) and
/// SetSpeedUp(speedUp). Returns amount - left, what went into the stores and pots already there (a new pile's part
/// is not counted, as in the original).
uint32_t AddResourceToPos(const glm::vec3& position, const Dropper& dropper, ResourceType type, uint32_t amount,
                          bool poisoned, bool speedUp);

/// fn_0066D1A0: the pile sound at pos, by type and amount (food < 200: G_PileFoodSmall_01..06 (77 + t % 6), else
/// G_PileFood_01/02 (75 + (t & 1)); wood < 200: G_PileWoodSmall_01..06 (92 + t % 6), else G_PileWood_01..06 (86 + t % 6))
void PlayPileSound(const glm::vec3& position, ResourceType type, uint32_t amount);
/// The InGame.sad sample fn_0066D1A0 picks for a random value t
[[nodiscard]] int PileSoundSample(ResourceType type, uint32_t amount, uint32_t t);

/// PileFood::GetProportionRaised 0x66EB60: p = amount / maxInPot, 0 when not positive, else 0.05 + 0.95 min(p, 1);
/// then 1 - (1 - p)^2, clamped to 0..1
[[nodiscard]] float PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot);

/// Object::Get2DRadius 0x638180 (scale x the larger half extent of the mesh), x GetProportionRaised for a PileFood
/// (0x66F180); PileWood, pots and stores use the Object one
[[nodiscard]] float Get2DRadius(entt::entity object);

/// GetRadiusMultiplierForApplyingPotToPos: Pot 0x66F520 = 2, Object 0x63AAD0 / WorshipSite 0x77E480 = 1.2
[[nodiscard]] float RadiusMultiplierForApplyingPotToPos(entt::entity object);

/// MapCoords::IsWater 0x6035B0 (the cell's water bit; out of the map or without a land block 1) and IsDryLand 0x603620
/// (the cell's altitude byte >= 4; out of the map or without a block 0)
[[nodiscard]] bool IsWater(const glm::vec3& position);
[[nodiscard]] bool IsDryLand(const glm::vec3& position);

/// PileFood::SetSpeedUp 0x66E220: the flag, and on switching on the PILEFOOD_SPEEDUP spot visual (46, scale 1, for
/// ever, on the pile); off closes it
void SetSpeedUp(entt::entity pile, bool on);

} // namespace openblack::ecs::pot_resource
