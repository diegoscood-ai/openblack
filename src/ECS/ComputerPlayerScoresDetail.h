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

#include <functional>
#include <optional>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/ComputerPlayerScores.h"
#include "Enums.h"

/// What the score rules of the different parts of the tree share; not for the rest of the game
namespace openblack::computer_player::scores
{

/// The node being scored, with its mind, its tree and the game
struct ScoreAt
{
	ScoreWorld& world;
	ecs::components::ComputerMind& mind;
	const MindTree& tree;
	uint16_t node;

	/// What the mind keeps for the node
	[[nodiscard]] ecs::components::ComputerMindNode& state();
};

/// The thing of the nearest node above this one that has one, starting at its parent: what most rules act on
[[nodiscard]] entt::entity AboveParent(const ScoreAt& at);

/// The game compares its floats so that a value that is not a number counts as below, at most and the same as any
/// other: these three keep that
[[nodiscard]] bool Below(float value, float than);
[[nodiscard]] bool AtMost(float value, float than);
[[nodiscard]] bool Same(float value, float than);

/// The size of a number of map units, which keeps the smallest one as it is (as the game's integer test does)
[[nodiscard]] int32_t Magnitude(int32_t value);

/// The float of the bits the game keeps for a constant
[[nodiscard]] float FromBits(uint32_t bits);

/// A town's harm lately, as the mind weighs it: of the last attacks of the seven players (the neutral one is not
/// counted), the one with the strictly highest fade, 1 - min(turns since / 600, 1)^3, a turn of no attack counting as
/// turn 0; its fade, its burn, and its crush and hit together
struct RecentHarm
{
	float fade {0.0f};
	float burn {0.0f};
	float physical {0.0f};
};
/// None when that attack did no burn and no crush or hit
[[nodiscard]] std::optional<RecentHarm> HighestRecentHarm(const ScoreWorld& world, entt::entity town);

/// A creature's size: its scale times 15
[[nodiscard]] float CreatureSize(const ScoreWorld& world, entt::entity creature);

/// The reach of a diversion by the mind's creature: twice its size and 200 more
[[nodiscard]] float DiversionReach(const ScoreWorld& world);

/// How near two creatures may be before one of them can no longer attack the other from afar: twice their sizes, at
/// least 30
[[nodiscard]] float FightDistance(const ScoreWorld& world, entt::entity mine, entt::entity other);

/// How many villagers are in `cells` map cells of a spiral that starts at the place's own cell, as each cell lists them
[[nodiscard]] uint32_t VillagersNear(const ScoreWorld& world, map_coords::MapCoords place, uint32_t cells);

/// How likely a town's villagers are to be found, from a place, and where: of its store's door, its meeting point, its
/// centre and its fields, in that order, the one with the most villagers in the nine cells round it times its distance's
/// fall-off over 1000 metres, the first of equals. Nothing while the town wants sleep more than half, and then `where`
/// stays as it was. Asking for the meeting point may work it out (ScoreWorld::CongregationPlace)
float PosWhereVillagersAre(ScoreWorld& world, entt::entity town, const map_coords::MapCoords& from,
                           map_coords::MapCoords& where);

/// The town's building or field most worth attacking from a place: a building by its life, how far it is built and
/// how much it is wanted, a field by how full of food it is and how much it is wanted, each times its distance's
/// fall-off over 1000 metres; the strictly best above 0, the buildings first (the newest first, fields among them),
/// then the fields. None when nothing is worth more than 0
[[nodiscard]] entt::entity BestAttackObject(const ScoreWorld& world, entt::entity town, const map_coords::MapCoords& from);

/// The thing in the map cells round a place that a weight finds best: over the cells within `reach` metres of it each
/// way (x outer, z inner, as the original works their edges out in floats), those on the map and inside the mind's
/// player's influence (at their middle), each of their things in the cell's order; a thing whose weight is not below
/// the best so far counts by its weight times its distance's fall-off over the reach, the strictly best. None when
/// nothing counts above 0
[[nodiscard]] entt::entity BestThingNear(const ScoreWorld& world, const map_coords::MapCoords& place, float reach,
                                         const std::function<float(entt::entity)>& weight);

/// The influence factor at a point twice the player's speed from `from`, towards `nearest`
[[nodiscard]] float FactorBeyond(const ScoreWorld& world, const map_coords::MapCoords& nearest,
                                 const map_coords::MapCoords& from);

/// The worth of a miracle at a place, its icon given to the node once it is found. (The original gives the node the
/// place itself when no icon was found, which is no thing: the node then holds nothing)
float SpellForNode(ScoreAt& at, MagicType type, const std::optional<map_coords::MapCoords>& place);

/// Twice a miracle's worth at a place, up to 1; asked again when below 1
float TwiceSpellForNode(ScoreAt& at, MagicType type, const std::optional<map_coords::MapCoords>& place);

/// The best miracle by a weight of each miracle: for each miracle with a weight, its worth at the place times the
/// weight; the strictly best and its icon
template <typename Weight>
float BestSpell(ScoreAt& at, const std::optional<map_coords::MapCoords>& place, entt::entity& icon, Weight weight)
{
	float best = 0.0f;
	entt::entity bestIcon = entt::null;
	for (uint32_t type = 1; type < ecs::components::PlayerMagic::k_MagicTypes; ++type)
	{
		const float share = weight(static_cast<MagicType>(type));
		if (Same(share, 0.0f))
		{
			continue;
		}
		entt::entity found = entt::null;
		float value = SpellWorth(at.world, at.mind, static_cast<MagicType>(type), place, found);
		value = value * share;
		if (value > best)
		{
			best = value;
			bestIcon = found;
		}
	}
	icon = bestIcon;
	return best;
}

// ---- the CreatureInteract rules (ComputerPlayerScoresCreature.cpp) ------------------------------------------------

/// The aim itself: the mind's creature, as free as it is for a job, but not while its body performs an action
float ScoreCreatureInteract(ScoreAt& at);
float ScoreCreatureDesire(ScoreAt& at, uint32_t desire, float share);
float ScoreGetFoodFromField(ScoreAt& at);
float ScoreGetFoodFromFishFarm(ScoreAt& at);
float ScoreCastFoodSpellOn(ScoreAt& at);
float ScoreRestoreCreatureHealth(ScoreAt& at);
float ScoreRestoreMyCreatureHealth(ScoreAt& at);
float ScoreTeachCreatureNormalAction(ScoreAt& at);
float ScoreTeachCreatureMagic(ScoreAt& at, uint32_t lessons);
float ScoreTeachCreatureCompassionateMagic166(ScoreAt& at);
float ScoreTeachCreatureImpressiveMagic168(ScoreAt& at);
float ScorePlayWithCreature(ScoreAt& at, float most, float least, bool offTheLeash);
float ScoreFindFreeVillagerInArea(ScoreAt& at);
float ScoreCastSpellOnMyCreatureForALaugh(ScoreAt& at);
float ScoreCastSpellOnMyCreatureForALaugh170(ScoreAt& at);
float ScoreReacttoCreatureStarvingEmergency(ScoreAt& at);
float ScoreTakeFromOtherTown(ScoreAt& at, uint32_t resource);

// ---- the TownInteract rules (ComputerPlayerScoresTown.cpp) --------------------------------------------------------

float ScoreTownWithHighestFoodDesire(ScoreAt& at);
float ScoreCreateDiscipleToFulfillTownDesire(ScoreAt& at);
float ScoreTownWithHighestWoodDesire(ScoreAt& at);
float ScoreShallCastWoodSpell(ScoreAt& at);
float ScoreGiveBuildingSiteWood(ScoreAt& at);
float ScoreGiveBuildingSiteWorkers(ScoreAt& at);
float ScoreFindFreeVillagerBuilderInTown(ScoreAt& at);
float ScoreCombineScaffolds(ScoreAt& at);
float ScoreFindScaffoldToCombineWith(ScoreAt& at);
float ScoreFindPlaceToUseScaffoldForBuilding(ScoreAt& at);
float ScoreFindLocationToUseScaffold(ScoreAt& at);
float ScoreFillWorkshop(ScoreAt& at);
float ScoreGiveWorshippersFood(ScoreAt& at);
float ScoreWorshipSiteThatNeedsVillagers(ScoreAt& at);
float ScoreTakeVillagerFromTownToWorshipSite(ScoreAt& at);
float ScoreChargeUpSpell(ScoreAt& at);
float ScoreMakeDiscipleBreader(ScoreAt& at);
float ScoreGetWoodFromForest(ScoreAt& at);

// ---- the DefeatPlayer rules (ComputerPlayerScoresDefeat.cpp) ------------------------------------------------------

/// A float times a double rounded once to a float, as the game's float unit does it
[[nodiscard]] float MulRoundedOnce(float value, double by);
/// A float plus a double rounded once to a float, as the game's float unit does it
[[nodiscard]] float AddRoundedOnce(float value, double plus);

/// The next of the game's players after a player (from the first without one) that has a type, among the seven that
/// are not the neutral, then the neutral player whatever its type; none after the neutral
[[nodiscard]] std::optional<PlayerNames> NextActivePlayerOrNeutral(const ScoreWorld& world, std::optional<PlayerNames> after);

/// How much the mind's player wants to attack another player, as the original works it out: the sum of how near the
/// mind's hand is to its own buildings, how far apart their alignments are, how much more the other harmed it than it
/// harmed the other, how much weaker or stronger the other is (the stronger by the nearness of their citadels) and how
/// much faster the other's influence grows than its own; all times the nearness of their citadels
[[nodiscard]] float DesireToAttack(const ScoreWorld& world, PlayerNames other);

/// How much a belief attack on the town matters to the mind's player: for its own town, ten times its best rival's
/// share of the belief, up to 1; 1 for any other
[[nodiscard]] float BeliefAttackImportance(const ScoreWorld& world, entt::entity town);

float ScoreExpandInfluence(ScoreAt& at);
float ScoreTakeOverTownByFulfillingFoodDesire(ScoreAt& at);
float ScoreTakeOverTown(ScoreAt& at);
float ScoreThrowThingsToImpress(ScoreAt& at);
float ScoreCastSpellToFulfilTownNeeds(ScoreAt& at);
float ScorePlayerWithBetterCreature(ScoreAt& at);
float ScoreAttackCreature(ScoreAt& at);
float ScoreDefeatPlayer37(ScoreAt& at);

/// Whether a villager is free for work for the mind, and if so the best so far becomes the distance's fall-off from
/// the hand (the original keeps that, not the score it compared)
bool FreeVillager(ScoreAt& at, entt::entity villager, float& best);

} // namespace openblack::computer_player::scores
