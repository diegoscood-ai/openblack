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
#include <glm/vec3.hpp>

#include "ECS/DrawList/BlockCull.h"

// The original's object draw list, the part that runs after the visible blocks are sorted: when the list is rebuilt,
// how it is filled from the blocks' object arrays, and which entries get their Draw called on a frame. Pure: every
// input is a value or a fake-able interface, and nothing here reads the locator.

namespace openblack::ecs::draw_list
{
/// The most entries the list holds. A full list skips later objects; it does not stop the walk
constexpr size_t k_MaxEntries = 2999;
/// The original's vanish distance: a block at or beyond it ends the walk. Only a script command changes it
constexpr float k_VanishObjectDistance = 100000.0f;
/// A camera that moved by no more than this (squared, the eye or the focus) since the last stored pass is still
constexpr float k_StillDistanceSquared = 1.0f;
/// With no other trigger, the list is rebuilt once the turn is more than this past the last rebuild's turn
constexpr uint32_t k_RebuildTurns = 10;

/// The rebuild state. It lives as long as the game. The last rebuild's turn, the rebuilt mark and the stored pair are
/// never reset, not even when a land is loaded; clearing the map sets `count` to 1 (and empties the list), which is the
/// caller's write
struct RebuildState
{
	/// Rebuilds still asked for: each frame that rebuilds takes one. A 2 gives two rebuilds in a row
	uint8_t count = 1;
	/// The turn of the last rebuild
	uint32_t lastTurn = 0;
	/// The list was rebuilt this frame and the full pass has not run yet. True before the first frame
	bool rebuiltThisFrame = true;
	/// The eye and the focus at the last pass that a camera move made full
	glm::vec3 p {};
	glm::vec3 f {};
};

/// Whether this frame rebuilds the list: rebuilds are pending, the turn is past the last rebuild's by more than
/// k_RebuildTurns (in unsigned arithmetic), or the land pass raised its flag
[[nodiscard]] bool RebuildDue(const RebuildState& state, uint32_t turn, bool landFlag);

/// Starts a rebuild: takes one pending rebuild, if any, marks the frame as rebuilt and stores the turn
void StartRebuild(RebuildState& state, uint32_t turn);

/// Whether the eye or the focus moved by more than k_StillDistanceSquared since the stored pass. A NaN distance
/// counts as still
[[nodiscard]] bool CameraMoved(const RebuildState& state, glm::vec3 eye, glm::vec3 focus);

/// Stores the eye and the focus of a pass that a camera move made full. Not called on a rebuild frame, so drift adds
/// up against the stored pair
void StorePass(RebuildState& state, glm::vec3 eye, glm::vec3 focus);

/// Which pass this frame runs: full after a rebuild, or when the camera moved (the pair is then stored); still
/// otherwise. A full pass clears the rebuilt mark
[[nodiscard]] bool TakeFullPass(RebuildState& state, glm::vec3 eye, glm::vec3 focus);

/// Whether an object joins the list: the list is not full, and the object is not listed already, is available and is
/// not marked DontDraw
[[nodiscard]] bool TryAdd(size_t count, bool listed, bool available, bool dontDraw);

/// What the last full pass found for an entry
enum class Active : uint8_t
{
	/// Not drawn by the still pass
	No,
	/// Drawn by the still pass
	Yes,
	/// The object has no ported Draw, so whether the original would draw it in the still pass is not known. The still
	/// pass does not draw it
	NotPorted,
};

/// The list: the entries in walk order (null for an entry the passes nulled) and, beside each, what its last full
/// pass found
struct List
{
	std::vector<entt::entity> entries;
	std::vector<Active> active;
};

/// The objects as the list sees them. Each query must answer for any entity, null and destroyed ones included, and
/// none of them may change what another returns
class ObjectProbe
{
public:
	virtual ~ObjectProbe() = default;
	/// The object still exists and is not unavailable
	[[nodiscard]] virtual bool Available(entt::entity entity) const = 0;
	/// The object's 3D object is marked DontDraw (false when it has none)
	[[nodiscard]] virtual bool DontDraw(entt::entity entity) const = 0;
	/// The object's 3D object is a human or a complex one (false when it has none)
	[[nodiscard]] virtual bool IsHuman(entt::entity entity) const = 0;
	[[nodiscard]] virtual bool IsComplex(entt::entity entity) const = 0;
};

/// The objects with their listed mark, which the rebuild reads and writes
class EntityProbe: public ObjectProbe
{
public:
	/// The object's listed mark, which stays with the object, not with the list
	[[nodiscard]] virtual bool Listed(entt::entity entity) const = 0;
	/// May be called for an entity that no longer exists, which does nothing
	virtual void SetListed(entt::entity entity, bool listed) = 0;
};

/// The objects of each land block, as the map files them
struct BlockArraysView
{
	/// For each land block (an index into the block states), its slot in the 32 x 32 table: (block x << 5) + block z
	std::span<const uint16_t> slotOfBlock;
	/// The table, one array per slot. An empty span stands for a slot with no array
	std::span<const std::span<const entt::entity>> slots;
};

/// An entry's Draw. It returns the Draw's on-screen flag, or nullopt for an object that has no ported Draw. The flag is
/// set to true before every Draw, so a Draw that returns before its on-screen test (hidden, nothing to draw) must
/// return true
using Consumer = std::function<std::optional<bool>(entt::entity)>;

/// Rebuilds the list (after StartRebuild). Old entries lose their listed mark (nulled ones keep it). Then, for each
/// visible block nearest first until one at or beyond `vanish`: the block's array, then the objects outside every
/// block. Their listed mark makes the outside list join once, after the first block, and not at all with no block
void Collect(std::span<const uint16_t> visible, std::span<const BlockState> blocks, BlockArraysView arrays,
             std::span<const entt::entity> global, EntityProbe& probe, float vanish, List& out);

/// The full pass: every entry's Draw, nulling unavailable ones, and each entry's Active from its flag (or a human or
/// complex 3D object)
void FullPass(List& list, const ObjectProbe& probe, const Consumer& consumer);

/// The still pass: the Draw of the entries the last full pass found active, nulling unavailable ones. Active is not
/// recomputed
void StillPass(List& list, const ObjectProbe& probe, const Consumer& consumer);
} // namespace openblack::ecs::draw_list
