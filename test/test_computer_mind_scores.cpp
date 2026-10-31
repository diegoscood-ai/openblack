/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "ECS/ComputerPlayerScores.h"
#include "ECS/ComputerPlayerScoresDetail.h"

using namespace openblack;
using namespace openblack::computer_player;
using openblack::ecs::components::ComputerMind;
using openblack::map_coords::MapCoords;

namespace
{

float Bits(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t BitsOf(float value)
{
	return std::bit_cast<uint32_t>(value);
}

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

constexpr entt::entity k_Creature {1};
constexpr entt::entity k_Rival {2};
constexpr entt::entity k_Town {3};
constexpr entt::entity k_OtherTown {4};
constexpr entt::entity k_Pit {5};
constexpr entt::entity k_Abode {6};
constexpr entt::entity k_OtherAbode {7};
constexpr entt::entity k_Icon {8};
constexpr entt::entity k_Site {9};
constexpr entt::entity k_Target {10};
constexpr entt::entity k_PlayerOne {11};
constexpr entt::entity k_Spell {12};

/// The first node of the tree whose row scores that way
uint16_t NodeScoredBy(ScoreKind kind)
{
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		if (RowOf(node).score == kind)
		{
			return node;
		}
	}
	return k_RootNode;
}

/// The node's parent holds the thing, as the weighing of the nodes above would have chosen it

void GiveParent(ComputerMind& mind, uint16_t node, entt::entity thing)
{
	mind.nodes.at(*MindNodes()[node].parent).thing = thing;
}

float ScoreOf(ScoreKind kind, ComputerMind& mind, uint16_t node, FakeScoreWorld& world)
{
	return Score(kind, GameTree(), mind, node, world);
}

/// A town of the mind's player, where its influence reaches
struct TownAt
{
	int32_t x;
	int32_t z;
	float radius;
};

/// The influence the oracle ran over: a citadel, towns and the reach of the virtual influence
void LayOutInfluence(FakeScoreWorld& world, bool citadel, const std::vector<TownAt>& towns, float power, float maxDistance)
{
	if (citadel)
	{
		world.citadel = ScoreWorld::Citadel {.place = MapCoords {0x300000, 0x300000, 0.0f}, .influence = 50.0f};
	}
	auto& mine = world.townsOf[world.me];
	auto entity = static_cast<uint32_t>(k_Town);
	for (const auto& at : towns)
	{
		const entt::entity town {entity++};
		mine.push_back(town);
		world.places[town] = MapCoords {at.x, at.z, 0.0f};
		world.townRadii[town] = at.radius;
	}
	world.powers[world.me] = power;
	world.maxDistance = maxDistance;
}

/// An icon of a worship site, as the oracle made one
void LayOutIcon(FakeScoreWorld& world, bool site, float store, float cost, float available, float battery, float total,
                float supply)
{
	if (site)
	{
		world.iconSites[k_Icon] = k_Site;
	}
	world.stores[k_Icon] = store;
	world.costs[k_Icon] = cost;
	world.available[k_Site] = available;
	world.batteries[k_Site] = battery;
	world.totals[k_Site] = total;
	world.supplies[k_Site] = supply;
}

/// The mind's creature as the oracle made it, at (0x100000, 0x100000)
void LayOutCreature(FakeScoreWorld& world, float life, float exhaustion, float energy, float scale, uint32_t desire,
                    std::optional<uint32_t> tiedTurn, uint32_t turn, bool fighting)
{
	world.myCreature = k_Creature;
	world.creatures.insert(k_Creature);
	world.places[k_Creature] = MapCoords {0x100000, 0x100000, 0.0f};
	world.lives[k_Creature] = life;
	world.exhaustions[k_Creature] = exhaustion;
	world.energies[k_Creature] = energy;
	world.scales[k_Creature] = scale;
	world.planDesires[k_Creature] = desire;
	if (tiedTurn.has_value())
	{
		world.leash = ScoreWorld::Leash {.tiedTo = k_None, .tiedTurn = *tiedTurn};
	}
	world.turn = turn;
	if (fighting)
	{
		world.fighting.insert(k_Creature);
	}
}

} // namespace

TEST(ComputerMindScores, EveryWayOfScoringIsPortedOrScoresNothing)
{
	FakeScoreWorld world;
	size_t ported = 0;
	for (uint8_t value = 1; value <= 86; ++value)
	{
		const auto kind = static_cast<ScoreKind>(value);
		ported += IsScorePorted(kind) ? 1 : 0;
		auto mind = MakeMind();
		// with nothing in the game, every rule scores nothing and chooses nothing, but defeating a player, which is
		// always worth it
		EXPECT_EQ(ScoreOf(kind, mind, 1, world), kind == ScoreKind::DefeatPlayer ? 1.0f : 0.0f) << static_cast<int>(value);
		EXPECT_EQ(mind.nodes.at(1).thing, k_None) << static_cast<int>(value);
	}
	EXPECT_EQ(ported, 84u);
	EXPECT_FALSE(IsScorePorted(ScoreKind::None));
	EXPECT_FALSE(IsScorePorted(ScoreKind::GiveWorshippersRest));
	EXPECT_FALSE(IsScorePorted(ScoreKind::TakeVillagerFromWorshipSiteToTown));
	// and with nothing in the game no rule draws from the random generator
	EXPECT_EQ(world.seed, game_random::k_InitialSeed);
}

TEST(ComputerMindScores, TalkingGivingAndThrowingToThePlayerScoreNothing)
{
	FakeScoreWorld world;
	LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 0, std::nullopt, 100, false);
	auto mind = MakeMind();
	for (const auto kind : {ScoreKind::TalktoPlayer, ScoreKind::GiveToPlayer, ScoreKind::ThrowThingAtPlayer,
	                        ScoreKind::ReactToPlayerCastingSpells})
	{
		EXPECT_TRUE(IsScorePorted(kind));
		EXPECT_EQ(ScoreOf(kind, mind, 2, world), 0.0f);
	}
}

TEST(ComputerMindScores, TheInfluenceFactorOfAPlaceIsTheOriginals)
{
	// the oracle's cases, the place at (0x400000, 0x380000)
	const MapCoords place {0x400000, 0x380000, 0.0f};
	struct Case
	{
		bool inside;
		bool citadel;
		std::vector<TownAt> towns;
		float power;
		float maxDistance;
		MapCoords at;
		uint32_t factor;
		std::optional<std::pair<int32_t, int32_t>> nearest;
	};
	const std::vector<Case> cases {
	    {true, true, {}, 100.0f, 200.0f, place, 0x3F800000u, std::make_pair(0x400000, 0x380000)},
	    {false, true, {{0x420000, 0x3A0000, 30.0f}}, 100.0f, 200.0f, place, 0x3F7FFB46u, std::make_pair(0x3FE0F2, 0x37E0F2)},
	    {false, true, {}, 100.0f, 200.0f, place, 0x3C569026u, std::make_pair(0x347916, 0x323BFD)},
	    {false,
	     true,
	     {{0x420000, 0x3A0000, 30.0f}},
	     10.0f,
	     20.0f,
	     MapCoords {0x900000, 0x900000, 0.0f},
	     0x00000000u,
	     std::make_pair(0x440202, 0x3C3AA1)},
	    {false, false, {}, 100.0f, 200.0f, place, 0x00000000u, std::nullopt},
	    {false,
	     true,
	     {{0x200000, 0x380000, 30.0f}, {0x3F0000, 0x370000, 12.5f}},
	     37.5f,
	     150.0f,
	     place,
	     0x3F7FFB46u,
	     std::make_pair(0x3FE246, 0x37E246)},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.inside = c.inside;
		LayOutInfluence(world, c.citadel, c.towns, c.power, c.maxDistance);
		EXPECT_EQ(BitsOf(InfluenceFactor(world, c.at)), c.factor) << index;
		const auto nearest = NearestPlaceInInfluence(world, c.at);
		ASSERT_EQ(nearest.has_value(), c.nearest.has_value()) << index;
		if (nearest.has_value())
		{
			EXPECT_EQ(nearest->x, c.nearest->first) << index;
			EXPECT_EQ(nearest->z, c.nearest->second) << index;
		}
	}
}

TEST(ComputerMindScores, AnIconsKeennessAndTurnsAreTheOriginals)
{
	struct Case
	{
		bool site;
		float store, cost, available, battery, total, supply;
		uint32_t charge, turns;
	};
	const std::vector<Case> cases {
	    {true, 10.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f, 0x3E2E16A8u, 0x42200000u},
	    {true, 10.0f, 100.0f, 5.0f, 2.0f, 30.0f, 0.0f, 0x00000000u, 0xBF800000u},
	    {true, 120.0f, 100.0f, 5.0f, 2.0f, 130.0f, 0.0f, 0x3FA28F46u, 0x00000000u},
	    {false, 10.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f, 0x3DCCD185u, 0xBF800000u},
	    {true, 0.0f, 3000.0f, 17.25f, 0.5f, 40.0f, 0.75f, 0x3BC1E360u, 0x4576AAABu},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		LayOutIcon(world, c.site, c.store, c.cost, c.available, c.battery, c.total, c.supply);
		EXPECT_EQ(BitsOf(ChargeUpSpeedDesire(world, k_Icon)), c.charge) << index;
		EXPECT_EQ(BitsOf(TurnsToCreate(world, k_Icon)), c.turns) << index;
	}
}

TEST(ComputerMindScores, AMiraclesWorthIsTheOriginalsAndCountsEachIconFound)
{
	struct Case
	{
		bool enabled, icon;
		float store, cost, supply;
		bool built;
		uint32_t worth;
		bool chosen;
		uint32_t count;
	};
	// the oracle's cases, the food miracle at no place: available 5, battery 2, total 30, three worshippers
	const std::vector<Case> cases {
	    {true, true, 120.0f, 100.0f, 1.5f, true, 0x3F7FFDA3u, true, 1},
	    {true, true, 10.0f, 100.0f, 1.5f, true, 0x3E2E150Du, true, 2},
	    {true, true, 10.0f, 100.0f, 1.5f, false, 0x00000000u, false, 2},
	    {false, true, 10.0f, 100.0f, 1.5f, true, 0x00000000u, false, 0},
	    {true, false, 10.0f, 100.0f, 1.5f, true, 0x00000000u, false, 0},
	    {true, true, 10.0f, 3000.0f, 0.5f, true, 0x345B450Cu, true, 2},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.citadel = ScoreWorld::Citadel {};
		LayOutIcon(world, true, c.store, c.cost, 5.0f, 2.0f, 30.0f, c.supply);
		if (c.enabled)
		{
			world.enabled.insert(MagicType::Food);
		}
		if (c.icon)
		{
			world.icons[MagicType::Food] = k_Icon;
		}
		if (c.built)
		{
			world.built.insert(k_Site);
		}
		world.repaired.insert(k_Site);
		world.worshippers[k_Site] = 3;
		auto mind = MakeMind();
		entt::entity icon = k_None;
		EXPECT_EQ(BitsOf(SpellWorth(world, mind, MagicType::Food, std::nullopt, icon)), c.worth) << index;
		EXPECT_EQ(icon, c.chosen ? k_Icon : k_None) << index;
		EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Food)), c.count) << index;
	}
}

TEST(ComputerMindScores, TheCreaturesFreeTimeAndWorthForAJobAreTheOriginals)
{
	struct Case
	{
		float life, exhaustion, energy, scale;
		uint32_t desire;
		std::optional<uint32_t> tiedTurn;
		uint32_t turn;
		bool fighting;
		MapCoords target;
		uint32_t free, job;
	};
	const std::vector<Case> cases {
	    {1.0f, 0.1f, 1.0f, 1.0f, 3, std::nullopt, 5000, false, {0x80000, 0, 0.0f}, 0x3F800000u, 0x3F6638AEu},
	    {1.0f, 0.1f, 1.0f, 1.0f, 3, 1000u, 1400, false, {0x80000, 0, 0.0f}, 0x00000000u, 0x00000000u},
	    {1.0f, 0.1f, 1.0f, 1.0f, 3, 1000u, 1450, false, {0x80000, 0, 0.0f}, 0x00000000u, 0x00000000u},
	    {0.375f, 0.625f, 0.25f, 0.8f, 3, 1000u, 1700, false, {0x80000, 0, 0.0f}, 0x3CA00000u, 0x3BEFD060u},
	    {0.375f, 0.625f, 0.25f, 0.8f, 15, 1000u, 1700, false, {0x80000, 0, 0.0f}, 0x00000000u, 0x00000000u},
	    {0.9f, 0.2f, 0.75f, 1.3f, 3, 1000u, 2900, false, {0x500000, 0x234567, 0.0f}, 0x3F800000u, 0x3E589DCFu},
	    {0.9f, 0.2f, 0.75f, 1.3f, 3, 1000u, 2900, true, {0x500000, 0x234567, 0.0f}, 0x00000000u, 0x00000000u},
	    {0.9f, 0.2f, 0.75f, 1.3f, 3, 0u, 123457, false, {0x1500000, 0x34567, 0.0f}, 0x3F800000u, 0x37F1D3D5u},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		LayOutCreature(world, c.life, c.exhaustion, c.energy, c.scale, c.desire, c.tiedTurn, c.turn, c.fighting);
		world.places[k_Target] = c.target;
		EXPECT_EQ(BitsOf(CreatureFreeTime(world)), c.free) << index;
		EXPECT_EQ(BitsOf(CreatureJobWorth(world, k_Target, false)), c.job) << index;
	}
	// a creature a script controls is no creature of the mind's
	FakeScoreWorld world;
	LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 3, std::nullopt, 10, false);
	world.scripted.insert(k_Creature);
	EXPECT_EQ(PlayersCreature(world), k_None);
	EXPECT_EQ(CreatureFreeTime(world), 0.0f);
}

TEST(ComputerMindScores, ABodyIsBusyWithAnythingThatMovesIt)
{
	using creature_body::BodyActivity;
	EXPECT_FALSE(creature_body::IsPerformingAction(BodyActivity {}));
	EXPECT_TRUE(creature_body::IsPerformingAction(BodyActivity {.playing = true}));
	EXPECT_TRUE(creature_body::IsPerformingAction(BodyActivity {.moving = true}));
	EXPECT_TRUE(creature_body::IsPerformingAction(BodyActivity {.handling = true}));
	EXPECT_TRUE(creature_body::IsPerformingAction(BodyActivity {.fighting = true}));
}

TEST(ComputerMindScores, TheCreatureIsLookedAfterOnlyWhileItsBodyIsFree)
{
	// the oracle's cases: the body standing free, walking, turning on the spot, lying fainted, in a sit's loop
	using creature_body::BodyActivity;
	struct Case
	{
		BodyActivity body;
		float life, exhaustion, energy;
		std::optional<uint32_t> tiedTurn;
		uint32_t turn;
		bool fighting;
		uint32_t score;
	};
	const std::vector<Case> cases {
	    {{}, 1.0f, 0.1f, 1.0f, std::nullopt, 5000, false, 0x3F800000u},
	    {{.moving = true}, 1.0f, 0.1f, 1.0f, std::nullopt, 5000, false, 0x00000000u},
	    {{.moving = true}, 1.0f, 0.1f, 1.0f, std::nullopt, 5000, false, 0x00000000u},
	    {{.fighting = true}, 1.0f, 0.1f, 1.0f, std::nullopt, 5000, false, 0x00000000u},
	    {{}, 0.375f, 0.625f, 0.25f, 1000u, 1700, false, 0x3CA00000u},
	    {{.playing = true}, 0.375f, 0.625f, 0.25f, 1000u, 1700, false, 0x00000000u},
	    {{}, 1.0f, 0.1f, 1.0f, 1000u, 1450, false, 0x00000000u},
	    {{}, 0.9f, 0.2f, 0.75f, 1000u, 2900, true, 0x00000000u},
	};
	const auto node = NodeScoredBy(ScoreKind::CreatureInteract);
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		LayOutCreature(world, c.life, c.exhaustion, c.energy, 1.0f, 3, c.tiedTurn, c.turn, c.fighting);
		world.bodies[k_Creature] = c.body;
		auto mind = MakeMind();
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::CreatureInteract, mind, node, world)), c.score) << index;
		EXPECT_EQ(mind.nodes.at(node).thing, k_None) << index;
	}
	// a creature a script controls, or none at all, is not looked after
	FakeScoreWorld world;
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::CreatureInteract, mind, node, world), 0.0f);
	LayOutCreature(world, 1.0f, 0.1f, 1.0f, 1.0f, 3, std::nullopt, 5000, false);
	world.scripted.insert(k_Creature);
	EXPECT_EQ(ScoreOf(ScoreKind::CreatureInteract, mind, node, world), 0.0f);
	EXPECT_TRUE(IsScorePorted(ScoreKind::CreatureInteract));
}

TEST(ComputerMindScores, ATownsRadiusIsHalfItsLargerSideInWholeMetres)
{
	// the oracle's three areas
	EXPECT_EQ(TownRadius({MapCoords {0x640000, 0xC80000, 0.0f}, MapCoords {0x820000, 0xD20000, 0.0f}}), 150.0f);
	EXPECT_EQ(TownRadius({MapCoords {0x53039, 0x9004D, 0.0f}, MapCoords {0x70001, 0x114321, 0.0f}}), 41.0f);
	EXPECT_EQ(TownRadius({MapCoords {0x12C8000, 0xFA0000, 0.0f}, MapCoords {0x12D7FFF, 0x106FFFF, 0.0f}}), 64.5f);
}

TEST(ComputerMindScores, AMiracleCastNearbyIsNotCastAgain)
{
	FakeScoreWorld world;
	auto mind = MakeMind();
	const MapCoords place {0x100000, 0x100000, 0.0f};
	EXPECT_FALSE(CastNear(world, mind, place, MagicType::Shield, 0.0f));
	mind.spellsCast.push_back({.spell = k_Spell, .place = {0x101FFF, 0x0FE001, 0.0f}});
	world.casts[k_Spell] = {.type = MagicType::Shield, .radius = 3.0f};
	EXPECT_TRUE(CastNear(world, mind, place, MagicType::Shield, 0.0f));
	EXPECT_FALSE(CastNear(world, mind, place, MagicType::PhysicalShield, 0.0f));
	// a reach below the one asked for does not count, unless any reach does
	EXPECT_TRUE(CastNear(world, mind, place, MagicType::Shield, 3.0f));
	EXPECT_FALSE(CastNear(world, mind, place, MagicType::Shield, 3.5f));
	// an eighth of a cell away is not near
	mind.spellsCast[0].place.x = 0x102000;
	EXPECT_FALSE(CastNear(world, mind, place, MagicType::Shield, 0.0f));
	// a new land forgets the miracles cast on the last one
	StartNewLand(mind);
	EXPECT_TRUE(mind.spellsCast.empty());
}

TEST(ComputerMindScores, TheCreatureImpressesATownWithAStoreUnlessItsLeashIsTiedThere)
{
	FakeScoreWorld world;
	LayOutCreature(world, 1.0f, 0.1f, 1.0f, 1.0f, 3, std::nullopt, 5000, false);
	world.towns.insert(k_Town);
	world.places[k_Pit] = MapCoords {0x80000, 0, 0.0f};
	for (const auto kind : {ScoreKind::UseCreatureToImpressTown, ScoreKind::GetCreaturetoHelp})
	{
		const auto node = NodeScoredBy(kind);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		// without a store, nothing
		world.pits.clear();
		EXPECT_EQ(ScoreOf(kind, mind, node, world), 0.0f);
		world.pits[k_Town] = k_Pit;
		EXPECT_EQ(BitsOf(ScoreOf(kind, mind, node, world)), 0x3F6638AEu);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Creature);
		// tied to a thing of the town: nothing
		mind.nodes.at(node).thing = k_None;
		world.leaderTiedTo = k_Abode;
		world.townOf[k_Abode] = k_Town;
		EXPECT_EQ(ScoreOf(kind, mind, node, world), 0.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_None);
		world.leaderTiedTo = k_None;
	}
}

TEST(ComputerMindScores, TheCreatureFightsTheCreatureAboveUnlessItsLeashIsTiedToIt)
{
	FakeScoreWorld world;
	LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 3, std::nullopt, 10, false);
	world.creatures.insert(k_Rival);
	const auto node = NodeScoredBy(ScoreKind::AttackCreatureWithCreature);
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::AttackCreatureWithCreature, mind, node, world), 0.0f);
	GiveParent(mind, node, k_Rival);
	EXPECT_EQ(ScoreOf(ScoreKind::AttackCreatureWithCreature, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Creature);
	world.leaderTiedTo = k_Rival;
	EXPECT_EQ(ScoreOf(ScoreKind::AttackCreatureWithCreature, mind, node, world), 0.0f);
}

TEST(ComputerMindScores, TheMostThreateningCreatureIsTheOneNearestTheCitadelAndTowns)
{
	FakeScoreWorld world;
	world.citadel = ScoreWorld::Citadel {.place = MapCoords {0x100000, 0x100000, 0.0f}, .influence = 50.0f};
	world.powers[world.me] = 99.0f;
	LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 3, std::nullopt, 10, false);
	constexpr entt::entity k_Far {20};
	constexpr entt::entity k_Near {21};
	constexpr entt::entity k_Ally {22};
	world.allCreatures = {k_Creature, k_Far, k_Near, k_Ally};
	world.places[k_Far] = MapCoords {0x150000, 0x100000, 0.0f};
	world.places[k_Near] = MapCoords {0x108000, 0x100000, 0.0f};
	world.places[k_Ally] = MapCoords {0x100000, 0x100000, 0.0f};
	for (const auto creature : world.allCreatures)
	{
		const auto place = world.places[creature];
		world.influentialAt[{place.x, place.z}] = world.me;
	}
	world.owners[k_Ally] = PlayerNames::PLAYER_THREE;
	world.allies.insert({PlayerNames::PLAYER_THREE, world.me});
	const auto node = NodeScoredBy(ScoreKind::ReactToAggressiveCreature);
	auto mind = MakeMind();
	const float expected =
	    gutils::GetDistanceModifier(gutils::GetDistanceInMetres(world.places[k_Near], world.citadel->place), 100.0f);
	EXPECT_EQ(ScoreOf(ScoreKind::ReactToAggressiveCreature, mind, node, world), expected);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Near);
	// a town nearer than its area adds 1, and its influence's fall-off: the score stops at 1
	world.townsOf[world.me] = {k_Town};
	world.centres[k_Town] = world.places[k_Far];
	world.areas[k_Town] = {MapCoords {0, 0, 0.0f}, MapCoords {0x10000, 0x10000, 0.0f}};
	world.townRadii[k_Town] = 40.0f;
	EXPECT_EQ(ScoreOf(ScoreKind::ReactToAggressiveCreature, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Far);
}

TEST(ComputerMindScores, AShieldOrWaterIsWorthTwiceTheMiracleUpToOne)
{
	FakeScoreWorld world;
	world.citadel = ScoreWorld::Citadel {};
	world.inside = true;
	LayOutIcon(world, true, 10.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
	world.built.insert(k_Site);
	world.repaired.insert(k_Site);
	world.worshippers[k_Site] = 3;
	for (const auto type : {MagicType::PhysicalShield, MagicType::Water})
	{
		world.enabled.insert(type);
		world.icons[type] = k_Icon;
	}
	world.objects.insert(k_Target);
	world.places[k_Target] = MapCoords {0x10000, 0x10000, 0.0f};
	const float once = Bits(0x3E2E150Du);
	{
		const auto node = NodeScoredBy(ScoreKind::CastPhysicalShieldToProtect);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Target);
		EXPECT_EQ(ScoreOf(ScoreKind::CastPhysicalShieldToProtect, mind, node, world), once + once);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
		// asked twice, each time with two looks at the icon
		EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::PhysicalShield)), 4u);
		// cast there already: nothing
		mind.spellsCast.push_back({.spell = k_Spell, .place = world.places[k_Target]});
		world.casts[k_Spell] = {.type = MagicType::PhysicalShield, .radius = 1.0f};
		EXPECT_EQ(ScoreOf(ScoreKind::CastPhysicalShieldToProtect, mind, node, world), 0.0f);
	}
	{
		const auto node = NodeScoredBy(ScoreKind::CastWaterSpell);
		auto mind = MakeMind();
		EXPECT_EQ(ScoreOf(ScoreKind::CastWaterSpell, mind, node, world), once + once);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
		// a keen icon is worth 1 at once, asked once
		world.stores[k_Icon] = 120.0f;
		auto keen = MakeMind();
		EXPECT_EQ(ScoreOf(ScoreKind::CastWaterSpell, keen, node, world), 1.0f);
		EXPECT_EQ(keen.iconsFound.at(static_cast<size_t>(MagicType::Water)), 1u);
	}
}

TEST(ComputerMindScores, TheHeartStruckLatelyByAThingThatBreaksBuildingsIsReactedTo)
{
	// the oracle's cases: the heart's blow turn and the game turn
	struct Case
	{
		uint32_t struck, turn;
		uint32_t score;
		bool heartChosen;
	};
	const std::vector<Case> cases {
	    {0, 5000, 0x00000000u, false},   {1000, 1020, 0x00000000u, false}, {1000, 1021, 0x3F770A3Du, true},
	    {1000, 1300, 0x3F000000u, true}, {1000, 1599, 0x3ADA7400u, true},  {1000, 1600, 0x00000000u, true},
	    {1000, 1601, 0x00000000u, true}, {1000, 1137, 0x3F458BF2u, true},  {5000, 1000, 0x00000000u, true},
	    {1, 1, 0x00000000u, false},      {77, 400, 0x3EEC5F92u, true},     {1, 17000003, 0x00000000u, true},
	};
	const auto node = NodeScoredBy(ScoreKind::ReactToPlayerThrowingStuff);
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.heart = k_Target;
		world.heartBlowTurn = c.struck;
		world.turn = c.turn;
		auto mind = MakeMind();
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::ReactToPlayerThrowingStuff, mind, node, world)), c.score) << index;
		EXPECT_EQ(mind.nodes.at(node).thing, c.heartChosen ? k_Target : k_None) << index;
	}
	// without a citadel there is no heart to react for
	FakeScoreWorld world;
	world.heartBlowTurn = 1000;
	world.turn = 1300;
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::ReactToPlayerThrowingStuff, mind, node, world), 0.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_None);
	EXPECT_TRUE(IsScorePorted(ScoreKind::ReactToPlayerThrowingStuff));
}

TEST(ComputerMindScores, TheTownMostHarmedLatelyIsDefendedAgainstItsAttacker)
{
	FakeScoreWorld world;
	world.turn = 1000;
	world.townsOf[world.me] = {k_Town, k_OtherTown};
	world.types[PlayerNames::PLAYER_ONE] = 1;
	world.types[PlayerNames::PLAYER_THREE] = 2;
	world.types[world.me] = 2;
	world.playerThings[PlayerNames::PLAYER_ONE] = k_PlayerOne;
	world.aggressions[{k_Town, PlayerNames::PLAYER_ONE}] = {.aggression = 0.25f, .lastTurn = 700};
	world.aggressions[{k_OtherTown, PlayerNames::PLAYER_THREE}] = {.aggression = 0.1f, .lastTurn = 900};
	// the mind's own harm and players of no type do not count
	world.aggressions[{k_OtherTown, world.me}] = {.aggression = 5.0f, .lastTurn = 999};
	world.aggressions[{k_OtherTown, PlayerNames::PLAYER_FOUR}] = {.aggression = 5.0f, .lastTurn = 999};
	const auto node = NodeScoredBy(ScoreKind::DefendTown);
	auto mind = MakeMind();
	const auto parent = *MindNodes()[node].parent;
	// (1 - 300/1200)^3 x 0.5 against (1 - 100/1200)^3 x 0.2
	const float first = 0.75f * 0.75f * 0.75f * 0.5f;
	EXPECT_EQ(ScoreOf(ScoreKind::DefendTown, mind, node, world), first);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Town);
	EXPECT_EQ(mind.nodes.at(parent).thing, k_PlayerOne);
	// with a thing above, the parent keeps nothing new
	auto other = MakeMind();
	other.nodes.at(0).thing = k_Target;
	EXPECT_EQ(ScoreOf(ScoreKind::DefendTown, other, node, world), first);
	EXPECT_EQ(other.nodes.at(parent).thing, k_None);
	// a town harmed by nobody is still the choice when it is the only one, at 0
	FakeScoreWorld calm;
	calm.townsOf[calm.me] = {k_Town};
	calm.types[PlayerNames::PLAYER_ONE] = 1;
	calm.aggressions[{k_Town, PlayerNames::PLAYER_ONE}] = {.aggression = 0.5f, .lastTurn = 0};
	calm.turn = 5000;
	auto quiet = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::DefendTown, quiet, node, calm), 0.0f);
	EXPECT_EQ(quiet.nodes.at(node).thing, k_Town);
}

namespace
{
/// A player's last attack on a town, as the oracle laid it out: its turn, burn, crush and hit
struct LastAttack
{
	uint8_t player;
	uint32_t turn;
	float burn, crush, hit;
};

void LayOutAttacks(FakeScoreWorld& world, entt::entity town, const std::vector<LastAttack>& attacks)
{
	for (const auto& a : attacks)
	{
		const auto player = static_cast<PlayerNames>(a.player);
		world.aggressions[{town, player}] = {.aggression = 1.0f, .lastTurn = a.turn};
		world.lastEffects[{town, player}] = {.burn = a.burn, .crush = a.crush, .hit = a.hit};
	}
}

/// A shield and a physical shield that the mind can cast, as the oracle's icon (store 10 or 120 of 100)
void LayOutShields(FakeScoreWorld& world, float store)
{
	world.citadel = ScoreWorld::Citadel {};
	world.inside = true;
	LayOutIcon(world, true, store, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
	world.built.insert(k_Site);
	world.repaired.insert(k_Site);
	world.worshippers[k_Site] = 3;
	for (const auto type : {MagicType::Shield, MagicType::PhysicalShield})
	{
		world.enabled.insert(type);
		world.icons[type] = k_Icon;
	}
}

/// The mind cast this miracle at the place already
void CastAt(FakeScoreWorld& world, ComputerMind& mind, const MapCoords& place, MagicType type)
{
	mind.spellsCast.push_back({.spell = k_Spell, .place = place});
	world.casts[k_Spell] = {.type = type, .radius = 1.0f};
}
} // namespace

TEST(ComputerMindScores, ATownsHarmLatelyIsTheOriginals)
{
	// the oracle's cases: the game turn and the players' last attacks
	struct Case
	{
		uint32_t turn;
		std::vector<LastAttack> attacks;
		std::optional<std::array<uint32_t, 3>> harm;
	};
	const std::vector<Case> cases {
	    {5000, {}, std::nullopt},
	    {1000, {{1, 900, 3.0f, 0.0f, 0.0f}}, std::array<uint32_t, 3> {0x3F7ED098u, 0x40400000u, 0x00000000u}},
	    {1000, {{1, 900, 0.0f, 0.5f, 0.25f}}, std::array<uint32_t, 3> {0x3F7ED098u, 0x00000000u, 0x3F400000u}},
	    // the most recent attack wins, and of two as recent the first player's
	    {1000,
	     {{1, 700, 3.0f, 0.0f, 0.0f}, {2, 950, 0.0f, 1.0f, 2.0f}},
	     std::array<uint32_t, 3> {0x3F7FDA13u, 0x00000000u, 0x40400000u}},
	    {1000,
	     {{1, 950, 3.0f, 0.0f, 0.0f}, {2, 950, 0.0f, 1.0f, 2.0f}},
	     std::array<uint32_t, 3> {0x3F7FDA13u, 0x40400000u, 0x00000000u}},
	    // 600 turns on, it is forgotten
	    {1000, {{1, 400, 3.0f, 0.0f, 0.0f}}, std::nullopt},
	    {1000, {{1, 399, 3.0f, 0.0f, 0.0f}}, std::nullopt},
	    // early in the game a player who never attacked counts as an attack at turn 0 that did nothing
	    {300, {}, std::nullopt},
	    {300, {{3, 100, 0.0f, 0.0f, 0.0f}}, std::nullopt},
	    // the neutral player's attacks are not counted
	    {1000, {{7, 999, 9.0f, 9.0f, 9.0f}}, std::nullopt},
	    // an attack after the game turn counts as long ago
	    {1000, {{1, 1200, 3.0f, 0.0f, 0.0f}}, std::nullopt},
	    // a later attack that did nothing hides an earlier one
	    {1237, {{0, 1000, 0.125f, 0.0f, 0.0f}, {4, 1001, 0.0f, 0.0f, 0.0f}}, std::nullopt},
	    {20000003, {{2, 20000000, 1.0f, 0.0f, 0.0f}}, std::array<uint32_t, 3> {0x3F7FFFFEu, 0x3F800000u, 0x00000000u}},
	    // a burn below 0 is no burn
	    {1000, {{1, 1000, -1.0f, 0.0f, 0.0f}}, std::nullopt},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.turn = c.turn;
		LayOutAttacks(world, k_Town, c.attacks);
		const auto harm = scores::HighestRecentHarm(world, k_Town);
		ASSERT_EQ(harm.has_value(), c.harm.has_value()) << index;
		if (harm.has_value())
		{
			EXPECT_EQ(BitsOf(harm->fade), (*c.harm)[0]) << index;
			EXPECT_EQ(BitsOf(harm->burn), (*c.harm)[1]) << index;
			EXPECT_EQ(BitsOf(harm->physical), (*c.harm)[2]) << index;
		}
	}
}

TEST(ComputerMindScores, ATownHarmedLatelyIsShieldedAgainstWhatHarmedIt)
{
	const auto node = NodeScoredBy(ScoreKind::DefendtownWithSpells);
	const MapCoords centre {0x200000, 0x400000, 0.0f};
	const auto layOut = [&](FakeScoreWorld& world, uint32_t turn, const std::vector<LastAttack>& attacks, float store) {
		world.turn = turn;
		world.towns.insert(k_Town);
		world.centres[k_Town] = centre;
		LayOutAttacks(world, k_Town, attacks);
		LayOutShields(world, store);
	};
	const float once = Bits(0x3EAD46BBu);
	{
		// a burn: a shield, twice the miracle's worth times the fade
		FakeScoreWorld world;
		layOut(world, 1000, {{1, 900, 3.0f, 0.0f, 0.0f}}, 10.0f);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		CastAt(world, mind, centre, MagicType::PhysicalShield);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::DefendtownWithSpells, mind, node, world)), BitsOf(once));
		EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
		// a shield cast there already: nothing
		auto shielded = MakeMind();
		GiveParent(shielded, node, k_Town);
		CastAt(world, shielded, centre, MagicType::Shield);
		EXPECT_EQ(ScoreOf(ScoreKind::DefendtownWithSpells, shielded, node, world), 0.0f);
		EXPECT_EQ(shielded.nodes.at(node).thing, k_None);
	}
	// a crush and hit, or as much burn as crush and hit: a physical shield
	for (const auto& attack : {LastAttack {1, 900, 0.0f, 0.5f, 0.25f}, LastAttack {1, 900, 1.0f, 0.5f, 0.5f}})
	{
		FakeScoreWorld world;
		layOut(world, 1000, {attack}, 10.0f);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		CastAt(world, mind, centre, MagicType::Shield);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::DefendtownWithSpells, mind, node, world)), BitsOf(once));
		auto shielded = MakeMind();
		GiveParent(shielded, node, k_Town);
		CastAt(world, shielded, centre, MagicType::PhysicalShield);
		EXPECT_EQ(ScoreOf(ScoreKind::DefendtownWithSpells, shielded, node, world), 0.0f);
	}
	{
		// a keen icon: not kept to 1
		FakeScoreWorld world;
		layOut(world, 1000, {{1, 900, 3.0f, 0.0f, 0.0f}}, 120.0f);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::DefendtownWithSpells, mind, node, world)), 0x3FFECE3Eu);
	}
	// nothing: the harm forgotten, no town above, no icon, the miracle not allowed, the latest attack doing nothing
	const auto nothing = [&](FakeScoreWorld& world, ComputerMind& mind) {
		EXPECT_EQ(ScoreOf(ScoreKind::DefendtownWithSpells, mind, node, world), 0.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_None);
	};
	{
		FakeScoreWorld world;
		layOut(world, 5000, {{1, 900, 3.0f, 0.0f, 0.0f}}, 10.0f);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		nothing(world, mind);
		auto none = MakeMind();
		nothing(world, none);
	}
	{
		FakeScoreWorld world;
		layOut(world, 1000, {{1, 900, 3.0f, 0.0f, 0.0f}}, 10.0f);
		world.icons.clear();
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		nothing(world, mind);
		world.icons[MagicType::Shield] = k_Icon;
		world.enabled.erase(MagicType::Shield);
		nothing(world, mind);
	}
	{
		FakeScoreWorld world;
		layOut(world, 1000, {{1, 500, 0.0f, 2.0f, 0.0f}, {2, 999, 0.0f, 0.0f, 0.0f}}, 10.0f);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		nothing(world, mind);
	}
	EXPECT_TRUE(IsScorePorted(ScoreKind::DefendtownWithSpells));
}

TEST(ComputerMindScores, TheTownHarmedMostLatelyIsCheckedForProtection)
{
	const auto node = NodeScoredBy(ScoreKind::CheckTownForProtection);
	constexpr entt::entity k_ThirdTown {13};
	const std::array<entt::entity, 3> towns {k_Town, k_OtherTown, k_ThirdTown};
	const auto layOut = [&](FakeScoreWorld& world, uint32_t turn, const std::vector<std::vector<LastAttack>>& attacks) {
		world.turn = turn;
		for (size_t k = 0; k < attacks.size(); ++k)
		{
			world.townsOf[world.me].push_back(towns.at(k));
			world.centres[towns.at(k)] = MapCoords {static_cast<int32_t>(0x100000 * (k + 1)), 0x400000, 0.0f};
			LayOutAttacks(world, towns.at(k), attacks[k]);
		}
	};
	struct Case
	{
		uint32_t turn;
		std::vector<std::vector<LastAttack>> attacks;
		uint32_t score;
		entt::entity town;
	};
	const std::vector<Case> cases {
	    {1000, {}, 0x00000000u, k_None},
	    {1000, {{{1, 900, 3.0f, 0.0f, 0.0f}}}, 0x3F7ED098u, k_Town},
	    {1000, {{{1, 900, 3.0f, 0.0f, 0.0f}}, {{1, 950, 0.0f, 1.0f, 0.0f}}}, 0x3F7FDA13u, k_OtherTown},
	    {1000, {{{1, 950, 3.0f, 0.0f, 0.0f}}, {{1, 900, 0.0f, 1.0f, 0.0f}}}, 0x3F7FDA13u, k_Town},
	    {1000, {{{1, 900, 0.0f, 0.0f, 0.0f}}}, 0x00000000u, k_None},
	    {5000, {{{1, 900, 3.0f, 0.0f, 0.0f}}}, 0x00000000u, k_None},
	    // a town harmed as lately as the best is passed over
	    {1000,
	     {{{1, 950, 3.0f, 0.0f, 0.0f}}, {{2, 950, 0.0f, 1.0f, 0.0f}}, {{3, 960, 0.5f, 0.0f, 0.0f}}},
	     0x3F7FEC95u,
	     k_ThirdTown},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		layOut(world, c.turn, c.attacks);
		auto mind = MakeMind();
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::CheckTownForProtection, mind, node, world)), c.score) << index;
		EXPECT_EQ(mind.nodes.at(node).thing, c.town) << index;
	}
	{
		// as much burn as crush and hit: a shield is looked for; one cast at the town passes it over
		FakeScoreWorld world;
		layOut(world, 1000, {{{1, 900, 1.0f, 0.5f, 0.5f}}});
		auto mind = MakeMind();
		CastAt(world, mind, world.centres[k_Town], MagicType::PhysicalShield);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::CheckTownForProtection, mind, node, world)), 0x3F7ED098u);
		auto shielded = MakeMind();
		CastAt(world, shielded, world.centres[k_Town], MagicType::Shield);
		EXPECT_EQ(ScoreOf(ScoreKind::CheckTownForProtection, shielded, node, world), 0.0f);
		EXPECT_EQ(shielded.nodes.at(node).thing, k_None);
	}
	{
		// the most recent town shielded already: the next one is taken
		FakeScoreWorld world;
		layOut(world, 1000, {{{1, 950, 3.0f, 0.0f, 0.0f}}, {{1, 900, 0.0f, 1.0f, 0.0f}}});
		auto mind = MakeMind();
		CastAt(world, mind, world.centres[k_Town], MagicType::Shield);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::CheckTownForProtection, mind, node, world)), 0x3F7ED098u);
		EXPECT_EQ(mind.nodes.at(node).thing, k_OtherTown);
	}
	EXPECT_TRUE(IsScorePorted(ScoreKind::CheckTownForProtection));
}

TEST(ComputerMindScores, TheWorstDamagedBuildingNeedsARepair)
{
	FakeScoreWorld world;
	world.inside = true;
	world.townsOf[world.me] = {k_Town};
	world.abodesOf[k_Town] = {k_Abode, k_OtherAbode};
	world.lives[k_Abode] = 0.95f;
	world.lives[k_OtherAbode] = 0.92f;
	const auto node = NodeScoredBy(ScoreKind::CheckTownForDamage);
	auto mind = MakeMind();
	const float square = 0.92f * 0.92f;
	EXPECT_EQ(ScoreOf(ScoreKind::CheckTownForDamage, mind, node, world), 1.0f - square);
	EXPECT_EQ(mind.nodes.at(node).thing, k_OtherAbode);
	world.lives[k_Abode] = 0.5f;
	EXPECT_EQ(ScoreOf(ScoreKind::CheckTownForDamage, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Abode);
	// a building in full life is no damage
	world.lives[k_Abode] = 1.0f;
	world.lives[k_OtherAbode] = 1.0f;
	auto whole = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::CheckTownForDamage, whole, node, world), 0.0f);
	EXPECT_EQ(whole.nodes.at(node).thing, k_None);
}

TEST(ComputerMindScores, ABuildingAboveBurnsOrIsDamagedOrIsUnprotected)
{
	FakeScoreWorld world;
	world.abodes.insert(k_Abode);
	auto mind = MakeMind();
	const auto burning = NodeScoredBy(ScoreKind::FindBurningAbode);
	const auto damaged = NodeScoredBy(ScoreKind::FindDamagedAbode);
	const auto unprotected = NodeScoredBy(ScoreKind::FindUnprotectedAbode);
	for (const auto node : {burning, damaged, unprotected})
	{
		GiveParent(mind, node, k_Abode);
	}
	EXPECT_EQ(ScoreOf(ScoreKind::FindBurningAbode, mind, burning, world), 0.0f);
	EXPECT_EQ(ScoreOf(ScoreKind::FindDamagedAbode, mind, damaged, world), 1.0f);
	world.burning.insert(k_Abode);
	EXPECT_EQ(ScoreOf(ScoreKind::FindBurningAbode, mind, burning, world), 0.0f);
	world.hot.insert(k_Abode);
	EXPECT_EQ(ScoreOf(ScoreKind::FindBurningAbode, mind, burning, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(burning).thing, k_Abode);
	mind.nodes.at(damaged).thing = k_None;
	EXPECT_EQ(ScoreOf(ScoreKind::FindDamagedAbode, mind, damaged, world), 0.0f);
	// a burning building wants a shield; the mind cast a physical one there, which does not count
	world.places[k_Abode] = MapCoords {0x10000, 0x10000, 0.0f};
	mind.spellsCast.push_back({.spell = k_Spell, .place = world.places[k_Abode]});
	world.casts[k_Spell] = {.type = MagicType::PhysicalShield, .radius = 1.0f};
	EXPECT_EQ(ScoreOf(ScoreKind::FindUnprotectedAbode, mind, unprotected, world), 1.0f);
	world.burning.clear();
	EXPECT_EQ(ScoreOf(ScoreKind::FindUnprotectedAbode, mind, unprotected, world), 0.0f);
}

TEST(ComputerMindScores, TheTownMostAttackedByBeliefIsDefendedWithAShield)
{
	FakeScoreWorld world;
	world.townsOf[world.me] = {k_Town, k_OtherTown};
	world.towns = {k_Town, k_OtherTown};
	world.rivalRecent[k_Town] = 0.5f;
	world.rivalRecent[k_OtherTown] = 0.75f;
	world.beliefLeft[k_Town] = 1.0f;
	// a town that believes as much in another player is not a town under attack
	world.beliefLeft[k_OtherTown] = 0.0f;
	const auto check = NodeScoredBy(ScoreKind::CheckTownsForBeliefAttack);
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::CheckTownsForBeliefAttack, mind, check, world), 0.5f);
	EXPECT_EQ(mind.nodes.at(check).thing, k_Town);

	world.citadel = ScoreWorld::Citadel {};
	world.inside = true;
	LayOutIcon(world, true, 120.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
	world.enabled = {MagicType::Shield, MagicType::PhysicalShield};
	world.icons[MagicType::Shield] = k_Icon;
	world.icons[MagicType::PhysicalShield] = k_Icon;
	const auto cast = NodeScoredBy(ScoreKind::CastSpellToDefendAgainstBelief);
	auto defend = MakeMind();
	GiveParent(defend, cast, k_Town);
	// a town with nothing to defend
	EXPECT_EQ(ScoreOf(ScoreKind::CastSpellToDefendAgainstBelief, defend, cast, world), 0.0f);
	world.pits[k_Town] = k_Pit;
	// another player's town counts in full; the shield, worth 1 against 0.9, wins
	world.townOwners[k_Town] = PlayerNames::PLAYER_ONE;
	const float keen = Bits(0x3F7FFDA3u);
	EXPECT_EQ(ScoreOf(ScoreKind::CastSpellToDefendAgainstBelief, defend, cast, world), keen);
	EXPECT_EQ(defend.nodes.at(cast).thing, k_Icon);
	// the rule reads the node's own thing before its parent's, so it starts with none, as each weighing leaves it
	defend.nodes.at(cast).thing = k_None;
	// the mind's own town counts by ten times its rival's share
	world.townOwners[k_Town] = world.me;
	world.rivalBelief[k_Town] = 0.05f;
	EXPECT_EQ(ScoreOf(ScoreKind::CastSpellToDefendAgainstBelief, defend, cast, world), 0.05f * 10.0f * keen);
}

TEST(ComputerMindScores, AFightMiracleIsForACreatureFarFromTheMindsOwn)
{
	FakeScoreWorld world;
	world.citadel = ScoreWorld::Citadel {};
	world.inside = true;
	LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 3, std::nullopt, 10, false);
	world.creatures.insert(k_Rival);
	world.places[k_Rival] = MapCoords {0x100000 + 0x50000, 0x100000, 0.0f};
	LayOutIcon(world, true, 120.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
	world.enabled = {MagicType::Fireball, MagicType::Heal};
	world.icons[MagicType::Fireball] = k_Icon;
	world.icons[MagicType::Heal] = k_Icon;
	world.perceived[MagicType::Fireball] = 2;
	const auto fight = NodeScoredBy(ScoreKind::TeachCreatureAggressiveMagic164);
	auto mind = MakeMind();
	// nothing above: nothing
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureAggressiveMagic164, mind, fight, world), 0.0f);
	GiveParent(mind, fight, k_Rival);
	// 50 m apart: nearer than twice the two creatures' sizes (15 each, so 60 m), nothing
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureAggressiveMagic164, mind, fight, world), 0.0f);
	world.places[k_Rival].x = 0x100000 + 0x70000;
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureAggressiveMagic164, mind, fight, world), Bits(0x3F7FFDA3u));
	EXPECT_EQ(mind.nodes.at(fight).thing, k_Icon);
	// only the miracles that make a creature angry count
	EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Fireball)), 1u);
	EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Heal)), 0u);
	// small creatures (15 m by their sizes) are still kept 30 m apart
	world.scales[k_Creature] = 0.25f;
	world.scales[k_Rival] = 0.25f;
	world.places[k_Rival].x = 0x100000 + 0x28000;
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureAggressiveMagic164, mind, fight, world), 0.0f);
	world.places[k_Rival].x = 0x100000 + 0x38000;
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureAggressiveMagic164, mind, fight, world), Bits(0x3F7FFDA3u));
}

TEST(ComputerMindScores, TheCreaturesNeedsAreHowStronglyItFeelsThem)
{
	FakeScoreWorld world;
	auto mind = MakeMind();
	const auto node = NodeScoredBy(ScoreKind::GiveCreatureFood);
	EXPECT_EQ(ScoreOf(ScoreKind::GiveCreatureFood, mind, node, world), 0.0f);
	LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 3, std::nullopt, 10, false);
	struct Case
	{
		ScoreKind kind;
		uint32_t desire;
		float share;
	};
	for (const auto& c :
	     {Case {ScoreKind::GiveCreatureFood, 4, 1.0f}, Case {ScoreKind::MakeCreatureDrink, 14, 1.0f},
	      Case {ScoreKind::MakeCreatureSleep, 8, 1.0f}, Case {ScoreKind::SendCreatureHome, 28, 1.0f},
	      Case {ScoreKind::MoveCreaturetoWarmerArea, 19, 0.5f}, Case {ScoreKind::MoveCreaturetoCoollerArea, 20, 0.1f}})
	{
		world.desires.clear();
		world.desires[{k_Creature, c.desire}] = 0.625f;
		const auto at = NodeScoredBy(c.kind);
		auto fresh = MakeMind();
		EXPECT_EQ(ScoreOf(c.kind, fresh, at, world), 0.625f * c.share) << static_cast<int>(c.kind);
		EXPECT_EQ(fresh.nodes.at(at).thing, k_Creature);
	}
	// a creature a script controls is looked after by nobody
	world.scripted.insert(k_Creature);
	EXPECT_EQ(ScoreOf(ScoreKind::GiveCreatureFood, mind, node, world), 0.0f);
}

TEST(ComputerMindScores, PlayingWithTheCreatureDrawsOnceFromTheSyncedGenerator)
{
	// the oracle's scores from the campaign's seed, which each draw leaves at 0xE6F4C8D6
	struct Case
	{
		ScoreKind kind;
		uint32_t score;
	};
	for (const auto& c :
	     {Case {ScoreKind::ThrowFoodatCreature, 0x3ECFE09Du}, Case {ScoreKind::SlapCreature, 0x3D8CA2F4u},
	      Case {ScoreKind::StrokeCreature, 0x3E2CB7E0u}, Case {ScoreKind::AttachLeashtoMovingObject, 0x3ECFE09Du}})
	{
		FakeScoreWorld world;
		LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 3, std::nullopt, 10, false);
		auto mind = MakeMind();
		const auto node = NodeScoredBy(c.kind);
		EXPECT_EQ(BitsOf(ScoreOf(c.kind, mind, node, world)), c.score) << static_cast<int>(c.kind);
		EXPECT_EQ(world.seed, 0xE6F4C8D6u);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Creature);
	}
	// without a creature, or with a leash on for the leash, no draw
	FakeScoreWorld world;
	auto mind = MakeMind();
	const auto node = NodeScoredBy(ScoreKind::ThrowFoodatCreature);
	EXPECT_EQ(ScoreOf(ScoreKind::ThrowFoodatCreature, mind, node, world), 0.0f);
	LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 3, 0u, 10, false);
	const auto leash = NodeScoredBy(ScoreKind::AttachLeashtoMovingObject);
	EXPECT_EQ(ScoreOf(ScoreKind::AttachLeashtoMovingObject, mind, leash, world), 0.0f);
	EXPECT_EQ(world.seed, game_random::k_InitialSeed);
}

TEST(ComputerMindScores, TheBestFieldOrFishFarmIsTheOriginals)
{
	// the oracle's cases: every place inside the influence, the hand at (0x100000, 0x100000)
	constexpr entt::entity k_FieldOne {30};
	constexpr entt::entity k_FieldTwo {31};
	struct Place
	{
		entt::entity town;
		entt::entity field;
		int32_t x, z;
		float food, capacity;
	};
	struct Case
	{
		std::vector<Place> places;
		uint32_t field;
		entt::entity fieldChosen;
		uint32_t farm;
		entt::entity farmChosen;
	};
	const std::vector<Case> cases {
	    {{{k_Town, k_FieldOne, 0x110000, 0x100000, 50.0f, 350.0f},
	      {k_OtherTown, k_FieldTwo, 0x120000, 0x130000, 300.0f, 350.0f}},
	     0x3F5B4233u,
	     k_FieldTwo,
	     0x3F7FF96Eu,
	     k_FieldOne},
	    {{{k_Town, k_FieldOne, 0x110000, 0x100000, 1.0f, 350.0f}}, 0x00000000u, k_None, 0x3F7FF96Eu, k_FieldOne},
	    {{{k_Town, k_FieldOne, 0x300000, 0x100000, 350.0f, 350.0f}, {k_Town, k_FieldTwo, 0x101000, 0x100000, 0.0f, 350.0f}},
	     0x00000000u,
	     k_None,
	     0x3F7FFDA3u,
	     k_FieldTwo},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.inside = true;
		world.hand = MapCoords {0x100000, 0x100000, 0.0f};
		auto& towns = world.townsOf[world.me];
		for (const auto& place : c.places)
		{
			if (std::find(towns.begin(), towns.end(), place.town) == towns.end())
			{
				towns.push_back(place.town);
			}
			world.fieldsOf[place.town].push_back(place.field);
			world.farmsOf[place.town].push_back(place.field);
			world.places[place.field] = MapCoords {place.x, place.z, 0.0f};
			world.fieldFood[place.field] = place.food;
			world.fieldCapacity[place.field] = place.capacity;
		}
		auto mind = MakeMind();
		const auto field = NodeScoredBy(ScoreKind::GetFoodFromField);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::GetFoodFromField, mind, field, world)), c.field) << index;
		EXPECT_EQ(mind.nodes.at(field).thing, c.fieldChosen) << index;
		const auto farm = NodeScoredBy(ScoreKind::GetFoodFromFishFarm);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::GetFoodFromFishFarm, mind, farm, world)), c.farm) << index;
		EXPECT_EQ(mind.nodes.at(farm).thing, c.farmChosen) << index;
	}
}

TEST(ComputerMindScores, FoodOrWoodFromAnotherTownIsTheOriginals)
{
	constexpr entt::entity k_ThirdTown {32};
	constexpr entt::entity k_ThirdPit {33};
	constexpr entt::entity k_Pile {34};
	struct Case
	{
		uint32_t resource;
		float parentScore;
		uint32_t pitResource;
		float desireTwo, desireThree;
		uint32_t pileAmount;
		uint32_t score;
		float asked;
	};
	// the oracle's cases: the town above asks; two other towns of the mind's player with a store
	const std::vector<Case> cases {
	    {0, 0.5f, 600, 0.25f, 0.75f, 10, 0x3F400000u, 500.0f}, {0, 0.04f, 600, 0.25f, 0.75f, 10, 0x00000000u, 0.0f},
	    {1, 0.5f, 500, 0.25f, 1.25f, 10, 0x00000000u, 0.0f},   {0, 0.5f, 600, 0.25f, 0.75f, 0, 0x00000000u, 0.0f},
	    {1, 0.3f, 600, 0.1f, 0.2f, 10, 0x3F666666u, 300.0f},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		const auto kind = c.resource == 0 ? ScoreKind::GetFoodFromOtherTownForObject : ScoreKind::GetWoodFromOtherTown;
		if (!IsScorePorted(kind))
		{
			continue;
		}
		FakeScoreWorld world;
		world.inside = true;
		world.townsOf[world.me] = {k_Town, k_OtherTown, k_ThirdTown};
		world.pits[k_OtherTown] = k_Pit;
		world.pits[k_ThirdTown] = k_ThirdPit;
		for (const auto pit : {k_Pit, k_ThirdPit})
		{
			world.pitResources[{pit, c.resource}] = c.pitResource;
			world.places[pit] = MapCoords {0x120000, 0x120000, 0.0f};
			// food has one pile, wood five, taken from the last
			world.piles[{pit, c.resource * 8 + (c.resource != 0 ? 4 : 0)}] = k_Pile;
		}
		world.pileAmounts[k_Pile] = c.pileAmount;
		world.rawDesires[{k_OtherTown, c.resource}] = c.desireTwo;
		world.rawDesires[{k_ThirdTown, c.resource}] = c.desireThree;
		const auto node = NodeScoredBy(kind);
		auto mind = MakeMind();
		const auto parent = *MindNodes()[node].parent;
		mind.nodes.at(parent).thing = k_Town;
		mind.nodes.at(parent).score = c.parentScore;
		EXPECT_EQ(BitsOf(ScoreOf(kind, mind, node, world)), c.score) << index;
		EXPECT_EQ(mind.nodes.at(node).thing, c.score != 0 ? k_Pile : k_None) << index;
		EXPECT_EQ(mind.nodes.at(node).floatValue, c.asked) << index;
	}
}

TEST(ComputerMindScores, AStarvingCreatureAsksForFoodByItsHunger)
{
	FakeScoreWorld world;
	LayOutCreature(world, 1.0f, 0.0f, 0.25f, 1.0f, 3, std::nullopt, 10, false);
	world.lowEnergy[k_Creature] = 0.25f;
	world.desires[{k_Creature, 4}] = 0.123f;
	const auto node = NodeScoredBy(ScoreKind::ReacttoCreatureStarvingEmergency);
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::ReacttoCreatureStarvingEmergency, mind, node, world), 0.123f);
	EXPECT_EQ(mind.nodes.at(node).intValue, 123);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Creature);
	world.desires[{k_Creature, 4}] = 1.5f;
	EXPECT_EQ(ScoreOf(ScoreKind::ReacttoCreatureStarvingEmergency, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).intValue, 1000);
	// with energy above its kind's least, no emergency
	world.energies[k_Creature] = 0.26f;
	auto calm = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::ReacttoCreatureStarvingEmergency, calm, node, world), 0.0f);
	EXPECT_EQ(calm.nodes.at(node).thing, k_None);
	// an ill creature is never seen: what it is tested on is never set
	const auto ill = NodeScoredBy(ScoreKind::ReacttoCreatureIllEmergency);
	EXPECT_EQ(ScoreOf(ScoreKind::ReacttoCreatureIllEmergency, calm, ill, world), 0.0f);
	EXPECT_EQ(calm.nodes.at(ill).thing, k_None);
}

TEST(ComputerMindScores, AHurtCreatureIsHealedAndAnUntaughtOneTaught)
{
	FakeScoreWorld world;
	LayOutCreature(world, 0.25f, 0.0f, 1.0f, 1.0f, 3, std::nullopt, 10, false);
	auto mind = MakeMind();
	const auto hurt = NodeScoredBy(ScoreKind::RestoreCreatureHealth);
	EXPECT_EQ(ScoreOf(ScoreKind::RestoreCreatureHealth, mind, hurt, world), 0.0f);
	world.lives[k_Creature] = 0.15f;
	EXPECT_EQ(ScoreOf(ScoreKind::RestoreCreatureHealth, mind, hurt, world), 1.0f - 0.15f);
	EXPECT_EQ(mind.nodes.at(hurt).thing, k_Creature);

	const auto normal = NodeScoredBy(ScoreKind::TeachCreatureNormalAction);
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureNormalAction, mind, normal, world), 0.0f);
	world.unknownActions[k_Creature] = 2;
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureNormalAction, mind, normal, world), 0.3f);

	// the lessons the mind's actions count: none yet, then each worth 0.7, up to 1
	const auto aggressive = NodeScoredBy(ScoreKind::TeachCreatureAggressiveMagic);
	auto teach = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureAggressiveMagic, teach, aggressive, world), 0.0f);
	EXPECT_EQ(teach.nodes.at(aggressive).thing, k_Creature);
	teach.destroyLessons = 1;
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureAggressiveMagic, teach, aggressive, world), 0.7f);
	teach.destroyLessons = 2;
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureAggressiveMagic, teach, aggressive, world), 1.0f);
	teach.helpLessons = 1;
	const auto compassionate = NodeScoredBy(ScoreKind::TeachCreatureCompassionateMagic);
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureCompassionateMagic, teach, compassionate, world), 0.7f);
	const auto impressive = NodeScoredBy(ScoreKind::TeachCreatureImpressiveMagic);
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureImpressiveMagic, teach, impressive, world), 0.0f);
	EXPECT_EQ(teach.nodes.at(impressive).thing, k_Creature);
}

TEST(ComputerMindScores, MiraclesForTheCreatureAreAtItsPlaceOrAnywhere)
{
	FakeScoreWorld world;
	world.citadel = ScoreWorld::Citadel {};
	world.inside = true;
	LayOutIcon(world, true, 120.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
	world.enabled = {MagicType::Food, MagicType::Heal};
	world.icons[MagicType::Food] = k_Icon;
	world.icons[MagicType::Heal] = k_Icon;
	world.perceived[MagicType::Heal] = 1;
	const float keen = Bits(0x3F7FFDA3u);
	auto mind = MakeMind();
	const auto heal = NodeScoredBy(ScoreKind::RestoreMyCreatureHealth);
	EXPECT_EQ(ScoreOf(ScoreKind::RestoreMyCreatureHealth, mind, heal, world), keen);
	EXPECT_EQ(mind.nodes.at(heal).thing, k_Icon);
	const auto compassion = NodeScoredBy(ScoreKind::TeachCreatureCompassionateMagic166);
	EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureCompassionateMagic166, mind, compassion, world), keen);
	EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Food)), 0u);
	// the food miracle at a town's store, which the town makes when it has none
	const auto food = NodeScoredBy(ScoreKind::CastFoodSpellOn);
	world.towns.insert(k_Town);
	world.drops[k_Town] = k_Pit;
	GiveParent(mind, food, k_Town);
	EXPECT_EQ(ScoreOf(ScoreKind::CastFoodSpellOn, mind, food, world), keen);
	ASSERT_EQ(world.dropsAsked.size(), 1u);
	EXPECT_EQ(world.dropsAsked.front(), std::make_pair(k_Town, 0u));
	EXPECT_EQ(mind.nodes.at(food).thing, k_Icon);
	// at a thing with a place, no town asked
	world.places[k_Target] = MapCoords {0x10000, 0x10000, 0.0f};
	GiveParent(mind, food, k_Target);
	EXPECT_EQ(ScoreOf(ScoreKind::CastFoodSpellOn, mind, food, world), keen);
	EXPECT_EQ(world.dropsAsked.size(), 1u);
}

TEST(ComputerMindScores, AFreeVillagerIsLookedForInTheCellsAroundTheHand)
{
	FakeScoreWorld world;
	world.inside = true;
	world.hand = MapCoords {0x100000, 0x100000, 0.0f};
	world.powers[world.me] = 100.0f;
	constexpr entt::entity k_Weak {40};
	constexpr entt::entity k_Strong {41};
	constexpr entt::entity k_Nearer {42};
	constexpr entt::entity k_Disciple {43};
	constexpr entt::entity k_Elsewhere {44};
	const ScoreWorld::VillagerFacts free {.available = true, .reachable = true, .idle = true};
	for (const auto villager : {k_Weak, k_Strong, k_Nearer, k_Elsewhere})
	{
		world.villagers[villager] = free;
	}
	auto disciple = free;
	disciple.disciple = true;
	world.villagers[k_Disciple] = disciple;
	world.places[k_Weak] = MapCoords {0x108000, 0x100000, 0.0f};
	world.places[k_Strong] = MapCoords {0x108000, 0x100000, 0.0f};
	world.places[k_Nearer] = MapCoords {0x104000, 0x100000, 0.0f};
	world.places[k_Disciple] = MapCoords {0x100000, 0x100000, 0.0f};
	world.places[k_Elsewhere] = MapCoords {0x0F8000, 0x100000, 0.0f};
	world.lives[k_Weak] = 0.5f;
	world.villagersAt[{0x10, 0x10}] = {k_Disciple, k_Weak, k_Strong, k_Nearer};
	// the next cell of the spiral, never looked at once a villager is found
	world.villagersAt[{0x0F, 0x10}] = {k_Elsewhere};
	const auto node = NodeScoredBy(ScoreKind::FindFreeVillagerInArea);
	auto mind = MakeMind();
	// the weak one is taken and the best becomes its distance's fall-off; the strong one at the same place is no better
	// than that; the nearer one is
	const float nearer = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(world.hand, world.places[k_Nearer]), 100.0f);
	EXPECT_EQ(ScoreOf(ScoreKind::FindFreeVillagerInArea, mind, node, world), nearer);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Nearer);
	// with nobody in the hand's cell, the spiral goes on to the next
	world.villagersAt.erase({0x10, 0x10});
	auto next = MakeMind();
	EXPECT_GT(ScoreOf(ScoreKind::FindFreeVillagerInArea, next, node, world), 0.0f);
	EXPECT_EQ(next.nodes.at(node).thing, k_Elsewhere);
}

TEST(ComputerMindScores, AMiracleForALaughIsCastOnlyOnACreatureWithNone)
{
	FakeScoreWorld world;
	LayOutCreature(world, 1.0f, 0.0f, 1.0f, 1.0f, 3, std::nullopt, 10, false);
	const auto node = NodeScoredBy(ScoreKind::CastSpellOnMyCreatureForALaugh);
	auto mind = MakeMind();
	EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::CastSpellOnMyCreatureForALaugh, mind, node, world)), 0x3D1332AEu);
	world.spelled.insert(k_Creature);
	auto spelled = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::CastSpellOnMyCreatureForALaugh, spelled, node, world), 0.0f);
	EXPECT_EQ(spelled.nodes.at(node).thing, k_Creature);
}

TEST(ComputerMindScores, TheTownWantingFoodOrWoodMostIsChosen)
{
	FakeScoreWorld world;
	world.townsOf[world.me] = {k_Town, k_OtherTown};
	world.townDesires[{k_Town, 0}] = 0.5f;
	world.townDesires[{k_OtherTown, 0}] = 0.5f;
	world.desiredFood[k_Town] = 10.5f;
	world.foodAvailable[k_Town] = 12;
	const auto food = NodeScoredBy(ScoreKind::TownWithHighestFoodDesire);
	auto mind = MakeMind();
	// a tie keeps the first; the food it is short of is truncated towards 0
	EXPECT_EQ(ScoreOf(ScoreKind::TownWithHighestFoodDesire, mind, food, world), 0.5f);
	EXPECT_EQ(mind.nodes.at(food).thing, k_Town);
	EXPECT_EQ(mind.nodes.at(food).intValue, -1);
	world.foodAvailable[k_Town] = 3;
	EXPECT_EQ(ScoreOf(ScoreKind::TownWithHighestFoodDesire, mind, food, world), 0.5f);
	EXPECT_EQ(mind.nodes.at(food).intValue, 7);

	// wood only for a town with a store
	world.townDesires[{k_Town, 1}] = 0.75f;
	world.townDesires[{k_OtherTown, 1}] = 0.25f;
	world.pits[k_OtherTown] = k_Pit;
	const auto wood = NodeScoredBy(ScoreKind::TownWithHighestWoodDesire);
	EXPECT_EQ(ScoreOf(ScoreKind::TownWithHighestWoodDesire, mind, wood, world), 0.25f);
	EXPECT_EQ(mind.nodes.at(wood).thing, k_OtherTown);
}

TEST(ComputerMindScores, ADiscipleIsMadeForTheTownsWantAsTheOriginalChooses)
{
	// the oracle's cases: the builder found is worth its distance's fall-off at the hand, which multiplies
	struct Case
	{
		std::array<uint32_t, 13> disciples;
		uint32_t adults;
		uint32_t score;
		int32_t kind;
	};
	const std::vector<Case> cases {
	    {{}, 10, 0x00000000u, 0},
	    {{0, 1, 0, 0, 2, 2}, 10, 0x00000000u, 0},
	    {{0, 3, 1, 0, 0, 2}, 40, 0x3E213DDAu, 3},
	    {{}, 0, 0x00000000u, 0},
	};
	constexpr entt::entity k_Builder {50};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.inside = true;
		world.powers[world.me] = 100.0f;
		world.towns.insert(k_Town);
		world.townOf[k_Town] = k_Town;
		world.adults[k_Town] = c.adults;
		for (uint32_t kind = 0; kind < 13; ++kind)
		{
			world.disciples[{k_Town, kind}] = c.disciples.at(kind);
		}
		world.townDesires[{k_Town, 0}] = c.adults == 40 ? 0.9f : 0.5f;
		world.townDesires[{k_Town, 1}] = c.adults == 40 ? 0.8f : 0.25f;
		world.townDesires[{k_Town, 8}] = c.adults == 40 ? 0.6f : 0.1f;
		world.townDesires[{k_Town, 9}] = c.adults == 40 ? 0.2f : 0.75f;
		// a free builder at the hand
		world.abodesOf[k_Town] = {k_Abode};
		world.inhabitants[k_Abode] = {k_Builder};
		world.villagers[k_Builder] = {.available = true, .reachable = true, .idle = true};
		const auto node = NodeScoredBy(ScoreKind::CreateDiscipleToFulfillTownDesire);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		const float builder = Bits(0x3F7FFDA3u);
		const float expected = builder * Bits(c.score);
		EXPECT_EQ(ScoreOf(ScoreKind::CreateDiscipleToFulfillTownDesire, mind, node, world), c.score != 0 ? expected : 0.0f)
		    << index;
		EXPECT_EQ(mind.nodes.at(node).intValue, c.kind) << index;
	}
}

TEST(ComputerMindScores, ATownWithTooFewBreedersGetsOneOfItsVillagersAtHome)
{
	constexpr entt::entity k_Man {51};
	constexpr entt::entity k_Woman {52};
	FakeScoreWorld world;
	world.towns.insert(k_Town);
	world.abodesOf[k_Town] = {k_Abode};
	world.inhabitants[k_Abode] = {k_Man, k_Woman};
	world.villagers[k_Man] = {.available = true, .sexuallyActive = true, .sex = 0};
	world.villagers[k_Woman] = {.available = true, .sexuallyActive = true, .sex = 1};
	const auto node = NodeScoredBy(ScoreKind::CreateDiscipleToFulfillTownDesire);
	auto mind = MakeMind();
	GiveParent(mind, node, k_Town);
	EXPECT_EQ(ScoreOf(ScoreKind::CreateDiscipleToFulfillTownDesire, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Man);
	EXPECT_EQ(mind.nodes.at(node).intValue, 5);
	// a sex that has a breeder gives none; one out of its home neither
	world.villagers[k_Man] = {.available = true, .disciple = true, .sexuallyActive = true, .sex = 0, .discipleType = 5};
	EXPECT_EQ(ScoreOf(ScoreKind::CreateDiscipleToFulfillTownDesire, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Woman);
	world.villagers[k_Woman].reachable = true;
	auto none = MakeMind();
	GiveParent(none, node, k_Town);
	EXPECT_EQ(ScoreOf(ScoreKind::CreateDiscipleToFulfillTownDesire, none, node, world), 0.0f);
	EXPECT_EQ(none.nodes.at(node).thing, k_None);
}

TEST(ComputerMindScores, AScaffoldsPlaceIsFoundAsTheOriginalDrawsIt)
{
	// the oracle's cases: two towns, every plan and cell as given, the campaign's seed; six draws a town
	struct Case
	{
		int32_t value;
		bool plans;
		bool steep;
		std::array<float, 2> civic;
		std::array<float, 2> abodes;
		std::array<int32_t, 2> places;
		uint32_t score;
		entt::entity town;
	};
	const std::vector<Case> cases {
	    {3, true, false, {0.25f, 0.5f}, {0.0f, 0.0f}, {10, 10}, 0x3F000000u, k_OtherTown},
	    {1, true, false, {0.0f, 0.0f}, {0.25f, 0.75f}, {10, 200}, 0x3E800000u, k_Town},
	    {3, false, false, {0.25f, 0.5f}, {0.0f, 0.0f}, {10, 10}, 0x00000000u, k_None},
	    {3, true, true, {0.25f, 0.5f}, {0.0f, 0.0f}, {10, 10}, 0x00000000u, k_None},
	    {7, true, false, {0.25f, 0.5f}, {0.0f, 0.0f}, {10, 10}, 0x3F800000u, k_Town},
	};
	constexpr entt::entity k_Scaffold {53};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.townsOf[world.me] = {k_Town, k_OtherTown};
		world.areas[k_Town] = {MapCoords {0x640000, 0xC80000, 0.0f}, MapCoords {0x820000, 0xD20000, 0.0f}};
		world.areas[k_OtherTown] = {MapCoords {0x900000, 0x500000, 0.0f}, MapCoords {0xAE0000, 0x5A0000, 0.0f}};
		world.centres[k_Town] = MapCoords {0x730000, 0xCD0000, 0.0f};
		world.centres[k_OtherTown] = MapCoords {0x9F0000, 0x550000, 0.0f};
		const std::array<entt::entity, 2> towns {k_Town, k_OtherTown};
		for (size_t town = 0; town < towns.size(); ++town)
		{
			world.rawDesires[{towns.at(town), 6}] = c.civic.at(town);
			world.rawDesires[{towns.at(town), 5}] = c.abodes.at(town);
			world.adultPlaces[towns.at(town)] = c.places.at(town);
		}
		world.scaffolds[k_Scaffold] = {.value = c.value};
		world.planWorks = c.plans;
		world.allSteep = c.steep;
		const auto node = NodeScoredBy(ScoreKind::FindLocationToUseScaffold);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Scaffold);
		const float score = ScoreOf(ScoreKind::FindLocationToUseScaffold, mind, node, world);
		EXPECT_EQ(BitsOf(score), c.score) << index;
		EXPECT_EQ(mind.nodes.at(node).thing, c.town) << index;
		EXPECT_EQ(world.seed, 0xCE2E4D78u) << index;
		ASSERT_EQ(world.triedAt.size(), 8u) << index;
		EXPECT_EQ(world.triedAt.at(0).place.x, 0x7202EE) << index;
		EXPECT_EQ(world.triedAt.at(0).place.z, 0xBEAAC3) << index;
		EXPECT_EQ(world.triedAt.at(4).place.x, 0xAA3788) << index;
		EXPECT_EQ(world.triedAt.at(4).place.z, 0x4E90D8) << index;
		EXPECT_EQ(world.triedAt.at(4).town, k_OtherTown) << index;
	}
}

TEST(ComputerMindScores, AScaffoldsPlaceStaysOnTheMapAndOffSteepCells)
{
	const ScoreWorld::Cells cells {512, 512};
	const auto clamped = ClampCellsOntoMap(MapCoords {static_cast<int32_t>(0xFFFF8000u), 0x2001234, 1.5f}, cells);
	// the cells move onto the map, the fractions stay
	EXPECT_EQ(clamped.x, 0x00008000);
	EXPECT_EQ(clamped.z, 0x01FF1234);
	EXPECT_EQ(clamped.altitude, 1.5f);
	EXPECT_EQ(ClampCellsOntoMap(MapCoords {0x123456, 0x7654, 0.0f}, cells), (MapCoords {0x123456, 0x7654, 0.0f}));
	EXPECT_FALSE(CellIsSteep(std::nullopt));
	EXPECT_FALSE(CellIsSteep(std::array<uint8_t, 4> {10, 20, 15, 12}));
	EXPECT_TRUE(CellIsSteep(std::array<uint8_t, 4> {10, 21, 15, 12}));
}

TEST(ComputerMindScores, ABreederIsMadeInATownDrawnAtRandom)
{
	// the oracle's cases from the campaign's seed: the first draw of three gives the first town
	struct Case
	{
		std::array<uint32_t, 3> breeders;
		entt::entity town;
	};
	constexpr entt::entity k_ThirdTown {54};
	for (const auto& c : {Case {{0, 0, 0}, k_Town}, Case {{2, 2, 1}, k_ThirdTown}, Case {{2, 2, 2}, k_None}})
	{
		FakeScoreWorld world;
		world.townsOf[world.me] = {k_Town, k_OtherTown, k_ThirdTown};
		world.disciples[{k_Town, 5}] = c.breeders.at(0);
		world.disciples[{k_OtherTown, 5}] = c.breeders.at(1);
		world.disciples[{k_ThirdTown, 5}] = c.breeders.at(2);
		const auto node = NodeScoredBy(ScoreKind::MakeDiscipleBreader);
		auto mind = MakeMind();
		EXPECT_EQ(ScoreOf(ScoreKind::MakeDiscipleBreader, mind, node, world), c.town != k_None ? 1.0f : 0.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, c.town);
		EXPECT_EQ(world.seed, 0xE6F4C8D6u);
	}
	// no town: no draw
	FakeScoreWorld world;
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::MakeDiscipleBreader, mind, NodeScoredBy(ScoreKind::MakeDiscipleBreader), world), 0.0f);
	EXPECT_EQ(world.seed, game_random::k_InitialSeed);
}

TEST(ComputerMindScores, AWorshipSitesWantsAndItsTownsWillAreTheOriginals)
{
	FakeScoreWorld world;
	world.built.insert(k_Site);
	// the oracle's cases: icons, worshippers, and the want of worshippers
	struct Want
	{
		uint32_t icons, worshippers, want;
	};
	for (const auto& c :
	     {Want {1, 0, 0x3F800000u}, Want {1, 1, 0x3F7F0000u}, Want {1, 2, 0x3F700000u}, Want {1, 3, 0x3F2F0000u},
	      Want {1, 4, 0u}, Want {1, 9, 0u}, Want {2, 3, 0x3F7AF000u}, Want {3, 5, 0x3F7848B1u}, Want {0, 3, 0u}})
	{
		world.siteIcons[k_Site] = std::vector<entt::entity>(c.icons, k_Icon);
		world.worshippers[k_Site] = c.worshippers;
		EXPECT_EQ(BitsOf(DesireForWorshippers(world, k_Site)), c.want) << c.icons << " " << c.worshippers;
	}
	world.built.clear();
	EXPECT_EQ(DesireForWorshippers(world, k_Site), 0.0f);
	// the town's will: its people, those worshipping and those on their way
	struct Will
	{
		uint32_t people, worshipping, onTheWay, will;
	};
	for (const auto& c :
	     {Will {15, 0, 0, 0x3F800000u}, Will {15, 3, 1, 0x3EC9EAEAu}, Will {15, 6, 0, 0x3E5D2F1Cu}, Will {7, 1, 0, 0x3F21367Cu},
	      Will {24, 2, 3, 0x3EFE097Du}, Will {3, 5, 0, 0u}, Will {9, 1, 1, 0x3EF0E65Eu}, Will {100, 1, 0, 0x3F800000u},
	      Will {32, 15, 0, 0u}, Will {32, 14, 0, 0u}, Will {10, 2, 1, 0x3EAF9DB2u}})
	{
		world.adults[k_Town] = c.people;
		world.townWorship[k_Town] = {.siteFunctional = true, .worshipping = c.worshipping, .onTheWay = c.onTheWay};
		EXPECT_EQ(BitsOf(DesireToGiveWorshippers(world, k_Town)), c.will) << c.people << " " << c.worshipping;
	}
	world.townWorship[k_Town].emergency = true;
	EXPECT_EQ(DesireToGiveWorshippers(world, k_Town), 0.0f);
	world.townWorship[k_Town] = {.siteFunctional = true, .worshipping = 3, .onTheWay = 1};
	world.adults[k_Town] = 15;
	// the site wants its icons' worshippers, from its most willing town
	world.built.insert(k_Site);
	world.siteIcons[k_Site] = {k_Icon, k_Icon};
	world.worshippers[k_Site] = 3;
	world.siteTowns[k_Site] = {k_OtherTown, k_Town};
	EXPECT_EQ(DesireForVillagerAvailableFromTown(world, k_Site), Bits(0x3EC9EAEAu) * Bits(0x3F7AF000u));
	world.citadel = ScoreWorld::Citadel {};
	world.citadelSites.at(2) = k_Site;
	const auto node = NodeScoredBy(ScoreKind::WorshipSiteThatNeedsVillagers);
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::WorshipSiteThatNeedsVillagers, mind, node, world), Bits(0x3EC9EAEAu) * Bits(0x3F7AF000u));
	EXPECT_EQ(mind.nodes.at(node).thing, k_Site);
}

TEST(ComputerMindScores, AVillagerIsTakenFromTheMostWillingTownToTheSite)
{
	constexpr entt::entity k_Near {55};
	constexpr entt::entity k_Far {56};
	constexpr entt::entity k_Homeless {57};
	FakeScoreWorld world;
	world.inside = true;
	world.worshipSiteSet.insert(k_Site);
	world.siteTowns[k_Site] = {k_Town};
	world.gates[k_Site] = MapCoords {0x100000, 0x100000, 0.0f};
	world.adults[k_Town] = 15;
	world.townWorship[k_Town] = {.siteFunctional = true, .worshipping = 3, .onTheWay = 1};
	world.abodesOf[k_Town] = {k_Abode};
	world.inhabitants[k_Abode] = {k_Near, k_Far};
	const ScoreWorld::VillagerFacts free {.reachable = true, .availableForWorship = true};
	world.villagers[k_Near] = free;
	world.villagers[k_Far] = free;
	world.villagers[k_Homeless] = {.availableForWorship = true};
	// one within 50 m of the gate is there already
	world.places[k_Near] = MapCoords {0x100000 + 0x40000, 0x100000, 0.0f};
	world.places[k_Far] = MapCoords {0x100000 + 0x80000, 0x100000, 0.0f};
	world.places[k_Homeless] = MapCoords {0x100000 + 0x80000, 0x100000, 0.0f};
	const auto node = NodeScoredBy(ScoreKind::TakeVillagerFromTownToWorshipSite);
	auto mind = MakeMind();
	GiveParent(mind, node, k_Site);
	EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::TakeVillagerFromTownToWorshipSite, mind, node, world)), 0x3EC9EAEAu);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Far);
	// none in the abodes: a homeless one, whom no reach or influence is asked of
	world.villagers[k_Far].reachable = false;
	world.homeless[k_Town] = {k_Homeless};
	EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::TakeVillagerFromTownToWorshipSite, mind, node, world)), 0x3EC9EAEAu);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Homeless);
}

TEST(ComputerMindScores, BuildingSitesAreGivenWoodAndWorkersInTheFirstTownThatNeedsThem)
{
	constexpr entt::entity k_Site1 {58};
	constexpr entt::entity k_Site2 {59};
	constexpr entt::entity k_Building {60};
	FakeScoreWorld world;
	world.inside = true;
	world.townsOf[world.me] = {k_Town, k_OtherTown};
	world.sitesOf[k_Town] = {k_Site1};
	world.sitesOf[k_OtherTown] = {k_Site2};
	world.buildingSites[k_Site1] = {.available = true,
	                                .building = k_Building,
	                                .woodNeeded = 100.0f,
	                                .wood = 50,
	                                .maxBuilders = 4,
	                                .buildersNeeded = 2,
	                                .desireForVillagers = 0.5f};
	world.buildingSites[k_Site2] = {.available = true,
	                                .building = k_Building,
	                                .woodNeeded = 100.0f,
	                                .wood = 0,
	                                .maxBuilders = 4,
	                                .buildersNeeded = 2,
	                                .desireForVillagers = 1.0f};
	world.disciples[{k_Town, 4}] = 1;
	auto mind = MakeMind();
	const auto wood = NodeScoredBy(ScoreKind::GiveBuildingSiteWood);
	// half its wood: 1 - 0.5^3, and the second town's better site is never looked at
	EXPECT_EQ(ScoreOf(ScoreKind::GiveBuildingSiteWood, mind, wood, world), 0.875f);
	EXPECT_EQ(mind.nodes.at(wood).thing, k_Site1);
	const auto workers = NodeScoredBy(ScoreKind::GiveBuildingSiteWorkers);
	EXPECT_EQ(ScoreOf(ScoreKind::GiveBuildingSiteWorkers, mind, workers, world), 0.75f * 0.5f);
	EXPECT_EQ(mind.nodes.at(workers).thing, k_Site1);
	// a built site that needs no repair is left
	world.buildingSites[k_Site1].rootBuilt = true;
	EXPECT_EQ(ScoreOf(ScoreKind::GiveBuildingSiteWood, mind, wood, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(wood).thing, k_Site2);
	// the wood miracle at a building site's building
	world.citadel = ScoreWorld::Citadel {};
	LayOutIcon(world, true, 120.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
	world.enabled.insert(MagicType::Wood);
	world.icons[MagicType::Wood] = k_Icon;
	const auto spell = NodeScoredBy(ScoreKind::ShallCastWoodSpell);
	GiveParent(mind, spell, k_Site2);
	EXPECT_EQ(ScoreOf(ScoreKind::ShallCastWoodSpell, mind, spell, world), Bits(0x3F7FFDA3u));
	EXPECT_EQ(mind.nodes.at(spell).thing, k_Icon);
	EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Wood)), 1u);
}

TEST(ComputerMindScores, ABuilderIsFoundInTheTownTheSiteWorksFor)
{
	constexpr entt::entity k_Builder {61};
	constexpr entt::entity k_SiteFor {62};
	FakeScoreWorld world;
	world.inside = true;
	world.powers[world.me] = 100.0f;
	world.buildingSites[k_SiteFor] = {.available = true, .building = k_Site};
	world.worshipSiteSet.insert(k_Site);
	world.siteTowns[k_Site] = {k_OtherTown, k_Town};
	world.abodesOf[k_OtherTown] = {k_Abode};
	world.inhabitants[k_Abode] = {k_Builder};
	world.villagers[k_Builder] = {.available = true, .reachable = true, .idle = true};
	const auto node = NodeScoredBy(ScoreKind::FindFreeVillagerBuilderInTown);
	auto mind = MakeMind();
	GiveParent(mind, node, k_SiteFor);
	EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::FindFreeVillagerBuilderInTown, mind, node, world)), 0x3F7FFDA3u);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Builder);
}

TEST(ComputerMindScores, TheBestScaffoldIsTheOneItsTownWantsMost)
{
	constexpr entt::entity k_Workshop {63};
	constexpr entt::entity k_Small {64};
	constexpr entt::entity k_Civic {65};
	constexpr entt::entity k_Held {66};
	FakeScoreWorld world;
	world.inside = true;
	world.powers[world.me] = 100.0f;
	world.townsOf[world.me] = {k_Town};
	world.abodesOf[k_Town] = {k_Abode, k_Workshop};
	world.workshops.insert(k_Workshop);
	world.workshopScaffolds[k_Workshop] = {k_None, k_Held, k_Small, k_Civic};
	world.scaffolds[k_Held] = {.busy = true, .value = 7};
	world.scaffolds[k_Small] = {.value = 1};
	world.scaffolds[k_Civic] = {.value = 4};
	world.rawDesires[{k_Town, 5}] = 0.25f;
	world.rawDesires[{k_Town, 6}] = 0.5f;
	auto mind = MakeMind();
	// civic wanted more than abodes: the civic scaffold
	const auto combine = NodeScoredBy(ScoreKind::CombineScaffolds);
	EXPECT_EQ(ScoreOf(ScoreKind::CombineScaffolds, mind, combine, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(combine).thing, k_Civic);
	const auto place = NodeScoredBy(ScoreKind::FindPlaceToUseScaffoldForBuilding);
	EXPECT_EQ(ScoreOf(ScoreKind::FindPlaceToUseScaffoldForBuilding, mind, place, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(place).thing, k_Civic);
	// another scaffold than the one above
	const auto with = NodeScoredBy(ScoreKind::FindScaffoldToCombineWith);
	GiveParent(mind, with, k_Civic);
	EXPECT_EQ(ScoreOf(ScoreKind::FindScaffoldToCombineWith, mind, with, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(with).thing, k_Small);
	// a town with too many places wants no small scaffold
	world.adultPlaces[k_Town] = 176;
	mind.nodes.at(with).thing = k_None;
	EXPECT_EQ(ScoreOf(ScoreKind::FindScaffoldToCombineWith, mind, with, world), 0.0f);
	// abodes wanted as much as civic: none to combine
	world.rawDesires[{k_Town, 5}] = 0.5f;
	auto equal = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::CombineScaffolds, equal, combine, world), 0.0f);
}

TEST(ComputerMindScores, TheFirstWorkshopShortOfWoodIsFilled)
{
	constexpr entt::entity k_Workshop {67};
	constexpr entt::entity k_Second {68};
	FakeScoreWorld world;
	world.townsOf[world.me] = {k_Town};
	world.abodesOf[k_Town] = {k_Workshop, k_Second};
	world.workshops = {k_Workshop, k_Second};
	world.functional = {k_Workshop, k_Second};
	world.space[k_Workshop] = 2;
	world.space[k_Second] = 3;
	world.mostObjects[k_Second] = 4.0f;
	world.scaffoldWood = 100;
	world.pitResources[{k_Workshop, 1}] = 100;
	world.pitResources[{k_Second, 1}] = 99;
	const auto node = NodeScoredBy(ScoreKind::FillWorkshop);
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::FillWorkshop, mind, node, world), 0.75f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Second);
}

TEST(ComputerMindScores, WorshippersAreFedAtTheHungriestSite)
{
	constexpr entt::entity k_SecondSite {69};
	FakeScoreWorld world;
	world.citadel = ScoreWorld::Citadel {};
	world.citadelSites.at(0) = k_Site;
	world.citadelSites.at(3) = k_SecondSite;
	world.siteFoodDesires[k_Site] = 0.25f;
	world.siteFoodDesires[k_SecondSite] = 0.5f;
	world.siteFoodNeeded[k_SecondSite] = 37;
	const auto node = NodeScoredBy(ScoreKind::GiveWorshippersFood);
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::GiveWorshippersFood, mind, node, world), 0.5f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_SecondSite);
	EXPECT_EQ(mind.nodes.at(node).intValue, 37);
}

TEST(ComputerMindScores, AMiracleIsChargedByHowOftenTheMindLookedForIt)
{
	constexpr entt::entity k_Other {70};
	constexpr entt::entity k_Unbuilt {71};
	FakeScoreWorld world;
	world.citadel = ScoreWorld::Citadel {};
	world.citadelSites = {k_Site, k_Unbuilt, k_None, k_None, k_None, k_None};
	world.built.insert(k_Site);
	world.worshippers[k_Site] = 2;
	world.siteIcons[k_Site] = {k_Icon, k_Other};
	world.iconTypes[k_Icon] = MagicType::Heal;
	world.iconTypes[k_Other] = MagicType::Water;
	world.repaired.insert(k_Unbuilt);
	const auto node = NodeScoredBy(ScoreKind::ChargeUpSpell);
	auto mind = MakeMind();
	// never looked for: nothing; but the repaired site not yet built is built at once
	EXPECT_EQ(ScoreOf(ScoreKind::ChargeUpSpell, mind, node, world), 0.0f);
	ASSERT_EQ(world.builtBy.size(), 1u);
	EXPECT_EQ(world.builtBy.front(), std::make_pair(k_Unbuilt, 1.0f));
	mind.iconsFound.at(static_cast<size_t>(MagicType::Heal)) = 30;
	mind.iconsFound.at(static_cast<size_t>(MagicType::Water)) = 250;
	EXPECT_EQ(ScoreOf(ScoreKind::ChargeUpSpell, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Other);
	// one charging, or with prayer stored already, is not charged again
	world.charging.insert(k_Other);
	EXPECT_EQ(ScoreOf(ScoreKind::ChargeUpSpell, mind, node, world), 30.0f * Bits(0x3C23D70Au));
	EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
	world.stores[k_Icon] = 1.0f;
	auto stored = MakeMind();
	stored.iconsFound = mind.iconsFound;
	EXPECT_EQ(ScoreOf(ScoreKind::ChargeUpSpell, stored, node, world), 0.0f);
}

TEST(ComputerMindScores, ARivalsTownIsWorthTakingAsTheOriginalWeighsIt)
{
	// the oracle's cases: the hand at (0x100000, 0x100000); the rival's towns, the first with the icons
	struct Town
	{
		int32_t x;
		uint32_t adults, children;
		bool mine;
	};
	struct Case
	{
		uint32_t population;
		float myPower, rivalPower;
		bool neutral;
		std::vector<Town> towns;
		uint32_t icons;
		bool enabled;
		uint32_t asked;
		float needed;
		uint32_t worth;
		size_t town;
	};
	const Town far {0x180000, 20, 5, false};
	const Town near {0x110000, 10, 2, false};
	const std::vector<Case> cases {
	    {50, 100.0f, 50.0f, false, {far, near}, 2, false, 0, 1.0f, 0x3F30A3BCu, 0},
	    {50, 100.0f, 50.0f, false, {far, near}, 2, false, 3, 1.0f, 0x3F800000u, 0},
	    {50, 100.0f, 150.0f, false, {far, near}, 0, true, 0, 3.0f, 0x3CF5C267u, 0},
	    {0, 100.0f, 50.0f, false, {far, near}, 2, false, 0, 1.0f, 0x3F800000u, 0},
	    {50, 0.0f, 50.0f, false, {far, near}, 0, true, 0, 4.5f, 0x3D4CCCAEu, 0},
	    {50, 100.0f, 50.0f, true, {far, near}, 0, true, 0, 1.0f, 0x3ECCCCAEu, 0},
	    {50, 100.0f, 37.0f, false, {{0x100000, 400, 100, true}}, 0, true, 0, 0.0f, 0x3F800000u, 0},
	    {50, 100.0f, 50.0f, false, {}, 0, true, 0, 1.0f, 0x00000000u, 9},
	    {64, 3.0f, 2.6f, false, {far}, 0, true, 0, 2.0f, 0x3D7CCCAAu, 0},
	};
	constexpr entt::entity k_MyTown {80};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		const auto rival = c.neutral ? PlayerNames::NEUTRAL : PlayerNames::PLAYER_ONE;
		world.hand = MapCoords {0x100000, 0x100000, 0.0f};
		world.townsOf[world.me] = {k_MyTown};
		world.adults[k_MyTown] = c.population;
		world.powers[world.me] = c.myPower;
		world.powers[rival] = c.rivalPower;
		std::vector<entt::entity> towns;
		for (size_t town = 0; town < c.towns.size(); ++town)
		{
			const entt::entity entity {static_cast<uint32_t>(90 + town)};
			towns.push_back(entity);
			world.places[entity] = MapCoords {c.towns[town].x, 0x100000, 0.0f};
			world.adults[entity] = c.towns[town].adults;
			world.children[entity] = c.towns[town].children;
			world.townOwners[entity] = c.towns[town].mine ? world.me : PlayerNames::PLAYER_ONE;
			world.beliefNeeded[entity] = c.needed;
		}
		world.townsOf[rival] = towns;
		if (!towns.empty())
		{
			world.iconTypesOf[towns.front()] = std::vector<MagicType>(c.icons, MagicType::Forest);
		}
		if (c.enabled)
		{
			world.enabled.insert(MagicType::Forest);
		}
		auto mind = MakeMind();
		mind.iconsFound.at(static_cast<size_t>(MagicType::Forest)) = c.asked;
		entt::entity chosen = k_None;
		EXPECT_EQ(BitsOf(BestTownToTake(world, mind, rival, chosen)), c.worth) << index;
		EXPECT_EQ(chosen, c.town < towns.size() ? towns.at(c.town) : k_None) << index;
	}
}

TEST(ComputerMindScores, ATownsWillToBeTakenOverIsTheOriginals)
{
	// the oracle's cases: the belief still needed, for a town of the mind's player and for another's
	struct Case
	{
		float needed;
		uint32_t own, other;
	};
	FakeScoreWorld world;
	for (const auto& c :
	     {Case {0.0f, 0x3F800000u, 0x3F800000u}, Case {1.0f, 0x3F800000u, 0x3F4CCCCDu}, Case {3.0f, 0x3F800000u, 0x3ECCCCCCu},
	      Case {4.5f, 0x3F599999u, 0x3DCCCCCDu}, Case {4.9f, 0x3F451EB8u, 0x3DCCCCCDu}, Case {6.0f, 0x3F400000u, 0x3DCCCCCDu}})
	{
		world.beliefNeeded[k_Town] = c.needed;
		world.townOwners[k_Town] = world.me;
		EXPECT_EQ(BitsOf(DesireToBeTakenOver(world, k_Town)), c.own) << c.needed;
		world.townOwners[k_Town] = PlayerNames::PLAYER_ONE;
		EXPECT_EQ(BitsOf(DesireToBeTakenOver(world, k_Town)), c.other) << c.needed;
	}
}

TEST(ComputerMindScores, TheInfluenceToExpandIsTheBestTownOfAnyRival)
{
	FakeScoreWorld world;
	world.hand = MapCoords {0x100000, 0x100000, 0.0f};
	world.powers[world.me] = 100.0f;
	constexpr entt::entity k_ThirdTown {81};
	world.townsOf[PlayerNames::PLAYER_ONE] = {k_Town};
	world.townsOf[PlayerNames::PLAYER_THREE] = {k_OtherTown};
	world.townsOf[world.me] = {k_ThirdTown};
	for (const auto town : {k_Town, k_OtherTown, k_ThirdTown})
	{
		world.places[town] = MapCoords {0x110000, 0x100000, 0.0f};
		world.adults[town] = 10;
		world.townOwners[town] = PlayerNames::PLAYER_FOUR;
		world.beliefNeeded[town] = 1.0f;
	}
	world.adults[k_OtherTown] = 20;
	const auto node = NodeScoredBy(ScoreKind::ExpandInfluence);
	auto mind = MakeMind();
	entt::entity first = k_None;
	entt::entity second = k_None;
	const float one = BestTownToTake(world, mind, PlayerNames::PLAYER_ONE, first);
	const float three = BestTownToTake(world, mind, PlayerNames::PLAYER_THREE, second);
	ASSERT_GT(three, one);
	EXPECT_EQ(ScoreOf(ScoreKind::ExpandInfluence, mind, node, world), three);
	EXPECT_EQ(mind.nodes.at(node).thing, k_OtherTown);
	// an ally's towns are not to be taken
	world.allies.insert({PlayerNames::PLAYER_THREE, world.me});
	EXPECT_EQ(ScoreOf(ScoreKind::ExpandInfluence, mind, node, world), one);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Town);
	// a player above: its best town, to take over or to destroy
	world.playerThings[PlayerNames::PLAYER_ONE] = k_PlayerOne;
	for (const auto kind : {ScoreKind::TakeOverTown, ScoreKind::DestroyTown})
	{
		const auto at = NodeScoredBy(kind);
		auto taking = MakeMind();
		GiveParent(taking, at, k_PlayerOne);
		EXPECT_EQ(ScoreOf(kind, taking, at, world), one);
		EXPECT_EQ(taking.nodes.at(at).thing, k_Town);
	}
}

TEST(ComputerMindScores, TheRivalToDefeatIsTheFirstActiveOneNotAnAlly)
{
	FakeScoreWorld world;
	world.types[PlayerNames::PLAYER_ONE] = 1;
	world.types[world.me] = 2;
	world.types[PlayerNames::PLAYER_FOUR] = 2;
	world.playerThings[PlayerNames::PLAYER_ONE] = k_PlayerOne;
	world.playerThings[PlayerNames::PLAYER_FOUR] = k_Target;
	EXPECT_EQ(NextActivePlayer(world, std::nullopt), std::optional<PlayerNames>(PlayerNames::PLAYER_ONE));
	EXPECT_EQ(NextActivePlayer(world, PlayerNames::PLAYER_ONE), std::optional<PlayerNames>(world.me));
	EXPECT_EQ(NextActivePlayer(world, world.me), std::optional<PlayerNames>(PlayerNames::PLAYER_FOUR));
	EXPECT_EQ(NextActivePlayer(world, PlayerNames::PLAYER_FOUR), std::nullopt);
	for (const auto kind : {ScoreKind::PlayerWithBetterCreature, ScoreKind::PlayerWithMoreSpells})
	{
		const auto node = NodeScoredBy(kind);
		auto mind = MakeMind();
		EXPECT_EQ(ScoreOf(kind, mind, node, world), 1.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_PlayerOne);
	}
	world.allies.insert({PlayerNames::PLAYER_ONE, world.me});
	const auto node = NodeScoredBy(ScoreKind::PlayerWithBetterCreature);
	auto mind = MakeMind();
	EXPECT_EQ(ScoreOf(ScoreKind::PlayerWithBetterCreature, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Target);
	// the walk stops at a player of the neutral's type
	world.types[PlayerNames::PLAYER_THREE] = 3;
	EXPECT_EQ(NextActivePlayer(world, world.me), std::nullopt);
	// the one with more worshippers is compared with the mind's own and never chosen; defeating a player is always
	// worth it
	const auto worshippers = NodeScoredBy(ScoreKind::PlayerWithMoreWorshippers);
	EXPECT_EQ(ScoreOf(ScoreKind::PlayerWithMoreWorshippers, mind, worshippers, world), 0.0f);
	EXPECT_EQ(mind.nodes.at(worshippers).thing, k_None);
	EXPECT_EQ(ScoreOf(ScoreKind::DefeatPlayer, mind, NodeScoredBy(ScoreKind::DefeatPlayer), world), 1.0f);
}

TEST(ComputerMindScores, ARivalsCreatureIsAttackedNearTheHand)
{
	FakeScoreWorld world;
	world.hand = MapCoords {0x100000, 0x100000, 0.0f};
	world.playerThings[PlayerNames::PLAYER_ONE] = k_PlayerOne;
	world.rivalCreatures[PlayerNames::PLAYER_ONE] = k_Rival;
	world.places[k_Rival] = MapCoords {0x100000 + 0x9FFFF, 0x100000, 0.0f};
	const auto node = NodeScoredBy(ScoreKind::AttackCreature);
	auto mind = MakeMind();
	GiveParent(mind, node, k_PlayerOne);
	EXPECT_EQ(ScoreOf(ScoreKind::AttackCreature, mind, node, world), 1.0f);
	EXPECT_EQ(mind.nodes.at(node).thing, k_Rival);
	world.places[k_Rival].x = 0x100000 + 0xC0000;
	const float fallOff = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(world.hand, world.places[k_Rival]), 500.0f);
	EXPECT_EQ(ScoreOf(ScoreKind::AttackCreature, mind, node, world), fallOff);
	world.allies.insert({PlayerNames::PLAYER_ONE, world.me});
	auto allied = MakeMind();
	GiveParent(allied, node, k_PlayerOne);
	EXPECT_EQ(ScoreOf(ScoreKind::AttackCreature, allied, node, world), 0.0f);
	EXPECT_EQ(allied.nodes.at(node).thing, k_None);
}

TEST(ComputerMindScores, ATownIsWonByMeetingItsNeeds)
{
	FakeScoreWorld world;
	world.towns.insert(k_Town);
	world.rawDesires[{k_Town, 0}] = 0.75f;
	world.rawDesires[{k_Town, 1}] = 0.5f;
	world.townOwners[k_Town] = PlayerNames::PLAYER_ONE;
	const auto food = NodeScoredBy(ScoreKind::TakeOverTownByFulfillingFoodDesire);
	auto mind = MakeMind();
	GiveParent(mind, food, k_Town);
	EXPECT_EQ(ScoreOf(ScoreKind::TakeOverTownByFulfillingFoodDesire, mind, food, world), 0.75f);
	EXPECT_EQ(mind.nodes.at(food).thing, k_Town);
	// the mind's own town by ten times its rival's share
	world.townOwners[k_Town] = world.me;
	world.rivalBelief[k_Town] = 0.02f;
	const float importance = 0.02f * 10.0f;
	EXPECT_EQ(ScoreOf(ScoreKind::TakeOverTownByFulfillingFoodDesire, mind, food, world), importance * 0.75f);
	// the miracles that meet its needs: the wood miracle by its want of food and the food one by its want of wood
	world.citadel = ScoreWorld::Citadel {};
	world.inside = true;
	LayOutIcon(world, true, 120.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
	world.enabled = {MagicType::Wood, MagicType::Food};
	world.icons[MagicType::Wood] = k_Icon;
	world.icons[MagicType::Food] = k_Spell;
	world.iconSites[k_Spell] = k_Site;
	world.stores[k_Spell] = 120.0f;
	world.costs[k_Spell] = 100.0f;
	world.drops[k_Town] = k_Pit;
	const auto needs = NodeScoredBy(ScoreKind::CastSpellToFulfilTownNeeds);
	GiveParent(mind, needs, k_Town);
	const float keen = Bits(0x3F7FFDA3u);
	EXPECT_EQ(ScoreOf(ScoreKind::CastSpellToFulfilTownNeeds, mind, needs, world), keen * 0.75f);
	EXPECT_EQ(mind.nodes.at(needs).thing, k_Icon);
	world.rawDesires[{k_Town, 1}] = 0.875f;
	EXPECT_EQ(ScoreOf(ScoreKind::CastSpellToFulfilTownNeeds, mind, needs, world), keen * 0.875f);
	EXPECT_EQ(mind.nodes.at(needs).thing, k_Spell);
	// it asks the town for its store (which makes its food pot when it has neither)
	EXPECT_EQ(world.dropsAsked.back(), std::make_pair(k_Town, 0u));
}

namespace
{
/// A player as the oracle laid them out: their number, type, influence power and its average, their citadel's heart,
/// the harm each player did them, their alignment and who they count an ally
struct RivalAt
{
	uint8_t number;
	uint32_t type;
	float power, average;
	int32_t x, z;
	std::vector<std::pair<uint8_t, float>> harmFrom;
	float alignment;
	std::vector<uint8_t> allies;
};

constexpr int32_t k_Cell16 = 0x100000;

void LayOutRivals(FakeScoreWorld& world, const std::vector<RivalAt>& rivals)
{
	for (const auto& r : rivals)
	{
		const auto player = static_cast<PlayerNames>(r.number);
		world.types[player] = r.type;
		world.powers[player] = r.power;
		world.averages[player] = r.average;
		world.fixedPlaces[player] = MapCoords {r.x, r.z, 0.0f};
		world.alignments[player] = r.alignment;
		world.playerThings[player] = entt::entity {static_cast<uint32_t>(20 + r.number)};
		for (const auto& [by, harm] : r.harmFrom)
		{
			world.harms[{player, static_cast<PlayerNames>(by)}] = harm;
		}
		for (const auto ally : r.allies)
		{
			world.allies.insert({player, static_cast<PlayerNames>(ally)});
		}
	}
}
} // namespace

TEST(ComputerMindScores, TheDesireToAttackAPlayerIsTheOriginals)
{
	// the oracle's cases: the mind's player is number 1, the other 0 but in the last (the neutral)
	constexpr int32_t c = k_Cell16;
	struct Case
	{
		std::vector<RivalAt> rivals;
		uint8_t other;
		MapCoords hand;
		std::vector<std::vector<std::pair<int32_t, int32_t>>> towns;
		uint32_t desire;
	};
	const std::vector<Case> cases {
	    {{{1, 2, 100.0f, 100.0f, c, c, {}, 0.0f, {}}, {0, 2, 100.0f, 100.0f, c * 5, c, {}, 0.0f, {}}},
	     0,
	     {c, c, 0.0f},
	     {},
	     0x00000000u},
	    {{{1, 2, 100.0f, 80.0f, c, c, {}, 0.0f, {}}, {0, 2, 150.0f, 100.0f, c * 5, c, {}, 0.0f, {}}},
	     0,
	     {c, c, 0.0f},
	     {},
	     0x3F3FF7C0u},
	    {{{1, 2, 200.0f, 200.0f, c, c, {}, 0.0f, {}}, {0, 2, 50.0f, 60.0f, c * 9, c * 4, {}, 0.0f, {}}},
	     0,
	     {c, c, 0.0f},
	     {},
	     0x3ED788DAu},
	    // the harm each did the other
	    {{{1, 2, 100.0f, 100.0f, c, c, {{0, 30.0f}}, 0.0f, {}}, {0, 2, 100.0f, 100.0f, c * 5, c, {{1, 10.0f}}, 0.0f, {}}},
	     0,
	     {c, c, 0.0f},
	     {},
	     0x3F2A9D08u},
	    {{{1, 2, 100.0f, 100.0f, c, c, {{0, 5.0f}}, 0.0f, {}}, {0, 2, 100.0f, 100.0f, c * 5, c, {{1, 40.0f}}, 0.0f, {}}},
	     0,
	     {c, c, 0.0f},
	     {},
	     0x00000000u},
	    // alignments a whole -1..1 apart count nothing: the gap is counted in whole quarters
	    {{{1, 2, 100.0f, 100.0f, c, c, {}, 0.9f, {}}, {0, 2, 100.0f, 100.0f, c * 5, c, {}, -0.9f, {}}},
	     0,
	     {c, c, 0.0f},
	     {},
	     0x00000000u},
	    // the hand near its own buildings
	    {{{1, 2, 100.0f, 100.0f, c, c, {}, 0.0f, {}}, {0, 2, 100.0f, 100.0f, c * 5, c, {}, 0.0f, {}}},
	     0,
	     {c * 3, c, 0.0f},
	     {{{c * 3 + 0x30000, c}, {c * 3, c + 0x8000}}},
	     0x3F73203Fu},
	    {{{1, 2, 10.0f, 10.0f, c, c, {}, 0.0f, {}}, {0, 2, 10.0f, 10.0f, c * 300, c, {}, 0.0f, {}}},
	     0,
	     {c, c, 0.0f},
	     {},
	     0x00000000u},
	    {{{1, 2, 0.0f, 0.0f, c, c, {}, 0.0f, {}}, {0, 2, 0.0f, 0.0f, c * 2, c, {}, 0.0f, {}}},
	     0,
	     {c, c, 0.0f},
	     {},
	     0x00000000u},
	    {{{1, 2, 123.456f, 111.1f, c, c, {{7, 2.5f}}, 0.3f, {}}, {7, 3, 33.3f, 34.4f, c * 2, c * 3, {{1, 1.25f}}, -0.2f, {}}},
	     7,
	     {c * 2, c * 2, 0.0f},
	     {{{c * 2 + 0x5000, c * 2 + 0x7000}}, {{c * 4, c * 4}}},
	     0x3FEAEA74u},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& k = cases[index];
		FakeScoreWorld world;
		LayOutRivals(world, k.rivals);
		world.hand = k.hand;
		auto entity = static_cast<uint32_t>(k_Town);
		for (const auto& abodes : k.towns)
		{
			const entt::entity town {entity++};
			world.townsOf[world.me].push_back(town);
			for (const auto& [x, z] : abodes)
			{
				const entt::entity abode {100 + entity++};
				world.abodesOf[town].push_back(abode);
				world.places[abode] = MapCoords {x, z, 0.0f};
			}
		}
		EXPECT_EQ(BitsOf(scores::DesireToAttack(world, static_cast<PlayerNames>(k.other))), k.desire) << index;
	}
}

TEST(ComputerMindScores, ThePlayerToDefeatIsTheOneMostWantedByTheAttitude)
{
	constexpr int32_t c = k_Cell16;
	const auto node = NodeScoredBy(ScoreKind::DefeatPlayer37);
	const RivalAt me {1, 2, 100.0f, 100.0f, c, c, {}, 0.0f, {}};
	const RivalAt first {0, 2, 150.0f, 100.0f, c * 5, c, {}, 0.0f, {}};
	struct Case
	{
		std::vector<RivalAt> rivals;
		std::vector<std::pair<uint8_t, float>> attitudes;
		uint32_t score;
		uint8_t chosen;
	};
	const std::vector<Case> cases {
	    {{me, first, {7, 3, 0.0f, 0.0f, c * 7, c * 7, {}, 0.0f, {}}}, {}, 0x3F7FF615u, 0},
	    // by the attitude, kept doubled; up to 1
	    {{me, first, {2, 2, 300.0f, 100.0f, c * 4, c, {}, 0.0f, {}}, {7, 3, 0.0f, 0.0f, c, c * 8, {}, 0.0f, {}}},
	     {{0, 1.0f}, {2, 0.5f}, {7, 2.0f}},
	     0x3F800000u,
	     2},
	    // a player who counts the mind's an ally is passed over; the neutral player is walked last
	    {{me, {0, 2, 150.0f, 100.0f, c * 5, c, {}, 0.0f, {1}}, {7, 3, 0.0f, 0.0f, c, c * 8, {}, 0.0f, {}}}, {}, 0x3F7F72AEu, 7},
	    // the first is taken at nothing, and a later one worth more takes its place
	    {{me, {0, 2, 100.0f, 100.0f, c * 300, c, {}, 0.0f, {}}, {7, 3, 0.0f, 0.0f, c * 300, c * 300, {}, 0.0f, {}}},
	     {},
	     0x38172447u,
	     7},
	    {{{1, 2, 10.0f, 10.0f, c, c, {}, 0.0f, {}},
	      {0, 2, 500.0f, 10.0f, c * 2, c, {}, 0.0f, {}},
	      {7, 3, 0.0f, 0.0f, c, c * 8, {}, 0.0f, {}}},
	     {{0, 4.0f}},
	     0x3F800000u,
	     0},
	    // the neutral player is walked whatever its type
	    {{me, {7, 0, 0.0f, 0.0f, c * 7, c * 7, {}, 0.0f, {}}}, {}, 0x3F7E78BCu, 7},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& k = cases[index];
		FakeScoreWorld world;
		LayOutRivals(world, k.rivals);
		world.hand = MapCoords {c, c, 0.0f};
		auto mind = MakeMind();
		for (const auto& [number, attitude] : k.attitudes)
		{
			mind.attitude.at(number) = attitude;
		}
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::DefeatPlayer37, mind, node, world)), k.score) << index;
		EXPECT_EQ(mind.nodes.at(node).thing, entt::entity {static_cast<uint32_t>(20 + k.chosen)}) << index;
	}
	EXPECT_TRUE(IsScorePorted(ScoreKind::DefeatPlayer37));
}

TEST(ComputerMindScores, TheActivePlayersAreWalkedThenTheNeutralOne)
{
	FakeScoreWorld world;
	world.types[PlayerNames::PLAYER_TWO] = 2;
	world.types[PlayerNames::PLAYER_FIVE] = 1;
	std::vector<PlayerNames> walked;
	for (auto player = scores::NextActivePlayerOrNeutral(world, std::nullopt); player.has_value();
	     player = scores::NextActivePlayerOrNeutral(world, player))
	{
		walked.push_back(*player);
	}
	EXPECT_EQ(walked, (std::vector<PlayerNames> {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_FIVE, PlayerNames::NEUTRAL}));
	EXPECT_FALSE(scores::NextActivePlayerOrNeutral(world, PlayerNames::NEUTRAL).has_value());
}

TEST(ComputerMindScores, AFloatPlusADoubleRoundsOnce)
{
	// 0.0001 and 0.001 as the game adds them to a float
	EXPECT_EQ(scores::AddRoundedOnce(100.0f, 0.0001), 100.0001f);
	EXPECT_EQ(scores::AddRoundedOnce(0.0f, 0.001), static_cast<float>(0.001));
	// a sum just above the tie between two floats, which the double sum rounds onto the tie (the double next above
	// 2^-24, as 2^-24 + 2^-80 is not a double): one rounding goes up
	const double tie = 1.0 + std::ldexp(1.0, -24);
	EXPECT_EQ(scores::AddRoundedOnce(1.0f, std::nextafter(std::ldexp(1.0, -24), 1.0)), std::nextafter(1.0f, 2.0f));
	EXPECT_EQ(scores::AddRoundedOnce(1.0f, std::ldexp(1.0, -24)), static_cast<float>(tie));
}

namespace
{
/// A place on the map, from metres, as the oracle laid it out
MapCoords Metres(double x, double z)
{
	return MapCoords {static_cast<int32_t>(std::lround(x * 6553.6)), static_cast<int32_t>(std::lround(z * 6553.6)), 0.0f};
}

/// A tree at a place, with its wood as it gives it and as its kind's row gives it
void LayOutTree(FakeScoreWorld& world, entt::entity tree, double x, double z, float wood, uint32_t kindWood)
{
	world.treeSet.insert(tree);
	world.places[tree] = Metres(x, z);
	world.woodValues[tree] = wood;
	world.kindWood[tree] = kindWood;
}
} // namespace

TEST(ComputerMindScores, WoodIsTakenFromTheBestForestOrTreeTheMindHasSeen)
{
	// the oracle's cases: both hands at (100, 100) m unless a case says otherwise; inside the influence
	using Seen = ecs::components::ComputerMindSeen;
	const auto node = NodeScoredBy(ScoreKind::GetWoodFromForest);
	constexpr entt::entity k_A {40};
	constexpr entt::entity k_B {41};
	constexpr entt::entity k_C {42};
	const auto prepare = [](float power) {
		FakeScoreWorld world;
		world.inside = true;
		world.powers[world.me] = power;
		world.hand = Metres(100, 100);
		world.leaderHand = Metres(100, 100);
		return world;
	};
	const auto check = [&](FakeScoreWorld& world, const std::vector<Seen>& seen, uint32_t score, entt::entity thing,
	                       const char* label) {
		auto mind = MakeMind();
		mind.seen = seen;
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::GetWoodFromForest, mind, node, world)), score) << label;
		EXPECT_EQ(mind.nodes.at(node).thing, thing) << label;
	};
	{
		auto world = prepare(500.0f);
		check(world, {}, 0x00000000u, k_None, "nothing seen");
		// a forest with no grown tree
		world.forestCentres[1] = k_A;
		LayOutTree(world, k_A, 110, 100, 100.0f, 100);
		check(world, {Seen {.forest = 1}}, 0x00000000u, k_None, "no grown tree");
	}
	{
		// a forest of 3 grown trees 50 m from both hands, wood 100; the mind's hand 200 m further for the score
		auto world = prepare(500.0f);
		world.forestGrown[1] = 3;
		world.forestCentres[1] = k_A;
		LayOutTree(world, k_A, 150, 100, 100.0f, 100);
		check(world, {Seen {.forest = 1}}, 0x3F7FFFFFu, k_A, "a forest");
		world.hand = Metres(300, 100);
		check(world, {Seen {.forest = 1}}, 0x3F7FFFB2u, k_A, "the mind's hand further");
		// its centre tree held by a script: the forest is passed over
		world.hand = Metres(100, 100);
		world.scripted.insert(k_A);
		LayOutTree(world, k_B, 400, 100, 1.0f, 1);
		check(world, {Seen {.forest = 1}, Seen {.thing = k_B}}, 0x3F7F72E1u, k_B, "held by a script");
	}
	{
		// a loose tree of wood 80; two loose trees as far, the first seen counting
		auto world = prepare(300.0f);
		LayOutTree(world, k_A, 130, 140, 80.0f, 80);
		check(world, {Seen {.thing = k_A}}, 0x3F7FFFFCu, k_A, "a loose tree");
		LayOutTree(world, k_B, 130, 100, 50.0f, 50);
		LayOutTree(world, k_C, 70, 100, 50.0f, 50);
		check(world, {Seen {.thing = k_B}, Seen {.thing = k_C}}, 0x3F7FFFFFu, k_B, "a tie");
	}
	{
		// outside the influence with nowhere inside: nothing
		auto world = prepare(500.0f);
		world.inside = false;
		world.forestGrown[1] = 1;
		world.forestCentres[1] = k_A;
		LayOutTree(world, k_A, 150, 100, 100.0f, 100);
		check(world, {Seen {.forest = 1}}, 0x00000000u, k_None, "outside the influence");
	}
	{
		// a seed is no wood; a loose tree of wood 100 at 20 m against a forest of 2 grown at 50 m
		auto world = prepare(500.0f);
		world.forestGrown[1] = 2;
		world.forestCentres[1] = k_A;
		LayOutTree(world, k_A, 150, 100, 100.0f, 100);
		LayOutTree(world, k_B, 120, 100, 100.0f, 100);
		world.places[k_C] = Metres(105, 100);
		check(world, {Seen {.thing = k_C}, Seen {.thing = k_B}, Seen {.forest = 1}}, 0x3F7FFFFFu, k_A, "the forest");
	}
	{
		// far away and weak
		auto world = prepare(100.0f);
		LayOutTree(world, k_A, 2100, 100, 1.0f, 1);
		check(world, {Seen {.thing = k_A}}, 0x3DEA5E18u, k_A, "far");
		// a forest of one grown tree whose wood the land's balance quarters, with no power at all
		auto none = prepare(0.0f);
		none.forestGrown[1] = 1;
		none.forestCentres[1] = k_A;
		LayOutTree(none, k_A, 150, 100, 25.0f, 100);
		check(none, {Seen {.forest = 1}}, 0x3DEA5E18u, k_A, "no power");
	}
	EXPECT_TRUE(IsScorePorted(ScoreKind::GetWoodFromForest));
}

TEST(ComputerMindScores, AFloatTimesADoubleRoundsOnce)
{
	EXPECT_EQ(BitsOf(scores::MulRoundedOnce(1.0f, 0.85)), 0x3F59999Au);
	EXPECT_EQ(scores::MulRoundedOnce(0.75f, 0.5), 0.375f);
	EXPECT_EQ(BitsOf(scores::MulRoundedOnce(Bits(0x3F5DDDDEu), 0.85)),
	          BitsOf(static_cast<float>(static_cast<double>(Bits(0x3F5DDDDEu)) * 0.85)));
}

namespace
{

/// A place a town's villagers may be, with that many villagers in its own map cell
struct PlaceAt
{
	int32_t x;
	int32_t z;
	uint32_t villagers;
};

/// The places of the town (k_Town) the oracle looked at: its store's door, its meeting point, its centre and its fields
struct TownPlaces
{
	std::optional<PlaceAt> door;
	PlaceAt congregation;
	std::optional<PlaceAt> centre;
	std::vector<PlaceAt> fields;
};

constexpr entt::entity k_Centre {90};

void LayOutTownPlaces(FakeScoreWorld& world, const TownPlaces& places)
{
	world.towns.insert(k_Town);
	auto next = static_cast<uint32_t>(1000);
	const auto villagers = [&world, &next](const PlaceAt& at) {
		auto& cell = world.villagersAt[{at.x >> 16, at.z >> 16}];
		for (uint32_t k = 0; k < at.villagers; ++k)
		{
			cell.push_back(entt::entity {next++});
		}
	};
	if (places.door.has_value())
	{
		world.pits[k_Town] = k_Pit;
		world.doors[k_Pit] = MapCoords {places.door->x, places.door->z, 0.0f};
		villagers(*places.door);
	}
	world.congregations[k_Town] = MapCoords {places.congregation.x, places.congregation.z, 0.0f};
	villagers(places.congregation);
	if (places.centre.has_value())
	{
		world.townCentres[k_Town] = k_Centre;
		world.places[k_Centre] = MapCoords {places.centre->x, places.centre->z, 0.0f};
		villagers(*places.centre);
	}
	auto field = static_cast<uint32_t>(91);
	for (const auto& at : places.fields)
	{
		const entt::entity entity {field++};
		world.fieldsOf[k_Town].push_back(entity);
		world.places[entity] = MapCoords {at.x, at.z, 0.0f};
		villagers(at);
	}
}

} // namespace

TEST(ComputerMindScores, TheVillagersNearAPlaceAreThoseOfTheNineCellsRoundIt)
{
	FakeScoreWorld world;
	for (int32_t x = 9; x <= 11; ++x)
	{
		for (int32_t z = 9; z <= 11; ++z)
		{
			world.villagersAt[{x, z}].push_back(entt::entity {static_cast<uint32_t>(100 + x * 16 + z)});
		}
	}
	world.villagersAt[{10, 10}].push_back(entt::entity {99});
	world.villagersAt[{12, 10}].push_back(entt::entity {98});
	world.villagersAt[{10, 8}].push_back(entt::entity {97});
	const MapCoords place {0xA8000, 0xA4000, 0.0f};
	EXPECT_EQ(scores::VillagersNear(world, place, 9), 10u);
	EXPECT_EQ(scores::VillagersNear(world, place, 1), 2u);
	// from (11, 10): its square holds eight of them, and the spiral's tenth cell, (9, 11), one more
	EXPECT_EQ(scores::VillagersNear(world, MapCoords {0xB8000, 0xA8000, 0.0f}, 9), 8u);
	EXPECT_EQ(scores::VillagersNear(world, MapCoords {0xB8000, 0xA8000, 0.0f}, 10), 9u);
}

TEST(ComputerMindScores, WhereATownsVillagersAreIsTheOriginals)
{
	struct Case
	{
		TownPlaces places;
		float sleep;
		uint32_t score;
		int32_t x, z;
	};
	// the oracle's cases, the hand at (0x100000, 0x100000)
	const std::vector<Case> cases {
	    {{PlaceAt {0x140000, 0x100000, 1},
	      {0x180000, 0x100000, 4},
	      PlaceAt {0x1C0000, 0x100000, 4},
	      {{0x200000, 0x100000, 2}, {0x240000, 0x100000, 9}}},
	     0.25f,
	     0x410FB09Fu,
	     0x240000,
	     0x100000},
	    // a sleep of just half still lets the villagers be looked for
	    {{PlaceAt {0x140000, 0x100000, 1},
	      {0x180000, 0x100000, 4},
	      PlaceAt {0x1C0000, 0x100000, 4},
	      {{0x200000, 0x100000, 2}, {0x240000, 0x100000, 9}}},
	     0.5f,
	     0x410FB09Fu,
	     0x240000,
	     0x100000},
	    // of equals the first stays first
	    {{PlaceAt {0x140000, 0x100000, 3}, {0x100000, 0x140000, 3}, PlaceAt {0x0C0000, 0x100000, 5}, {{0x100000, 0x0C0000, 5}}},
	     0.25f,
	     0x409FFBE5u,
	     0x0C0000,
	     0x100000},
	    {{PlaceAt {0x2E0000, 0x100000, 10}, {0x420000, 0x100000, 6}, PlaceAt {0x560000, 0x100000, 3}, {}},
	     0.25f,
	     0x411D5F3Cu,
	     0x2E0000,
	     0x100000},
	    {{std::nullopt, {0x140000, 0x100000, 2}, PlaceAt {0x6A0000, 0x100000, 40}, {{0x180000, 0x180000, 1}}},
	     0.25f,
	     0x3FFFF96Eu,
	     0x140000,
	     0x100000},
	    {{PlaceAt {0x140000, 0x100000, 0}, {0x180000, 0x100000, 0}, std::nullopt, {{0x8A0000, 0x7A0000, 1}}},
	     0.25f,
	     0x38172465u,
	     0x8A0000,
	     0x7A0000},
	    // nobody anywhere: the store's door, else the meeting point
	    {{PlaceAt {0x140000, 0x100000, 0}, {0x120000, 0x100000, 0}, PlaceAt {0x110000, 0x100000, 0}, {{0x108000, 0x100000, 0}}},
	     0.0f,
	     0x00000000u,
	     0x140000,
	     0x100000},
	    {{std::nullopt, {0x120000, 0x100000, 0}, PlaceAt {0x110000, 0x100000, 0}, {{0x108000, 0x100000, 0}}},
	     0.0f,
	     0x00000000u,
	     0x120000,
	     0x100000},
	    // far away
	    {{PlaceAt {0x6140000, 0x100000, 5}, {0x6120000, 0x100000, 5}, std::nullopt, {{0x6108000, 0x100000, 5}}},
	     0.0f,
	     0x393CED7Eu,
	     0x6140000,
	     0x100000},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		LayOutTownPlaces(world, c.places);
		world.rawDesires[{k_Town, 16}] = c.sleep;
		MapCoords where {0x7777, 0x7777, 9.0f};
		const float score = scores::PosWhereVillagersAre(world, k_Town, MapCoords {0x100000, 0x100000, 0.0f}, where);
		EXPECT_EQ(BitsOf(score), c.score) << index;
		EXPECT_EQ(where.x, c.x) << index;
		EXPECT_EQ(where.z, c.z) << index;
		EXPECT_EQ(world.congregationAsked, 1u) << index;
	}
	// a town that wants sleep more than half: nothing, nothing asked and the place left as it was
	FakeScoreWorld world;
	LayOutTownPlaces(world, cases.front().places);
	world.rawDesires[{k_Town, 16}] = 0.6f;
	MapCoords where {0x7777, 0x7777, 9.0f};
	EXPECT_EQ(scores::PosWhereVillagersAre(world, k_Town, MapCoords {0x100000, 0x100000, 0.0f}, where), 0.0f);
	EXPECT_EQ(where.x, 0x7777);
	EXPECT_EQ(where.z, 0x7777);
	EXPECT_EQ(world.congregationAsked, 0u);
}

TEST(ComputerMindScores, AnImpressiveMiracleIsWorthAsMuchAsTheTownsVillagersAreLikelyToSeeIt)
{
	const auto prepare = [](const TownPlaces& places) {
		FakeScoreWorld world;
		world.citadel = ScoreWorld::Citadel {};
		world.inside = true;
		LayOutIcon(world, true, 120.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
		world.enabled = {MagicType::Food, MagicType::Heal};
		world.icons[MagicType::Food] = k_Icon;
		world.icons[MagicType::Heal] = k_Icon;
		world.impressive[MagicType::Food] = 1.0f;
		world.hand = MapCoords {0x100000, 0x100000, 0.0f};
		LayOutTownPlaces(world, places);
		world.places[k_Town] = MapCoords {0x150000, 0x160000, 0.0f};
		return world;
	};
	const float keen = Bits(0x3F7FFDA3u);
	const TownPlaces likely {PlaceAt {0x2E0000, 0x100000, 10}, {0x420000, 0x100000, 6}, PlaceAt {0x560000, 0x100000, 3}, {}};
	const TownPlaces unlikely {
	    PlaceAt {0x140000, 0x100000, 0}, {0x180000, 0x100000, 0}, std::nullopt, {{0x8A0000, 0x7A0000, 1}}};
	const auto node = NodeScoredBy(ScoreKind::TeachCreatureImpressiveMagic168);
	{
		// nothing above: nothing, and the town is not asked
		auto world = prepare(likely);
		auto mind = MakeMind();
		EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureImpressiveMagic168, mind, node, world), 0.0f);
		EXPECT_EQ(world.congregationAsked, 0u);
	}
	{
		// the villagers are likely to see it: the miracle's whole worth, the town asked once
		auto world = prepare(likely);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureImpressiveMagic168, mind, node, world), keen);
		EXPECT_EQ(world.congregationAsked, 1u);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
		EXPECT_EQ(mind.nodes.at(node).point, world.WorldPoint(MapCoords {0x2E0000, 0x100000, 0.0f}));
		// only the miracles with an impressive balance were looked for
		EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Food)), 1u);
		EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Heal)), 0u);
	}
	{
		// unlikely: the town is asked twice and the worth is that much less
		auto world = prepare(unlikely);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::TeachCreatureImpressiveMagic168, mind, node, world)), 0x38172300u);
		EXPECT_EQ(world.congregationAsked, 2u);
		EXPECT_EQ(mind.nodes.at(node).point, world.WorldPoint(MapCoords {0x8A0000, 0x7A0000, 0.0f}));
	}
	{
		// a sleepy town: nothing, but the icon is still the node's, at the point of nowhere
		auto world = prepare(likely);
		world.rawDesires[{k_Town, 16}] = 0.75f;
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureImpressiveMagic168, mind, node, world), 0.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
		EXPECT_EQ(mind.nodes.at(node).point, world.WorldPoint(MapCoords {}));
	}
	{
		// no impressive miracle: nothing chosen, though the town is asked
		auto world = prepare(likely);
		world.impressive.clear();
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::TeachCreatureImpressiveMagic168, mind, node, world), 0.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_None);
		EXPECT_EQ(world.congregationAsked, 1u);
	}
	EXPECT_TRUE(IsScorePorted(ScoreKind::TeachCreatureImpressiveMagic168));
}

namespace
{

/// A building of the town (k_Town) the oracle weighed for an attack
struct AttackAbodeAt
{
	int32_t x;
	int32_t z;
	float percentBuilt;
	float life;
	float desire;
};

/// A field of the town the oracle weighed for an attack
struct AttackFieldAt
{
	int32_t x;
	int32_t z;
	float food;
	float capacity;
	float desire;
};

/// The buildings come as entities 100.., the fields as 120..
void LayOutAttackTargets(FakeScoreWorld& world, const std::vector<AttackAbodeAt>& abodes,
                         const std::vector<AttackFieldAt>& fields)
{
	world.towns.insert(k_Town);
	auto next = static_cast<uint32_t>(100);
	for (const auto& at : abodes)
	{
		const entt::entity abode {next++};
		world.abodesOf[k_Town].push_back(abode);
		world.places[abode] = MapCoords {at.x, at.z, 0.0f};
		world.percentBuilt[abode] = at.percentBuilt;
		world.lives[abode] = at.life;
		world.abodeAttack[abode] = at.desire;
	}
	next = 120;
	for (const auto& at : fields)
	{
		const entt::entity field {next++};
		world.fieldsOf[k_Town].push_back(field);
		world.places[field] = MapCoords {at.x, at.z, 0.0f};
		world.fieldFood[field] = at.food;
		world.fieldCapacity[field] = at.capacity;
		world.fieldAttack[field] = at.desire;
	}
}

} // namespace

TEST(ComputerMindScores, ATownsBuildingMostWorthAttackingIsTheOriginals)
{
	struct Case
	{
		std::vector<AttackAbodeAt> abodes;
		std::vector<AttackFieldAt> fields;
		entt::entity best;
	};
	constexpr entt::entity k_First {100};
	constexpr entt::entity k_Second {101};
	constexpr entt::entity k_Field {120};
	// the oracle's cases, from the hand at (0x100000, 0x100000)
	const std::vector<Case> cases {
	    {{{0x110000, 0x100000, 1.0f, 1.0f, 0.5f}, {0x120000, 0x100000, 1.0f, 1.0f, 0.75f}},
	     {{0x130000, 0x100000, 100.0f, 400.0f, 1.0f}},
	     k_Second},
	    // of equals the first stays
	    {{{0x110000, 0x100000, 0.5f, 0.5f, 2.0f}, {0x110000, 0x100000, 1.0f, 0.25f, 2.0f}}, {}, k_First},
	    {{{0x110000, 0x100000, 0.25f, 1.0f, 1.0f}}, {{0x110000, 0x100000, 350.0f, 400.0f, 0.3f}}, k_Field},
	    {{{0x110000, 0x100000, 0.0f, 1.0f, 1.0f}}, {{0x110000, 0x100000, 0.0f, 400.0f, 1.0f}}, k_None},
	    {{{0x7110000, 0x100000, 1.0f, 1.0f, 1.0f}}, {}, k_First},
	    {{{0x123456, 0x0FEDCB, 0.8125f, 0.6f, 0.7f}, {0x0F1234, 0x1A0000, 0.9f, 0.95f, 0.66f}},
	     {{0x0E8000, 0x123000, 123.0f, 350.0f, 0.9f}},
	     k_Second},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		LayOutAttackTargets(world, c.abodes, c.fields);
		EXPECT_EQ(scores::BestAttackObject(world, k_Town, MapCoords {0x100000, 0x100000, 0.0f}), c.best) << index;
	}
}

TEST(ComputerMindScores, AnAttackMiracleGoesWhereTheTownIsMostWorthAttacking)
{
	const auto prepare = [] {
		FakeScoreWorld world;
		world.citadel = ScoreWorld::Citadel {};
		LayOutIcon(world, true, 120.0f, 100.0f, 5.0f, 2.0f, 30.0f, 1.5f);
		world.enabled = {MagicType::Fireball, MagicType::Heal};
		world.icons[MagicType::Fireball] = k_Icon;
		world.icons[MagicType::Heal] = k_Icon;
		world.aggressive[MagicType::Fireball] = 1.0f;
		world.hand = MapCoords {0x100000, 0x100000, 0.0f};
		return world;
	};
	const float keen = Bits(0x3F7FFDA3u);
	const auto node = NodeScoredBy(ScoreKind::CastSpellOnMyCreatureForALaugh170);
	{
		// at the building most worth attacking: the miracle is worth something only where it goes
		auto world = prepare();
		LayOutAttackTargets(world, {{0x110000, 0x100000, 1.0f, 1.0f, 0.5f}, {0x120000, 0x100000, 1.0f, 1.0f, 0.75f}}, {});
		world.centres[k_Town] = MapCoords {0x1B0000, 0x1C0000, 0.0f};
		world.insideAt = {{0x120000, 0x100000}};
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::CastSpellOnMyCreatureForALaugh170, mind, node, world), keen);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
		// only the miracles with an attack value were looked for
		EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Fireball)), 1u);
		EXPECT_EQ(mind.iconsFound.at(static_cast<size_t>(MagicType::Heal)), 0u);
		world.insideAt = {{0x1B0000, 0x1C0000}};
		auto other = MakeMind();
		GiveParent(other, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::CastSpellOnMyCreatureForALaugh170, other, node, world), 0.0f);
		EXPECT_EQ(other.nodes.at(node).thing, k_None);
	}
	{
		// nothing worth attacking: the town's middle, at half the attack value
		auto world = prepare();
		world.towns.insert(k_Town);
		world.centres[k_Town] = MapCoords {0x1B0000, 0x1C0000, 0.0f};
		world.insideAt = {{0x1B0000, 0x1C0000}};
		world.aggressive[MagicType::Fireball] = 0.5f;
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::CastSpellOnMyCreatureForALaugh170, mind, node, world), keen * 0.5f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
	}
	{
		// a thing with a place
		auto world = prepare();
		world.places[k_Target] = MapCoords {0x190000, 0x1A0000, 0.0f};
		world.insideAt = {{0x190000, 0x1A0000}};
		auto mind = MakeMind();
		GiveParent(mind, node, k_Target);
		EXPECT_EQ(ScoreOf(ScoreKind::CastSpellOnMyCreatureForALaugh170, mind, node, world), keen);
	}
	{
		// nothing above: the map's corner
		auto world = prepare();
		world.insideAt = {{0, 0}};
		auto mind = MakeMind();
		EXPECT_EQ(ScoreOf(ScoreKind::CastSpellOnMyCreatureForALaugh170, mind, node, world), keen);
		EXPECT_EQ(mind.nodes.at(node).thing, k_Icon);
	}
	EXPECT_TRUE(IsScorePorted(ScoreKind::CastSpellOnMyCreatureForALaugh170));
}

namespace
{

/// A thing on the oracle's map: its place (map coordinates) and what the hand weighs it by
struct ThingAt
{
	int32_t x;
	int32_t z;
	ScoreWorld::ThrowFacts facts;
};

/// The things come as entities 200.., each listed in its cell after those listed before it (the oracle's cells, whose
/// fixed list gets each new thing at its head, are given here in the order the game walks them)
void LayOutThings(FakeScoreWorld& world, const std::vector<ThingAt>& things)
{
	auto next = static_cast<uint32_t>(200);
	for (const auto& at : things)
	{
		const entt::entity thing {next++};
		world.places[thing] = MapCoords {at.x, at.z, 0.0f};
		world.throwFacts[thing] = at.facts;
		world.cellObjects[{map_coords::CellOf(at.x), map_coords::CellOf(at.z)}].push_back(thing);
	}
}

constexpr ScoreWorld::ThrowFacts Rock(float weight)
{
	return {.throwable = true, .breaksBuildings = true, .weight = weight};
}

} // namespace

TEST(ComputerMindScores, TheThingsNearAPlaceAreLookedAtCellByCellAsTheOriginalDoes)
{
	const auto weight = [](const FakeScoreWorld& world) {
		return [&world](entt::entity thing) { return world.ThrowFactsOf(thing).weight; };
	};
	{
		// 25 m round (0x88000, 0x88000) on a map of 16 x 16 cells: the cells 6 to 11 each way, x outer
		FakeScoreWorld world;
		world.mapCells = {16, 16};
		world.inside = true;
		LayOutThings(world, {{0x88000, 0x88000, Rock(1.0f)},
		                     {0xA8000, 0x88000, Rock(5.0f)},
		                     {0xC8000, 0x88000, Rock(50.0f)},
		                     {0x58000, 0x88000, Rock(1.5f)}});
		const MapCoords place {0x88000, 0x88000, 0.0f};
		EXPECT_EQ(scores::BestThingNear(world, place, 25.0f, weight(world)), entt::entity {200});
		ASSERT_EQ(world.cellsAsked.size(), 36u);
		EXPECT_EQ(world.cellsAsked.front(), std::make_pair(uint16_t {6}, uint16_t {6}));
		EXPECT_EQ(world.cellsAsked[1], std::make_pair(uint16_t {6}, uint16_t {7}));
		EXPECT_EQ(world.cellsAsked.back(), std::make_pair(uint16_t {11}, uint16_t {11}));
	}
	{
		// 30 m round the middle of the corner cell: the cells off the map are passed over
		FakeScoreWorld world;
		world.mapCells = {16, 16};
		world.inside = true;
		LayOutThings(world, {{0x8000, 0x8000, Rock(1.0f)}, {0x28000, 0x38000, Rock(2.0f)}});
		EXPECT_EQ(scores::BestThingNear(world, MapCoords {0x8000, 0x8000, 0.0f}, 30.0f, weight(world)), entt::entity {200});
		ASSERT_EQ(world.cellsAsked.size(), 16u);
		EXPECT_EQ(world.cellsAsked.front(), std::make_pair(uint16_t {0}, uint16_t {0}));
		EXPECT_EQ(world.cellsAsked.back(), std::make_pair(uint16_t {3}, uint16_t {3}));
	}
	{
		// only the cells whose middle is inside the influence are looked in
		FakeScoreWorld world;
		world.mapCells = {16, 16};
		world.insideAt = {{0x98000, 0x88000}};
		LayOutThings(world, {{0x88000, 0x88000, Rock(5.0f)}, {0x98000, 0x88000, Rock(1.0f)}});
		EXPECT_EQ(scores::BestThingNear(world, MapCoords {0x88000, 0x88000, 0.0f}, 25.0f, weight(world)), entt::entity {201});
		ASSERT_EQ(world.cellsAsked.size(), 1u);
		// and nothing at all where the influence reaches nowhere
		world.insideAt.clear();
		EXPECT_EQ(scores::BestThingNear(world, MapCoords {0x88000, 0x88000, 0.0f}, 25.0f, weight(world)), k_None);
	}
}

TEST(ComputerMindScores, ARockOrATreeNearTheHandIsThrownAsTheOriginalChoosesIt)
{
	struct Case
	{
		std::vector<ThingAt> things;
		bool influence;
		entt::entity thrown;
	};
	const ScoreWorld::ThrowFacts tree {.throwable = true, .tree = true, .weight = 3.0f};
	const ScoreWorld::ThrowFacts unthrowable {.breaksBuildings = true, .weight = 9.0f};
	const ScoreWorld::ThrowFacts plain {.throwable = true, .weight = 9.0f};
	// the oracle's cases, the hand at (0x88000, 0x88000) on a map of 16 x 16 cells
	const std::vector<Case> cases {
	    // a tree two cells off beats a rock at the hand; neither the thing a hand cannot throw nor the plain one counts
	    {{{0x89000, 0x88000, plain}, {0x88000, 0x88000, Rock(2.0f)}, {0xA8000, 0x88000, tree}, {0x90000, 0x88000, unthrowable}},
	     true,
	     entt::entity {202}},
	    {{{0x89000, 0x88000, plain}, {0x88000, 0x88000, Rock(2.0f)}, {0xA8000, 0x88000, tree}, {0x90000, 0x88000, unthrowable}},
	     false,
	     k_None},
	    // of two equals in a cell the one walked first
	    {{{0x98000, 0x88000, Rock(2.0f)}, {0x98000, 0x88000, Rock(2.0f)}}, true, entt::entity {200}},
	    // a heavier rock farther off
	    {{{0x88000, 0x88000, Rock(1.0f)}, {0xE8000, 0xE8000, Rock(1.25f)}}, true, entt::entity {201}},
	};
	const auto node = NodeScoredBy(ScoreKind::ThrowrockatCreature);
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.mapCells = {16, 16};
		world.inside = c.influence;
		world.hand = MapCoords {0x88000, 0x88000, 0.0f};
		LayOutThings(world, c.things);
		auto mind = MakeMind();
		// with no creature above, whatever is thrown is worth 1
		EXPECT_EQ(ScoreOf(ScoreKind::ThrowrockatCreature, mind, node, world), c.thrown != k_None ? 1.0f : 0.0f) << index;
		EXPECT_EQ(mind.nodes.at(node).thing, c.thrown) << index;
	}
}

TEST(ComputerMindScores, ThePointBeyondTheNearestPlaceIsTheOriginals)
{
	struct Case
	{
		float speed;
		MapCoords nearest, from;
		int32_t x, z;
	};
	// the oracle's cases: the point the influence factor is asked at
	const std::vector<Case> cases {
	    {90.0f, {0x500000, 0x500000, 0.0f}, {0x300000, 0x500000, 3.5f}, 0x420000, 0x500000},
	    {90.0f, {0x500000, 0x500000, 0.0f}, {0x400000, 0x400000, 3.5f}, 0x4CBA59, 0x4CBA59},
	    {37.5f, {0x123456, 0x0FEDCB, 0.0f}, {0x2A0000, 0x050000, 3.5f}, 0x232BF4, 0x081A2F},
	    // towards itself is along x
	    {90.0f, {0x500000, 0x500000, 0.0f}, {0x500000, 0x500000, 3.5f}, 0x620000, 0x500000},
	    {0.0f, {0x500000, 0x500000, 0.0f}, {0x400000, 0x600000, 3.5f}, 0x400000, 0x600000},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		FakeScoreWorld world;
		world.speed = c.speed;
		world.insideAt = {{c.x, c.z}};
		EXPECT_EQ(scores::FactorBeyond(world, c.nearest, c.from), 1.0f) << index;
		world.insideAt = {{c.x + 1, c.z}};
		EXPECT_EQ(scores::FactorBeyond(world, c.nearest, c.from), 0.0f) << index;
	}
}

TEST(ComputerMindScores, ARockIsThrownAtACreatureFarFromTheMindsOwn)
{
	const auto prepare = [](int32_t x, int32_t z) {
		FakeScoreWorld world;
		world.hand = MapCoords {0x100000, 0x100000, 0.0f};
		world.insideAt = {{0x108000, 0x108000}};
		LayOutThings(world, {{0x108000, 0x108000, Rock(1.0f)}});
		world.creatures.insert(k_Rival);
		world.places[k_Rival] = MapCoords {x, z, 0.0f};
		world.citadel = ScoreWorld::Citadel {.place = MapCoords {0x300000, 0x300000, 0.0f}, .influence = 50.0f};
		world.powers[world.me] = 100.0f;
		world.maxDistance = 100.0f;
		world.speed = 90.0f;
		return world;
	};
	const auto node = NodeScoredBy(ScoreKind::ThrowrockatCreature);
	struct Case
	{
		int32_t x, z;
		uint32_t worth;
	};
	// the oracle's cases: nowhere inside the influence, the citadel's reaching 50 m, the virtual one 120 m
	const std::vector<Case> cases {
	    {0x400000, 0x380000, 0x3F48C2BBu}, {0x320000, 0x340000, 0x00000000u}, {0x900000, 0x900000, 0x00000000u},
	    {0x300000, 0x2A0000, 0x3C569026u}, {0x3A0000, 0x300000, 0x3F7CF433u},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		auto world = prepare(c.x, c.z);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Rival);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::ThrowrockatCreature, mind, node, world)), c.worth) << index;
		// the rock is chosen whatever it is worth
		EXPECT_EQ(mind.nodes.at(node).thing, entt::entity {200}) << index;
	}
	{
		// the mind's own creature 60 m off, the two of size 15: far enough
		auto world = prepare(0x400000, 0x380000);
		world.myCreature = k_Creature;
		world.places[k_Creature] = MapCoords {0x400000, 0x380000 + 0x60000, 0.0f};
		auto mind = MakeMind();
		GiveParent(mind, node, k_Rival);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::ThrowrockatCreature, mind, node, world)), 0x3F48C2BBu);
		// nearer, too near to throw
		world.places[k_Creature].z = 0x380000 + 0x5F000;
		auto near = MakeMind();
		GiveParent(near, node, k_Rival);
		EXPECT_EQ(ScoreOf(ScoreKind::ThrowrockatCreature, near, node, world), 0.0f);
		EXPECT_EQ(near.nodes.at(node).thing, k_None);
	}
	{
		// no place in the influence at all: nothing
		auto world = prepare(0x400000, 0x380000);
		world.citadel.reset();
		auto mind = MakeMind();
		GiveParent(mind, node, k_Rival);
		EXPECT_EQ(ScoreOf(ScoreKind::ThrowrockatCreature, mind, node, world), 0.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_None);
	}
	EXPECT_TRUE(IsScorePorted(ScoreKind::ThrowrockatCreature));
}

TEST(ComputerMindScores, AThingIsThrownToImpressATownWhereItsVillagersWillSeeIt)
{
	// the oracle's game: nowhere inside the influence but the hand's cell, a citadel at (0x300000, 0x300000) reaching
	// 50 m, the virtual influence 120 m, the speed 90; the town's store door at (0x400000, 0x380000)
	const auto prepare = [](uint32_t villagers, float boredom) {
		FakeScoreWorld world;
		world.hand = MapCoords {0x100000, 0x100000, 0.0f};
		world.insideAt = {{0x108000, 0x108000}};
		const ScoreWorld::ThrowFacts plain {.throwable = true, .impressiveValue = 2.0f};
		LayOutThings(world, {{0x108000, 0x108000, plain}});
		world.citadel = ScoreWorld::Citadel {.place = MapCoords {0x300000, 0x300000, 0.0f}, .influence = 50.0f};
		world.powers[world.me] = 100.0f;
		world.maxDistance = 100.0f;
		world.speed = 90.0f;
		LayOutTownPlaces(world, {PlaceAt {0x400000, 0x380000, villagers}, {0x480000, 0x480000, 0}, std::nullopt, {}});
		world.boredoms[{k_Town, 9}] = boredom;
		return world;
	};
	const auto node = NodeScoredBy(ScoreKind::ThrowThingsToImpress);
	struct Case
	{
		uint32_t villagers;
		float boredom;
		uint32_t worth;
		uint32_t asked;
	};
	const std::vector<Case> cases {
	    // unlikely to be seen: the town is asked twice
	    {1, 0.8f, 0x3CE42671u, 2},
	    {40, 0.8f, 0x3F209BC9u, 1},
	    {40, 0.25f, 0x3E48C2BBu, 1},
	};
	for (size_t index = 0; index < cases.size(); ++index)
	{
		const auto& c = cases[index];
		auto world = prepare(c.villagers, c.boredom);
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::ThrowThingsToImpress, mind, node, world)), c.worth) << index;
		EXPECT_EQ(world.congregationAsked, c.asked) << index;
		EXPECT_EQ(mind.nodes.at(node).thing, entt::entity {200}) << index;
		EXPECT_EQ(mind.nodes.at(node).point, world.WorldPoint(MapCoords {0x400000, 0x380000, 0.0f})) << index;
	}
	{
		// nothing above, or a sleepy town: nothing
		auto world = prepare(40, 0.8f);
		auto mind = MakeMind();
		EXPECT_EQ(ScoreOf(ScoreKind::ThrowThingsToImpress, mind, node, world), 0.0f);
		world.rawDesires[{k_Town, 16}] = 0.75f;
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::ThrowThingsToImpress, mind, node, world), 0.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_None);
		EXPECT_EQ(world.congregationAsked, 0u);
	}
	{
		// the villagers at most a hundredth likely to see it: nothing, and nothing looked for
		auto world = prepare(0, 0.8f);
		LayOutTownPlaces(world,
		                 {PlaceAt {0x140000, 0x100000, 0}, {0x180000, 0x100000, 0}, std::nullopt, {{0x8A0000, 0x7A0000, 1}}});
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::ThrowThingsToImpress, mind, node, world), 0.0f);
		EXPECT_EQ(mind.nodes.at(node).thing, k_None);
		EXPECT_TRUE(world.cellsAsked.empty());
	}
	{
		// the most impressive thing a hand may throw that breaks no buildings, a town artefact ready to glow counting more
		auto world = prepare(40, 0.8f);
		world.cellObjects.clear();
		LayOutThings(world, {{0x108000, 0x108000, {.throwable = true, .breaksBuildings = true, .impressiveValue = 9.0f}},
		                     {0x108000, 0x108000, {.throwable = true, .impressiveValue = 0.5f}},
		                     {0x108000, 0x108000, {.throwable = true, .impressiveValue = 2.0f}},
		                     {0x108000, 0x108000, {.impressiveValue = 9.0f}}});
		auto mind = MakeMind();
		GiveParent(mind, node, k_Town);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::ThrowThingsToImpress, mind, node, world)), 0x3F209BC9u);
		EXPECT_EQ(mind.nodes.at(node).thing, entt::entity {202});
		world.throwFacts[entt::entity {201}].artefactModifier = 3.0f;
		auto glowing = MakeMind();
		GiveParent(glowing, node, k_Town);
		EXPECT_EQ(BitsOf(ScoreOf(ScoreKind::ThrowThingsToImpress, glowing, node, world)), 0x3F209BC9u);
		EXPECT_EQ(glowing.nodes.at(node).thing, entt::entity {201});
		// nothing to throw: nothing
		world.cellObjects.clear();
		auto none = MakeMind();
		GiveParent(none, node, k_Town);
		EXPECT_EQ(ScoreOf(ScoreKind::ThrowThingsToImpress, none, node, world), 0.0f);
		EXPECT_EQ(none.nodes.at(node).thing, k_None);
	}
	EXPECT_TRUE(IsScorePorted(ScoreKind::ThrowThingsToImpress));
}
