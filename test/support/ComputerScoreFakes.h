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
#include <bit>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/ComputerPlayerScores.h"

// A computer god's score world laid out by a test, shared by the mind's score tests and the hand's action tests

namespace openblack::test
{
namespace computer_score_fakes
{
using namespace openblack::computer_player;
using openblack::ecs::components::ComputerMind;
using openblack::map_coords::MapCoords;

constexpr entt::entity k_None {entt::null};

template <typename Map>
typename Map::mapped_type At(const Map& map, const typename Map::key_type& key, typename Map::mapped_type fallback)
{
	const auto found = map.find(key);
	return found != map.end() ? found->second : fallback;
}

/// A game the test lays out: what each score rule reads, and nothing else
class FakeScoreWorld final: public ScoreWorld
{
public:
	[[nodiscard]] uint32_t Turn() const override { return turn; }
	[[nodiscard]] PlayerNames Me() const override { return me; }
	[[nodiscard]] bool IsAllied(PlayerNames player, PlayerNames other) const override
	{
		return allies.contains({player, other});
	}
	[[nodiscard]] uint32_t PlayerType(PlayerNames player) const override { return At(types, player, 0u); }
	[[nodiscard]] entt::entity PlayerThing(PlayerNames player) const override { return At(playerThings, player, k_None); }
	[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity thing) const override
	{
		for (const auto& [player, entity] : playerThings)
		{
			if (entity == thing)
			{
				return player;
			}
		}
		return std::nullopt;
	}
	[[nodiscard]] float InfluencePower(PlayerNames player) const override { return At(powers, player, 0.0f); }
	[[nodiscard]] MapCoords PlayerFixedPlace(PlayerNames player) const override
	{
		return At(fixedPlaces, player, MapCoords {});
	}
	[[nodiscard]] float AverageInfluencePower(PlayerNames player) const override { return At(averages, player, 0.0f); }
	[[nodiscard]] float HarmFrom(PlayerNames player, PlayerNames by) const override { return At(harms, {player, by}, 0.0f); }
	[[nodiscard]] float AlignmentValue(PlayerNames player) const override { return At(alignments, player, 0.0f); }
	[[nodiscard]] uint32_t ForestGrownTrees(uint32_t forest) const override { return At(forestGrown, forest, 0u); }
	[[nodiscard]] entt::entity ForestCentreTree(uint32_t forest) const override { return At(forestCentres, forest, k_None); }
	[[nodiscard]] bool IsTree(entt::entity thing) const override { return treeSet.contains(thing); }
	[[nodiscard]] float TreeWoodValue(entt::entity tree) const override { return At(woodValues, tree, 0.0f); }
	[[nodiscard]] uint32_t TreeKindWood(entt::entity tree) const override { return At(kindWood, tree, 0u); }
	[[nodiscard]] float ThingRadius(entt::entity thing) const override { return At(radii, thing, 0.0f); }
	[[nodiscard]] MapCoords LeaderHandPlace() const override { return leaderHand; }
	[[nodiscard]] float VirtualInfluenceMaxDistance() const override { return maxDistance; }
	[[nodiscard]] bool InMyInfluence(const MapCoords& place) const override
	{
		return inside || insideAt.contains({place.x, place.z});
	}
	[[nodiscard]] std::optional<PlayerNames> MostInfluentialPlayerAt(const MapCoords& place) const override
	{
		for (const auto& [at, player] : influentialAt)
		{
			if (at.first == place.x && at.second == place.z)
			{
				return player;
			}
		}
		return std::nullopt;
	}

	[[nodiscard]] bool IsTown(entt::entity thing) const override { return towns.contains(thing); }
	[[nodiscard]] bool IsCreature(entt::entity thing) const override { return creatures.contains(thing); }
	[[nodiscard]] bool IsAbode(entt::entity thing) const override { return abodes.contains(thing); }
	[[nodiscard]] bool IsObject(entt::entity thing) const override { return objects.contains(thing); }
	[[nodiscard]] bool HasPlace(entt::entity thing) const override { return places.contains(thing); }
	[[nodiscard]] MapCoords Place(entt::entity thing) const override { return At(places, thing, MapCoords {}); }
	[[nodiscard]] float Life(entt::entity thing) const override { return At(lives, thing, 1.0f); }
	[[nodiscard]] entt::entity TownOf(entt::entity thing) const override { return At(townOf, thing, k_None); }
	[[nodiscard]] bool ScriptControlled(entt::entity thing) const override { return scripted.contains(thing); }

	[[nodiscard]] entt::entity MyCreature() const override { return myCreature; }
	[[nodiscard]] std::vector<entt::entity> Creatures() const override { return allCreatures; }
	[[nodiscard]] std::optional<PlayerNames> OwnerOf(entt::entity creature) const override
	{
		const auto found = owners.find(creature);
		return found != owners.end() ? std::optional<PlayerNames>(found->second) : std::nullopt;
	}
	[[nodiscard]] float CreatureScale(entt::entity creature) const override { return At(scales, creature, 1.0f); }
	[[nodiscard]] float CreatureEnergy(entt::entity creature) const override { return At(energies, creature, 1.0f); }
	[[nodiscard]] float CreatureExhaustion(entt::entity creature) const override { return At(exhaustions, creature, 0.0f); }
	[[nodiscard]] bool IsFighting(entt::entity creature) const override { return fighting.contains(creature); }
	[[nodiscard]] creature_body::BodyActivity CreatureBodyActivity(entt::entity creature) const override
	{
		return At(bodies, creature, creature_body::BodyActivity {});
	}
	[[nodiscard]] std::optional<uint32_t> PlanDesire(entt::entity creature) const override
	{
		const auto found = planDesires.find(creature);
		return found != planDesires.end() ? std::optional<uint32_t>(found->second) : std::nullopt;
	}
	[[nodiscard]] std::optional<Leash> LeashOn() const override { return leash; }
	[[nodiscard]] entt::entity LeaderLeashTiedTo() const override { return leaderTiedTo; }

	[[nodiscard]] std::vector<entt::entity> TownsOf(PlayerNames player) const override { return At(townsOf, player, {}); }
	[[nodiscard]] entt::entity StoragePit(entt::entity town) const override { return At(pits, town, k_None); }
	[[nodiscard]] entt::entity WorshipSiteOf(entt::entity town) const override { return At(worshipSites, town, k_None); }
	[[nodiscard]] float TownInfluenceRadius(entt::entity town) const override { return At(townRadii, town, 0.0f); }
	[[nodiscard]] Area TownArea(entt::entity town) const override { return At(areas, town, Area {}); }
	[[nodiscard]] MapCoords TownAreaCentre(entt::entity town) const override { return At(centres, town, MapCoords {}); }
	[[nodiscard]] std::vector<entt::entity> TownAbodes(entt::entity town) const override { return At(abodesOf, town, {}); }
	[[nodiscard]] PlayerNames TownOwner(entt::entity town) const override { return At(townOwners, town, PlayerNames::NEUTRAL); }
	[[nodiscard]] Aggression TownAggression(entt::entity town, PlayerNames attacker) const override
	{
		return At(aggressions, {town, attacker}, Aggression {});
	}
	[[nodiscard]] ecs::town_aggression::AttackEffect TownLastAttackEffect(entt::entity town,
	                                                                      PlayerNames attacker) const override
	{
		return At(lastEffects, {town, attacker}, ecs::town_aggression::AttackEffect {});
	}
	[[nodiscard]] float RivalRecentRatio(entt::entity town) const override { return At(rivalRecent, town, 0.0f); }
	[[nodiscard]] float BeliefNeededOrLeft(entt::entity town) const override { return At(beliefLeft, town, 0.0f); }
	[[nodiscard]] float RivalBeliefRatio(entt::entity town) const override { return At(rivalBelief, town, 0.0f); }
	[[nodiscard]] MapCoords DoorPlace(entt::entity building) const override { return At(doors, building, MapCoords {}); }
	MapCoords CongregationPlace(entt::entity town) override
	{
		++congregationAsked;
		return At(congregations, town, MapCoords {});
	}
	[[nodiscard]] entt::entity TownCentre(entt::entity town) const override { return At(townCentres, town, k_None); }
	[[nodiscard]] float TownBoredom(entt::entity town, uint32_t reaction) const override
	{
		return At(boredoms, {town, reaction}, 0.0f);
	}
	[[nodiscard]] float AbodePercentBuilt(entt::entity abode) const override { return At(percentBuilt, abode, 0.0f); }
	[[nodiscard]] float AbodeAttackDesire(entt::entity abode) const override { return At(abodeAttack, abode, 0.0f); }
	[[nodiscard]] float FieldAttackDesire(entt::entity field) const override { return At(fieldAttack, field, 0.0f); }

	[[nodiscard]] bool OnFire(entt::entity abode) const override { return burning.contains(abode); }
	[[nodiscard]] bool FireAboveReactionTemperature(entt::entity abode) const override { return hot.contains(abode); }

	[[nodiscard]] std::optional<Citadel> MyCitadel() const override { return citadel; }
	[[nodiscard]] entt::entity MyCitadelHeart() const override { return heart; }
	[[nodiscard]] uint32_t HeartBreakingBlowTurn(entt::entity of) const override { return of == heart ? heartBlowTurn : 0u; }

	[[nodiscard]] bool IsMagicTypeEnabled(MagicType type) const override { return enabled.contains(type); }
	[[nodiscard]] uint32_t PerceivedDesire(MagicType type) const override { return At(perceived, type, 0u); }
	[[nodiscard]] float MagicImpressiveBalance(MagicType type) const override { return At(impressive, type, 0.0f); }
	[[nodiscard]] float MagicAggressiveAttackValue(MagicType type) const override { return At(aggressive, type, 0.0f); }
	[[nodiscard]] entt::entity CitadelSpellIcon(MagicType type) const override { return At(icons, type, k_None); }
	[[nodiscard]] entt::entity IconSite(entt::entity icon) const override { return At(iconSites, icon, k_None); }
	[[nodiscard]] float IconChantStore(entt::entity icon) const override { return At(stores, icon, 0.0f); }
	[[nodiscard]] float IconChantsToCreate(entt::entity icon) const override { return At(costs, icon, 0.0f); }
	[[nodiscard]] float SiteChantsAvailable(entt::entity site) const override { return At(available, site, 0.0f); }
	[[nodiscard]] float SiteBattery(entt::entity site) const override { return At(batteries, site, 0.0f); }
	[[nodiscard]] float SiteTotalChantsAvailable(entt::entity site) const override { return At(totals, site, 0.0f); }
	[[nodiscard]] float SiteChantsVillagersCanSupply(entt::entity site) const override { return At(supplies, site, 0.0f); }
	[[nodiscard]] bool SiteBuilt(entt::entity site) const override { return built.contains(site); }
	[[nodiscard]] bool SiteRepaired(entt::entity site) const override { return repaired.contains(site); }
	[[nodiscard]] uint32_t SiteWorshippers(entt::entity site) const override { return At(worshippers, site, 0u); }
	[[nodiscard]] CastSpell SpellCast(entt::entity spell) const override { return At(casts, spell, CastSpell {}); }

	[[nodiscard]] MapCoords HandPlace() const override { return hand; }
	[[nodiscard]] float CreatureDesire(entt::entity creature, uint32_t desire) const override
	{
		return At(desires, {creature, desire}, 0.0f);
	}
	[[nodiscard]] float CreatureLowEnergyThreshold(entt::entity creature) const override
	{
		return At(lowEnergy, creature, 0.0f);
	}
	[[nodiscard]] uint32_t UnknownNormalActions(entt::entity creature) const override
	{
		return At(unknownActions, creature, 0u);
	}
	[[nodiscard]] float MiracleKnowledge(entt::entity creature, CreatureDesires desire) const override
	{
		return At(miracleKnowledge, {creature, desire}, 1.0f);
	}
	[[nodiscard]] std::optional<uint32_t> MiracleLeftToLearn(entt::entity creature, CreatureDesires desire) const override
	{
		return At(miraclesLeft, {creature, desire}, std::optional<uint32_t> {});
	}
	[[nodiscard]] std::optional<uint32_t> NormalActionLeftToLearn(entt::entity creature) const override
	{
		return At(normalLeft, creature, std::optional<uint32_t> {});
	}
	[[nodiscard]] std::optional<MapCoords> CreatureHome(entt::entity creature) const override
	{
		return At(homes, creature, std::optional<MapCoords> {});
	}
	[[nodiscard]] entt::entity NearestOfType(const MapCoords& place, ObjectType type, float radius) const override
	{
		nearestAsked.emplace_back(place, type, radius);
		return At(nearestOfType, type, k_None);
	}
	[[nodiscard]] bool AnySpellOnCreature(entt::entity creature) const override { return spelled.contains(creature); }

	[[nodiscard]] std::vector<entt::entity> TownFields(entt::entity town) const override { return At(fieldsOf, town, {}); }
	[[nodiscard]] std::vector<entt::entity> TownFishFarms(entt::entity town) const override { return At(farmsOf, town, {}); }
	[[nodiscard]] float FieldFood(entt::entity field) const override { return At(fieldFood, field, 0.0f); }
	[[nodiscard]] float FieldFoodCapacity(entt::entity field) const override { return At(fieldCapacity, field, 0.0f); }
	[[nodiscard]] entt::entity ResourceDropObject(entt::entity town, uint32_t resource) override
	{
		dropsAsked.emplace_back(town, resource);
		return At(drops, town, k_None);
	}
	[[nodiscard]] uint32_t AbodeResource(entt::entity pit, uint32_t resource) const override
	{
		return At(pitResources, {pit, resource}, 0u);
	}
	[[nodiscard]] entt::entity StorePile(entt::entity pit, uint32_t resource, uint32_t index) const override
	{
		return At(piles, {pit, resource * 8 + index}, k_None);
	}
	[[nodiscard]] uint32_t PileAmount(entt::entity pile) const override { return At(pileAmounts, pile, 0u); }
	[[nodiscard]] float RawDesire(entt::entity town, uint32_t desire) const override
	{
		return At(rawDesires, {town, desire}, 0.0f);
	}

	[[nodiscard]] VillagerFacts VillagerFactsOf(entt::entity villager) const override
	{
		return At(villagers, villager, VillagerFacts {});
	}
	[[nodiscard]] std::vector<entt::entity> VillagersAt(const MapCoords& place) const override
	{
		return At(villagersAt, {place.x >> 16, place.z >> 16}, {});
	}

	float GameFloatRand(float most) override { return game_random::FloatRand(most, seed); }
	uint32_t GameRand(uint32_t range) override { return range == 0 ? 0 : game_random::SeededRandom(range, seed); }

	[[nodiscard]] float TownDesire(entt::entity town, uint32_t desire) const override
	{
		return At(townDesires, {town, desire}, 0.0f);
	}
	[[nodiscard]] float DesiredFood(entt::entity town) const override { return At(desiredFood, town, 0.0f); }
	[[nodiscard]] uint32_t FoodAvailable(entt::entity town) const override { return At(foodAvailable, town, 0u); }
	[[nodiscard]] uint32_t TownDisciples(entt::entity town, uint32_t kind) const override
	{
		return At(disciples, {town, kind}, 0u);
	}
	[[nodiscard]] uint32_t TownAdults(entt::entity town) const override { return At(adults, town, 0u); }
	[[nodiscard]] uint32_t TownChildren(entt::entity town) const override { return At(children, town, 0u); }
	[[nodiscard]] int32_t TownAdultPlaces(entt::entity town) const override { return At(adultPlaces, town, 0); }
	[[nodiscard]] float TotemWorship(entt::entity town) const override { return At(totemWorship, town, 0.0f); }
	[[nodiscard]] TownWorship TownWorshipFacts(entt::entity town) const override
	{
		return At(townWorship, town, TownWorship {});
	}
	[[nodiscard]] std::vector<entt::entity> AbodeInhabitants(entt::entity abode) const override
	{
		return At(inhabitants, abode, {});
	}
	[[nodiscard]] std::vector<entt::entity> TownHomeless(entt::entity town) const override { return At(homeless, town, {}); }

	[[nodiscard]] bool IsBuildingSite(entt::entity thing) const override { return buildingSites.contains(thing); }
	[[nodiscard]] std::vector<entt::entity> TownBuildingSites(entt::entity town) const override
	{
		return At(sitesOf, town, {});
	}
	[[nodiscard]] BuildingSiteFacts BuildingSiteFactsOf(entt::entity site) const override
	{
		return At(buildingSites, site, BuildingSiteFacts {});
	}
	[[nodiscard]] bool IsWorshipSite(entt::entity thing) const override { return worshipSiteSet.contains(thing); }
	[[nodiscard]] std::vector<entt::entity> SiteTowns(entt::entity site) const override { return At(siteTowns, site, {}); }
	[[nodiscard]] bool IsScaffold(entt::entity thing) const override { return scaffolds.contains(thing); }
	[[nodiscard]] bool IsWorkshop(entt::entity abode) const override { return workshops.contains(abode); }
	[[nodiscard]] std::vector<entt::entity> WorkshopScaffolds(entt::entity workshop) const override
	{
		return At(workshopScaffolds, workshop, {});
	}
	[[nodiscard]] ScaffoldFacts ScaffoldFactsOf(entt::entity scaffold) const override
	{
		return At(scaffolds, scaffold, ScaffoldFacts {});
	}
	[[nodiscard]] bool AbodeFunctional(entt::entity abode) const override { return functional.contains(abode); }
	[[nodiscard]] int32_t WorkshopSpace(entt::entity workshop) const override { return At(space, workshop, 0); }
	[[nodiscard]] float WorkshopMostObjects(entt::entity workshop) const override { return At(mostObjects, workshop, 1.0f); }
	[[nodiscard]] uint32_t ScaffoldWoodValue() const override { return scaffoldWood; }
	bool TryToBuildAt(entt::entity town, const MapCoords& place, int32_t scaffoldValue) override
	{
		// the town's planning draws once as it starts
		static_cast<void>(game_random::FloatRand(std::bit_cast<float>(0x40C90FDBu), seed));
		triedAt.push_back({town, place, scaffoldValue});
		return planWorks;
	}
	[[nodiscard]] Cells MapCells() const override { return mapCells; }
	[[nodiscard]] std::vector<entt::entity> CellObjects(uint16_t x, uint16_t z) const override
	{
		cellsAsked.emplace_back(x, z);
		return At(cellObjects, {x, z}, {});
	}
	[[nodiscard]] ThrowFacts ThrowFactsOf(entt::entity thing) const override { return At(throwFacts, thing, ThrowFacts {}); }
	[[nodiscard]] float PlayerSpeed() const override { return speed; }
	[[nodiscard]] std::optional<std::array<uint8_t, 4>> CellCorners(int32_t x, int32_t z) const override
	{
		if (allSteep)
		{
			return std::array<uint8_t, 4> {0, 0, 11, 0};
		}
		const auto found = corners.find({x, z});
		return found != corners.end() ? std::optional<std::array<uint8_t, 4>>(found->second) : std::nullopt;
	}
	[[nodiscard]] glm::vec3 WorldPoint(const MapCoords& place) const override
	{
		return {static_cast<float>(place.x), 0.0f, static_cast<float>(place.z)};
	}

	[[nodiscard]] std::array<entt::entity, 6> CitadelSites() const override { return citadelSites; }
	[[nodiscard]] float SiteFoodDesire(entt::entity site) const override { return At(siteFoodDesires, site, 0.0f); }
	[[nodiscard]] float SiteRestDesire(entt::entity site) const override { return At(siteRestDesires, site, 0.0f); }
	[[nodiscard]] std::vector<SiteWorshipper> SiteWorshipperList(entt::entity site) const override
	{
		return At(siteWorshippers, site, {});
	}
	[[nodiscard]] int32_t SiteFoodNeededFromTown(entt::entity site) const override { return At(siteFoodNeeded, site, 0); }
	[[nodiscard]] uint32_t SiteIconCount(entt::entity site) const override
	{
		return static_cast<uint32_t>(At(siteIcons, site, {}).size());
	}
	[[nodiscard]] std::vector<entt::entity> SiteIcons(entt::entity site) const override { return At(siteIcons, site, {}); }
	[[nodiscard]] bool IconCharging(entt::entity icon) const override { return charging.contains(icon); }
	[[nodiscard]] MagicType IconMagicType(entt::entity icon) const override { return At(iconTypes, icon, MagicType::None); }
	void BuildSite(entt::entity site, float amount) override { builtBy.emplace_back(site, amount); }
	[[nodiscard]] MapCoords SiteGatePlace(entt::entity site) const override { return At(gates, site, MapCoords {}); }

	[[nodiscard]] entt::entity CreatureOf(PlayerNames player) const override
	{
		return player == me ? myCreature : At(rivalCreatures, player, k_None);
	}
	[[nodiscard]] std::vector<MagicType> TownSpellIconTypes(entt::entity town) const override
	{
		return At(iconTypesOf, town, {});
	}
	[[nodiscard]] float BeliefNeededToConvert(entt::entity town) const override { return At(beliefNeeded, town, 0.0f); }

	uint32_t turn {0};
	PlayerNames me {PlayerNames::PLAYER_TWO};
	std::set<std::pair<PlayerNames, PlayerNames>> allies;
	std::map<PlayerNames, uint32_t> types;
	std::map<PlayerNames, entt::entity> playerThings;
	std::map<PlayerNames, float> powers;
	std::map<PlayerNames, MapCoords> fixedPlaces;
	std::map<PlayerNames, float> averages;
	std::map<std::pair<PlayerNames, PlayerNames>, float> harms;
	std::map<PlayerNames, float> alignments;
	std::map<uint32_t, uint32_t> forestGrown;
	std::map<uint32_t, entt::entity> forestCentres;
	std::set<entt::entity> treeSet;
	std::map<entt::entity, float> woodValues;
	std::map<entt::entity, uint32_t> kindWood;
	std::map<entt::entity, float> radii;
	MapCoords leaderHand {};
	float maxDistance {0.0f};
	bool inside {false};
	/// Places inside the influence, by their map coordinates, beside everywhere when `inside`
	std::set<std::pair<int32_t, int32_t>> insideAt;
	std::map<std::pair<int32_t, int32_t>, PlayerNames> influentialAt;

	std::set<entt::entity> towns;
	std::set<entt::entity> creatures;
	std::set<entt::entity> abodes;
	std::set<entt::entity> objects;
	std::map<entt::entity, MapCoords> places;
	std::map<entt::entity, float> lives;
	std::map<entt::entity, entt::entity> townOf;
	std::set<entt::entity> scripted;

	entt::entity myCreature {entt::null};
	std::vector<entt::entity> allCreatures;
	std::map<entt::entity, PlayerNames> owners;
	std::map<entt::entity, float> scales;
	std::map<entt::entity, float> energies;
	std::map<entt::entity, float> exhaustions;
	std::set<entt::entity> fighting;
	std::map<entt::entity, creature_body::BodyActivity> bodies;
	std::map<entt::entity, uint32_t> planDesires;
	std::optional<Leash> leash;
	entt::entity leaderTiedTo {entt::null};

	std::map<PlayerNames, std::vector<entt::entity>> townsOf;
	std::map<entt::entity, entt::entity> pits;
	std::map<entt::entity, entt::entity> worshipSites;
	std::map<entt::entity, float> townRadii;
	std::map<entt::entity, Area> areas;
	std::map<entt::entity, MapCoords> centres;
	std::map<entt::entity, std::vector<entt::entity>> abodesOf;
	std::map<entt::entity, PlayerNames> townOwners;
	std::map<std::pair<entt::entity, PlayerNames>, Aggression> aggressions;
	std::map<std::pair<entt::entity, PlayerNames>, ecs::town_aggression::AttackEffect> lastEffects;
	std::map<entt::entity, float> rivalRecent;
	std::map<entt::entity, float> beliefLeft;
	std::map<entt::entity, float> rivalBelief;
	std::map<entt::entity, MapCoords> doors;
	std::map<entt::entity, MapCoords> congregations;
	/// How many times a town's meeting point was asked for
	uint32_t congregationAsked {0};
	std::map<entt::entity, entt::entity> townCentres;
	std::map<std::pair<entt::entity, uint32_t>, float> boredoms;
	std::map<entt::entity, float> percentBuilt;
	std::map<entt::entity, float> abodeAttack;
	std::map<entt::entity, float> fieldAttack;

	std::set<entt::entity> burning;
	std::set<entt::entity> hot;

	std::optional<Citadel> citadel;
	entt::entity heart {entt::null};
	uint32_t heartBlowTurn {0};

	std::set<MagicType> enabled;
	std::map<MagicType, uint32_t> perceived;
	std::map<MagicType, float> impressive;
	std::map<MagicType, float> aggressive;
	std::map<MagicType, entt::entity> icons;
	std::map<entt::entity, entt::entity> iconSites;
	std::map<entt::entity, float> stores;
	std::map<entt::entity, float> costs;
	std::map<entt::entity, float> available;
	std::map<entt::entity, float> batteries;
	std::map<entt::entity, float> totals;
	std::map<entt::entity, float> supplies;
	std::set<entt::entity> built;
	std::set<entt::entity> repaired;
	std::map<entt::entity, uint32_t> worshippers;
	std::map<entt::entity, CastSpell> casts;

	MapCoords hand {};
	std::map<std::pair<entt::entity, uint32_t>, float> desires;
	std::map<entt::entity, float> lowEnergy;
	std::map<entt::entity, uint32_t> unknownActions;
	std::map<std::pair<entt::entity, CreatureDesires>, float> miracleKnowledge;
	std::map<std::pair<entt::entity, CreatureDesires>, std::optional<uint32_t>> miraclesLeft;
	std::map<entt::entity, std::optional<uint32_t>> normalLeft;
	std::map<ObjectType, entt::entity> nearestOfType;
	std::map<entt::entity, std::optional<MapCoords>> homes;
	mutable std::vector<std::tuple<MapCoords, ObjectType, float>> nearestAsked;
	std::set<entt::entity> spelled;
	std::map<entt::entity, std::vector<entt::entity>> fieldsOf;
	std::map<entt::entity, std::vector<entt::entity>> farmsOf;
	std::map<entt::entity, float> fieldFood;
	std::map<entt::entity, float> fieldCapacity;
	std::map<entt::entity, entt::entity> drops;
	std::vector<std::pair<entt::entity, uint32_t>> dropsAsked;
	std::map<std::pair<entt::entity, uint32_t>, uint32_t> pitResources;
	std::map<std::pair<entt::entity, uint32_t>, entt::entity> piles;
	std::map<entt::entity, uint32_t> pileAmounts;
	std::map<std::pair<entt::entity, uint32_t>, float> rawDesires;
	std::map<entt::entity, VillagerFacts> villagers;
	std::map<std::pair<int32_t, int32_t>, std::vector<entt::entity>> villagersAt;

	std::map<std::pair<entt::entity, uint32_t>, float> townDesires;
	std::map<entt::entity, float> desiredFood;
	std::map<entt::entity, uint32_t> foodAvailable;
	std::map<std::pair<entt::entity, uint32_t>, uint32_t> disciples;
	std::map<entt::entity, uint32_t> adults;
	std::map<entt::entity, uint32_t> children;
	std::map<entt::entity, int32_t> adultPlaces;
	std::map<entt::entity, float> totemWorship;
	std::map<entt::entity, TownWorship> townWorship;
	std::map<entt::entity, std::vector<entt::entity>> inhabitants;
	std::map<entt::entity, std::vector<entt::entity>> homeless;
	std::map<entt::entity, std::vector<entt::entity>> sitesOf;
	std::map<entt::entity, BuildingSiteFacts> buildingSites;
	std::set<entt::entity> worshipSiteSet;
	std::map<entt::entity, std::vector<entt::entity>> siteTowns;
	std::map<entt::entity, ScaffoldFacts> scaffolds;
	std::set<entt::entity> workshops;
	std::map<entt::entity, std::vector<entt::entity>> workshopScaffolds;
	std::set<entt::entity> functional;
	std::map<entt::entity, int32_t> space;
	std::map<entt::entity, float> mostObjects;
	uint32_t scaffoldWood {0};
	/// Whether the towns' planning finds a building, and where it was asked
	bool planWorks {true};
	struct Try
	{
		entt::entity town;
		MapCoords place;
		int32_t value;
	};
	std::vector<Try> triedAt;
	std::map<std::pair<int32_t, int32_t>, std::array<uint8_t, 4>> corners;
	Cells mapCells {512, 512};
	/// The things of each map cell in the order the game walks them, and the cells looked at, in order
	std::map<std::pair<uint16_t, uint16_t>, std::vector<entt::entity>> cellObjects;
	mutable std::vector<std::pair<uint16_t, uint16_t>> cellsAsked;
	std::map<entt::entity, ThrowFacts> throwFacts;
	float speed {0.0f};
	bool allSteep {false};
	std::array<entt::entity, 6> citadelSites {k_None, k_None, k_None, k_None, k_None, k_None};
	std::map<entt::entity, float> siteFoodDesires;
	std::map<entt::entity, float> siteRestDesires;
	std::map<entt::entity, std::vector<SiteWorshipper>> siteWorshippers;
	std::map<entt::entity, int32_t> siteFoodNeeded;
	std::map<entt::entity, std::vector<entt::entity>> siteIcons;
	std::set<entt::entity> charging;
	std::map<entt::entity, MagicType> iconTypes;
	std::vector<std::pair<entt::entity, float>> builtBy;
	std::map<entt::entity, MapCoords> gates;
	std::map<PlayerNames, entt::entity> rivalCreatures;
	std::map<entt::entity, std::vector<MagicType>> iconTypesOf;
	std::map<entt::entity, float> beliefNeeded;
	/// The synced generator's seed, the campaign's first
	uint32_t seed {game_random::k_InitialSeed};
};

} // namespace computer_score_fakes

using computer_score_fakes::FakeScoreWorld;
} // namespace openblack::test
