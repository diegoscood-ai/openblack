/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// A player's heartbeat: how fast their god's heart beats, quickening with the danger their people are in. The beat's
/// pulse makes the temple's column of light throb. The beat itself is stepped by the guidance (audio::guidance), whose
/// heart beat this describes.
namespace openblack::ecs::heartbeat
{

/// The rate the beat starts at, with no danger
inline constexpr float k_CalmRate = 30.0f;
/// Beats a second for each unit of rate
inline constexpr float k_BeatsPerRate = 0.025f;

struct State
{
	/// The danger last measured, 0 to 1
	float stress {0.0f};
	/// How fast it beats, against k_BeatsPerRate
	float rate {k_CalmRate};
	/// How far through a beat it is, 0 to 1
	float phase {0.0f};
	/// The beat's swell this turn and the last: 0 between beats, 1 at the top
	float pulse {0.0f};
	float previousPulse {0.0f};
	/// The smoothed share of the world's people who believe in the player, and of all influence over the player's
	float believerShare {0.0f};
	float influenceShare {0.0f};
	/// What the temple's destruction holds the beat at, none (0) when it beats as the danger has it
	float destructionBeats {0.0f};
};

} // namespace openblack::ecs::heartbeat
