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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Creature/CreatureBodyActivity.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "ECS/TownAggression.h"
#include "Enums.h"

/// How a computer player's mind scores the nodes of its tree: one rule for each way of scoring, each over what it reads
/// of the game. The game's answers come from the computer player system; tests give their own.
namespace openblack::computer_player
{

/// What the score rules read of the game, always for the mind's own player
class ScoreWorld
{
public:
	virtual ~ScoreWorld() = default;

	// ---- the game and its players

	/// The game turn
	[[nodiscard]] virtual uint32_t Turn() const = 0;
	/// The mind's own player
	[[nodiscard]] virtual PlayerNames Me() const = 0;
	/// The first player counts the second as an ally (a script set a value above 0 between them)
	[[nodiscard]] virtual bool IsAllied(PlayerNames player, PlayerNames other) const = 0;
	/// The player's type: 0 none, 1 a person, 2 the computer, 3 the neutral player
	[[nodiscard]] virtual uint32_t PlayerType(PlayerNames player) const = 0;
	/// The player as a thing a node can hold
	[[nodiscard]] virtual entt::entity PlayerThing(PlayerNames player) const = 0;
	/// The player a thing is, when the thing is a player
	[[nodiscard]] virtual std::optional<PlayerNames> PlayerOf(entt::entity thing) const = 0;
	/// The player's influence power
	[[nodiscard]] virtual float InfluencePower(PlayerNames player) const = 0;
	/// The citadel's reach of the virtual influence, before the player's power adds to it
	[[nodiscard]] virtual float VirtualInfluenceMaxDistance() const = 0;
	/// The mind's player has influence at the place (counting its allies')
	[[nodiscard]] virtual bool InMyInfluence(const map_coords::MapCoords& place) const = 0;
	/// The player whose influence is the strongest at the place, if any
	[[nodiscard]] virtual std::optional<PlayerNames> MostInfluentialPlayerAt(const map_coords::MapCoords& place) const = 0;
	/// The player's own place: their citadel's heart; without one, their computer hand while their computer player is
	/// on, else their hand
	[[nodiscard]] virtual map_coords::MapCoords PlayerFixedPlace(PlayerNames /*player*/) const { return {}; }
	/// The running average of the player's influence power
	[[nodiscard]] virtual float AverageInfluencePower(PlayerNames /*player*/) const { return 0.0f; }
	/// The harm the effects of `by` did the things of `player`
	[[nodiscard]] virtual float HarmFrom(PlayerNames /*player*/, PlayerNames /*by*/) const { return 0.0f; }
	/// The player's alignment, from -1 evil to 1 good
	[[nodiscard]] virtual float AlignmentValue(PlayerNames /*player*/) const { return 0.0f; }

	// ---- things

	[[nodiscard]] virtual bool IsTown(entt::entity thing) const = 0;
	[[nodiscard]] virtual bool IsCreature(entt::entity thing) const = 0;
	/// A building of a town: a house, a store, a workshop or any other abode
	[[nodiscard]] virtual bool IsAbode(entt::entity thing) const = 0;
	/// Anything on the land the game counts as an object: a living thing, a building, a tree, a rock...
	[[nodiscard]] virtual bool IsObject(entt::entity thing) const = 0;
	/// The thing has a place on the land
	[[nodiscard]] virtual bool HasPlace(entt::entity thing) const = 0;
	/// Where the thing is
	[[nodiscard]] virtual map_coords::MapCoords Place(entt::entity thing) const = 0;
	/// The thing's life, from 0 to 1
	[[nodiscard]] virtual float Life(entt::entity thing) const = 0;
	/// The town the thing belongs to: a town is its own; none for a thing of no town
	[[nodiscard]] virtual entt::entity TownOf(entt::entity thing) const = 0;
	/// The thing is controlled by a script
	[[nodiscard]] virtual bool ScriptControlled(entt::entity thing) const = 0;

	// ---- creatures

	/// The mind's player's creature, also while a script controls it; none without one
	[[nodiscard]] virtual entt::entity MyCreature() const = 0;
	/// Every creature on the land, in the game's list of creatures: the newest first
	[[nodiscard]] virtual std::vector<entt::entity> Creatures() const = 0;
	/// The player a creature belongs to, if any
	[[nodiscard]] virtual std::optional<PlayerNames> OwnerOf(entt::entity creature) const = 0;
	/// The creature's scale, as its size was set
	[[nodiscard]] virtual float CreatureScale(entt::entity creature) const = 0;
	[[nodiscard]] virtual float CreatureEnergy(entt::entity creature) const = 0;
	[[nodiscard]] virtual float CreatureExhaustion(entt::entity creature) const = 0;
	[[nodiscard]] virtual bool IsFighting(entt::entity creature) const = 0;
	/// What moves the creature's body this turn
	[[nodiscard]] virtual creature_body::BodyActivity CreatureBodyActivity(entt::entity creature) const = 0;
	/// The desire the creature's mind keeps as the one its plan meets (its number); none when it keeps none
	[[nodiscard]] virtual std::optional<uint32_t> PlanDesire(entt::entity creature) const = 0;

	/// A leash that the mind's player has on
	struct Leash
	{
		/// What it is tied to, none when it is not tied
		entt::entity tiedTo {entt::null};
		/// The game turn it was last tied (0 if it never was)
		uint32_t tiedTurn {0};
	};
	/// The first leash of the mind's player that is on, if any
	[[nodiscard]] virtual std::optional<Leash> LeashOn() const = 0;
	/// What the leash of the mind's player's first hand is tied to, on or not; none when it is tied to nothing
	[[nodiscard]] virtual entt::entity LeaderLeashTiedTo() const = 0;

	// ---- towns

	/// The player's towns, in the order they became the player's
	[[nodiscard]] virtual std::vector<entt::entity> TownsOf(PlayerNames player) const = 0;
	/// The town's store of food and wood, none without one
	[[nodiscard]] virtual entt::entity StoragePit(entt::entity town) const = 0;
	/// The town's worship site, none without one
	[[nodiscard]] virtual entt::entity WorshipSiteOf(entt::entity town) const = 0;
	/// The radius of the town's influence
	[[nodiscard]] virtual float TownInfluenceRadius(entt::entity town) const = 0;
	/// The corners of the town's area
	struct Area
	{
		map_coords::MapCoords min;
		map_coords::MapCoords max;
	};
	[[nodiscard]] virtual Area TownArea(entt::entity town) const = 0;
	/// The centre of the town's area
	[[nodiscard]] virtual map_coords::MapCoords TownAreaCentre(entt::entity town) const = 0;
	/// The town's buildings, the newest first
	[[nodiscard]] virtual std::vector<entt::entity> TownAbodes(entt::entity town) const = 0;
	/// The player who owns the town
	[[nodiscard]] virtual PlayerNames TownOwner(entt::entity town) const = 0;
	/// What the town holds against a player for the harm the player did it, and the game turn of the last harm
	struct Aggression
	{
		float aggression {0.0f};
		uint32_t lastTurn {0};
	};
	[[nodiscard]] virtual Aggression TownAggression(entt::entity town, PlayerNames attacker) const = 0;
	/// What the player's last attack on the town did: its burn, crush and hit; nothing before any
	[[nodiscard]] virtual ecs::town_aggression::AttackEffect TownLastAttackEffect(entt::entity /*town*/,
	                                                                              PlayerNames /*attacker*/) const
	{
		return {};
	}
	/// How strongly a rival not allied to the mind's player has lately been believed in by the town, against the mind's
	/// player: 0 when no rival counts
	[[nodiscard]] virtual float RivalRecentRatio(entt::entity town) const = 0;
	/// The town's belief in the mind's player less the most it believes in any other player
	[[nodiscard]] virtual float BeliefNeededOrLeft(entt::entity town) const = 0;
	/// The most the town believes in an active rival not allied to the mind's player, over its belief in the mind's
	/// player (0 when it believes nothing in the mind's player)
	[[nodiscard]] virtual float RivalBeliefRatio(entt::entity town) const = 0;
	/// The door of a building, where a villager comes out of it: its door point on the land, else its place
	[[nodiscard]] virtual map_coords::MapCoords DoorPlace(entt::entity /*building*/) const { return {}; }
	/// Where the town's villagers gather: worked out the first time it is asked (with two draws from the synced generator
	/// when no clear place is found) and kept by the town from then on
	virtual map_coords::MapCoords CongregationPlace(entt::entity /*town*/) { return {}; }
	/// The town's centre, none without one
	[[nodiscard]] virtual entt::entity TownCentre(entt::entity /*town*/) const { return entt::null; }
	/// How bored the town is with a kind of reaction (by its number): 1 while the reaction is fresh to it
	[[nodiscard]] virtual float TownBoredom(entt::entity /*town*/, uint32_t /*reaction*/) const { return 0.0f; }
	/// How far a building is built, from 0 when it is begun to 1
	[[nodiscard]] virtual float AbodePercentBuilt(entt::entity /*abode*/) const { return 0.0f; }
	/// How much a computer player wants to attack a building, as its building's info gives it, and a field, as its kind
	/// of field gives it
	[[nodiscard]] virtual float AbodeAttackDesire(entt::entity /*abode*/) const { return 0.0f; }
	[[nodiscard]] virtual float FieldAttackDesire(entt::entity /*field*/) const { return 0.0f; }

	// ---- buildings and fire

	/// The building has a fire on it
	[[nodiscard]] virtual bool OnFire(entt::entity abode) const = 0;
	/// The building's fire is hot enough to spread and burn
	[[nodiscard]] virtual bool FireAboveReactionTemperature(entt::entity abode) const = 0;

	// ---- the citadel

	/// The mind's player's citadel: its place and the radius of its influence
	struct Citadel
	{
		map_coords::MapCoords place;
		float influence {0.0f};
	};
	[[nodiscard]] virtual std::optional<Citadel> MyCitadel() const = 0;
	/// The mind's player's citadel heart, none without a citadel
	[[nodiscard]] virtual entt::entity MyCitadelHeart() const = 0;
	/// The game turn a thing that breaks buildings last struck the heart itself, 0 if none ever did
	[[nodiscard]] virtual uint32_t HeartBreakingBlowTurn(entt::entity heart) const = 0;

	// ---- miracles

	/// The mind's player may cast the miracle
	[[nodiscard]] virtual bool IsMagicTypeEnabled(MagicType type) const = 0;
	/// The desire a miracle makes a creature watching it feel (its number)
	[[nodiscard]] virtual uint32_t PerceivedDesire(MagicType type) const = 0;
	/// How much the mind counts a miracle as impressive, as its info gives it for a computer player
	[[nodiscard]] virtual float MagicImpressiveBalance(MagicType /*type*/) const { return 0.0f; }
	/// How much the mind counts a miracle as an attack, as its info gives it
	[[nodiscard]] virtual float MagicAggressiveAttackValue(MagicType /*type*/) const { return 0.0f; }
	/// The icon of the miracle on the first of the citadel's worship sites that has one; none without a citadel or icon
	[[nodiscard]] virtual entt::entity CitadelSpellIcon(MagicType type) const = 0;
	/// The worship site an icon stands at, none
	[[nodiscard]] virtual entt::entity IconSite(entt::entity icon) const = 0;
	/// The prayer an icon has stored
	[[nodiscard]] virtual float IconChantStore(entt::entity icon) const = 0;
	/// The prayer an icon's miracle needs to be made
	[[nodiscard]] virtual float IconChantsToCreate(entt::entity icon) const = 0;
	/// The prayer a worship site has to give, and the prayer of its battery
	[[nodiscard]] virtual float SiteChantsAvailable(entt::entity site) const = 0;
	[[nodiscard]] virtual float SiteBattery(entt::entity site) const = 0;
	/// All the prayer the worship site holds
	[[nodiscard]] virtual float SiteTotalChantsAvailable(entt::entity site) const = 0;
	/// The prayer the site's worshippers make each turn
	[[nodiscard]] virtual float SiteChantsVillagersCanSupply(entt::entity site) const = 0;
	[[nodiscard]] virtual bool SiteBuilt(entt::entity site) const = 0;
	[[nodiscard]] virtual bool SiteRepaired(entt::entity site) const = 0;
	/// The villagers worshipping at the site
	[[nodiscard]] virtual uint32_t SiteWorshippers(entt::entity site) const = 0;

	/// A miracle the mind cast: which, and how far it reaches (where, the mind keeps itself)
	struct CastSpell
	{
		MagicType type {MagicType::None};
		float radius {0.0f};
	};
	[[nodiscard]] virtual CastSpell SpellCast(entt::entity spell) const = 0;

	// ---- the mind's hand and the creature's needs

	/// Where the mind's hand is on the map
	[[nodiscard]] virtual map_coords::MapCoords HandPlace() const = 0;
	/// How strongly the creature feels a desire (by its number), 0 while its mind has none
	[[nodiscard]] virtual float CreatureDesire(entt::entity creature, uint32_t desire) const = 0;
	/// The energy below which the creature's kind starves
	[[nodiscard]] virtual float CreatureLowEnergyThreshold(entt::entity creature) const = 0;
	/// The ordinary actions the creature could be taught that it does not know yet
	[[nodiscard]] virtual uint32_t UnknownNormalActions(entt::entity creature) const = 0;
	/// Some miracle is working on the creature
	[[nodiscard]] virtual bool AnySpellOnCreature(entt::entity creature) const = 0;

	// ---- food and stores

	/// The town's fields and fish farms, the newest first
	[[nodiscard]] virtual std::vector<entt::entity> TownFields(entt::entity town) const = 0;
	[[nodiscard]] virtual std::vector<entt::entity> TownFishFarms(entt::entity town) const = 0;
	/// The food a field has grown, and the most it can hold
	[[nodiscard]] virtual float FieldFood(entt::entity field) const = 0;
	[[nodiscard]] virtual float FieldFoodCapacity(entt::entity field) const = 0;
	/// Where the town takes a resource (0 food, 1 wood): its store, else its temporary pot, which this makes when the town
	/// has none, as the original does
	[[nodiscard]] virtual entt::entity ResourceDropObject(entt::entity town, uint32_t resource) = 0;
	/// How much of a resource (0 food, 1 wood) a building holds: a store's, a workshop's
	[[nodiscard]] virtual uint32_t AbodeResource(entt::entity abode, uint32_t resource) const = 0;
	/// A pile of a resource at a store (food: pile 0; wood: piles 0 to 4), none when it is not there to take from
	[[nodiscard]] virtual entt::entity StorePile(entt::entity pit, uint32_t resource, uint32_t index) const = 0;
	/// How much a pile holds
	[[nodiscard]] virtual uint32_t PileAmount(entt::entity pile) const = 0;
	/// How much the town wants something (a town desire, by its number), before its villagers' share
	[[nodiscard]] virtual float RawDesire(entt::entity town, uint32_t desire) const = 0;

	// ---- villagers

	/// What decides whether a villager is free for the mind's work
	struct VillagerFacts
	{
		/// Not a script's, alive and in a state that leaves it free for other work
		bool available {false};
		bool child {false};
		/// Not at home, not in a hand, and in a state the land can reach
		bool reachable {false};
		/// The state it is heading for serves no desire of its town
		bool idle {false};
		bool atWorshipSite {false};
		bool disciple {false};
		/// It builds at a building site
		bool building {false};
		bool sexuallyActive {false};
		/// Its sex (0 or 1) and its kind of disciple
		uint8_t sex {0};
		uint8_t discipleType {0};
		/// It may go to worship at a site (not counting a site it is at)
		bool availableForWorship {false};
	};
	[[nodiscard]] virtual VillagerFacts VillagerFactsOf(entt::entity villager) const = 0;
	/// The villagers in the map cell of the place, in the order the cell lists them
	[[nodiscard]] virtual std::vector<entt::entity> VillagersAt(const map_coords::MapCoords& place) const = 0;

	// ---- the towns' wants and people

	/// How much the town wants something (a town desire, by its number), with its villagers' share
	[[nodiscard]] virtual float TownDesire(entt::entity town, uint32_t desire) const = 0;
	/// The food the town wants for its dinner, and the food it has (carried and stored)
	[[nodiscard]] virtual float DesiredFood(entt::entity town) const = 0;
	[[nodiscard]] virtual uint32_t FoodAvailable(entt::entity town) const = 0;
	/// The town's disciples of a kind (by its number), its adults and children, and the most villagers its abodes hold
	[[nodiscard]] virtual uint32_t TownDisciples(entt::entity town, uint32_t kind) const = 0;
	[[nodiscard]] virtual uint32_t TownAdults(entt::entity town) const = 0;
	[[nodiscard]] virtual uint32_t TownChildren(entt::entity town) const = 0;
	[[nodiscard]] virtual int32_t TownAdultPlaces(entt::entity town) const = 0;
	/// How much the town worships at its village totem, when the totem works; 0 without one
	[[nodiscard]] virtual float TotemWorship(entt::entity town) const = 0;
	/// What the town's will to send worshippers reads
	struct TownWorship
	{
		bool emergency {false};
		/// The town's centre works (the original asks its centre, not its worship site)
		bool siteFunctional {false};
		/// Its villagers worshipping, and those on their way to worship
		uint32_t worshipping {0};
		uint32_t onTheWay {0};
	};
	[[nodiscard]] virtual TownWorship TownWorshipFacts(entt::entity town) const = 0;
	/// The villagers living in a building, in the order it lists them; the town's homeless villagers
	[[nodiscard]] virtual std::vector<entt::entity> AbodeInhabitants(entt::entity abode) const = 0;
	[[nodiscard]] virtual std::vector<entt::entity> TownHomeless(entt::entity town) const = 0;

	// ---- building sites, scaffolds and workshops

	[[nodiscard]] virtual bool IsBuildingSite(entt::entity thing) const = 0;
	/// The town's building sites, in the order it lists them
	[[nodiscard]] virtual std::vector<entt::entity> TownBuildingSites(entt::entity town) const = 0;
	/// What the score rules read of a building site
	struct BuildingSiteFacts
	{
		/// The site still stands
		bool available {false};
		/// The building it builds, none
		entt::entity building {entt::null};
		float woodNeeded {0.0f};
		/// The building it belongs to is built (so the site repairs it)
		bool rootBuilt {false};
		float desireToBeRepaired {0.0f};
		/// The wood it holds
		uint32_t wood {0};
		/// The most builders it takes, and those it still needs
		int32_t maxBuilders {0};
		int32_t buildersNeeded {0};
		float desireForVillagers {0.0f};
	};
	[[nodiscard]] virtual BuildingSiteFacts BuildingSiteFactsOf(entt::entity site) const = 0;
	[[nodiscard]] virtual bool IsWorshipSite(entt::entity thing) const = 0;
	/// The towns a worship site serves, in the order it lists them
	[[nodiscard]] virtual std::vector<entt::entity> SiteTowns(entt::entity site) const = 0;
	[[nodiscard]] virtual bool IsScaffold(entt::entity thing) const = 0;
	[[nodiscard]] virtual bool IsWorkshop(entt::entity abode) const = 0;
	/// The scaffolds a workshop keeps, in its order (an entry may hold none)
	[[nodiscard]] virtual std::vector<entt::entity> WorkshopScaffolds(entt::entity workshop) const = 0;
	/// What the score rules read of a scaffold
	struct ScaffoldFacts
	{
		/// It already belongs to a building site
		bool inSite {false};
		/// A hand holds it, or it flies
		bool busy {false};
		/// The scaffolds it counts for, from its size
		int32_t value {0};
	};
	[[nodiscard]] virtual ScaffoldFacts ScaffoldFactsOf(entt::entity scaffold) const = 0;
	/// The building works (it is built and not destroyed)
	[[nodiscard]] virtual bool AbodeFunctional(entt::entity abode) const = 0;
	/// The room a workshop has for more scaffolds, and the most it makes
	[[nodiscard]] virtual int32_t WorkshopSpace(entt::entity workshop) const = 0;
	[[nodiscard]] virtual float WorkshopMostObjects(entt::entity workshop) const = 0;
	/// The wood a scaffold is made of
	[[nodiscard]] virtual uint32_t ScaffoldWoodValue() const = 0;
	/// The town finds a building a scaffold of the value could be used for at the place, as its planning does (one draw
	/// from the synced generator, into a trial plan that leaves nothing in the world)
	virtual bool TryToBuildAt(entt::entity town, const map_coords::MapCoords& place, int32_t scaffoldValue) = 0;
	/// The map's size in cells
	struct Cells
	{
		int32_t x {0};
		int32_t z {0};
	};
	[[nodiscard]] virtual Cells MapCells() const = 0;
	/// The things in a map cell on the land, in the order the game walks them: its fixed things, then its moving ones
	[[nodiscard]] virtual std::vector<entt::entity> CellObjects(uint16_t /*x*/, uint16_t /*z*/) const { return {}; }
	/// What the mind's hand weighs a thing by when it looks for one to throw
	struct ThrowFacts
	{
		/// A player's hand may pick it up and throw it
		bool throwable {false};
		/// It breaks the buildings it hits
		bool breaksBuildings {false};
		/// The game counts it as a tree
		bool tree {false};
		/// Its weight, as the hand weighs it
		float weight {0.0f};
		/// How impressive it is, and what a town artefact ready to glow adds to that (1 for anything else)
		float impressiveValue {0.0f};
		float artefactModifier {1.0f};
	};
	[[nodiscard]] virtual ThrowFacts ThrowFactsOf(entt::entity /*thing*/) const { return {}; }
	/// The mind's player's speed, in metres a second, which its hand takes
	[[nodiscard]] virtual float PlayerSpeed() const { return 0.0f; }
	// ---- forests and trees

	/// How many trees of the forest (by its number) have grown
	[[nodiscard]] virtual uint32_t ForestGrownTrees(uint32_t /*forest*/) const { return 0; }
	/// The tree the forest is measured by: its first growing or its first grown tree, whichever is nearer the forest (the
	/// grown one on a tie); none without trees
	[[nodiscard]] virtual entt::entity ForestCentreTree(uint32_t /*forest*/) const { return entt::null; }
	/// The thing is a tree, a magic one too
	[[nodiscard]] virtual bool IsTree(entt::entity /*thing*/) const { return false; }
	/// The wood a tree gives: its life, its kind's wood, its size and the land's balance
	[[nodiscard]] virtual float TreeWoodValue(entt::entity /*tree*/) const { return 0.0f; }
	/// The wood of the tree's kind, as its row gives it
	[[nodiscard]] virtual uint32_t TreeKindWood(entt::entity /*tree*/) const { return 0; }
	/// How far the thing reaches round its place on the land, in metres: 0 without a body
	[[nodiscard]] virtual float ThingRadius(entt::entity /*thing*/) const { return 0.0f; }
	/// Where the mind's player's own hand is on the map
	[[nodiscard]] virtual map_coords::MapCoords LeaderHandPlace() const { return {}; }

	/// The altitudes of a map cell's four corners: (x, z), (x, z + 1), (x + 1, z) and (x + 1, z + 1); none off the land
	[[nodiscard]] virtual std::optional<std::array<uint8_t, 4>> CellCorners(int32_t x, int32_t z) const = 0;
	/// The point on the land of a place, as a node keeps it
	[[nodiscard]] virtual glm::vec3 WorldPoint(const map_coords::MapCoords& place) const = 0;

	// ---- the citadel's worship sites

	/// The citadel's six worship sites by their slots, none in an empty slot or without a citadel
	[[nodiscard]] virtual std::array<entt::entity, 6> CitadelSites() const = 0;
	/// How much a worship site's worshippers want food, and the food it asks of its towns
	[[nodiscard]] virtual float SiteFoodDesire(entt::entity site) const = 0;
	[[nodiscard]] virtual int32_t SiteFoodNeededFromTown(entt::entity site) const = 0;
	/// The icons of a worship site, how many and in its order
	[[nodiscard]] virtual uint32_t SiteIconCount(entt::entity site) const = 0;
	[[nodiscard]] virtual std::vector<entt::entity> SiteIcons(entt::entity site) const = 0;
	/// The icon is charging; the miracle it makes
	[[nodiscard]] virtual bool IconCharging(entt::entity icon) const = 0;
	[[nodiscard]] virtual MagicType IconMagicType(entt::entity icon) const = 0;
	/// The site is built by the amount, as the game builds it
	virtual void BuildSite(entt::entity site, float amount) = 0;
	/// The site's gate, (0, 0, 0) without one
	[[nodiscard]] virtual map_coords::MapCoords SiteGatePlace(entt::entity site) const = 0;

	// ---- the random generator the game keeps in step

	/// A float from 0 to just below `most`, from the game's synced generator
	virtual float GameFloatRand(float most) = 0;
	/// A number from 0 to `range` - 1 from the game's synced generator
	virtual uint32_t GameRand(uint32_t range) = 0;

	// ---- the rivals

	/// A player's creature, also while a script controls it; none without one
	[[nodiscard]] virtual entt::entity CreatureOf(PlayerNames player) const = 0;
	/// The miracles of a town's spell icons, one for each icon
	[[nodiscard]] virtual std::vector<MagicType> TownSpellIconTypes(entt::entity town) const = 0;
	/// The belief the mind's player still needs to win the town: for its own town, the belief of the rival whose margin
	/// over the player's is strictly the largest; for another's, the town's belief in its owner
	[[nodiscard]] virtual float BeliefNeededToConvert(entt::entity town) const = 0;
};

/// The way of scoring has its rule in openblack. The rest score nothing, which the original scores only when it finds
/// nothing to do: the wiki lists what each of them still needs
[[nodiscard]] bool IsScorePorted(ScoreKind kind);

/// What the node is worth by the way of scoring, before its weight; it may choose the node's thing and point, and it
/// keeps what the original keeps in the mind as it scores
float Score(ScoreKind kind, const MindTree& tree, ecs::components::ComputerMind& mind, uint16_t node, ScoreWorld& world);

// ---- the rules several ways of scoring share ------------------------------------------------------------------------

/// The mind's player's creature, none while a script controls it
[[nodiscard]] entt::entity PlayersCreature(const ScoreWorld& world);

/// How far the mind's player's virtual influence reaches: its influence power times 0.2, plus the citadel's reach
[[nodiscard]] float VirtualInfluenceReach(const ScoreWorld& world);

/// The nearest place inside the mind's player's influence: the place itself when it is inside; else the edge of the
/// citadel's influence towards it, unless a town's influence edge is nearer. None without a citadel or towns
[[nodiscard]] std::optional<map_coords::MapCoords> NearestPlaceInInfluence(const ScoreWorld& world,
                                                                           const map_coords::MapCoords& place);

/// How well the mind's player can act at a place: 1 inside its influence; else, from the nearest place inside, the
/// square of the distance's fall-off over the virtual influence's reach, 0 below 0.01
[[nodiscard]] float InfluenceFactor(const ScoreWorld& world, const map_coords::MapCoords& place);

/// How keen the icon is to be charged: its stored prayer and what the site can give, over what the miracle needs;
/// 0 when that is short and no worshipper prays at the site
[[nodiscard]] float ChargeUpSpeedDesire(const ScoreWorld& world, entt::entity icon);

/// The turns the icon still needs to be made at the prayer of its worshippers: 0 when it has enough, -1 without a site
/// or without worshippers
[[nodiscard]] float TurnsToCreate(const ScoreWorld& world, entt::entity icon);

/// How keen the mind is on a miracle's icon, which it then gives: none without a citadel or icon; an icon that is not
/// keen enough to be charged at once counts only at a built, repaired site with worshippers. Each icon found is counted
/// in the mind, the count the miracle charge reads
float IconWant(const ScoreWorld& world, ecs::components::ComputerMind& mind, MagicType type, entt::entity& icon);

/// What casting the miracle at a place (or anywhere, without one) is worth, up to 1: the icon's keenness, its turns
/// to be made and the influence at the place. The icon goes into `icon` only once it is found and can be made
float SpellWorth(const ScoreWorld& world, ecs::components::ComputerMind& mind, MagicType type,
                 const std::optional<map_coords::MapCoords>& place, entt::entity& icon);

/// The mind cast this miracle near the place already (closer than an eighth of a map cell on each side), and it reaches
/// at least `minRadius` (any reach when that is 0)
[[nodiscard]] bool CastNear(const ScoreWorld& world, const ecs::components::ComputerMind& mind,
                            const map_coords::MapCoords& place, MagicType type, float minRadius);

/// How free the mind's creature is for a job: 1 off its leash; on it, 0 until 450 turns after it was tied, then rising
/// with the time, its life, energy and rest, up to 1; 0 while it fights or its plan is to heal
[[nodiscard]] float CreatureFreeTime(const ScoreWorld& world);

/// How good the mind's creature is for a job at a thing: its free time, its rest and the distance's fall-off; 0 when
/// its leash is tied to that thing, or (with `sameTown`) to a thing of that thing's town
[[nodiscard]] float CreatureJobWorth(const ScoreWorld& world, entt::entity target, bool sameTown);

/// The radius of a town's area: half its larger side, in whole metres
[[nodiscard]] float TownRadius(const ScoreWorld::Area& area);

/// The place with its cells kept on the map, the fraction of each cell kept
[[nodiscard]] map_coords::MapCoords ClampCellsOntoMap(const map_coords::MapCoords& place, const ScoreWorld::Cells& cells);

/// A cell whose corners differ by more than 10 is too steep to build on; a cell off the land is not
[[nodiscard]] bool CellIsSteep(const std::optional<std::array<uint8_t, 4>>& corners);

/// How much a built worship site wants more worshippers: 1 - (worshippers / (4 x icons))^4, its share up to 1; 0
/// without icons
[[nodiscard]] float DesireForWorshippers(const ScoreWorld& world, entt::entity site);

/// How willing a town is to send worshippers: from the share of its people worshipping or on their way, (1 - share)^3,
/// 1 from 0.7, 0 below 0.2; 0 with nobody, in an emergency or without a worship site that works
[[nodiscard]] float DesireToGiveWorshippers(const ScoreWorld& world, entt::entity town);

/// How much a worship site wants a villager from its towns: its want of worshippers times its most willing town's will
[[nodiscard]] float DesireForVillagerAvailableFromTown(const ScoreWorld& world, entt::entity site);

/// The next active player after a player (from the first without one), as the game walks them: the first of the seven
/// players but the neutral that has a type; none past them, or when the next one is of the neutral's type
[[nodiscard]] std::optional<PlayerNames> NextActivePlayer(const ScoreWorld& world, std::optional<PlayerNames> after);

/// How willing a town is to be taken over by the mind's player: 1 less a fifth of the belief still needed (up to 1),
/// 0.75 more for its own town, from 0.1 up to 1
[[nodiscard]] float DesireToBeTakenOver(const ScoreWorld& world, entt::entity town);

/// The rival's town most worth taking: by its people against the mind's player's, the miracles its icons make that the
/// player lacks, its will to be taken over, the rival's influence and its distance from the hand; up to 1. The town is
/// given only when one is worth more than 0
float BestTownToTake(const ScoreWorld& world, const ecs::components::ComputerMind& mind, PlayerNames victim,
                     entt::entity& town);

} // namespace openblack::computer_player
