/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <bit>
#include <chrono>
#include <functional>
#include <numbers>
#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::zoom_to
{

/// Where the camera is and what it looks at
struct CameraView
{
	glm::vec3 origin;
	glm::vec3 focus;
};

/// The temple key flies to the temple from this far, looking down this steeply
inline constexpr float k_TempleDistance = 130.0f;
/// (the game's float for it is three steps below the nearest float to a quarter of pi)
inline constexpr float k_TemplePitch = std::bit_cast<float>(0x3F490FD8u);
/// The realm key flies over the middle of the island from this far, looking down this steeply
inline constexpr float k_RealmDistance = 2200.0f;
inline constexpr float k_RealmPitch = std::numbers::pi_v<float> / 8.0f;
/// A single tap of the temple key puts the camera this far from what it looks at, this steeply
inline constexpr float k_TapDistance = 50.0f;
/// (the game's float for it is two steps below the nearest float to a sixth of pi)
inline constexpr float k_TapPitch = std::bit_cast<float>(0x3F060A90u);
/// The camera looks at a point this far above the ground
inline constexpr float k_FocusAboveGround = 3.0f;
/// A second tap of the temple key within this long is a double tap
inline constexpr std::chrono::milliseconds k_DoubleTapTime {500};
/// While the camera still looks within this far of where it was flown to, the key flies it back
inline constexpr float k_ReturnRadius = 100.0f;
/// How high the middle of a flight to a place, or back from it, rises for each metre across (the float of 0.4)
inline constexpr float k_PlaceRise = std::bit_cast<float>(0x3ECCCCCDu);
/// The flight to a single tap's view does not rise
inline constexpr float k_TapRise = 0.0f;

/// The land's height at a place (x, z), read at the place's map position
using Altitude = std::function<float(glm::vec2 place)>;

/// The time a tap of the keys is measured by: the game's engine clock as sampled at the start of this frame. It runs on
/// in pause and keeps to the fixed step, so a double tap is told apart the same way on every run.
[[nodiscard]] std::chrono::milliseconds TapTime();

/// The heading of a view, as the camera takes it from its origin and focus
[[nodiscard]] float HeadingOf(const CameraView& view);
/// The point the camera looks at over a place: the land's height there, a little higher
[[nodiscard]] glm::vec3 FocusOver(glm::vec2 place, const Altitude& altitude);
/// The point a distance from the camera along its line of sight: the direction from the origin to the focus made a unit
/// one (its squares added z first, then y, then x; left as it is when the camera looks at itself), times the distance,
/// plus the origin
[[nodiscard]] glm::vec3 AlongView(const CameraView& view, float distance);
/// The view from a distance and pitch at a heading that looks at the point over a place
[[nodiscard]] CameraView ViewOver(glm::vec2 place, float distance, float heading, float pitch, const Altitude& altitude);

/// A flight a key starts
struct Flight
{
	CameraView view;
	/// How high the flight's middle rises for each metre across
	float rise;
	/// It goes to a place or back from it, not to a single tap's view
	bool toPlace;
};

/// The temple and realm keys. A single tap of the temple key gives a pleasing view of the land ahead, as far along the
/// view as the camera finds the land; a double tap flies to the temple, and the realm key flies over the middle of the
/// island. Pressed again while still looking there, either key flies back to where the camera was.
class ZoomToPlaces
{
public:
	/// The temple key pressed at a time (TapTime()). clickDistance is how far ahead the world camera finds the land;
	/// without it (a fight watched, or another camera) a single tap starts no flight but still counts as a tap. Without a
	/// temple the double tap flies over the realm.
	[[nodiscard]] std::optional<Flight> PressTemple(std::chrono::milliseconds now, const CameraView& current,
	                                                std::optional<float> clickDistance, std::optional<glm::vec2> temple,
	                                                glm::vec2 realm, const Altitude& altitude);
	/// A game starts with a new camera, which has no view to fly back to and no last tap, and flies from its view to the
	/// temple as the temple key's double tap flies
	[[nodiscard]] Flight StartAtTemple(const CameraView& current, glm::vec2 temple, const Altitude& altitude);
	/// The realm key pressed
	[[nodiscard]] Flight PressRealm(const CameraView& current, glm::vec2 realm, const Altitude& altitude);
	/// A new world camera is made (as when the keys take the camera back from Creature Mode): no last tap, and no view to
	/// fly back to
	void Restart();

	/// Whether the temple key pressed at a time would be a double tap
	[[nodiscard]] bool IsDoubleTap(std::chrono::milliseconds now) const noexcept
	{
		return _lastTap.has_value() && now - *_lastTap <= k_DoubleTapTime;
	}

	/// Whether the camera has been flown to a place it can be flown back from
	[[nodiscard]] bool IsZoomedTo() const noexcept { return _returnTo.has_value(); }

private:
	/// A double tap or the realm key: back to the view the camera was flown from while it still looks near the place,
	/// else to the place
	[[nodiscard]] Flight ZoomTo(const CameraView& current, glm::vec2 place, float distance, float pitch,
	                            const Altitude& altitude);

	std::optional<std::chrono::milliseconds> _lastTap;
	/// The view to fly back to, while flown to a place
	std::optional<CameraView> _returnTo;
};

} // namespace openblack::zoom_to
