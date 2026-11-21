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

#include <array>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The two lanterns at a temple's door: town lamps that stand beside the entrance with a village light each, made the
// first time the temple is drawn more than a little built and drawn with it from then on, until a little after the
// explosion of the temple being destroyed. They are only models, never found under the cursor. (wiki:
// docs/wiki/world/day-and-night.md#night-lights)

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct Transform;
}

namespace openblack::ecs::systems
{
class VillageLightSystemInterface;
}

namespace openblack::worship::citadel_lanterns
{
/// The lamps of a temple
constexpr size_t k_Count = 2;

/// Where each lamp stands in the temple's frame: (across, along) its right and forward axes, on the temple's level
constexpr std::array<glm::vec2, k_Count> k_Offsets {{{-8.85f, -10.34f}, {-2.3f, -13.4f}}};

/// The lamps are made and drawn only once the temple is drawn more than this much built
constexpr float k_LitPercent = 0.07f;

/// Whether a temple drawn this much built has its lamps
[[nodiscard]] constexpr bool Lit(float drawPercent) noexcept
{
	return drawPercent > k_LitPercent;
}

/// Where lamp `lamp` stands: along the temple's forward axis, then across its right axis, then the temple's position,
/// each a float sum in that order
[[nodiscard]] glm::vec3 Position(const ecs::components::Transform& heart, size_t lamp);

/// One drawn frame of the world for a temple: with its entrance there and the temple drawn more than a little built,
/// the lamps are made on the first such frame, each with its light (`lights` may be null in tests), and drawn; else,
/// and from a little after the explosion of the temple being destroyed, they are not drawn
void Draw(ecs::Registry& registry, entt::entity heart, ecs::systems::VillageLightSystemInterface* lights);

/// Every temple, newest first, once a drawn frame of the world
void DrawAll(ecs::Registry& registry, ecs::systems::VillageLightSystemInterface* lights);

/// A temple going away takes its lamps and their lights with it at once
void Delete(ecs::Registry& registry, entt::entity heart, ecs::systems::VillageLightSystemInterface* lights);
} // namespace openblack::worship::citadel_lanterns
