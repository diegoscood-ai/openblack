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

/// A poisoned Living: bit 1 of Living +0xB4 (Living::IsPoisoned 0x416F90 / SetPoisoned 0x416FA0, vt 0x4A4 / 0x69C).
/// The heal miracle cures it (Spell::ApplyDefaultSpellEffect 0x720E34, event 5). Everything else about it is in
/// ECS/Life.h (ecs::life): who sets it (Villager::AddResource 0x7564F3 / GetResourceFrom 0x7533FC, the poisoned food
/// that reaches a villager), the life it costs on every periodic check (Villager::CheckHungry 0x75BD92) and the tint
/// the drawing needs, k_PoisonDiffuse / k_PoisonSpecular (fn_0051B3D0 0x51B43D, not drawn yet).
struct Poisoned
{
};

} // namespace openblack::ecs::components
