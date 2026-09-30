/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/


#pragma once

// The Villager handler of ECS/Effects/Reactions: ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for a villager of a
// cell, by reaction type (the ported ones: REACT_TO_FIRE in VillagerFire.cpp, REACT_TO_TELEPORT in
// VillagerTeleport.cpp; the rest reach no villager yet).

namespace openblack::ecs::villager_reactions
{
/// SetLivingReactionHandler(Villager, ...): at every map load (villager_fire::Clear, villager_teleport::Clear)
void Register();
} // namespace openblack::ecs::villager_reactions
