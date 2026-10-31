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
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include <AnimalRecords.h>
#include <BuildingRecords.h>
#include <EffectRecords.h>
#include <ForestRecords.h>
#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>
#include <WorshipRecords.h>
#include <entt/entity/entity.hpp>

#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Weather/Storms.h"
#include "Enums.h"

namespace openblack::gamesave
{
struct WorldRead;
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

/// A reaction of the save, by its number in the world file, with the player its record names (none for its null
/// player)
struct PlannedReaction
{
	gamesave::ObjectNumber number {0};
	gamesave::ReactionRecord record;
	std::optional<PlayerNames> player;
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

/// A pile of wood or of food of the save, by its number in the world file, with the type its row names
struct PlannedPile
{
	gamesave::ObjectNumber number {0};
	PotInfo type {PotInfo::FoodPot};
	/// The parts both kinds of pile write
	gamesave::MobileObjectFields base;
	gamesave::PotFields pile;
	/// The record, whole
	std::variant<gamesave::WoodPileRecord, gamesave::FoodPileRecord> record;
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

/// An abode, a creche or a graveyard of the save, by its number in the world file, with the type its row names
struct PlannedAbode
{
	gamesave::ObjectNumber number {0};
	AbodeInfo type {AbodeInfo::CelticHut};
	/// The parts the three classes write
	gamesave::MultiMapFixedFields base;
	gamesave::AbodeFields abode;
	/// The record, whole
	std::variant<gamesave::AbodeRecord, gamesave::CrecheRecord, gamesave::GraveyardRecord> record;
};

/// A rock or another mobile static of the save, by its number in the world file, with the type its row names
struct PlannedMobileStatic
{
	gamesave::ObjectNumber number {0};
	MobileStaticInfo type {MobileStaticInfo::Rock};
	/// The parts both classes write
	gamesave::MultiMapFixedFields base;
	gamesave::MobileStaticPart mobileStatic;
	/// A rock's player, by its player record's number; none for another mobile static
	std::optional<PlayerNames> player;
	/// The record, whole
	std::variant<gamesave::RockRecord, gamesave::MobileStaticRecord> record;
};

/// A villager or a special villager of the save, by its number in the world file, with the type its row names
struct PlannedVillager
{
	gamesave::ObjectNumber number {0};
	VillagerInfo type {VillagerInfo::None};
	std::variant<gamesave::VillagerRecord, gamesave::SpecialVillagerRecord> record;
};

/// One of a town's desire flags of the save, by its number in the world file
struct PlannedDesireFlag
{
	gamesave::ObjectNumber number {0};
	gamesave::TownDesireFlagsRecord record;
};

/// An object the load makes, of a class it knows how to make
using PlannedObject = std::variant<PlannedTree, PlannedBigForest, PlannedPile, PlannedStoragePit, PlannedCitadelHeart,
                                   PlannedAbode, PlannedDesireFlag, PlannedMobileStatic>;

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
	/// The fires as the game's list holds them, the newest first, and the two tags as the last fire read leaves them
	std::vector<PlannedFire> fires;
	/// The worship sites, in the order their reads ended
	std::vector<PlannedWorshipSite> worshipSites;
	/// The dances as the game's list holds them, the newest first, and their key frames by number
	std::vector<PlannedDance> dances;
	std::map<gamesave::ObjectNumber, gamesave::DanceKeyFrameRecord> keyFrames;
	std::array<uint8_t, 2> fireTags {};
	/// The arenas as the game's arena list holds them, the newest first
	std::vector<PlannedArena> arenas;
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
	/// A dance of the plan, once its dancers and what it is danced about are made (`objectOf` gives the entity made of
	/// an object of the file); entt::null when it is not made
	std::function<entt::entity(const PlannedDance& dance, const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    dance;
	/// Every fire of the plan at once, newest first, once every object is made (`objectOf` gives the entity made of an
	/// object of the file, or none) with the tags as the last fire read left them; each one's handle, 0 for one not made
	std::function<std::vector<uint32_t>(const std::vector<PlannedFire>& fires, const std::array<uint8_t, 2>& tags,
	                                    const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)>
	    fires;
	/// An arena; made from the list's last to its first, so that the newest is made last
	std::function<entt::entity(const PlannedArena& arena)> arena;
	/// A reaction, with the entities its initiator and its dance are now (null for one not made); made from the
	/// list's last to its first; its id, 0 for none
	std::function<uint32_t(const PlannedReaction& reaction, entt::entity initiator, entt::entity dance)> reaction;
};

/// Makes the plan's forests in the forest list (each with its saved id, scenic flag and record), puts back the two
/// values of the whole game, makes the towns through `makers` and gives each its forest list, then makes the objects
/// in their order, each of the forest its record names (by the key the forest list gave it), and last gives each
/// forest the big forest its record names. Of every class of the save, how many were made. With `loaded`, each entity
/// made is added to it by its number in the file and the class of its record, for the next save's writer
Census Apply(const WorldPlan& plan, const Makers& makers, saved_world_write::LoadedObjects* loaded = nullptr);

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
/// 3D object, which a load makes afresh); its record kept on it
entt::entity MakeAbodeEntity(const PlannedAbode& abode, uint32_t townKey);
/// The game's desire flag: the desire flag archetype of its town, its desire, its slot's point of the store, its mesh
/// and its object scale, at its saved place; the place it was made at, its height over the ground and its desire from
/// the save; its creation number and record kept. Its town keeps it among its flags
entt::entity MakeDesireFlagEntity(const PlannedDesireFlag& flag, entt::entity town);
/// The game's rock or mobile static: the mobile static archetype at its saved place, raised by its saved altitude over
/// the land, of its type, at its saved X, Y and Z angles and size; a rock of its saved player, a mobile static's built
/// percentage when it is not 0; its saved creation number and life, and its record kept on it
entt::entity MakeMobileStaticEntity(const PlannedMobileStatic& object);
/// The game's villager: the villager archetype's restored villager of its row at its saved place, angle and size,
/// with its saved values (its flags, food, last check, disciple, held food and wood, pregnancy, birth, life, states,
/// walk, specular colour), its counter as drawn or saved, its creation number and its record kept, put in the map with
/// its first clip; its home, town and mother are given once every villager is made. One the load does not make yet
/// (held, in flight, on a footpath or a data path, in a flock or a reaction, walking round something, or in a
/// state that loads values) is not made, and one log line says why
entt::entity MakeVillagerEntity(const PlannedVillager& villager, std::optional<uint16_t> counter);
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
/// tags the fires take and are processed by. A fire whose object was not made is not made. Its reaction is made again
/// by its first turn, as the reactions are not loaded yet
std::vector<uint32_t> MakeFires(const std::vector<PlannedFire>& fires, const std::array<uint8_t, 2>& tags,
                                const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
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
/// The game's makers: MakeFootpathEntity, MakeFootpathLinkEntity, MakeTownEntity, MakeTreeEntity, MakeBigForestEntity,
/// MakePileEntity and MakeStoragePitEntity
[[nodiscard]] Makers GameMakers();

} // namespace openblack::ecs::saved_world_load
