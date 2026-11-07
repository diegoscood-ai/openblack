/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldReader.h"

#include <cstdio>
#include <cstring>

#include <array>
#include <memory>
#include <utility>
#include <variant>

using namespace openblack::gamesave;

WorldRead::WorldRead() = default;
WorldRead::~WorldRead() = default;
WorldRead::WorldRead(const WorldRead& other) = default;
WorldRead::WorldRead(WorldRead&& other) noexcept = default;
WorldRead& WorldRead::operator=(const WorldRead& other) = default;
WorldRead& WorldRead::operator=(WorldRead&& other) noexcept = default;

void WorldRead::ResizeObjects(size_t count)
{
	objects.resize(count);
}

std::optional<SavedRecord>& WorldRead::AppendObject()
{
	return objects.emplace_back();
}

void openblack::gamesave::PutRecord(std::optional<SavedRecord>& slot, SavedRecord record)
{
	slot.emplace(std::move(record));
}

namespace
{
/// The game object is the stream's first object: it has no reference before its record
constexpr ObjectNumber k_GameObject = 1;

std::string Hex(uint64_t value)
{
	std::array<char, 24> text {};
	std::snprintf(text.data(), text.size(), "0x%llX", static_cast<unsigned long long>(value));
	return text.data();
}

void Keep(WorldRead& world, ObjectNumber number, size_t at, SavedRecord&& record)
{
	if (world.objects.size() < number)
	{
		world.objects.resize(number);
		world.positions.resize(number, 0);
	}
	world.objects[number - 1] = std::move(record);
	world.positions[number - 1] = at;
	world.loadOrder.push_back(number);
}
} // namespace

std::map<uint32_t, uint32_t> WorldRead::Census() const
{
	std::map<uint32_t, uint32_t> census;
	for (const auto& record : objects)
	{
		if (record.has_value())
		{
			++census[TypeOf(*record)];
		}
	}
	return census;
}

std::string openblack::gamesave::TypeName(uint32_t type)
{
	switch (type)
	{
	case GameHead::k_Type:
		return "game";
	case PlayerRecord::k_Type:
		return "player";
	case ComputerPlayerRecord::k_Type:
		return "computer player";
	case ActionStateRecord::k_Type:
		return "computer player's action";
	case ComputerAttitudeRecord::k_Type:
		return "computer player's attitude";
	case GameStatsRecord::k_Type:
		return "game statistics";
	case CitadelRecord::k_Type:
		return "citadel";
	case CitadelHeartRecord::k_Type:
		return "citadel's heart";
	case FootpathLinkRecord::k_Type:
		return "footpath link";
	case FootpathRecord::k_Type:
		return "footpath";
	case FootpathNodeRecord::k_Type:
		return "footpath node";
	case FootpathLinkSaveRecord::k_Type:
		return "footpath link kept";
	case StoragePitRecord::k_Type:
		return "storage pit";
	case WoodPileRecord::k_Type:
		return "pile of wood";
	case FoodPileRecord::k_Type:
		return "pile of food";
	case TownRecord::k_Type:
		return "town";
	case ForestRecord::k_Type:
		return "forest";
	case BigForestRecord::k_Type:
		return "big forest";
	case TreeRecord::k_Type:
		return "tree";
	case AbodeRecord::k_Type:
		return "abode";
	case CrecheRecord::k_Type:
		return "creche";
	case GraveyardRecord::k_Type:
		return "graveyard";
	case FieldRecord::k_Type:
		return "field";
	case VillagerRecord::k_Type:
		return "villager";
	case SpecialVillagerRecord::k_Type:
		return "special villager";
	case TownCentreRecord::k_Type:
		return "town centre";
	case ScriptReminderRecord::k_Type:
		return "script's reminder";
	case MobileStaticRecord::k_Type:
		return "mobile static";
	case FishFarmRecord::k_Type:
		return "fish farm";
	case DataPathRecord::k_Type:
		return "data path";
	case StandardBuildingSiteRecord::k_Type:
		return "standard building site";
	case FlockRecord::k_Type:
		return "flock";
	case TotemStatueRecord::k_Type:
		return "totem statue";
	case MagicWoodRecord::k_Type:
		return "magic wood";
	case WorkshopRecord::k_Type:
		return "workshop";
	case ScaffoldRecord::k_Type:
		return "scaffold";
	case ReactionRecord::k_Type:
		return "reaction";
	case TownDesireFlagsRecord::k_Type:
		return "town's desire flags";
	case PuzzleSheepRecord::k_Type:
		return "puzzle sheep";
	case PuzzleLionRecord::k_Type:
		return "puzzle lion";
	case PuzzleVillagerRecord::k_Type:
		return "puzzle villager";
	case CowRecord::k_Type:
		return "cow";
	case SheepRecord::k_Type:
		return "sheep";
	case GoatRecord::k_Type:
		return "goat";
	case HorseRecord::k_Type:
		return "horse";
	case ZebraRecord::k_Type:
		return "zebra";
	case PigRecord::k_Type:
		return "pig";
	case TortoiseRecord::k_Type:
		return "tortoise";
	case LionRecord::k_Type:
		return "lion";
	case LeopardRecord::k_Type:
		return "leopard";
	case TigerRecord::k_Type:
		return "tiger";
	case WolfRecord::k_Type:
		return "wolf";
	case DoveRecord::k_Type:
		return "dove";
	case CrowRecord::k_Type:
		return "crow";
	case SwallowRecord::k_Type:
		return "swallow";
	case PigeonRecord::k_Type:
		return "pigeon";
	case SeagullRecord::k_Type:
		return "seagull";
	case BatRecord::k_Type:
		return "bat";
	case VultureRecord::k_Type:
		return "vulture";
	case PieceLionRecord::k_Type:
		return "piece lion";
	case PieceSheepRecord::k_Type:
		return "piece sheep";
	case PieceWolfRecord::k_Type:
		return "piece wolf";
	case PieceVillagerRecord::k_Type:
		return "piece villager";
	case PlannedCitadelHeartRecord::k_Type:
		return "planned citadel heart";
	case PlannedAbodeRecord::k_Type:
		return "planned abode";
	case PlannedTownCentreRecord::k_Type:
		return "planned town centre";
	case SpellDispenserRecord::k_Type:
		return "spell dispenser";
	case CitadelEntranceRecord::k_Type:
		return "citadel's entrance";
	case OneOffSpellSeedRecord::k_Type:
		return "one-off spell's seed";
	case PlayerInterfaceRecord::k_Type:
		return "player's interface";
	case InterfaceStatusRecord::k_Type:
		return "interface's status";
	case LeashStatusRecord::k_Type:
		return "leash's status";
	case ComputerSeenRecord::k_Type:
		return "computer player's sight";
	case PlayerSubActionRecord::k_Type:
		return "player's sub-action";
	case PlayerSubActionArgumentRecord::k_Type:
		return "sub-action's argument";
	case ParticleContainerRecord::k_Type:
		return "particle container";
	case ParticleInterfaceRecord::k_Type:
		return "particle interface";
	case CreatureRecord::k_Type:
		return "creature";
	case FireEffectRecord::k_Type:
		return "fire";
	case RockRecord::k_Type:
		return "rock";
	case SpellSeedRecord::k_Type:
		return "spell's seed";
	case WorshipSiteRecord::k_Type:
		return "worship site";
	case ShowNeedsRecord::k_Type:
		return "worship site's needs";
	case ShowNeedsVisualsRecord::k_Type:
		return "needs' visual";
	case WorshipTotemRecord::k_Type:
		return "worship totem";
	case TownArtifactRecord::k_Type:
		return "town's artifact";
	case DanceRecord::k_Type:
		return "dance";
	case DanceKeyFrameRecord::k_Type:
		return "dance's key frame";
	case DanceKeyActionRecord::k_Type:
		return "key frame's action";
	case DanceGroupRecord::k_Type:
		return "dance's group";
	case SpellDoveRecord::k_Type:
		return "spell's dove";
	case SpellBatRecord::k_Type:
		return "spell's bat";
	case SpellWolfRecord::k_Type:
		return "spell's wolf";
	case ParticleManagerRecord::k_Type:
		return "particle manager";
	case AtomCollectionRecord::k_Type:
		return "particles' atom collection";
	case TownSpellIconRecord::k_Type:
		return "town's spell icon";
	case TownCentreSpellIconRecord::k_Type:
		return "town centre's spell icon";
	case WorshipSpellIconRecord::k_Type:
		return "worship site's spell icon";
	case WonderRecord::k_Type:
		return "wonder";
	case ScriptHighlightRecord::k_Type:
		return "script's highlight";
	case MagicFoodRecord::k_Type:
		return "magic food";
	case WorkshopBuildingSiteRecord::k_Type:
		return "workshop's building site";
	case DeadTreeRecord::k_Type:
		return "dead tree";
	case FelledTreeRecord::k_Type:
		return "felled tree";
	case PotRecord::k_Type:
		return "pot";
	case StreetLanternRecord::k_Type:
		return "street lantern";
	case StreetLightRecord::k_Type:
		return "street light";
	case PooRecord::k_Type:
		return "poo";
	case AtomCoreRecord::k_Type:
		return "atom core";
	case ParticleSpriteRecord::k_Type:
		return "particle sprite";
	case BonfireRecord::k_Type:
		return "bonfire";
	case FireFlyRecord::k_Type:
		return "fire fly";
	case ParticlePointRecord::k_Type:
		return "particle point";
	case ScriptMarkerRecord::k_Type:
		return "script marker";
	case ArenaRecord::k_Type:
		return "arena";
	case BeamSubCollectionRecord::k_Type:
		return "beam sub collection";
	case EmitterCollectionRecord::k_Type:
		return "emitter collection";
	case MobileObjectRecord::k_Type:
		return "mobile object";
	case HanoiBlockRecord::k_Type:
		return "hanoi block";
	case RewardRecord::k_Type:
		return "reward";
	case ParticleChainJointRecord::k_Type:
		return "particle chain joint";
	case MistRecord::k_Type:
		return "mist";
	case StreamRecord::k_Type:
		return "stream";
	case InfluenceRingRecord::k_Type:
		return "influence ring";
	case WaypointRecord::k_Type:
		return "waypoint";
	case FeatureRecord::k_Type:
		return "feature";
	case WeatherThingRecord::k_Type:
		return "weather thing";
	case AnimatedStaticRecord::k_Type:
		return "animated static";
	case ClimateRecord::k_Type:
		return "climate";
	case SpellWaterRecord::k_Type:
		return "spell water";
	case SpellShieldRecord::k_Type:
		return "spell shield";
	case SpellRecord::k_Type:
		return "spell";
	case SpellStormAndTornadoRecord::k_Type:
		return "spell storm and tornado";
	case SpellHealRecord::k_Type:
		return "spell heal";
	case SpellResourceRecord::k_Type:
		return "spell resource";
	case SpellWithObjectsRecord::k_Type:
		return "spell with objects";
	case SpellTeleportRecord::k_Type:
		return "spell teleport";
	case SpellFlockGroundRecord::k_Type:
		return "spell flock on the ground";
	case SpellFlockFlyingRecord::k_Type:
		return "spell flock flying";
	case SpellForestRecord::k_Type:
		return "spell forest";
	case SpellCreatureRecord::k_Type:
		return "spell creature";
	case HandSprinkleCollectionRecord::k_Type:
		return "hand sprinkle collection";
	case PhysicalShieldRecord::k_Type:
		return "physical shield";
	case LandscapeVortexInRecord::k_Type:
		return "landscape vortex in";
	case SoundAtomDataRecord::k_Type:
		return "sound atom data";
	case ParticleTexturedObjectRecord::k_Type:
		return "particle textured object";
	case ChainRecord::k_Type:
		return "chain";
	case ParticleMistRecord::k_Type:
		return "particle mist";
	case BeamAtomDataRecord::k_Type:
		return "beam atom data";
	case ParticleSoundRecord::k_Type:
		return "particle sound";
	case TargetAtomsCollectionRecord::k_Type:
		return "target atoms collection";
	case TrailCollectionRecord::k_Type:
		return "trail collection";
	case PuzzleGameRecord::k_Type:
		return "puzzle game";
	case VortexSaveRecord::k_Type:
		return "vortex save";
	case VortexAttractAtomDataRecord::k_Type:
		return "vortex attract atom data";
	case ParticleGameObjectRecord::k_Type:
		return "particle game object";
	case CameraRecord::k_Type:
		return "camera";
	case ScriptTimerRecord::k_Type:
		return "script's timer";
	case BaseOnlyRecord::k_Type:
		return "object of no other class";
	case ThingMusicInfoRecord::k_Type:
		return "thing's music";
	case WillowWispCollectionRecord::k_Type:
		return "willow wisp's collection";
	case FadeOutAtomDataRecord::k_Type:
		return "fading atom's data";
	default:
		return "class " + Hex(type);
	}
}

WorldRead openblack::gamesave::ReadWorld(std::span<const uint8_t> bytes)
{
	WorldRead world;
	const auto framing = ReadVanillaFraming(bytes);
	if (!framing.has_value())
	{
		world.stop = "the file does not start as a world file";
		return world;
	}
	world.framing = *framing;
	StreamReader reader(bytes, {.objects = k_GameObject, .checksum = framing->checksum}, framing->body);

	// Each new object's record follows its number: its class's number and its own word, then the class's values
	ReadNewObject readNew;
	readNew = [&world, &readNew](StreamReader& stream, ObjectNumber number) -> bool {
		const auto at = stream.Position();
		uint32_t type = 0;
		uint32_t extra = 0;
		if (!stream.Get(type) || !stream.Get(extra))
		{
			return false;
		}
		if (!IsKnownType(type))
		{
			// The part every record starts with is known: its stored sum proves what was read before it
			uint32_t stored = 0;
			stream.GetChecksum(stored);
			world.unreadType = type;
			world.unreadNumber = number;
			world.stoppedAt = at;
			world.stop = TypeName(type) + " (object " + std::to_string(number) + ") at byte " + Hex(at) + " is not read yet";
			stream.SetUnknownType(type);
			return false;
		}
		// On the heap: the walk reads records inside records, and a record can be thousands of bytes
		auto record = std::make_unique<SavedRecord>();
		const auto read = ReadRecord(stream, type, extra, *record, readNew);
		Keep(world, number, at, std::move(*record));
		return read;
	};

	const auto at = reader.Position();
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	if (type != k_GameType)
	{
		world.stoppedAt = at;
		world.stop = "the first object is not the game object";
	}
	else
	{
		SavedRecord game;
		const auto read = ReadRecord(reader, type, extra, game, readNew);
		Keep(world, k_GameObject, at, std::move(game));
		if (read)
		{
			world.stoppedAt = reader.Position();
			world.stop = reader.Position() == bytes.size()
			                 ? std::string(k_ReadToTheEnd)
			                 : "the game object ends " + std::to_string(bytes.size() - reader.Position()) +
			                       " bytes before the end of the file";
		}
	}
	world.sumsRead = reader.SumsRead();
	world.firstWrongSum = reader.FirstWrongSum();
	if (world.stop.empty())
	{
		world.stoppedAt = reader.Position();
		world.stop = "the file ends or a reference is not valid at byte " + Hex(reader.Position());
	}
	return world;
}

std::vector<uint8_t> openblack::gamesave::WriteWorld(const WorldRead& world)
{
	auto bytes = WriteVanillaFraming(world.framing.landscape);
	std::memcpy(bytes.data(), &world.framing.attempts, sizeof(world.framing.attempts));
	if (world.objects.empty() || !world.objects.front().has_value())
	{
		return bytes;
	}
	StreamWriter writer({.objects = k_GameObject, .checksum = world.framing.checksum});
	// The writer stops where the read stopped: at the number of the first object whose record it does not have
	std::optional<size_t> stopAt;
	WriteNewObject writeNew;
	writeNew = [&world, &writeNew, &stopAt](StreamWriter& stream, ObjectNumber number) {
		if (stopAt.has_value())
		{
			return;
		}
		if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			stopAt = stream.Bytes().size();
			return;
		}
		Write(stream, *world.objects[number - 1], writeNew);
	};
	Write(writer, *world.objects.front(), writeNew);
	auto body = writer.Take();
	if (stopAt.has_value())
	{
		body.resize(*stopAt);
	}
	// A record the read stopped inside writes only what was read
	if (world.stoppedAt >= bytes.size() && body.size() > world.stoppedAt - bytes.size())
	{
		body.resize(world.stoppedAt - bytes.size());
	}
	bytes.insert(bytes.end(), body.begin(), body.end());
	return bytes;
}
