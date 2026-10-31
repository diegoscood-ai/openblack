/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The fireball's rules (Rules/Fireball.cpp): what the game reads of a ball's atom once the effects have stepped.
// Wiki: docs/wiki/magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire.

#include <cstdint>

#include <glm/vec3.hpp>

namespace openblack::particles::fireball_rules
{
/// A thrown ball's direction from the hand's (a unit vector, or zero): its heading kept, pitched up 0.3 rad and kept
/// between -pi/2 and pi/2, as the game's FPU works it out
[[nodiscard]] glm::vec3 PitchedUp(const glm::vec3& d);
/// The size class of a thrown ball's whoosh (1 large, 2 medium, 3 small) from its speed: the speed over 200, more than
/// 0.6 large, more than 0.3 medium
[[nodiscard]] int32_t ThrowSoundSize(float speed);

/// Once the spells have stepped their effects: each ball gets where the hand sees it and its radius from its atom as the
/// step's post-update left it (the drawn position while the atom is drawn and not faint, the rule scale times the base
/// scale). A ball whose atom is gone is skipped
void UpdateHandTargets();

/// UpdateRuleGravityWithFloor's velocity in the wind (the climate's smoothed wind at the atom): towards the wind times
/// WindMagnification times 0.1, by Damping times the difference times dt
[[nodiscard]] glm::vec3 WindVelocity(glm::vec3 velocity, glm::vec3 wind, float windMagnification, float damping, float dt);
/// AR_FadeAlphaWithHeightAboveLandscape's alpha at `height` above the land: alphaAtZero below the land, alphaAtRef above
/// refHeight, the straight line between them in between, kept in 0..255 and truncated
[[nodiscard]] uint8_t AlphaWithHeight(float alphaAtZero, float alphaAtRef, float refHeight, float height) noexcept;
} // namespace openblack::particles::fireball_rules
