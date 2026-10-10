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
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/TownDesire.h"
#include "ECS/TownAggression.h"
#include "Enums.h"

namespace openblack::ecs::components
{

/// A building the town plans to build (CREATE_PLANNED_ABODE: a planned town centre when the abode's type is
/// TownCentre, else a planned abode; added to the town). Only data: the planned objects are invisible (their draw
/// does nothing). ecs::plans converts them into abodes under construction. A field with no known use and the
/// footpath link (footpaths not ported) are left out
struct PlannedAbode
{
	AbodeInfo info;
	glm::vec3 position;
	float yAngleRadians; ///< The script's N4 * 0.001
	float scale;         ///< N5 * 0.001
	bool townCentre;     ///< A planned town centre
	/// "was a built building" (a rebuild plan made from a building with the matching flag set); 0 for the
	/// script's. Read by the desire to be repaired and when the plan is created
	bool wasBuilt {false};
	/// The creation turn; its age = turn - it. plans::AddPlanned writes it
	uint32_t creationTurn {0};
	/// The footpath link of the .fot when no built abode was at its point (resolved on load). (pending) its hand-over
	/// when the plan is built
	entt::entity footpathLink {entt::null};
	/// a planned citadel heart (CitadelArchetype::CreatePlan): `info` is None and its info is the citadel heart info
	/// of `heartInfo` (the script's N3). Its abode type is 0x804, it is not civic and has no town
	bool citadelHeart {false};
	uint32_t heartInfo {0};
};

/// std::array<float, N> with every element `value` (the default member initialisers of TownBelief)
template <size_t N>
constexpr std::array<float, N> FilledArray(float value)
{
	std::array<float, N> result {};
	result.fill(value);
	return result;
}

/// The town's belief, every row by the player number (PlayerNames, NEUTRAL = 7); changed by ecs::town_belief and
/// ecs::town_stores::AddToBelief. The defaults are the zero-filled allocation with the belief init's cap and boredom,
/// so that a town made without the init still caps at 10; the init also sets the desire thresholds. Two town
/// fields read only by the belief are kept here too.
struct TownBelief
{
	/// [player] BeliefInPlayer (GetBeliefInPlayer): SetBelief (capped), ReduceBelief (not clamped: it can go below 0);
	/// 0 in the init but the neutral slot (= beliefInNeutralPlayer)
	std::array<float, 8> belief {};
	/// [player], float: += f; only decays (x 0.997 a turn, from the player info, at the fold) and is read only by the
	/// computer player. Not reset by the init
	std::array<float, 8> recent {};
	std::array<uint32_t, 8> lastAddedTurn {}; ///< [player], the turn of the last f != 0; no reader
	/// [player] BeliefInPlayerMax: 10.0 (init); SET_TOWN_BELIEF_CAP (SetBeliefInPlayerCap)
	std::array<float, 8> cap {10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
	/// [player]: += the folded amount; 0 every 10 turns (ProcessOncePerTurn). Not reset by the init
	std::array<float, 8> addedThisPeriod {};
	/// [player]: ReduceBelief's accumulator for its (never shown) draw; 0 in the init
	std::array<float, 8> reduceAccumulator {};
	/// [player]: += f, what was added since the last fold; the fold adds it x beliefScale into belief (SetBelief) and
	/// addedThisPeriod, then 0
	std::array<float, 8> pending {};
	/// [REACTION] BoredomMultiplier: 1.0 (init); the fold adds the reaction info's amount below 1, the villagers' how
	/// impressed update calls AddToBoredomMultiplier; read by GetBoredomMultiplier
	std::array<float, 41> boredom = FilledArray<41>(1.0f);
	/// [TOWN_DESIRE]: the desire above which the owner loses belief (at the fold); the town desire info's threshold in
	/// the init and on every fold with something pending, then lowered a fold while above the belief info's limit
	/// (0.25)
	std::array<float, 17> desireThreshold {};
	/// BeliefInNeutralPlayer: the town info's 0.5 after the init in the town's constructor; SET_TOWN_BELIEF of the
	/// neutral player. The fold pins belief[NEUTRAL] to it
	float beliefInNeutralPlayer {0.0f};
	/// The scale of the pending belief (1.0 at construction); SET_TOWN_BALANCE_BELIEF_SCALE (case 98), LHVM
	/// SET_OBJECT_BELIEF_SCALE (not ported)
	float beliefScale {1.0f};
};

struct Town
{
	uint32_t id;
	/// (openblack) the order the town constructor ran in (TownArchetype::Create, 1 up; 0 for a town made elsewhere).
	/// The global town list is filled at the head: the higher, the newer. Town::id is the script's CREATE_TOWN
	/// argument, not that order (maps create towns out of id order, and one repeats id 0)
	uint32_t creationStamp {0};
	/// (openblack) reproduces the insertion order of the owner's town list (appended at the tail, from the town's
	/// constructor and when a player takes over a town): one counter for both, so sorting by it (map_cells::TownsOf)
	/// walks the towns as that list does
	uint32_t ownerListStamp {0};
	/// The town's player: the player given to CREATE_TOWN (the neutral player when none).
	/// Planned citadels belong to it, not to the player named in CREATE_PLANNED_CITADEL.
	PlayerNames owner {PlayerNames::NEUTRAL};
	bool uninhabitable = false; ///< SET_TOWN_UNINHABITABLE
	/// the town's homeless, the head first (made homeless without a state change: inserted at the head). Changed only
	/// by ecs::town_villagers
	std::vector<entt::entity> homelessVillagers;
	/// the first town centre made for it (CREATE_TOWN_CENTRE sets it only while empty)
	entt::entity centre {entt::null};
	/// [RESOURCE_TYPE]: the temporary pots (FOOD, WOOD) GetTemporaryResourceStorePotOrPos makes and keeps;
	/// ecs::town_stores
	std::array<entt::entity, 2> temporaryPots {entt::null, entt::null};
	/// [player][RESOURCE_TYPE]: the turn each player last took FOOD / WOOD from this town's abodes or storage pit
	/// through an interface (SetGameTurnResourceLastRemoved); 0 = never. ecs::town_stores
	std::array<std::array<uint32_t, 2>, 8> resourceLastRemovedTurn {};
	/// the town's belief (ecs::town_belief; AddToBelief is ecs::town_stores')
	TownBelief belief;
	/// SetWorshipPercentage (CREATE_TOWN_CENTRE's N5 * 0.001; all the shipped lands pass 0).
	/// The original keeps it only if the town has a worship site (otherwise 0) and passes it on to the totem statue;
	/// openblack has no worship sites yet and stores the script's value.
	float worshipPercentage {0.0f};
	std::vector<PlannedAbode> plannedAbodes; ///< Oldest first (AddPlanned, ecs::plans)
	/// the building sites, the head first (AddBuildingSite inserts at the head);
	/// entities with components::BuildingSite. Changed only by ecs::building_sites
	std::vector<entt::entity> buildingSites;
	/// the graveyard (ecs::graveyard: set when a graveyard becomes functional, cleared when it is deleted); read by
	/// GetDesireToBeBuilt (the graveyard and spell dispenser cases)
	entt::entity graveyard {entt::null};
	/// the town rectangle, MapCoords x / z (min, max; the cells are the high words). SetTownArea (town_placement);
	/// min 0x7FFFFFFF, max 0 = empty
	glm::ivec2 areaMin {0x7FFFFFFF, 0x7FFFFFFF};
	glm::ivec2 areaMax {0, 0};
	/// the town has had a centre, a storage pit and a house (set once). (not ported)
	/// readers
	bool hasCentrePitAndHouse {false};
	/// the wood used building, kept (the stats are recomputed)
	float woodUsedForBuilding {0.0f};
	/// CREATE_FLOCK's flocks for this town; a flock is taken off when an animal that can't be shepherded joins it
	std::vector<entt::entity> flocks;
	/// the animals whose town is this one, the head first (animal_ai::SetTown inserts at the head; an animal's deletion
	/// unlinks it). Literal: an animal that moved to another town stays listed here (SetTown does not unlink), so a
	/// reader must test each entry (Valid, and its town == this town if it matters). (pending) the town's deletion:
	/// SetTown(animal, 0) for each
	std::vector<entt::entity> animals;
	TownDesire desire;
	/// GetCongregationPos's cache, MapCoords x / z (6553.6 per metre) and y; (0, 0, 0) = not computed yet. Zeroed by
	/// the constructor; written by GetCongregationPos, by SET_TOWN_CONGREGATION_POS (map command case 6) and cleared by
	/// CheckWhenNewBuildingCreated (a building made within 7.5 m, after a plan is created: ecs::plans)
	glm::ivec2 congregationPos {0, 0};
	float congregationPosY {0.0f};
	/// the turn the town's emergency started (IsInStateOfEmergency reads it; 0 = none).
	/// Written by SetInStateOfEmergency, cleared by ProcessTownEmergency (ecs::town_emergency)
	uint32_t emergencyStartTurn {0};
	/// the worship percentage saved while the emergency lasts (ProcessTownEmergency), given back after it and then 0.
	/// 0 at creation (the zero-filled town)
	float savedWorshipPercentage {0.0f};
	/// what the town keeps of the attacks on it (town_aggression): the aggression held against each player, the last
	/// attacker and its turn (0 = never), and this turn's wants of protection and mercy, which its desires read.
	/// Written by town_emergency::UpdateAggressor, faded each turn by the town's process
	town_aggression::Record aggression;
	/// SetStoragePit (when a storage pit becomes functional; the last one wins); read through
	/// town_queries::GetStoragePit. A whole (script) one at its creation, a plan's when built (MakeFunctional,
	/// town_stores::SetStoragePit)
	entt::entity storagePit {entt::null};
	/// a creche becoming functional sets it when it is still null (the first one wins): a whole (script) one
	/// at its creation, a plan's when built (abodes::MakeFunctional)
	entt::entity creche {entt::null};
	/// the workshops, the head first (AddWorkshop pushes at the head when not there;
	/// RemoveWorkshop). Changed only by ecs::workshops
	std::vector<entt::entity> workshops;
	/// "a plan was asked for this turn" (0 at construction; the town process clears it; CheckSatisfyAbodes /
	/// Civic set it)
	bool requestedPlanThisTurn {false};
	/// the building / resource pulse (0 at construction; AddBuildingSite and a storage pit's AddResource write the
	/// pulse); the town process: previous != 0 -> pulse = 0; previous = pulse
	uint32_t buildPulse {0};
	uint32_t buildPulsePrevious {0};
	/// the empty town's countdown (RemoveVillager sets 50, TODO: villager death); the town process
	uint32_t emptyCountdown {0};
	/// this turn's town stats (ecs::town_stats)
	TownStats stats;
	/// the food the villagers have eaten (UseFood), kept: it cannot be recomputed
	float foodUsed {0.0f};
	/// the number FindTownWithID compares (town_queries::ScriptIdOf): the script's CREATE_TOWN number (unset:
	/// Town::id), or the founded-town id (k_FoundedTownScriptId) for every town a scaffold founds, so several towns can
	/// share it. Town::id stays openblack's unique key for the map and the abodes ((openblack): the original's abode
	/// keeps a pointer to its town, which a unique key stands for)
	std::optional<uint32_t> scriptId;
};

} // namespace openblack::ecs::components
