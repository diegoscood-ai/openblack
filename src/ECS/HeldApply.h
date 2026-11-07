/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <span>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/PotResource.h"
#include "Help/HelpProfile.h"

// An object held in the hand given to the object under the hand on the action press: whether the held class can be
// given to it, and the giving itself, for the classes a store takes. The hand sends the press, then applies it at the
// next turn's start (HandApplyToObject.cpp). Wiki: docs/wiki/hand/grab-hold-and-throw.md#giving-a-held-object.

namespace openblack::ecs::held_apply
{
/// The held classes that can be given to a store
enum class HeldKind : uint8_t
{
	Other,     ///< not given here
	Tree,      ///< a tree: its wood
	DeadTree,  ///< a dead or felled tree: its wood
	Pot,       ///< a pot or a pile, the hand's own included: its type and amount
	Animal,    ///< an animal: its food
	Fence,     ///< a fence (a static object of a fence's mesh): its wood
	GateStone, ///< a carved gate stone (a static object of the ape stone's kind): laid in the gate stone plinth
};

/// What the target is for the held object
struct TargetFacts
{
	bool storesHeldType {false};     ///< the target stores the held object's resource type
	bool sameTypePot {false};        ///< the target is a pot or a pile of the held object's resource type
	bool storagePit {false};         ///< the target is a storage pit
	bool heldIndestructible {false}; ///< the held object is indestructible
	bool gateStonePlinth {false};    ///< the target is the gate stone plinth
};

/// What the press does with the held object over the target
enum class ApplyAction : uint8_t
{
	None,         ///< not valid: the press arms the put down or the throw
	GiveToStore,  ///< the target store takes the object
	MergeIntoPot, ///< the held pot's resource is put down where it is (into the pot under it), and the pot goes
	LayInPlinth,  ///< the gate stone is laid in the plinth (a full plinth still takes it), and the stone goes
};

/// The apply result the hand reads when the object went (consumed)
inline constexpr int k_ApplyConsumed = 3;

/// The decision of the held class: a tree or a dead tree is given to a store of wood; a pot to a store of its type,
/// else into a pot of its type; an animal only to a storage pit, and a fence to a store of wood, each taken only when
/// it is not indestructible; a gate stone to the gate stone plinth; nothing else here
[[nodiscard]] ApplyAction HeldClassApply(HeldKind held, const TargetFacts& target);
/// Whether the press is valid at all (an indestructible animal or fence over its store is valid, but its apply does
/// nothing)
[[nodiscard]] bool HeldClassValid(HeldKind held, const TargetFacts& target);
/// Whether a thrown animal that hit `hit` goes into it: both available and the hit a store of food
[[nodiscard]] bool AnimalImpactTakes(bool hitAvailable, bool selfAvailable, bool hitStoresFood);
/// Whether a thrown static object that hit `hit` goes into it: a fence, not indestructible, and the hit a store of wood
[[nodiscard]] bool FenceImpactTakes(bool isFence, bool indestructible, bool hitStoresWood);
/// The interface of the player who threw a body (pot_resource::DropperOf), else none
[[nodiscard]] pot_resource::Dropper ThrowerInterface(std::optional<PlayerNames> throwerPlayer);
/// Whether a thrown pot that hit `hit` goes into it: both available, and the hit stores the pot's type or is a pot of
/// that type
[[nodiscard]] bool PotImpactTakes(bool hitAvailable, bool potAvailable, const TargetFacts& hit);
/// Whether a thrown tree or dead tree that hit `hit` goes into it as wood: the hit is a store of wood, any of them
/// (a storage pit, a building site, a worship site, a pile of wood), as the trees' impact takes it
[[nodiscard]] bool TreeImpactTakes(bool hitValid, bool hitStoresWood);
/// The apply result of an action that was done (or not)
[[nodiscard]] int ApplyResult(ApplyAction action, bool done);
/// The help event an apply to an object counts for the local interface, whatever the apply did: SACRIFICE (7) on a
/// sacrifice altar, else SUPPLY (6)
[[nodiscard]] help_profile::Event ApplyHelpEvent(bool sacrificeAltar);

/// A fixed object of the cell under a point, as the building site test reads it
struct SiteCandidate
{
	bool multiCell {false};    ///< a building-like fixed object, filed in every cell its shape touches
	bool built {true};         ///< it is built
	bool repaired {true};      ///< it is repaired
	bool hasShape {false};     ///< it has collide data
	bool shapeTouches {false}; ///< its collide data meets the point's circle
};

/// The radius of the circle the building site test puts at the point, in metres
inline constexpr float k_BuildingSitePointRadius = 0.3f;

/// Which of a cell's fixed objects, in the list's order, a point is within the building site of: a building-like object
/// that is not both built and repaired, whose collide data meets the point's circle or that has none. The last such one
/// in the list; none when there is none
[[nodiscard]] std::optional<size_t> BuildingSiteAmong(std::span<const SiteCandidate> fixed);
/// Whether a held object of the class let go over the map looks for a building site under its own place before it is
/// put down or thrown: a tree, a dead or felled tree, a pot or a pile, and a fence do; an animal, a gate stone and
/// every other class are only put down or thrown
[[nodiscard]] bool ReleaseLooksForBuildingSite(HeldKind held);
/// Whether an object let go over the map that looks for a building site goes to the one under it, before it is put
/// down or thrown: there is a site and it stores the object's resource (a pot's own, wood for the others)
[[nodiscard]] bool ReleasedGoesToSite(bool siteFound, bool siteStoresHeldType);

/// The held object's class
[[nodiscard]] HeldKind KindOfHeld(entt::entity held);
/// The facts of `target` for `held`
[[nodiscard]] TargetFacts FactsFor(entt::entity held, entt::entity target);
/// Whether the press over `target` gives it `held`
[[nodiscard]] bool ValidToApplyThisToObject(entt::entity held, entt::entity target);
/// Whether `target` is a sacrifice altar: only a worship totem is
[[nodiscard]] bool IsSacrificeAltar(entt::entity target);
/// Gives `held` to `target` for the interface `dropper`: `leaveHand` runs once it is sure the object goes (it takes the
/// object out of the hand), then the target takes it. Returns the apply result: k_ApplyConsumed when it went, 0 when
/// nothing was done (the object is still held)
int ApplyThisToObject(entt::entity held, entt::entity target, const pot_resource::Dropper& dropper,
                      const std::function<void()>& leaveHand);
/// The building or building-like object whose building site the point (x, z) is within (BuildingSiteAmong over the
/// fixed list of the point's cell); none off the map or without the map cells
[[nodiscard]] entt::entity BuildingSiteAt(glm::vec3 position);
/// The building site that takes `held` if it is let go now: the one under its own place (not the hand's point), for a
/// class that looks for one, when it stores the object's resource; none otherwise
[[nodiscard]] entt::entity BuildingSiteTakingReleased(entt::entity held);
/// An object let go over the map: the building site under its own place takes it when it stores its resource
/// (BuildingSiteTakingReleased; `leaveHand` runs first, as for a store's gift). Returns k_ApplyConsumed when it went,
/// 0 when the object is to be put down or thrown as usual
int ApplyHeldToMapCoord(entt::entity held, const pot_resource::Dropper& dropper, const std::function<void()>& leaveHand);
} // namespace openblack::ecs::held_apply
