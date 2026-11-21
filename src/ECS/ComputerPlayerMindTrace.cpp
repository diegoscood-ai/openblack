/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerPlayerMindTrace.h"

#include <array>
#include <string>

#include <spdlog/spdlog.h>

using openblack::ecs::components::ComputerMind;

namespace openblack::computer_player
{

NodeTrace::NodeTrace(MindWorld& world, const MindTree& tree, const MindRowNames* names, spdlog::logger& log, int player,
                     uint32_t turn)
    : _world(world)
    , _tree(tree)
    , _names(names)
    , _log(log)
    , _player(player)
    , _turn(turn)
{
}

float NodeTrace::Score(ScoreKind kind, ComputerMind& mind, uint16_t node)
{
	return _world.Score(kind, mind, node);
}

bool NodeTrace::HasScore(ScoreKind kind) const
{
	return _world.HasScore(kind);
}

bool NodeTrace::HasAction(ActionKind kind) const
{
	return _world.HasAction(kind);
}

void NodeTrace::GiveUpSteps()
{
	_world.GiveUpSteps();
}

bool NodeTrace::RunAction(ActionKind kind, ComputerMind& mind, uint16_t node)
{
	return _world.RunAction(kind, mind, node);
}

void NodeTrace::TimeSteps()
{
	_world.TimeSteps();
}

void NodeTrace::ActionMissing(ActionKind kind, uint16_t node)
{
	_world.ActionMissing(kind, node);
}

void NodeTrace::ActionRan(ActionKind kind, uint16_t node, bool done)
{
	_world.ActionRan(kind, node, done);
}

std::string_view NodeTrace::Name(uint16_t node) const
{
	const auto row = _tree.nodes[node].row;
	std::string_view name = _names != nullptr ? _names->Name(row) : _tree.rows[row].name;
	name = name.substr(0, name.find(':'));
	while (!name.empty() && name.back() == ' ')
	{
		name.remove_suffix(1);
	}
	return name;
}

void NodeTrace::Weighed(const NodeWeighed& weighed)
{
	++_weighed;
	if (!(weighed.worth > 0.0f))
	{
		++_worthNothing;
	}
	constexpr std::array<std::string_view, 3> k_Children {"no children", "children weighed",
	                                                      "children skipped (scored nothing)"};
	const auto parent = _tree.nodes[weighed.node].parent;
	_log.trace("Computer mind {} turn {} node {} {} under {}: score {}, weight {:.6g}, suppression {:.6g}, worth {:.6g}; "
	           "{}; chosen {}",
	           _player, _turn, weighed.node, Name(weighed.node), parent.has_value() ? Name(*parent) : "nothing",
	           weighed.scores ? fmt::format("{:.6g}", weighed.score) : std::string("none"), weighed.personality,
	           weighed.suppression, weighed.worth, k_Children.at(static_cast<size_t>(weighed.children)),
	           weighed.chosen.has_value() ? Name(*weighed.chosen) : "none");
}

void NodeTrace::ChildWeighed(uint16_t parent, uint16_t child, float worth, bool best)
{
	_log.trace("Computer mind {} turn {} child {} {} of {} {}: worth {:.6g}{}", _player, _turn, child, Name(child), parent,
	           Name(parent), worth, best ? ", the best so far" : "");
}

void NodeTrace::ChildNotWeighed(uint16_t parent, uint16_t child, NotWeighed why, float gate, float best)
{
	++_notWeighed;
	if (why == NotWeighed::NotDue)
	{
		_log.trace("Computer mind {} turn {} child {} {} of {} {}: not weighed, not due", _player, _turn, child, Name(child),
		           parent, Name(parent));
		return;
	}
	_log.trace("Computer mind {} turn {} child {} {} of {} {}: not weighed, the parent's {:.6g} below the best {:.6g}", _player,
	           _turn, child, Name(child), parent, Name(parent), gate, best);
}

void NodeTrace::ChildrenDone(uint16_t parent, std::optional<uint16_t> best, float worth)
{
	_log.trace("Computer mind {} turn {} children of {} {}: best {} {} worth {:.6g}", _player, _turn, parent, Name(parent),
	           best.has_value() ? std::to_string(*best) : std::string("-"), best.has_value() ? Name(*best) : "none", worth);
}

void NodeTrace::Summary(std::string_view done, const ComputerMind& mind) const
{
	const auto chosen = mind.nodes.empty() ? std::nullopt : mind.nodes.front().chosen;
	_log.trace("Computer mind {} turn {} nodes: {}; {} weighed ({} worth nothing), {} not weighed; root worth {:.6g}, "
	           "chosen {}",
	           _player, _turn, done, _weighed, _worthNothing, _notWeighed, mind.nodes.empty() ? 0.0f : mind.nodes.front().score,
	           chosen.has_value() ? Name(*chosen) : "none");
}

} // namespace openblack::computer_player
