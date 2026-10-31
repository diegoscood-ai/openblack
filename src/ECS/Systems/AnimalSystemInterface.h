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
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{
/// The animals: what they share (the visual time of day of the turn, the death listeners and each species' own dying,
/// which ecs::animal_ai goes through) and what the rest of the game may ask about one animal or flock. Each animal's
/// own data is in its components; its turn is ecs::animal_ai's, in the living list.
class AnimalSystemInterface
{
public:
	/// A species' own dying, called with the animal
	using DeathCallback = std::function<void(entt::entity)>;

	virtual ~AnimalSystemInterface() = default;

	/// The visual time of day (hours) of this turn
	[[nodiscard]] virtual float VisualTime() const = 0;
	virtual void SetVisualTime(float hours) = 0;

	/// The species' own dying (empty: the common one)
	virtual void SetSpeciesDying(std::size_t species, DeathCallback dying) = 0;
	/// The species' own dying; null when it has none
	[[nodiscard]] virtual const DeathCallback* SpeciesDying(std::size_t species) const = 0;

	/// A new size for an animal
	virtual void SetScale(entt::entity animal, float scale) = 0;
	/// An animal's radius across the ground: the larger half of its model's width and depth, scaled
	[[nodiscard]] virtual float RadiusOf(entt::entity animal) const = 0;
	/// The flock's leader, its first animal still there, none for an empty flock
	[[nodiscard]] virtual entt::entity LeaderOf(entt::entity flock) const = 0;
	/// The flock's animals, the leader first
	[[nodiscard]] virtual std::vector<entt::entity> MembersOf(entt::entity flock) const = 0;
	/// Where an animal is heading across the land now
	[[nodiscard]] virtual glm::vec2 GoalOf(entt::entity animal) const = 0;
	/// How high above the land at its goal an animal is heading for
	[[nodiscard]] virtual float GoalHeightOf(entt::entity animal) const = 0;

	/// Whether a creature is frightened of an animal: bats, vultures, the big cats and the wolves frighten creatures, the
	/// other animals don't
	[[nodiscard]] virtual bool IsFrighteningToCreature(entt::entity animal) const = 0;
	/// Whether the player's hand may pick the animal up
	[[nodiscard]] virtual bool CanPlayerPickUp(entt::entity animal) const = 0;
	/// A bird a creature can pick up: one less than a metre above the land. For the other animals: false (pending)
	[[nodiscard]] virtual bool CanBePickedUpByCreature(entt::entity animal) const = 0;
	/// A bird a creature of that height can stamp on: one lower than a quarter of it. For the other animals: false
	/// (pending)
	[[nodiscard]] virtual bool CanBeStompedOnByCreature(entt::entity animal, float creatureHeight) const = 0;
	/// A flock as a land's script makes one (CREATE_FLOCK): its number, where it is made, its home, how far about it its
	/// leader goes (the game's own reach for none) and how close its birds keep
	virtual entt::entity CreateScriptFlock(int32_t id, glm::vec2 position, glm::vec2 home, float reach,
	                                       float flockDistance) = 0;
	/// The flock a land's script made with the number, none if there is none
	[[nodiscard]] virtual entt::entity FindScriptFlock(int32_t id) const = 0;
	/// An animal of a kind, a bird among them, made as a land's script makes one (CREATE_NEW_ANIMAL) at a point of the land,
	/// of an age (0 for its kind's chance one), in a flock or none
	virtual entt::entity CreateBird(AnimalInfo type, glm::vec2 position, uint32_t age, entt::entity flock) = 0;
};
} // namespace openblack::ecs::systems
