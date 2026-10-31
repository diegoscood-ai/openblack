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

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <MindFile.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureWatching.h"

/// What a creature has learnt and remembers, beyond its desires: its opinions of actions, the examples its decision
/// trees grow from, what it did lately, how it feels about other creatures, the skills and miracles it knows, and its
/// thoughts. A mind file sets all of it, and the mind can be written back to one.
namespace openblack::creature_mind_model
{
using creature_desires::k_DesireCount;

/// The two trees of each desire, in the order a mind file keeps them: how good a thing is as what a plan for the desire
/// is for (its goal: a town to help, a creature to fight), and how good a thing is to do the plan's action to
enum class TreeKind : uint8_t
{
	Goal,
	ActOn,
};
constexpr size_t k_TreeKinds = 2;
/// The kinds of lesson an example is kept with: about the desire itself, about what the plan was for, about what it
/// was done to, and about the action
enum class LessonKind : int32_t
{
	Desire,
	Goal,
	ActOn,
	Opinion,
};
/// The tree a lesson of a kind teaches: one about what a plan was for the goal tree, one about what it was done to the
/// act-on tree; the others, and a kind that is none of these, teach no tree
[[nodiscard]] std::optional<TreeKind> TreeOf(int32_t lessonKind);

/// The most thoughts kept, the newest last
constexpr size_t k_MaxThoughts = 8;

/// The game's object types the mind keeps counts of, one by one
constexpr size_t k_ObjectTypeCounts = 45;

/// What the mind keeps of one type of object, from holding them up and eating them
struct ObjectTypeCounts
{
	/// How many times it played with a thing of the type it held up to its player
	uint32_t heldPlays {0};
	/// How many of the type it has eaten
	uint32_t eaten {0};
	/// How many it found were not food: while above 0 it eats no more of the type
	uint32_t notFood {0};
};

struct Learnt
{
	/// The opinion of each action, -1 to 1, and turns since each was last done
	std::vector<float> opinions;
	std::vector<uint32_t> turnsSinceDone;
	/// Each desire's examples, by tree kind, and the trees grown from them
	std::array<std::array<std::vector<creature_tree::Episode>, k_DesireCount>, k_TreeKinds> episodes {};
	std::array<std::array<creature_tree::Tree, k_DesireCount>, k_TreeKinds> trees {};
	std::vector<creature_learning::Context> contexts;
	std::vector<creature_learning::CreatureAttitude> creatures;
	creature_watching::Knowledge knowledge;
	std::optional<creature_watching::Mimicry> mimicry;
	std::vector<std::string> thoughts;
	/// Each desire's sources' thresholds as the creature started, to tell how much it has come to like it
	std::array<std::vector<float>, k_DesireCount> initialThresholds {};
	std::u16string name;
	float alignment {0.0f};
	uint32_t developmentTimer {0};
	/// By the game's object type, what it has eaten and what it found was not food
	std::array<ObjectTypeCounts, k_ObjectTypeCounts> objectTypes {};
	/// The mind file it was loaded from, for what the file keeps that the mind doesn't use, written back as it was
	std::shared_ptr<const creaturemind::MindFileData> file;
};

/// A fresh mind's learning: no opinions, nothing seen but what is known from the start
[[nodiscard]] Learnt Fresh(const creature_desires::Desires& desires, size_t actionCount,
                           creature_watching::Knowledge knowledge);
/// The example that every new creature starts with: an enemy villager that is alive and not on fire is good to eat, a
/// lesson about what to act on
[[nodiscard]] creature_tree::Episode StartingLesson();
/// The feedback of the starting example
constexpr float k_StartingLessonFeedback = 0.8f;
/// A new creature learns the starting example in its hunger's act-on tree, only when that tree has no example yet. The
/// tree grows again with the attributes allowed. With that one example the tree is a single leaf, so every thing is
/// as good to eat as the example's step of the feedback scale (0.6) until other examples split it.
void TeachStartingLesson(Learnt& learnt, std::span<const creature_tree::Attribute> hungerAllowed);
/// Adds a thought, forgetting the oldest
void Think(Learnt& learnt, std::string thought);
/// Adds an example to a desire's tree and grows the tree again, testing the attributes allowed. An example with no kind
/// of lesson of its own is kept with the kind that teaches the tree (1 for the first, 2 for the second), as a mind file
/// saves it
void Learn(Learnt& learnt, TreeKind kind, creature_desires::Desire desire, creature_tree::Episode episode,
           std::span<const creature_tree::Attribute> allowed);
/// Grows every tree again from its examples
void RebuildTrees(Learnt& learnt, const std::array<std::vector<creature_tree::Attribute>, k_DesireCount>& allowed);

/// Everything a mind file sets
struct LoadedMind
{
	creature_desires::Desires desires;
	Learnt learnt;
	uint32_t developmentPhase {0};
	float attitudeToPlayer {0.0f};
	/// The game turns its player's camera was looking at it
	uint32_t turnsLookedAt {0};
	uint32_t speciesRow {0};
	/// The body as the file keeps it, for the body to take up
	float strength {0.0f};
	std::optional<float> size;
	std::optional<std::vector<uint8_t>> tattoo;
};
/// The mind a file holds, over a fresh mind of its species for what the file doesn't keep (how desires fade, how fast
/// sources fade, and so on)
[[nodiscard]] LoadedMind FromFile(const creaturemind::MindFileData& file, const creature_desires::Desires& fresh,
                                  const Learnt& freshLearnt);
/// A mind written as a file of the current version, over the file it was loaded from when there was one, so that
/// what the mind doesn't use is kept
[[nodiscard]] creaturemind::MindFileData ToFile(const creature_desires::Desires& desires, const Learnt& learnt,
                                                uint32_t developmentPhase, float attitudeToPlayer, uint32_t speciesRow);

} // namespace openblack::creature_mind_model
