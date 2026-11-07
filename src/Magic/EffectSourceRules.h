/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Enums.h"
#include "MagicWorldInterface.h"

/// Who applied an effect, taken from where it came from when the effect's own values say nothing of it
namespace openblack::magic
{
/// Whether the values name nobody who applied them: no thing, no creature, no player and no one to blame
[[nodiscard]] bool NamesNoApplier(const EffectValues& values);

/// What the source says of who applied the effect, as the values would say it. A thing that struck answers for itself
/// with its own player (struckPlayer), and the source's player is the one blamed; with nothing struck, the creature
/// that cast it, else the source's player itself. A playerless source names no player at all. struck says whether the
/// source's applier is a thing still there, and struckIsCreature whether that thing is a creature
[[nodiscard]] EffectValues ApplierFrom(EffectValues values, const EffectSource& source, bool struck, bool struckIsCreature,
                                       std::optional<PlayerNames> struckPlayer);
} // namespace openblack::magic
