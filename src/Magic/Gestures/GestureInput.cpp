/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureInput.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/HandLandPoint.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "Game.h"
#include "Input/GameCursor.h"
#include "Input/RealInput.h"
#include "Locator.h"
#include "PowerUpSystem.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::magic::gestures;

namespace
{
/// A scripted stroke plays one pixel each time this many ms have gone by
constexpr float k_MessageMs = mouse_messages::k_SampleMs;

/// The mouse sampling, the camera-moved flag and a scripted stroke being played; never cleared on a land load
struct GestureSamplingState
{
	std::optional<glm::vec3> lastCamera;
	bool cameraMoving {false};
	std::optional<glm::ivec2> lastMouse;
	/// The mouse messages' clock and the handler's sum
	mouse_messages::Clock messages;
	/// The messages of the frame
	std::vector<float> frameMessages;
	float strokeMs {0.0f};
	std::vector<glm::ivec2> stroke;
	size_t strokeNext {0};
};

/// This module's state (the miracles' service's hand magic state)
GestureSamplingState& Sampling()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("magic::gestures::sampling: no miracles' service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value().HandMagic().Get<GestureSamplingState>();
}

glm::vec2 ScreenSize()
{
	if (!Locator::windowing::has_value())
	{
		return {1.0f, 1.0f};
	}
	const auto size = Locator::windowing::value().GetSize();
	return {static_cast<float>(std::max(1, size.x)), static_cast<float>(std::max(1, size.y))};
}

void Ray(glm::vec2 pixel, glm::vec3& origin, glm::vec3& direction)
{
	const auto ray = Locator::camera::value().RayFromEye(pixel / ScreenSize());
	origin = ray.origin;
	direction = ray.direction;
}

/// The point `length` along the line from the eye through a pixel. While the falling spell draws a view of its own, and
/// without a screen, it stays the camera's position plus the unit mouse ray times the length, as the gestures had it
/// before they took the near plane's line (which eye the original has then is not known)
glm::vec3 PointAlongPixel(glm::ivec2 pixel, float length)
{
	if (!Locator::camera::has_value())
	{
		return glm::vec3(0.0f);
	}
	const auto& camera = Locator::camera::value();
	if (!camera.HasDrawnView())
	{
		if (const auto line = sampling::LineThroughPixel(pixel); line.has_value())
		{
			return ecs::hand_land_point::AlongMouseRay(line->eye, line->nearPoint, length);
		}
	}
	glm::vec3 origin;
	glm::vec3 direction;
	Ray(glm::vec2(pixel), origin, direction);
	return camera.GetOrigin() + glm::normalize(direction) * length;
}
} // namespace

float sampling::ScreenRatio()
{
	const auto size = ScreenSize();
	return size.x / size.y;
}

std::optional<sampling::PixelLine> sampling::LineThroughPixel(glm::ivec2 pixel)
{
	if (!Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return std::nullopt;
	}
	const auto screenSize = Locator::windowing::value().GetSize();
	if (screenSize.x <= 0 || screenSize.y <= 0)
	{
		return std::nullopt;
	}
	const auto& camera = Locator::camera::value();
	const auto eye = camera.GetEye();
	if (glm::any(glm::isnan(eye)))
	{
		return std::nullopt;
	}
	return PixelLine {.eye = eye, .nearPoint = camera.PixelNearPoint(eye, screenSize, pixel, camera.GetPixelNearClip())};
}

std::optional<glm::vec3> sampling::InterfaceLandUnderPixel(glm::ivec2 pixel)
{
	if (!Locator::pickingSystem::has_value())
	{
		return std::nullopt;
	}
	const auto line = LineThroughPixel(pixel);
	if (!line.has_value())
	{
		return std::nullopt;
	}
	return Locator::pickingSystem::value().LandUnderPixel(line->eye, line->nearPoint, true);
}

glm::vec3 sampling::LandOrPointAlongPixel(glm::ivec2 pixel, float length)
{
	if (const auto line = LineThroughPixel(pixel); line.has_value() && Locator::pickingSystem::has_value())
	{
		// the line unmoved, from the eye: the land, else the sea no further than its reach from the eye
		if (const auto hit = Locator::pickingSystem::value().LandOrSeaAlong(line->eye, line->nearPoint, line->eye);
		    hit.has_value())
		{
			if (!Locator::terrainSystem::has_value())
			{
				return *hit;
			}
			const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
			const float height = Locator::terrainSystem::value().GetHeightAt(glm::vec2(lookup(hit->x), lookup(hit->z)));
			return {hit->x, height, hit->z};
		}
	}
	return PointAlongPixel(pixel, length);
}

Projection sampling::CurrentProjection()
{
	Projection projection;
	if (!Locator::camera::has_value())
	{
		return projection;
	}
	const auto& camera = Locator::camera::value();
	projection.screenToLand = [](glm::ivec2 pixel) { return InterfaceLandUnderPixel(pixel); };
	projection.alongPixel = [](glm::ivec2 pixel, float length) { return PointAlongPixel(pixel, length); };
	projection.cameraPosition = camera.GetOrigin();
	// the camera's yaw: (cos, -sin) is the camera's right on the ground (inferred)
	const auto forward = camera.GetForward();
	const glm::vec2 right(-forward.z, forward.x);
	projection.yawAxis = glm::length(right) > 1e-5f ? glm::normalize(right) : glm::vec2(1.0f, 0.0f);
	return projection;
}

const mouse_messages::Clock& sampling::MessageClock()
{
	return Sampling().messages;
}

bool sampling::CameraMoving()
{
	return Sampling().cameraMoving;
}

void sampling::PlayStroke(std::vector<glm::ivec2> pixels)
{
	Sampling().stroke = std::move(pixels);
	Sampling().strokeNext = 0;
	Sampling().strokeMs = 0.0f;
}

bool sampling::PlayingStroke()
{
	return Sampling().strokeNext < Sampling().stroke.size();
}

void sampling::Update(float realSeconds)
{
	if (Locator::camera::has_value())
	{
		const auto position = Locator::camera::value().GetOrigin();
		Sampling().cameraMoving = Sampling().lastCamera.has_value() && *Sampling().lastCamera != position;
		Sampling().lastCamera = position;
	}
	if (PlayingStroke())
	{
		Sampling().strokeMs += realSeconds * 1000.0f;
		while (Sampling().strokeMs > k_MessageMs && PlayingStroke())
		{
			Sampling().strokeMs -= k_MessageMs;
			FeedSample(Sampling().stroke[Sampling().strokeNext++]);
		}
		if (!PlayingStroke())
		{
			Sampling().lastMouse = Sampling().stroke.back();
		}
		return;
	}
	if (!Locator::inputState::has_value())
	{
		return;
	}
	const auto mouse = input::GameCursor();
	const bool moved = !Sampling().lastMouse.has_value() || *Sampling().lastMouse != mouse;
	Sampling().lastMouse = mouse;
	// the input thread sends its own messages only while the window is active; a check run, which ignores the real
	// input, counts it as active whatever window has the focus, so that it runs the same every time
	const bool active =
	    input::IgnoreRealInput() || !Locator::windowing::has_value() || Locator::windowing::value().HasInputFocus();
	auto& state = Sampling();
	state.frameMessages.clear();
	// (approximate: a frame in which the mouse moved stands for one move, at its end; the frame's time for the system's
	// clock)
	mouse_messages::Frame(state.messages, realSeconds * 1000.0f, moved, active, state.frameMessages);
	for (const float ms : state.frameMessages)
	{
		if (mouse_messages::Handle(state.messages, ms))
		{
			FeedSample(mouse); // the message's pixel: the cursor's, moved or still
		}
	}
}
