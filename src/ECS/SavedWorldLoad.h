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
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <AnimalRecords.h>
#include <BuildingRecords.h>
#include <CreatureRecords.h>
#include <EffectRecords.h>
#include <ForestRecords.h>
#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>
#include <WorshipRecords.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureFight.h"
#include "Creature/CreatureScriptPlay.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/ScriptMemory.h"
#include "ECS/FireFlies.h"
#include "ECS/SavedWorld/LoadParticles.h"
#include "ECS/SavedWorld/LoadSpells.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Weather/Storms.h"
#include "Enums.h"

namespace openblack::gamesave
{
struct WorldRead;
}

namespace openblack::ecs
{
class Registry;
}

// The world's objects of a saved game made again as openblack's entities, so far the forests and their trees: first a
// pure plan from the world file's records, in the order the game puts its loaded objects' links back, then the
// entities made from it. See docs/wiki/engine/saved-games.md#what-a-load-does.
namespace openblack::ecs::saved_world_load
{

/// The tree type a tree row's number in the game's tables names, none for a number that is not a tree row. The game
/// numbers its rows from the last one of its tables, so the 23 tree rows run from 2115 (the first) down to 2093
[[nodiscard]] std::optional<TreeInfo> TreeTypeOfInfo(uint32_t info);
/// The big forest type its row's number names, the same way: the 4 big forest rows run from 369 (the first) down
[[nodiscard]] std::optional<BigForestInfo> BigForestTypeOfInfo(uint32_t info);
/// The other way: the number of a tree type's row, and of a big forest type's
[[nodiscard]] uint32_t TreeInfoRowOf(TreeInfo type);
[[nodiscard]] uint32_t BigForestInfoRowOf(BigForestInfo type);
/// The pot or pile type, the same way: the pot rows run from 1876 (the first) down
[[nodiscard]] std::optional<PotInfo> PotTypeOfInfo(uint32_t info);
/// The abode type, the same way: the abode rows run from 2394 (the first) down
[[nodiscard]] std::optional<AbodeInfo> AbodeTypeOfInfo(uint32_t info);
/// The villager type, the same way: the villager rows run from 2247 (the first) down
[[nodiscard]] std::optional<VillagerInfo> VillagerTypeOfInfo(uint32_t info);
/// Whether a loaded villager's state counter is drawn again: when neither of its two states loads values of its own
[[nodiscard]] bool VillagerCounterDrawnOnLoad(uint8_t topState, uint8_t finalState);
/// The mobile static type, the same way: the mobile static rows run from 1937 (the first) down, and the row of a type
[[nodiscard]] std::optional<MobileStaticInfo> MobileStaticTypeOfInfo(uint32_t info);
[[nodiscard]] uint32_t MobileStaticInfoRowOf(MobileStaticInfo type);
/// The animal type, the same way: the 31 animal rows run from 2548 (the first, the lion's) down
[[nodiscard]] std::optional<AnimalInfo> AnimalTypeOfInfo(uint32_t info);
/// The creature species, the same way: the 17 creature rows run from 2517 (the first, the Giant Ape's) down, each
/// species at its row of the game's creature tables (creature::InfoRow)
[[nodiscard]] std::optional<CreatureType> CreatureSpeciesOfInfo(uint32_t info);
/// The other way: the number of a pot or pile type's row, and of an abode type's
[[nodiscard]] uint32_t PotInfoRowOf(PotInfo type);
[[nodiscard]] uint32_t AbodeInfoRowOf(AbodeInfo type);

/// A forest of the save, by its number in the world file
struct PlannedForest
{
	gamesave::ObjectNumber number {0};
	gamesave::ForestRecord record;
};

/// A tree of the save, by its number in the world file, with the type its row names
struct PlannedTree
{
	gamesave::ObjectNumber number {0};
	TreeInfo type {TreeInfo::Beech};
	gamesave::TreeRecord record;
};

/// A big forest of the save, by its number in the world file, with the type its row names
struct PlannedBigForest
{
	gamesave::ObjectNumber number {0};
	BigForestInfo type {BigForestInfo::Info1};
	gamesave::BigForestRecord record;
};

/// A town of the save, by its number in the world file
struct PlannedTown
{
	gamesave::ObjectNumber number {0};
	gamesave::TownRecord record;
};

/// A town's artifact of the save, by its number in the world file, with its record, its owner, whether its town's list
/// holds it and the places it takes in its town's and the game's lists (1 for the oldest)
struct PlannedArtifact
{
	gamesave::ObjectNumber number {0};
	gamesave::TownArtifactRecord record;
	std::optional<PlayerNames> player;
	bool inTown {false};
	uint64_t joined {0};
	uint64_t made {0};
};

/// A footpath of the save, by its number in the world file, with its nodes' records in its list's order
struct PlannedFootpath
{
	gamesave::ObjectNumber number {0};
	gamesave::FootpathRecord record;
	std::vector<gamesave::FootpathNodeRecord> nodes;
};

/// A footpath link of the save, by its number in the world file
struct PlannedFootpathLink
{
	gamesave::ObjectNumber number {0};
	gamesave::FootpathLinkRecord record;
};

/// A bank of mist of the save, by its number in the world file
struct PlannedMist
{
	gamesave::ObjectNumber number {0};
	gamesave::MistRecord record;
};

/// A river of the save, by its number in the world file
struct PlannedStream
{
	gamesave::ObjectNumber number {0};
	gamesave::StreamRecord record;
};

/// A climate of the save, by its number in the world file
/// A thing of the weather of the save, by its number in the world file
struct PlannedWeatherThing
{
	gamesave::ObjectNumber number {0};
	gamesave::WeatherThingRecord record;
};

/// An arena of the save, by its number in the world file
struct PlannedArena
{
	gamesave::ObjectNumber number {0};
	gamesave::ArenaRecord record;
};

/// A fish farm of the save, by its number in the world file
struct PlannedFishFarm
{
	gamesave::ObjectNumber number {0};
	gamesave::FishFarmRecord record;
};

/// An influence ring of the save, by its number in the world file, with its player by its player record's number
struct PlannedInfluenceRing
{
	gamesave::ObjectNumber number {0};
	gamesave::InfluenceRingRecord record;
	std::optional<PlayerNames> player;
};

/// A script's timer of the save, by its number in the world file
struct PlannedScriptTimer
{
	gamesave::ObjectNumber number {0};
	gamesave::ScriptTimerRecord record;
};

/// A creature's drinking waypoint of the save, by its number in the world file
struct PlannedWaypoint
{
	gamesave::ObjectNumber number {0};
	gamesave::WaypointRecord record;
};

/// A script's marker of the save, by its number in the world file
struct PlannedScriptMarker
{
	gamesave::ObjectNumber number {0};
	gamesave::ScriptMarkerRecord record;
};

/// A script's highlight of the save, by its number in the world file
struct PlannedHighlight
{
	gamesave::ObjectNumber number {0};
	gamesave::ScriptHighlightRecord record;
};

/// A reaction of the save, by its number in the world file, with the player its record names (none for its null
/// player)
struct PlannedReaction
{
	gamesave::ObjectNumber number {0};
	gamesave::ReactionRecord record;
	std::optional<PlayerNames> player;
	/// The player whose interface status started it (a hand's pick-up), when one did
	std::optional<PlayerNames> statusPlayer;
};

struct PlannedClimate
{
	gamesave::ObjectNumber number {0};
	gamesave::ClimateRecord record;
};

/// A worship site of the save, by its number in the world file
struct PlannedWorshipSite
{
	gamesave::ObjectNumber number {0};
	gamesave::WorshipSiteRecord record;
	/// Its totem, and its three need signs in their order, by their numbers in the world file
	gamesave::ObjectNumber totemNumber {0};
	std::optional<gamesave::WorshipTotemRecord> totem;
	std::array<std::pair<gamesave::ObjectNumber, std::optional<gamesave::ShowNeedsVisualsRecord>>, 3> signs {};
	/// Its spell icons, in its list's order, by their numbers in the world file
	std::vector<std::pair<gamesave::ObjectNumber, gamesave::WorshipSpellIconRecord>> icons;
	/// The number of its needs' record in the world file, 0 when it names none
	gamesave::ObjectNumber needsNumber {0};
	/// The effect of its mana path, its particles as the save kept them; none when it names none
	std::optional<saved_particles::PlannedEffect> manaPath;
};

/// A dance of the save, by its number in the world file, with its groups' numbers and records in its order
struct PlannedDance
{
	gamesave::ObjectNumber number {0};
	gamesave::DanceRecord record;
	std::vector<std::pair<gamesave::ObjectNumber, gamesave::DanceGroupRecord>> groups;
};

/// A fire of the save, by its number in the world file, with its player
struct PlannedFire
{
	gamesave::ObjectNumber number {0};
	gamesave::FireEffectRecord record;
	std::optional<PlayerNames> player;
};

/// A fireball's ball of the save, by its number in the world file, with its row of the fireball's three
struct PlannedFireBall
{
	gamesave::ObjectNumber number {0};
	int infoRow {0};
	gamesave::MagicFireBallRecord record;
};

/// A pile of wood or of food, or a plain pot, of the save, by its number in the world file, with the type its row names
struct PlannedPile
{
	gamesave::ObjectNumber number {0};
	PotInfo type {PotInfo::FoodPot};
	/// The parts both kinds of pile write
	gamesave::MobileObjectFields base;
	/// A plain pot's resource, town, amount and flags, the rest of a pile's values none
	gamesave::PotFields pile;
	/// The record, whole
	std::variant<gamesave::WoodPileRecord, gamesave::FoodPileRecord, gamesave::MagicWoodRecord, gamesave::PotRecord> record;
	/// A magic wood's player, by its player record's number
	std::optional<PlayerNames> player;
	/// The player a food or wood pile's record names as the one it answers with, by its player record's number; none
	/// when it names none
	std::optional<PlayerNames> pilePlayer;
};

/// A storage pit of the save, by its number in the world file, with the type its row names
struct PlannedStoragePit
{
	gamesave::ObjectNumber number {0};
	AbodeInfo type {AbodeInfo::CelticStoragePit};
	gamesave::StoragePitRecord record;
};

/// A citadel's heart of the save, by its number in the world file, with its citadel's player
struct PlannedCitadelHeart
{
	gamesave::ObjectNumber number {0};
	PlayerNames owner {PlayerNames::NEUTRAL};
	gamesave::CitadelHeartRecord record;
	/// Its citadel's record, kept on the citadel's first heart
	gamesave::CitadelRecord citadel;
};

/// An abode, a creche, a graveyard, a field, a town centre or a workshop of the save, by its number in the world file,
/// with the type its row names
struct PlannedAbode
{
	gamesave::ObjectNumber number {0};
	AbodeInfo type {AbodeInfo::CelticHut};
	/// The parts the six classes write
	gamesave::MultiMapFixedFields base;
	gamesave::AbodeFields abode;
	/// The record, whole
	std::variant<gamesave::AbodeRecord, gamesave::CrecheRecord, gamesave::GraveyardRecord, gamesave::FieldRecord,
	             gamesave::TownCentreRecord, gamesave::SpellDispenserRecord, gamesave::WorkshopRecord>
	    record;
	/// A workshop's sign of the wood it wants, by its number in the world file, with its record
	std::optional<std::pair<gamesave::ObjectNumber, gamesave::ShowNeedsVisualsRecord>> sign;
};

/// An animated static of the save, by its number in the world file, with the type its row names
struct PlannedAnimatedStatic
{
	gamesave::ObjectNumber number {0};
	AnimatedStaticInfo type {AnimatedStaticInfo::NorseGate};
	gamesave::AnimatedStaticRecord record;
};

/// A rock or another mobile static of the save, by its number in the world file, with the type its row names
struct PlannedMobileStatic
{
	gamesave::ObjectNumber number {0};
	MobileStaticInfo type {MobileStaticInfo::Rock};
	/// The parts both classes write
	gamesave::MultiMapFixedFields base;
	gamesave::MobileStaticPart mobileStatic;
	/// A rock's or a bonfire's player, by its player record's number; none for another mobile static
	std::optional<PlayerNames> player;
	/// The record, whole
	std::variant<gamesave::RockRecord, gamesave::MobileStaticRecord, gamesave::BonfireRecord> record;
};

/// A dead or a felled tree of the save, by its number in the world file, with the tree row it was and its player
struct PlannedDeadTree
{
	gamesave::ObjectNumber number {0};
	TreeInfo type {TreeInfo::Beech};
	std::optional<PlayerNames> player;
	std::variant<gamesave::DeadTreeRecord, gamesave::FelledTreeRecord> record;
	/// A player's hand holds it: made off the map
	bool inHand {false};
};

/// A feature of the save (a script's statue, sign or other fixed thing), by its number in the world file, with its row
struct PlannedFeature
{
	gamesave::ObjectNumber number {0};
	FeatureInfo type {FeatureInfo::None};
	gamesave::FeatureRecord record;
};

/// A town centre's totem statue of the save, by its number in the world file, with the tribe its row names
struct PlannedTotemStatue
{
	gamesave::ObjectNumber number {0};
	Tribe tribe {Tribe::CELTIC};
	gamesave::TotemStatueRecord record;
};

/// A town centre's spell icon of the save, by its number in the world file, with the seed its row names
struct PlannedTownCentreSpellIcon
{
	gamesave::ObjectNumber number {0};
	SpellSeedType seed {SpellSeedType::None};
	gamesave::TownCentreSpellIconRecord record;
};

/// A building site of the save, standard, a workshop's or a citadel's, by its number in the world file
struct PlannedBuildingSite
{
	gamesave::ObjectNumber number {0};
	std::variant<gamesave::StandardBuildingSiteRecord, gamesave::WorkshopBuildingSiteRecord,
	             gamesave::CitadelBuildingSiteRecord>
	    record;
};

/// A street lantern of the save, of its mobile static row, by its number in the world file
struct PlannedStreetLantern
{
	gamesave::ObjectNumber number {0};
	MobileStaticInfo type {MobileStaticInfo::StreetLantern};
	gamesave::StreetLanternRecord record;
};

/// A reward chest of the save, by its number in the world file: its kind, from its row of the reward table, and its
/// player, from the player record it names (none when it names none)
struct PlannedReward
{
	gamesave::ObjectNumber number {0};
	RewardObjectInfo type {RewardObjectInfo::None};
	std::optional<PlayerNames> player;
	gamesave::RewardRecord record;
};

/// A football of the save, by its number in the world file
struct PlannedBall
{
	gamesave::ObjectNumber number {0};
	gamesave::BallRecord record;
};

/// A singing stone's base of the save (an object of no other class of the base's row), by its number in the world file
struct PlannedStoneBase
{
	gamesave::ObjectNumber number {0};
	gamesave::BaseOnlyRecord record;
};

/// What a scaffold's record names that is not an entity: its plan and its candidate, its own planned abodes, as their
/// records hold them (none for a null pointer), and the player who holds or dropped it
struct PlannedScaffold
{
	std::array<std::optional<gamesave::PlannedAbodeRecord>, 2> plans;
	std::optional<PlayerNames> holder;
};

/// A moving object of the save, a plain one, a one-off spell's orb or a scaffold, by its number in the world file, with
/// the row its object names (a scaffold's is the game's one scaffold kind)
struct PlannedMobileObject
{
	gamesave::ObjectNumber number {0};
	MobileObjectInfo type {MobileObjectInfo::EgyptBarrel};
	std::variant<gamesave::MobileObjectRecord, gamesave::OneOffSpellSeedRecord, gamesave::ScaffoldRecord, gamesave::PooRecord>
	    record;
	/// A player's hand holds it: made off the map
	bool inHand {false};
	/// A scaffold's plans and holder
	PlannedScaffold scaffold;
};

/// A vortex of the save (a way into another land, a way out of one or a volcano), by its number in the world file
struct PlannedVortex
{
	gamesave::ObjectNumber number {0};
	std::variant<gamesave::LandscapeVortexInRecord, gamesave::LandscapeVortexOutRecord, gamesave::LandscapeVortexVolcanoRecord>
	    record;
	/// A way out's crossing file save's lines read, when it has one
	std::optional<uint32_t> linesRead;
	/// A way in's object mover, as the save kept it (its particle interface's records)
	std::optional<saved_particles::PlannedEffect> mover;
};

/// A spell's seed of the save, by its number in the world file, with the seed its row names, its creator when that is a
/// player and the player of the interface whose hand holds it
struct PlannedSpellSeed
{
	gamesave::ObjectNumber number {0};
	SpellSeedType type {SpellSeedType::None};
	gamesave::SpellSeedRecord record;
	std::optional<PlayerNames> creatorPlayer;
	std::optional<PlayerNames> statusPlayer;
};

/// A villager or a special villager of the save, by its number in the world file, with the type its row names
struct PlannedVillager
{
	gamesave::ObjectNumber number {0};
	VillagerInfo type {VillagerInfo::None};
	std::variant<gamesave::VillagerRecord, gamesave::SpecialVillagerRecord> record;
	/// A player's hand holds it: made despite its held marks, and off the map
	bool inHand {false};
	/// The walk along a script's track its record names, if it walks one
	std::optional<gamesave::DataPathRecord> dataPath;
	/// Its age in years at the saved game turn (villager_age::AgeOf), which its 3D object made again takes its draw
	/// importance from
	uint32_t ageAtLoad {0};
};

/// One of a town's desire flags of the save, by its number in the world file
struct PlannedDesireFlag
{
	gamesave::ObjectNumber number {0};
	gamesave::TownDesireFlagsRecord record;
};

/// A flock of the save, by its number in the world file, with its player and its place in the game's list of flocks
struct PlannedFlock
{
	gamesave::ObjectNumber number {0};
	gamesave::FlockRecord record;
	/// Its player, by its player record's number; none for none
	std::optional<PlayerNames> player;
	/// The order it was made in, from 1, as the game's list of flocks holds it (the newest first); 0 for one the list
	/// does not name
	uint32_t stamp {0};
	/// Each member's place in the line as the flock's load puts it in, in its record's order (LoadedFlockLine)
	std::vector<uint8_t> lineOrders;
};

/// An animal or a bird of the save, by its number in the world file, with the type its row names (None for a number
/// that is not an animal row): the parts every animal class writes, and its whole record
struct PlannedAnimal
{
	gamesave::ObjectNumber number {0};
	/// The class of its record
	uint32_t recordType {0};
	AnimalInfo type {AnimalInfo::None};
	/// Its age in years at the save's turn, by its birth turn (1500 turns a year)
	uint32_t age {0};
	/// Its place in the order the reads ended
	uint32_t readEnd {0};
	gamesave::ThingPart thing;
	gamesave::PositionPart position;
	gamesave::ObjectPart object;
	gamesave::MobilePart mobile;
	gamesave::WallHugPart move;
	gamesave::LivingFields living;
	gamesave::AnimalFields animal;
	gamesave::SavedRecord record;
	/// A player's hand holds it: made despite its held marks, and off the map
	bool inHand {false};
	/// A vortex's object mover the load makes again carries it: made despite its carried marks, which the mover's load
	/// puts back
	bool carriedByParticles {false};
};

/// A creature of the save, by its number in the world file, with the species its row names
struct PlannedCreature
{
	gamesave::ObjectNumber number {0};
	std::optional<CreatureType> species;
	/// Its place in the order the reads ended
	uint32_t readEnd {0};
	gamesave::CreatureRecord record;
};

/// The line a flock's load puts its members in: each member, in its record's order, goes before the first member already
/// in the line (from the head) whose place is not lower than its own, else at the end, so members of the same place come
/// back the other way round. The indices of the record's members, from the head to the tail
[[nodiscard]] std::vector<size_t> LoadedFlockLine(std::span<const uint8_t> orders);

/// A block of the beach temple's Hanoi puzzle of the save (a base or a ring), by its number in the world file, with the
/// moving object row its record names
struct PlannedHanoiBlock
{
	gamesave::ObjectNumber number {0};
	MobileObjectInfo type {MobileObjectInfo::HanoiPuzzleBase};
	gamesave::HanoiBlockRecord record;
	/// A player's hand holds it: made off the map
	bool inHand {false};
};

/// A puzzle game of the save, by its number in the world file
struct PlannedPuzzleGame
{
	gamesave::ObjectNumber number {0};
	gamesave::PuzzleGameRecord record;
};

/// A wonder of the save, by its number in the world file, with the abode row its record names
struct PlannedWonder
{
	gamesave::ObjectNumber number {0};
	AbodeInfo type {AbodeInfo::CelticWonder};
	gamesave::WonderRecord record;
};

/// A pile of magic food of the save, by its number in the world file, with the pot row its record names and its player,
/// by its player record's number
struct PlannedMagicFood
{
	gamesave::ObjectNumber number {0};
	PotInfo type {PotInfo::MagicFood};
	gamesave::MagicFoodRecord record;
	std::optional<PlayerNames> player;
};

/// An object the load makes, of a class it knows how to make
using PlannedObject = std::variant<PlannedTree, PlannedBigForest, PlannedPile, PlannedStoragePit, PlannedCitadelHeart,
                                   PlannedAbode, PlannedDesireFlag, PlannedMobileStatic, PlannedAnimatedStatic>;

/// The game's script music as the audio's part of the game's tail keeps it: its type, whether it plays, and the town
/// the music last played for, by its number in the world file (0 for none)
struct PlannedScriptMusic
{
	uint32_t type {0};
	uint32_t playing {0};
	gamesave::ObjectNumber town {0};
};

/// What a load makes of the world file
struct WorldPlan
{
	/// The forests, from the last of the game's forest list to its first, so that the newest is made last; a forest
	/// the list does not name comes first
	std::vector<PlannedForest> forests;
	/// The towns, from the last of the game's town list to its first, so that the newest is made last; a town the
	/// list does not name comes first
	std::vector<PlannedTown> towns;
	/// The towns' numbers in the order their players' lists hold them: the players in order, each one's list from
	/// its head
	std::vector<gamesave::ObjectNumber> ownersTowns;
	/// The footpaths, from the last of the game's footpath list to its first, so that the newest is made last; one the
	/// list does not name comes first
	std::vector<PlannedFootpath> footpaths;
	/// The footpath links, in the order their reads ended
	std::vector<PlannedFootpathLink> footpathLinks;
	/// The rivers and the banks of mist, in the order the game puts their links back
	std::vector<PlannedStream> streams;
	std::vector<PlannedMist> mists;
	/// The climates as the game's climate list holds them, the newest first
	std::vector<PlannedClimate> climates;
	/// The things of the weather as the game's list holds them, the newest first
	std::vector<PlannedWeatherThing> weatherThings;
	/// The villagers and the special villagers, in the order their reads ended (the order the game draws their
	/// counters in)
	std::vector<PlannedVillager> villagers;
	/// The scripts' reminders of the living things, by their numbers in the world file
	std::unordered_map<gamesave::ObjectNumber, gamesave::ScriptReminderRecord> scriptReminders;
	/// The flocks, the animals and birds, and the creatures, each in the order their reads ended
	std::vector<PlannedFlock> flocks;
	std::vector<PlannedAnimal> animals;
	std::vector<PlannedCreature> creatures;
	/// The fires as the game's list holds them, the newest first, and the two tags as the last fire read leaves them
	std::vector<PlannedFire> fires;
	/// The fireballs' balls as the game's list holds them, the newest first
	std::vector<PlannedFireBall> fireBalls;
	/// The worship sites, in the order their reads ended
	std::vector<PlannedWorshipSite> worshipSites;
	/// The dances as the game's list holds them, the newest first, and their key frames by number
	std::vector<PlannedDance> dances;
	std::map<gamesave::ObjectNumber, gamesave::DanceKeyFrameRecord> keyFrames;
	std::array<uint8_t, 2> fireTags {};
	/// The fireflies, as the game's list holds them (its head first), and the record read last, whose reward table and
	/// top-up flag the load leaves
	std::vector<std::pair<gamesave::ObjectNumber, gamesave::FireFlyRecord>> fireFlies;
	std::optional<gamesave::FireFlyRecord> lastFireFly;
	/// The arenas as the game's arena list holds them, the newest first
	std::vector<PlannedArena> arenas;
	/// The towns' artifacts as the game's list holds them, the newest first
	std::vector<PlannedArtifact> artifacts;
	/// The scripts' highlights as the game's list holds them, the newest first
	std::vector<PlannedHighlight> highlights;
	/// The scripts' markers as the game's list of things with a place holds them, the newest first
	std::vector<PlannedScriptMarker> markers;
	/// The creature's drinking waypoints as the game's list of waypoints holds them, the newest first
	std::vector<PlannedWaypoint> waypoints;
	/// The scripts' timers, in the order their reads ended (no game list holds them: only the scripts name them)
	std::vector<PlannedScriptTimer> timers;
	/// The influence rings as the game's list holds them, the newest first
	std::vector<PlannedInfluenceRing> rings;
	/// The fish farms that stand on the map, in the order their reads ended
	std::vector<PlannedFishFarm> fishFarms;
	/// The towns' plans by their numbers: a planned abode's, a planned town centre's and a planned citadel heart's
	/// class and values
	std::unordered_map<gamesave::ObjectNumber, std::pair<uint32_t, gamesave::PlannedFields>> townPlans;
	/// The reactions as the game's list holds them, the newest first
	std::vector<PlannedReaction> reactions;
	/// The trees and big forests that stand on the map, in the order the game puts their links back (the order the
	/// map cells get them in): one held, in flight, or of a row that is not its class's is not made yet
	std::vector<PlannedObject> objects;
	/// The two values of the whole game every forest writes, as the last forest read leaves them: the turn the last
	/// tree was planted by a forest and the id the next forest takes
	std::optional<uint32_t> lastTreeCreatedTurn;
	std::optional<uint32_t> nextForestId;
	/// How many records of each class the save holds
	std::map<uint32_t, uint32_t> census;
	/// The moving objects, the one-off spells' orbs and the scaffolds that stand on the map or are in a hand, in the
	/// order their reads ended
	std::vector<PlannedMobileObject> mobileObjects;
	/// The dead and felled trees that stand on the map or are in a hand, and the singing stones' bases, in the order
	/// their reads ended
	std::vector<PlannedDeadTree> deadTrees;
	std::vector<PlannedStoneBase> stoneBases;
	/// The features that stand on the map, in the order their reads ended
	std::vector<PlannedFeature> features;
	/// The street lanterns that stand on the map, in the order their reads ended
	std::vector<PlannedStreetLantern> streetLanterns;
	/// The reward chests, as the game's list of them holds them, the oldest first
	std::vector<PlannedReward> rewards;
	/// The footballs, as the game's list of them holds them, the oldest first
	std::vector<PlannedBall> balls;
	/// The building sites, standard, workshops' and citadels', in the order their reads ended
	std::vector<PlannedBuildingSite> buildingSites;
	/// The town centres' totem statues that stand on the map, in the order their reads ended
	std::vector<PlannedTotemStatue> totemStatues;
	/// The town centres' spell icons that stand on the map, in the order their reads ended
	std::vector<PlannedTownCentreSpellIcon> townCentreIcons;
	/// The spells' seeds, in the order their reads ended
	std::vector<PlannedSpellSeed> spellSeeds;
	/// The vortices, in the order their reads ended
	std::vector<PlannedVortex> vortices;
	/// The objects the players' hands hold, as their interfaces' statuses name them: made despite their held marks (a
	/// thing a script holds keeps them and is not made yet), then taken off the map, as the game's load leaves a thing
	/// that is unavailable
	std::unordered_set<gamesave::ObjectNumber> inHand;
	/// The objects the physics moved when the game was saved, with their bodies: made as any other, then put back into
	/// the physics with their saved pose, velocity and angular momentum, as the game's load does
	std::unordered_map<gamesave::ObjectNumber, gamesave::PhysicsPart> inPhysics;
	/// The Hanoi puzzle's blocks that stand on the map or are in a hand, in the order their reads ended
	std::vector<PlannedHanoiBlock> hanoiBlocks;
	/// The puzzle games, from the last of the game's puzzle game list to its first, so that the newest is made last
	std::vector<PlannedPuzzleGame> puzzleGames;
	/// The wonders that stand on the map, in the order their reads ended
	std::vector<PlannedWonder> wonders;
	/// The piles of magic food that stand on the map, are in a hand or in the physics, in the order their reads ended
	std::vector<PlannedMagicFood> magicFoods;
	/// The food, wood and magic piles in play that were saved made to start their reaction with none started, in the
	/// order their reads ended: the load starts their reaction again
	std::vector<gamesave::ObjectNumber> pileReactions;
	/// The music attached to things, as the game's audio lists it, head first, by their numbers in the world file
	std::vector<std::pair<gamesave::ObjectNumber, gamesave::ThingMusicInfoRecord>> thingMusic;
	/// The game's script music; none for a world with no game object
	std::optional<PlannedScriptMusic> scriptMusic;
	/// The spot visuals' containers of the game's list with their effects, made once every thing they can hang on is
	/// made (saved_particles::MakeContainers)
	std::vector<saved_particles::PlannedContainer> containers;
	/// The particles of the spell in the hand, as the hand's part of the game object's tail names them, with the seed they
	/// play for and whether they are drawn at the hand's bone; made once the seeds are (Makers::handEffect)
	struct PlannedHandEffect
	{
		saved_particles::PlannedEffect effect;
		gamesave::ObjectNumber seed {0};
		bool atBone {false};
	};
	std::optional<PlannedHandEffect> handEffect;
	/// The water and shield spells of the game's list of spells with their effects, made once their creators, seeds and the
	/// things their effects follow are made, before the reactions they started (saved_spells::MakeSpells)
	std::vector<saved_spells::PlannedSpell> spells;
	/// The physical shields of the game's list of map shields, made before the spells that keep them
	std::vector<saved_spells::PlannedPhysicalShield> physicalShields;
	/// The spiritual shields of the same list, made with them in the list's order
	std::vector<saved_spells::PlannedSpiritualShield> spiritualShields;
	/// The teleports' stones of the game's list of objects of several cells, made before the spells that keep them and
	/// the reactions they spread
	std::vector<saved_spells::PlannedTeleportStone> teleportStones;
	/// Each object's place in the order the reads ended (the file's load order), which orders the creation draws the load
	/// takes at its end (ecs::creation_draws)
	std::unordered_map<gamesave::ObjectNumber, size_t> readEnds;
};

/// The plan of a world file as far as it was read
[[nodiscard]] WorldPlan PlanOf(const gamesave::WorldRead& world);

/// Of a class of records: how many the save holds and how many the load made
struct CensusLine
{
	uint32_t type {0};
	uint32_t saved {0};
	uint32_t made {0};

	bool operator==(const CensusLine&) const = default;
};
using Census = std::vector<CensusLine>;

/// What makes each class of object: a tree or a big forest of the plan, of the forest of that key (0: none);
/// entt::null when it is not made
struct Makers
{
	/// A footpath of the plan, with its nodes
	std::function<entt::entity(const PlannedFootpath& footpath)> footpath;
	/// A footpath link of the plan, with the footpaths it lists (made already)
	std::function<entt::entity(const PlannedFootpathLink& link, const std::vector<entt::entity>& footpaths)> footpathLink;
	/// A town of the plan; entt::null when it is not made
	std::function<entt::entity(const PlannedTown& town)> town;
	std::function<entt::entity(const PlannedTree& tree, uint32_t forestId)> tree;
	std::function<entt::entity(const PlannedBigForest& bigForest, uint32_t forestId)> bigForest;
	/// A pile of the plan
	std::function<entt::entity(const PlannedPile& pile)> pile;
	/// A storage pit of the plan, of the town of that key, with its piles (made already: they are read inside it)
	std::function<entt::entity(const PlannedStoragePit& pit, uint32_t townKey,
	                           const archetypes::AbodeArchetype::StoragePitPiles& piles)>
	    storagePit;
	/// A citadel's heart of the plan, of the citadel kept on the entity given (null: the citadel's first heart, which
	/// keeps it)
	std::function<entt::entity(const PlannedCitadelHeart& heart, entt::entity citadel)> citadelHeart;
	/// An abode, a creche or a graveyard of the plan, of the town of that key
	std::function<entt::entity(const PlannedAbode& abode, uint32_t townKey)> abode;
	/// A desire flag of the plan, of the town given (made already)
	std::function<entt::entity(const PlannedDesireFlag& flag, entt::entity town)> desireFlag;
	/// A rock or another mobile static of the plan
	std::function<entt::entity(const PlannedMobileStatic& object)> mobileStatic;
	/// An animated static of the plan
	std::function<entt::entity(const PlannedAnimatedStatic& object)> animatedStatic;
	/// A moving object, a one-off spell's orb or a scaffold of the plan (a scaffold's building site, workshop, town and
	/// holder are given once the building sites are made)
	std::function<entt::entity(const PlannedMobileObject& object)> mobileObject;
	/// A dead or felled tree, and a singing stone's base, of the plan
	std::function<entt::entity(const PlannedDeadTree& tree)> deadTree;
	std::function<entt::entity(const PlannedStoneBase& base)> stoneBase;
	/// A feature of the plan
	std::function<entt::entity(const PlannedFeature& feature)> feature;
	/// A street lantern of the plan
	std::function<entt::entity(const PlannedStreetLantern& lantern)> streetLantern;
	/// A reward chest of the plan, given the town it names, made already (or null)
	std::function<entt::entity(const PlannedReward& reward, entt::entity town)> reward;
	/// A football of the plan, given the town it names, made already (or null)
	std::function<entt::entity(const PlannedBall& ball, entt::entity town)> ball;
	/// A building site of the plan, on its building (made already, or null), with the objects its record names (its
	/// wood piles, made already); its builders and its town's list are given once the villagers are made
	std::function<entt::entity(const PlannedBuildingSite& site, entt::entity building,
	                           const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    buildingSite;
	/// A town centre's totem statue of the plan, of its town centre (made already, or null)
	std::function<entt::entity(const PlannedTotemStatue& totem, entt::entity townCentre)> totemStatue;
	/// A town centre's spell icon of the plan, of its town and its town centre (made already, or null); the towns' lists
	/// of icons are given once every icon is made
	std::function<entt::entity(const PlannedTownCentreSpellIcon& icon, entt::entity town, entt::entity townCentre)>
	    townCentreIcon;
	/// A spell's seed, once every object is made, with the entities of the worship icon that charged it and of its
	/// creator when that is not a player (null for none or one not made)
	std::function<entt::entity(const PlannedSpellSeed& seed, entt::entity icon, entt::entity creator)> spellSeed;
	/// A vortex of the plan, once every object it names is made (`objectOf` gives the entity made of an object of the
	/// file)
	std::function<entt::entity(const PlannedVortex& vortex,
	                           const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    vortex;
	/// A river and a bank of mist of the plan
	std::function<entt::entity(const PlannedStream& stream)> stream;
	std::function<entt::entity(const PlannedMist& mist)> mist;
	/// Every climate of the plan at once, newest first; how many were made
	std::function<size_t(const std::vector<PlannedClimate>& climates)> climates;
	/// A thing of the weather; made from the list's last to its first, each at the head of the things' list
	std::function<entt::entity(const PlannedWeatherThing& thing)> weatherThing;
	/// A worship site of the plan, once its citadel's heart, its towns and its food pot are made (`objectOf` gives the
	/// entity made of an object of the file); entt::null when it is not made
	std::function<entt::entity(const PlannedWorshipSite& site,
	                           const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    worshipSite;
	/// A villager of the plan, with the state counter its load drew (none: as saved); entt::null when it is not made
	std::function<entt::entity(const PlannedVillager& villager, std::optional<uint16_t> counter)> villager;
	/// A flock of the plan, once the villagers are made; entt::null when it is not made
	std::function<entt::entity(const PlannedFlock& flock, const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    flock;
	/// An animal or a bird of the plan, once the flocks are made; entt::null when it is not made
	std::function<entt::entity(const PlannedAnimal& animal,
	                           const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    animal;
	/// A creature of the plan, once the animals are made; entt::null when it is not made
	std::function<entt::entity(const PlannedCreature& creature,
	                           const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    creature;
	/// A dance of the plan, once its dancers and what it is danced about are made (`objectOf` gives the entity made of
	/// an object of the file); entt::null when it is not made
	std::function<entt::entity(const PlannedDance& dance, const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    dance;
	/// A fireball's ball, before the fires (its fire names it) and the effect whose atom carries it; that effect is given
	/// to it once the effects are made
	std::function<entt::entity(const PlannedFireBall& ball)> fireBall;
	/// Every fire of the plan at once, newest first, once every object is made (`objectOf` gives the entity made of an
	/// object of the file, or none) with the tags as the last fire read left them; each one's handle, 0 for one not made
	std::function<std::vector<uint32_t>(const std::vector<PlannedFire>& fires, const std::array<uint8_t, 2>& tags,
	                                    const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    fires;
	/// A firefly, at the fireflies' list's tail; and the reward table and top-up flag of the last firefly read
	std::function<void(const SavedFireFly& fireFly)> fireFly;
	std::function<void(const gamesave::FireFlyRecord& record)> fireFlyRewards;
	/// An arena; made from the list's last to its first, so that the newest is made last
	std::function<entt::entity(const PlannedArena& arena)> arena;
	/// A script's highlight; made from the list's last to its first, so that the newest is made last
	std::function<entt::entity(const PlannedHighlight& highlight)> highlight;
	/// A script's marker; made from the list's last to its first, so that the newest is made last
	std::function<entt::entity(const PlannedScriptMarker& marker)> marker;
	/// A drinking waypoint; made from the list's last to its first, in the order the land made them
	std::function<entt::entity(const PlannedWaypoint& waypoint)> waypoint;
	/// A script's timer
	std::function<entt::entity(const PlannedScriptTimer& timer)> timer;
	/// An influence ring, once every object is made, with the entity of the object it follows (null for none); made
	/// from the list's last to its first, so that the newest is at the list's head again
	std::function<entt::entity(const PlannedInfluenceRing& ring, entt::entity attached)> influenceRing;
	/// A fish farm, of the town its record names (made already; null for none)
	std::function<entt::entity(const PlannedFishFarm& farm, entt::entity town)> fishFarm;
	/// A reaction, with the entities its initiator and its dance are now (null for one not made); made from the
	/// list's last to its first; its id, 0 for none
	std::function<uint32_t(const PlannedReaction& reaction, entt::entity initiator, entt::entity dance)> reaction;
	/// Starts the reaction of a loaded pile saved made to start it with none started, once the reactions are made
	std::function<void(entt::entity pile)> pileReaction;
	/// A Hanoi puzzle's block, with the moving objects; its links to the blocks and the puzzle it names are given by
	/// its puzzle's maker
	std::function<entt::entity(const PlannedHanoiBlock& block)> hanoiBlock;
	/// A puzzle game, once every object is made (`objectOf` gives the entity made of an object of the file);
	/// entt::null when it is not made
	std::function<entt::entity(const PlannedPuzzleGame& puzzle,
	                           const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    puzzleGame;
	/// A wonder, of the town of that key (k_NoTown: none), once the plan's other buildings are made
	std::function<entt::entity(const PlannedWonder& wonder, uint32_t townKey)> wonder;
	/// A pile of magic food; the town it was made for and its sparkle's container are given by the load
	std::function<entt::entity(const PlannedMagicFood& food)> magicFood;
	/// The music attached to things, once every object is made, put back as the game's audio list; how many of them are
	std::function<uint32_t(std::span<const std::pair<gamesave::ObjectNumber, gamesave::ThingMusicInfoRecord>> saved,
	                       const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    thingMusic;
	/// The game's script music, once every object is made, with the entity of the town it names (null for none)
	std::function<void(const PlannedScriptMusic& music, entt::entity town)> scriptMusic;
	/// The particles of the spell in the hand, made again from their records, given to the hand's effects with the seed
	/// they play for (null when the load did not make it), their file's particle type and whether they are drawn at the
	/// hand's bone; their particle id, 0 when none was taken
	std::function<uint32_t(std::unique_ptr<particles::Effect> effect, entt::entity seed, ParticleType type, bool atBone)>
	    handEffect;
};

/// Makes the plan's forests in the forest list (each with its saved id, scenic flag and record), puts back the two
/// values of the whole game, makes the towns through `makers` and gives each its forest list, then makes the objects
/// in their order, each of the forest its record names (by the key the forest list gave it), and last gives each
/// forest the big forest its record names. Of every class of the save, how many were made. With `loaded`, each entity
/// made is added to it by its number in the file and the class of its record, for the next save's writer. With
/// `resolves`, the objects whose resolve draws random numbers are added to it in the order the game puts their links
/// back, for Resolve once the game's seeds are back; without it each is resolved at once
Census Apply(const WorldPlan& plan, const Makers& makers, saved_world_write::LoadedObjects* loaded = nullptr,
             std::vector<entt::entity>* resolves = nullptr);
/// (census) the census once the load's later steps (the players' values, the loaded sources) have kept their objects
/// too: a class counts as made at least as many of its objects as `loaded` holds of it, never more than were saved
void CountKept(Census& census, const saved_world_write::LoadedObjects& loaded);
/// The mind file a loaded creature reads: the profile's (given, none for a profile that names no file) for the local
/// player's creature, else the SavedCreature file its place in the list of creatures names in the slot's folder; none
/// for a creature of no place
[[nodiscard]] std::optional<std::filesystem::path> CreatureMindFileOf(const gamesave::CreatureFields& fields,
                                                                      const std::filesystem::path& slotFolder,
                                                                      const std::optional<std::filesystem::path>& profile);
/// What the resolve of a loaded game's objects needs: the folder of its slot (and land), where its creatures' mind files
/// are, and the game's leash byte
struct ResolveContext
{
	std::filesystem::path slotFolder;
	std::optional<uint8_t> leashes;
};
/// The part of the game's resolve of its loaded objects that draws random numbers, after the whole read and so after
/// the seeds are back, in the order given: each animal or bird takes the clip of the state it was saved in (a bird's
/// flight clip, an eating animal's clip draw); each creature reads its mind file (the profile's for the local player's
/// creature, its slot's SavedCreature file for the others), with the draws of its saved sources, its body and leashes
/// from it, and stays out of the map when the file is not there. Then the local player's creature knows the evil, rope
/// and good leashes the game's leash byte says
void Resolve(std::span<const entt::entity> resolves, const ResolveContext& context);

/// The game's tree: the tree archetype at its saved place, angle, size and maximum, with its saved growth counter and
/// no random draw, neither growing nor marked as standing in a town (the save keeps neither flag, and the game makes a
/// loaded tree with both clear); its saved creation number and life, and its record kept on it
entt::entity MakeTreeEntity(const PlannedTree& tree, uint32_t forestId);
/// The game's big forest: the big forest archetype at its saved place, angle and size, of its saved forest (it makes
/// none) with its saved wood; its saved creation number and life, and its record kept on it
entt::entity MakeBigForestEntity(const PlannedBigForest& bigForest, uint32_t forestId);
/// The game's town: the town archetype with its saved id (the next free key when another town has it, the saved id
/// kept as its script id), place, player and tribe, without desire flags (they have records of their own); its
/// belief in the neutral player, its area and whether it is uninhabitable, and its record kept on it
entt::entity MakeTownEntity(const PlannedTown& town);
/// The game's pile: the pot archetype at its saved place and angle, of its type, holding its saved amount, at once at
/// the height of that amount (the saved rise is kept in its record), with its saved creation number and life, and its
/// record kept on it
entt::entity MakePileEntity(const PlannedPile& pile);
/// The game's storage pit: the abode archetype at its saved place, angle and size, of its town, whole, taking its
/// saved piles rather than making its own, with its saved food and wood, creation number and life, and its record
/// kept on it
entt::entity MakeStoragePitEntity(const PlannedStoragePit& pit, uint32_t townKey,
                                  const archetypes::AbodeArchetype::StoragePitPiles& piles);
/// The game's footpath: a new footpath at the head of the footpath list, with its saved nodes (their places, flags and
/// order) and whether it is in use, and its record kept on it
entt::entity MakeFootpathEntity(const PlannedFootpath& footpath);
/// The game's footpath link: the footpaths it lists, in its saved order, and its record kept on it; the things that
/// hold it are given it as they are made
entt::entity MakeFootpathLinkEntity(const PlannedFootpathLink& link, const std::vector<entt::entity>& footpaths);
/// The game's citadel heart: the citadel archetype's heart at its saved place and angle, of its citadel's player and
/// of its citadel (the first heart of a citadel keeps it), with its saved scale, life, build flags and percent built,
/// opening no worship site (they have records of their own); its saved creation number, and its record (the first
/// heart also its citadel's) kept on it
entt::entity MakeCitadelHeartEntity(const PlannedCitadelHeart& heart, entt::entity citadel);
/// The game's abode, creche or graveyard: the abode archetype at its saved place, angle and size, of its town, whole
/// or being built as its saved build flags say, with its saved food and wood, build flags, percent built, creation
/// number and life; a graveyard with its saved dead and its graves not shown (the original keeps their stage on the
/// 3D object, which a load makes afresh); a town centre not made functional, as its load makes its totem and its icons
/// from their own records and its town names it as its centre, but with its belief symbols when it is functional; its
/// record kept on it
entt::entity MakeAbodeEntity(const PlannedAbode& abode, uint32_t townKey);
/// The game's desire flag: the desire flag archetype of its town, its desire, its slot's point of the store, its mesh
/// and its object scale, at its saved place; the place it was made at, its height over the ground and its desire from
/// the save; its creation number and record kept. Its town keeps it among its flags
entt::entity MakeDesireFlagEntity(const PlannedDesireFlag& flag, entt::entity town);
/// The game's rock or mobile static: the mobile static archetype at its saved place, raised by its saved altitude over
/// the land, of its type, at its saved X, Y and Z angles and size; a rock of its saved player, a mobile static's built
/// percentage when it is not 0; its saved creation number and life, and its record kept on it
entt::entity MakeMobileStaticEntity(const PlannedMobileStatic& object);
/// The game's animated static: the animated static archetype of its row at its saved place, angle and size, with the
/// open state its record keeps and the gate stones its slots keep, each laid again by the model its slot names; its
/// creation number, life, place's marks and record kept
entt::entity MakeAnimatedStaticEntity(const PlannedAnimatedStatic& object);
/// A moving object made again at its saved place, angles and size through the moving object's archetype, or a one-off
/// spell's orb of its saved seed, power-up and multiplier through the orb's; into the map unless a hand holds it; its
/// creation number, life and record kept
entt::entity MakeMobileObjectEntity(const PlannedMobileObject& object);
/// A spell's seed made again with its saved seed, power-up, flags, charge, chants and the rest it writes, of its icon and
/// its creator (a player, the icon given, or none), its creation number, life and record kept. The spell cast from it is
/// not made again (none), and like every seed the game makes it is an interface's
entt::entity MakeSpellSeedEntity(const PlannedSpellSeed& seed, entt::entity icon, entt::entity creator);
/// A fireball's ball made again at its saved place and size, of its row, with the place it was made at, whether rain
/// cools it, its creation number and record kept; its fire and its effect are given by the load
entt::entity MakeFireBallEntity(const PlannedFireBall& ball);
/// A vortex made again as a script's CREATE makes one at its saved centre, its crossing file opened as the game's
/// load opens it (a way in's written after what is there, a way out's read on past the lines it had read), then given
/// its saved state, radius, the heights it flattened and their mean, its marks, creation number and record kept; a
/// way out also its flocks, their place and lengths, the counts of what it made, the villagers it threw out and its
/// town
entt::entity MakeVortexEntity(const PlannedVortex& vortex, const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// A dead or felled tree made again through the dead tree's archetype on the land under its place, raised by its
/// altitude, of its tree row and player, at its life, X, Y and Z angles, with its wood multiplier; a felled one also
/// felled; into the map unless a hand holds it; its creation number and record kept
entt::entity MakeDeadTreeEntity(const PlannedDeadTree& tree);
/// A feature made again through the feature's archetype at its saved place, angle and size, built as far as saved, with
/// its marks, creation number, life and record kept
entt::entity MakeFeatureEntity(const PlannedFeature& feature);
/// A street lantern made again as its load makes it: on the land at its saved place, a country lantern or not as saved,
/// with its light, its creation number, life and record kept; the lanterns' sounds on or off as it was saved
entt::entity MakeStreetLanternEntity(const PlannedStreetLantern& lantern);
/// A reward chest made again as its load reads it: at its saved place, angle and size, with its saved state, clock,
/// dust time and map mark (the reward system's Restore), its creation number, life and record kept
entt::entity MakeRewardEntity(const PlannedReward& reward, entt::entity town);
/// A football made again as its load reads it: at its saved place, angles and size, with its two points and its town,
/// and free to be picked up, as the game does not save whether its town has taken it; its creation number, life and
/// record kept, and no call to come and play, which its own record makes again
entt::entity MakeBallEntity(const PlannedBall& ball, entt::entity town);
/// A building site made again as its load reads it: on its building, which links back to it, with its ring of builders'
/// places, builders' count, repair mark, forced desire and repair base as saved; a standard site with its wood pile, a
/// workshop's with none (its pile is the workshop's), a citadel's with its six wood piles by their slots; its record
/// kept; none without its building
entt::entity MakeBuildingSiteEntity(const PlannedBuildingSite& site, entt::entity building,
                                    const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// A town centre's totem statue made again as its load makes it: its tribe's plinth and the icon on it at its saved
/// place, angle and size, of its town centre, built as saved, standing at the worship share it was saved rising to, with
/// the hand's share the same, as its resolve sets it; given to no player (the load's end gives every totem its player's
/// creature's icon). Its town centre has it as its totem when the centre's record names it. Its creation number, life,
/// place marks and record kept
entt::entity MakeTotemStatueEntity(const PlannedTotemStatue& totem, entt::entity townCentre);
/// A town centre's spell icon made again as its load makes it: the town's icon model at its saved place, angle and size,
/// of its seed, its town and its town centre, in the centre's slot the centre's record names it in, with the power-up
/// levels it was saved with, its seed's graphic above it showing the highest of them; its player its town's. It goes in
/// no town's list (the town's own record lists its icons) and makes no worship site's icon (the site's records hold
/// them). Its creation number, place marks and record kept
entt::entity MakeTownCentreSpellIconEntity(const PlannedTownCentreSpellIcon& icon, entt::entity town, entt::entity townCentre);
/// The feature row of a kind's number, none for a number no feature row has (the rows run from 2064 down)
[[nodiscard]] std::optional<FeatureInfo> FeatureTypeOfInfo(uint32_t info);
/// A singing stone's base made again as a mobile static of the base's row at its place, height, angle and size, with
/// the thing's marks of an object out of play and of a hidden name, its creation number and record kept
entt::entity MakeStoneBaseEntity(const PlannedStoneBase& base);
/// The moving object row of a kind's number, none for a number no moving object row has
[[nodiscard]] std::optional<MobileObjectInfo> MobileObjectTypeOfInfo(uint32_t info);
/// The game's villager: the villager archetype's restored villager of its row at its saved place, angle and size,
/// with its saved values (its flags, food, last check, disciple, held food and wood, pregnancy, birth, life, states,
/// walk, specular colour), its counter as drawn or saved, its creation number and its record kept, put in the map with
/// its first clip; its home, town and mother are given once every villager is made. One the load does not make yet
/// (held, in flight, on a footpath or a data path, in a flock or a reaction, walking round something, or in a
/// state that loads values) is not made, and one log line says why
entt::entity MakeVillagerEntity(const PlannedVillager& villager, std::optional<uint16_t> counter);
/// What a villager kept of its script state, from its script's reminder: the walk's destination with its height, the
/// script's place in whole map x and z and its number, the pair of whole numbers, the clip its state last put on and
/// the state to take up; a place, number and pair all 0 are what no script gave (none), as the villager's own state
/// values are read
[[nodiscard]] components::ScriptMemory ScriptMemoryOf(const gamesave::ScriptReminderRecord& reminder, entt::entity target);
/// A firefly of the save as openblack holds it, from its record: what it is doing, where it is and where its object was
/// made, its home, the light and its last turn's place from the record's raw map coordinates, its drawn point, clock,
/// drift, progress, flight's length, speeds, six phases, flags and creation number
[[nodiscard]] SavedFireFly SavedFireFlyOf(const gamesave::FireFlyRecord& record);
/// The fireflies' reward table (its running sums and weights) and whether the next nightfall tops them up, from a record
void RestoreFireFlyRewards(const gamesave::FireFlyRecord& record);
/// The game's worship site: the site in its saved slot of its citadel, turned to its saved angle, of its tribe (the
/// tribe rows run from 742 down), as built as saved, with its towns (each town's site set), food pot, chants, cheats,
/// strain and food on its way, and its totem with its need signs, each with its saved creation number (and a sign its
/// saved desire) and its record kept; its creation number, life, marks and record kept. Its villagers and those requesting to
/// go home are given once the villagers are made. None for a citadel that is not made or a slot that is taken
entt::entity MakeWorshipSiteEntity(const PlannedWorshipSite& planned,
                                   const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// The game's dance: a dance of its row, with the file its row names, at its saved place, with its saved clock, loop,
/// speeds, round, state and times, its owner, and its groups as their records say, each with its dancers (each given
/// its place in the dance); made after every dance there is. One whose dancers were not all made is not made, and one
/// log line says so
entt::entity MakeDanceEntity(const PlannedDance& planned, const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// The game's fires: from the list's last to its first, each put back at the head of the fires' list (fire::Restore)
/// burning the object it burnt, heated by its heater, of its player, at its saved temperatures, charring, tag and
/// flags; then each linked to its group's root and next and given its root's firemen, as they were made; then the
/// tags the fires take and are processed by. A fire whose object was not made is not made. Its reaction comes once the
/// reactions are made (GiveFiresTheirReactions)
std::vector<uint32_t> MakeFires(const std::vector<PlannedFire>& fires, const std::array<uint8_t, 2>& tags,
                                const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// Each made fire's reaction, as the fire's load reads it: the id of the reaction its record names, made again (by the
/// fires' ids and the reactions' ids, by their numbers in the file); a fire whose reaction was not made keeps none
void GiveFiresTheirReactions(std::span<const PlannedFire> fires,
                             const std::unordered_map<gamesave::ObjectNumber, uint32_t>& fireIds,
                             const std::unordered_map<gamesave::ObjectNumber, uint32_t>& reactionIds);
/// The game's river: its saved id and its points as they were saved (each a place with the ground's height there),
/// its record kept on it. Its landscape footprints are made once every river is (ecs::CreateRiverFootprints), as the
/// original's load makes each river's as it resolves
entt::entity MakeStreamEntity(const PlannedStream& stream);
/// The game's bank of mist: the mist archetype at its saved place and altitude over the land, with its saved size,
/// colour and edge-on shrink, its record kept on it
entt::entity MakeMistEntity(const PlannedMist& mist);
/// The climate type its row's number names: the 7 climate rows run from 327 (the first, the world's) down
[[nodiscard]] std::optional<uint32_t> ClimateTypeOfInfo(uint32_t info);
/// The other way: the number of a climate type's row
[[nodiscard]] uint32_t ClimateInfoRowOf(uint32_t type);
/// The game's climates: each with its saved place, radii, id, type, rain, temperature and its target, wind, storms'
/// most, world flag, storms' height, fall speed, the word the game keeps, and its storms' lightning; the counters as
/// the last climate saved them. Each climate's natural storms are made as the original's load makes them: in their saved
/// order, each from its saved descriptor and put at the head of the climate's list (so the list comes back the other
/// way round), then given its saved age, speed and target. How many climates were made
size_t MakeClimates(const std::vector<PlannedClimate>& climates);
/// The game's thing of the weather: at its saved place, with its own descriptor and
/// whether the wind moves it, and its storm when it has one, made from its saved descriptor and given its saved age,
/// speed and target; at the head of the things' list, its record kept on it
entt::entity MakeWeatherThingEntity(const PlannedWeatherThing& thing);
/// The game's arena: an arena entity at its saved place (the place's map units, its height above the land, and the
/// land's height there in metres, as a land script's arena stands), with its saved radius, the next in the arenas'
/// order
entt::entity MakeArenaEntity(const PlannedArena& arena);
/// A script's highlight as a script makes it, of its row, at its place with its size and angle, with its script id; then
/// whether it is hidden or active, its set height and its did-you-know category, its creation number and its marks as
/// the record says
entt::entity MakeHighlightEntity(const PlannedHighlight& highlight);
/// A script's marker as a script makes it, at its saved place and height over the land, with its marks as the record
/// says
entt::entity MakeScriptMarkerEntity(const PlannedScriptMarker& marker);
/// A drinking waypoint as the land's script makes it, at its saved place, numbered after the waypoints made so far
entt::entity MakeWaypointEntity(const PlannedWaypoint& waypoint);
/// A script's timer as a script makes one, then the turn it was set on and the turns it was set for, and its marks, as
/// the record says
entt::entity MakeScriptTimerEntity(const PlannedScriptTimer& timer);
/// An influence ring as a script makes it, at its saved place, of its player, with its radius and anti word, and
/// following the object given while it exists; none for a ring of no player
entt::entity MakeInfluenceRingEntity(const PlannedInfluenceRing& ring, entt::entity attached);
/// A fish farm as a script makes it, of its row, at its place; then its town, its stock and its creation number as the
/// record says. Its fishermen stay as the record names them
entt::entity MakeFishFarmEntity(const PlannedFishFarm& farm, entt::entity town);
/// A reaction as its record keeps it: its initiator and dance as given, its type, player, start turn, whether it is
/// active and stealthy, and its radius, added to the game's reactions without being spread (the load's followers are
/// put back by their own living things); its id
uint32_t MakeReaction(const PlannedReaction& reaction, entt::entity initiator, entt::entity dance);
/// A storm's descriptor as the game writes it: its 80 bytes in the descriptor's order (the place, the two radii, the
/// fade and life times, the strength, the clouds' number, blackness and height, the fall speed, the four lightning
/// times, the snow cover's rate, the word the game never reads, and the weather it adds, a byte each)
[[nodiscard]] weather::storms::StormDescriptor StormDescriptorOf(const std::array<uint8_t, 80>& bytes);
/// The other way: a descriptor's 80 bytes as the game writes them
[[nodiscard]] std::array<uint8_t, 80> StormDescriptorBytes(const weather::storms::StormDescriptor& descriptor);
/// The game's flock: of its player, with its saved script id, info, domain's centre and radius, members' distance,
/// flying height, the most members it had, its leader's turns, calm, followers' states and mode, whether it follows a
/// shepherd, its kept centre, its town, its shepherd and the temple heart whose doves or bats it is, and its members in
/// the line its load puts them in (LoadedFlockLine), each animal given the flock and its saved place; its record kept.
/// Not made (entt::null) while any object it names is not made
entt::entity MakeFlockEntity(const PlannedFlock& planned, const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// The game's animal or bird: the animal archetype's restore at its saved place, y angle and scale, with its creation
/// number, life and place marks, its states, counters, move, needs, sleeping cell, status, birth turn, death reason and
/// specular colour as saved, its record kept; its flock, town and the things it names are given once every animal is
/// made. One of a spell or a puzzle game, held or in flight, on a footpath or a data path, with a script's reminder or in
/// a dance is not made yet, with one log line
entt::entity MakeAnimalEntity(const PlannedAnimal& planned,
                              const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// The game's creature: made as a new creature of its species is (CreatureArchetype::Create, whose draws come before the
/// load puts the seeds back, as the original's), for its player, at its saved place, y angle and size, with its saved
/// alignment and strength; then its creation number, place marks, whether it can die and go through a vortex, its spell
/// slots, freeze and fizz, its mind's stillness, its size limits, the leash's target, its home, its fight health, its
/// autoscale and its creed glows as saved, its record kept, and a script's play it was carrying out to take up again.
/// Its mind file is not read yet. One held, in flight, fighting, carrying out any other kept plan, dancing, casting or
/// with a spell waiting is not made yet, with one log line
/// The script's play a creature's kept plan carries out, as the save writes it: its plan of obeying its player, the
/// steps left of playing one animation (individual or static) and then the wait until its body is free. None for any
/// other plan
[[nodiscard]] std::optional<creature_script_play::Request> ScriptPlayOf(const gamesave::KeptCreaturePlan& plan);
/// A plan forced on a creature that its kept plan carries out: the action's row, the things it is for, done to and
/// done with, the step it is at and, at a look at the camera, the turns it looks for
struct KeptForcedPlan
{
	uint32_t action {0};
	std::array<gamesave::ObjectNumber, 3> objects {};
	size_t step {0};
	size_t count {0};
	std::optional<uint32_t> turns;
	/// At a played pose (a sit), the seconds it is held for
	std::optional<float> seconds;

	bool operator==(const KeptForcedPlan&) const = default;
};
/// The forced plan a creature's kept plan carries out, as the save writes it: its plan of obeying its player, a look at
/// the camera (its one step), lying dead for good (the faint and the long wait) or sleeping at home (a yawn or not, the
/// walk home, the face down the slope, the sleep and the dazed wake), or sitting down at a thing (the walk, the face,
/// the face down the slope and the sit, the main step). None for any other plan
[[nodiscard]] std::optional<KeptForcedPlan> ForcedPlanOf(const gamesave::KeptCreaturePlan& plan);
/// Once the miracles are made again, each loaded creature's spell slots name the miracle made from the number they were
/// saved naming (`made`, by the miracle's number), none for one not made; the numbers kept for it go
void LinkSpellSlots(ecs::Registry& registry, const std::unordered_map<uint32_t, entt::entity>& made);
/// The legs left of a script's move a creature's kept plan carries out, as the save writes it: its plan of following its
/// player, each step a walk to within a distance of a point (the point and the distance). None for any other plan
[[nodiscard]] std::optional<std::vector<std::pair<glm::vec3, float>>> ScriptMoveOf(const gamesave::KeptCreaturePlan& plan);
/// A creature in its duel as its kept plan keeps it: whether it started the fight, and the other creature
struct KeptDuel
{
	bool startedIt {false};
	gamesave::ObjectNumber other {0};

	bool operator==(const KeptDuel&) const = default;
};
/// The duel a creature's kept plan holds it in, as the save writes it: the creature that started the fight at the fight
/// step of its fight's agenda, the opponent obeying it at the fight its order gave it, each fighting the other. None for
/// any other plan, before the duel or after it
[[nodiscard]] std::optional<KeptDuel> DuelOf(const gamesave::KeptCreaturePlan& plan);
/// Who fights for a creature, by the game's saved value: 1 the player, 2 the computer, 0 (or any other) nobody
[[nodiscard]] creature_fight::Control AutoFightControlOf(uint32_t autoFight);
/// A creature's dance with villagers as its kept plan keeps it: the desire and action of its plan, the things it is for,
/// done to (the villager) and done with, the step it is at, and what the steps left say of the dance: its place, its
/// row of the dance table, the fewest dancers, the turns to dance and how near the creature goes to the villager (0 for
/// those its steps left do not hold)
struct KeptDance
{
	uint32_t desire {0};
	uint32_t action {0};
	std::array<gamesave::ObjectNumber, 3> objects {};
	size_t step {0};
	glm::vec2 place {0.0f};
	uint32_t type {0};
	uint32_t fewestDancers {0};
	uint32_t danceTurns {0};
	float nearDistance {0.0f};

	bool operator==(const KeptDance&) const = default;
};
/// The dance with villagers a creature's kept plan carries out, as the save writes it: the fourteen steps of the dance's
/// agenda, those left from the step it is at. None for any other plan
[[nodiscard]] std::optional<KeptDance> DanceOf(const gamesave::KeptCreaturePlan& plan);
/// The turn a script's focus has a creature make, as its kept plan keeps it: its plan of following its player, one step,
/// the turn to face a point or a thing (the thing's number and the seconds the turn is held). None for any other plan
struct KeptScriptTurn
{
	std::optional<glm::vec3> point;
	gamesave::ObjectNumber thing {0};
	float seconds {0.0f};

	bool operator==(const KeptScriptTurn&) const = default;
};
[[nodiscard]] std::optional<KeptScriptTurn> ScriptTurnOf(const gamesave::KeptCreaturePlan& plan);
/// Whether a kept plan is the one a finished plan leaves: no desire of the forty, no action, nothing in its agenda
[[nodiscard]] bool IsClearedPlan(const gamesave::KeptCreaturePlan& plan);
/// Why the load does not make a creature yet (MakeCreatureEntity), none when it does
[[nodiscard]] const char* WhyCreatureNotMade(const PlannedCreature& planned);
entt::entity MakeCreatureEntity(const PlannedCreature& planned,
                                const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// The game's makers: MakeFootpathEntity, MakeFootpathLinkEntity, MakeTownEntity, MakeTreeEntity, MakeBigForestEntity,
/// MakePileEntity and MakeStoragePitEntity
[[nodiscard]] Makers GameMakers();
/// A Hanoi puzzle's block made again as a moving object of its row at its saved place, angle and size, into the map
/// unless a hand holds it, its creation number, life, place marks and record kept, with its saved rest place; where
/// the puzzle put it is where it stands when that is its rest place, else its rest place. Its links are given by its
/// puzzle's maker
entt::entity MakeHanoiBlockEntity(const PlannedHanoiBlock& block);
/// Whether the load makes a puzzle game of that type, whose state its record holds whole: the two tree puzzles, the
/// beach temple's Hanoi puzzle and the types that have nothing to play
[[nodiscard]] bool PuzzleGameTypeLoaded(uint32_t type);
/// A puzzle game made again as its load reads it, without the start a script's making gives it: its type, place, size,
/// angle, grid steps, whether it is played and whether its play has begun as saved; its count of calm turns 5, as
/// every new puzzle game's. The Hanoi puzzle also its bases and rings, each block given the blocks above and below it
/// and its puzzle as its record names them; a tree puzzle its spots' counts, trees, mist and bats, and its lanterns.
/// It joins the puzzles processed each turn after those made before it. Its record kept on it. One of another type
/// is not made, and one log line says so
entt::entity MakePuzzleGameEntity(const PlannedPuzzleGame& puzzle,
                                  const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// A wonder made again as an abode of its row is (MakeAbodeEntity), of its town, with the power its record keeps and its
/// record kept on it. Its making leaves every player's most tribal power as it was: the players' own records hold it,
/// a built wonder's power in it. One of a row that is not a wonder's is not made, and one log line says so
entt::entity MakeWonderEntity(const PlannedWonder& wonder, uint32_t townKey);
/// A pile of magic food made again as a food pile of its row is (MakePileEntity), with its amount, poison, speed-up and
/// its sparkle, reaction marks and sink as saved, the player its record names and its record kept on it
entt::entity MakeMagicFoodEntity(const PlannedMagicFood& food);

/// The circle of a thing a loaded animal goes round or heads for, as its walk keeps it: the thing's circle at the saved
/// place among its circles (its only circle for -1), once the thing is made; none when the walk holds no thing, the
/// thing is not made or has no circle at that place
[[nodiscard]] std::optional<components::AnimalBrain::HugCircle>
AnimalThingCircleOf(const gamesave::WallHugPart& move, const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// A loaded animal's walk round a circle, as its record keeps it: its turns to the circle and its entry distance, held
/// whether a circle is or not, and the circle it holds: a thing's (`thingCircle`), or water or the land's edge in the
/// middle of the cell its saved centre is in; none otherwise. Nothing changes outside the circling states
void RestoreAnimalCircle(components::AnimalBrain& brain, const gamesave::WallHugPart& move,
                         const std::optional<components::AnimalBrain::HugCircle>& thingCircle);

} // namespace openblack::ecs::saved_world_load
