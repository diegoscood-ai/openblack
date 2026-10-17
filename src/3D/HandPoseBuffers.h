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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>

namespace openblack
{

/// The hand's two buffers of drawn bone matrices (world space): the one drawn, and the one a state blend fades from.
///
/// The game never copies the drawn pose when the hand changes: it swaps the two buffers, so the pose just drawn becomes
/// the one faded from, and the next pose is written over the older one. Two changes with no pose drawn between them
/// therefore fade from the pose kept at the change before, not from the last one drawn. Only hiding the hand copies the
/// drawn pose into the kept buffer first, so the hand shown again fades from where it was last drawn.
class HandPoseBuffers
{
public:
	/// A change of the hand's state: the drawn buffer becomes the one faded from, the other one is drawn into next
	void Swap() noexcept { std::swap(_drawn, _kept); }

	/// Hiding the hand: the kept buffer becomes a copy of the drawn one
	void KeepDrawn() { _kept = _drawn; }

	/// The buffer the frame's pose is written into, and drawn
	[[nodiscard]] std::vector<glm::mat4>& Drawn() noexcept { return _drawn; }
	[[nodiscard]] const std::vector<glm::mat4>& Drawn() const noexcept { return _drawn; }

	/// The buffer a blend fades from
	[[nodiscard]] const std::vector<glm::mat4>& Kept() const noexcept { return _kept; }

	/// While a blend runs, every float of the drawn matrices = kept + (drawn - kept) t. Returns false and leaves the
	/// drawn pose alone while the kept buffer holds no pose of the same bones (in the original it is uninitialised
	/// memory until the first change)
	bool Blend(float t) noexcept
	{
		if (_kept.size() != _drawn.size())
		{
			return false;
		}
		for (size_t i = 0; i < _drawn.size(); ++i)
		{
			for (int c = 0; c < 4; ++c)
			{
				for (int r = 0; r < 4; ++r)
				{
					const float prev = _kept[i][c][r];
					_drawn[i][c][r] = (_drawn[i][c][r] - prev) * t + prev;
				}
			}
		}
		return true;
	}

private:
	std::vector<glm::mat4> _drawn;
	std::vector<glm::mat4> _kept;
};

/// The pose a hand state showed last, as the normal state's update keeps it: none just after the state is entered.
/// Another pose than the last one starts a blend, as a change of the hand's state does.
class HandStatePose
{
public:
	/// Entering the state: its first pose is only noted
	void Enter() noexcept { _pose.reset(); }

	/// The frame's pose: true when the state showed another one before it since its Enter
	[[nodiscard]] bool Change(std::string_view pose)
	{
		const bool changed = _pose.has_value() && *_pose != pose;
		if (!_pose || changed)
		{
			_pose = std::string(pose);
		}
		return changed;
	}

private:
	std::optional<std::string> _pose;
};

} // namespace openblack
