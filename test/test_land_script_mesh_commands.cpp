/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land script's commands whose objects are sized from their mesh in the resource cache: CREATE_NEW_TOWN_FIELD and
// CREATE_TOWN_FIELD, CREATE_NEW_FEATURE and CREATE_FEATURE, CREATE_NEW_BIG_FOREST and CREATE_BIG_FOREST,
// CREATE_ANIMATED_STATIC and CREATE_WORSHIP_SITE. The synthetic tests use a hand-made info table and empty meshes put
// in the cache under the info's mesh (the worship site's mesh with its special points); the real-data tests, which
// skip themselves without OPENBLACK_GAME_PATH, read the game's info.dat and its meshes from Data/AllMeshes.g3d. Then
// the one-shot miracles and the miracle dispensers: CREATE_ONE_SHOT_SPELL_PU, CREATE_ONE_SHOT_SPELL and
// CREATE_SPELL_DISPENSER, the particles inert.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <L3DFile.h>
#include <PackFile.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/ObjectMatrix.h"
#include "Common/EventManager.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/Archetypes/TownArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Trees.h"
#include "EngineConfig.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/Resources.h"
#include "Worship/Citadel.h"
#include "support/BgfxShutdown.h"
#include "support/LandFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::archetypes::CitadelArchetype;
using openblack::ecs::archetypes::TownArchetype;
using openblack::lhscriptx::FeatureScriptCommands;
using openblack::lhscriptx::Script;

namespace
{
// The rows of the hand-made info table
constexpr auto k_FieldRow = AbodeInfo::NorseField;
constexpr auto k_FeatureRow = static_cast<FeatureInfo>(5);
constexpr auto k_FlowersRow = FlowersInfo::Pink;
constexpr auto k_ForestRow = BigForestInfo::Info1;
constexpr auto k_GateRow = AnimatedStaticInfo::NorseGate;
constexpr auto k_CaveRow = static_cast<AnimatedStaticInfo>(2);
/// The mesh every hand-made row but the gate's wears: an empty mesh in the cache
constexpr auto k_PlainMesh = MeshId::BuildingCampfire;
/// The worship site's mesh in the cache, as the temple's meshes are loaded
constexpr auto k_SiteMesh = entt::hashed_string("temple/B_WORSHIP_l3d");
/// The wood of each big forest row the game ships (docs/wiki/beings/villager-jobs.md#forests-and-big-forests)
constexpr uint32_t k_ShippedForestWood = 15000;
/// A sixth slot's share of the turn around the temple: two pi over seven
constexpr float k_SlotTurn = 2.0f * std::numbers::pi_v<float> / 7.0f;

/// The bytes of an L3D file with no sub-mesh and no skin, holding only the special points given (the extra metrics
/// block: its size, the count, where the metrics start, then each metric, an unturned matrix and the point), laid out
/// as test_building_sites.cpp's yard mesh. With no point it is an empty mesh
std::vector<uint8_t> MeshOfPoints(const std::vector<glm::vec3>& points)
{
	constexpr uint32_t k_None = std::numeric_limits<uint32_t>::max();
	using Metric = std::array<float, 12>;
	const auto count = static_cast<uint32_t>(points.size());
	const auto metricsAt = static_cast<uint32_t>(sizeof(l3d::L3DHeader) + (3 * sizeof(uint32_t)));
	const std::array<uint32_t, 3> block {static_cast<uint32_t>((3 * sizeof(uint32_t)) + (count * sizeof(Metric))), count,
	                                     metricsAt};
	std::vector<uint8_t> file(points.empty() ? sizeof(l3d::L3DHeader) : metricsAt + (count * sizeof(Metric)));
	l3d::L3DHeader header {};
	std::memcpy(header.magic.data(), "L3D0", header.magic.size());
	header.flags = points.empty() ? l3d::L3DMeshFlags::None : l3d::L3DMeshFlags::ContainsExtraMetrics;
	header.size = static_cast<uint32_t>(file.size());
	header.submeshOffsetsOffset = k_None;
	header.skinOffsetsOffset = k_None;
	header.extraDataOffset = k_None;
	header.footprintDataOffset = points.empty() ? k_None : static_cast<uint32_t>(sizeof(l3d::L3DHeader));
	std::memcpy(file.data(), &header, sizeof(header));
	if (!points.empty())
	{
		std::memcpy(file.data() + sizeof(header), block.data(), sizeof(block));
		for (size_t i = 0; i < points.size(); ++i)
		{
			const auto& point = points[i];
			const Metric metric {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, point.x, point.y, point.z};
			std::memcpy(file.data() + metricsAt + (i * sizeof(Metric)), metric.data(), sizeof(Metric));
		}
	}
	return file;
}

/// The row's description, as info.dat holds it
template <typename Row>
void Name(Row& row, std::string_view description)
{
	row.debugString.fill('\0');
	std::ranges::copy(description, row.debugString.begin());
}

/// The install the real-data tests read, from OPENBLACK_GAME_PATH; none when it is not set
std::optional<std::filesystem::path> RealGamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return std::filesystem::path(game);
}

/// info.dat's table as the game reads it (after the file's header), or nothing when the file is missing or of another
/// size
std::unique_ptr<InfoConstants> ReadRealInfoDat(const std::filesystem::path& game)
{
	std::ifstream file(game / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return nullptr;
	}
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (data.size() != 0x2C + sizeof(InfoConstants))
	{
		return nullptr;
	}
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	return info;
}

class LandScriptMeshCommandsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the commands log their misses to the scripts' logger, the worship sites to the game's: null sinks
		for (const auto* name : {"scripting", "game"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_mt>(name);
			}
		}
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		// the Norse field row; every other abode row is a Celtic house, which no field takes
		auto& field = info->abode.at(static_cast<size_t>(k_FieldRow));
		field.tribeType = Tribe::NORSE;
		field.abodeNumber = AbodeNumber::Field;
		field.abodeType = AbodeType::Field;
		field.meshId = k_PlainMesh;
		Name(field, "ABODE_FIELD");
		auto& feature = info->feature.at(static_cast<size_t>(k_FeatureRow));
		feature.meshId = k_PlainMesh;
		Name(feature, "PILAR3 LIME");
		info->flowers.at(static_cast<size_t>(k_FlowersRow)).meshId = k_PlainMesh;
		auto& forest = info->bigForest.at(static_cast<size_t>(k_ForestRow));
		forest.meshId = k_PlainMesh;
		forest.woodValue = k_ShippedForestWood;
		auto& gate = info->animatedStatic.at(static_cast<size_t>(k_GateRow));
		gate.meshId = MeshId::BuildingNorseGate;
		gate.defaultAnim = static_cast<AnimId>(191);
		Name(gate, "Norse Gate");
		auto& cave = info->animatedStatic.at(static_cast<size_t>(k_CaveRow));
		cave.meshId = k_PlainMesh;
		cave.defaultAnim = static_cast<AnimId>(189);
		Name(cave, "Piper Cave Entrance");
		// the temple's heart starts whole
		info->citadelHeart.startLife = 1.0f;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		Locator::events::emplace<EventManager>();
		// a flat dry land at height 0
		Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2) { return 0.0f; });
		// the meshes the objects are sized from: empty ones, and the worship site's with its special points, the dance
		// centre (8) and the place the villagers arrive at (9) in front of it
		Locator::resources::emplace<resources::Resources>();
		auto& meshes = Locator::resources::value().GetMeshes();
		for (const auto mesh : {k_PlainMesh, MeshId::BuildingNorseGate})
		{
			meshes.Load(mesh, resources::L3DLoader::FromBufferTag {}, std::string("test empty mesh"), MeshOfPoints({}));
		}
		std::vector<glm::vec3> sitePoints(11, glm::vec3(0.0f));
		sitePoints.at(8) = {0.0f, 0.0f, -30.0f};
		sitePoints.at(9) = {0.0f, 0.0f, -40.0f};
		meshes.Load(k_SiteMesh.value(), resources::L3DLoader::FromBufferTag {}, std::string("test worship site"),
		            MeshOfPoints(sitePoints));
	}

	void TearDown() override
	{
		Script::SetLastCreated(entt::null);
		Locator::terrainSystem::reset();
		Locator::events::reset();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
		Locator::resources::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// Every entity with the component, in the registry's order
	template <typename Component>
	static std::vector<entt::entity> All()
	{
		std::vector<entt::entity> found;
		Reg().Each<const Component>([&found](entt::entity entity, const Component& /*component*/) { found.push_back(entity); });
		return found;
	}

	/// Something an earlier line made, as the last created object
	static entt::entity EarlierObject()
	{
		const auto earlier = Reg().Create();
		Script::SetLastCreated(earlier);
		return earlier;
	}
};

/// The same, reading the game's own info.dat and meshes
class LandScriptMeshCommandsRealDataTest: public LandScriptMeshCommandsTest
{
};

/// The mesh of the pack's index into the cache, as the game loads the pack
void LoadRealMesh(const pack::PackFile& pack, MeshId mesh)
{
	Locator::resources::value().GetMeshes().Load(mesh, resources::L3DLoader::FromBufferTag {}, std::string("real mesh"),
	                                             pack.GetMesh(static_cast<uint32_t>(mesh)));
}
} // namespace

// ---- Fields ---------------------------------------------------------------------------------------------------------

// CREATE_NEW_TOWN_FIELD makes a field of the numbered town: the town's tribe's field abode, the field kind it keeps,
// turned by the line's angle as it is, at scale 1, starting empty, and the last created object, as the original
// (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests, docs/wiki/world/objects.md#fields-field,
// docs/wiki/world/objects.md#making-a-field,
// docs/wiki/beings/towns.md#players-effects-and-towns-of-the-objects-involved,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, CreateNewTownFieldMakesTheTownsFieldOfItsKindAtTheAngleAsItIs)
{
	TownArchetype::Create(3, glm::vec3(3231.53f, 0.0f, 3396.45f), PlayerNames::PLAYER_ONE, Tribe::NORSE);
	EarlierObject();
	const glm::vec3 at(3264.61f, 0.0f, 3432.65f);
	FeatureScriptCommands::CreateNewTownField(3, at, static_cast<FieldTypeInfo>(2), 0.75f);
	const auto fields = All<Field>();
	ASSERT_EQ(fields.size(), 1u);
	const auto field = fields.front();
	EXPECT_EQ(Script::LastCreated(), field);
	const auto& made = Reg().Get<const Field>(field);
	EXPECT_EQ(made.town, 3);
	EXPECT_EQ(made.type, static_cast<FieldTypeInfo>(2));
	EXPECT_EQ(made.crops, 0u);
	EXPECT_EQ(made.growth, 0.0f);
	const auto& abode = Reg().Get<const Abode>(field);
	EXPECT_EQ(abode.info, k_FieldRow);
	EXPECT_EQ(abode.townId, 3u);
	EXPECT_EQ(abode.yAngle, 0.75f);
	const auto& transform = Reg().Get<const Transform>(field);
	EXPECT_EQ(transform.rotation, affine::AngleY(0.75f));
	EXPECT_EQ(transform.scale, glm::vec3(1.0f));
	EXPECT_EQ(transform.position.x, at.x);
	EXPECT_EQ(transform.position.z, at.z);
}

// CREATE_TOWN_FIELD is the same field of the town with no angle, as the original
// (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object,
// docs/wiki/world/objects.md#fields-field)
TEST_F(LandScriptMeshCommandsTest, CreateTownFieldMakesTheTownsFieldUnturned)
{
	TownArchetype::Create(3, glm::vec3(3231.53f, 0.0f, 3396.45f), PlayerNames::PLAYER_ONE, Tribe::NORSE);
	EarlierObject();
	FeatureScriptCommands::CreateTownField(3, glm::vec3(3252.30f, 0.0f, 3431.16f), static_cast<FieldTypeInfo>(4));
	const auto fields = All<Field>();
	ASSERT_EQ(fields.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), fields.front());
	EXPECT_EQ(Reg().Get<const Field>(fields.front()).town, 3);
	EXPECT_EQ(Reg().Get<const Field>(fields.front()).type, static_cast<FieldTypeInfo>(4));
	EXPECT_EQ(Reg().Get<const Abode>(fields.front()).yAngle, 0.0f);
	EXPECT_EQ(Reg().Get<const Transform>(fields.front()).scale, glm::vec3(1.0f));
}

// CREATE_TOWN_FIELD and CREATE_NEW_TOWN_FIELD with a number no town has make nothing and leave the last created object
// as it was, as the original (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests)
TEST_F(LandScriptMeshCommandsTest, ATownFieldOfNoTownMakesNothingAndKeepsTheLastCreatedObject)
{
	TownArchetype::Create(3, glm::vec3(3231.53f, 0.0f, 3396.45f), PlayerNames::PLAYER_ONE, Tribe::NORSE);
	const auto earlier = EarlierObject();
	FeatureScriptCommands::CreateTownField(9, glm::vec3(3252.30f, 0.0f, 3431.16f), static_cast<FieldTypeInfo>(4));
	FeatureScriptCommands::CreateNewTownField(9, glm::vec3(3264.61f, 0.0f, 3432.65f), static_cast<FieldTypeInfo>(2), 0.75f);
	EXPECT_TRUE(All<Field>().empty());
	EXPECT_EQ(Script::LastCreated(), earlier);
}

// CREATE_FIELD makes a field of no town, of its kind, unturned and at scale 1, with the Celtic field row, and the last
// created object, as the original (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests)
TEST_F(LandScriptMeshCommandsTest, CreateFieldMakesAFieldOfNoTownWithTheCelticFieldRow)
{
	// the fixture's table with a Celtic field row like its Norse one, and a town nearby, which the field does not join
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	auto& celtic = info->abode.at(static_cast<size_t>(AbodeInfo::CelticField));
	celtic = info->abode.at(static_cast<size_t>(k_FieldRow));
	celtic.tribeType = Tribe::CELTIC;
	Locator::infoConstants::reset(info.release());
	TownArchetype::Create(3, glm::vec3(3231.53f, 0.0f, 3396.45f), PlayerNames::PLAYER_ONE, Tribe::NORSE);
	EarlierObject();
	FeatureScriptCommands::CreateField(glm::vec3(3252.30f, 0.0f, 3431.16f), static_cast<FieldTypeInfo>(4));
	const auto fields = All<Field>();
	ASSERT_EQ(fields.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), fields.front());
	EXPECT_EQ(Reg().Get<const Field>(fields.front()).town, -1);
	EXPECT_EQ(Reg().Get<const Field>(fields.front()).type, static_cast<FieldTypeInfo>(4));
	EXPECT_EQ(Reg().Get<const Abode>(fields.front()).info, AbodeInfo::CelticField);
	EXPECT_EQ(Reg().Get<const Abode>(fields.front()).townId, Abode::k_NoTown);
	EXPECT_EQ(Reg().Get<const Abode>(fields.front()).yAngle, 0.0f);
	EXPECT_EQ(Reg().Get<const Transform>(fields.front()).scale, glm::vec3(1.0f));
}

// ---- Features -------------------------------------------------------------------------------------------------------

// CREATE_NEW_FEATURE makes the feature whose description is the line's name, whatever its case, at the line's place,
// turned and sized by its whole numbers in thousandths, and the last created object, as the original
// (docs/wiki/scripts/map-script-objects.md#pots-features-and-flowers,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, CreateNewFeatureMakesTheNamedFeatureTurnedAndSizedInThousandths)
{
	EarlierObject();
	const glm::vec3 at(2031.32f, 0.0f, 2325.22f);
	FeatureScriptCommands::CreateNewFeature(at, "Pilar3 Lime", 1047, 474, 0);
	const auto features = All<Feature>();
	ASSERT_EQ(features.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), features.front());
	EXPECT_EQ(Reg().Get<const Feature>(features.front()).type, k_FeatureRow);
	const auto& transform = Reg().Get<const Transform>(features.front());
	EXPECT_EQ(transform.position, at);
	EXPECT_EQ(transform.rotation, affine::AngleY(1047 * 0.001f));
	EXPECT_EQ(transform.scale, glm::vec3(474 * 0.001f));
}

// CREATE_FEATURE makes the feature of the line's number the same way, as the original
// (docs/wiki/scripts/map-script-objects.md#pots-features-and-flowers,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, CreateFeatureMakesTheNumberedFeatureTurnedAndSizedInThousandths)
{
	EarlierObject();
	const glm::vec3 at(2566.27f, 0.0f, 3167.91f);
	FeatureScriptCommands::CreateFeature(at, k_FeatureRow, 981, 577, 0);
	const auto features = All<Feature>();
	ASSERT_EQ(features.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), features.front());
	EXPECT_EQ(Reg().Get<const Feature>(features.front()).type, k_FeatureRow);
	const auto& transform = Reg().Get<const Transform>(features.front());
	EXPECT_EQ(transform.position, at);
	EXPECT_EQ(transform.rotation, affine::AngleY(981 * 0.001f));
	EXPECT_EQ(transform.scale, glm::vec3(577 * 0.001f));
}

// CREATE_FLOWERS "ANFF" makes a patch of the flowers of the line's number at the line's place, turned and sized by its
// two floats as they are, not in thousandths as a feature's whole numbers, and the last created object, as the original
// (docs/wiki/scripts/map-script-objects.md#pots-features-and-flowers,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, CreateFlowersTakesItsAngleAndScaleAsTheyAre)
{
	EarlierObject();
	const glm::vec3 at(1500.0f, 0.0f, 2200.0f);
	FeatureScriptCommands::CreateFlowers(at, k_FlowersRow, 0.75f, 1.5f);
	const auto patches = All<Flowers>();
	ASSERT_EQ(patches.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), patches.front());
	EXPECT_EQ(Reg().Get<const Flowers>(patches.front()).type, k_FlowersRow);
	const auto& transform = Reg().Get<const Transform>(patches.front());
	EXPECT_EQ(transform.position, at);
	EXPECT_EQ(transform.rotation, affine::AngleY(0.75f));
	EXPECT_EQ(transform.scale, glm::vec3(1.5f));
}

// ---- Big forests ----------------------------------------------------------------------------------------------------

// CREATE_NEW_BIG_FOREST makes the big forest of the line's number, turned and sized by its numbers as they are, holding
// its row's wood at its scale, with a forest of its own that points back at it, and the last created object, as the
// original (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests,
// docs/wiki/beings/villager-jobs.md#forests-and-big-forests,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, CreateNewBigForestHoldsItsRowsWoodAtItsScaleAndOwnsItsForest)
{
	EarlierObject();
	const glm::vec3 at(1970.70f, 0.0f, 2987.58f);
	FeatureScriptCommands::CreateNewBigForest(at, k_ForestRow, 0, 2.421f, 0.784f);
	const auto forests = All<BigForest>();
	ASSERT_EQ(forests.size(), 1u);
	const auto forest = forests.front();
	EXPECT_EQ(Script::LastCreated(), forest);
	const auto& made = Reg().Get<const BigForest>(forest);
	EXPECT_EQ(made.woodValue, static_cast<float>(k_ShippedForestWood));
	EXPECT_EQ(made.wood, static_cast<float>(k_ShippedForestWood) * 0.784f);
	EXPECT_EQ(ecs::ForestBigForest(made.forestId), forest);
	const auto& transform = Reg().Get<const Transform>(forest);
	EXPECT_EQ(transform.position, at);
	EXPECT_EQ(transform.rotation, affine::AngleY(2.421f));
	EXPECT_EQ(transform.scale, glm::vec3(0.784f));
}

// CREATE_BIG_FOREST, the older form, makes the same big forest from its angle and scale as they are, as the original
// (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests,
// docs/wiki/beings/villager-jobs.md#forests-and-big-forests,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, CreateBigForestTakesItsAngleAndScaleAsTheyAre)
{
	EarlierObject();
	const glm::vec3 at(2133.05f, 0.0f, 2283.50f);
	FeatureScriptCommands::CreateBigForest(at, k_ForestRow, 0.13f, 0.991f);
	const auto forests = All<BigForest>();
	ASSERT_EQ(forests.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), forests.front());
	EXPECT_EQ(Reg().Get<const BigForest>(forests.front()).wood, static_cast<float>(k_ShippedForestWood) * 0.991f);
	EXPECT_EQ(ecs::ForestBigForest(Reg().Get<const BigForest>(forests.front()).forestId), forests.front());
	const auto& transform = Reg().Get<const Transform>(forests.front());
	EXPECT_EQ(transform.rotation, affine::AngleY(0.13f));
	EXPECT_EQ(transform.scale, glm::vec3(0.991f));
}

// ---- Animated statics -----------------------------------------------------------------------------------------------

// CREATE_ANIMATED_STATIC makes the animated object whose description is the line's name, whatever its case, turned and
// sized in thousandths, with its row's clip, and the last created object, as the original
// (docs/wiki/scripts/map-script-objects.md#animated-statics,
// docs/wiki/scripts/map-script-objects.md#pots-features-and-flowers,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, CreateAnimatedStaticMakesTheNamedObjectWithItsRowsClip)
{
	EarlierObject();
	const glm::vec3 at(1970.80f, 0.0f, 2812.12f);
	FeatureScriptCommands::CreateAnimatedStatic(at, "PIPER CAVE ENTRANCE", -2302, 1000);
	const auto made = All<AnimatedStatic>();
	ASSERT_EQ(made.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), made.front());
	EXPECT_EQ(Reg().Get<const AnimatedStatic>(made.front()).type, k_CaveRow);
	ASSERT_TRUE(Reg().AllOf<SkeletalAnimation>(made.front()));
	EXPECT_EQ(Reg().Get<const SkeletalAnimation>(made.front()).clipIndex, 189);
	const auto& transform = Reg().Get<const Transform>(made.front());
	EXPECT_EQ(transform.position, at);
	EXPECT_EQ(transform.rotation, affine::AngleY(-2302 * 0.001f));
	EXPECT_EQ(transform.scale, glm::vec3(1000 * 0.001f));
	// only the Norse gate has lamps
	EXPECT_TRUE(All<GateLamp>().empty());
}

// The Norse gate made by CREATE_ANIMATED_STATIC has two lamps of the town light's mesh, unturned at scale 1, 15 m
// either side of it along its own width and 30 m up, as the original (docs/wiki/scripts/map-script-objects.md#animated-statics)
TEST_F(LandScriptMeshCommandsTest, TheNorseGateHasTwoUnturnedLampsFifteenMetresEitherSideAndThirtyUp)
{
	const glm::vec3 at(1781.03f, 0.0f, 2810.63f);
	FeatureScriptCommands::CreateAnimatedStatic(at, "Norse Gate", 5953, 1000);
	const auto gates = All<AnimatedStatic>();
	ASSERT_EQ(gates.size(), 1u);
	EXPECT_EQ(Reg().Get<const AnimatedStatic>(gates.front()).type, k_GateRow);
	EXPECT_EQ(Reg().Get<const SkeletalAnimation>(gates.front()).clipIndex, 191);
	const auto lamps = All<GateLamp>();
	ASSERT_EQ(lamps.size(), 2u);
	glm::vec2 middle(0.0f);
	for (const auto lamp : lamps)
	{
		EXPECT_EQ(Reg().Get<const GateLamp>(lamp).gate, gates.front());
		EXPECT_EQ(Reg().Get<const Mesh>(lamp).id, resources::HashIdentifier(MeshId::ObjectTownLight));
		const auto& transform = Reg().Get<const Transform>(lamp);
		EXPECT_EQ(transform.rotation, glm::mat3(1.0f));
		EXPECT_EQ(transform.scale, glm::vec3(1.0f));
		EXPECT_NEAR(transform.position.y, at.y + 30.0f, 1e-3f);
		EXPECT_NEAR(glm::distance(glm::vec2(transform.position.x, transform.position.z), glm::vec2(at.x, at.z)), 15.0f, 1e-3f);
		middle += glm::vec2(transform.position.x, transform.position.z) * 0.5f;
	}
	// one on each side
	EXPECT_NEAR(middle.x, at.x, 1e-3f);
	EXPECT_NEAR(middle.y, at.z, 1e-3f);
}

// ---- Worship sites --------------------------------------------------------------------------------------------------

// CREATE_WORSHIP_SITE makes the site of the line's tribe for the named player's temple: owned by that player, in a slot
// of the temple, standing at the temple's origin turned to the slot (the temple's angle plus the slot's sevenths of a
// turn) whatever place the line gives, holding the player's town of that tribe once. That town has no building site of
// it, so the site stays unbuilt. It is the last created object, as the original (Land 2's Khazar line;
// docs/wiki/magic/temple-and-citadel.md#the-citadel-and-its-six-slots,
// docs/wiki/scripts/map-script-reader.md#towns-and-players,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, AWorshipSiteIsTheTribesSiteOfThePlayersTempleNotTheLinesPlace)
{
	const glm::vec3 temple(2509.12f, 0.0f, 1779.93f);
	CitadelArchetype::Create(temple, PlayerNames::PLAYER_TWO, glm::mat4(1.0f), glm::vec3(1.0f));
	const auto town = TownArchetype::Create(1, glm::vec3(2761.43f, 0.0f, 1980.72f), PlayerNames::PLAYER_TWO, Tribe::NORSE);
	EarlierObject();
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(2541.44f, 0.0f, 1808.88f), 7, "PLAYER_TWO", "NORSE", 8151, 1000);
	const auto sites = All<WorshipSite>();
	ASSERT_EQ(sites.size(), 1u);
	const auto site = sites.front();
	EXPECT_EQ(Script::LastCreated(), site);
	const auto citadel = worship::citadel::Of(PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(citadel != entt::null);
	const auto& made = Reg().Get<const WorshipSite>(site);
	EXPECT_EQ(made.player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(made.tribe, Tribe::NORSE);
	EXPECT_EQ(made.citadel, citadel);
	const auto& slots = Reg().Get<const CitadelWorship>(citadel);
	ASSERT_LT(made.slot, slots.sites.size());
	EXPECT_EQ(slots.sites.at(made.slot), site);
	EXPECT_FLOAT_EQ(made.yAngle, slots.heartYAngle + (static_cast<float>(made.slot) * k_SlotTurn));
	const auto& transform = Reg().Get<const Transform>(site);
	EXPECT_EQ(transform.position.x, temple.x);
	EXPECT_EQ(transform.position.z, temple.z);
	EXPECT_EQ(std::ranges::count(made.towns, town), 1);
	EXPECT_FALSE(ecs::abodes::IsBuilt(site));
}

// A second CREATE_WORSHIP_SITE of the tribe finds the same site; the town of the tribe has a building site of it, so the
// site is finished and the building site removed, as the original
// (docs/wiki/magic/temple-and-citadel.md#the-citadel-and-its-six-slots)
TEST_F(LandScriptMeshCommandsTest, ATownsBuildingSiteOfTheWorshipSiteIsFinishedAndRemoved)
{
	CitadelArchetype::Create(glm::vec3(2509.12f, 0.0f, 1779.93f), PlayerNames::PLAYER_TWO, glm::mat4(1.0f), glm::vec3(1.0f));
	const auto town = TownArchetype::Create(1, glm::vec3(2761.43f, 0.0f, 1980.72f), PlayerNames::PLAYER_TWO, Tribe::NORSE);
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(2541.44f, 0.0f, 1808.88f), 7, "PLAYER_TWO", "NORSE", 8151, 1000);
	const auto site = Script::LastCreated();
	ASSERT_TRUE(site != entt::null);
	ASSERT_TRUE(ecs::building_sites::AddBuildingSite(town, site) != entt::null);
	EarlierObject();
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(2541.44f, 0.0f, 1808.88f), 7, "PLAYER_TWO", "NORSE", 8151, 1000);
	EXPECT_EQ(Script::LastCreated(), site);
	EXPECT_EQ(All<WorshipSite>().size(), 1u);
	EXPECT_TRUE(ecs::abodes::IsBuilt(site));
	EXPECT_TRUE(ecs::building_sites::GetBuildingSiteInList(town, site) == entt::null);
}

// With no town of its tribe, CREATE_WORSHIP_SITE still makes the player's site of the tribe, built at once, and the last
// created object is none, as the original (Land 2's Lethys line;
// docs/wiki/magic/temple-and-citadel.md#the-citadel-and-its-six-slots,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMeshCommandsTest, WithNoTownOfItsTribeTheWorshipSiteIsBuiltAndNothingIsTheLastCreated)
{
	CitadelArchetype::Create(glm::vec3(1006.36f, 0.0f, 3677.02f), PlayerNames::PLAYER_THREE, glm::mat4(1.0f), glm::vec3(1.0f));
	EarlierObject();
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(1049.73f, 0.0f, 3675.78f), 0, "PLAYER_THREE", "CELTIC", 1110, 1000);
	EXPECT_TRUE(Script::LastCreated() == entt::null);
	const auto sites = All<WorshipSite>();
	ASSERT_EQ(sites.size(), 1u);
	EXPECT_EQ(Reg().Get<const WorshipSite>(sites.front()).player, PlayerNames::PLAYER_THREE);
	EXPECT_EQ(Reg().Get<const WorshipSite>(sites.front()).tribe, Tribe::CELTIC);
	EXPECT_TRUE(ecs::abodes::IsBuilt(sites.front()));
}

// Each tribe's CREATE_WORSHIP_SITE makes a site of its own in a slot of its own around the player's temple, all owned
// by the player, as the original (Land 5's Nemesis lines; docs/wiki/magic/temple-and-citadel.md#the-citadel-and-its-six-slots)
TEST_F(LandScriptMeshCommandsTest, EachTribeGetsItsOwnWorshipSiteAroundTheTemple)
{
	CitadelArchetype::Create(glm::vec3(3951.38f, 0.0f, 2348.26f), PlayerNames::PLAYER_TWO, glm::mat4(1.0f), glm::vec3(1.0f));
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(3969.56f, 0.0f, 2308.86f), 3, "PLAYER_TWO", "JAPANESE", 0, 1000);
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(3908.93f, 0.0f, 2339.30f), 6, "PLAYER_TWO", "GREEK", 4487, 1000);
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(3952.09f, 0.0f, 2391.64f), 8, "PLAYER_TWO", "TIBETAN", 2692, 1000);
	FeatureScriptCommands::CreateWorshipSite(glm::vec3(3917.91f, 0.0f, 2375.87f), 2, "PLAYER_TWO", "AZTEC", 3590, 1000);
	const auto sites = All<WorshipSite>();
	ASSERT_EQ(sites.size(), 4u);
	const auto citadel = worship::citadel::Of(PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(citadel != entt::null);
	const auto& slots = Reg().Get<const CitadelWorship>(citadel);
	std::set<Tribe> tribes;
	std::set<uint8_t> taken;
	for (const auto site : sites)
	{
		const auto& made = Reg().Get<const WorshipSite>(site);
		EXPECT_EQ(made.player, PlayerNames::PLAYER_TWO);
		EXPECT_EQ(made.citadel, citadel);
		ASSERT_LT(made.slot, slots.sites.size());
		EXPECT_EQ(slots.sites.at(made.slot), site);
		tribes.insert(made.tribe);
		taken.insert(made.slot);
	}
	EXPECT_EQ(tribes, (std::set<Tribe> {Tribe::JAPANESE, Tribe::GREEK, Tribe::TIBETAN, Tribe::AZTEC}));
	EXPECT_EQ(taken.size(), 4u);
}

// ---- With the game's data -------------------------------------------------------------------------------------------

// With OPENBLACK_GAME_PATH set to the install: the animated objects the land scripts name are info.dat's rows of those
// descriptions, whatever the case, with the Norse gate's mesh and the clips 191, 195 and 189, as the original
// (docs/wiki/scripts/map-script-objects.md#animated-statics,
// docs/wiki/scripts/map-script-objects.md#pots-features-and-flowers)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LandScriptMeshCommandsRealDataTest, TheLandScriptsAnimatedStaticNamesFindTheirRowsAndClips)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	Locator::infoConstants::reset(info.release());
	const auto& rows = Locator::infoConstants::value().animatedStatic;
	const auto gate = GAnimatedStaticInfo::Find("Norse Gate");
	ASSERT_NE(gate, AnimatedStaticInfo::None);
	EXPECT_EQ(GAnimatedStaticInfo::Find("NORSE GATE"), gate);
	EXPECT_EQ(rows.at(static_cast<size_t>(gate)).meshId, MeshId::BuildingNorseGate);
	EXPECT_EQ(static_cast<int>(rows.at(static_cast<size_t>(gate)).defaultAnim), 191);
	const auto plinth = GAnimatedStaticInfo::Find("Gate Stone Plinth");
	ASSERT_NE(plinth, AnimatedStaticInfo::None);
	EXPECT_EQ(static_cast<int>(rows.at(static_cast<size_t>(plinth)).defaultAnim), 195);
	const auto cave = GAnimatedStaticInfo::Find("Piper Cave Entrance");
	ASSERT_NE(cave, AnimatedStaticInfo::None);
	EXPECT_EQ(static_cast<int>(rows.at(static_cast<size_t>(cave)).defaultAnim), 189);
}

// With OPENBLACK_GAME_PATH set to the install: Land 1's gate line, with the game's info.dat and the gate's mesh, makes
// the Norse gate playing clip 191, turned by 5.953 at scale 1, with its two lamps 15 m either side and 30 m up, as the
// original (docs/wiki/scripts/map-script-objects.md#animated-statics,
// docs/wiki/scripts/map-script-objects.md#pots-features-and-flowers)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LandScriptMeshCommandsRealDataTest, LandOnesNorseGateLineMakesTheGateWithItsClipAndLamps)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(*game / "Data" / "AllMeshes.g3d"), pack::PackResult::Success);
	ASSERT_GT(pack.GetMeshes().size(), static_cast<size_t>(MeshId::BuildingNorseGate));
	// the config is the test's only when it had none: then it goes with the test
	const test::RestoreService<Locator::config> config;
	if (!Locator::config::has_value())
	{
		Locator::config::emplace();
	}
	// the sub-meshes and skins build bgfx buffers and textures
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		// the game's meshes go before the bgfx shutdown, also after a failed ASSERT
		const test::RestoreService<Locator::resources> restoreResources;
		Locator::resources::emplace<resources::Resources>();
		Locator::infoConstants::reset(info.release());
		LoadRealMesh(pack, MeshId::BuildingNorseGate);
		const glm::vec3 at(1781.03f, 0.0f, 2810.63f);
		FeatureScriptCommands::CreateAnimatedStatic(at, "Norse Gate", 5953, 1000);
		const auto gates = All<AnimatedStatic>();
		ASSERT_EQ(gates.size(), 1u);
		EXPECT_EQ(Script::LastCreated(), gates.front());
		EXPECT_EQ(Reg().Get<const SkeletalAnimation>(gates.front()).clipIndex, 191);
		EXPECT_EQ(Reg().Get<const Mesh>(gates.front()).id, resources::HashIdentifier(MeshId::BuildingNorseGate));
		const auto& transform = Reg().Get<const Transform>(gates.front());
		EXPECT_EQ(transform.rotation, affine::AngleY(5953 * 0.001f));
		EXPECT_EQ(transform.scale, glm::vec3(1.0f));
		const auto lamps = All<GateLamp>();
		ASSERT_EQ(lamps.size(), 2u);
		for (const auto lamp : lamps)
		{
			const auto& placed = Reg().Get<const Transform>(lamp).position;
			EXPECT_NEAR(placed.y, at.y + 30.0f, 1e-3f);
			EXPECT_NEAR(glm::distance(glm::vec2(placed.x, placed.z), glm::vec2(at.x, at.z)), 15.0f, 1e-3f);
		}
	}
}

// With OPENBLACK_GAME_PATH set to the install: a Land 1 big forest line, with the game's info.dat and the forest's mesh,
// holds the row's 15000 wood at its scale 0.784, as the original
// (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests,
// docs/wiki/beings/villager-jobs.md#forests-and-big-forests)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LandScriptMeshCommandsRealDataTest, LandOnesScaledBigForestHoldsItsRowsWoodAtItsScale)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	const auto mesh = info->bigForest.at(static_cast<size_t>(BigForestInfo::Info1)).meshId;
	EXPECT_EQ(info->bigForest.at(static_cast<size_t>(BigForestInfo::Info1)).woodValue, k_ShippedForestWood);
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(*game / "Data" / "AllMeshes.g3d"), pack::PackResult::Success);
	ASSERT_GT(pack.GetMeshes().size(), static_cast<size_t>(mesh));
	const test::RestoreService<Locator::config> config;
	if (!Locator::config::has_value())
	{
		Locator::config::emplace();
	}
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		const test::RestoreService<Locator::resources> restoreResources;
		Locator::resources::emplace<resources::Resources>();
		Locator::infoConstants::reset(info.release());
		LoadRealMesh(pack, mesh);
		FeatureScriptCommands::CreateNewBigForest(glm::vec3(1970.70f, 0.0f, 2987.58f), BigForestInfo::Info1, 0, 2.421f, 0.784f);
		const auto forests = All<BigForest>();
		ASSERT_EQ(forests.size(), 1u);
		EXPECT_EQ(Script::LastCreated(), forests.front());
		EXPECT_EQ(Reg().Get<const BigForest>(forests.front()).wood, static_cast<float>(k_ShippedForestWood) * 0.784f);
		EXPECT_EQ(Reg().Get<const Mesh>(forests.front()).id, resources::HashIdentifier(mesh));
	}
}

// With OPENBLACK_GAME_PATH set to the install: Land 2's first field line, with the game's info.dat and the field's mesh,
// makes the field of the Norse town 0 from the Norse field row, of the line's kind, as the original
// (docs/wiki/scripts/map-script-objects.md#fields-and-big-forests, docs/wiki/world/objects.md#fields-field,
// docs/wiki/beings/towns.md#players-effects-and-towns-of-the-objects-involved)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LandScriptMeshCommandsRealDataTest, LandTwosNorseFieldIsTheNorseFieldRow)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	const auto& row = info->abode.at(static_cast<size_t>(AbodeInfo::NorseField));
	EXPECT_EQ(row.tribeType, Tribe::NORSE);
	EXPECT_EQ(row.abodeNumber, AbodeNumber::Field);
	const auto mesh = row.meshId;
	pack::PackFile pack;
	ASSERT_EQ(pack.Open(*game / "Data" / "AllMeshes.g3d"), pack::PackResult::Success);
	ASSERT_GT(pack.GetMeshes().size(), static_cast<size_t>(mesh));
	const test::RestoreService<Locator::config> config;
	if (!Locator::config::has_value())
	{
		Locator::config::emplace();
	}
	bgfx::renderFrame(); // single-threaded
	bgfx::Init init {};
	init.type = bgfx::RendererType::Noop;
	ASSERT_TRUE(bgfx::init(init));
	const test::BgfxShutdown bgfxShutdown;
	{
		const test::RestoreService<Locator::resources> restoreResources;
		Locator::resources::emplace<resources::Resources>();
		Locator::infoConstants::reset(info.release());
		LoadRealMesh(pack, mesh);
		TownArchetype::Create(0, glm::vec3(3231.53f, 0.0f, 3396.45f), PlayerNames::PLAYER_ONE, Tribe::NORSE);
		FeatureScriptCommands::CreateNewTownField(0, glm::vec3(3264.61f, 0.0f, 3432.65f), static_cast<FieldTypeInfo>(2), 0.0f);
		const auto fields = All<Field>();
		ASSERT_EQ(fields.size(), 1u);
		EXPECT_EQ(Script::LastCreated(), fields.front());
		EXPECT_EQ(Reg().Get<const Field>(fields.front()).town, 0);
		EXPECT_EQ(Reg().Get<const Field>(fields.front()).type, static_cast<FieldTypeInfo>(2));
		EXPECT_EQ(Reg().Get<const Abode>(fields.front()).info, AbodeInfo::NorseField);
		EXPECT_EQ(Reg().Get<const Mesh>(fields.front()).id, resources::HashIdentifier(mesh));
	}
}

// ---- One-shot miracles and miracle dispensers -----------------------------------------------------------------------

namespace
{
/// The seeds of the hand-made tables, where the game's file has them: the fire seed casts the fireball and its two
/// power-ups, the heal seed the heal and its power-up
constexpr auto k_FireSeed = static_cast<SpellSeedType>(2);
constexpr auto k_HealSeed = static_cast<SpellSeedType>(7);
/// The Celtic dispenser's row of the hand-made table
constexpr auto k_DispenserRow = AbodeInfo::CelticSpellDispenser;

/// Each magic record's own magic type set to where the record stands, as the game's file has them
void NumberMagicRecords(InfoConstants& info)
{
	uint32_t next = 0;
	const auto number = [&next](auto& records) {
		for (auto& record : records)
		{
			record.magicType = static_cast<MagicType>(next++);
		}
	};
	number(info.magicGeneral);
	number(info.magicHeal);
	number(info.magicTeleport);
	number(info.magicForest);
	number(info.magicFood);
	number(info.magicStormAndTornado);
	number(info.magicShield);
	number(info.magicWood);
	number(info.magicWater);
	number(info.magicFlockFlying);
	number(info.magicFlockGround);
	number(info.magicCreatureSpell);
}

/// The mesh commands' tables with the miracles named: the magic records numbered, the effect rows named NONE, FIRE,
/// FIRE_PU1, FIRE_PU2 and HEAL, the fire and heal seeds, and a Celtic dispenser row wearing the empty mesh; the
/// particles inert
class LandScriptMagicObjectsTest: public LandScriptMeshCommandsTest
{
protected:
	void SetUp() override
	{
		LandScriptMeshCommandsTest::SetUp();
		auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
		NumberMagicRecords(*info);
		Name(info->magicEffect.at(static_cast<size_t>(MagicType::None)), "NONE");
		Name(info->magicEffect.at(static_cast<size_t>(MagicType::Fireball)), "FIRE");
		Name(info->magicEffect.at(static_cast<size_t>(MagicType::FireballPowerUpOne)), "FIRE_PU1");
		Name(info->magicEffect.at(static_cast<size_t>(MagicType::FireballPowerUpTwo)), "FIRE_PU2");
		Name(info->magicEffect.at(static_cast<size_t>(MagicType::Heal)), "HEAL");
		auto& fire = info->spellSeed.at(static_cast<size_t>(k_FireSeed));
		Name(fire, "FIRE");
		fire.magicTypes = {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::None};
		auto& heal = info->spellSeed.at(static_cast<size_t>(k_HealSeed));
		Name(heal, "HEAL");
		heal.magicTypes = {MagicType::Heal, MagicType::HealPowerUpOne, MagicType::None, MagicType::None};
		auto& dispenser = info->abode.at(static_cast<size_t>(k_DispenserRow));
		dispenser.tribeType = Tribe::CELTIC;
		dispenser.abodeNumber = AbodeNumber::SpellDispenser;
		dispenser.abodeType = AbodeType::SpellDispenser;
		dispenser.meshId = k_PlainMesh;
		Name(dispenser, "ABODE_SPELL_DISPENSER");
		Locator::infoConstants::reset(info.release());
		Locator::particleSystem::emplace<test::InertParticleSystem>();
	}

	void TearDown() override
	{
		LandScriptMeshCommandsTest::TearDown();
		Locator::particleSystem::reset();
	}
};

/// The same, reading the game's own info.dat
class LandScriptMagicObjectsRealDataTest: public LandScriptMagicObjectsTest
{
};
} // namespace

// CREATE_ONE_SHOT_SPELL_PU(place, miracle) makes a one-shot orb of the first seed that casts the miracle named (without
// case), at the miracle's power-up level in that seed and at scale 1, and the orb is the last created object, as the
// original (docs/wiki/magic/spell-core.md#seeds-and-one-off-miracles,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMagicObjectsTest, CreateOneShotSpellPuMakesTheMiraclesFirstSeedAtItsLevel)
{
	EarlierObject();
	FeatureScriptCommands::CreateOneShotSpellPu(glm::vec3(1010.0f, 0.0f, 1020.0f), "fire_pu2");
	const auto orbs = All<OneOffSpellSeed>();
	ASSERT_EQ(orbs.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), orbs.front());
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(orbs.front()).seedType, k_FireSeed);
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(orbs.front()).powerUp, 1);
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(orbs.front()).scale, 1.0f);

	// a base miracle is its seed's base level
	FeatureScriptCommands::CreateOneShotSpellPu(glm::vec3(1030.0f, 0.0f, 1020.0f), "FIRE");
	ASSERT_NE(Script::LastCreated(), entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(Script::LastCreated()).seedType, k_FireSeed);
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(Script::LastCreated()).powerUp, -1);
	EXPECT_EQ(All<OneOffSpellSeed>().size(), 2u);
}

// The name of magic type 0, or a name no miracle has, makes no orb and leaves the last created object, as the original
// (docs/wiki/magic/spell-core.md#seeds-and-one-off-miracles)
TEST_F(LandScriptMagicObjectsTest, CreateOneShotSpellPuNeedsAMiracle)
{
	const auto earlier = EarlierObject();
	FeatureScriptCommands::CreateOneShotSpellPu(glm::vec3(1010.0f, 0.0f, 1020.0f), "NONE");
	FeatureScriptCommands::CreateOneShotSpellPu(glm::vec3(1010.0f, 0.0f, 1020.0f), "NOT_A_MIRACLE");
	EXPECT_TRUE(All<OneOffSpellSeed>().empty());
	EXPECT_EQ(Script::LastCreated(), earlier);
}

// CREATE_ONE_SHOT_SPELL(place, seed) makes a one-shot orb of the seed named (without case), at its base level and at
// scale 1, and the orb is the last created object, as the original
// (docs/wiki/magic/spell-core.md#seeds-and-one-off-miracles,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMagicObjectsTest, CreateOneShotSpellMakesTheNamedSeedAtItsBaseLevel)
{
	EarlierObject();
	FeatureScriptCommands::CreateOneShotSpell(glm::vec3(1010.0f, 0.0f, 1020.0f), "heal");
	const auto orbs = All<OneOffSpellSeed>();
	ASSERT_EQ(orbs.size(), 1u);
	EXPECT_EQ(Script::LastCreated(), orbs.front());
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(orbs.front()).seedType, k_HealSeed);
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(orbs.front()).powerUp, -1);
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(orbs.front()).scale, 1.0f);
}

// CREATE_SPELL_DISPENSER(town, place, building, miracle, angle, scale, period) makes the named dispenser building in
// the numbered town, gives it the miracle named, switches it on, which makes its orb at once (of the first seed that
// casts the miracle), and sets its period to the line's turns, truncated; the dispenser is the last created object, as
// the original (docs/wiki/magic/worship.md#the-dispenser-and-its-turn,
// docs/wiki/magic/worship.md#the-orbs-seed-and-the-magic-tables-filled-fields,
// docs/wiki/scripts/map-script-reader.md#the-command-callback-and-the-last-created-object)
TEST_F(LandScriptMagicObjectsTest, CreateSpellDispenserGivesItsMiracleAnOrbAtOnceAndItsPeriod)
{
	TownArchetype::Create(3, glm::vec3(3000.0f, 0.0f, 3150.0f), PlayerNames::PLAYER_ONE, Tribe::CELTIC);
	EarlierObject();
	FeatureScriptCommands::CreateSpellDispenser(3, glm::vec3(3014.44f, 0.0f, 3155.89f), "CELTIC_ABODE_SPELL_DISPENSER",
	                                            "FIRE_PU2", 0.0f, 1.0f, 600.7f);
	const auto dispensers = All<SpellDispenser>();
	ASSERT_EQ(dispensers.size(), 1u);
	const auto dispenser = dispensers.front();
	EXPECT_EQ(Script::LastCreated(), dispenser);
	EXPECT_EQ(Reg().Get<const Abode>(dispenser).townId, 3u);
	const auto& made = Reg().Get<const SpellDispenser>(dispenser);
	EXPECT_EQ(made.magicType, MagicType::FireballPowerUpTwo);
	EXPECT_EQ(made.timer.period, 600u);
	EXPECT_TRUE(made.timer.active);
	ASSERT_NE(made.orb, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(made.orb).seedType, k_FireSeed);
	EXPECT_EQ(All<OneOffSpellSeed>().size(), 1u);
}

// A period under one turn is none: the dispenser still makes its orb at once, then is switched off, as the original
// (docs/wiki/magic/worship.md#the-dispenser-and-its-turn)
TEST_F(LandScriptMagicObjectsTest, CreateSpellDispenserWithNoPeriodMakesItsOrbAndIsSwitchedOff)
{
	TownArchetype::Create(3, glm::vec3(3000.0f, 0.0f, 3150.0f), PlayerNames::PLAYER_ONE, Tribe::CELTIC);
	FeatureScriptCommands::CreateSpellDispenser(3, glm::vec3(3014.44f, 0.0f, 3155.89f), "CELTIC_ABODE_SPELL_DISPENSER", "HEAL",
	                                            0.0f, 1.0f, 0.5f);
	const auto dispensers = All<SpellDispenser>();
	ASSERT_EQ(dispensers.size(), 1u);
	const auto& made = Reg().Get<const SpellDispenser>(dispensers.front());
	EXPECT_EQ(made.magicType, MagicType::Heal);
	EXPECT_EQ(made.timer.period, 0u);
	EXPECT_FALSE(made.timer.active);
	ASSERT_NE(made.orb, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(made.orb).seedType, k_HealSeed);
}

// The one-shot commands over the game's own info.dat: its fire seed casts the fireball and its two power-ups, its
// water seed the water and its power-up
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LandScriptMagicObjectsRealDataTest, realInfoDatOneShotSpells)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	Locator::infoConstants::reset(info.release());
	FeatureScriptCommands::CreateOneShotSpellPu(glm::vec3(1010.0f, 0.0f, 1020.0f), "FIRE_PU2");
	ASSERT_EQ(All<OneOffSpellSeed>().size(), 1u);
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(Script::LastCreated()).seedType, k_FireSeed);
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(Script::LastCreated()).powerUp, 1);
	FeatureScriptCommands::CreateOneShotSpell(glm::vec3(1030.0f, 0.0f, 1020.0f), "WATER");
	ASSERT_EQ(All<OneOffSpellSeed>().size(), 2u);
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(Script::LastCreated()).seedType, static_cast<SpellSeedType>(9));
	EXPECT_EQ(Reg().Get<const OneOffSpellSeed>(Script::LastCreated()).powerUp, -1);
}
