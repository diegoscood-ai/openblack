/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandGrain.h"

#include <cstdlib>

#include <algorithm>
#include <chrono>

#include <spdlog/spdlog.h>

#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
struct State
{
	bool active {false};        ///< +0x124
	bool loop {false};          ///< +0x128
	float t {0.0f};             ///< +0x12C
	float totalTime {1.0f};     ///< +0x130
	float heightToRaise {0.0f}; ///< +0x134
	float angleToRaise {0.0f};  ///< +0x138
	bool clampHand {false};     ///< +0x1F8
	glm::vec3 start {0.0f};     ///< +0x1EC
	float tilt {0.0f};          ///< +0x114
	float height {0.0f};        ///< +0x118
	float lastTilt {0.0f};      ///< +0x11C
	float lastHeight {0.0f};    ///< +0x120
	bool holdingSeed {false};
	std::chrono::steady_clock::time_point turnTime {};
};
State g_State;

const hand_grain::Spline& KeyPoints()
{
	// the ctor (0x5B2CF2) passes 1e30 for both end slopes because +0x148 is 1: a natural spline (with +0x148 = 0 it
	// would pass 0 and the raise would peak at 1.96 instead of 1.61)
	static const auto spline = hand_grain::BuildSpline(1e30f, 1e30f);
	return spline;
}

/// g_game +0x205D64: the fraction of the turn since the last GameTurnUpdate (inf: a turn of 0.1 s)
float TurnFraction()
{
	const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - g_State.turnTime).count();
	return std::clamp(elapsed / 0.1f, 0.0f, 1.0f);
}

bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_SPELL_TRACE") != nullptr || std::getenv("OPENBLACK_HAND_TRACE") != nullptr;
	return trace;
}
} // namespace

hand_grain::Spline hand_grain::BuildSpline(float yp1, float ypn)
{
	Spline spline;
	const auto& x = spline.x;
	const auto& y = spline.y;
	auto& y2 = spline.y2;
	constexpr size_t k_N = 4;
	std::array<float, k_N> u {};
	if (yp1 > 0.99e30f)
	{
		y2[0] = 0.0f;
		u[0] = 0.0f;
	}
	else
	{
		y2[0] = -0.5f;
		u[0] = (3.0f / (x[1] - x[0])) * ((y[1] - y[0]) / (x[1] - x[0]) - yp1);
	}
	for (size_t i = 1; i + 1 < k_N; ++i)
	{
		const float sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
		const float p = sig * y2[i - 1] + 2.0f;
		y2[i] = (sig - 1.0f) / p;
		u[i] = (y[i + 1] - y[i]) / (x[i + 1] - x[i]) - (y[i] - y[i - 1]) / (x[i] - x[i - 1]);
		u[i] = (6.0f * u[i] / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
	}
	float qn = 0.0f;
	float un = 0.0f;
	if (!(ypn > 0.99e30f))
	{
		qn = 0.5f;
		un = (3.0f / (x[k_N - 1] - x[k_N - 2])) * (ypn - (y[k_N - 1] - y[k_N - 2]) / (x[k_N - 1] - x[k_N - 2]));
	}
	y2[k_N - 1] = (un - qn * u[k_N - 2]) / (qn * y2[k_N - 2] + 1.0f);
	for (size_t k = k_N - 1; k-- > 0;)
	{
		y2[k] = y2[k] * y2[k + 1] + u[k];
	}
	return spline;
}

float hand_grain::Evaluate(const Spline& spline, float t)
{
	int lo = 0;
	int hi = static_cast<int>(spline.x.size()) - 1;
	while (hi - lo > 1)
	{
		const int k = (hi + lo) / 2;
		if (spline.x[static_cast<size_t>(k)] > t)
		{
			hi = k;
		}
		else
		{
			lo = k;
		}
	}
	const auto l = static_cast<size_t>(lo);
	const auto h = static_cast<size_t>(hi);
	const float step = spline.x[h] - spline.x[l];
	if (step == 0.0f)
	{
		return t;
	}
	const float a = (spline.x[h] - t) / step;
	const float b = (t - spline.x[l]) / step;
	return a * spline.y[l] + b * spline.y[h] +
	       ((a * a * a - a) * spline.y2[l] + (b * b * b - b) * spline.y2[h]) * (step * step) * (1.0f / 6.0f);
}

void hand_grain::Start(bool clampHand, float totalTime, float heightToRaise, float angleToRaise, bool loop)
{
	g_State.active = true;
	g_State.loop = loop;
	g_State.t = 0.0f;
	g_State.totalTime = totalTime;
	g_State.heightToRaise = heightToRaise;
	g_State.angleToRaise = angleToRaise;
	g_State.clampHand = clampHand;
	// CHand +0x78: the hand's point (openblack's left hand transform: the grip point while it holds something)
	if (Locator::handSystem::has_value() && Locator::entitiesRegistry::has_value())
	{
		const auto hand = Locator::handSystem::value().GetPlayerHands()[0];
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(hand))
		{
			g_State.start = registry.Get<const ecs::components::Transform>(hand).position;
		}
	}
}

void hand_grain::Stop()
{
	g_State.clampHand = false;
	g_State.height = 0.0f;
	g_State.tilt = 0.0f;
	g_State.active = false;
}

void hand_grain::GameTurnUpdate(float dt)
{
	g_State.lastHeight = g_State.height;
	g_State.lastTilt = g_State.tilt;
	g_State.turnTime = std::chrono::steady_clock::now();
	// +0x13C (a creature it follows, vt 0x2C IsAvailable): creatures are M8
	if (!g_State.active)
	{
		return;
	}
	g_State.t += dt / g_State.totalTime;
	if (g_State.t > 1.0f)
	{
		if (g_State.loop)
		{
			g_State.t = 0.0f;
		}
		else
		{
			Stop();
		}
	}
	if (!g_State.active)
	{
		return;
	}
	const float v = Evaluate(KeyPoints(), g_State.t);
	g_State.height = v * g_State.heightToRaise;
	g_State.tilt = v * g_State.angleToRaise;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Grain trace: t {:.2f} of {:.1f} s, raise {:.2f} m, tilt {:.3f} rad, clamped {}",
		                   g_State.t, g_State.totalTime, g_State.height, g_State.tilt, g_State.clampHand);
	}
}

float hand_grain::Height()
{
	return g_State.lastHeight + (g_State.height - g_State.lastHeight) * TurnFraction();
}

float hand_grain::Tilt()
{
	return g_State.lastTilt + (g_State.tilt - g_State.lastTilt) * TurnFraction();
}

std::optional<glm::vec3> hand_grain::ClampedPosition()
{
	return g_State.clampHand ? std::optional(g_State.start) : std::nullopt;
}

bool hand_grain::Active()
{
	return g_State.active;
}

glm::vec3 hand_grain::Debug()
{
	return {g_State.t, g_State.height, g_State.tilt};
}

void hand_grain::SetHoldingSeed(bool holding)
{
	if (holding == g_State.holdingSeed)
	{
		return;
	}
	g_State.holdingSeed = holding;
	if (holding)
	{
		// HandStateGrain Enter 0x5B3080 (after HandStateHolding's): the grain state is cleared
		const auto turnTime = g_State.turnTime;
		g_State = State {};
		g_State.holdingSeed = true;
		g_State.turnTime = turnTime;
	}
	else
	{
		// Exit 0x5B3290: +0x124 = 0, +0x13C = 0
		g_State.active = false;
	}
}

void hand_grain::Reset()
{
	g_State = State {};
}
