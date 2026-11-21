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
#include <optional>
#include <span>
#include <string>
#include <vector>

/// What a creature learns about the things it acts on. For each desire it keeps two decision trees: which things to act
/// on to satisfy it, and which things to use doing so. Each tree learns from the latest examples of things it acted on
/// and the feedback it had for them, and is grown again from scratch after every new example. A tree tests the
/// attributes of a thing (whose it is, whether it is alive, what kind of thing it is, and so on) and ends in how good the
/// thing is, from -1 to 1. Grown the C4.5 way: at each step the attribute that best separates the examples' feedback is
/// tested, until the examples left agree or no attribute helps.
namespace openblack::creature_tree
{

/// What a creature can know about a thing, as the game's decision trees test it
enum class Attribute : uint8_t
{
	/// Whose it is: the creature's own player's, nobody's, another player's
	Allegiance,
	/// Natural or made
	Origin,
	/// Whether it is alive and moves
	Animate,
	PlayerNumber,
	/// Whether it is stronger than the creature
	HarderThanMe,
	/// The species of a creature
	CreatureType,
	/// The kind of thing (the belief type)
	Type,
	Life,
	Tribe,
	TownReligiousBelief,
	TownNeedsMost,
	TownSize,
	/// The strongest desire of another creature
	DominantDesire,
	Height,
	SpellKnowledge,
	Carrying,
	ForestSize,
	VillagerJob,
	Sex,
	MobileObjectType,
	AbodeType,
	AbodeBeingBuilt,
	OnFire,
	_Count
};
constexpr size_t k_AttributeCount = static_cast<size_t>(Attribute::_Count);
/// The game's tables use this for no attribute
constexpr uint32_t k_NoAttribute = k_AttributeCount;
[[nodiscard]] const char* Name(Attribute attribute);

/// The kinds of thing a creature has beliefs about, as the game numbers them
namespace belief_types
{
constexpr uint32_t k_Town = 0;
constexpr uint32_t k_Forest = 1;
constexpr uint32_t k_Citadel = 2;
constexpr uint32_t k_Abode = 3;
constexpr uint32_t k_CitadelPart = 4;
constexpr uint32_t k_Tree = 5;
constexpr uint32_t k_Villager = 6;
constexpr uint32_t k_Animal = 7;
constexpr uint32_t k_Creature = 8;
constexpr uint32_t k_Spell = 10;
constexpr uint32_t k_Field = 11;
constexpr uint32_t k_Rock = 12;
constexpr uint32_t k_Poo = 13;
constexpr uint32_t k_MagicFood = 14;
constexpr uint32_t k_Feature = 15;
constexpr uint32_t k_Pot = 16;
constexpr uint32_t k_SpellIcon = 17;
constexpr uint32_t k_SpellSeed = 18;
constexpr uint32_t k_MobileObject = 19;
constexpr uint32_t k_Other = 21;
constexpr uint32_t k_Flock = 22;
constexpr uint32_t k_TotemStatue = 23;
} // namespace belief_types
/// What a kind of thing is called, for the debug readouts
[[nodiscard]] const char* BeliefName(uint32_t type);

/// The attributes a kind of thing has, in the order the game lists them in its mind files: seven every thing has, and
/// four more for towns, abodes, villagers and creatures, one more for forests
[[nodiscard]] std::span<const Attribute> SlotsFor(uint32_t beliefType);
/// Whether every kind of thing has the attribute: the seven attributes before the ones only some kinds have
[[nodiscard]] bool IsCommon(Attribute attribute);
/// How many values each attribute has, by attribute: a tree's test of the attribute has a branch for each. The "on fire"
/// attribute is tested with the test of the dominant desire, so it has the range of the dominant desire. A mobile
/// object type and an abode type each have one value for each bit of a 15 bit number.
constexpr std::array<uint32_t, k_AttributeCount> k_ValueRanges {3,  3, 2,  8,  2, 17, 24, 2,  9,     4, 17, 3,
                                                                41, 4, 43, 45, 3, 17, 2,  32, 32768, 2, 41};

/// What the creature knew of a thing: its kind and the values of the attributes it has
struct Belief
{
	uint32_t type {belief_types::k_Other};
	std::array<std::optional<uint32_t>, k_AttributeCount> values {};

	[[nodiscard]] std::optional<uint32_t> Value(Attribute attribute) const { return values.at(static_cast<size_t>(attribute)); }
	void Set(Attribute attribute, uint32_t value) { values.at(static_cast<size_t>(attribute)) = value; }
};
/// A belief from the values listed in its kind's order, as mind files keep them
[[nodiscard]] Belief FromSlots(uint32_t type, std::span<const uint32_t> slots);
/// A belief's values in its kind's order, missing ones as 0
[[nodiscard]] std::vector<uint32_t> ToSlots(const Belief& belief);
/// Whether a belief keeps the value of an attribute from the first time the creature saw the thing. The kind of thing,
/// its origin, whether it lives, its player number, the job, sex and tribe of a villager, the size of a town and the
/// kind of a building or a mobile object do not change after the first sight. All other attributes change with the
/// thing.
[[nodiscard]] bool KeptFromFirstSight(Attribute attribute);
/// A belief about a thing as the creature knows it now. The kind of thing and the attributes that do not change come from
/// the first sight. The other attributes of the first sight take their values now, when the thing has them now.
[[nodiscard]] Belief WithFirstSight(const Belief& firstSight, const Belief& now);

/// One example: a thing and the feedback for acting on it, from -1 to 1
struct Episode
{
	Belief belief;
	float feedback {0.0f};
	/// What a mind file keeps with an example and the trees don't use: the kind of lesson, the action, and where the
	/// thing was
	std::array<int32_t, 4> saved {};
	/// The desire a mind file gave the example, written back as it is; none for an example learnt in play, which is
	/// written with its tree's desire
	std::optional<int32_t> savedDesire;
};
/// A tree keeps at most this many examples
constexpr size_t k_MaxEpisodes = 16;
/// Adds an example; when the list is full the newest goes first, so the first examples learnt always stay
void AddEpisode(std::vector<Episode>& episodes, Episode episode);

/// The feedback is sorted into 11 steps from -1 to 1, each the first whose middle is within a quarter of it
constexpr std::array<float, 11> k_Buckets {-1.0f, -0.8f, -0.6f, -0.4f, -0.2f, 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
constexpr size_t k_NeutralBucket = 5;
/// The first step within a quarter of the feedback; none for feedback further than a quarter from each step. Feedback
/// that is not a number is within a quarter of the first step, as the game's test finds it.
[[nodiscard]] std::optional<size_t> StepOf(float feedback);
/// The step of the feedback; feedback further than a quarter from each step takes the nearest end step
[[nodiscard]] size_t BucketOf(float feedback);
/// The base 2 logarithm of a share of the examples, as the game finds it: the base 10 logarithm times a constant a
/// little less than the one that changes the base, rounded to a float
[[nodiscard]] float ShareLog2(float share);
/// The mean feedback of the examples, given oldest first. The sum starts with the newest example, as the game keeps its
/// list newest first and adds it up in that order, one float step at a time.
[[nodiscard]] float MeanFeedback(std::span<const Episode> episodes);
/// How mixed the examples' feedback is: two parts how mixed its signs are and one how mixed its steps are, in bits, times
/// the game's float of a third. Feedback further than a quarter from each step is in no step.
[[nodiscard]] float Entropy(std::span<const Episode> episodes);
/// How much testing an attribute separates the examples, against how much it splits them; 0 if any example lacks it.
/// Only the values in the attribute's range make parts: an example with a value out of the range is in no part.
[[nodiscard]] float Gain(std::span<const Episode> episodes, Attribute attribute);

struct Node
{
	/// The attribute tested, or none at a leaf
	std::optional<Attribute> test;
	/// For each value of the attribute the examples had, the node it leads to
	std::vector<std::pair<uint32_t, size_t>> children;
	/// How many values the tested attribute has. A value below this with no child is a branch with no examples: it
	/// takes the step of this node.
	uint32_t branches {0};
	/// The step of the feedback scale of the examples that reached it: a leaf's value, and the value of each branch
	/// with no examples below it
	size_t bucket {k_NeutralBucket};
	/// How many examples reached it
	size_t examples {0};
};
struct Tree
{
	/// The root first; empty for a tree with no examples
	std::vector<Node> nodes;
};
/// Grows a tree from examples. The root can test only the allowed attributes that every kind of thing has. Below a test
/// of the kind of thing, each branch can also test the allowed attributes of its own kind.
[[nodiscard]] Tree Build(std::span<const Episode> episodes, std::span<const Attribute> allowed);
/// Where a thing ends in a tree
struct Leaf
{
	/// The node whose step the thing takes
	size_t node {0};
	/// The thing went down a branch with no examples below the node
	bool emptyBranch {false};

	[[nodiscard]] bool BelowRoot() const { return node != 0 || emptyBranch; }
	bool operator==(const Leaf& other) const = default;
};
/// The leaf a thing reaches in the tree; none when the tree is empty, the thing lacks a tested attribute, or its value
/// is out of the attribute's range
[[nodiscard]] std::optional<Leaf> LeafOf(const Tree& tree, const Belief& belief);
/// How good a thing is by the tree, from -1 to 1; 0 when the tree has nothing to say about it
[[nodiscard]] float Evaluate(const Tree& tree, const Belief& belief);
/// Nothing is more useful than this, whatever it is learnt to be worth
constexpr float k_MostUsefulness = 0.125f;
/// How useful a thing is for the planner, from how good it is: 0.1 for things nothing is known about, rising to
/// k_MostUsefulness, which a little better than nothing known already reaches, and falling to 0 for the worst; 0 for no
/// number at all
[[nodiscard]] float Usefulness(float utility);
/// The tree as lines of text, for the debug readouts
[[nodiscard]] std::vector<std::string> Describe(const Tree& tree);

/// The value of the life attribute of a thing with a life (a villager, an animal, a building): high above a fifth of
/// its life, else low (a life that is not a number is low)
[[nodiscard]] uint32_t LifeValue(float life);
/// The value of the "harder than me" attribute of another creature: 1 unless the creature of the believer's player is
/// drawn at least as tall (a height that is not a number is taller); 0 when that player has no creature
[[nodiscard]] uint32_t HarderThanMeValue(float height, std::optional<float> playersCreatureHeight);
/// The value of the height attribute of another creature, by the size it is drawn at: 0 below a quarter (or a size
/// that is not a number), 1 below three quarters, 2 below one and a quarter, else 3
[[nodiscard]] uint32_t CreatureHeightValue(float drawnSize);
/// The value of the "being built" attribute of a building: 0 while it goes up, 1 when it is built
[[nodiscard]] uint32_t BeingBuiltValue(bool built);
/// The value of the job attribute of a villager: the town desire the state it is in serves, at most the last value,
/// which a state that serves none (-1) also gives
[[nodiscard]] uint32_t VillagerJobValue(int32_t servedDesire);
/// The desire that another creature shows: the desire that the action of its plan shows, at most the last desire value
[[nodiscard]] uint32_t DominantDesireValue(uint32_t shownDesire);
/// The miracle knowledge of another creature: how many miracles it knows, at most the last value
[[nodiscard]] uint32_t SpellKnowledgeValue(size_t miraclesKnown);
/// The object type that the carrying attribute keeps when the creature carries no thing
constexpr uint32_t k_CarriesNothing = 29;
/// What another creature carries: the object type of the thing in its hand, or k_CarriesNothing, at most the last value
[[nodiscard]] uint32_t CarryingValue(std::optional<uint32_t> objectType);

/// Where a thing comes from, as the origin attribute keeps it
constexpr uint32_t k_OriginNatural = 0;
constexpr uint32_t k_OriginMadeByPeople = 1;
constexpr uint32_t k_OriginMagic = 2;
/// The player number a thing of no player has, the neutral player's
constexpr uint32_t k_NeutralPlayerNumber = 7;
/// Whose a thing is to a believer: the believer's own (0) when the thing's player is the believer's player, else
/// another's (1). A thing of no player is another's to every believer that has a player
[[nodiscard]] uint32_t AllegianceValue(std::optional<uint32_t> thingPlayer, std::optional<uint32_t> believerPlayer);
/// The number of a thing's player; the neutral player's for a thing of no player, and no higher than it
[[nodiscard]] uint32_t PlayerNumberValue(std::optional<uint32_t> thingPlayer);
/// What a believer knows of a thing that is not a creature and has only the attributes every thing has: whose it is,
/// where it comes from, whether it lives, its player's number, never harder than the believer, the first species, and
/// its kind
[[nodiscard]] Belief CommonBelief(uint32_t type, std::optional<uint32_t> thingPlayer, std::optional<uint32_t> believerPlayer,
                                  uint32_t origin, bool animate);

} // namespace openblack::creature_tree
