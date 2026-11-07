/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

// The belief sprites' maths: how opaque a sprite starts for the belief it shows, how it rises, wiggles, pulses and fades
// with its age, and when it goes. Free of state, so they are tested on their own. Wiki:
// docs/wiki/physics/particle-rules.md#the-belief-sprites.

namespace openblack::particles::maths
{

/// The belief sprite rule's properties, with the defaults a spell file that leaves them out gets
struct BeliefSpriteRule
{
	float riseSpeed {10.0f};
	float maxWiggleOffset {10.0f};
	float ageAtMaxOffset {2.0f};
	float noiseFreq {1.0f};
	float scaleMax {1.0f};
	float scaleMin {0.3f};
	float scaleFreq {1.0f};
	float fadeTimeStart {1.5f};
	float fadeTimeEnd {2.5f};
	int32_t alphaMin {20};
	int32_t alphaMax {150};
	int32_t valueAlphaMin {10};
	int32_t valueAlphaMax {100};
};

/// The noise the wiggle reads, about -1..1 (particles::noise::SignedValueNoise in the game)
using BeliefSpriteNoise = float (*)(float x);

/// The alpha a sprite starts with for the belief it shows: AlphaMin at ValueAlphaMin or less, AlphaMax at ValueAlphaMax
/// or more, in a straight line between, rounded to the nearest
[[nodiscard]] uint8_t BeliefSpriteStartAlpha(const BeliefSpriteRule& rule, int32_t amount);

/// A sprite worth more than ValueAlphaMin plays the rule's sound when it is made
[[nodiscard]] bool BeliefSpritePlaysSound(const BeliefSpriteRule& rule, int32_t amount);

/// Where a sprite is at an age: it rises from its start at the rise speed, a little faster for some random bytes, and
/// wiggles across the land with the noise, the wiggle growing from nothing to MaxWiggleOffset by AgeAtMaxOffset.
/// `random` is the sprite's own random byte
[[nodiscard]] glm::vec3 BeliefSpritePosition(const BeliefSpriteRule& rule, const glm::vec3& start, float age, uint8_t random,
                                             BeliefSpriteNoise noise);

/// The alpha at an age: the start alpha until FadeTimeStart, then fading to nothing at FadeTimeEnd
[[nodiscard]] uint8_t BeliefSpriteAlpha(const BeliefSpriteRule& rule, uint8_t startAlpha, float age);

/// The vertical stretch of a sprite drawn as a sprite at an age: a pulse between the scales, its pace and phase taken
/// from the sprite's random byte
[[nodiscard]] float BeliefSpriteStretch(const BeliefSpriteRule& rule, float age, uint8_t random);

/// A sprite goes once it is older than FadeTimeEnd and a little more
[[nodiscard]] bool BeliefSpriteExpired(const BeliefSpriteRule& rule, float age);

} // namespace openblack::particles::maths
