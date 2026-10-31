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

#include <optional>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

/// The animals' clips (docs/wiki/beings/animation.md#which-clip-plays, docs/wiki/beings/animal-life.md#clips-per-species): an
/// animal's clip always comes from the species' function of the state (the animal state table, no info.dat fallback); -1 keeps
/// the clip playing. There are no into / out-of clips for animals.

/// The ANM_ index for the animal's top state, -1 = keep the current clip
[[nodiscard]] int32_t AnimalAnimId(entt::entity entity);
/// Cut to that clip (from 0 with `reset`); the same clip is not restarted
void SetAnimalAnim(entt::entity entity, int32_t clip, bool reset);
/// The state's clip, on every top state change
void SetAnimalStateAnim(entt::entity entity);

/// A new animal's first clip, as it is made (ecs::living::AnimalFirstClip): the species' stand clip from the start, or
/// a bird's move clip, with that clip's draw, keeping its time
void SetFirstAnimalClip(entt::entity entity);
/// The speed by which an animal's or a bird's clip plays this frame, as the original draws it
/// (docs/wiki/beings/animation.md#the-clips-advance-by-the-ground): while it moved during the last turn, its whole
/// speed (map units a turn), by which a clip with a stride plays the ground it covers (ecs::villager_clips::FramePlay),
/// whether or not that speed is what it moved; none while it did not move, and the clip plays by the frame's
/// milliseconds
[[nodiscard]] constexpr std::optional<uint16_t> AnimalGroundSpeed(bool movedLastTurn, uint16_t speed)
{
	return movedLastTurn ? std::optional<uint16_t>(speed) : std::nullopt;
}
/// Every frame: new animals get their clip, and every animal and bird the speed its clip plays by (AnimalGroundSpeed)
void UpdateAnimalAnimations();

} // namespace openblack::ecs
