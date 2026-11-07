/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>

// EmitterRuleLightningSprite's maths: the storm held in the hand emits a lightning sprite now and then, and each one
// lights the cloud it comes from for a moment. Wiki: docs/wiki/physics/particle-rules.md#the-storm-in-the-hands-flashes.

namespace openblack::particles::storm_flash
{
/// The cloud's specular once a flash is over: black, alpha 255
constexpr uint32_t k_FlashOff = 0xFF000000u;

/// The rule's emission properties, with their defaults
struct Emission
{
	float frequency {0.001f}; ///< EmissionFreq: with Randomise the mean wait between sprites is 0.625 / frequency
	int maxAtoms {-1};        ///< MaxAtoms: no sprite while the group has more atoms than this (-1: no limit)
	int maxTotal {-1};        ///< MaxTotalAtomsToEmit (-1: no limit)
	bool randomise {true};    ///< Randomise: each wait is 0.25 to 1 times 1 / EmissionFreq
};

/// The rule's data on its collection
struct State
{
	bool first {true};      ///< the first step, which starts the clock at the collection's age
	float nextTime {0.0f};  ///< the collection age the next sprite is due at
	int emitted {0};        ///< the sprites made so far
	bool striking {false};  ///< a flash is lighting the cloud
	float strikeAge {0.0f}; ///< the collection age the flash began at
};

/// Whether a sprite comes this step, at the collection's age with the step's dt and the group's atom count. The first
/// call starts the clock at that age. Nothing comes once maxTotal are made, before the due time (age + dt must pass
/// it), or while the group holds more than maxAtoms. When one comes the due time moves on by 1 / frequency, times
/// random(0.75) + 0.25 with randomise (the only draw, made only then), and the count goes up
[[nodiscard]] bool ShouldEmit(const Emission& emission, State& state, float age, float dt, size_t atomCount,
                              const std::function<float(float)>& random);

/// The cloud's specular (D3DCOLOR ARGB) at `elapsed` seconds into a flash of specLife seconds: v = trunc((1 - elapsed /
/// specLife) x 255) & 0xFF, then alpha and blue (255 v) >> 8, red and green (200 v) >> 8
[[nodiscard]] uint32_t FlashSpecular(float elapsed, float specLife);

/// The flash at the collection's age: none while no flash is on; past specLife it ends with k_FlashOff, else
/// FlashSpecular. A flash at exactly specLife gives 0 (alpha 0 too) and ends on the next step
[[nodiscard]] std::optional<uint32_t> FlashStep(State& state, float age, float specLife);
} // namespace openblack::particles::storm_flash
