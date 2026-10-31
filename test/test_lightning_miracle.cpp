/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The lightning miracle cast from the hand, pinned to the original: its cone aims along the camera's forward, it takes
// what is available ahead (not a miracle seed nor a dying villager) up to its cap, makes the rest up with points on the
// ground ahead, searches again every second without making more forks, strikes a share of its targets each step, and
// lays its forks out thinner with depth, jittered sideways and flickering. The three levels' numbers come from the
// game's own files. Wiki: docs/wiki/magic/fireball-and-lightning.md#lightning.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/AudioManagerNoOp.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Services/SpellSounds.h"
#include "Common/Zip.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "Enums.h"
#include "FileSystem/DefaultFileSystem.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/SoundAction.h"
#include "Particles/SpellLink.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Resources/ResourcesInterface.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;

namespace
{
/// The hand-cast bolt of SF_LightningBolt (the base level) with the game's file's values: a reach of 60 m, a cone of
/// half-angle 0.2 rad, up to 6 objects, at least 3 targets, at most 4 at once, 10 joints per fork, a fork scale of 2
/// and a sideways jitter of 0.0460177 of a fork's length. RenewSearchEvery is not in the game's file, so the rule's own
/// default of one second holds. No light map and no glow: group 0 holds the rule, the forks are group 1
constexpr std::string_view k_Bolt = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS UR_Lightning UR_Lightning0
BEGINPROPERTIES
PROPERTY AverageLightmapLife FLOAT 1.5
PROPERTY CastingFromHand BOOL 1
PROPERTY CommonGlowGroup INTEGER 2
PROPERTY ForkGroup INTEGER 1
PROPERTY ForkScale FLOAT 2
PROPERTY Group INTEGER 0
PROPERTY LightMapGroup INTEGER -1
PROPERTY MaxJointsPerFork INTEGER 10
PROPERTY MaxLightningObjects INTEGER 6
PROPERTY MaxLightningObjectsAtOnce INTEGER 4
PROPERTY MinLightningObjects INTEGER 3
PROPERTY NumTexturesToTile INTEGER -1
PROPERTY PCreator PERSIS_PNTR ParticleChainCreator0
PROPERTY RandomFrac FLOAT 0.0460177
PROPERTY RenewTargetsOnMove BOOL 0
PROPERTY RenewTargetsOnMoveFrac FLOAT 0.5
PROPERTY SearchRadius PERSIS_PNTR ConstFloatProvider_SearchRadius
PROPERTY SplitAngle FLOAT 0.2
ENDPROPERTIES
ENDCLASS
BEGINCLASS ConstFloatProvider ConstFloatProvider_SearchRadius
BEGINPROPERTIES
PROPERTY ConstValue FLOAT 60
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

/// The base level's numbers (SF_LightningBolt)
constexpr float k_Reach = 60.0f;
constexpr float k_RandomFrac = 0.0460177f;
constexpr float k_ForkScale = 2.0f;
constexpr size_t k_Joints = 10;
/// A ground point stands this high over the land
constexpr float k_GroundPointHeight = 2.0f;
/// Ground points lie within this share of the reach
constexpr float k_GroundPointReach = 0.6f;

/// The hand, 30 m above the middle of a map cell
constexpr float k_HandX = 1005.0f;
constexpr float k_HandY = 30.0f;
constexpr float k_HandZ = 1005.0f;

glm::vec3 Hand()
{
	return {k_HandX, k_HandY, k_HandZ};
}

/// The camera looking east, along +x
glm::vec3 East()
{
	return {1.0f, 0.0f, 0.0f};
}

/// A point on the ground, dx east and dz north of the hand
glm::vec3 Ahead(float dx, float dz)
{
	return {k_HandX + dx, 0.0f, k_HandZ + dz};
}

std::shared_ptr<const particles::File> Parse(std::string_view text)
{
	auto file = particles::File::Parse(text, "SF_LightningBoltMiracleTest");
	return file.has_value() ? std::make_shared<const particles::File>(*file) : nullptr;
}

/// The test bolt with some of its property lines changed, each (line, replacement)
std::shared_ptr<const particles::File> BoltWith(const std::vector<std::pair<std::string_view, std::string_view>>& changes)
{
	std::string text(k_Bolt);
	for (const auto& [line, replacement] : changes)
	{
		const auto at = text.find(line);
		if (at == std::string::npos)
		{
			return nullptr;
		}
		text.replace(at, line.size(), replacement);
	}
	return Parse(text);
}

/// Keeps the events the rule sends and answers 1, as a spell that lets every strike through
class RecordingSink final: public particles::SpellSink
{
public:
	int SpellEvent(const particles::SpellEventInfo& event) override
	{
		events.push_back(event);
		return 1;
	}
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	std::vector<particles::SpellEventInfo> events;
};

/// A bolt held in the hand, its camera looking along `forward`, stepped a tenth of a second at a time
class Bolt
{
public:
	Bolt(const std::shared_ptr<const particles::File>& file, glm::vec3 forward)
	    : _effect(file, glm::vec3(0.0f), 1.0f)
	    , _forward(forward)
	{
		_effect.SetSink(&_sink);
	}

	/// One step: the tips it struck in it
	std::vector<glm::vec3> Step()
	{
		particles::ProcessInfo info;
		info.handPos = Hand();
		info.cameraForward = _forward;
		info.enabled = true;
		_effect.SetProcessInfo(info);
		_sink.events.clear();
		_effect.Step(0.1f);
		std::vector<glm::vec3> tips;
		for (const auto& event : _sink.events)
		{
			if (event.type == particles::SpellEventInfo::Type::Landed)
			{
				tips.push_back(event.position);
			}
		}
		return tips;
	}

	/// Steps on, `count` times: every tip struck
	std::vector<glm::vec3> Steps(int count)
	{
		std::vector<glm::vec3> all;
		for (int i = 0; i < count; ++i)
		{
			const auto tips = Step();
			all.insert(all.end(), tips.begin(), tips.end());
		}
		return all;
	}

	/// Its forks: every collection but the rule's own, two for each target of its first search
	[[nodiscard]] size_t Forks() const { return _effect.CollectionCount() - 1; }

	/// The forks drawn after the last step, the trunk first
	[[nodiscard]] std::vector<particles::Effect::DrawChain> Chains() const
	{
		std::vector<particles::Effect::DrawChain> chains;
		_effect.CollectChains(1.0f, chains);
		return chains;
	}

private:
	// the sink outlives the effect that points to it
	RecordingSink _sink;
	particles::Effect _effect;
	glm::vec3 _forward;
};

/// Whether one of the tips is the point (an object's tip is its map position, within a hundredth of a metre)
bool Hit(const std::vector<glm::vec3>& tips, const glm::vec3& point)
{
	return std::ranges::any_of(tips, [&point](const glm::vec3& tip) { return glm::distance(tip, point) < 0.01f; });
}

/// How many different points the tips are
size_t Distinct(const std::vector<glm::vec3>& tips)
{
	std::vector<glm::vec3> seen;
	for (const auto& tip : tips)
	{
		if (!Hit(seen, tip))
		{
			seen.push_back(tip);
		}
	}
	return seen.size();
}

/// Data\Spells\ZSpellFiles\<name>_txt.zzz: the length then a deflate stream
std::optional<particles::File> LoadSpellFile(const std::filesystem::path& root, const std::string& name)
{
	std::ifstream stream(root / "Data" / "Spells" / "ZSpellFiles" / (name + "_txt.zzz"), std::ios::binary);
	if (!stream.is_open())
	{
		return std::nullopt;
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	if (bytes.size() <= 4)
	{
		return std::nullopt;
	}
	uint32_t size = 0;
	std::memcpy(&size, bytes.data(), sizeof(size));
	const auto inflated = zip::Inflate(std::vector<uint8_t>(bytes.begin() + 4, bytes.end()), size);
	return particles::File::Parse(std::string(inflated.begin(), inflated.end()), name);
}

/// The first object of that class in the file
const particles::Object* FindClass(const particles::File& file, std::string_view className)
{
	const auto it = std::ranges::find_if(
	    file.objects, [className](const particles::Object& object) { return object.className == className; });
	return it != file.objects.end() ? &*it : nullptr;
}

/// What one level's file gives its bolt
struct Level
{
	const char* file;
	float reach;
	float splitAngle;
	int maxObjects;
	int minObjects;
	int atOnce;
	const char* crackle;
};

/// The three levels as the game's files have them
constexpr std::array<Level, 3> k_Levels {{
    {.file = "SF_LightningBolt",
     .reach = 60.0f,
     .splitAngle = 0.2f,
     .maxObjects = 6,
     .minObjects = 3,
     .atOnce = 4,
     .crackle = "SOUND_SPELL_HAND_LIGHTNING"},
    {.file = "SF_LightningBoltPUOne",
     .reach = 100.0f,
     .splitAngle = 0.35f,
     .maxObjects = 12,
     .minObjects = 8,
     .atOnce = 10,
     .crackle = "SOUND_SPELL_HAND_LIGHTNING_LEVEL_2"},
    {.file = "SF_LightningBoltPUTwo",
     .reach = 140.0f,
     .splitAngle = 0.5f,
     .maxObjects = 28,
     .minObjects = 15,
     .atOnce = 20,
     .crackle = "SOUND_SPELL_HAND_LIGHTNING_LEVEL_3"},
}};

/// The same reads on one level's file: its rule, its reach and the crackle on its root sprite
void ExpectLevel(const particles::File& file, const Level& level)
{
	const auto* rule = FindClass(file, "UR_Lightning");
	ASSERT_NE(rule, nullptr) << level.file;
	EXPECT_TRUE(rule->Bool("CastingFromHand", false)) << level.file;
	EXPECT_FLOAT_EQ(rule->Float("SplitAngle", 0.0f), level.splitAngle) << level.file;
	EXPECT_EQ(rule->Int("MaxLightningObjects", 0), level.maxObjects) << level.file;
	EXPECT_EQ(rule->Int("MinLightningObjects", 0), level.minObjects) << level.file;
	EXPECT_EQ(rule->Int("MaxLightningObjectsAtOnce", 0), level.atOnce) << level.file;
	// no level sets the search period: the rule's default of one second holds for all three
	EXPECT_FALSE(rule->Has("RenewSearchEvery")) << level.file;
	const auto* reach = file.Find(rule->String("SearchRadius"));
	ASSERT_NE(reach, nullptr) << level.file;
	EXPECT_EQ(reach->className, "ConstFloatProvider") << level.file;
	EXPECT_FLOAT_EQ(reach->Float("ConstValue", 0.0f), level.reach) << level.file;
	// the crackle: a sound of its own per level, on the root sprite while the bolt is not closing, let go softly
	const auto* crackle = FindClass(file, "StartStopSoundOnCondition");
	ASSERT_NE(crackle, nullptr) << level.file;
	EXPECT_EQ(crackle->Sound("Sound").sound, level.crackle) << level.file;
	EXPECT_TRUE(crackle->Sound("Sound").softRelease) << level.file;
	EXPECT_EQ(crackle->Int("FadeStep", 0), 20) << level.file;
	EXPECT_EQ(crackle->Int("Group", -1), 5) << level.file;
}

/// A world with an entities registry and the world's lists, zeroed tables, and the turn set by each test; each put
/// back when the test ends
class LightningMiracle: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		auto info = std::make_unique<InfoConstants>();
		Locator::infoConstants::reset(info.release());
	}
	void TearDown() override
	{
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
	}

	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }

	/// A mobile object standing at the point, filed in its map cell
	static entt::entity MakeObject(const glm::vec3& at)
	{
		const auto object = Registry().Create();
		Registry().Assign<ecs::components::Transform>(object, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Registry().Assign<ecs::components::MobileObject>(object);
		ecs::map_cells::InsertMapObject(object);
		return object;
	}

	/// A villager standing at the point, filed in its map cell, in that state now and as its final one
	static entt::entity MakeVillager(const glm::vec3& at, VillagerStates state)
	{
		const auto villager = Registry().Create();
		Registry().Assign<ecs::components::Transform>(villager, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Registry().Assign<ecs::components::Villager>(villager);
		auto& action = Registry().Assign<ecs::components::LivingAction>(villager, state, uint16_t {0});
		action.states.at(static_cast<size_t>(ecs::components::LivingAction::Index::Final)) = static_cast<uint8_t>(state);
		ecs::map_cells::InsertMapObject(villager);
		return villager;
	}

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

// The cone's heading is the angle of the camera's forward across the land, atan2(forward z, forward x), not where the
// hand moves: looking east the bolt strikes what stands east and never what stands north, looking north the other way
// round. The forward's height plays no part (docs/wiki/magic/fireball-and-lightning.md#lightning, origin and direction)
TEST_F(LightningMiracle, TheBoltsAimAlongTheCamerasForward)
{
	const auto file = Parse(k_Bolt);
	ASSERT_NE(file, nullptr);
	const auto east = Ahead(30.0f, 0.0f);
	const auto north = Ahead(0.0f, 30.0f);
	MakeObject(east);
	MakeObject(north);
	{
		game_clock::SetTurn(100);
		Bolt bolt(file, East());
		const auto tips = bolt.Steps(10);
		EXPECT_TRUE(Hit(tips, east));
		EXPECT_FALSE(Hit(tips, north));
	}
	{
		game_clock::SetTurn(200);
		Bolt bolt(file, glm::vec3(0.0f, -0.8f, 0.6f));
		const auto tips = bolt.Steps(10);
		EXPECT_TRUE(Hit(tips, north));
		EXPECT_FALSE(Hit(tips, east));
	}
}

// Anything available in the cone is a target, a villager too, but a miracle seed never is, nor a villager whose final
// state is dying (docs/wiki/magic/fireball-and-lightning.md#lightning, the search: IsAvailable and the seed test)
TEST_F(LightningMiracle, AnythingAvailableIsATargetButNotASeedNorADyingVillager)
{
	const auto file = Parse(k_Bolt);
	ASSERT_NE(file, nullptr);
	const auto plain = Ahead(20.0f, 0.0f);
	const auto living = Ahead(24.0f, 1.0f);
	const auto seed = Ahead(28.0f, 0.0f);
	const auto dying = Ahead(32.0f, -1.0f);
	MakeObject(plain);
	MakeVillager(living, VillagerStates::MoveToPos);
	// a seed lying in a map cell: the search skips it
	const auto seedObject = MakeObject(seed);
	Registry().Assign<ecs::components::SpellSeed>(seedObject);
	MakeVillager(dying, VillagerStates::Dying);
	game_clock::SetTurn(300);
	Bolt bolt(file, East());
	const auto tips = bolt.Steps(10);
	EXPECT_TRUE(Hit(tips, plain));
	EXPECT_TRUE(Hit(tips, living));
	EXPECT_FALSE(Hit(tips, seed));
	EXPECT_FALSE(Hit(tips, dying));
	// the two it takes and one ground point: two forks each
	EXPECT_EQ(bolt.Forks(), 2u * 3u);
}

// Only what lies inside the cone of half-angle SplitAngle (0.2 rad) about the heading is taken: just past the edge,
// behind and to the side it is not, though the cap is far (docs/wiki/magic/fireball-and-lightning.md#lightning, the
// targets from the hand)
TEST_F(LightningMiracle, OnlyObjectsInsideTheConeAreTaken)
{
	const auto file = Parse(k_Bolt);
	ASSERT_NE(file, nullptr);
	// cos(0.2) = 0.98007: 30 m ahead and 5 m aside is in (0.98639), 7 m aside is out (0.97384)
	const std::array inside {Ahead(30.0f, 0.0f), Ahead(30.0f, 5.0f)};
	const std::array outside {Ahead(30.0f, 7.0f), Ahead(-30.0f, 0.0f), Ahead(0.0f, 30.0f)};
	for (const auto& at : inside)
	{
		MakeObject(at);
	}
	for (const auto& at : outside)
	{
		MakeObject(at);
	}
	game_clock::SetTurn(400);
	Bolt bolt(file, East());
	const auto tips = bolt.Steps(10);
	for (const auto& at : inside)
	{
		EXPECT_TRUE(Hit(tips, at)) << at.x << ", " << at.z;
	}
	for (const auto& at : outside)
	{
		EXPECT_FALSE(Hit(tips, at)) << at.x << ", " << at.z;
	}
	// the two inside and one ground point
	EXPECT_EQ(bolt.Forks(), 2u * 3u);
}

// The first search stops at MaxLightningObjects, 6 for the base level: with eight objects in the cone it takes six,
// and only six are ever struck (docs/wiki/magic/fireball-and-lightning.md#lightning, the targets from the hand)
TEST_F(LightningMiracle, TheFirstSearchTakesAtMostSixObjects)
{
	const auto file = Parse(k_Bolt);
	ASSERT_NE(file, nullptr);
	std::vector<glm::vec3> objects;
	for (int i = 0; i < 8; ++i)
	{
		objects.push_back(Ahead(12.0f + 4.0f * static_cast<float>(i), 0.0f));
		MakeObject(objects.back());
	}
	game_clock::SetTurn(500);
	Bolt bolt(file, East());
	const auto tips = bolt.Steps(10);
	EXPECT_EQ(bolt.Forks(), 2u * 6u);
	EXPECT_LE(Distinct(tips), 6u);
	for (const auto& tip : tips)
	{
		EXPECT_TRUE(Hit(objects, tip)) << tip.x << ", " << tip.y << ", " << tip.z;
	}
}

// With fewer than MinLightningObjects found (3 for the base level), the rest are points on the ground ahead: at the
// heading plus up to a quarter turn (never less), up to 0.6 of the reach away, two metres above the land
// (docs/wiki/magic/fireball-and-lightning.md#lightning, the ground points)
TEST_F(LightningMiracle, MissingTargetsAreMadeUpWithGroundPointsAhead)
{
	const auto file = Parse(k_Bolt);
	ASSERT_NE(file, nullptr);
	struct Look
	{
		glm::vec3 forward;
		float heading;
		uint32_t turn;
	};
	const std::array looks {
	    Look {.forward = East(), .heading = 0.0f, .turn = 600},
	    Look {.forward = glm::vec3(0.0f, 0.0f, 1.0f), .heading = std::numbers::pi_v<float> / 2.0f, .turn = 700},
	};
	for (const auto& look : looks)
	{
		game_clock::SetTurn(look.turn);
		Bolt bolt(file, look.forward);
		const auto tips = bolt.Steps(10);
		// nothing on the map: three ground points, two forks each
		EXPECT_EQ(bolt.Forks(), 2u * 3u);
		ASSERT_FALSE(tips.empty());
		EXPECT_LE(Distinct(tips), 3u);
		for (const auto& tip : tips)
		{
			EXPECT_FLOAT_EQ(tip.y, k_GroundPointHeight);
			const float dx = tip.x - k_HandX;
			const float dz = tip.z - k_HandZ;
			const float distance = std::sqrt(dx * dx + dz * dz);
			EXPECT_LE(distance, k_GroundPointReach * k_Reach + 1e-3f);
			// the angle only where the point is far enough from the hand for it to be read
			if (distance > 1.0f)
			{
				const float off = std::atan2(dz, dx) - look.heading;
				EXPECT_GE(off, -1e-3f) << look.heading;
				EXPECT_LE(off, std::numbers::pi_v<float> / 4.0f + 1e-3f) << look.heading;
			}
		}
	}
}

// The targets are searched again once more than RenewSearchEvery has passed since the last search: one second, the
// rule's default, as no level's file sets it. What appears in the cone is not struck before that and is after it. The
// forks stay those of the first search: two per target found then, however many the new search finds
// (docs/wiki/magic/fireball-and-lightning.md#lightning, the structure and the re-search)
TEST_F(LightningMiracle, TargetsAreSearchedAgainEverySecondWithTheFirstForks)
{
	const auto file = Parse(k_Bolt);
	ASSERT_NE(file, nullptr);
	game_clock::SetTurn(800);
	Bolt bolt(file, East());
	// the first search finds nothing: three ground points
	static_cast<void>(bolt.Step());
	EXPECT_EQ(bolt.Forks(), 2u * 3u);
	std::vector<glm::vec3> objects;
	for (int i = 0; i < 5; ++i)
	{
		objects.push_back(Ahead(16.0f + 4.0f * static_cast<float>(i), 0.0f));
		MakeObject(objects.back());
	}
	// up to nine tenths of a second since the search: still the ground points
	const auto before = bolt.Steps(9);
	for (const auto& at : objects)
	{
		EXPECT_FALSE(Hit(before, at)) << at.x;
	}
	// past the second: the new search takes them
	const auto after = bolt.Steps(20);
	EXPECT_TRUE(std::ranges::any_of(objects, [&after](const glm::vec3& at) { return Hit(after, at); }));
	EXPECT_EQ(bolt.Forks(), 2u * 3u);
}

// Each step every target is picked with probability min(MaxLightningObjectsAtOnce, (N + 1) / 2) / N until that many
// are: with six targets at most three a step for the base level (4 at once), at most two with 2 at once
// (docs/wiki/magic/fireball-and-lightning.md#lightning, each step)
TEST_F(LightningMiracle, EachStepStrikesAShareOfTheTargets)
{
	std::vector<glm::vec3> objects;
	for (int i = 0; i < 6; ++i)
	{
		objects.push_back(Ahead(12.0f + 5.0f * static_cast<float>(i), 0.0f));
		MakeObject(objects.back());
	}
	struct Case
	{
		std::shared_ptr<const particles::File> file;
		size_t most;
		uint32_t turn;
	};
	const std::array cases {
	    Case {.file = Parse(k_Bolt), .most = 3, .turn = 900},
	    Case {.file = BoltWith({{"MaxLightningObjectsAtOnce INTEGER 4", "MaxLightningObjectsAtOnce INTEGER 2"}}),
	          .most = 2,
	          .turn = 1000},
	};
	for (const auto& run : cases)
	{
		ASSERT_NE(run.file, nullptr);
		game_clock::SetTurn(run.turn);
		Bolt bolt(run.file, East());
		size_t most = 0;
		for (int i = 0; i < 10; ++i)
		{
			const auto tips = bolt.Step();
			EXPECT_LE(tips.size(), run.most);
			most = std::max(most, tips.size());
			for (const auto& tip : tips)
			{
				EXPECT_TRUE(Hit(objects, tip));
			}
		}
		EXPECT_EQ(most, run.most);
	}
}

// A fork's joints run straight from its start to its end, the inner ones moved sideways in x and z (never in height)
// by up to RandomFrac of its length; its scale runs from S / (depth + 1) at the start to S / (depth + 2) at the end, S
// the fork scale (2); each joint's alpha is 128 plus a draw below 127
// (docs/wiki/magic/fireball-and-lightning.md#lightning, the fork tree)
TEST_F(LightningMiracle, TheTrunkThinsFromHandToTipJitteredSidewaysAndFlickering)
{
	const auto one = BoltWith({{"MaxLightningObjects INTEGER 6", "MaxLightningObjects INTEGER 1"},
	                           {"MaxLightningObjectsAtOnce INTEGER 4", "MaxLightningObjectsAtOnce INTEGER 1"},
	                           {"MinLightningObjects INTEGER 3", "MinLightningObjects INTEGER 1"}});
	ASSERT_NE(one, nullptr);
	const auto target = Ahead(20.0f, 0.0f);
	MakeObject(target);
	game_clock::SetTurn(1100);
	Bolt bolt(one, East());
	// a single target is struck every step
	for (int step = 0; step < 3; ++step)
	{
		const auto tips = bolt.Step();
		ASSERT_EQ(tips.size(), 1u);
		EXPECT_TRUE(Hit(tips, target));
		const auto chains = bolt.Chains();
		ASSERT_EQ(chains.size(), 1u);
		const auto& joints = chains.front().joints;
		ASSERT_EQ(joints.size(), k_Joints);
		const glm::vec3 start = Hand();
		const glm::vec3 end = tips.front();
		const float length = glm::distance(start, end);
		EXPECT_NEAR(glm::distance(joints.front().position, start), 0.0f, 1e-3f);
		EXPECT_NEAR(glm::distance(joints.back().position, end), 0.0f, 1e-3f);
		bool flickers = false;
		for (size_t i = 0; i < joints.size(); ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(k_Joints - 1);
			const glm::vec3 line = start + (end - start) * t;
			EXPECT_NEAR(joints[i].position.y, line.y, 1e-3f) << i;
			EXPECT_LE(std::abs(joints[i].position.x - line.x), k_RandomFrac * length + 1e-3f) << i;
			EXPECT_LE(std::abs(joints[i].position.z - line.z), k_RandomFrac * length + 1e-3f) << i;
			// depth 0: from S to S / 2
			EXPECT_NEAR(joints[i].scale, k_ForkScale + (k_ForkScale / 2.0f - k_ForkScale) * t, 1e-5f) << i;
			EXPECT_GE(joints[i].alpha, 128.0f) << i;
			EXPECT_LE(joints[i].alpha, 254.0f) << i;
			flickers = flickers || std::abs(joints[i].alpha - joints.front().alpha) > 0.5f;
		}
		EXPECT_TRUE(flickers);
	}
}

// A step that strikes two targets splits the trunk (depth 0, scale S to S / 2) into two forks of depth 1, each from
// S / 2 to S / 3, opaque only in a clash, so here 128 to 254 (docs/wiki/magic/fireball-and-lightning.md#lightning, the
// fork tree)
TEST_F(LightningMiracle, TheForksThinWithDepth)
{
	const auto file = Parse(k_Bolt);
	ASSERT_NE(file, nullptr);
	game_clock::SetTurn(1200);
	Bolt bolt(file, East());
	const auto expectFork = [](const particles::Effect::DrawChain& chain, float from, float to) {
		ASSERT_EQ(chain.joints.size(), k_Joints);
		EXPECT_NEAR(chain.joints.front().scale, from, 1e-5f);
		EXPECT_NEAR(chain.joints.back().scale, to, 1e-5f);
		for (const auto& joint : chain.joints)
		{
			EXPECT_GE(joint.alpha, 128.0f);
			EXPECT_LE(joint.alpha, 254.0f);
		}
	};
	// three ground points: at most two struck a step
	bool split = false;
	for (int i = 0; i < 10 && !split; ++i)
	{
		const auto tips = bolt.Step();
		if (tips.size() != 2)
		{
			continue;
		}
		split = true;
		const auto chains = bolt.Chains();
		ASSERT_EQ(chains.size(), 3u);
		expectFork(chains[0], k_ForkScale, k_ForkScale / 2.0f);
		expectFork(chains[1], k_ForkScale / 2.0f, k_ForkScale / 3.0f);
		expectFork(chains[2], k_ForkScale / 2.0f, k_ForkScale / 3.0f);
		// the children start where the trunk ends and each ends at one of the struck tips
		EXPECT_NEAR(glm::distance(chains[1].joints.front().position, chains[0].joints.back().position), 0.0f, 1e-3f);
		EXPECT_NEAR(glm::distance(chains[2].joints.front().position, chains[0].joints.back().position), 0.0f, 1e-3f);
		EXPECT_TRUE(Hit(tips, chains[1].joints.back().position));
		EXPECT_TRUE(Hit(tips, chains[2].joints.back().position));
	}
	EXPECT_TRUE(split);
}

/// With OPENBLACK_GAME_PATH set to the install: the three levels' files
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LightningMiracle, realDataLevels)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const std::filesystem::path root(game);
	for (const auto& level : k_Levels)
	{
		const auto file = LoadSpellFile(root, level.file);
		ASSERT_TRUE(file.has_value()) << level.file;
		ExpectLevel(*file, level);
	}
}

/// The same reads on a synthetic twin of the base level: the test bolt with the game's file's crackle rule added
TEST_F(LightningMiracle, realDataLevelsSynthetic)
{
	std::string text(k_Bolt);
	text += "BEGINCLASS StartStopSoundOnCondition StartStopSoundOnCondition0\nBEGINPROPERTIES\n"
	        "PROPERTY Condition PERSIS_PNTR NULL_STRING\nPROPERTY Group INTEGER 5\n"
	        "PROPERTY Sound SOUND_ACTION SOUND_SPELL_HAND_LIGHTNING LOOPING 0 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0\n"
	        "PROPERTY SoundCondition PERSIS_PNTR EventConditionFalseOnCloseDown0\nPROPERTY FadeStep INTEGER 20\n"
	        "ENDPROPERTIES\nENDCLASS\n";
	const auto file = particles::File::Parse(text, "SF_LightningBolt");
	ASSERT_TRUE(file.has_value());
	ExpectLevel(*file, k_Levels.front());
}

/// With OPENBLACK_GAME_PATH set to the install: what a fork that hits a shield costs it, per level. The shield pays the
/// hit's strength (1 for a fork) times the bolt's costPerShieldCollide (ShieldMiracleSpells pins that rule)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LightningMiracle, realInfoDatShieldCost)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	EXPECT_FLOAT_EQ(magic::GetMagicEffectInfo(*info, MagicType::LightningBolt).costPerShieldCollide, 10.0f);
	EXPECT_FLOAT_EQ(magic::GetMagicEffectInfo(*info, MagicType::LightningBoltPowerUpOne).costPerShieldCollide, 20.0f);
	EXPECT_FLOAT_EQ(magic::GetMagicEffectInfo(*info, MagicType::LightningBoltPowerUpTwo).costPerShieldCollide, 30.0f);
}

/// The lookups realInfoDatShieldCost goes through, on a zeroed info block with made-up rows: each level reads its own
/// effect row
TEST_F(LightningMiracle, realInfoDatShieldCostSynthetic)
{
	auto info = std::make_unique<InfoConstants>();
	constexpr std::array k_Types {MagicType::LightningBolt, MagicType::LightningBoltPowerUpOne,
	                              MagicType::LightningBoltPowerUpTwo};
	for (size_t i = 0; i < k_Types.size(); ++i)
	{
		info->magicEffect.at(static_cast<size_t>(k_Types.at(i))).costPerShieldCollide = 1.5f * static_cast<float>(i + 1);
	}
	for (size_t i = 0; i < k_Types.size(); ++i)
	{
		const auto& row = magic::GetMagicEffectInfo(*info, k_Types.at(i));
		EXPECT_EQ(&row, &info->magicEffect.at(static_cast<size_t>(k_Types.at(i))));
		EXPECT_FLOAT_EQ(row.costPerShieldCollide, 1.5f * static_cast<float>(i + 1));
	}
}

namespace
{
/// The crackle of SF_LightningBolt: its root (a sprite in the game's file, a point here) in group 5 plays
/// SOUND_SPELL_HAND_LIGHTNING while the bolt is not closing, released softly, fading by 20 a turn once let go
constexpr std::string_view k_Crackle = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS StartStopSoundOnCondition StartStopSoundOnCondition0
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 5
PROPERTY Sound SOUND_ACTION SOUND_SPELL_HAND_LIGHTNING LOOPING 0 ONLYONE 0 SOFTRELEASE 1 USESURFACE 0
PROPERTY SoundCondition PERSIS_PNTR EventConditionFalseOnCloseDown0
PROPERTY FadeStep INTEGER 20
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionTrueOnCloseDown EventConditionFalseOnCloseDown0
BEGINPROPERTIES
PROPERTY InvertResponse BOOL 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS CreateRuleAnAtom CreateRuleAnAtom_Root
BEGINPROPERTIES
PROPERTY Condition PERSIS_PNTR NULL_STRING
PROPERTY Group INTEGER 5
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RemoveOnCloseDown BOOL 0
ENDPROPERTIES
ENDCLASS
)";

/// A tiny SoundAction.h with the crackle, and the value it gives it
constexpr std::string_view k_SyntheticSoundActions = "enum LHSoundAction\n{\n\tNO_SOUND_ACTION = 0,\n"
                                                     "\tSOUND_SPELL_HAND_LIGHTNING = 70,\n};\n";
constexpr int32_t k_CrackleSound = 70;

/// The full volume of a channel
constexpr int k_FullVolume = 127;

/// Puts the synthetic SoundAction.h into a fresh resource cache, under the path the sound actions are read from, so
/// no file is opened. The previous file system and resources come back when it goes
class SyntheticSoundActions
{
public:
	SyntheticSoundActions()
	{
		Locator::filesystem::emplace<filesystem::DefaultFileSystem>();
		Locator::resources::emplace<resources::Resources>();
		const auto path = Locator::filesystem::value().GetPath<filesystem::Path::Data>() / "SoundAction.h";
		Locator::resources::value().GetBlobs().Load(
		    resources::BlobId(path), resources::BlobLoader::FromBufferTag {},
		    std::vector<uint8_t>(k_SyntheticSoundActions.begin(), k_SyntheticSoundActions.end()));
	}

private:
	// built before the constructor's body, so they keep the services that were there before
	const test::RestoreService<Locator::filesystem> _restoreFilesystem;
	const test::RestoreService<Locator::resources> _restoreResources;
};

/// An audio service that hears every animation effect asked of it and keeps one channel playing for every owner, at
/// the volume last set (full to begin with), every volume set kept in order
class HeardAudio final: public audio::AudioManagerNoOp
{
public:
	struct Request
	{
		audio::AnimKey key;
		audio::AnimAction action;
	};
	[[nodiscard]] audio::Channel PlayingChannel(audio::Owner /*owner*/, audio::BankId /*bank*/) override { return 7; }
	[[nodiscard]] int Volume(audio::Channel /*channel*/) override { return volume; }
	void SetVolume(audio::Channel /*channel*/, int value) override
	{
		volume = value;
		volumes.push_back(value);
	}
	audio::Channel PlayAnimationEffect(audio::Owner /*owner*/, float /*distance*/, const audio::AnimKey& key,
	                                   audio::AnimAction action, audio::BankId /*bank*/, bool /*track*/, float /*minDistance*/,
	                                   float /*maxDistance*/) override
	{
		requests.push_back({key, action});
		return audio::k_NoChannel;
	}
	/// The actions asked for the sounds of that action (the key's last attribute), in order
	[[nodiscard]] std::vector<audio::AnimAction> ActionsOf(int32_t sound) const
	{
		std::vector<audio::AnimAction> actions;
		for (const auto& request : requests)
		{
			if (request.key[4] == sound)
			{
				actions.push_back(request.action);
			}
		}
		return actions;
	}
	int volume {k_FullVolume};
	std::vector<int> volumes;
	std::vector<Request> requests;
};

/// The drawn point atoms of an effect made by its creator of that name
std::vector<const particles::Atom*> PointAtomsOf(const particles::Effect& effect, std::string_view creator)
{
	std::vector<particles::Effect::DrawAtom> drawn;
	effect.Collect(1.0f, drawn, particles::Creator::Kind::Point);
	const auto* wanted = effect.FindCreator(std::string(creator));
	std::vector<const particles::Atom*> atoms;
	for (const auto& atom : drawn)
	{
		if (atom.creator == wanted && atom.atom != nullptr)
		{
			atoms.push_back(atom.atom);
		}
	}
	return atoms;
}
} // namespace

// The crackle starts with the cast, on the bolt's root, once (not again each step), released softly. When the bolt
// closes the root lets it go with its FadeStep of 20: from the next sound turn its channel's volume drops by 20 a
// turn, never below 0, so from full (127) it is silent on the seventh turn, and the soft release is sent once
// (docs/wiki/audio/magic-and-weather-sounds.md, the rules and the turn of the particle sounds; SF_LightningBolt's
// StartStopSoundOnCondition0)
TEST_F(LightningMiracle, TheCrackleStartsWithTheCastAndFadesOutInSevenTurnsOnceLetGo)
{
	const SyntheticSoundActions soundActions;
	ASSERT_EQ(particles::SoundActionByName("SOUND_SPELL_HAND_LIGHTNING"), k_CrackleSound);
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardAudio&>(Locator::audio::emplace<HeardAudio>());
	// no particle sound left from before
	audio::spell_sounds::Clear();
	const auto file = Parse(k_Crackle);
	ASSERT_NE(file, nullptr);
	{
		particles::Effect effect(file, Hand(), 1.0f);
		// the root is made by the rule after the sound's, so the sound starts with the second step at the latest
		effect.Step(0.1f);
		effect.Step(0.1f);
		const auto roots = PointAtomsOf(effect, "ParticlePointCreator0");
		ASSERT_EQ(roots.size(), 1u);
		ASSERT_EQ(roots[0]->sounds.size(), 1u);
		const auto& action = roots[0]->sounds.front()->action;
		EXPECT_EQ(action.action, k_CrackleSound);
		EXPECT_EQ(action.flags, particles::SoundAction::k_SoftRelease);
		// held on: still that one sound
		effect.Step(0.1f);
		EXPECT_EQ(roots[0]->sounds.size(), 1u);
		// let go: the bolt closes and its root lets the crackle go
		effect.CloseDown();
		effect.Step(0.1f);
		EXPECT_TRUE(roots[0]->sounds.empty());
		heard.requests.clear();
		for (int turn = 0; turn < 8; ++turn)
		{
			audio::spell_sounds::ProcessTurn(0.1f);
		}
		EXPECT_EQ(heard.volumes, (std::vector<int> {107, 87, 67, 47, 27, 7, 0, 0}));
		const auto released = heard.ActionsOf(k_CrackleSound);
		ASSERT_EQ(released.size(), 1u);
		EXPECT_EQ(released.front(), audio::AnimAction::Release);
	}
	audio::spell_sounds::Clear();
}

namespace
{
/// The test's house row, its town and the alignment table's building column for the crush, the hit and the fly away
constexpr auto k_StrikeHouse = static_cast<AbodeInfo>(0);
constexpr uint32_t k_StrikeTownId = 1;
constexpr float k_CrushColumn = -0.5f;
constexpr float k_HitColumn = -0.25f;
constexpr float k_FlyAwayColumn = -0.125f;

/// The game's file's values of the base level's effect row: burn 800, crush 0.0006, hit 0.0017, no heal, fly away
/// 0.007, alignment 1, no belief, radius 1
GMagicEffectInfo BaseLevelRow()
{
	GMagicEffectInfo row {};
	row.effectBurn = 800.0f;
	row.effectCrush = 0.0006f;
	row.effectHit = 0.0017f;
	row.effectHeal = 0.0f;
	row.effectFlyAway = 0.007f;
	row.effectAlignmentModification = 1.0f;
	row.effectBeliefModification = 0.0f;
	row.radius = 1.0f;
	return row;
}

/// The first player's town with a hand-made info block: a built house row that takes a crush and a hit whole, burns
/// from 1000 degrees, is a building to the alignment and has an aggressor value of 2; the alignment table's building
/// column; the town info's first attack addition of 0.5 and no addition to the alignment change. The world's lists,
/// the map and the villagers' defaults, each put back when the test ends
class LightningMiracleStrike: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		auto& house = info->abode.at(static_cast<size_t>(k_StrikeHouse));
		house.abodeType = AbodeType::LivingQuarters;
		house.abodeNumber = AbodeNumber::A;
		house.tribeType = Tribe::CELTIC;
		house.maxVillagersInAbode = 4;
		house.desireToBeRepaired = 0.5f;
		house.maxVillagerNeededToBuild = 4;
		house.thresholdForStopBeingFunctional = 0.75f;
		house.aggressorValue = 2.0f;
		house.alignmentType = AlignmentType::Building;
		house.defenceMultiplierCrush = 1.0f;
		house.defenceMultiplierHit = 1.0f;
		house.heatCapacity = 1000.0f;
		house.combustionTemperature = 1000.0f;
		info->alignment.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Crush)).building = k_CrushColumn;
		info->alignment.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Hit)).building = k_HitColumn;
		info->alignment.at(static_cast<size_t>(ecs::effects::EffectValues::Number::FlyAway)).building = k_FlyAwayColumn;
		info->town.thresholdToStartRepairing = 0.9f;
		info->town.gameTurnsAfterEmergencyVillagersReact = 1200;
		info->town.firstTimeDamageDoneAddition = 0.5f;
		Locator::infoConstants::reset(info.release());
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		_town = registry.Create();
		auto& town = registry.Assign<ecs::components::Town>(_town);
		town.id = k_StrikeTownId;
		town.owner = PlayerNames::PLAYER_ONE;
		registry.Context().towns[k_StrikeTownId] = _town;
		game_clock::SetTurn(0);
	}
	void TearDown() override
	{
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}

	/// The second player's strike with that row's values, at full strength, on a built house of the first player's town
	/// (applied by the hand the bolt leaves from): it hurts the house, the town holds it against the second player, and
	/// the second player's alignment moves
	void ExpectStrikeCounts(const GMagicEffectInfo& row) const
	{
		const test::RestoreService<Locator::playerSystem> restorePlayers;
		auto& players = Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
		auto& registry = Locator::entitiesRegistry::value();
		const auto house = registry.Create();
		registry.Assign<ecs::components::Abode>(house, AbodeNumber::A, k_StrikeTownId, 0u, 0u).info = k_StrikeHouse;
		const auto hand = registry.Create();
		auto values = ecs::effects::EffectValues::FromEffectInfo(row);
		values.player = PlayerNames::PLAYER_TWO;
		values.appliedBy = hand;
		ecs::effects::ApplyEffect(house, values);

		// The harm is the crush and the hit, each taken whole; the burn's 800 degrees are under the house's 1000, so the
		// burn does no harm
		const float harm = row.effectCrush + row.effectHit;
		ASSERT_TRUE(registry.Valid(house));
		const float life = 1.0f - harm;
		EXPECT_FLOAT_EQ(ecs::life::LifeOf(house), life);
		// The town holds the harm against the caster: the harm x the house's aggressor value of 2, + 0.5 the first time
		const auto& grudge = registry.Get<const ecs::components::Town>(_town).aggression;
		EXPECT_FLOAT_EQ(grudge.aggression.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)), (harm * 2.0f) + 0.5f);
		EXPECT_EQ(grudge.lastAggressor, std::optional(PlayerNames::PLAYER_TWO));
		EXPECT_EQ(grudge.aggression.at(static_cast<size_t>(PlayerNames::PLAYER_ONE)), 0.0f);
		// The caster's pending alignment: the crush, the hit and the fly away, each x the building column x K, K the life
		// lost; an alignment of 0 neither damps nor boosts it, and the alignment itself waits for the player's turn
		const float k = 1.0f - life;
		EXPECT_FLOAT_EQ(players.Alignment(PlayerNames::PLAYER_TWO).pending, (row.effectCrush * k_CrushColumn * k) +
		                                                                        (row.effectHit * k_HitColumn * k) +
		                                                                        (row.effectFlyAway * k_FlyAwayColumn * k));
		EXPECT_LT(players.Alignment(PlayerNames::PLAYER_TWO).pending, 0.0f);
		EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_TWO).value, 0.0f);
		// The town's own player is not the caster: its alignment does not move
		EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_ONE).pending, 0.0f);
	}

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	entt::entity _town {entt::null};
};
} // namespace

// Each strike applies the bolt's effect to what it reaches with the caster's player and the hand as the one who applied
// it: a town's thing it hurts makes its town hold the harm against the caster, by the thing's aggressor value, and moves
// the caster's alignment by the alignment table's column for the thing's kind (docs/wiki/magic/fireball-and-lightning.md
// #lightning, the effect's values; docs/wiki/magic/spell-core.md, ApplyDefaultSpellEffect;
// docs/wiki/magic/influence-and-alignment.md#player-alignment)

/// With OPENBLACK_GAME_PATH set to the install: the base level's effect row as the game's file has it, applied
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST_F(LightningMiracleStrike, realInfoDatStrike)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	const auto& row = magic::GetMagicEffectInfo(*info, MagicType::LightningBolt);
	const auto expected = BaseLevelRow();
	EXPECT_FLOAT_EQ(row.effectBurn, expected.effectBurn);
	EXPECT_FLOAT_EQ(row.effectCrush, expected.effectCrush);
	EXPECT_FLOAT_EQ(row.effectHit, expected.effectHit);
	EXPECT_FLOAT_EQ(row.effectHeal, expected.effectHeal);
	EXPECT_FLOAT_EQ(row.effectFlyAway, expected.effectFlyAway);
	EXPECT_FLOAT_EQ(row.effectAlignmentModification, expected.effectAlignmentModification);
	EXPECT_FLOAT_EQ(row.effectBeliefModification, expected.effectBeliefModification);
	EXPECT_FLOAT_EQ(row.radius, expected.radius);
	ExpectStrikeCounts(row);
}

/// The same strike with the game's file's values written in
TEST_F(LightningMiracleStrike, realInfoDatStrikeSynthetic)
{
	ExpectStrikeCounts(BaseLevelRow());
}
