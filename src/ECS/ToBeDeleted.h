/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

/// The common "this object goes" per class (the physics' deletion, a sunk animal, a drowned villager...): the class's
/// own clean-up, then out of the physics and out of the registry.
///
/// - a villager: villager::ToBeDeletedOverride (its dependants, then it stops reacting; ECS/Villager/VillagerDeath.h);
/// - an animal: out of its town's list, its town cleared, then out of its flock (animal_ai::Forget); prey and hunter
///   links are not cleared (their readers ask IsAvailable);
/// - a pot or pile: the reactions it started go (animal_ai::RemovePotReaction);
/// - a miracle: its own part (magic::SpellToBeDeleted: out of the miracles' list, its class's part, a shield's walls
///   set dying, then the base's), then the common mark below;
/// - a tree: its deletion listeners told, then out of its forest's two lists and so out of the game's tree list
///   (MarkTreeDeleted, Trees.h);
/// - every object: the rest; its fire is deleted after the mark, trees and dead trees too.
///
/// Then: already unavailable, nothing; `now`, deleted at once; else marked Unavailable and put at the head of the dead
/// list, freed by ProcessDeadList; its physics body stays until the physics' turn update drops the unavailable ones.
/// Every game thing's class waits so, its readers made to ask IsAvailable, except the livings (villagers, animals,
/// creatures): while the deferral is off (SetDeferredDeletion) one of those goes at once (out of the physics, then
/// destroyed), as before. The classes that wait are the deletion's own list (DefersClassOf, ToBeDeleted.cpp). A tree
/// waits too: its unlinking (MarkTreeDeleted, Trees.h) at the mark, its destruction at the dead list's pass; deleted
/// at once, it goes through DeleteTree.
void ToBeDeleted(entt::entity entity, bool now = false);

/// A creature's own things, deleted with it in the original's order after its leash comes off: the miracle it holds,
/// its two casting beams, its last swap's beam, the chain its gesture draws, the mark of where its leash sent it, then
/// its dance. Each visual goes at once, not faded. (The bookmark graphic over where its leash sent it is drawn from
/// the creature's own state each frame, so nothing of it is left)
void DeleteCreaturesOwnThings(entt::entity creature);

/// A valid entity not marked Unavailable (entt::null: false)
[[nodiscard]] bool IsAvailable(entt::entity entity);

/// From the head (the last marked first), each thing marked before this pass; the first pass only notes it, the next
/// one frees it. `drain` (closing the game, clearing the map, loading a game) frees them all, again until the list is
/// empty. The turn calls it with false. Nothing to do while the deferral is off.
void ProcessDeadList(bool drain);

/// (openblack) on once every owner's readers are zombie-safe
void SetDeferredDeletion(bool deferred);
[[nodiscard]] bool DeferredDeletion();

} // namespace openblack::ecs
