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

// The audio's part of the end of a game turn. The sound map and the sound tags run at the end of every turn; then,
// past the fifth turn and with the game not paused, the audio's own turn (music, ambience banks, 3D channels and the
// ambience mix), else the ambience is silenced. A turn ends paused only when the pause was set during that turn (a key
// or an interface action taken in the turn's inputs, or a script that starts a film): while the game stays paused no
// turn runs, so nothing here runs and the ambience keeps sounding at its last volumes.
namespace openblack::audio::end_of_turn
{

inline constexpr uint32_t k_FirstAudioTurn = 6; ///< the audio's turn runs from turn 6 on (turn > 5, unsigned)

enum class Atmos : uint8_t
{
	Process, ///< the audio's game turn
	Silence, ///< the ambience stopped (atmos_banks::Silence)
};

struct Steps
{
	bool soundMap {true};
	bool soundTags {true};
	Atmos atmos {Atmos::Silence};
};

/// The steps of the end of a turn, paused or not, on that turn
[[nodiscard]] constexpr Steps For(bool paused, uint32_t turn) noexcept
{
	return Steps {
	    .soundMap = true,
	    .soundTags = true,
	    .atmos = !paused && turn >= k_FirstAudioTurn ? Atmos::Process : Atmos::Silence,
	};
}

} // namespace openblack::audio::end_of_turn
