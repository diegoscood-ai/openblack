/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The fire's crackle: only the 2 burning objects nearest the camera play it (the slot array 0xDA09CC[2] of {fire,
// distance}, 0xDA09C8 the farthest slot's distance), a looped G_Fire sample at the object.

namespace openblack::ecs::fire
{
struct FireEffect;

namespace sound
{
/// ProcessList 0x730760, first: the slots' camera distances again
void RefreshDistances();
/// fn_0072EFB0's end: a fire with a fraction above 0.1 takes the first free or farthest slot when it is nearer than
/// that one (or no slot is taken); otherwise it gives its slot back
void Consider(FireEffect& fire, bool loud);
/// ProcessList's end: each slot's fire plays (fn_0072EDE0, looped, bank 2)
void StartSlots();
/// ToBeDeleted: the fire's slot is freed (fn_0072EDC0 stops its sound)
void Free(FireEffect& fire);
void Clear();
} // namespace sound
} // namespace openblack::ecs::fire
