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

#include <algorithm>
#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Creature/CreatureDecisionTree.h"

/// Which things a creature has come across: each thing it sees is remembered by the thing's kind (by the game's belief
/// types: a forest, a temple, a flock and so on) until the thing is gone from the world, so that the next sighting of
/// it is a sighting of a thing it already knows. A creature keeps only so many things of each kind; a new one beyond
/// that takes the place of an old one. What it thinks of a thing is not kept here: that is read from the world each time
/// it is needed. Also here: how a creature's eyes sweep the map cells about it, a few cells each turn in growing rings.
/// Everything here is pure.
namespace openblack::creature_object_beliefs
{

/// The kinds of thing a creature keeps beliefs about, numbered as the game's belief types
inline constexpr uint32_t k_KindCount = 24;
/// How many beliefs of each kind a creature keeps at most: forty towns, fifty forests, twenty temples, a hundred abodes
/// and so on, one of the kind that stands for what it is doing, forty flocks
inline constexpr std::array<uint32_t, k_KindCount> k_Capacity {40,  50,  20,  100, 50,  100, 100, 100, 100, 30,  100, 40,
                                                               100, 100, 100, 100, 100, 100, 100, 100, 1,   100, 40,  20};

/// The kind of belief a loose thing of the mobile objects' class is, by the kind its row of the game's tables gives it:
/// magic food's kind is believed in as magic food, a lump of poo's as poo, and every other kind (none included) as a
/// mobile object
inline constexpr int32_t k_MagicFoodRowKind = 4;
inline constexpr int32_t k_PooRowKind = 5;
[[nodiscard]] uint32_t MobileObjectBeliefKind(std::optional<int32_t> rowKind);
/// The kind of belief a static thing that can be moved is: one of the rock class (a rock, a bonfire, a piece knocked
/// off a building) as a rock, every other (a dead tree, a magic teleport, a vortex, a plain static) as a mobile object
[[nodiscard]] uint32_t MobileStaticBeliefKind(bool rockClass);

/// One thing a creature has come across, and its kind; for another creature, the game turn it last looked it over
struct Belief
{
	entt::entity thing {entt::null};
	uint32_t kind {0};
	std::optional<uint32_t> inspected;
	/// The game turn on which seeing the thing last stirred the creature's desire sources; 0 for a new belief
	uint32_t sourcesTurn {0};
	/// The game turn on which the creature last found no way to the thing, none while it has found its way
	std::optional<uint32_t> blockedTurn;
	/// The whole thing it took the thing to belong to as it first came across it (a villager's, an abode's or a totem
	/// statue's town, an animal's flock), none for a thing that belongs to nothing or to itself
	entt::entity belongsTo {entt::null};
	/// The order the beliefs were made in: a newer belief has a larger number
	uint64_t order {0};
	/// What the creature took in of the thing when the belief was new. The attributes that do not change keep these
	/// values (creature_tree::KeptFromFirstSight); a belief whose thing is not taken in again keeps them all.
	std::optional<creature_tree::Belief> firstSight;
	/// Where the creature saw the thing last (SightingTakesInPlace), which it takes the thing to be at until it sees
	/// it again: a run from the thing goes from there and is measured to there
	std::optional<map_coords::MapCoords> place;
};

/// How long a creature keeps in mind that it found no way to a thing: ten seconds, a minute for a thing fixed in the
/// land over several cells
inline constexpr float k_BlockedSeconds = 10.0f;
inline constexpr float k_BlockedSecondsFixed = 60.0f;
/// Whether the creature found no way to the thing lately: within the seconds above, in the game's whole turns (the
/// seconds times the whole turns in a second, cut to a whole number), counted on whole numbers that may wrap. A mark
/// older than that is forgotten as it is asked, as the game forgets it
[[nodiscard]] bool BlockedRecently(Belief& belief, uint32_t turn, uint32_t turnsPerSecond, bool fixed);

/// What seeing again a thing it has a belief about stirs in a creature. Every kind of belief but an abode's takes the
/// game's common steps when the thing is seen again: a whole thing it may act on (a town, a forest, a temple, a flock or
/// another creature, not itself) stirs the wish to impress, compassion and anger from seeing things, each by a half
/// when the thing is more than a tenth useful to the desire the source drives, at most once in 200 game turns for each
/// thing; then a toy stirs the wish to play from watching the player by a half, at every sighting.
inline constexpr std::array<uint32_t, 3> k_SightingSources {1, 3, 7};
inline constexpr float k_SightingUsefulness = 0.1f;
inline constexpr float k_SightingSourceAmount = 0.5f;
inline constexpr uint32_t k_SightingSourceTurns = 200;
inline constexpr uint32_t k_ToySightingSource = 12;
inline constexpr float k_ToySightingAmount = 0.5f;
/// After those steps, a town seen again that wants something stirs compassion from seeing things that deserve it, unless
/// a script controls the creature: by one over the game turns in twenty seconds (the seconds times the whole turns in a
/// second, cut to a whole number)
inline constexpr uint32_t k_TownSightingSource = 3;
inline constexpr float k_TownSightingSeconds = 20.0f;
[[nodiscard]] bool TownSightingStirs(bool controlledByScript, bool townWantsSomething);
[[nodiscard]] float TownSightingAmount(uint32_t turnsPerSecond);
/// Whether a kind of belief takes the game's common steps when its thing is seen again: all but an abode's
[[nodiscard]] bool SeeingAgainStirs(uint32_t kind);
/// Whether a sighting of a thing puts where it stands in the belief: a new belief's always, and every belief's seen
/// again but an abode's, which keeps where it was first seen
[[nodiscard]] bool SightingTakesInPlace(bool isNew, uint32_t kind);
/// What a creature knows of a thing it has a belief about, from what it finds of the thing now: the attributes that do
/// not change as it first saw them, the others as they are now. A belief about an abode is not taken in again when its
/// thing is seen, so it keeps its first sight whole. Without a first sight, the thing as it is now.
[[nodiscard]] creature_tree::Belief KnownOf(const Belief& belief, const creature_tree::Belief& now);
/// Whether a kind of thing is a whole thing a creature may act on: a town, a forest, a temple, a flock or a creature
[[nodiscard]] bool InListOfWholeThings(uint32_t kind);
/// Whether seeing again a thing of a kind, on a game turn, stirs the sources from seeing things: a whole thing, not the
/// creature itself, and more than 200 turns since the thing last stirred them (counted as the game does, on whole
/// numbers that may wrap)
[[nodiscard]] bool SightingStirsSources(uint32_t kind, bool itself, uint32_t turn, uint32_t sourcesTurn);
/// Whether a thing is useful enough to a source's desire for the sight of it to stir the source
[[nodiscard]] bool UsefulEnoughToStir(float usefulness);

/// The last moving thing a creature came across, which its look on the way to a far point takes for two seconds: where
/// it was on the map's grid (x, z, in metres), its height, the game turn it was seen on and how much the record counts.
/// A creature starts with all of them 0. Coming across a thing that moves, not the creature itself, keeps it while the
/// record counts no more than a half, and the record then counts a half; the look forgets it (counting 0 again) once it
/// is two seconds old
struct MovingSeen
{
	glm::vec2 at {0.0f};
	float height {0.0f};
	uint32_t turn {0};
	float weight {0.0f};
};
inline constexpr float k_MovingSeenWeight = 0.5f;
/// Whether coming across a thing keeps it as the last moving thing seen (a record that counts nothing comparable, as
/// the game compares it, keeps it too)
[[nodiscard]] bool KeepsMovingThing(bool moving, bool itself, float weight);
/// Whether the last moving thing seen is still looked at on a game turn: fewer turns since it was seen than two seconds
/// of the game's whole turns (counted on whole numbers that may wrap, as the game counts them)
[[nodiscard]] bool MovingSeenRecently(uint32_t turn, uint32_t seenTurn, uint32_t turnsPerSecond);

/// The things a creature has come across, a fixed number of places for each kind
class Store
{
public:
	/// Comes across a thing of a kind: the belief about it is found, or else a new belief is made. With all its kind's
	/// places taken, the new belief takes the place the oldest-kept marker is at, which moves on only when the belief
	/// there is forgotten, so that while the kind stays full every new belief replaces the last one put there. None for a
	/// kind beyond the game's. Whether the belief is new, as only a new one leads the creature on to what the thing
	/// belongs to.
	struct Added
	{
		Belief* belief {nullptr};
		bool isNew {false};
	};
	Added Add(entt::entity thing, uint32_t kind);
	[[nodiscard]] const Belief* Find(entt::entity thing) const;
	[[nodiscard]] Belief* Find(entt::entity thing);
	/// Forgets the belief about a thing; whether there was one
	bool Forget(entt::entity thing);
	/// Marks that the creature found no way to a thing it has a belief about, on a game turn
	void MarkBlocked(entt::entity thing, uint32_t turn);
	/// Forgets every belief about a thing that is gone
	template <typename Gone>
	void ForgetIf(Gone gone)
	{
		std::vector<entt::entity> forgotten;
		for (const auto& [thing, at] : _index)
		{
			if (gone(thing))
			{
				forgotten.push_back(thing);
			}
		}
		for (const auto thing : forgotten)
		{
			Forget(thing);
		}
	}
	/// Every belief it holds, kind by kind in the kinds' order, each kind's in the order of its places. A visit may add
	/// beliefs (the searches that walk it come across what a thing belongs to): each place is read again as it is
	/// reached, so the walk goes on over the store as it then is. A belief that replaces another in a place the walk has
	/// passed is not visited, one in a place ahead of it is; the belief a visit is given stays in its place while the
	/// visit runs, and may be replaced, so a visit reads what it needs of it first
	template <typename Visit>
	void ForEach(Visit visit) const
	{
		for (const auto& pool : _pools)
		{
			for (size_t place = 0; place < pool.slots.size(); ++place)
			{
				if (const auto& slot = pool.slots[place]; slot.has_value())
				{
					visit(*slot);
				}
			}
		}
	}
	/// How many beliefs it holds of a kind, and in all
	[[nodiscard]] size_t Count(uint32_t kind) const;
	/// How many of the things it holds beliefs about it took to belong to a whole thing
	[[nodiscard]] size_t CountBelongingTo(entt::entity whole) const;
	/// Its beliefs about whole things (InListOfWholeThings: towns, forests, temples, creatures and flocks), the newest
	/// first, as the game keeps them in one list it puts each new belief at the head of
	[[nodiscard]] std::vector<const Belief*> WholeThingsNewestFirst() const;
	/// Its beliefs about the things it took to belong to a whole thing, the newest first, as the game puts each part at
	/// the head of its whole thing's list of parts
	[[nodiscard]] std::vector<const Belief*> PartsOf(entt::entity whole) const;
	[[nodiscard]] size_t Size() const { return _index.size(); }

private:
	struct Place
	{
		uint32_t kind;
		size_t slot;
	};
	struct Pool
	{
		/// Its places, made when the kind is first believed in
		std::vector<std::optional<Belief>> slots;
		size_t used {0};
		/// The place the next belief replaces once all are taken: the first place taken, moving on to the next place when
		/// the belief there is forgotten; none while nothing is held
		std::optional<size_t> oldest;
	};

	/// The beliefs held for which the test is true, the newest first
	template <typename Test>
	[[nodiscard]] std::vector<const Belief*> NewestFirst(Test test) const
	{
		std::vector<const Belief*> held;
		ForEach([&held, &test](const Belief& belief) {
			if (test(belief))
			{
				held.push_back(&belief);
			}
		});
		std::ranges::sort(held, [](const Belief* a, const Belief* b) { return a->order > b->order; });
		return held;
	}

	std::array<Pool, k_KindCount> _pools {};
	std::unordered_map<entt::entity, Place> _index;
	/// How many beliefs it has made
	uint64_t _made {0};
};

/// How a creature's eyes sweep about it: the cells of a square around its own, its own cell first and then ring after
/// ring outwards, a ninth of them each turn (at most nine), starting again from its own cell once all have been looked
/// at
struct Sweep
{
	/// The cells still to look at in this sweep, the ring it is on (1 for its own cell) and the place in the ring
	uint32_t left {0};
	uint32_t ring {1};
	uint32_t step {0};
};

/// The cell at a place in a ring about a cell: ring 1 is the cell itself; ring r goes round the square r - 1 cells out,
/// along its four sides in turn starting from the corner at the least x and z
[[nodiscard]] glm::ivec2 RingCell(glm::ivec2 centre, uint32_t ring, uint32_t step);
/// The cells to look at this turn, moving the sweep on; a sweep that has finished starts again with the cells for the
/// creature's size now (creature_look::SweepCells). Cells beyond the map are among them, and are only passed over.
[[nodiscard]] std::vector<glm::ivec2> NextCells(Sweep& sweep, glm::ivec2 centre, float shownSize);

/// The town and the citadel its long-range vision last took in
struct LongRangeVision
{
	entt::entity town {entt::null};
	entt::entity citadel {entt::null};
};
/// What its long-range vision takes in this time: the town found and the citadel found, each only when it is not the one
/// taken in last time, which they become. The town is looked for leaving out the one taken in last time, so with two
/// towns or more it takes in another one each time, as the game's does
struct LongRangeSighting
{
	entt::entity town {entt::null};
	entt::entity citadel {entt::null};
};
[[nodiscard]] LongRangeSighting TakeInFromAfar(LongRangeVision& vision, entt::entity otherTown, entt::entity citadel);

/// Whether the creature forgets what it believes about a thing: once the thing is no longer available (deleted,
/// waiting on the dead list, or destroyed), as the original's sweep of beliefs about deleted objects tests each thing's
/// unavailable mark
[[nodiscard]] constexpr bool ForgetsBeliefAbout(bool available)
{
	return !available;
}
} // namespace openblack::creature_object_beliefs
