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

#include <functional>
#include <optional>

#include "Enums.h"

/// The clip a new villager or animal starts with. It is chosen as the living is made, right after it is put in the
/// map's lists and before anything else is done with it, from the state it is made in. A villager is made just created
/// (drowning when made on water) and an animal deciding what to do; a later change of state chooses its own clip. Pure,
/// tested with a fake random generator.
namespace openblack::ecs::living
{

/// Draws a whole number below a limit on the game's shared stream
using IntRandom = std::function<uint32_t(uint32_t)>;

/// The first clip and how it is put on
struct FirstClip
{
	int32_t clip;
	/// Whether the clip starts from its beginning: the birds' own clip change never restarts it
	bool fromStart;
};

/// Whether the animal is one of the birds, whose clip rules are their own: a bird's clip while it decides what to do is
/// its move clip, and changing its clip never restarts it
[[nodiscard]] bool IsBirdForClips(AnimalInfo type) noexcept;

/// A bird's move clip, which is also its clip while it decides what to do: the dove, the crow, the pigeon and the
/// seagull toss a coin, the swallow draws one of three, the spell dove, the bats and the vulture have one each (the
/// vulture's is none, -1). Not a bird: -1, with no draw
[[nodiscard]] int32_t BirdMoveClip(AnimalInfo type, const IntRandom& random);

/// A new villager's first clip: the table clip of the state it is made in, from the start. No draw. A negative clip
/// (none) sets nothing
[[nodiscard]] std::optional<FirstClip> VillagerFirstClip(int32_t bornStateTableClip) noexcept;

/// A new animal's first clip, as it is made deciding what to do: a bird takes its move clip (with that clip's draw) and
/// keeps the clip's time; any other animal takes its species' stand clip from the start, with no draw. A negative clip
/// (none) sets nothing
[[nodiscard]] std::optional<FirstClip> AnimalFirstClip(AnimalInfo type, int32_t standClip, const IntRandom& random);

} // namespace openblack::ecs::living
