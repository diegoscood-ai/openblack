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
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "AnimalRecords.h"
#include "BuildingRecords.h"
#include "CreatureRecords.h"
#include "EffectRecords.h"
#include "ForestRecords.h"
#include "GameTailRecords.h"
#include "InterfaceRecords.h"
#include "ParticleRecords.h"
#include "PlannedRecords.h"
#include "SaveParts.h"
#include "SpellIconRecords.h"
#include "TownRecords.h"
#include "VillageRecords.h"
#include "VillagerRecords.h"
#include "WorldListRecords.h"
#include "WorshipRecords.h"

// The records of the world file's objects, in the game's layout and order. Every record starts with the part every
// object writes (its class's number, a word of its own, the running sum, whether it is destroyed and its flags); then
// each class adds its values, the base class first. A reference to another object is its number in the stream: an
// object new to the stream has its whole record at once after its number, so each reader and writer of a record takes
// the function that reads or writes the records of the new objects it meets.
namespace openblack::gamesave
{

/// The game's numbers of the classes whose records are here
inline constexpr uint32_t k_GameType = 0x6A;
inline constexpr uint32_t k_PlayerType = 0x63;
inline constexpr uint32_t k_ComputerPlayerType = 0x66;
inline constexpr uint32_t k_ActionStateType = 0xF9;
inline constexpr uint32_t k_ComputerAttitudeType = 0x10D;
inline constexpr uint32_t k_GameStatsType = 0x9F;

/// The revision text the game object writes first
inline constexpr std::string_view k_GameRevision = "$Revision: 756 $";
/// The players of a game
inline constexpr size_t k_PlayerCount = 8;
/// The nodes of a computer player's tree of desires, in the order the game writes them
inline constexpr size_t k_DesireNodeCount = 198;

/// The global lists the game object writes after its players, in the game's order
inline constexpr size_t k_GlobalListCount = 44;

/// The game object's record: its players, whose records hold, depth first, most of the world; its data; three bytes
/// and a word; the global lists, which hold the rest; ten words; and its tail, to the end of the file.
struct GameHead
{
	static constexpr uint32_t k_Type = k_GameType;
	ThingPart thing;
	std::string revision {k_GameRevision};
	uint32_t gameFlags {0};
	uint32_t landNumber {0};
	std::array<ObjectNumber, k_PlayerCount> players {};
	/// The game's data: eight words
	std::array<uint32_t, 8> data {};
	/// The local player, the curse of its creature and a byte
	std::array<uint8_t, 3> bytes {};
	uint32_t word {0};
	std::array<RefList, k_GlobalListCount> lists {};
	/// Ten words, the game's visual time among them
	std::array<uint32_t, 10> words {};
	/// The help system and everything after it, to the end of the file
	GameTail tail;

	bool operator==(const GameHead&) const = default;
};

/// A player's alignment as the game writes it: its whole object, with a word it leaves from its memory
struct AlignmentPart
{
	/// The same in every save of the exe: a value of the game's code, not of the player
	uint32_t classWord {0};
	uint32_t destroyed {0};
	float value {0.0f};
	float change {0.0f};

	bool operator==(const AlignmentPart&) const = default;
};
static_assert(sizeof(AlignmentPart) == 16);

/// Where a player starts
struct StartPosition
{
	MapCoords position {};
	uint32_t first {0};
	int32_t second {0};

	bool operator==(const StartPosition&) const = default;
};
static_assert(sizeof(StartPosition) == 20);

/// A player's record
struct PlayerRecord
{
	static constexpr uint32_t k_Type = k_PlayerType;
	ThingPart thing;
	uint8_t playerNumber {0};
	AlignmentPart alignment;
	uint32_t info {0};
	std::vector<float> tribalPower;
	std::vector<float> maxTribalPower;
	uint32_t type {0};
	/// The name's 32 characters, its zeros included
	std::vector<char16_t> name;
	uint32_t userId {0};
	uint32_t cheatOn {0};
	uint32_t windResistance {0};
	uint32_t allMagicEnabled {0};
	uint32_t cheatWord {0};
	uint32_t infiniteInfluence {0};
	ObjectNumber computerPlayer {0};
	uint32_t totalPopulation {0};
	StartPosition start;
	std::vector<int32_t> magicTypeEnabled;
	/// One byte for each kind of miracle
	std::vector<uint8_t> magicTypeEverBeenEnabled;
	ObjectNumber gameStats {0};
	ObjectNumber citadel {0};
	ObjectNumber creature {0};
	std::vector<ObjectNumber> towns;
	std::vector<ObjectNumber> teleports;
	float influencePower {0.0f};
	float averageInfluencePower {0.0f};
	std::vector<float> damageFromPlayer;
	std::vector<float> allies;
	std::vector<ObjectNumber> interfaces;
	uint32_t hasLost {0};
	uint32_t lastWord {0};

	bool operator==(const PlayerRecord&) const = default;
};

/// A node of a computer player's tree of desires: how much it is held back and how much the player wants it
struct DesireNodeValues
{
	float suppression {1.0f};
	float personality {0.0f};

	bool operator==(const DesireNodeValues&) const = default;
};

/// A computer player's record. Its tree of desires has no count in the stream: the game writes every node of its
/// fixed tree, depth first.
struct ComputerPlayerRecord
{
	static constexpr uint32_t k_Type = k_ComputerPlayerType;
	ThingPart thing;
	PositionPart position;
	ObjectNumber player {0};
	ObjectNumber actionState {0};
	/// Three lists of objects, each its count and then a reference for each object
	std::array<std::vector<ObjectNumber>, 3> lists {};
	/// Four words the reader keeps as they are, and a word every computer player writes from the whole game
	std::array<uint32_t, 4> words {};
	uint32_t gameWord {0};
	std::vector<uint32_t> table;
	uint32_t tableWord {0};
	std::array<int32_t, 3> numbers {};
	uint8_t byte {0};
	std::vector<ObjectNumber> attitudes;
	std::array<DesireNodeValues, k_DesireNodeCount> desires {};

	bool operator==(const ComputerPlayerRecord&) const = default;
};

/// The record of what a computer player's hand is doing
struct ActionStateRecord
{
	static constexpr uint32_t k_Type = k_ActionStateType;
	ThingPart thing;
	PositionPart position;
	ObjectNumber computerPlayer {0};
	std::vector<ObjectNumber> objects;
	/// Eight words and a value of twelve bytes the reader keeps as they are
	std::array<uint32_t, 8> words {};
	std::array<uint8_t, 12> block {};
	Point point {};
	float first {0.0f};
	ObjectNumber object {0};
	float second {0.0f};
	Point otherPoint {};

	bool operator==(const ActionStateRecord&) const = default;
};

/// A computer player's attitude to one player
struct ComputerAttitudeRecord
{
	static constexpr uint32_t k_Type = k_ComputerAttitudeType;
	ThingPart thing;
	float attitude {1.0f};

	bool operator==(const ComputerAttitudeRecord&) const = default;
};

/// A graph of the game statistics: the samples, then where it is
struct StatsGraph
{
	std::array<float, 500> data {};
	uint32_t size {500};
	uint32_t index {0};
	uint16_t samplesPerSlot {50};
	uint16_t samplesInSlot {0};

	bool operator==(const StatsGraph&) const = default;
};
static_assert(sizeof(StatsGraph) == 2012);

/// A player's game statistics, in the order the game writes them. The words of the game's whole run come between
/// them: the players that left, and later the lines of script run, the frame rates, the allocations, the start time
/// and the statistics' version.
struct GameStatsRecord
{
	static constexpr uint32_t k_Type = k_GameStatsType;
	ThingPart thing;
	ObjectNumber player {0};
	uint32_t playersThatLeft {0};
	uint32_t position {0};
	uint32_t lossReason {0};
	/// The towns' figures, written as one value
	std::array<uint32_t, 10> townStats {};
	/// From the births to the wood used: the population, the converts, the satisfaction, the buildings and the
	/// disciples (the satisfaction is a float, kept as its bits)
	std::array<uint32_t, 25> counts {};
	StatsGraph influence;
	StatsGraph population;
	/// From the gods defeated to the buildings destroyed, and three words after them
	std::array<uint32_t, 11> results {};
	/// The lines of script run, the highest and lowest frame rates, the allocations, the start time and the version
	std::array<uint32_t, 6> run {};
	/// How many times each miracle was cast
	std::array<uint32_t, 36> casts {};
	/// The chants used (a float, kept as its bits), the sacrifices and the towns at the start
	std::array<uint32_t, 3> last {};

	bool operator==(const GameStatsRecord&) const = default;
};

/// The records of every class the reader knows
using SavedRecordVariant = std::variant<
    GameHead, PlayerRecord, ComputerPlayerRecord, ActionStateRecord, ComputerAttitudeRecord, GameStatsRecord, CitadelRecord,
    CitadelHeartRecord, FootpathLinkRecord, FootpathRecord, FootpathNodeRecord, FootpathLinkSaveRecord, StoragePitRecord,
    WoodPileRecord, FoodPileRecord, TownRecord, ForestRecord, BigForestRecord, TreeRecord, AbodeRecord, CrecheRecord,
    GraveyardRecord, FieldRecord, VillagerRecord, SpecialVillagerRecord, TownCentreRecord, ScriptReminderRecord,
    MobileStaticRecord, FishFarmRecord, DataPathRecord, StandardBuildingSiteRecord, FlockRecord, TotemStatueRecord,
    MagicWoodRecord, WorkshopRecord, ScaffoldRecord, ReactionRecord, TownDesireFlagsRecord, PuzzleSheepRecord, PuzzleLionRecord,
    PuzzleVillagerRecord, CowRecord, SheepRecord, GoatRecord, HorseRecord, ZebraRecord, PigRecord, TortoiseRecord, LionRecord,
    LeopardRecord, TigerRecord, WolfRecord, DoveRecord, CrowRecord, SwallowRecord, PigeonRecord, SeagullRecord, BatRecord,
    VultureRecord, PieceLionRecord, PieceSheepRecord, PieceWolfRecord, PieceVillagerRecord, PlannedCitadelHeartRecord,
    PlannedAbodeRecord, PlannedTownCentreRecord, SpellDispenserRecord, CitadelEntranceRecord, OneOffSpellSeedRecord,
    PlayerInterfaceRecord, InterfaceStatusRecord, LeashStatusRecord, ComputerSeenRecord, PlayerSubActionRecord,
    PlayerSubActionArgumentRecord, ParticleContainerRecord, ParticleInterfaceRecord, CreatureRecord, FireEffectRecord,
    RockRecord, SpellSeedRecord, WorshipSiteRecord, ShowNeedsRecord, ShowNeedsVisualsRecord, WorshipTotemRecord,
    TownArtifactRecord, DanceRecord, DanceKeyFrameRecord, DanceKeyActionRecord, DanceGroupRecord, SpellDoveRecord,
    SpellBatRecord, SpellWolfRecord, ParticleManagerRecord, AtomCollectionRecord, TownSpellIconRecord,
    TownCentreSpellIconRecord, WorshipSpellIconRecord, WonderRecord, ScriptHighlightRecord, MagicFoodRecord,
    WorkshopBuildingSiteRecord, DeadTreeRecord, FelledTreeRecord, PotRecord, StreetLanternRecord, StreetLightRecord, PooRecord,
    AtomCoreRecord, ParticleSpriteRecord, BonfireRecord, FireFlyRecord, ParticlePointRecord, ScriptMarkerRecord, ArenaRecord,
    BeamSubCollectionRecord, EmitterCollectionRecord, MobileObjectRecord, HanoiBlockRecord, RewardRecord,
    ParticleChainJointRecord, MistRecord, StreamRecord, InfluenceRingRecord, WaypointRecord, FeatureRecord, WeatherThingRecord,
    AnimatedStaticRecord, ClimateRecord, SpellWaterRecord, SpellShieldRecord, SpellRecord, SpellStormAndTornadoRecord,
    SpellHealRecord, SpellResourceRecord, SpellWithObjectsRecord, SpellTeleportRecord, SpellFlockGroundRecord,
    SpellFlockFlyingRecord, SpellForestRecord, SpellCreatureRecord, HandSprinkleCollectionRecord, PhysicalShieldRecord,
    LandscapeVortexInRecord, SoundAtomDataRecord, ParticleTexturedObjectRecord, ChainRecord, ParticleMistRecord,
    BeamAtomDataRecord, ParticleSoundRecord, TargetAtomsCollectionRecord, TrailCollectionRecord, PuzzleGameRecord,
    VortexSaveRecord, VortexAttractAtomDataRecord, ParticleGameObjectRecord, CameraRecord, ScriptTimerRecord, BaseOnlyRecord,
    ThingMusicInfoRecord, WillowWispCollectionRecord, FadeOutAtomDataRecord>;

/// The record of an object of a class that the reader knows. Its copies, moves, assignments, destruction and
/// comparison are made once, in the library: over every class the reader knows each of them is a large function, and
/// a file that made its own would compile it again
struct SavedRecord: SavedRecordVariant
{
	using SavedRecordVariant::SavedRecordVariant;
	using SavedRecordVariant::operator=;

	SavedRecord();
	SavedRecord(const SavedRecord& other);
	SavedRecord(SavedRecord&& other) noexcept;
	SavedRecord& operator=(const SavedRecord& other);
	SavedRecord& operator=(SavedRecord&& other) noexcept;
	~SavedRecord();
};

/// Two records are equal when they are of the same class and their values are
[[nodiscard]] bool operator==(const SavedRecord& left, const SavedRecord& right);

/// The game's number of the record's class
[[nodiscard]] uint32_t TypeOf(const SavedRecord& record);
/// Whether the reader has the layout of the class
[[nodiscard]] bool IsKnownType(uint32_t type);

/// Writes a whole record from its class's number. The game object writes only its head here: what follows its players
/// is not modelled yet.
void Write(StreamWriter& writer, const SavedRecord& record, const WriteNewObject& writeNew);
/// Reads the rest of a record whose class's number and own word the reader read
bool ReadRecord(StreamReader& reader, uint32_t type, uint32_t extra, SavedRecord& record, const ReadNewObject& readNew);

} // namespace openblack::gamesave
