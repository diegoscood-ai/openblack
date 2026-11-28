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

#include <optional>
#include <string_view>

#include "ECS/ComputerPlayerMind.h"

namespace spdlog
{
class logger;
}

/// openblack's debug trace of a computer mind's weighing: one line for each node it weighs, each child it weighs or
/// passes over and each set of children it finishes, and one line for the turn. Only written while switched on
namespace openblack::computer_player
{

/// A world that passes everything on to the mind's own world, and logs the weighing at the trace level as it is told
/// of it. It only reads: the mind weighs and runs as with the world alone
class NodeTrace final: public MindWorld
{
public:
	/// The nodes are named by their rows' names in the names given, or without them by the tree's own rows' names
	NodeTrace(MindWorld& world, const MindTree& tree, const MindRowNames* names, spdlog::logger& log, int player,
	          uint32_t turn);

	float Score(ScoreKind kind, ecs::components::ComputerMind& mind, uint16_t node) override;
	[[nodiscard]] bool HasScore(ScoreKind kind) const override;
	[[nodiscard]] bool HasAction(ActionKind kind) const override;
	void GiveUpSteps() override;
	bool RunAction(ActionKind kind, ecs::components::ComputerMind& mind, uint16_t node) override;
	void TimeSteps() override;
	void ActionMissing(ActionKind kind, uint16_t node) override;
	void ActionRan(ActionKind kind, uint16_t node, bool done) override;

	[[nodiscard]] bool TellsWeighing() const override { return true; }
	void Weighed(const NodeWeighed& weighed) override;
	void ChildWeighed(uint16_t parent, uint16_t child, float worth, bool best) override;
	void ChildNotWeighed(uint16_t parent, uint16_t child, NotWeighed why, float gate, float best) override;
	void ChildrenDone(uint16_t parent, std::optional<uint16_t> best, float worth) override;

	/// The turn's line: what the turn did, how many nodes were weighed, were worth nothing or were passed over, and the
	/// root's worth and choice
	void Summary(std::string_view done, const ecs::components::ComputerMind& mind) const;

	/// The node's name: its row's name up to its ':'
	[[nodiscard]] std::string_view Name(uint16_t node) const;

private:
	MindWorld& _world;
	MindTree _tree;
	const MindRowNames* _names;
	spdlog::logger& _log;
	int _player;
	uint32_t _turn;
	uint32_t _weighed {0};
	uint32_t _worthNothing {0};
	uint32_t _notWeighed {0};
};

} // namespace openblack::computer_player
