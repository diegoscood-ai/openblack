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

#include <optional>

#include <glm/vec3.hpp>

namespace openblack::particles::sprinkle
{
/// How far a tumbling atom (a falling log) turns this step: its speed |v| (summed z, y, x) times its tumble speed, kept
/// within the most it may turn when it has one, times the step
[[nodiscard]] float TumbleAngle(const glm::vec3& velocity, float tumbleSpeed, std::optional<float> maxTumbleSpeed, float dt);
/// It turns about z when it moves more along x than along z, else about x
[[nodiscard]] bool TumblesAboutZ(const glm::vec3& velocity);
/// Whether a sprinkle starts raising the hand as it pours: only the local player's own casting, with a height to raise
/// it by, and not while the effect closes
[[nodiscard]] bool StartsHandRaise(bool myInterfaceCasting, float heightToRaise, bool closing);

/// What the batch pick-up's emitter owes (ER_MultiPickup's collection data): the atoms it owes so far, a float, and
/// the atoms it has made
struct PickupEmitter
{
	float owed {0.0f};
	int32_t made {0};
};
/// One step of the emitter: it owes the step times EmitRate more, and makes atoms while it has made fewer than it
/// owes. Returns how many it makes this step
[[nodiscard]] int32_t PickupEmits(PickupEmitter& emitter, float dt, float emitRate);
/// Where a rising piece is at its age: the straight way from where it started (the land under the hand as it was
/// made) to the hand, a share age / RaiseTime of it; none once it is older than RaiseTime, when it goes
[[nodiscard]] std::optional<glm::vec3> PickupRise(const glm::vec3& start, const glm::vec3& hand, float age, float raiseTime);
} // namespace openblack::particles::sprinkle
