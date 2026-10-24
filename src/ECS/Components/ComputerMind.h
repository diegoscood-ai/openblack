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

#include <array>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/PlayerMagic.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// What a computer player's mind keeps for one node of its tree (computer_player::MindNodes(), at the same place)
struct ComputerMindNode
{
	/// How much the node is held back, from 0 to 1: it drops when the node's action is done or fails, and comes back
	/// with the turns
	float suppression {1.0f};
	/// The node's weight, from its row; a script can set it
	float personality {1.0f};
	/// The game turn it was last weighed
	uint32_t lastWeighedTurn {0};
	/// The game turn it was last run
	uint32_t lastRunTurn {0};
	/// The thing the node's weighing chose, or a script gave it
	entt::entity thing {entt::null};
	/// A second thing a script gave it
	entt::entity secondThing {entt::null};
	/// The point the node's weighing chose, or a script's marker gave it; the game makes its nodes zeroed, so 0 until then
	glm::vec3 point {0.0f};
	/// A number and an amount the node's weighing chose for its action (an amount of food, a kind of disciple...)
	int32_t intValue {0};
	float floatValue {0.0f};
	/// What the node was last found to be worth
	float score {0.0f};
	/// The node whose action runs if this node is chosen, by its place in the tree
	std::optional<uint16_t> chosen;
};

/// Something a computer player's mind has seen round its hand: a forest, a tree or a miracle's seed
struct ComputerMindSeen
{
	/// The tree or the seed; none for a forest
	entt::entity thing {entt::null};
	/// The forest, by its number, when it is one
	std::optional<uint32_t> forest;
	/// The game turn it was first seen
	uint32_t turn {0};
};

/// The mind of a computer player, on the player's entity: its own copy of the tree with the weights a script gave it,
/// and how it feels about each player. It is made the first time a script sets it, and lasts the whole game: as a land
/// is cleared it is kept for the player's entity on the next land.
struct ComputerMind
{
	static_assert(static_cast<size_t>(PlayerNames::_COUNT) == 8, "one attitude for each of the eight players");

	/// What a mind feels about every player before a script says otherwise, as kept: twice what a script reads
	static constexpr float k_StartingAttitude = 1.0f;

	/// One for each node of the tree, at the node's place
	std::vector<ComputerMindNode> nodes;
	/// What the plan under way was worth when it was chosen
	float planScore {0.0f};
	/// The game turn a weighing last found nothing worth doing, 0 when none since one did
	uint32_t emptyWeighingTurn {0};
	/// The hand had a turn of steps: the next turn it is free, the mind rests
	bool restAfterSteps {true};
	/// The debug window's switch (openblack's): while it is on, the mind weighs nothing. Off unless ticked
	bool heldByDebug {false};
	/// How many times the weighing has found each miracle's icon, by the magic type's number. Nothing clears it: it
	/// lasts the whole game
	std::array<uint32_t, PlayerMagic::k_MagicTypes> iconsFound {};
	/// The miracles the mind has cast, which it does not cast again at the same place
	std::vector<entt::entity> spellsCast;
	/// What the mind has seen round its hand, the newest first (computer_player::ScanAroundHand); a land's clear empties it
	std::vector<ComputerMindSeen> seen;
	/// How many times its creature, still learning, helped a town, destroyed for it or impressed for it: what it is then
	/// taught. Only the mind's actions count them
	uint32_t helpLessons {0};
	uint32_t destroyLessons {0};
	uint32_t impressLessons {0};
	/// How the mind feels about each player, by the player's number, as kept: twice what a script sets
	std::array<float, static_cast<size_t>(PlayerNames::_COUNT)> attitude {
	    k_StartingAttitude, k_StartingAttitude, k_StartingAttitude, k_StartingAttitude,
	    k_StartingAttitude, k_StartingAttitude, k_StartingAttitude, k_StartingAttitude,
	};
};

} // namespace openblack::ecs::components
