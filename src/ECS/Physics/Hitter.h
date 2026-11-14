/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <memory>
#include <vector>

#include "ECS/PhysicsEntry.h"

/// What hit each body of the physics list, kept as the original keeps it: the hitter is a place in the list, not a
/// body, so it names whatever entry holds that place when it is read. It is looked up only at the end of a turn whose
/// force counts, and stays as it was otherwise, from turn to turn.
namespace openblack::ecs::physics::hitter
{
using Entries = std::vector<std::unique_ptr<PhysicsEntry>>;

/// The turn start of a listed body: its summed force and the body it last touched are cleared. Its hitter stays.
void ClearTurnContacts(PhysicsEntry& po);

/// Before the end of a turn's walk: each body's last touched body as its place in the list. Every body touched in the
/// steps is listed (the steps touch listed bodies only, and an entry leaving the list is forgotten by the others).
void CaptureLastHits(Entries& objects);

/// The impact of the turn's summed force, written only when it counts (turn::CountedImpact): a force too small to count
/// leaves the impact as it was. Returns whether it counts.
bool TakeImpact(PhysicsEntry& po);

/// The end of a turn's lookup of what hit the body at place i, made only when its force counts (turn::LookUpHitter).
/// listGrewInThisLoop: the list grew since CaptureLastHits. The body it touched names a place: whatever entry holds it
/// now is the hitter, and gives it its credit for the throw when it has none of its own.
void FindHitter(Entries& objects, size_t i, bool listGrewInThisLoop);

/// The entry the body's hitter names now, nullptr for none. A hitter's place at or past the end of the list stops the
/// game with a message: the original never reads one there.
[[nodiscard]] PhysicsEntry* HitterEntry(const Entries& objects, const PhysicsEntry& po);
} // namespace openblack::ecs::physics::hitter
