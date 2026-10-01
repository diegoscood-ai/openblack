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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// SpellStormAndTornado (0xF8 bytes, vtable 0x9847DC, save type 0x10; GMagicStormAndTornadoInfo::AllocSpell 0x5FBAB0):
// MAGIC_TYPE 16 STORM (wind and rain), 17 STORM_PU1 (with lightning) and 18 STORM_PU2 (the tornado). The three cast the
// same SF_LightningStormPush, whose rules read the power-up level (Rules/Storm.cpp), plus an SF_StormCast swirl at the
// hand. Wiki: docs/bw1-notes/miracles.md, "Tormenta".

namespace openblack::magic
{
/// SpellStormAndTornado +0xEC / +0xF0 (fn_0072DA10 zeroes both)
struct SpellStormData
{
	uint32_t castPsys {0};      ///< +0xEC the SF_StormCast effect (psys::manager id), stepped by the spell
	uint32_t waterReaction {0}; ///< +0xF0 REACTION_REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (34), made by fn_0072DCC0
};

/// SpellStormAndTornado::InitWithPos 0x72DAA0's clamp (0x72DAB2..0x72DADC): r = min(r, maxRadius), then max(r, minRadius)
[[nodiscard]] float ClampStormRadius(float minRadius, float maxRadius, float radius);
/// SpellStormAndTornado::CalculateCostToMaintain 0x72DB50: base x (magnitude / radiusForNormalCost)^2
[[nodiscard]] float StormCostToMaintain(float baseCost, float magnitude, float radiusForNormalCost);

namespace spell_storm
{
/// fn_0072DCC0 (from FireEffect's rain cooling fn_0072EFB0 0x72F2D9): the first storm spell of the list (0xDA07F8,
/// the newest first) with no water reaction whose Get2DRadius (its magnitude) reaches the burning object (distance in
/// x, z to the spell's +0x14) gets REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE by its player, stamped
void ReactToRainOnFire(const glm::vec3& objectPosition);
/// 0xDA07F8 / 0xDA07FC
[[nodiscard]] const std::vector<entt::entity>& Spells();
/// The SF_StormCast effect of a storm spell (0 none)
[[nodiscard]] uint32_t CastPSysOf(entt::entity spell);
/// A land is loaded
void Clear();
} // namespace spell_storm
} // namespace openblack::magic
