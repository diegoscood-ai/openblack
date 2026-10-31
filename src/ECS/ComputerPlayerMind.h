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
#include <span>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/ComputerMind.h"
#include "ECS/ComputerPlayerMindTree.h"

/// The rules of a computer player's mind: its copy of the tree, how it weighs the tree's nodes and runs the one it chose
namespace openblack::computer_player
{

/// A new mind: a node for each node of the tree, each with its row's weight and nothing held back
[[nodiscard]] ecs::components::ComputerMind MakeMind();

/// The shape of a tree a mind weighs: its nodes and their rows. The game's is the original's; a test gives its own
struct MindTree
{
	std::span<const MindNode> nodes;
	std::span<const MindRow> rows;
};

/// The original's tree
[[nodiscard]] MindTree GameTree();

/// What a suppression drops to when the node's action could not be done
constexpr float k_SuppressionAfterFailure = 0.01f;
/// For this many turns after a weighing found nothing worth doing, a free hand does not weigh again
constexpr uint32_t k_TurnsAfterEmptyWeighing = 30;

/// What the mind's weighing and actions read and change in the game
class MindWorld
{
public:
	virtual ~MindWorld() = default;

	/// What the node is worth by the row's way of scoring, before its weight. It may choose the node's thing and point
	virtual float Score(ScoreKind kind, ecs::components::ComputerMind& mind, uint16_t node) = 0;
	/// openblack has the way of scoring
	[[nodiscard]] virtual bool HasScore(ScoreKind kind) const = 0;
	/// openblack has the action
	[[nodiscard]] virtual bool HasAction(ActionKind kind) const = 0;
	/// The hand gives up its steps and lets go of what it holds, before a node's action runs
	virtual void GiveUpSteps() = 0;
	/// The node's action gives the hand its steps; whether it could be done
	virtual bool RunAction(ActionKind kind, ecs::components::ComputerMind& mind, uint16_t node) = 0;
	/// An action was done: the original starts a time estimate of the hand's new steps here, which only its saved games
	/// keep (nothing in play reads it)
	virtual void TimeSteps() = 0;
	/// The mind chose a node whose action openblack lacks: it is not run and nothing of the mind changes (openblack's,
	/// while the actions are being ported: the original would run it). Nothing by default
	virtual void ActionMissing(ActionKind /*kind*/, uint16_t /*node*/) {}
};

/// Before it is weighed, a node held back comes back: the rate's share for every turn since it was last weighed, up to
/// nothing held back
void Recover(ecs::components::ComputerMindNode& node, float recoveryRate, uint32_t turn);

/// The node is weighed: with a way of scoring, its score times its weight and suppression, and when that is not 0 times
/// the best of its children; without one, the best of its children, which act on the thing nearest above it. What it
/// is worth, and the node whose action it would run
float Weigh(const MindTree& tree, ecs::components::ComputerMind& mind, uint16_t node, uint32_t turn, MindWorld& world);

/// The children are weighed in their order while the node's weight times its suppression is at least the best so far.
/// A child better than every one before it, times that weight and suppression, is the best and gives the node its
/// chosen node. The node is then worth its own score times the best, or the best alone without a way of scoring
void WeighChildren(const MindTree& tree, ecs::components::ComputerMind& mind, uint16_t node, uint32_t turn, MindWorld& world);

/// The whole tree is weighed from its root: what the best plan is worth
float WeighAll(const MindTree& tree, ecs::components::ComputerMind& mind, uint32_t turn, MindWorld& world);

/// An aim is weighed while the hand is busy when its period is 0 or the turn is a multiple of it
[[nodiscard]] bool IsDue(uint32_t period, uint32_t turn);

/// While the hand is busy: each aim that is due is weighed, and the best of them, when better than 0, is the root's
/// worth and chosen node. What the best was worth (0 when none was due)
float WeighDueAims(const MindTree& tree, ecs::components::ComputerMind& mind, uint32_t turn, MindWorld& world);

/// The node's own action runs after the hand gave up its steps: done, the node is held back by its row's amount, and
/// the steps are timed; not done, it is held back to 0.01. An action openblack lacks is not run and changes nothing
bool RunAction(const MindTree& tree, ecs::components::ComputerMind& mind, uint16_t node, MindWorld& world);

/// The node runs: it keeps the turn, and its action runs, or without one its parent runs, up to the root
bool RunNode(const MindTree& tree, ecs::components::ComputerMind& mind, uint16_t node, uint32_t turn, MindWorld& world);

/// The node chosen at the node runs; nothing when it chose none
bool RunChosen(const MindTree& tree, ecs::components::ComputerMind& mind, uint16_t node, uint32_t turn, MindWorld& world);

/// The plan the root chose runs; done, it is the plan under way, with the root's worth
bool RunPlan(const MindTree& tree, ecs::components::ComputerMind& mind, uint32_t turn, MindWorld& world);

/// A new best is worth more than twice the plan under way
[[nodiscard]] bool OutbidsPlan(const ecs::components::ComputerMind& mind, float best);

/// A turn of the hand's steps (busy, or held by a script): the next free turn rests. When no script holds the hand, the
/// aims that are due are weighed, and a best worth more than twice the plan under way runs
void BusyTurn(const MindTree& tree, ecs::components::ComputerMind& mind, bool heldByScript, uint32_t turn, MindWorld& world);

/// What a free hand's mind does this turn
enum class FreeTurn : uint8_t
{
	/// It rests: a weighing found nothing worth doing in the last 30 turns, or the hand had steps last turn
	Rest,
	/// It weighs the whole tree
	Weigh,
};

/// Whether a free hand's mind rests or weighs this turn; a rest ends the rest after steps
[[nodiscard]] FreeTurn NextFreeTurn(ecs::components::ComputerMind& mind, uint32_t turn);

/// The hand a mind's turn drives
class TurnHand
{
public:
	virtual ~TurnHand() = default;

	/// The hand has steps, or a script holds it
	[[nodiscard]] virtual bool Busy() const = 0;
	/// A script holds the hand
	[[nodiscard]] virtual bool HeldByScript() const = 0;
	/// One turn of the hand's steps
	virtual void Step() = 0;
	/// The next queued action is forced, taken off the queue whatever comes of it: whether it gave the hand something
	/// to do; false with nothing queued
	virtual bool ForceNext() = 0;
};

/// What a mind's turn did, for its trace
enum class TurnDone : uint8_t
{
	/// The hand went through its steps (busy or held), and while no script held it the due aims were weighed
	Steps,
	/// A queued action was forced and gave the hand something to do
	Forced,
	/// The free mind rested
	Rested,
	/// The free mind weighed the whole tree
	Weighed,
};

/// A turn of an available mind, in the original's order: a busy or held hand goes through its steps, and then, when no
/// script holds it, the due aims are weighed and a best worth more than twice the plan under way runs; a free hand
/// forces the next queued action, and when that gave it nothing the mind rests or weighs the whole tree in the same
/// turn
TurnDone TakeTurn(const MindTree& tree, ecs::components::ComputerMind& mind, TurnHand& hand, uint32_t turn, MindWorld& world);

/// The whole tree is weighed: a plan worth more than 0 runs, else the turn is kept as the one that found nothing.
/// Whether a plan ran
bool WeighAndRun(const MindTree& tree, ecs::components::ComputerMind& mind, uint32_t turn, MindWorld& world);

// ---- a script sets the mind ----------------------------------------------------------------------------------------

/// A script sets the weight of every node that the name names, by the rows' names as they are now; whether it named any
bool SetPersonality(ecs::components::ComputerMind& mind, std::string_view name, float value,
                    const MindRowNames& names = OwnRowNames());

/// A script sets how much every node that the name names is held back; whether it named any
bool SetSuppression(ecs::components::ComputerMind& mind, std::string_view name, float value,
                    const MindRowNames& names = OwnRowNames());

/// A script sets how the mind feels about the player, which is kept as twice the value
void SetAttitude(ecs::components::ComputerMind& mind, PlayerNames toward, float value);

/// How the mind feels about the player, as a script reads it: half of what is kept
[[nodiscard]] float Attitude(const ecs::components::ComputerMind& mind, PlayerNames toward);

/// A new land: the mind keeps its tree, its weights and its attitudes, and starts again as a new mind: no weighing
/// found nothing lately, it rests its first free turn, and it forgets the miracles it cast and what it has seen
void StartNewLand(ecs::components::ComputerMind& mind);

/// The mind a player's entity takes up: the one kept from the land before, which is then no longer kept, or a new one
[[nodiscard]] ecs::components::ComputerMind TakeUp(std::optional<ecs::components::ComputerMind>& kept);

// ---- a script forces an action through the mind's nodes ------------------------------------------------------------

/// What a forced action's node is worth
constexpr float k_ForcedScore = 1.0f;

/// The thing of the node, or of the nearest node above it that has one: what a node's action acts on
[[nodiscard]] entt::entity ThingAbove(const MindTree& tree, const ecs::components::ComputerMind& mind, uint16_t node);

/// The node the node chose, none when it chose itself or nothing: where a node's action finds its point
[[nodiscard]] std::optional<uint16_t> ChosenBelow(const ecs::components::ComputerMind& mind, uint16_t node);

/// A name that begins with "DIRECT", in any case: its node's action runs at once
[[nodiscard]] bool IsDirectName(std::string_view name);

/// What a script forces an action on
struct ForcedOn
{
	entt::entity first {entt::null};
	entt::entity second {entt::null};
	/// The second thing when it has a place on the land, as a DIRECT node keeps it (none otherwise)
	entt::entity secondWithPlace {entt::null};
	/// Where the second thing is, when it is a script's marker
	std::optional<glm::vec3> marker;
};

/// The weighing under the node needs only ways of scoring that openblack has
[[nodiscard]] bool CanWeighUnder(const MindTree& tree, uint16_t node, const MindWorld& world);

/// A script forces the action named on the things, through the first node of the game's tree that the name names:
/// - a "DIRECT" name gives the node both things and runs its action at once; done, that is all;
/// - else (or when that was not done), with a marker, the node and its first child take the marker's point, and the node
///   with an action chooses that child;
/// - else the node takes the first thing, and with an action either its first child the second thing (and chooses it),
///   or as a leaf its parent the first thing and itself the second; a node with an action and children but no second
///   thing, or a node without an action, weighs its children (and a node that then chose itself refuses).
/// Its worth is 1, and the node it chose runs. Whether that was done. A force that would weigh a way of scoring openblack
/// lacks is refused and changes nothing
bool Force(ecs::components::ComputerMind& mind, std::string_view name, const ForcedOn& on, uint32_t turn, MindWorld& world,
           const MindRowNames& names = OwnRowNames());

// ---- the personality file ------------------------------------------------------------------------------------------

/// A personality file is read over the mind as the game reads it, node by node, the root first and each node before
/// its children. For each node, as many bytes as its row's name has now, and one more for the nul after it, go over
/// the row's name, then four bytes over the node's weight (a float, lowest byte first). Nothing is compared: the names
/// read are the rows' names from then on, for every mind. A file that ends early leaves what it does not reach as it
/// was, the last weight it reaches only partly written; the bytes after the last node are not read
void ReadPersonality(std::span<const uint8_t> file, ecs::components::ComputerMind& mind, MindRowNames& names);

/// The personality file of the mind as the game writes it: for each node in the same order, its row's name as it is
/// now and the nul after it, then its weight's four bytes, lowest first
[[nodiscard]] std::vector<uint8_t> WritePersonality(const ecs::components::ComputerMind& mind, const MindRowNames& names);

} // namespace openblack::computer_player
