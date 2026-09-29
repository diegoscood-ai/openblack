/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs
{

/// The animals' clips (docs/bw1-notes/animation.md): Animal::GetAnimId (0x417FA0) always asks the species' function of
/// the state (g_AnimalStateTable). openblack's animals have no states yet and stand still, so they play the species'
/// StandAnimation (e.g. Cow 0x41C7E0 = ANM_A_COW_STAND); goats and zebras return -1 there and keep the rest pose.
void UpdateAnimalAnimations();

} // namespace openblack::ecs
