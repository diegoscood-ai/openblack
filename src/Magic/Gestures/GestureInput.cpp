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

#include <glm/geometric.hpp>
#include <glm/gtx/intersect.hpp>

#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Game.h"
#include "Locator.h"
#include "PowerUpSystem.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::magic::gestures;

namespace
{
/// 0x1C: past this many ms of mouse events a position message goes
constexpr float k_MessageMs = 28.0f;

std::optional<glm::vec3> g_LastCamera;
bool g_CameraMoving = false;
std::optional<glm::ivec2> g_LastMouse;
float g_MouseMs = 0.0f;
std::vector<glm::ivec2> g_Stroke;
size_t g_StrokeNext = 0;

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
	Locator::camera::value().DeprojectScreenToWorld(pixel / ScreenSize(), origin, direction);
}
} // namespace

float sampling::ScreenRatio()
{
	const auto size = ScreenSize();
	return size.x / size.y;
}

std::optional<glm::vec3> sampling::ScreenToLand(glm::vec2 pixel)
{
	if (!Locator::camera::has_value() || !Locator::dynamicsSystem::has_value())
	{
		return std::nullopt;
	}
	glm::vec3 origin;
	glm::vec3 direction;
	Ray(pixel, origin, direction);
	if (glm::any(glm::isnan(origin)) || glm::any(glm::isnan(direction)))
	{
		return std::nullopt;
	}
	// as Game::Update finds the point under the cursor: the island, else the sea plane (inferido: openblack's picking,
	// the 1e10 range and the y = 0 sea plane are not the original's; fn_005E5620's land ray is not ported)
	if (const auto hit = Locator::dynamicsSystem::value().RayCastClosestHit(origin, direction, 1e10f); hit)
	{
		return hit->first.position;
	}
	float distance = 0.0f;
	if (glm::intersectRayPlane(origin, direction, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), distance))
	{
		return origin + direction * distance;
	}
	return std::nullopt;
}

Projection sampling::CurrentProjection()
{
	Projection projection;
	if (!Locator::camera::has_value())
	{
		return projection;
	}
	const auto& camera = Locator::camera::value();
	projection.screenToLand = [](glm::vec2 pixel) { return ScreenToLand(pixel); };
	projection.rayDirection = [](glm::vec2 pixel) {
		glm::vec3 origin;
		glm::vec3 direction;
		Ray(pixel, origin, direction);
		return glm::normalize(direction);
	};
	projection.cameraPosition = camera.GetOrigin();
	// fn_00441E60's yaw: (cos, -sin) is the camera's right on the ground (inf)
	const auto forward = camera.GetForward();
	const glm::vec2 right(-forward.z, forward.x);
	projection.yawAxis = glm::length(right) > 1e-5f ? glm::normalize(right) : glm::vec2(1.0f, 0.0f);
	return projection;
}

bool sampling::CameraMoving()
{
	return g_CameraMoving;
}

void sampling::PlayStroke(std::vector<glm::ivec2> pixels)
{
	g_Stroke = std::move(pixels);
	g_StrokeNext = 0;
	g_MouseMs = 0.0f;
}

bool sampling::PlayingStroke()
{
	return g_StrokeNext < g_Stroke.size();
}

void sampling::Update(float realSeconds)
{
	if (Locator::camera::has_value())
	{
		const auto position = Locator::camera::value().GetOrigin();
		g_CameraMoving = g_LastCamera.has_value() && *g_LastCamera != position;
		g_LastCamera = position;
	}
	if (PlayingStroke())
	{
		g_MouseMs += realSeconds * 1000.0f;
		while (g_MouseMs > k_MessageMs && PlayingStroke())
		{
			g_MouseMs -= k_MessageMs;
			FeedSample(g_Stroke[g_StrokeNext++]);
		}
		if (!PlayingStroke())
		{
			g_LastMouse = g_Stroke.back();
		}
		return;
	}
	if (Game::Instance() == nullptr)
	{
		return;
	}
	const auto mouse = Game::Instance()->GetMousePosition();
	const bool moved = !g_LastMouse.has_value() || *g_LastMouse != mouse;
	g_LastMouse = mouse;
	// (aproximado: a frame in which the mouse moved stands for a mouse event, and its time for the event's int ms)
	if (!moved)
	{
		return; // no mouse event, no message
	}
	g_MouseMs += realSeconds * 1000.0f; // [0xD019CC] += the event's ms (0x55001B)
	if (g_MouseMs > k_MessageMs)
	{
		g_MouseMs = 0.0f; // reset, not subtracted (0x55006C)
		FeedSample(mouse);
	}
}
