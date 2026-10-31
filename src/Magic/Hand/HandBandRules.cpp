/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandBandRules.h"

#include <cmath>

#include <glm/gtc/constants.hpp>

using namespace openblack::magic;

namespace
{
constexpr float k_ChargeDurationFrom = 3.5f;
constexpr float k_ChargeDurationTo = 1.0f;
constexpr float k_ChargeIntervalFrom = 6.0f;
constexpr float k_ChargeIntervalTo = 0.3f;
} // namespace

float hand_bands::FlyInStart(int i, bool delayed) noexcept
{
	const float base = delayed ? k_DelayedStart : 0.0f;
	return static_cast<float>(i) * 0.1f + base;
}

float hand_bands::PermanentStart(bool delayed) noexcept
{
	return delayed ? k_DelayedStart : 0.0f;
}

float hand_bands::NextSpinAngle(float angle, int index, float seconds) noexcept
{
	return std::fmod(angle + (1.0f + 0.2f * static_cast<float>(index)) * k_Spin * seconds, glm::two_pi<float>());
}

float hand_bands::HeightAlongArm(int index) noexcept
{
	return k_Offset + static_cast<float>(index) * k_Step;
}

bool hand_bands::ChargeBandDue(bool started, float timer, float charge) noexcept
{
	return started || timer > k_ChargeIntervalFrom + (k_ChargeIntervalTo - k_ChargeIntervalFrom) * charge;
}

float hand_bands::ChargeBandSeconds(float charge) noexcept
{
	return k_ChargeDurationFrom + (k_ChargeDurationTo - k_ChargeDurationFrom) * charge;
}

uint8_t hand_bands::ChargeBandAlpha0(float charge) noexcept
{
	return static_cast<uint8_t>(static_cast<int>(3.0f + 2.0f * charge));
}

uint8_t hand_bands::ChargeBandAlpha1(float charge) noexcept
{
	return static_cast<uint8_t>(static_cast<int>(15.0f + 35.0f * charge));
}
