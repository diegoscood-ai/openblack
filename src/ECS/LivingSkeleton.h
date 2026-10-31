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

#include <entt/entity/fwd.hpp>

/// The skeleton state of a living thing, as the SET_SKELETON and IS_SKELETON script commands see it. A villager made a
/// skeleton wears the skeleton's model (villager::SetSkeleton); an animal only keeps the mark, which its hunters read. Any
/// other thing has no skeleton state: setting it does nothing and it is never one.
namespace openblack::ecs::living_skeleton
{

/// The skeleton mark in a living thing's status bits
inline constexpr uint16_t k_SkeletonBit = 0x40;

/// The status bits with the skeleton mark set or cleared, every other bit kept
[[nodiscard]] constexpr uint16_t WithSkeleton(uint16_t status, bool skeleton)
{
	return static_cast<uint16_t>((status & ~k_SkeletonBit) | (skeleton ? k_SkeletonBit : 0));
}

/// Whether the status bits carry the skeleton mark
[[nodiscard]] constexpr bool HasSkeleton(uint16_t status)
{
	return (status & k_SkeletonBit) != 0;
}

/// Makes a villager or an animal a skeleton, or itself again; anything else is left as it is
void Set(entt::entity thing, bool skeleton);

/// Whether the thing is a villager or an animal marked a skeleton
[[nodiscard]] bool Is(entt::entity thing);

} // namespace openblack::ecs::living_skeleton
