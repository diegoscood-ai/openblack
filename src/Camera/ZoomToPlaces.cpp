/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ZoomToPlaces.h"

#include <cmath>

#include "GameClock.h"
#include "ScriptCamera.h"

namespace openblack::zoom_to
{

std::chrono::milliseconds TapTime()
{
	return std::chrono::milliseconds(game_clock::EngineFrameSampleMs());
}

float HeadingOf(const CameraView& view)
{
	float heading = 0.0f;
	float pitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(view.origin, view.focus, heading, pitch);
	return heading;
}

glm::vec3 FocusOver(glm::vec2 place, const Altitude& altitude)
{
	return {place.x, altitude(place) + k_FocusAboveGround, place.y};
}

glm::vec3 AlongView(const CameraView& view, float distance)
{
	auto direction = view.focus - view.origin;
	if (direction.x != 0.0f || direction.y != 0.0f || direction.z != 0.0f)
	{
		const float length =
		    std::sqrt(((direction.z * direction.z) + (direction.y * direction.y)) + (direction.x * direction.x));
		const float share = 1.0f / length;
		direction = {direction.x * share, direction.y * share, direction.z * share};
	}
	return {(direction.x * distance) + view.origin.x, (direction.y * distance) + view.origin.y,
	        (direction.z * distance) + view.origin.z};
}

CameraView ViewOver(glm::vec2 place, float distance, float heading, float pitch, const Altitude& altitude)
{
	const auto focus = FocusOver(place, altitude);
	return {.origin = script_camera::PointFromDistanceHeadingAndPitch(focus, distance, heading, pitch), .focus = focus};
}

Flight ZoomToPlaces::ZoomTo(const CameraView& current, glm::vec2 place, float distance, float pitch, const Altitude& altitude)
{
	_lastTap.reset();
	if (_returnTo.has_value())
	{
		// Still looking within the return radius of the place (the squares added z first), it flies back
		const float acrossX = place.x - current.focus.x;
		const float acrossZ = place.y - current.focus.z;
		const auto back = *_returnTo;
		_returnTo.reset();
		if (!(k_ReturnRadius * k_ReturnRadius < (acrossZ * acrossZ) + (acrossX * acrossX)))
		{
			return {.view = back, .rise = k_PlaceRise, .toPlace = true};
		}
	}
	// Flown back from, if pressed again while still looking there; the heading is kept
	_returnTo = current;
	return {.view = ViewOver(place, distance, HeadingOf(current), pitch, altitude), .rise = k_PlaceRise, .toPlace = true};
}

std::optional<Flight> ZoomToPlaces::PressTemple(std::chrono::milliseconds now, const CameraView& current,
                                                std::optional<float> clickDistance, std::optional<glm::vec2> temple,
                                                glm::vec2 realm, const Altitude& altitude)
{
	if (!IsDoubleTap(now))
	{
		// A single tap looks at the land as far ahead as the camera finds it, keeping the heading, from a pleasing
		// distance and pitch
		_lastTap = now;
		if (!clickDistance.has_value())
		{
			return std::nullopt;
		}
		const auto ahead = AlongView(current, *clickDistance);
		return Flight {.view = ViewOver({ahead.x, ahead.z}, k_TapDistance, HeadingOf(current), k_TapPitch, altitude),
		               .rise = k_TapRise,
		               .toPlace = false};
	}
	if (!temple.has_value())
	{
		return PressRealm(current, realm, altitude);
	}
	return ZoomTo(current, *temple, k_TempleDistance, k_TemplePitch, altitude);
}

Flight ZoomToPlaces::StartAtTemple(const CameraView& current, glm::vec2 temple, const Altitude& altitude)
{
	Restart();
	return ZoomTo(current, temple, k_TempleDistance, k_TemplePitch, altitude);
}

Flight ZoomToPlaces::PressRealm(const CameraView& current, glm::vec2 realm, const Altitude& altitude)
{
	return ZoomTo(current, realm, k_RealmDistance, k_RealmPitch, altitude);
}

void ZoomToPlaces::Restart()
{
	_lastTap.reset();
	_returnTo.reset();
}

} // namespace openblack::zoom_to
