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
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"

/// What a creature does when the first search of a walk finds it shut in: the search ends with no way there while the
/// destination is farther than the search got. The creature stands, keeps the destination, and clears one of the things
/// that the search met: it throws the thing, kicks it or casts an angry miracle at it, as its player's order. Once that is
/// done it walks on to the kept destination. While it is shut in, its routes keep clear of things by its drawn size, its
/// walks search no further at first, and the area that keeps it does not call it back.
/// Pure rules, tested on their own.
namespace openblack::creature_clear_way
{
/// The search notes at most this many of the things that it meets
inline constexpr size_t k_MostMet = 16;
/// One time in this many the creature clears any of the things that the search met, else one of the first few
inline constexpr uint32_t k_AnyOneIn = 4;
inline constexpr uint32_t k_FirstFew = 3;
/// After it clears the way it walks on to within its height of the destination, or within this many metres when its
/// height is less
inline constexpr float k_LeastWalkOnDistance = 8.0f;

/// What the creature knows of a thing in its way
struct Obstacle
{
	/// The heart of a temple, a built storage pit or a town centre: never cleared
	bool templeHeart {false};
	bool storagePit {false};
	bool townCentre {false};
	/// Whether the creature can pick the thing up, and whether it can kick it
	bool canPickUp {false};
	bool canKick {false};
};

/// A miracle, in the order of the magic types: whether the creature knows it, the desire it serves and the creature
/// action that casts it (0 for none)
struct Miracle
{
	bool known {false};
	uint32_t desire {0};
	uint32_t action {0};
};

/// The actions of the game's table that throw a thing and kick it
struct Actions
{
	uint32_t throwAround {0};
	uint32_t kick {0};
};

/// The action that clears a thing out of the way, none when the creature cannot clear it:
/// 1. the heart of a temple, a built storage pit and a town centre are never cleared;
/// 2. a thing it can pick up is thrown;
/// 3. a thing it can kick is kicked;
/// 4. else the first miracle it knows, in the order of the magic types, with a casting action and serving its anger.
[[nodiscard]] std::optional<uint32_t> ClearingAction(const Obstacle& obstacle, std::span<const Miracle> miracles,
                                                     const Actions& actions);

/// Whether the search notes a thing that it meets, before asking whether the creature can clear it: while fewer than
/// k_MostMet are noted, a thing not noted already
[[nodiscard]] bool MayNote(std::span<const entt::entity> met, entt::entity thing);

/// The index of the thing the creature clears among the things the search noted; none, drawing nothing, when it noted
/// none. One time in k_AnyOneIn any of them, else one of the first k_FirstFew. The random function gives a whole number
/// below its argument.
[[nodiscard]] std::optional<size_t> Choose(size_t count, const std::function<uint32_t(uint32_t)>& random);

/// How near the creature walks on to the destination: its height, or k_LeastWalkOnDistance when that is more
[[nodiscard]] float WalkOnDistance(float height);

/// The walk on to the kept destination: one walk to within WalkOnDistance of it, looking where it goes
[[nodiscard]] std::vector<creature_mind::Step> WalkOn(glm::vec2 destination, float height);
} // namespace openblack::creature_clear_way
