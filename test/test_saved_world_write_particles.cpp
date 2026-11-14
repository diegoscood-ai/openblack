/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spot visuals' containers, with their effects' particle interfaces and managers, written from the particle
// engine: a live container and its effect as the game saves them, the game's list of containers with those a load
// kept after the live ones, and the frozen Land 1 autosave, whose two bonfires' containers no load makes yet, writes
// back byte for byte (docs/wiki/engine/saved-games.md#openblack)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <InterfaceRecords.h>
#include <ObjectStream.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <VanillaSaveFile.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/LoadParticles.h"
#include "ECS/SavedWorld/ParticleCodecs.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/Systems/GlintTargetsInterface.h"
#include "ECS/Weather/Storms.h"
#include "Locator.h"
#include "Particles/BeamData.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/ParticleRegistry.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/Lightning.h"
#include "Particles/Rules/ManaPath.h"
#include "Particles/Rules/VortexAttract.h"
#include "Particles/VolumeGlowData.h"
#include "support/FileSystemFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::ObjectNumber;
using gamesave::ParticleContainerRecord;
using gamesave::ParticleInterfaceRecord;
using gamesave::ParticleManagerRecord;
using gamesave::SavedRecord;

namespace
{
constexpr std::string_view k_Land = ".\\DATA\\LANDSCAPE\\Land1.lnd";
constexpr uint32_t k_Effect = 7;
/// The magic beam's particle type, SF_SimpleBeam's
constexpr uint32_t k_MagicBeamType = 119;

/// The bytes the game writes for the records given, by their numbers in the file
std::vector<uint8_t> FileOf(const std::map<ObjectNumber, SavedRecord>& records)
{
	auto bytes = gamesave::WriteVanillaFraming(k_Land);
	const auto framing = gamesave::ReadVanillaFraming(bytes);
	gamesave::StreamWriter writer({.objects = 1, .checksum = framing->checksum});
	gamesave::WriteNewObject writeNew;
	writeNew = [&](gamesave::StreamWriter& stream, ObjectNumber number) {
		gamesave::Write(stream, records.at(number), writeNew);
	};
	gamesave::Write(writer, records.at(1), writeNew);
	const auto body = writer.Take();
	bytes.insert(bytes.end(), body.begin(), body.end());
	return bytes;
}

/// The game object a new game's writer makes: the game's list of containers, by its writer, in its first list
void RegisterNewGame(ClassWriters& writers)
{
	writers.Register(gamesave::k_GameType, {},
	                 [](WriteContext& context, entt::entity /*unused*/) -> std::optional<SavedRecord> {
		                 gamesave::GameHead game;
		                 const auto* list = context.Writers().ListWriterOf(GlobalList::ParticleContainers);
		                 if (list != nullptr)
		                 {
			                 game.lists[0].numbers = (*list)(context);
		                 }
		                 return game;
	                 });
}

/// A spot visual's container hanging on a thing, its effect started from a file of no parts at a point, drawn as one
void StartContainer(particles::manager::State& state, entt::entity thing)
{
	auto file = std::make_shared<particles::File>();
	file->name = "SF_Bonfire";
	auto& running = state.effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	running.inContainer = true;
	running.path = particles::DrawPath::Queued;
	state.containers.push_front({.effect = k_Effect,
	                             .object = entt::null,
	                             .owner = thing,
	                             .turns = -1,
	                             .hadOwner = true,
	                             .coords = {.x = 0x1000000, .z = 0x2000000, .altitude = 27.5f},
	                             .placed = glm::vec3(0.0f),
	                             .particleType = ParticleType::Bonfire});
}

/// The manager a new spot visual's effect has, at the point, for the file of the particle type
ParticleManagerRecord NewManager()
{
	ParticleManagerRecord manager;
	const float strength = 1.0f;
	std::memcpy(manager.processInfo.data() + 48, &strength, sizeof(strength));
	manager.processInfo[56] = 1;
	manager.point = {1803.5f, 27.5f, 2645.5f};
	manager.byte = 255;
	manager.points[2] = {1.0f, 0.0f, 0.0f};
	manager.words[1] = std::bit_cast<uint32_t>(1.0f);
	manager.bytes = {1, 1};
	manager.persistent = {.first = static_cast<uint32_t>(ParticleType::Bonfire), .second = 0xFFFFFFFF};
	return manager;
}

/// A spell file of one atom whose emitter makes more atoms ten times a second
constexpr std::string_view k_EmitterFile =
    "BEGINPROPERTIES\n"
    "PROPERTY DeleteOnCloseDown BOOL 0\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS ParticlePointCreator P\n"
    "BEGINPROPERTIES\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n"
    "BEGINCLASS CreateRuleAnAtom A\n"
    "BEGINPROPERTIES\n"
    "PROPERTY Group INTEGER 0\n"
    "PROPERTY NextGroups ARRAY SIZE 1 1\n"
    "PROPERTY PCreator PERSIS_PNTR P\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n"
    "BEGINCLASS EmitterRuleSimple E\n"
    "BEGINPROPERTIES\n"
    "PROPERTY Group INTEGER 1\n"
    "PROPERTY NextGroups ARRAY SIZE 0\n"
    "PROPERTY PCreator PERSIS_PNTR P\n"
    "PROPERTY EmissionFreq FLOAT 10\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n";

/// A chain of four joints made once by its collection's rule
constexpr std::string_view k_ChainFile =
    "BEGINPROPERTIES\n"
    "PROPERTY DeleteOnCloseDown BOOL 0\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS CreateRuleMakeChain M\n"
    "BEGINPROPERTIES\n"
    "PROPERTY Group INTEGER 0\n"
    "PROPERTY NumAtoms INTEGER 4\n"
    "PROPERTY PCreator PERSIS_PNTR C\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n"
    "BEGINCLASS ParticleChainCreator C\n"
    "BEGINPROPERTIES\n"
    "PROPERTY FrameHeight INTEGER 256\n"
    "PROPERTY FrameWidth INTEGER 64\n"
    "PROPERTY NumTexturesForWholeChain INTEGER 4\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n";

/// A simple beam of two beams of three joints through four key points
constexpr std::string_view k_BeamFile =
    "BEGINPROPERTIES\n"
    "PROPERTY DeleteOnCloseDown BOOL 0\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS UR_SimpleBeam B\n"
    "BEGINPROPERTIES\n"
    "PROPERTY BeamGroup INTEGER 1\n"
    "PROPERTY Group INTEGER 0\n"
    "PROPERTY MaxJointsPerFork INTEGER 3\n"
    "PROPERTY MinHeight FLOAT -1000\n"
    "PROPERTY NumBeams INTEGER 2\n"
    "PROPERTY NumSplinePoints INTEGER 4\n"
    "PROPERTY PCreator PERSIS_PNTR C\n"
    "PROPERTY RandomFrac FLOAT 3\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n"
    "BEGINCLASS ParticleChainCreator C\n"
    "BEGINPROPERTIES\n"
    "PROPERTY FrameHeight INTEGER 256\n"
    "PROPERTY FrameWidth INTEGER 64\n"
    "PROPERTY NumTexturesForWholeChain INTEGER 4\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n";

/// One cloud core and its cloud gather of 10 clouds, with SF_LightningStormPush's float providers (test_storm.cpp's)
/// A bolt cast from the hand with one target (a ground point) and five joints per fork: group 0 holds the rule, the
/// forks are group 1
constexpr std::string_view k_BoltFile = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS UR_Lightning UR_Lightning0
BEGINPROPERTIES
PROPERTY CastingFromHand BOOL 1
PROPERTY CommonGlowGroup INTEGER 2
PROPERTY ForkGroup INTEGER 1
PROPERTY ForkScale FLOAT 2
PROPERTY Group INTEGER 0
PROPERTY LightMapGroup INTEGER -1
PROPERTY MaxJointsPerFork INTEGER 5
PROPERTY MaxLightningObjects INTEGER 1
PROPERTY MaxLightningObjectsAtOnce INTEGER 1
PROPERTY MinLightningObjects INTEGER 1
PROPERTY DefaultSearchRadius FLOAT 20
PROPERTY RandomFrac FLOAT 0.05
PROPERTY SplitAngle FLOAT 0.2
PROPERTY RenewSearchEvery FLOAT 0
PROPERTY PCreator PERSIS_PNTR ParticleChainCreator0
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticleChainCreator ParticleChainCreator0
BEGINPROPERTIES
PROPERTY NumTexturesForWholeChain INTEGER 4
PROPERTY TextureFileName STRING .\Data\Textures\S_Lightning.raw
PROPERTY UseAdditiveAlpha BOOL 1
ENDPROPERTIES
ENDCLASS
)";

constexpr std::string_view k_GatherFile = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS MagnitudeFloatProvider FP_Radius
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1e+006
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.2
ENDPROPERTIES
ENDCLASS
BEGINCLASS MagnitudeFloatProvider FP_Scale
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1e+006
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 0.0075
ENDPROPERTIES
ENDCLASS
BEGINCLASS MagnitudeFloatProvider FP_Height
BEGINPROPERTIES
PROPERTY Maximum FLOAT 1e+006
PROPERTY Minimum FLOAT 0
PROPERTY ScaleBy FLOAT 1.5
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator Point
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleSphere Cores
BEGINPROPERTIES
PROPERTY Group INTEGER 0
PROPERTY NextGroups ARRAY SIZE 1 2
PROPERTY NumAtoms INTEGER 1
PROPERTY PCreator PERSIS_PNTR Point
PROPERTY Radius FLOAT 0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_CloudGather Gather
BEGINPROPERTIES
PROPERTY Group INTEGER 2
PROPERTY NumAtoms INTEGER 10
PROPERTY TimeToForm FLOAT 8
PROPERTY FracToMaxSize FLOAT 0.413717
PROPERTY PCreator PERSIS_PNTR Point
PROPERTY RadiusFloatProvider PERSIS_PNTR FP_Radius
PROPERTY ScaleFloatProvider PERSIS_PNTR FP_Scale
PROPERTY CloudHeight PERSIS_PNTR FP_Height
PROPERTY MaxAlpha INTEGER 180
PROPERTY MinAlpha INTEGER 0
PROPERTY MaxColor INTEGER 120
PROPERTY MinColor INTEGER 90
PROPERTY MaxCloudRatio FLOAT 10
PROPERTY MinCloudRatio FLOAT 1
PROPERTY CloudRatioMaxCollection FLOAT 1
PROPERTY CollectionRadiusInitialScale FLOAT 2
PROPERTY LightningGroup INTEGER 4
ENDPROPERTIES
ENDCLASS
)";

/// The artefact's glowing shell (UR_VolFXOnObject) over a point creator
constexpr std::string_view k_GlowFile =
    "BEGINPROPERTIES\n"
    "PROPERTY DeleteOnCloseDown BOOL 0\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS UR_VolFXOnObject UR_VolFXOnObject0\n"
    "BEGINPROPERTIES\n"
    "PROPERTY Alpha INTEGER 207\n"
    "PROPERTY Group INTEGER 0\n"
    "PROPERTY MatInd INTEGER 2\n"
    "PROPERTY NextGroups ARRAY SIZE 0\n"
    "PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n"
    "BEGINCLASS ParticlePointCreator ParticlePointCreator0\n"
    "BEGINPROPERTIES\n"
    "PROPERTY InitialScale FLOAT 1\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n";

/// Objects standing where they are told; the others are not there
class StandingObjects final: public ecs::systems::GlintTargetsInterface
{
public:
	[[nodiscard]] std::optional<glm::vec3> ObjectPosition(entt::entity object) const override
	{
		if (const auto found = objects.find(object); found != objects.end())
		{
			return found->second;
		}
		return std::nullopt;
	}
	[[nodiscard]] uint32_t TargetPointCount(entt::entity /*object*/) const override { return 0; }
	[[nodiscard]] std::optional<glm::vec3> TargetPoint(entt::entity /*object*/, uint32_t /*index*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] float TargetScale(entt::entity /*object*/) const override { return 1.0f; }

	std::map<entt::entity, glm::vec3> objects;
};

/// The game's mana path (SF_ManaPathNew): its rule, letting each mote out as a horizontal sprite
constexpr std::string_view k_ManaPathFile =
    "BEGINPROPERTIES\n"
    "PROPERTY DeleteOnCloseDown BOOL 1\n"
    "PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
    "ENDPROPERTIES\n"
    "BEGINCLASS UR_ManaPathNew UR_ManaPathNew0\n"
    "BEGINPROPERTIES\n"
    "PROPERTY GlowPosSpeed FLOAT 40\n"
    "PROPERTY Group INTEGER 0\n"
    "PROPERTY Height FLOAT 0.4\n"
    "PROPERTY NextGroups ARRAY SIZE 0\n"
    "PROPERTY NoiseAmplitude FLOAT 5.9885\n"
    "PROPERTY NoiseFrequency FLOAT 0.0893805\n"
    "PROPERTY PCreator PERSIS_PNTR ParticleSpriteCreatorGood\n"
    "PROPERTY TimeToTravel FLOAT 3\n"
    "PROPERTY UseConstantSpeed BOOL 1\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n"
    "BEGINCLASS ParticleSpriteCreator ParticleSpriteCreatorGood\n"
    "BEGINPROPERTIES\n"
    "PROPERTY ColorA INTEGER 15\n"
    "PROPERTY InitialScale FLOAT 2\n"
    "PROPERTY NumFrames INTEGER 8\n"
    "PROPERTY SetHorozontal BOOL 1\n"
    "PROPERTY StretchVertically FLOAT 3\n"
    "PROPERTY UseAdditiveAlpha BOOL 1\n"
    "ENDPROPERTIES\n"
    "ENDCLASS\n";

/// A frozen profile's saved game, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenSave(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto path = std::filesystem::path(game) / "Profiles" / relative;
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		return std::nullopt;
	}
	std::ifstream file(path, std::ios::binary);
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}
} // namespace

// A live spot visual's container is written with the thing it hangs on, its effect's interface and its manager: the
// container's place, its effect drawn as one, the turns it has left and its strength; the manager's origin,
// alpha, age, start direction along x, magnitude, synced flag and its file named by the spot visual's particle type
TEST(ParticleWrite, ALiveContainerIsWrittenWithItsEffectsInterfaceAndManager)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto bonfire = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(bonfire, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), bonfire);

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;

	gamesave::GameHead game;
	game.lists[0].numbers = {2};
	ParticleContainerRecord container;
	container.position.position = {.x = 0x1000000, .z = 0x2000000, .altitude = 27.5f};
	container.byte = 1;
	container.object = {3};
	container.system = {4};
	container.word = 0xFFFFFFFF;
	container.lastWord = std::bit_cast<uint32_t>(1.0f);
	EXPECT_EQ(written.bytes, FileOf({{1, game},
	                                 {2, container},
	                                 {3, gamesave::WaypointRecord {}},
	                                 {4, ParticleInterfaceRecord {.system = {5}}},
	                                 {5, NewManager()}}));

	// gone with its thing, the container hangs on nothing; closing down, its manager says so
	registry.Destroy(bonfire);
	particles.GetState().effects[k_Effect].effect->CloseDown();
	const auto alone = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(alone.Complete()) << alone.failure;
	container.object = {0};
	container.system = {3};
	auto closing = NewManager();
	closing.secondWord = 1;
	EXPECT_EQ(alone.bytes, FileOf({{1, game}, {2, container}, {3, ParticleInterfaceRecord {.system = {4}}}, {4, closing}}));

	// a container whose spot visual's particle type is not known cannot name its effect's file: the write stops and
	// says which
	particles.GetState().containers.front().particleType = ParticleType::None;
	const auto unknown = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	EXPECT_FALSE(unknown.Complete());
	EXPECT_NE(unknown.failure.find("particle effect 7"), std::string::npos) << unknown.failure;
}

// A spot visual's object a script holds is named in the scripts' table by its container, as the game's table holds the
// container: no class of its own is needed
TEST(ParticleWrite, ASpotVisualAScriptHoldsIsNamedByItsContainer)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto visual = registry.Create();
	StartContainer(particles.GetState(), entt::null);
	particles.GetState().containers.front().object = visual;
	ClassWriters writers;
	RegisterParticleWriters(writers);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	const auto number = ScriptTableObject(context, visual);
	EXPECT_NE(number, 0u);
	EXPECT_EQ(number, context.Reference(SourceKey {.type = ParticleContainerRecord::k_Type, .id = k_Effect}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A computer player CALL_COMPUTER_PLAYER handed the scripts, which openblack keeps on its player's entity, is named by
// the computer player's object of the player's number, as the game's table holds the computer player
TEST(ParticleWrite, AComputerPlayerAScriptHoldsIsNamedByItsComputerPlayersObject)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player).name = PlayerNames::PLAYER_THREE;
	registry.Assign<ecs::components::ComputerPlayer>(player);
	ClassWriters writers;
	RegisterParticleWriters(writers);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	const auto number = ScriptTableObject(context, player);
	EXPECT_NE(number, 0u);
	EXPECT_EQ(number, context.Reference(SourceKey {.type = gamesave::k_ComputerPlayerType, .id = 2}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// The game's list of containers is the live ones, the newest first, then those the load kept, as the loaded file
// listed them
TEST(ParticleWrite, TheGamesListOfContainersIsTheLiveOnesThenTheKeptOnes)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::ParticleContainers)).numbers = {2};
	kept.objects.emplace_back(game);
	ParticleContainerRecord loaded;
	loaded.word = 5;
	kept.objects.emplace_back(loaded);
	ecs::Registry registry;
	StartContainer(particles.GetState(), entt::null);

	ClassWriters writers;
	RegisterParticleWriters(writers);
	const auto* list = writers.ListWriterOf(GlobalList::ParticleContainers);
	ASSERT_NE(list, nullptr);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept};
	WriteContext context(writers, source);
	const auto numbers = (*list)(context);
	ASSERT_EQ(numbers.size(), 2u);
	EXPECT_EQ(numbers[1], 2u);
	const auto live = context.TargetOf(numbers[0]);
	ASSERT_TRUE(live.has_value());
	EXPECT_EQ(live->type, ParticleContainerRecord::k_Type);
	EXPECT_EQ(live->id, std::optional<uint64_t>(k_Effect));
}

// A kept container a load made live again (a sped-up pile's sparkle: the load names the live one by the kept number) is
// listed once, among the live ones; a spot visual's object names its effect while it runs
TEST(ParticleWrite, AKeptContainerMadeLiveAgainIsListedOnce)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::ParticleContainers)).numbers = {2};
	kept.objects.emplace_back(game);
	kept.objects.emplace_back(ParticleContainerRecord {});
	ecs::Registry registry;
	StartContainer(particles.GetState(), entt::null);
	const auto sparkle = registry.Create();
	particles.GetState().containers.front().object = sparkle;
	EXPECT_EQ(SpotVisualEffect(sparkle), std::optional<uint64_t>(k_Effect));
	EXPECT_EQ(SpotVisualEffect(registry.Create()), std::nullopt);

	ClassWriters writers;
	RegisterParticleWriters(writers);
	const auto* list = writers.ListWriterOf(GlobalList::ParticleContainers);
	ASSERT_NE(list, nullptr);
	const WorldSource source {.registry = registry,
	                          .landscape = std::string(k_Land),
	                          .kept = &kept,
	                          .loaded = {{2, LoadedObject {.type = ParticleContainerRecord::k_Type, .id = k_Effect}}}};
	WriteContext context(writers, source);
	EXPECT_EQ((*list)(context), (std::vector<gamesave::ObjectNumber> {2}));
}

// A loaded container made again and gone since (its turns run out) has left the game's list of containers, as the
// original's container unlinks itself from that list as it is deleted; one the load did not make stays as kept
// (docs/wiki/engine/saved-games.md#openblack, the particles)
TEST(ParticleWrite, ALoadedContainerGoneSinceTheLoadLeavesTheGamesList)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::ParticleContainers)).numbers = {2, 3};
	kept.objects.emplace_back(game);
	kept.objects.emplace_back(ParticleContainerRecord {});
	kept.objects.emplace_back(ParticleContainerRecord {});
	ecs::Registry registry;
	// container 2 was made again by the load as effect k_Effect, which has gone; container 3 was not made
	particles.GetState().containers.clear();
	particles.GetState().effects.clear();

	ClassWriters writers;
	RegisterParticleWriters(writers);
	const auto* list = writers.ListWriterOf(GlobalList::ParticleContainers);
	ASSERT_NE(list, nullptr);
	const WorldSource source {.registry = registry,
	                          .landscape = std::string(k_Land),
	                          .kept = &kept,
	                          .loaded = {{2, LoadedObject {.type = ParticleContainerRecord::k_Type, .id = k_Effect}}}};
	WriteContext context(writers, source);
	EXPECT_EQ((*list)(context), (std::vector<gamesave::ObjectNumber> {3}));

	// while it is there the list names it, first as it is live
	StartContainer(particles.GetState(), entt::null);
	WriteContext again(writers, source);
	EXPECT_EQ((*list)(again), (std::vector<gamesave::ObjectNumber> {2, 3}));
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's two bonfires' containers are listed as the load kept them (no load makes them yet), and with the particle
// writers the file is the original's byte for byte
TEST(ParticleWrite, TheFrozenLandOneAutosaveWritesBackByteForByteWithTheParticleWriters)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	Locator::particleSystem::emplace<test::InertParticleSystem>();
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	const test::RestoreService<Locator::computerPlayerSystem> restoreComputers;
	Locator::computerPlayerSystem::reset();
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto state = ecs::saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());

	LoadedObjects loaded;
	KeepLoadedSources(world, loaded);
	ecs::Registry registry;
	ClassWriters writers;
	RegisterGameObjectWriter(writers, [&state] { return ecs::saved_game_save::ValuesOf(*state); });
	RegisterParticleWriters(writers);
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;
	ASSERT_EQ(written.bytes.size(), bytes->size());
	constexpr std::ptrdiff_t k_Counter = sizeof(uint32_t);
	const auto difference =
	    std::mismatch(written.bytes.begin() + k_Counter, written.bytes.end(), bytes->begin() + k_Counter, bytes->end());
	EXPECT_TRUE(difference.first == written.bytes.end())
	    << "first difference at byte " << std::distance(written.bytes.begin(), difference.first);
	size_t containers = 0;
	for (const auto& record : written.records)
	{
		containers += record.type == ParticleContainerRecord::k_Type ? 1 : 0;
	}
	EXPECT_EQ(containers, 2u);
}

// A physical shield's effect, which no container or spell holds, names its file by the physical shield's particle type
TEST(ParticleWrite, APhysicalShieldsEffectNamesItsFileByThePhysicalShieldsParticleType)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	auto file = std::make_shared<particles::File>();
	file->name = "SF_PhysicalShieldFX";
	particles.GetState().effects[k_Effect].effect = std::make_unique<particles::Effect>(
	    file, glm::vec3(1.0f, 2.0f, 3.0f), 1.0f, game_random::particles::NetGameType::Synced);
	const auto shield = registry.Create();
	registry.Assign<ecs::components::MagicShield>(
	    shield, ecs::components::MagicShield {.kind = ecs::components::MagicShield::Kind::Physical, .fx = k_Effect});

	ClassWriters writers;
	RegisterParticleWriters(writers);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land)};
	WriteContext context(writers, source);
	const auto made = context.RecordOf(context.Reference(SourceKey {.type = ParticleManagerRecord::k_Type, .id = k_Effect}));
	ASSERT_NE(made.record, nullptr) << context.Failure();
	const auto* manager = std::get_if<ParticleManagerRecord>(made.record.get());
	ASSERT_NE(manager, nullptr);
	EXPECT_EQ(manager->persistent.first, static_cast<uint32_t>(ParticleType::PhysicalShieldFx));
}

// A worship site's mana path, which no container or spell holds, names its file by the mana path's particle type
TEST(ParticleWrite, AWorshipSitesManaPathNamesItsFileByTheManaPathsParticleType)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	auto file = std::make_shared<particles::File>();
	file->name = "SF_ManaPathNew";
	particles.GetState().effects[k_Effect].effect =
	    std::make_unique<particles::Effect>(file, glm::vec3(0.0f), 1.0f, game_random::particles::NetGameType::Local);
	const auto site = registry.Create();
	registry.Assign<ecs::components::WorshipSite>(site).manaPathEffect = k_Effect;

	ClassWriters writers;
	RegisterParticleWriters(writers);
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land)};
	WriteContext context(writers, source);
	const auto made = context.RecordOf(context.Reference(SourceKey {.type = ParticleManagerRecord::k_Type, .id = k_Effect}));
	ASSERT_NE(made.record, nullptr) << context.Failure();
	const auto* manager = std::get_if<ParticleManagerRecord>(made.record.get());
	ASSERT_NE(manager, nullptr);
	EXPECT_EQ(manager->persistent.first, static_cast<uint32_t>(ParticleType::ManaPath));
}

// A container's effect with its collections, atoms and an emitter's data is written, made again from the file it wrote
// and written again the same, byte for byte: the load puts back every value the write takes
TEST(ParticleWrite, AnEffectWithItsAtomsIsWrittenLoadedAndWrittenAgainTheSame)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_EmitterFile), "SF_Bonfire");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	for (int step = 0; step < 4; ++step)
	{
		running.effect->Step(0.1f);
	}
	ASSERT_FALSE(running.effect->GetRoots().empty());
	ASSERT_FALSE(running.effect->GetRoots().front()->atoms.empty());

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;
	const auto world = gamesave::ReadWorld(first.bytes);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	ASSERT_EQ(game->lists[0].numbers.size(), 1u);
	const auto* container = std::get_if<ParticleContainerRecord>(&*world.objects.at(game->lists[0].numbers[0] - 1));
	ASSERT_NE(container, nullptr);
	const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
	ASSERT_TRUE(planned.has_value());
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&](ObjectNumber number) { return number == container->object.number ? thing : entt::null; }};
	std::string why;
	auto restored = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(restored, nullptr) << why;
	running.effect = std::move(restored);

	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
}

// A container written with its effect is made again by the load from the file it wrote, at the head of the engine's
// containers with its effect taken in as the save kept it (nothing started again), and the world written again is the
// same, byte for byte: the containers' load puts back every value their write takes
TEST(ParticleWrite, AContainerAndItsEffectAreLoadedFromTheFileTheyWroteAndWrittenAgainTheSame)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	// the load reads the effect's spell file through the file system, uncached without the resources
	const test::RestoreService<Locator::filesystem> restoreFiles;
	auto& files = static_cast<test::MemoryFileSystem&>(Locator::filesystem::emplace<test::MemoryFileSystem>());
	files.files[files.GetPath<filesystem::Path::Data>() / "Spells" / "ZSpellFiles" / "SF_Bonfire.txt"] =
	    std::vector<uint8_t>(k_EmitterFile.begin(), k_EmitterFile.end());
	const test::RestoreService<Locator::resources> restoreResources;
	Locator::resources::reset();

	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_EmitterFile), "SF_Bonfire");
	ASSERT_TRUE(parsed.has_value());
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(std::make_shared<const particles::File>(std::move(*parsed)),
	                                                     glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	for (int step = 0; step < 4; ++step)
	{
		running.effect->Step(0.1f);
	}

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;

	// the load: the game's list planned, the engine emptied of what the write took, the containers made again
	const auto world = gamesave::ReadWorld(first.bytes);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	// the new game's writer puts the game's containers in its first list (RegisterNewGame)
	const auto planned =
	    ecs::saved_particles::PlanContainers(world, game->lists[0].numbers, [](ObjectNumber /*number*/) { return -1; });
	ASSERT_EQ(planned.size(), 1u);
	const auto owner = planned.front().record.object.number;
	particles.GetState().containers.clear();
	particles.GetState().effects.clear();
	const ecs::saved_particles::EffectLinks links {
	    .entityOf = [&](ObjectNumber number) { return number == owner ? thing : entt::null; }};
	const auto made = ecs::saved_particles::MakeContainers(planned, links, {}, {});
	ASSERT_EQ(made.size(), 1u);
	ASSERT_EQ(particles.GetState().containers.size(), 1u);
	const auto& container = particles.GetState().containers.front();
	EXPECT_EQ(container.owner, thing);
	EXPECT_EQ(container.object, made.begin()->second);
	EXPECT_EQ(container.particleType, ParticleType::Bonfire);
	EXPECT_EQ(container.turns, -1);
	ASSERT_EQ(std::distance(particles.GetState().effects.begin(), particles.GetState().effects.end()), 1);
	EXPECT_TRUE(particles.GetState().effects.begin()->second.inContainer);
	EXPECT_EQ(particles.GetState().effects.begin()->second.path, particles::DrawPath::Queued);

	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
}

// A container whose thing was not made by the load (kept as read, as a creature casting at the save is) is kept as
// read with it: nothing is made or taken into the engine, and its numbers keep naming the records the file holds, so
// the thing's kept record never names a container gone. Made, it would close at once with no thing to hang on
TEST(ParticleWrite, AContainerWhoseThingTheLoadDidNotMakeIsKeptAsReadWithIt)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::saved_particles::PlannedContainer planned;
	planned.number = 19;
	planned.record.object.number = 7130;
	planned.effect.manager.persistent.first = static_cast<uint32_t>(ParticleType::Bonfire);
	const ecs::saved_particles::EffectLinks links {
	    .entityOf = [](ObjectNumber /*number*/) { return entt::entity {entt::null}; }};
	int keptSources = 0;
	int kept = 0;
	const auto made = ecs::saved_particles::MakeContainers(
	    {planned}, links, [&keptSources](ObjectNumber /*number*/, uint32_t /*type*/, uint64_t /*id*/) { ++keptSources; },
	    [&kept](ObjectNumber /*number*/, entt::entity /*entity*/, uint32_t /*type*/) { ++kept; });
	EXPECT_TRUE(made.empty());
	EXPECT_TRUE(particles.GetState().containers.empty());
	EXPECT_EQ(std::distance(particles.GetState().effects.begin(), particles.GetState().effects.end()), 0);
	EXPECT_EQ(keptSources, 0);
	EXPECT_EQ(kept, 0);
}

// An atom's draw offsets are written with it and loaded the same: a decaying one once for all the atoms that share it,
// with their count, and one that follows the hand as its atom's own; written again after the load, the file is the same
TEST(ParticleWrite, TheAtomsDrawOffsetsAreWrittenOnceForTheAtomsSharingThemAndLoadedTheSame)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_EmitterFile), "SF_Bonfire");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	for (int step = 0; step < 4; ++step)
	{
		running.effect->Step(0.1f);
	}
	ASSERT_FALSE(running.effect->GetRoots().empty());
	ASSERT_FALSE(running.effect->GetRoots().front()->atoms.empty());
	auto& root = *running.effect->GetRoots().front()->atoms.front();
	ASSERT_FALSE(root.subCollections.empty());
	auto& emitted = root.subCollections.front()->atoms;
	ASSERT_GE(emitted.size(), 2u);
	// the root's decaying offset, two frames on, and the first emitted atom sharing it; the second follows the hand
	root.decayingOffset = particles::Atom::DecayingDrawOffset::From(glm::vec3(2.0f, 1.0f, -3.0f));
	root.decayingOffset->id = running.effect->NewDrawOffsetId();
	root.decayingOffset->Step(16);
	root.decayingOffset->Step(17);
	emitted[0]->ShareDrawOffset(root);
	emitted[1]->drawOffset.emplace();
	emitted[1]->drawOffset->SetReference(glm::vec3(1800.0f, 30.0f, 2640.0f), 0.25f);

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;
	size_t decaying = 0;
	size_t hand = 0;
	const auto world = gamesave::ReadWorld(first.bytes);
	for (const auto& object : world.objects)
	{
		if (const auto* offset = object.has_value() ? std::get_if<gamesave::DrawOffsetDecayRecord>(&*object) : nullptr)
		{
			++decaying;
			EXPECT_EQ(offset->references, 2u);
			EXPECT_EQ(offset->start, (gamesave::Point {2.0f, 1.0f, -3.0f}));
		}
		if (const auto* offset = object.has_value() ? std::get_if<gamesave::DrawOffsetHandRecord>(&*object) : nullptr)
		{
			++hand;
			EXPECT_EQ(offset->references, 1u);
			EXPECT_EQ(offset->amount, 0.25f);
		}
	}
	EXPECT_EQ(decaying, 1u);
	EXPECT_EQ(hand, 1u);

	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	const auto* container = std::get_if<ParticleContainerRecord>(&*world.objects.at(game->lists[0].numbers[0] - 1));
	ASSERT_NE(container, nullptr);
	const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
	ASSERT_TRUE(planned.has_value());
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&](ObjectNumber number) { return number == container->object.number ? thing : entt::null; }};
	std::string why;
	auto restored = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(restored, nullptr) << why;
	running.effect = std::move(restored);
	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
}

// A chain's collection names its chain, and each joint's atom its particle, which names the chain: the chain with its
// creator, its layout and its joints as the frame's draw left them, the joints by their numbers
TEST(ParticleWrite, AChainIsWrittenWithItsJointsAsTheFramesDrawLeftThem)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_ChainFile), "SF_SimpleBeam");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	for (int step = 0; step < 3; ++step)
	{
		running.effect->Step(0.1f);
	}
	ASSERT_FALSE(running.effect->GetRoots().empty());
	const auto& collection = *running.effect->GetRoots().front();
	ASSERT_EQ(collection.chainJoints.size(), 4u);
	running.effect->KeepDrawnJoints(0.5f);
	ASSERT_TRUE(collection.chainDrawn);

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;
	const auto world = gamesave::ReadWorld(written.bytes);
	const auto record = [&world](ObjectNumber number) -> const SavedRecord& { return *world.objects.at(number - 1); };
	ObjectNumber chainNumber = 0;
	std::vector<uint32_t> numbers;
	for (size_t i = 0; i < world.objects.size(); ++i)
	{
		const auto* object = world.objects[i].has_value() ? &*world.objects[i] : nullptr;
		if (const auto* chain = object != nullptr ? std::get_if<gamesave::ChainRecord>(object) : nullptr)
		{
			chainNumber = static_cast<ObjectNumber>(i + 1);
			EXPECT_EQ(chain->persistent.second, 1u);
			EXPECT_EQ(chain->words[1], 256u);
			EXPECT_EQ(chain->words[2], 64u);
			EXPECT_EQ(chain->words[6], 4u);
			ASSERT_EQ(chain->joints.values.size(), 4u);
			for (size_t j = 0; j < 4; ++j)
			{
				EXPECT_EQ(collection.chainJoints[j].position, particle_codecs::ChainJointOf(chain->joints.values[j]).position);
				EXPECT_EQ(chain->joints.values[j][24], 1u);
			}
			EXPECT_EQ(chain->lastByte, 1u);
		}
	}
	ASSERT_NE(chainNumber, 0u);
	for (const auto& object : world.objects)
	{
		if (const auto* collectionRecord = object.has_value() ? std::get_if<gamesave::AtomCollectionRecord>(&*object) : nullptr)
		{
			EXPECT_EQ(collectionRecord->third.number, chainNumber);
			ASSERT_EQ(collectionRecord->secondSystems.numbers.size(), 4u);
			for (const auto atomNumber : collectionRecord->secondSystems.numbers)
			{
				const auto* atom = std::get_if<gamesave::AtomCoreRecord>(&record(atomNumber));
				ASSERT_NE(atom, nullptr);
				const auto* joint = std::get_if<gamesave::ParticleChainJointRecord>(&record(atom->others[2].number));
				ASSERT_NE(joint, nullptr);
				EXPECT_EQ(joint->chain.number, chainNumber);
				numbers.push_back(joint->words[0]);
			}
		}
	}
	// the atoms the newest first, so the joints from the last made
	EXPECT_EQ(numbers, (std::vector<uint32_t> {3, 2, 1, 0}));
}

// A chain is loaded from the file it wrote: its collection's chain with its creator, its joints as the last frame's draw
// left them, its v-scroll and its rate, and each joint's atom its creator and number; written again, the same bytes
TEST(ParticleWrite, AChainIsLoadedFromTheFileItWroteAndWrittenAgainTheSame)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_ChainFile), "SF_SimpleBeam");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	for (int step = 0; step < 3; ++step)
	{
		running.effect->Step(0.1f);
	}
	ASSERT_FALSE(running.effect->GetRoots().empty());
	running.effect->KeepDrawnJoints(0.5f);
	auto& made = *running.effect->GetRoots().front();
	made.chainScroll = 0.25f;
	made.chainScrollRate = 1.5f;
	const auto joints = made.chainJoints;

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;
	const auto world = gamesave::ReadWorld(first.bytes);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	const auto* container = std::get_if<ParticleContainerRecord>(&*world.objects.at(game->lists[0].numbers[0] - 1));
	ASSERT_NE(container, nullptr);
	const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
	ASSERT_TRUE(planned.has_value());
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&](ObjectNumber number) { return number == container->object.number ? thing : entt::null; }};
	std::string why;
	auto restored = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(restored, nullptr) << why;
	ASSERT_FALSE(restored->GetRoots().empty());
	const auto& loaded = *restored->GetRoots().front();
	ASSERT_NE(loaded.chainCreator, nullptr);
	EXPECT_EQ(loaded.chainCreator->fileIndex, made.chainCreator->fileIndex);
	ASSERT_EQ(loaded.chainJoints.size(), joints.size());
	for (size_t j = 0; j < joints.size(); ++j)
	{
		EXPECT_EQ(loaded.chainJoints[j].position, joints[j].position);
		EXPECT_EQ(loaded.chainJoints[j].scale, joints[j].scale);
		EXPECT_EQ(loaded.chainJoints[j].colour, joints[j].colour);
		EXPECT_TRUE(loaded.chainJoints[j].drawn);
	}
	EXPECT_TRUE(loaded.chainDrawn);
	EXPECT_EQ(loaded.chainScroll, 0.25f);
	EXPECT_EQ(loaded.chainScrollRate, 1.5f);
	EXPECT_EQ(loaded.chainTextures, -1);
	ASSERT_EQ(loaded.atoms.size(), made.atoms.size());
	for (size_t a = 0; a < made.atoms.size(); ++a)
	{
		EXPECT_EQ(loaded.atoms[a]->chainJoint, made.atoms[a]->chainJoint);
		EXPECT_EQ(loaded.atoms[a]->creator, loaded.chainCreator);
	}
	running.effect = std::move(restored);
	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
}

// The simple beam's data is written on its atom, with its rule, its unset turn for a point and its two ends, and on each
// of its beams' collections, with its spline through the key points it was last laid through, its number and the
// count of beams
TEST(ParticleWrite, TheSimpleBeamsDataIsWrittenOnItsAtomAndOnEachBeam)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_BeamFile), "SF_SimpleBeam");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	running.effect->AddTargetPoint({1810.0f, 20.0f, 2650.0f});
	running.effect->Step(0.1f);
	running.effect->Step(0.1f);

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;
	const auto world = gamesave::ReadWorld(written.bytes);
	size_t atomData = 0;
	std::vector<uint32_t> numbers;
	for (const auto& object : world.objects)
	{
		if (const auto* data = object.has_value() ? std::get_if<gamesave::BeamAtomDataRecord>(&*object) : nullptr)
		{
			++atomData;
			EXPECT_EQ(data->persistent.second, 0u);
			EXPECT_EQ(data->word, 0xCDCDCDCDu);
			EXPECT_EQ(data->points[0], (gamesave::Point {1810.0f, 20.0f, 2650.0f}));
			EXPECT_EQ(data->points[1], (gamesave::Point {1803.5f, 27.5f, 2645.5f}));
		}
		if (const auto* data = object.has_value() ? std::get_if<gamesave::BeamSubCollectionRecord>(&*object) : nullptr)
		{
			EXPECT_EQ(data->persistent.second, 0u);
			EXPECT_EQ(data->word, 1u);
			ASSERT_EQ(data->points.elements.size(), 4u);
			EXPECT_EQ(data->points.elements.front().first, (gamesave::Point {1803.5f, 27.5f, 2645.5f}));
			EXPECT_EQ(data->lastWords[1], 2u);
			numbers.push_back(data->lastWords[0]);
		}
	}
	EXPECT_EQ(atomData, 1u);
	// the beams' collections the newest first, number 0 first
	EXPECT_EQ(numbers, (std::vector<uint32_t> {0, 1}));
}

// A simple beam is loaded from the file it wrote: its atom's turn and two ends, each beam's number, count of beams and
// key points, and its chains; written again, the same bytes
TEST(ParticleWrite, ASimpleBeamIsLoadedFromTheFileItWroteAndWrittenAgainTheSame)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_BeamFile), "SF_SimpleBeam");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	running.effect->AddTargetPoint({1810.0f, 20.0f, 2650.0f});
	for (int step = 0; step < 3; ++step)
	{
		running.effect->Step(0.1f);
	}
	running.effect->KeepDrawnJoints(0.5f);

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;
	const auto world = gamesave::ReadWorld(first.bytes);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	const auto* container = std::get_if<ParticleContainerRecord>(&*world.objects.at(game->lists[0].numbers[0] - 1));
	ASSERT_NE(container, nullptr);
	const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
	ASSERT_TRUE(planned.has_value());
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&](ObjectNumber number) { return number == container->object.number ? thing : entt::null; }};
	std::string why;
	auto restored = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(restored, nullptr) << why;
	const auto& made = *running.effect->GetRoots().front()->atoms.front();
	const auto& loaded = *restored->GetRoots().front()->atoms.front();
	ASSERT_EQ(loaded.dataMade.size(), 1u);
	const auto& madeTarget = *std::static_pointer_cast<particles::beam::Target>(made.modifierData.at(made.dataMade.front()));
	const auto& loadedTarget =
	    *std::static_pointer_cast<particles::beam::Target>(loaded.modifierData.at(loaded.dataMade.front()));
	EXPECT_EQ(loadedTarget.validatedTurn, madeTarget.validatedTurn);
	EXPECT_EQ(loadedTarget.target, madeTarget.target);
	EXPECT_EQ(loadedTarget.origin, madeTarget.origin);
	ASSERT_EQ(loaded.subCollections.size(), made.subCollections.size());
	for (size_t b = 0; b < made.subCollections.size(); ++b)
	{
		const auto& madeSub = *made.subCollections[b];
		const auto& loadedSub = *loaded.subCollections[b];
		ASSERT_EQ(loadedSub.dataMade.size(), 1u);
		const auto& madeRibbon =
		    *std::static_pointer_cast<particles::beam::Ribbon>(madeSub.modifierData.at(madeSub.dataMade.front()));
		const auto& loadedRibbon =
		    *std::static_pointer_cast<particles::beam::Ribbon>(loadedSub.modifierData.at(loadedSub.dataMade.front()));
		EXPECT_EQ(loadedRibbon.number, madeRibbon.number);
		EXPECT_EQ(loadedRibbon.beams, madeRibbon.beams);
		EXPECT_EQ(loadedRibbon.keys, madeRibbon.keys);
		EXPECT_EQ(loadedSub.chainJoints.size(), 3u);
	}
	running.effect = std::move(restored);
	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
}

// A storm's cloud gather is loaded from the file it wrote: its collection's data (0xD2) with the clouds' own (0xD3), and
// a lightning collection that hangs from no cloud, owned by the gather's data, made again from its record; written
// again, the same bytes
TEST(ParticleWrite, ACloudGatherIsLoadedFromTheFileItWroteAndWrittenAgainTheSame)
{
	ASSERT_NE(particles::FindModifierFactory("UR_CloudGather"), nullptr); // fills the registry
	weather::storms::Clear();
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_GatherFile), "SF_GatherTest");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 60.0f,
	                                                     game_random::particles::NetGameType::Synced);
	for (int step = 0; step < 15; ++step)
	{
		running.effect->Step(0.1f);
	}
	// the gather under the core, with a lightning collection let go of its cloud
	auto& core = *running.effect->GetRoots().front()->atoms.front();
	ASSERT_FALSE(core.subCollections.empty());
	auto& gather = *core.subCollections.front();
	ASSERT_FALSE(gather.dataMade.empty());
	const auto& rule = *gather.dataMade.front();
	auto values = particles::storm::CloudGatherOf(gather, rule);
	ASSERT_TRUE(values.has_value());
	EXPECT_FALSE(values->first);
	ASSERT_GE(gather.atoms.size(), 2u);
	values->detached = true;
	auto bolt = running.effect->NewCollection(4, nullptr);
	ASSERT_TRUE(particles::storm::RestoreCloudGather(gather, rule, *values, nullptr, nullptr, std::move(bolt)));
	ASSERT_NE(particles::storm::DetachedLightningOf(gather, rule), nullptr);

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;
	const auto world = gamesave::ReadWorld(first.bytes);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	const auto* container = std::get_if<ParticleContainerRecord>(&*world.objects.at(game->lists[0].numbers[0] - 1));
	ASSERT_NE(container, nullptr);
	const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
	ASSERT_TRUE(planned.has_value());
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&](ObjectNumber number) { return number == container->object.number ? thing : entt::null; }};
	std::string why;
	auto restored = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(restored, nullptr) << why;
	const auto& loadedCore = *restored->GetRoots().front()->atoms.front();
	ASSERT_FALSE(loadedCore.subCollections.empty());
	const auto& loadedGather = *loadedCore.subCollections.front();
	ASSERT_FALSE(loadedGather.dataMade.empty());
	const auto& loadedRule = *loadedGather.dataMade.front();
	const auto loaded = particles::storm::CloudGatherOf(loadedGather, loadedRule);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(loaded->heading, values->heading);
	EXPECT_EQ(loaded->emitted, values->emitted);
	EXPECT_EQ(loaded->count, values->count);
	EXPECT_EQ(loaded->rate, values->rate);
	EXPECT_EQ(loaded->nextStrike, values->nextStrike);
	EXPECT_EQ(loaded->spin, values->spin);
	EXPECT_TRUE(loaded->detached);
	EXPECT_EQ(loaded->registered, values->registered);
	ASSERT_NE(particles::storm::DetachedLightningOf(loadedGather, loadedRule), nullptr);
	EXPECT_EQ(particles::storm::LightningOf(loadedGather, loadedRule),
	          particles::storm::DetachedLightningOf(loadedGather, loadedRule));
	ASSERT_EQ(loadedGather.atoms.size(), gather.atoms.size());
	for (size_t i = 0; i < gather.atoms.size(); ++i)
	{
		const auto* madeCloud = particles::storm::CloudOf(*gather.atoms[i], rule);
		const auto* loadedCloud = particles::storm::CloudOf(*loadedGather.atoms[i], loadedRule);
		ASSERT_NE(madeCloud, nullptr);
		ASSERT_NE(loadedCloud, nullptr);
		EXPECT_EQ(loadedCloud->radius, madeCloud->radius);
		EXPECT_EQ(loadedCloud->theta, madeCloud->theta);
		EXPECT_EQ(loadedCloud->height, madeCloud->height);
	}
	running.effect = std::move(restored);
	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
	running.effect.reset();
	weather::storms::Clear();
}

// A lightning bolt is loaded from the file it wrote: its collection's data (0xD8), its targets, the lists its forks took,
// its counters, its forks and its own collection; written again, the same bytes
TEST(ParticleWrite, ALightningIsLoadedFromTheFileItWroteAndWrittenAgainTheSame)
{
	const openblack::test::ScopedWorldSystems worldSystems;
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_BoltFile), "SF_LightningBoltTest");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect =
	    std::make_unique<particles::Effect>(file, glm::vec3(0.0f), 1.0f, game_random::particles::NetGameType::Synced);
	particles::ProcessInfo info;
	info.handPos = glm::vec3(0.0f, 2.0f, 0.0f);
	info.cameraForward = glm::vec3(1.0f, 0.0f, 0.0f);
	info.enabled = true;
	running.effect->SetProcessInfo(info);
	for (int step = 0; step < 3; ++step)
	{
		running.effect->Step(0.1f);
	}
	const auto boltOf = [](const particles::Collection& collection) {
		std::optional<particles::lightning::SavedBolt> bolt;
		for (const auto& slot : collection.modifiers)
		{
			if (slot.modifier != nullptr && !bolt.has_value())
			{
				bolt = particles::lightning::SavedBoltOf(collection, *slot.modifier);
			}
		}
		return bolt;
	};
	const auto made = boltOf(*running.effect->GetRoots().front());
	ASSERT_TRUE(made.has_value());

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;
	const auto world = gamesave::ReadWorld(first.bytes);
	size_t bolts = 0;
	for (const auto& object : world.objects)
	{
		bolts += object.has_value() && std::holds_alternative<gamesave::LightningCollectionRecord>(*object) ? 1 : 0;
	}
	EXPECT_EQ(bolts, 1u);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	const auto* container = std::get_if<ParticleContainerRecord>(&*world.objects.at(game->lists[0].numbers[0] - 1));
	ASSERT_NE(container, nullptr);
	const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
	ASSERT_TRUE(planned.has_value());
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&](ObjectNumber number) { return number == container->object.number ? thing : entt::null; }};
	std::string why;
	auto restored = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(restored, nullptr) << why;
	particles::lightning::LinkLoadedBolts();
	const auto& loadedCollection = *restored->GetRoots().front();
	const auto loaded = boltOf(loadedCollection);
	ASSERT_TRUE(loaded.has_value());
	EXPECT_EQ(loaded->collection, &loadedCollection);
	EXPECT_EQ(loaded->origin, made->origin);
	EXPECT_EQ(loaded->life, made->life);
	EXPECT_EQ(loaded->lists, made->lists);
	EXPECT_EQ(loaded->nextList, made->nextList);
	EXPECT_EQ(loaded->nextFork, made->nextFork);
	EXPECT_EQ(loaded->searchLimit, made->searchLimit);
	EXPECT_EQ(loaded->atOnce, made->atOnce);
	EXPECT_EQ(loaded->createdTurn, made->createdTurn);
	ASSERT_EQ(loaded->targets.size(), made->targets.size());
	ASSERT_EQ(loaded->forks.size(), made->forks.size());
	for (size_t i = 0; i < made->forks.size(); ++i)
	{
		EXPECT_EQ(loaded->forks[i].second, made->forks[i].second) << i;
	}
	running.effect = std::move(restored);
	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
	running.effect.reset();
}

// A glowing shell is loaded from the file it wrote: its atom's object, by the load's entities, and the turn it was last
// found there (0xBB), noted as made; written again, the same bytes
TEST(ParticleWrite, AGlowingShellIsLoadedFromTheFileItWroteAndWrittenAgainTheSame)
{
	const test::RestoreService<Locator::glintTargets> restoreObjects;
	auto& standing = static_cast<StandingObjects&>(Locator::glintTargets::emplace<StandingObjects>());
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	standing.objects[thing] = {1810.0f, 20.0f, 2650.0f};
	StartContainer(particles.GetState(), thing);
	auto parsed = particles::File::Parse(std::string(k_GlowFile), "SF_GlowArtefact");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Synced);
	running.effect->AddTarget(thing);
	for (int step = 0; step < 3; ++step)
	{
		running.effect->Step(0.1f);
	}

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;
	const auto world = gamesave::ReadWorld(first.bytes);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	const auto* container = std::get_if<ParticleContainerRecord>(&*world.objects.at(game->lists[0].numbers[0] - 1));
	ASSERT_NE(container, nullptr);
	const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
	ASSERT_TRUE(planned.has_value());
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&](ObjectNumber number) { return number == container->object.number ? thing : entt::null; }};
	std::string why;
	auto restored = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(restored, nullptr) << why;
	const auto& made = *running.effect->GetRoots().front()->atoms.front();
	const auto& loaded = *restored->GetRoots().front()->atoms.front();
	ASSERT_EQ(loaded.dataMade.size(), 1u);
	const auto* madeData = particles::volume_glow::DataOf(made, *made.dataMade.front());
	const auto* loadedData = particles::volume_glow::DataOf(loaded, *loaded.dataMade.front());
	ASSERT_NE(madeData, nullptr);
	ASSERT_NE(loadedData, nullptr);
	EXPECT_EQ(loadedData->object, thing);
	EXPECT_EQ(loadedData->validatedTurn, madeData->validatedTurn);
	running.effect = std::move(restored);
	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
}

// A mana path's sparks are loaded from the file they wrote: each spark's path and progress, its rule's data (0xC0) noted
// as made; written again, the same bytes; and the next step of each is the one the made effect takes
TEST(ParticleWrite, AManaPathsSparksAreLoadedFromTheFileTheyWroteAndWrittenAgainTheSame)
{
	const test::RestoreService<Locator::terrainSystem> keepLand;
	Locator::terrainSystem::reset();
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	ecs::Registry registry;
	const auto thing = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<gamesave::WaypointRecord>>(thing, gamesave::WaypointRecord {});
	StartContainer(particles.GetState(), thing);
	particles.GetState().containers.front().particleType = ParticleType::ManaPath;
	auto parsed = particles::File::Parse(std::string(k_ManaPathFile), "SF_ManaPathNew");
	ASSERT_TRUE(parsed.has_value());
	const auto file = std::make_shared<const particles::File>(std::move(*parsed));
	auto& running = particles.GetState().effects[k_Effect];
	running.effect = std::make_unique<particles::Effect>(file, glm::vec3(1803.5f, 27.5f, 2645.5f), 1.0f,
	                                                     game_random::particles::NetGameType::Local);
	running.effect->AddSpellPoint({.from = {1800.0f, 20.0f, 2640.0f}, .to = {1880.0f, 25.0f, 2700.0f}, .rgb = 0xFF4646});
	running.effect->AddSpellPoint({.from = {1800.0f, 20.0f, 2640.0f}, .to = {1820.0f, 25.0f, 2600.0f}, .rgb = 0x4646FF});
	for (int step = 0; step < 3; ++step)
	{
		running.effect->Step(0.1f);
	}

	ClassWriters writers;
	RegisterNewGame(writers);
	RegisterParticleWriters(writers);
	const auto first = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(first.Complete()) << first.failure;
	const auto world = gamesave::ReadWorld(first.bytes);
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.at(0));
	ASSERT_NE(game, nullptr);
	const auto* container = std::get_if<ParticleContainerRecord>(&*world.objects.at(game->lists[0].numbers[0] - 1));
	ASSERT_NE(container, nullptr);
	const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
	ASSERT_TRUE(planned.has_value());
	const auto data = std::ranges::count_if(planned->parts, [](const auto& part) {
		const auto* record = std::get_if<gamesave::RuleDataRecord>(&part.second);
		return record != nullptr && record->type == particle_codecs::k_ManaPathAtomDataType;
	});
	EXPECT_EQ(data, 2);
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&](ObjectNumber number) { return number == container->object.number ? thing : entt::null; }};
	std::string why;
	auto restored = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(restored, nullptr) << why;
	const auto& made = running.effect->GetRoots().front()->atoms;
	const auto& loaded = restored->GetRoots().front()->atoms;
	ASSERT_EQ(made.size(), 2u);
	ASSERT_EQ(loaded.size(), made.size());
	for (size_t i = 0; i < made.size(); ++i)
	{
		ASSERT_EQ(loaded[i]->dataMade.size(), 1u);
		const auto* madeData = particles::mana_path::DataOf(*made[i], *made[i]->dataMade.front());
		const auto* loadedData = particles::mana_path::DataOf(*loaded[i], *loaded[i]->dataMade.front());
		ASSERT_NE(madeData, nullptr);
		ASSERT_NE(loadedData, nullptr);
		EXPECT_EQ(loadedData->path.from, madeData->path.from);
		EXPECT_EQ(loadedData->path.to, madeData->path.to);
		EXPECT_EQ(loadedData->path.side, madeData->path.side);
		EXPECT_EQ(loadedData->path.cubic, madeData->path.cubic);
		EXPECT_EQ(loadedData->path.square, madeData->path.square);
		EXPECT_EQ(loadedData->path.linear, madeData->path.linear);
		EXPECT_EQ(loadedData->path.constant, madeData->path.constant);
		EXPECT_EQ(loadedData->path.length, madeData->path.length);
		EXPECT_EQ(loadedData->path.amplitude, madeData->path.amplitude);
		EXPECT_EQ(loadedData->path.frequency, madeData->path.frequency);
		EXPECT_EQ(loadedData->path.phase, madeData->path.phase);
		EXPECT_EQ(loadedData->progress, madeData->progress);
	}
	const auto madeEffect = std::move(running.effect);
	running.effect = std::move(restored);
	const auto second = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(second.Complete()) << second.failure;
	EXPECT_EQ(second.bytes, first.bytes);
	madeEffect->Step(0.1f);
	running.effect->Step(0.1f);
	for (size_t i = 0; i < made.size(); ++i)
	{
		EXPECT_EQ(loaded[i]->position, made[i]->position);
	}
}

namespace
{
/// A file of the game's data, or nothing without it
std::optional<std::vector<uint8_t>> GameFile(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto path = std::filesystem::path(game) / relative;
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		return std::nullopt;
	}
	std::ifstream file(path, std::ios::binary);
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

/// The record of the class among the effect's parts that equals the one given, its object's own part aside
template <typename Record>
bool HasPart(const ecs::saved_particles::PlannedEffect& planned, Record expected)
{
	return std::ranges::any_of(planned.parts, [&expected](const auto& part) {
		const auto* record = std::get_if<Record>(&part.second);
		if (record == nullptr)
		{
			return false;
		}
		expected.thing = record->thing;
		return expected == *record;
	});
}
} // namespace

// The frozen slot 0 save's two magic beams (SF_SimpleBeam) are made again from their records, and what openblack keeps
// of their chains and beams gives back the original's records: each chain with its layout, v-scroll and twenty joints
// as the last frame drew them, each joint's particle, each beam's spline with its second derivatives, number and count
// of beams, and each beam atom's data, bit for bit
TEST(ParticleWrite, TheFrozenSlotZeroMagicBeamsGiveBackTheirChainsAndBeamsBitForBit)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "0" / "0.sav");
	const auto spellFile = GameFile(std::filesystem::path("Data") / "Spells" / "ZSpellFiles" / "SF_SimpleBeam_txt.zzz");
	if (!bytes.has_value() || !spellFile.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen slot 0 save or no SF_SimpleBeam file";
	}
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	Locator::particleSystem::emplace<test::InertParticleSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	Locator::entitiesRegistry::emplace<ecs::Registry>();
	const test::RestoreService<Locator::filesystem> restoreFiles;
	auto& files = static_cast<test::MemoryFileSystem&>(Locator::filesystem::emplace<test::MemoryFileSystem>());
	files.files[files.GetPath<filesystem::Path::Data>() / "Spells" / "ZSpellFiles" / "SF_SimpleBeam_txt.zzz"] = *spellFile;
	const test::RestoreService<Locator::resources> restoreResources;
	Locator::resources::reset();
	const auto file = particles::File::Load("SF_SimpleBeam");
	ASSERT_NE(file, nullptr);

	const auto world = gamesave::ReadWorld(*bytes);
	const ecs::saved_particles::EffectLinks links {.playerOf = [](ObjectNumber /*number*/) { return -1; },
	                                               .entityOf = [](ObjectNumber /*number*/) { return entt::null; }};
	size_t beams = 0;
	size_t chains = 0;
	size_t joints = 0;
	size_t carriers = 0;
	for (const auto& object : world.objects)
	{
		const auto* container = object.has_value() ? std::get_if<ParticleContainerRecord>(&*object) : nullptr;
		if (container == nullptr)
		{
			continue;
		}
		const auto planned = ecs::saved_particles::PlanEffect(world, container->system.number);
		if (!planned.has_value() || planned->manager.persistent.first != k_MagicBeamType)
		{
			continue;
		}
		std::string why;
		const auto effect = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
		ASSERT_NE(effect, nullptr) << why;
		const std::function<void(const particles::Collection&)> check = [&](const particles::Collection& collection) {
			if (collection.chainCreator != nullptr)
			{
				++chains;
				EXPECT_TRUE(HasPart(*planned, particle_codecs::ChainValues(collection, k_MagicBeamType)));
			}
			for (const auto* rule : collection.dataMade)
			{
				if (rule->SavedData() == particles::RuleData::SimpleBeam)
				{
					++beams;
					const auto& ribbon = *std::static_pointer_cast<particles::beam::Ribbon>(collection.modifierData.at(rule));
					EXPECT_TRUE(HasPart(*planned, particle_codecs::BeamRibbonValues(ribbon, *rule, k_MagicBeamType)));
				}
			}
			for (const auto& atom : collection.atoms)
			{
				if (atom->chainJoint >= 0)
				{
					++joints;
					const auto expected = particle_codecs::ChainJointValues(*atom);
					EXPECT_TRUE(std::ranges::any_of(planned->parts, [&expected](const auto& part) {
						const auto* record = std::get_if<gamesave::ParticleChainJointRecord>(&part.second);
						return record != nullptr && record->renderByte == expected.renderByte &&
						       record->words == expected.words;
					}));
				}
				for (const auto* rule : atom->dataMade)
				{
					if (rule->SavedData() == particles::RuleData::SimpleBeam)
					{
						++carriers;
						const auto& target = *std::static_pointer_cast<particles::beam::Target>(atom->modifierData.at(rule));
						EXPECT_TRUE(HasPart(*planned, particle_codecs::BeamAtomDataValues(target, *rule, k_MagicBeamType)));
					}
				}
				for (const auto& sub : atom->subCollections)
				{
					check(*sub);
				}
			}
		};
		for (const auto& root : effect->GetRoots())
		{
			check(*root);
		}
	}
	EXPECT_EQ(chains, 6u);
	EXPECT_EQ(beams, 6u);
	EXPECT_EQ(joints, 120u);
	EXPECT_EQ(carriers, 2u);
}

// The Land 1 save 14/1's landscape vortex (record 3268) draws in three cows with its object mover
// (SF_LandscapeVortexObjectMover, manager 3271): the mover is made again from its records, each of its three particles carrying
// its cow back, with the turn it was last found available on, the byte its take cleared and its rule's data, and they give back
// the original's game object particles (3275, 3278, 3281) and vortex attract data (3274, 3277, 3280) bit for bit. The carriers
// hold their cows only once the vortex takes the mover (vortex_attract::TakeRestoredCarriers)
TEST(ParticleWrite, TheFrozenLandOneVortexsMoverGivesBackItsThreeCowsCarriersBitForBit)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "14" / "1" / "14.sav");
	const auto spellFile =
	    GameFile(std::filesystem::path("Data") / "Spells" / "ZSpellFiles" / "SF_LandscapeVortexObjectMover_txt.zzz");
	if (!bytes.has_value() || !spellFile.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen save 14/1 or no SF_LandscapeVortexObjectMover file";
	}
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	Locator::particleSystem::emplace<test::InertParticleSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const test::RestoreService<Locator::filesystem> restoreFiles;
	auto& files = static_cast<test::MemoryFileSystem&>(Locator::filesystem::emplace<test::MemoryFileSystem>());
	files.files[files.GetPath<filesystem::Path::Data>() / "Spells" / "ZSpellFiles" / "SF_LandscapeVortexObjectMover_txt.zzz"] =
	    *spellFile;
	const test::RestoreService<Locator::resources> restoreResources;
	Locator::resources::reset();
	const auto file = particles::File::Load("SF_LandscapeVortexObjectMover");
	ASSERT_NE(file, nullptr);

	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_GE(world.objects.size(), 3281u);
	const auto& vortex = world.objects[3268 - 1];
	ASSERT_TRUE(vortex.has_value());
	const auto* in = std::get_if<gamesave::LandscapeVortexInRecord>(&*vortex);
	ASSERT_NE(in, nullptr);
	EXPECT_EQ(in->objects[1].number, 3270u);
	const auto planned = ecs::saved_particles::PlanEffect(world, in->objects[1].number);
	ASSERT_TRUE(planned.has_value());
	EXPECT_EQ(planned->managerNumber, 3271u);
	constexpr uint32_t k_MoverType = 76;
	EXPECT_EQ(planned->manager.persistent.first, k_MoverType);

	// the three cows, made before the mover as the load makes them
	std::map<ObjectNumber, entt::entity> cows;
	for (const ObjectNumber number : {2081U, 2082U, 2083U})
	{
		cows.emplace(number, registry.Create());
	}
	const ecs::saved_particles::EffectLinks links {
	    .playerOf = [](ObjectNumber /*number*/) { return -1; },
	    .entityOf = [&cows](ObjectNumber number) { return cows.contains(number) ? cows.at(number) : entt::null; }};
	std::string why;
	auto effect = ecs::saved_particles::RestoreEffect(*planned, file, links, why);
	ASSERT_NE(effect, nullptr) << why;

	const auto numberOf = [&cows](entt::entity entity) -> ObjectNumber {
		for (const auto& [number, cow] : cows)
		{
			if (cow == entity)
			{
				return number;
			}
		}
		return 0;
	};
	size_t carried = 0;
	size_t data = 0;
	std::function<void(const particles::Collection&)> check = [&](const particles::Collection& collection) {
		for (const auto& atom : collection.atoms)
		{
			if (const auto values = particles::vortex_attract::CarriedOf(*atom); values.has_value())
			{
				++carried;
				EXPECT_EQ(values->availableTurn, 34001u);
				EXPECT_EQ(values->byte, 0);
				EXPECT_FALSE(values->letGo);
				auto expected = particle_codecs::GameObjectValues(*values, atom->savedDraw[1]);
				expected.objects[0] = {numberOf(values->object)};
				EXPECT_NE(expected.objects[0].number, 0u);
				const auto found = std::ranges::find_if(planned->parts, [&expected](const auto& part) {
					const auto* record = std::get_if<gamesave::ParticleGameObjectRecord>(&part.second);
					return record != nullptr && record->renderByte == expected.renderByte && record->word == expected.word &&
					       record->objects[0] == expected.objects[0] && record->zoomer == expected.zoomer &&
					       record->byte == expected.byte;
				});
				EXPECT_NE(found, planned->parts.end());
			}
			for (const auto* rule : atom->dataMade)
			{
				if (const auto* attract = particles::vortex_attract::DataOf(*atom, *rule); attract != nullptr)
				{
					++data;
					EXPECT_TRUE(HasPart(*planned, particle_codecs::VortexAttractDataValues(*attract, *rule, k_MoverType)));
				}
			}
			for (const auto& sub : atom->subCollections)
			{
				check(*sub);
			}
		}
	};
	for (const auto& root : effect->GetRoots())
	{
		check(*root);
	}
	EXPECT_EQ(carried, 3u);
	EXPECT_EQ(data, 3u);
	// a mover the load drops leaves its cows alone: its carriers did not hold them yet
	effect.reset();
	for (const auto& [number, cow] : cows)
	{
		EXPECT_TRUE(registry.Valid(cow) && !registry.AllOf<ecs::components::Unavailable>(cow)) << number;
	}
}
