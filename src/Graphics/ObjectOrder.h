/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <compare>
#include <cstdint>

#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

// Where each model instance falls in the frame's object draw, for the models that are blended and drawn at once (not
// depth sorted). The original draws every object as its list visits it, so such a model hides, with its depth, the
// objects drawn after it behind it, and only those drawn before it show through. Batched by mesh, the opaque models
// only need their place relative to those. Pure: values only.
namespace openblack::graphics::object_order
{

/// The part of the frame's object draw an instance is drawn in, in the frame's order
enum class Band : uint8_t
{
	/// The object list, each object as it visits it
	List,
	/// What no part of the draw claims yet: after the list, before its tail (where every such model was drawn before)
	NotPlaced,
	/// The list's tail, after its passes: the falling roots, the objects walking a script's path, the villagers'
	/// souls, the chests falling from the sky and the ground marks, in that order
	Tail,
	/// After the leashes: the physics objects
	AfterLeashes,
};

/// An instance's place in the frame's object draw: its band, the place there of the object whose draw draws it, then
/// its step inside that draw
struct Key
{
	Band band {Band::List};
	uint32_t place {0};
	uint8_t step {0};

	[[nodiscard]] friend constexpr auto operator<=>(const Key&, const Key&) = default;
};
/// The step an object's own model is drawn at; the models drawn inside its draw before it take a lower step, those
/// after it a higher one
constexpr uint8_t k_OwnStep = 2;

/// An instance row and the object it belongs to
struct Row
{
	uint32_t index;
	/// The object whose draw draws it: itself, or the owner of a model drawn inside its owner's draw
	entt::entity object;
	uint8_t step {k_OwnStep};
};

/// The objects of the frame's draw by part, each in the order it draws them
struct FrameOrder
{
	/// The objects in the order the list drew them
	std::span<const entt::entity> visited;
	/// The list's tail (TailOrder)
	std::span<const entt::entity> tail;
	/// The physics objects, in the order they are drawn after the leashes
	std::span<const entt::entity> physics;
};

/// Each instance row's key, by row: its object's band and place there (the list, then the tail, then the physics
/// objects; an object in two keeps the first), NotPlaced for an object in none and for a row of no object
[[nodiscard]] std::vector<Key> RowKeys(const FrameOrder& order, std::span<const Row> rows, size_t rowCount);
/// The same with only the list
[[nodiscard]] std::vector<Key> RowKeys(std::span<const entt::entity> visited, std::span<const Row> rows, size_t rowCount);

/// The list's tail: the falling roots, the objects walking a script's path, the villagers' souls, the chests falling
/// from the sky, then the ground marks, each as given (their lists put the newest first)
[[nodiscard]] std::vector<entt::entity> TailOrder(std::span<const entt::entity> roots, std::span<const entt::entity> walkers,
                                                  std::span<const entt::entity> souls,
                                                  std::span<const entt::entity> fallingChests,
                                                  std::span<const entt::entity> marks);

/// The segment of the opaque draw an instance with this key goes in: how many of the blended models drawn at once
/// (their keys, sorted) come before it. With none, every instance is in segment 0, the draw as it was
[[nodiscard]] size_t SegmentOf(Key key, std::span<const Key> blendedKeys);

/// The rows of the frame's model draw by the part that draws them, each 1 where it is drawn and 0 where not
struct PhysicsSplit
{
	/// The object list's draw (the opaque models and the blended ones drawn at once): drawn, not a physics object
	std::vector<uint8_t> list;
	/// The physics objects' draw after the leashes (the opaque models): drawn, a physics object not wholly sunk
	std::vector<uint8_t> physics;
	/// The depth-sorted blended models: both. A physics object's draw queues its blended model in the depth sort as an
	/// object of the list does (a thrown one-shot orb's bubble), so it leaves the queue only when wholly sunk
	std::vector<uint8_t> queued;
};

/// Splits the drawn rows (`drawn`: 0 for a row left out, every row past its end drawn) between the list and the
/// physics objects (`physics`, drawn after the leashes; `sunk`, flying wholly under the water, drawn by neither), by
/// the object each row belongs to. Neither list needs to be sorted
[[nodiscard]] PhysicsSplit SplitPhysics(std::span<const Row> rows, std::span<const uint8_t> drawn, size_t rowCount,
                                        std::span<const entt::entity> physics, std::span<const entt::entity> sunk);

/// Whether a blended model is drawn at once, among the opaque ones: neither its mesh nor its object is depth sorted
[[nodiscard]] constexpr bool DrawnAtOnce(bool meshSorted, bool hasSortPoint, bool objectSorted)
{
	return !meshSorted && !hasSortPoint && !objectSorted;
}

} // namespace openblack::graphics::object_order
