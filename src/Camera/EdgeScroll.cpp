/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EdgeScroll.h"

#include <cmath>

#include <glm/gtc/constants.hpp>

#include "KeyboardMoveSpeed.h"

namespace openblack::edge_scroll
{

namespace
{
/// The step's share of the camera's height over or under its focus
constexpr float k_HeightShare = 3.0f;
/// The push's speed is half the step
constexpr double k_StepHalf = 0.5;
/// The band past the edges is an eighth of the screen's width
constexpr int32_t k_BandShare = 8;
/// The speeds die away by e to these over a second
constexpr float k_DecayInside = -7.0f;
constexpr float k_DecayAtEdge = -0.2f;
constexpr float k_TurnDecayAtEdge = -2.2f;
/// The move's share of the speed across and ahead
constexpr float k_MoveShare = 0.16f;

/// A cube kept the same sign
float Cubed(float t)
{
	return t * t * t;
}
} // namespace

bool Enabled(bool pushScrolling, int32_t features, bool windowed)
{
	return pushScrolling && (features & k_Feature) != 0 && !windowed;
}

float Step(float originHeight, float focusHeight)
{
	float step;
	if (focusHeight <= originHeight)
	{
		step = (originHeight - focusHeight) * k_HeightShare;
		if (step <= k_MinSpeed)
		{
			step = k_MinSpeed;
		}
		else if (k_MaxSpeed <= step)
		{
			step = k_MaxSpeed;
		}
	}
	else
	{
		const float height = (focusHeight - originHeight) * k_HeightShare;
		step = k_MinSpeed;
		if (k_MinSpeed < height && height < k_MinSpeed * 4.0f)
		{
			step = height;
		}
		else if (k_MinSpeed < height)
		{
			step = k_MinSpeed * 4.0f;
		}
	}
	return static_cast<float>(static_cast<double>(step) * k_StepHalf);
}

Cursor FollowPointer(glm::ivec2 cursor, glm::ivec2 pointer, glm::ivec2 mouseDelta, glm::ivec2 screen, bool windowed)
{
	Cursor followed {.position = cursor, .insideX = true, .insideY = true};
	if ((pointer.x > 0 && pointer.x < screen.x - 1) || windowed)
	{
		followed.position.x = pointer.x;
	}
	else
	{
		followed.position.x += mouseDelta.x;
		followed.insideX = false;
	}
	if ((pointer.y > 0 && pointer.y < screen.y - 1) || windowed)
	{
		followed.position.y = pointer.y;
	}
	else
	{
		followed.position.y += mouseDelta.y;
		followed.insideY = false;
	}
	return followed;
}

glm::vec3 Push(glm::vec3 speed, glm::ivec2 cursor, glm::ivec2 pointer, glm::ivec2 mouseDelta, glm::ivec2 screen, float step,
               float seconds)
{
	const int32_t width = screen.x - 1;
	const int32_t height = screen.y - 1;
	const int32_t band = width / k_BandShare;
	const int32_t dx = mouseDelta.x;
	const int32_t dy = mouseDelta.y;
	if (std::sqrt(static_cast<float>(dx * dx + dy * dy)) == 0.0f)
	{
		return speed;
	}
	if ((cursor.x <= -band && dx < 0) || (cursor.x >= band + width && dx > 0))
	{
		// across, turning more the further from the middle row
		float t = Cubed(static_cast<float>(pointer.y - height / 2) / static_cast<float>(height));
		speed.x += static_cast<float>(dx) * k_Factor * step * seconds;
		if (pointer.y < height / 2)
		{
			t = -t;
		}
		speed.z += static_cast<float>(dx) * k_TurnFactor * t * step * seconds;
		return speed;
	}
	if ((cursor.y <= -band && dy < 0) || (cursor.y >= band + height && dy > 0))
	{
		// up and down, turning more the further from the middle column
		const float upDownStep = step * k_UpDownShare;
		float t = Cubed(static_cast<float>(pointer.x - width / 2) / static_cast<float>(width));
		speed.y += static_cast<float>(dy) * k_Factor * upDownStep * seconds;
		if (pointer.y < height / 2)
		{
			t = -t;
		}
		speed.z += static_cast<float>(dy) * k_TurnFactor * t * upDownStep * seconds;
	}
	return speed;
}

glm::vec3 Decay(glm::vec3 speed, bool insideX, bool insideY, float seconds)
{
	const bool inside = insideX && insideY;
	const float across = DecayFactor(inside ? k_DecayInside : k_DecayAtEdge, seconds);
	const float turn = DecayFactor(inside ? k_DecayInside : k_TurnDecayAtEdge, seconds);
	speed.x *= across;
	speed.y *= across;
	speed.z *= turn;
	return Stopped(speed) ? glm::vec3(0.0f) : speed;
}

float DecayFactor(float rate, float seconds)
{
	constexpr double k_Log2E = 1.4426950408889634;
	constexpr double k_Ln2 = 0.6931471805599453;
	const float power = rate * seconds;
	const auto twoPower = static_cast<float>(static_cast<double>(power) * k_Log2E);
	const float whole = std::nearbyint(twoPower);
	const float rest = twoPower - whole;
	const auto scaled = static_cast<float>(1.0 + std::expm1(static_cast<double>(rest) * k_Ln2));
	return std::ldexp(scaled, static_cast<int>(whole));
}

float FrameSeconds(int64_t milliseconds)
{
	constexpr float k_SecondsPerMs = 0.001f;
	return static_cast<float>(milliseconds) * k_SecondsPerMs;
}

bool Stopped(glm::vec3 speed)
{
	return speed.x * speed.x + speed.y * speed.y + speed.z * speed.z < k_StopSpeedSq;
}

glm::vec3 MoveOverLand(glm::vec3 speed, float seconds, float pitch, float heading)
{
	const float dx = seconds * speed.x;
	const float dy = seconds * speed.y;
	const double slope = MoveSlope(pitch);
	const float across = dx * -k_MoveShare;
	const auto ahead = static_cast<float>(static_cast<double>(dy * k_MoveShare) / slope);
	return TurnedByHeading(across, ahead, heading);
}

float Turn(glm::vec3 speed, float seconds, int32_t screenWidth)
{
	const float dz = seconds * speed.z;
	return dz * glm::pi<float>() / static_cast<float>(screenWidth);
}

} // namespace openblack::edge_scroll
