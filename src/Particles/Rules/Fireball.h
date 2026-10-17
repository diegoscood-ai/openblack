/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The fireball's rules (Rules/Fireball.cpp): what the game reads of a ball's atom once the effects have stepped.
// Wiki: docs/wiki/magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire.

namespace openblack::psys::fireball_rules
{
/// Once the spells have stepped their effects: each ball gets where the hand sees it and its radius from its atom as the
/// step's post-update left it (the drawn position while the atom is drawn and not faint, the rule scale times the base
/// scale). A ball whose atom is gone is skipped
void UpdateHandTargets();
} // namespace openblack::psys::fireball_rules
