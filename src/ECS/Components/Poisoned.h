/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// A poisoned Living: bit 1 of Living +0xB4 (Living::IsPoisoned 0x416F90 / SetPoisoned 0x416FA0). The heal miracle
/// cures it (Spell::ApplyDefaultSpellEffect 0x720C30, event 5). Villager::Draw's helper fn_0051B3D0 draws a poisoned
/// villager with diffuse 0xFFE8FFDD and specular 0xFF001000 (Pot::GetPoisonColor / GetPoisonSpecular, not drawn yet);
/// who poisons villagers (eating poisoned food) is not ported yet.
struct Poisoned
{
};

} // namespace openblack::ecs::components
