/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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
} // namespace openblack::particles::sprinkle
