/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BeliefSpriteMaths.h"

#include <cmath>

#include "Common/GUtilsX87.h"

using namespace openblack::particles;

namespace x87 = openblack::gutils::x87;

namespace
{
/// The wiggle of the second noise term, and the pulse's half height
constexpr float k_Half = 0.5f;
/// The z wiggle reads the noise further along by the random byte x this
constexpr float k_ZNoiseShift = 0.3f;
/// The rise is faster by this for each step of the random byte modulo 13
constexpr float k_RiseStep = 0.0249f;
constexpr int k_RiseSteps = 13;
/// The pulse's pace grows with the random byte / 256 x 0.3, and its phase is the random byte x 0.23
constexpr float k_PulsePace = 0.3f;
constexpr float k_PulsePhase = 0.23f;
/// A sprite stays this long after its fade ends
constexpr float k_Linger = 0.3f;

/// 0 when it is not above 0 (a NaN too), 1 when it is 1 or more
float Clamp01(float x)
{
	if (!(x > 0.0f))
	{
		return 0.0f;
	}
	return x < 1.0f ? x : 1.0f;
}

/// To the nearest integer, ties to even, as the FPU stores it
int32_t ToInt(float x)
{
	return static_cast<int32_t>(std::lrint(x));
}
} // namespace

uint8_t maths::BeliefSpriteStartAlpha(const BeliefSpriteRule& rule, int32_t amount)
{
	// the integer differences wrap as 32-bit integers do; the division and what follows round to float precision
	const auto above = static_cast<int32_t>(static_cast<uint32_t>(amount) - static_cast<uint32_t>(rule.valueAlphaMin));
	const auto range =
	    static_cast<int32_t>(static_cast<uint32_t>(rule.valueAlphaMax) - static_cast<uint32_t>(rule.valueAlphaMin));
	const float t = Clamp01(x87::Store(x87::Div(above, range)));
	const double span = x87::Sub(rule.alphaMax, rule.alphaMin);
	const float alpha = x87::Store(x87::Add(x87::Mul(span, t), rule.alphaMin));
	return static_cast<uint8_t>(ToInt(alpha) & 0xFF);
}

bool maths::BeliefSpritePlaysSound(const BeliefSpriteRule& rule, int32_t amount)
{
	return amount > rule.valueAlphaMin;
}

glm::vec3 maths::BeliefSpritePosition(const BeliefSpriteRule& rule, const glm::vec3& start, float age, uint8_t random,
                                      BeliefSpriteNoise noise)
{
	// one operation per statement, each rounded to a float as the original's are
	const float t = age * rule.noiseFreq;
	const auto k = static_cast<float>(random);
	const float twice = t + t;
	// x: the noise at k + t, plus half the noise at k + 2t
	const float atX = k + t;
	const float farAtX = twice + k;
	float noiseX = noise(farAtX) * k_Half;
	noiseX = noise(atX) + noiseX;
	// z: the same further along, at 0.3k
	const float kz = k * k_ZNoiseShift;
	const float atZ = kz + t;
	const float farAtZ = kz + twice;
	float noiseZ = noise(farAtZ) * k_Half;
	noiseZ = noise(atZ) + noiseZ;
	// the wiggle grows with the age until AgeAtMaxOffset
	const float toFull = 1.0f / rule.ageAtMaxOffset;
	float wiggle = age * toFull;
	wiggle = Clamp01(wiggle) * rule.maxWiggleOffset;
	const float wiggleX = wiggle * noiseX;
	const float wiggleZ = noiseZ * wiggle;
	// the rise
	float speed = static_cast<float>(random % k_RiseSteps) * k_RiseStep;
	speed = speed + 1.0f;
	speed = speed * rule.riseSpeed;
	const float rise = speed * age;
	// each axis summed from 0, as the original adds the offset (0, rise, 0) first
	const float x = 0.0f + start.x;
	const float y = start.y + rise;
	const float z = 0.0f + start.z;
	return {x + wiggleX, 0.0f + y, z + wiggleZ};
}

uint8_t maths::BeliefSpriteAlpha(const BeliefSpriteRule& rule, uint8_t startAlpha, float age)
{
	const float fade = rule.fadeTimeEnd - rule.fadeTimeStart;
	const float toEnd = 1.0f / fade;
	float faded = 0.0f;
	if (age > rule.fadeTimeStart)
	{
		faded = age - rule.fadeTimeStart;
		faded = Clamp01(faded * toEnd);
	}
	// a - a x faded, the product first
	const auto a = static_cast<float>(startAlpha);
	float alpha = -a * faded;
	alpha = alpha + a;
	return static_cast<uint8_t>(ToInt(alpha) & 0xFF);
}

float maths::BeliefSpriteStretch(const BeliefSpriteRule& rule, float age, uint8_t random)
{
	const auto k = static_cast<float>(random);
	// sin((k / 256 x 0.3 + 1) x age x ScaleFreq + k x 0.23), the sine unrounded into the next product
	float pace = k * (1.0f / 256.0f);
	pace = pace * k_PulsePace;
	pace = pace + 1.0f;
	pace = pace * age;
	pace = pace * rule.scaleFreq;
	const float phase = k * k_PulsePhase;
	const float angle = pace + phase;
	// (sin x 0.5 + 1) x (ScaleMax - ScaleMin) + ScaleMin
	const double pulse = x87::Add(x87::Mul(x87::Sin(angle), k_Half), 1.0);
	const float range = rule.scaleMax - rule.scaleMin;
	return x87::Store(x87::Add(x87::Mul(pulse, range), rule.scaleMin));
}

bool maths::BeliefSpriteExpired(const BeliefSpriteRule& rule, float age)
{
	// a NaN age goes too
	const float end = rule.fadeTimeEnd + k_Linger;
	return !(end >= age);
}
