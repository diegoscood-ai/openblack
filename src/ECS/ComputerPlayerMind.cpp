/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerPlayerMind.h"

#include <cctype>
#include <cmath>
#include <cstdint>

#include <algorithm>
#include <bit>
#include <iterator>
#include <utility>

using openblack::ecs::components::ComputerMind;
using openblack::ecs::components::ComputerMindNode;

namespace openblack::computer_player
{

namespace
{

const MindRow& RowAt(const MindTree& tree, uint16_t node)
{
	return tree.rows[tree.nodes[node].row];
}

std::span<const uint16_t> ChildrenAt(const MindTree& tree, uint16_t node)
{
	const auto& entry = tree.nodes[node];
	return std::span<const uint16_t>(entry.children.data(), entry.childCount);
}

/// The thing of the node, or of the nearest node above it that has one
entt::entity ThingFrom(const MindTree& tree, const ComputerMind& mind, uint16_t node)
{
	std::optional<uint16_t> at = node;
	while (at.has_value())
	{
		if (mind.nodes[*at].thing != entt::null)
		{
			return mind.nodes[*at].thing;
		}
		at = tree.nodes[*at].parent;
	}
	return entt::null;
}

/// Neither more nor less than 0 (a value that is not a number is neither)
bool IsNothing(float value)
{
	return value == 0.0f || std::isnan(value);
}

} // namespace

MindTree GameTree()
{
	return MindTree {MindNodes(), k_MindRows};
}

ComputerMind MakeMind()
{
	ComputerMind mind;
	mind.nodes.reserve(k_MindNodeCount);
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		mind.nodes.push_back(ComputerMindNode {.personality = RowOf(node).personality});
	}
	return mind;
}

void Recover(ComputerMindNode& node, float recoveryRate, uint32_t turn)
{
	if (node.suppression == 1.0f)
	{
		return;
	}
	// every product rounds to a float, as at the game's 24-bit precision
	const auto turns = static_cast<float>(turn - node.lastWeighedTurn);
	float value = recoveryRate - 1.0f;
	value = value * turns;
	value = value + 1.0f;
	value = value * node.suppression;
	node.suppression = value < 1.0f || std::isnan(value) ? value : 1.0f;
}

float Weigh(const MindTree& tree, ComputerMind& mind, uint16_t node, uint32_t turn, MindWorld& world)
{
	const auto& row = RowAt(tree, node);
	Recover(mind.nodes[node], row.recoveryRate, turn);
	{
		auto& state = mind.nodes[node];
		state.chosen.reset();
		state.thing = entt::null;
		state.lastWeighedTurn = turn;
		state.score = 0.0f;
	}
	if (row.score != ScoreKind::None)
	{
		float value = world.Score(row.score, mind, node) + mind.nodes[node].score;
		auto& state = mind.nodes[node];
		// the node is its own choice whatever it scores; its children may choose better
		state.chosen = node;
		value = value * state.personality;
		value = value * state.suppression;
		state.score = value;
		if (IsNothing(value))
		{
			return value;
		}
	}
	else if (const auto parent = tree.nodes[node].parent; parent.has_value())
	{
		mind.nodes[node].thing = ThingFrom(tree, mind, *parent);
	}
	WeighChildren(tree, mind, node, turn, world);
	return mind.nodes[node].score;
}

void WeighChildren(const MindTree& tree, ComputerMind& mind, uint16_t node, uint32_t turn, MindWorld& world)
{
	const auto children = ChildrenAt(tree, node);
	if (children.empty())
	{
		return;
	}
	float best = 0.0f;
	for (const auto child : children)
	{
		// the gate: once the best is above the node's own weight, no later child is weighed
		const float gate = mind.nodes[node].personality * mind.nodes[node].suppression;
		if (gate < best || std::isnan(gate))
		{
			continue;
		}
		float value = Weigh(tree, mind, child, turn, world);
		value = value * mind.nodes[node].personality;
		value = value * mind.nodes[node].suppression;
		// strictly better only: of two equal children the first stays
		if (value > best)
		{
			best = value;
			mind.nodes[node].chosen = mind.nodes[child].chosen;
		}
	}
	auto& state = mind.nodes[node];
	state.score = RowAt(tree, node).score != ScoreKind::None ? best * state.score : best;
}

float WeighAll(const MindTree& tree, ComputerMind& mind, uint32_t turn, MindWorld& world)
{
	return Weigh(tree, mind, k_RootNode, turn, world);
}

bool IsDue(uint32_t period, uint32_t turn)
{
	return period == 0 || turn % period == 0;
}

float WeighDueAims(const MindTree& tree, ComputerMind& mind, uint32_t turn, MindWorld& world)
{
	float best = 0.0f;
	for (const auto aim : ChildrenAt(tree, k_RootNode))
	{
		const float value = IsDue(RowAt(tree, aim).period, turn) ? Weigh(tree, mind, aim, turn, world) : 0.0f;
		if (value > best)
		{
			best = value;
			mind.nodes[k_RootNode].score = value;
			mind.nodes[k_RootNode].chosen = mind.nodes[aim].chosen;
		}
	}
	return best;
}

bool RunAction(const MindTree& tree, ComputerMind& mind, uint16_t node, MindWorld& world)
{
	const auto& row = RowAt(tree, node);
	if (row.action == ActionKind::None)
	{
		return false;
	}
	if (!world.HasAction(row.action))
	{
		world.ActionMissing(row.action, node);
		return false;
	}
	world.GiveUpSteps();
	const bool done = world.RunAction(row.action, mind, node);
	if (done)
	{
		mind.nodes[node].suppression = row.suppressionAfterSuccess;
		world.TimeSteps();
	}
	else
	{
		mind.nodes[node].suppression = k_SuppressionAfterFailure;
	}
	return done;
}

bool RunNode(const MindTree& tree, ComputerMind& mind, uint16_t node, uint32_t turn, MindWorld& world)
{
	std::optional<uint16_t> at = node;
	while (at.has_value())
	{
		mind.nodes[*at].lastRunTurn = turn;
		if (RowAt(tree, *at).action != ActionKind::None)
		{
			return RunAction(tree, mind, *at, world);
		}
		at = tree.nodes[*at].parent;
	}
	return false;
}

bool RunChosen(const MindTree& tree, ComputerMind& mind, uint16_t node, uint32_t turn, MindWorld& world)
{
	const auto chosen = mind.nodes[node].chosen;
	return chosen.has_value() && RunNode(tree, mind, *chosen, turn, world);
}

bool RunPlan(const MindTree& tree, ComputerMind& mind, uint32_t turn, MindWorld& world)
{
	if (!RunChosen(tree, mind, k_RootNode, turn, world))
	{
		return false;
	}
	mind.planScore = mind.nodes[k_RootNode].score;
	return true;
}

bool OutbidsPlan(const ComputerMind& mind, float best)
{
	const float over = best - mind.planScore;
	return over > mind.planScore;
}

void BusyTurn(const MindTree& tree, ComputerMind& mind, bool heldByScript, uint32_t turn, MindWorld& world)
{
	mind.restAfterSteps = true;
	if (heldByScript)
	{
		return;
	}
	const float best = WeighDueAims(tree, mind, turn, world);
	if (OutbidsPlan(mind, best))
	{
		RunPlan(tree, mind, turn, world);
	}
}

FreeTurn NextFreeTurn(ComputerMind& mind, uint32_t turn)
{
	const bool lately = mind.emptyWeighingTurn != 0 && turn - mind.emptyWeighingTurn < k_TurnsAfterEmptyWeighing;
	if (lately || mind.restAfterSteps)
	{
		mind.restAfterSteps = false;
		return FreeTurn::Rest;
	}
	return FreeTurn::Weigh;
}

TurnDone TakeTurn(const MindTree& tree, ComputerMind& mind, TurnHand& hand, uint32_t turn, MindWorld& world)
{
	if (hand.Busy())
	{
		hand.Step();
		BusyTurn(tree, mind, hand.HeldByScript(), turn, world);
		return TurnDone::Steps;
	}
	if (hand.ForceNext())
	{
		return TurnDone::Forced;
	}
	if (NextFreeTurn(mind, turn) == FreeTurn::Rest)
	{
		return TurnDone::Rested;
	}
	WeighAndRun(tree, mind, turn, world);
	return TurnDone::Weighed;
}

bool WeighAndRun(const MindTree& tree, ComputerMind& mind, uint32_t turn, MindWorld& world)
{
	const float best = WeighAll(tree, mind, turn, world);
	if (!(best > 0.0f))
	{
		mind.emptyWeighingTurn = turn;
		return false;
	}
	const bool ran = RunPlan(tree, mind, turn, world);
	mind.emptyWeighingTurn = 0;
	return ran;
}

bool SetPersonality(ComputerMind& mind, std::string_view name, float value, const MindRowNames& names)
{
	const auto nodes = FindNodes(name, names);
	for (const auto node : nodes)
	{
		mind.nodes.at(node).personality = value;
	}
	return !nodes.empty();
}

bool SetSuppression(ComputerMind& mind, std::string_view name, float value, const MindRowNames& names)
{
	const auto nodes = FindNodes(name, names);
	for (const auto node : nodes)
	{
		mind.nodes.at(node).suppression = value;
	}
	return !nodes.empty();
}

void SetAttitude(ComputerMind& mind, PlayerNames toward, float value)
{
	mind.attitude.at(static_cast<size_t>(toward)) = value + value;
}

float Attitude(const ComputerMind& mind, PlayerNames toward)
{
	return mind.attitude.at(static_cast<size_t>(toward)) * 0.5f;
}

void StartNewLand(ComputerMind& mind)
{
	mind.emptyWeighingTurn = 0;
	mind.restAfterSteps = true;
	// the miracles it cast and what it has seen were the land's; how often it found each icon lasts
	mind.spellsCast.clear();
	mind.seen.clear();
}

ComputerMind TakeUp(std::optional<ComputerMind>& kept)
{
	if (!kept.has_value())
	{
		return MakeMind();
	}
	auto mind = std::move(*kept);
	kept.reset();
	return mind;
}

entt::entity ThingAbove(const MindTree& tree, const ComputerMind& mind, uint16_t node)
{
	return ThingFrom(tree, mind, node);
}

std::optional<uint16_t> ChosenBelow(const ComputerMind& mind, uint16_t node)
{
	const auto chosen = mind.nodes.at(node).chosen;
	return chosen == std::optional<uint16_t>(node) ? std::nullopt : chosen;
}

bool IsDirectName(std::string_view name)
{
	constexpr std::string_view k_Direct = "DIRECT";
	if (name.size() < k_Direct.size())
	{
		return false;
	}
	for (size_t index = 0; index < k_Direct.size(); ++index)
	{
		const auto letter = static_cast<unsigned char>(name.at(index));
		if (std::toupper(letter) != static_cast<unsigned char>(k_Direct.at(index)))
		{
			return false;
		}
	}
	return true;
}

bool CanWeighUnder(const MindTree& tree, uint16_t node, const MindWorld& world)
{
	for (const auto child : ChildrenAt(tree, node))
	{
		const auto score = RowAt(tree, child).score;
		if ((score != ScoreKind::None && !world.HasScore(score)) || !CanWeighUnder(tree, child, world))
		{
			return false;
		}
	}
	return true;
}

namespace
{

/// The node is forced with a marker's point (the second thing itself is not kept)
bool ForceAtPoint(ComputerMind& mind, uint16_t node, const ForcedOn& on, uint32_t turn, MindWorld& world)
{
	const auto tree = GameTree();
	const bool action = RowAt(tree, node).action != ActionKind::None;
	if (!action && !CanWeighUnder(tree, node, world))
	{
		return false;
	}
	auto& state = mind.nodes.at(node);
	state.point = *on.marker;
	state.thing = on.first;
	state.score = k_ForcedScore;
	const auto children = ChildrenAt(tree, node);
	if (!action)
	{
		WeighChildren(tree, mind, node, turn, world);
	}
	else if (!children.empty())
	{
		mind.nodes.at(children.front()).point = *on.marker;
		mind.nodes.at(node).chosen = children.front();
	}
	return RunChosen(tree, mind, node, turn, world);
}

/// The node is forced on the two things
bool ForceOnThings(ComputerMind& mind, uint16_t node, const ForcedOn& on, uint32_t turn, MindWorld& world)
{
	const auto tree = GameTree();
	const bool action = RowAt(tree, node).action != ActionKind::None;
	const auto children = ChildrenAt(tree, node);
	const bool weighs = !action || (!children.empty() && on.second == entt::null);
	if (weighs && !CanWeighUnder(tree, node, world))
	{
		return false;
	}
	mind.nodes.at(node).score = k_ForcedScore;
	mind.nodes.at(node).thing = on.first;
	if (!action)
	{
		WeighChildren(tree, mind, node, turn, world);
		if (mind.nodes.at(node).chosen == std::optional<uint16_t>(node))
		{
			return false;
		}
	}
	else if (!children.empty())
	{
		if (on.second == entt::null)
		{
			WeighChildren(tree, mind, node, turn, world);
		}
		else
		{
			const auto first = children.front();
			mind.nodes.at(first).thing = on.second;
			mind.nodes.at(first).chosen = first;
			mind.nodes.at(node).chosen = first;
		}
	}
	else if (const auto parent = tree.nodes[node].parent; parent.has_value())
	{
		mind.nodes.at(*parent).thing = on.first;
		mind.nodes.at(node).thing = on.second;
	}
	return RunChosen(tree, mind, node, turn, world);
}

} // namespace

bool Force(ComputerMind& mind, std::string_view name, const ForcedOn& on, uint32_t turn, MindWorld& world,
           const MindRowNames& names)
{
	const auto found = FindNode(name, names);
	if (!found.has_value())
	{
		return false;
	}
	const auto node = *found;
	if (IsDirectName(name))
	{
		mind.nodes.at(node).thing = on.first;
		mind.nodes.at(node).secondThing = on.secondWithPlace;
		if (RunAction(GameTree(), mind, node, world))
		{
			return true;
		}
	}
	return on.marker.has_value() ? ForceAtPoint(mind, node, on, turn, world) : ForceOnThings(mind, node, on, turn, world);
}

void ReadPersonality(std::span<const uint8_t> file, ComputerMind& mind, MindRowNames& names)
{
	// The file is read as one run of bytes: each read takes as much of what it asks for as is left
	size_t at = 0;
	const auto take = [&file, &at](size_t count) {
		const auto taken = file.subspan(at, std::min(count, file.size() - at));
		at += taken.size();
		return taken;
	};
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		const auto row = MindNodes().at(node).row;
		const auto room = names.Room(row);
		const auto name = take(names.Name(row).size() + 1);
		std::ranges::transform(name.first(std::min(name.size(), room.size())), room.begin(),
		                       [](uint8_t byte) { return static_cast<char>(byte); });
		auto& personality = mind.nodes.at(node).personality;
		auto bits = std::bit_cast<uint32_t>(personality);
		const auto weight = take(sizeof(bits));
		for (size_t index = 0; index < weight.size(); ++index)
		{
			const auto shift = 8 * index;
			bits = (bits & ~(0xFFu << shift)) | (static_cast<uint32_t>(weight[index]) << shift);
		}
		personality = std::bit_cast<float>(bits);
	}
}

std::vector<uint8_t> WritePersonality(const ComputerMind& mind, const MindRowNames& names)
{
	std::vector<uint8_t> file;
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		const auto name = names.Name(MindNodes().at(node).row);
		std::ranges::transform(name, std::back_inserter(file), [](char letter) { return static_cast<uint8_t>(letter); });
		file.push_back(0);
		const auto bits = std::bit_cast<uint32_t>(mind.nodes.at(node).personality);
		for (size_t index = 0; index < sizeof(bits); ++index)
		{
			file.push_back(static_cast<uint8_t>(bits >> (8 * index)));
		}
	}
	return file;
}

} // namespace openblack::computer_player
