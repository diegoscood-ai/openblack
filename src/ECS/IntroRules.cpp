/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "IntroRules.h"

#include <glm/vec4.hpp>

#include "Common/ModelInverseSquareRoot.h"

namespace openblack::ecs::intro_rules
{
using graphics::billboard::Sprite;

float PutDownYaw()
{
	const float step = std::bit_cast<float>(0x41700000u) * std::bit_cast<float>(0x3C8EFA35u); // 15 x pi/180
	return std::bit_cast<float>(0xBF490FDBu) - step;                                          // -pi/4 - step
}

int32_t AdvanceClip(int32_t time, uint32_t milliseconds, int32_t playTime, bool looping, bool& cameRound)
{
	const auto n = static_cast<int32_t>(static_cast<uint32_t>(time) + milliseconds);
	int32_t result = n;
	if (looping)
	{
		// the signed remainder. (pending) a clip of 0 ms divides by zero in the original
		result = playTime > 0 ? n % playTime : 0;
	}
	else if (playTime - 1 < n)
	{
		result = playTime - 1;
	}
	cameRound = result < time; // the time went back
	return result;
}

namespace light
{
namespace
{
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu);        ///< Random(0, 2 pi)
constexpr float k_SizeStep = std::bit_cast<float>(0x3D4CCCCDu);     ///< 0.05
constexpr float k_SizeBase = std::bit_cast<float>(0x40400000u);     ///< 3
constexpr float k_MinSize = std::bit_cast<float>(0x38D1B717u);      ///< 1e-4, the smallest a sprite is made
constexpr float k_Jitter = std::bit_cast<float>(0x40000000u);       ///< Random(-2, 2)
constexpr float k_TailBack = std::bit_cast<float>(0xC0C00000u);     ///< -6: the streak behind the start
constexpr float k_StreakSize = std::bit_cast<float>(0x43AF0000u);   ///< 350
constexpr float k_StreakHeight = std::bit_cast<float>(0x3D4CCCCDu); ///< 0.05: half height 17.5
constexpr float k_StreakAngle = std::bit_cast<float>(0xBEC90FDBu);  ///< -pi / 8
constexpr float k_GlowSize = std::bit_cast<float>(0x42A00000u);     ///< 80, the glow

/// A sprite's new size: its origin scaled by new / old, then the size. The light's origins stay 0, so only the size
/// changes
void SetSize(Sprite& sprite, float size)
{
	const float ratio = size / sprite.size;
	sprite.origin.x = ratio * sprite.origin.x;
	sprite.origin.y = ratio * sprite.origin.y;
	sprite.size = size;
}

/// At least the smallest size
float Floor(float size)
{
	return size < k_MinSize ? k_MinSize : size;
}
} // namespace

Light Make(const glm::vec3& target, const glm::vec3& way, const Random& random)
{
	Light light;
	// (x^2 + y^2) + z^2, its inverse square root, each component times it
	const float lengthSquared = (way.x * way.x + way.y * way.y) + way.z * way.z;
	const float inverse = gutils::ModelInverseSquareRoot(lengthSquared);
	light.way = {inverse * way.x, inverse * way.y, inverse * way.z};
	// start = target + way x -4000; head and start both
	light.start = {target.x + light.way.x * k_Start, target.y + light.way.y * k_Start, target.z + light.way.z * k_Start};
	light.head = light.start;
	for (int i = 0; i < k_Sprites; ++i)
	{
		auto& sprite = light.sprites.at(static_cast<size_t>(i));
		sprite = Sprite {}; // zeroed
		// start + way x (float(-i) x 6)
		const float k = static_cast<float>(-i) * k_Spacing;
		sprite.position = {light.way.x * k + light.start.x, light.way.y * k + light.start.y, light.way.z * k + light.start.z};
		const auto steps = static_cast<float>((19 - i) * 15);
		float size = steps * k_SizeStep + k_SizeBase;
		if (i > 5)
		{
			sprite.cell = 50;
			size = size + size;
		}
		else
		{
			sprite.cell = 48;
		}
		SetSize(sprite, Floor(size));
		sprite.angle = random(0.0f, k_TwoPi);
		// alpha (19 - i) 255 / 20 (truncated), white
		sprite.argb = (static_cast<uint32_t>((19 - i) * 255 / 20) << 24) | 0xFFFFFFu;
		if (i == 18) // the glow at the head, 80 wide, alpha 0x28
		{
			sprite.argb = 0x28FFFFFFu;
			SetSize(sprite, k_GlowSize);
			sprite.position = light.start;
		}
	}
	// the last sprite, a thin streak 350 x 17.5 (half sizes) 6 behind the start, turned -pi / 8
	auto& streak = light.sprites.at(19);
	streak.position = {light.way.x * k_TailBack + light.start.x, light.way.y * k_TailBack + light.start.y,
	                   light.way.z * k_TailBack + light.start.z};
	streak.angle = k_StreakAngle;
	streak.argb = 0x0EFFFFFFu;
	streak.cell = 49;
	SetSize(streak, k_StreakSize);
	streak.height = k_StreakHeight; // its height: the origin's y scaled by 17.5 / the old half height (0)
	light.state = State::Falling;   // state, elapsed and arrived = 0
	light.elapsed = 0;
	light.arrived = false;
	return light;
}

glm::vec3 HeadPosition(Light& light)
{
	float d = static_cast<float>(light.elapsed) * k_Speed;
	if (!(d <= k_Arrives))
	{
		light.arrived = true;
		d = k_Arrives;
	}
	return {d * light.way.x + light.start.x, d * light.way.y + light.start.y, d * light.way.z + light.start.z};
}

glm::vec3 Start(Light& light)
{
	light.head = HeadPosition(light);
	return light.head;
}

void Step(Light& light, uint32_t milliseconds, const Random& random, Frame& out)
{
	out.drawn.clear();
	out.throughEverything = false;
	out.cameraAt.reset();
	const auto msSigned = static_cast<int32_t>(milliseconds);
	if (light.state == State::Gone)
	{
		return;
	}
	if (light.state == State::Falling)
	{
		const glm::vec3 head = Start(light);
		if (light.arrived) // hold for 500 ms
		{
			light.state = State::Holding;
			light.timer = k_HoldMs;
		}
		for (int i = 0; i < k_Sprites; ++i)
		{
			auto& sprite = light.sprites.at(static_cast<size_t>(i));
			if (i == 19) // the streak turns, the frame's game ms x the spin + angle
			{
				sprite.angle = static_cast<float>(msSigned) * k_Spin + sprite.angle;
			}
			sprite.position = head;
			if (i >= 18) // the glow and the streak stay at the head
			{
				continue;
			}
			const float k = static_cast<float>(-i) * k_Spacing;
			sprite.position = {light.way.x * k + head.x, light.way.y * k + head.y, light.way.z * k + head.z};
			if (milliseconds == 0) // no random numbers while the game time stands
			{
				continue;
			}
			sprite.angle = random(0.0f, k_TwoPi);
			if (i >= 5)
			{
				continue;
			}
			// the five front sprites flicker, (float(15 (19 - i)) x 0.05 + Random(-2, 2)) + 3
			const float jitter = random(-k_Jitter, k_Jitter);
			const float steps = static_cast<float>((19 - i) * 15);
			SetSize(sprite, Floor((steps * k_SizeStep + jitter) + k_SizeBase));
		}
		// drawn through everything once the fall has run 8237 ms
		out.throughEverything = light.elapsed > k_ThroughEverythingAfterMs;
		out.drawn.assign(light.sprites.begin(), light.sprites.end());
		light.elapsed = static_cast<int32_t>(static_cast<uint32_t>(light.elapsed) + milliseconds);
		// the chasing camera, 150 back up the way and 30 above
		glm::vec3 camera = HeadPosition(light);
		const glm::vec3 back {light.way.x * k_CameraBehind, light.way.y * k_CameraBehind, light.way.z * k_CameraBehind};
		camera.x = camera.x - back.x;
		camera.z = camera.z - back.z;
		camera.y = (camera.y - back.y) + k_CameraAbove;
		out.cameraAt = camera;
		return;
	}
	auto& glow = light.sprites.at(0);
	if (light.state == State::Holding)
	{
		light.timer = static_cast<int32_t>(static_cast<uint32_t>(light.timer) - milliseconds);
		if (light.timer < 0)
		{
			light.state = State::Fading;
			light.timer = k_FadeMs;
		}
	}
	if (light.state == State::Fading) // (int)((timer / 1000) x (255 - 50) + 50)
	{
		const float fraction = static_cast<float>(light.timer) / static_cast<float>(k_FadeMs);
		const float alpha = fraction * static_cast<float>(0xFF - k_FadeTo) + static_cast<float>(k_FadeTo);
		glow.argb = (static_cast<uint32_t>(static_cast<int32_t>(alpha)) << 24) | 0xFFFFFFu;
		light.timer = static_cast<int32_t>(static_cast<uint32_t>(light.timer) - milliseconds);
		if (light.timer < 0)
		{
			light.state = State::Gone;
		}
	}
	// through everything; the front sprite three times, a new Random(0, 2 pi) angle after the first and second
	out.throughEverything = true;
	out.drawn.push_back(glow);
	glow.angle = random(0.0f, k_TwoPi);
	out.drawn.push_back(glow);
	glow.angle = random(0.0f, k_TwoPi);
	out.drawn.push_back(glow);
}
} // namespace light

std::optional<glm::vec3> GripPoint(int32_t clipTime, std::span<const glm::mat4> bones, uint32_t bone, const glm::vec3& point,
                                   const glm::mat4& model)
{
	// nothing past 1817 ms, nor for a bone outside the pose. (pending) the original reads past the pose all the same
	if (!(static_cast<float>(clipTime) <= k_HoldsUntilMs) || bone >= bones.size())
	{
		return std::nullopt;
	}
	// the grip point taken through the bone, in the world, the boy 0.65 below it
	const glm::vec4 world = model * bones[bone] * glm::vec4(point.x, point.y, point.z, 1.0f);
	return glm::vec3(world.x, world.y - k_HeldBelowGrip, world.z);
}

} // namespace openblack::ecs::intro_rules
