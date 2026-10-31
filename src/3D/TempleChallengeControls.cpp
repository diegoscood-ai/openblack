/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleChallengeControls.h"

#include <algorithm>
#include <utility>

#include "3D/TempleChallengePick.h"

using namespace openblack;
using temple_challenge_room::Control;

void TempleChallengeControls::Find(std::span<const std::string> subMeshNames)
{
	_controls.clear();
	_held.reset();
	for (uint32_t i = 0; i < subMeshNames.size(); ++i)
	{
		if (const auto control = temple_challenge_room::ControlOf(subMeshNames[i]); control.has_value())
		{
			_controls.push_back({.subMesh = i, .control = *control});
		}
	}
}

void TempleChallengeControls::SetChallenges(std::span<const temple_challenge_room::Challenge> challenges)
{
	for (size_t slot = 0; slot < _challenges.size(); ++slot)
	{
		_challenges.at(slot) = slot < challenges.size() ? std::optional(challenges[slot]) : std::nullopt;
	}
}

std::optional<temple_challenge_room::Challenge> TempleChallengeControls::ChallengeOf(uint32_t slot) const
{
	return slot < _challenges.size() ? _challenges.at(slot) : std::nullopt;
}

std::vector<TempleChallengeControl> TempleChallengeControls::GetToolTipControls() const
{
	std::vector<TempleChallengeControl> controls;
	for (const auto& found : _controls)
	{
		const bool button = found.control.control == Control::Button;
		const auto challenge = ChallengeOf(found.control.slot);
		if ((button || found.control.control == Control::Picture) && challenge.has_value())
		{
			controls.push_back({.subMesh = found.subMesh,
			                    .button = button,
			                    .place = found.control.slot,
			                    .done = !temple_challenge_pick::CanChoose(challenge->success)});
		}
	}
	return controls;
}

std::vector<TempleChallengeControls::Picture> TempleChallengeControls::GetPictures() const
{
	std::vector<Picture> pictures;
	for (const auto& found : _controls)
	{
		if (found.control.control == Control::Picture && ChallengeOf(found.control.slot).has_value())
		{
			pictures.push_back({.slot = found.control.slot, .subMesh = found.subMesh});
		}
	}
	return pictures;
}

void TempleChallengeControls::Enter()
{
	_highDetail.reset();
	_pictureLookedAt = false;
}

void TempleChallengeControls::LookAt(uint32_t slot)
{
	_highDetail = slot;
	_pictureLookedAt = true;
}

std::optional<uint32_t> TempleChallengeControls::GetLookedAtSlot() const
{
	return _pictureLookedAt ? _highDetail : std::nullopt;
}

bool TempleChallengeControls::Shown(const Found& found) const
{
	return temple_challenge_room::Visible(found.control.control, ChallengeOf(found.control.slot));
}

const TempleChallengeControls::Found* TempleChallengeControls::FindShown(uint32_t subMesh) const
{
	const auto found = std::ranges::find(_controls, subMesh, &Found::subMesh);
	if (found == _controls.end() || !Shown(*found))
	{
		return nullptr;
	}
	return &*found;
}

bool TempleChallengeControls::Hold(bool pressed, std::optional<uint32_t> hoveredSubMesh, std::optional<Click>& click)
{
	const bool pressing = pressed && !_wasPressed;
	_wasPressed = pressed;
	if (pressing && hoveredSubMesh.has_value())
	{
		if (const auto* found = FindShown(*hoveredSubMesh); found != nullptr && found->control.control != Control::Scroll)
		{
			_held = *found;
		}
	}
	if (pressed || !_held.has_value())
	{
		return _held.has_value();
	}
	// let go: a click if over the same control, still shown
	const auto held = *std::exchange(_held, std::nullopt);
	const bool clicks = held.control.control == Control::Picture || held.control.control == Control::Button;
	if (clicks && hoveredSubMesh == held.subMesh && Shown(held))
	{
		click = Click {.control = held.control.control, .slot = held.control.slot, .subMesh = held.subMesh};
	}
	return true;
}

bool TempleChallengeControls::IsControl(uint32_t subMesh) const
{
	const auto* found = FindShown(subMesh);
	return found != nullptr && found->control.control != Control::Scroll;
}

std::vector<uint32_t> TempleChallengeControls::GetHidden() const
{
	std::vector<uint32_t> hidden;
	for (const auto& found : _controls)
	{
		if (!Shown(found))
		{
			hidden.push_back(found.subMesh);
		}
	}
	return hidden;
}
