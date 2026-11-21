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

#include <glm/vec3.hpp>

namespace openblack
{

/// The hand's move from one of its states to another, such as from hovering to gripping the land.
///
/// The game fades the hand from where it was last drawn to where its new state puts it, at an even pace over 0.13
/// seconds. The place faded from stays put while the new one moves on. The fade is moved on after the state's update;
/// the frame it reaches 0.13 seconds it is over and draws nothing faded.
class HandCrossFade
{
public:
	static constexpr float k_Seconds = 0.13f;

	/// Starts a fade from where the hand was last drawn
	void Start(glm::vec3 from)
	{
		_from = from;
		_elapsed = 0.0f;
	}

	/// Moves the fade on by a frame
	void Update(float deltaSeconds)
	{
		if (!_from.has_value())
		{
			return;
		}
		_elapsed += deltaSeconds;
		if (_elapsed >= k_Seconds)
		{
			_from.reset();
		}
	}

	/// How far through the fade, 0 to 1, or none when not fading
	[[nodiscard]] std::optional<float> Fraction() const
	{
		return _from.has_value() ? std::optional(_elapsed / k_Seconds) : std::nullopt;
	}

	/// Where the fade started from, none when not fading
	[[nodiscard]] std::optional<glm::vec3> From() const { return _from; }

	/// Where the hand is drawn, given where its state puts it: from + (to - from) t, the same lerp as the drawn matrices
	[[nodiscard]] glm::vec3 Apply(glm::vec3 to) const
	{
		if (!_from.has_value())
		{
			return to;
		}
		const float t = _elapsed / k_Seconds;
		return (to - *_from) * t + *_from;
	}

	[[nodiscard]] bool IsActive() const { return _from.has_value(); }

private:
	std::optional<glm::vec3> _from;
	float _elapsed {0.0f};
};

} // namespace openblack
