/*******************************************************************************
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
#include <string_view>

/// The challenge room's controls: 21 slots, each with a picture of its challenge (a click turns the room's camera to
/// it, or chooses it once the camera has come all the way), a button that chooses it straight away, and three frames
/// (bronze for a slot with no challenge yet, silver or gold for one with a challenge), and the room's scroll. Slot k
/// holds the (k + 1)th challenge found. Wiki:
/// docs/wiki/interface/temple-interior.md#the-challenge-rooms-controls-and-its-cameras-blend
namespace openblack::temple_challenge_room
{

/// The room's slots
inline constexpr uint32_t k_Slots = 21;

enum class Control : uint8_t
{
	Picture,
	/// The bronze frame, of a slot with no challenge
	Frame,
	SilverFrame,
	GoldFrame,
	Button,
	Scroll,
};

/// One of the room's controls and its slot (0 for the scroll)
struct ControlSlot
{
	Control control {Control::Scroll};
	uint32_t slot {0};

	bool operator==(const ControlSlot&) const = default;
};

/// The control a submesh of the room's mesh is, by its name, in any case; none for the other submeshes
[[nodiscard]] std::optional<ControlSlot> ControlOf(std::string_view subMeshName);

/// What the controls of a slot show of its challenge
struct Challenge
{
	/// Its frame is gold rather than silver (the challenge's type as it was found)
	bool gold {false};
	/// How far it is done, 0 to 1
	float success {0.0f};
};

/// Whether a control is shown, for the challenge of its slot (none: the slot has no challenge yet). A control not shown
/// cannot be clicked either. The picture and the silver or gold frame show with a challenge, the bronze frame without
/// one, and the button only with a challenge that can still be chosen; the scroll always
[[nodiscard]] bool Visible(Control control, const std::optional<Challenge>& challenge);

/// The room camera's blend toward what it looks at, which a click on a picture reads (temple_challenge_pick::ClickOn): it
/// rises by 0.75 a second while the camera looks at one of the room's submeshes (any of them, the scroll too), held to
/// 1, and falls by 0.7 a second otherwise, from at most 1 and held to 0. `seconds` is the frame's time
[[nodiscard]] float BlendStep(float blend, bool looking, float seconds);

} // namespace openblack::temple_challenge_room
