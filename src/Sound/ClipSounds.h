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

#include <span>
#include <string>
#include <vector>

#include <entt/core/hashed_string.hpp>

/// The sounds placed on the frames of the people's, animals' and birds' clips (Data/SmallSounds.SAS)
namespace openblack::audio::clip_sounds
{

/// A sound placed at a time of a clip
struct Event
{
	int32_t time;
	int32_t soundId;
	int32_t action; ///< 0 play, 1 stop (only M_P_Saw_Wood: forward saw off, back saw on)
};

/// The sounds the file places under a clip's name
struct Clip
{
	std::string name;
	int32_t group {0};
	std::vector<Event> events;
};

/// The file's clips, in its order
struct ClipSoundTable
{
	std::vector<Clip> clips;
};

/// The table's id in the resources
inline constexpr entt::hashed_string k_TableId = entt::hashed_string("audio/clip_sounds");

/// The file's bytes: "hasGroup", then "name [group]" and "time soundId action more" lines until more is 0, until END
[[nodiscard]] ClipSoundTable Parse(std::span<const uint8_t> bytes);

} // namespace openblack::audio::clip_sounds
