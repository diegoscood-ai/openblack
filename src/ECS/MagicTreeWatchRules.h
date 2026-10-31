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

/// How a villager looks at the trees of a forest miracle. It goes from tree to tree of the forest, in the forest's own
/// order, from one picked at random. At each tree it turns to the tree and looks at it for a while. After it has looked
/// at about half to all of the trees, it goes back to what it was doing. The rules here are pure, free of the game.
namespace openblack::ecs::magic_tree_watch
{

/// The range of the roll for the first tree the villager looks at: one less than the number of trees, so the last
/// tree is never the first. Unsigned, as the game takes it: a forest with no trees rolls over the whole range
[[nodiscard]] constexpr uint32_t FirstTreeRollRange(uint32_t trees)
{
	return trees - 1;
}

/// The range of the roll of the trees that the villager will not look at: half the trees, rounded down
[[nodiscard]] constexpr uint32_t SkippedRollRange(uint32_t trees)
{
	return trees >> 1u;
}

/// Whether the villager looks at one more tree: it has looked at fewer trees than all the trees less the roll of the
/// ones it will not look at. The roll is made again before each tree
[[nodiscard]] constexpr bool LooksAtAnother(uint32_t looked, uint32_t trees, uint32_t skippedRoll)
{
	return looked < trees - skippedRoll;
}

/// The number of the tree after a tree, back to the first after the last
[[nodiscard]] constexpr uint32_t NextTree(uint32_t tree, uint32_t trees)
{
	return (tree + 1) % trees;
}

/// How long the villager looks at a tree, in 8 bits: the roll, less the turns of minus two seconds, so two seconds
/// more
[[nodiscard]] constexpr uint8_t LookTurns(uint32_t roll, int32_t minusTwoSecondsTurns)
{
	return static_cast<uint8_t>(static_cast<uint8_t>(roll) - static_cast<uint8_t>(minusTwoSecondsTurns));
}

/// The look's count of turns, as the villager keeps it: the turns left in the low 8 bits, all the turns in the high
[[nodiscard]] constexpr uint16_t LookCounter(uint8_t turns)
{
	return static_cast<uint16_t>((static_cast<uint32_t>(turns) << 8u) | turns);
}

/// Whether the villager changes how it looks at the tree on this turn: when the turns left are half of all the turns
[[nodiscard]] constexpr bool ChangesLookNow(uint16_t counter)
{
	const auto left = static_cast<uint8_t>(counter & 0xFFu);
	const auto all = static_cast<uint8_t>(counter >> 8u);
	return left == static_cast<uint8_t>(all >> 1u);
}

/// The look's count after a turn: one turn fewer left. The turns left are 8 bits, so 0 becomes 255
[[nodiscard]] constexpr uint16_t CountDown(uint16_t counter)
{
	const auto left = static_cast<uint8_t>((counter & 0xFFu) - 1u);
	return static_cast<uint16_t>((counter & 0xFF00u) | left);
}

/// Whether the look is over: no turns left
[[nodiscard]] constexpr bool LookOver(uint16_t counter)
{
	return (counter & 0xFFu) == 0;
}

} // namespace openblack::ecs::magic_tree_watch
