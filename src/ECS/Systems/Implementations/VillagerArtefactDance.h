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

#include "Enums.h"
#include "VillagerDance.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager dancing round an artefact: waiting near it for its dance to start (WAIT_FOR_ARTIFACT_DANCE, 253), then
/// dancing (ARTIFACT_DANCE, 230), as the artefact's town sets it (ecs::systems::ArtefactSystem)
namespace openblack::ecs::villager_artefact_dance
{

/// Waiting for the dance: from farther than 12 m it walks to a clear spot nearer, on its own side of the artefact; there,
/// on its first turn it makes itself room if it has none, and once it has waited its time the dance starts. With no
/// dance, it decides what to do. Always 1
uint32_t WaitForArtefactDance(components::LivingAction& action);
/// Going into the wait: its turns of waiting start from 0; always accepted (1)
uint32_t EnterWaitForArtefactDance(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// Dancing round the artefact: one standing on the dance's own spot walks straight to its place; otherwise it dances
/// as the scripts' dance does (villager_dance::PerformDance), coming back to this state. Always 1
uint32_t ArtefactDance(components::LivingAction& action, const villager_dance::WalkStep& walk);
/// Leaving either: into a state with another exit, other than the scripts' dance, it leaves its dance. It may always go
/// (1)
uint32_t ExitDance(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_artefact_dance
