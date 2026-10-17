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

#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{
struct Spell;
}

namespace openblack::ecs
{
/// A forest: its centre, the empty timer and the planting attempts. Its trees are the ones whose Tree::forestId is its
/// id (the original keeps two lists sorted by distance to the centre).
struct ForestData
{
	glm::vec3 centre;
	uint16_t emptyTimer {0};
	uint16_t attempts {0};
	uint32_t created {0}; ///< creation order (the original's list is newest first)
	entt::entity bigForest {entt::null};
	bool scenic {false}; ///< The town's scenic forest (MakeScenicForest)
};
} // namespace openblack::ecs

namespace openblack::ecs::systems
{
/// The game's forests: every forest by id, each town's forests, the ids and the turn the last tree was planted
/// (the forest functions of ECS/Trees.h go through it). It also fronts the forest miracle's forests: the miracle plants
/// all its trees at once on a spiral round where it was cast, of the kinds the ground there grows, and its spell grows
/// them each turn while it lasts and withers them once it has no strength left, the forest going with its last tree.
class ForestSystemInterface
{
public:
	using Forests = std::map<uint32_t, ForestData>;
	/// Each town's forests, head first
	using TownForestLists = std::unordered_map<uint32_t, std::vector<uint32_t>>;

	virtual ~ForestSystemInterface() = default;

	/// A forest at `centre` with that id (0: the next free id; a given id raises the next one past it); its id
	[[nodiscard]] virtual uint32_t Create(uint32_t id, glm::vec3 centre) = 0;
	/// Every forest, by id
	[[nodiscard]] virtual Forests& All() = 0;
	[[nodiscard]] virtual TownForestLists& TownLists() = 0;

	/// The turn the last tree of the world was planted by a forest
	[[nodiscard]] virtual uint32_t LastTreeCreatedTurn() const = 0;
	virtual void SetLastTreeCreatedTurn(uint32_t turn) = 0;

	/// A magic tree of this forest was deleted (noted once per tree, for the forest spell to look at)
	virtual void NoteForestLostAMagicTree(uint32_t forestId) = 0;
	/// Whether the forest lost a magic tree since it was last asked; its notes go
	[[nodiscard]] virtual bool TakeForestLostAMagicTree(uint32_t forestId) = 0;
	/// No forest has lost a magic tree (every land load, with the spells)
	virtual void ClearForestsThatLostAMagicTree() = 0;

	/// The forest miracle's seed has landed and been paid for: the spell's whole forest at once, on the spiral round its
	/// cast point, a tree in each slot where one may stand, while the forest has fewer than the spell wants. Each tree's
	/// wood is worth more by its caster's tribal power. How many trees it planted: none when the spell has made its
	/// forest already.
	virtual uint32_t Plant(entt::entity spell, components::Spell& miracle, float tribalPower) = 0;
	/// Whether a forest miracle may be cast at a point: on the map, on land, no building there whose fire would be in
	/// the forest, and room for a tree
	[[nodiscard]] virtual bool CanGrowAt(glm::vec3 point) const = 0;
	/// Whether a forest miracle's forest still stands and has trees
	[[nodiscard]] virtual bool HasTrees(entt::entity spell) const = 0;
	/// A young tree of the same kind is planted near a tree of a forest, as the water miracle does by a full grown
	/// tree: only more than 40 turns after the last tree planted this way, at the first free spot round it. The tree
	/// joins the same forest. The tree planted; none when it is too soon, the tree is in no forest, or no spot is free.
	virtual std::optional<entt::entity> AddTreeNear(entt::entity tree) = 0;

	/// A land is loaded: no forests, no town lists, ids from 1, no tree planted yet. The creation order and the
	/// forests that lost a magic tree stay (ClearForestsThatLostAMagicTree empties those)
	virtual void Reset() = 0;
};
} // namespace openblack::ecs::systems
