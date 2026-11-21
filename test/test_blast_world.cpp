/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The blast of the testbed scene miracles.blast, cast at a point and played turn by turn in a small world of the game's
// own systems and data, cast as a seed of it in the hand casts it: on dry land a heap of rubble lies at the centre, eight
// times its model's size, and smoke rises for four seconds; on water there is none, and three rings spread and steam
// rises instead. Its bang and camera shake are at the point at sea level, and each of the first power-up's seven blasts
// has its own whoosh, bang and shake. The blast runs on the game's own
// effect files, which synthetic data can't stand for, so these tests run on the game's data only. Every expected value is the
// original's (docs/wiki/project/progress/physics/explosions.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerNoOp.h"
#include "Blast/CameraShake.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/GroundMarks.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Systems/Implementations/VortexSystem.h"
#include "ECS/Systems/Implementations/WaterRingSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "Magic/Core/SpellCastData.h"
#include "Magic/MagicTables.h"
#include "Particles/SoundAction.h"
#include "Particles/SpellLink.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

using namespace openblack;

namespace
{
/// Open sea everywhere, its floor 20 m down
class OpenSea final: public LandIslandInterface
{
public:
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return -20.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return -20.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { return _sea; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	lnd::LNDCell _sea {test::MakeWaterCell(0, true, false)};
	std::vector<lnd::LNDCountry> _countries;
};

/// An audio service that counts the animation effects it is asked to play, by sound action (the key's last attribute)
class HeardPlays final: public audio::AudioManagerNoOp
{
public:
	audio::Channel PlayAnimationEffect(audio::Owner /*owner*/, float /*distance*/, const audio::AnimKey& key,
	                                   audio::AnimAction action, audio::BankId /*bank*/, bool /*track*/, float /*minDistance*/,
	                                   float /*maxDistance*/) override
	{
		if (action == audio::AnimAction::Play)
		{
			plays.push_back(key[4]);
		}
		return audio::k_NoChannel;
	}
	[[nodiscard]] size_t PlaysOf(int32_t sound) const { return static_cast<size_t>(std::ranges::count(plays, sound)); }
	std::vector<int32_t> plays;
};

constexpr std::string_view k_Scene = "miracles.blast";
const glm::vec2 k_At {1005.0f, 1205.0f};

class BlastWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		if (!GameData())
		{
			GTEST_SKIP() << "the blast runs on the game's own effect files: no synthetic data stands for them";
		}
		LoadMesh(MeshId::TreeRootsPile, glm::vec3(0.0f), glm::vec3(0.0f));
		_rings =
		    &static_cast<ecs::systems::WaterRingSystem&>(Locator::waterRingSystem::emplace<ecs::systems::WaterRingSystem>());
		// the miracles' services the blast reaches
		Locator::snowSystem::emplace<ecs::systems::SnowSystem>();
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>();
		Locator::vortexSystem::emplace<ecs::systems::VortexSystem>();
		// what the effects log as they start, where the spot visuals they start show
		_log = std::make_shared<spdlog::sinks::ostream_sink_mt>(_logged);
		spdlog::get("game")->sinks().push_back(_log);
	}
	void TearDown() override
	{
		if (_log)
		{
			auto& sinks = spdlog::get("game")->sinks();
			sinks.erase(std::remove(sinks.begin(), sinks.end(), _log), sinks.end());
		}
		WorldHarness::TearDown();
	}

	/// The turns each spot visual of that number was started for, in the order they started
	[[nodiscard]] std::vector<int> SpotVisualsStarted(int number) const
	{
		std::vector<int> turns;
		const std::regex started("PSys: spot visual " + std::to_string(number) + R"( \([^)]*\) at [^\n]* for (-?\d+) turns)");
		const auto text = _logged.str();
		for (auto it = std::sregex_iterator(text.begin(), text.end(), started); it != std::sregex_iterator(); ++it)
		{
			turns.push_back(std::stoi((*it)[1].str()));
		}
		return turns;
	}

	void BeforePhysics() override
	{
		Locator::magicSystem::value().ProcessTurn();
		Locator::particleSystem::value().ProcessTurn();
	}
	void AfterPhysics() override { Locator::magicSystem::value().ProcessSpellParticlesEndOfLoop(); }

	/// Turns until the blast has had its 6 s
	void PlayTheBlast()
	{
		for (int turn = 0; turn < 70; ++turn)
		{
			Turn();
		}
	}

	/// The camera shakes the blast makes, each as it is made, and the most that shake at once: one game turn's frames
	/// count the shakes down by the turn's 100 ms after each turn, as the drawn frames do
	struct Shakes
	{
		std::vector<camera_shake::Checker> made;
		size_t mostAtOnce {0};
	};
	[[nodiscard]] Shakes PlayAndHearTheShakes(int turns)
	{
		Shakes shakes;
		for (int turn = 0; turn < turns; ++turn)
		{
			Turn();
			for (const auto& checker : camera_shake::Checkers())
			{
				if (checker.remainingMs == checker.totalMs)
				{
					shakes.made.push_back(checker);
				}
			}
			shakes.mostAtOnce = std::max(shakes.mostAtOnce, camera_shake::Checkers().size());
			camera_shake::Tick(100);
		}
		return shakes;
	}

	ecs::systems::WaterRingSystem* _rings {nullptr};
	/// The spot visuals of the blast: smoke on dry land, steam on water
	static constexpr int k_Smoke = 23;
	static constexpr int k_Steam = 22;

	/// The scene's miracle cast by the first player at the point on the land
	[[nodiscard]] entt::entity Cast(glm::vec2 at, float ground = k_Ground, std::string_view scene = k_Scene) const
	{
		const auto& miracle = Scene(scene).miracles.at(0);
		// what a seed of it in the hand casts: its usual size, its effect's prayer power and the player's timer
		const auto& info = Info();
		float size = 1.0f;
		if (const auto* radius = magic::GetMagicInfoAs<GMagicRadiusSpellInfo>(info, miracle.type))
		{
			size = radius->radiusForNormalCost;
		}
		const float timer = magic::GetTimerWhenPlayerCasting(info, miracle.type);
		const magic::SpellCastData cast {.magnitude = size,
		                                 .chants = magic::GetMagicEffectInfo(info, miracle.type).initialChants,
		                                 .duration = timer < 0.0f ? -1.0f : timer,
		                                 .maxObjectsToCreate = -1};
		const glm::vec3 point(at.x, ground, at.y);
		particles::ProcessInfo process {};
		process.handPos = point + glm::vec3(0.0f, miracle.handHeight, 0.0f);
		process.cameraForward = glm::vec3(0.0f, -1.0f, 0.0f);
		process.direction = glm::vec3(0.0f, -1.0f, 0.0f);
		return Locator::magicSystem::value().CastAtPoint(miracle.type, PlayerNames::PLAYER_ONE, point, cast, process);
	}

private:
	std::ostringstream _logged;
	std::shared_ptr<spdlog::sinks::ostream_sink_mt> _log;
	test::RestoreService<Locator::snowSystem> _restore_snowSystem;
	test::RestoreService<Locator::dayNightClock> _restore_dayNightClock;
	test::RestoreService<Locator::waterRingSystem> _restoreRings;
	test::RestoreService<Locator::vortexSystem> _restoreVortex;
};
} // namespace

// On dry land a heap of rubble lies at the blast's centre, eight times its model's size
TEST_P(BlastWorld, OnDryLandAHeapOfRubbleLiesAtTheCentre)
{
	ASSERT_NE(Cast(k_At), entt::entity {entt::null});
	for (int turn = 0; turn < 60 && ecs::ground_marks::NewestFirst().empty(); ++turn)
	{
		Turn();
	}
	const auto marks = ecs::ground_marks::NewestFirst();
	ASSERT_EQ(marks.size(), 1u);
	const auto& at = Registry().Get<const ecs::components::Transform>(marks.front()).position;
	EXPECT_LT(glm::distance(glm::vec2(at.x, at.z), k_At), 0.5f);
	EXPECT_EQ(Registry().Get<const ecs::components::Transform>(marks.front()).scale.x, ecs::ground_marks::k_ExplosionScale);
}

// On water there is no rubble: three rings spread from the blast instead
TEST_P(BlastWorld, OnWaterThereIsNoRubbleButThreeRingsSpread)
{
	Locator::terrainSystem::emplace<OpenSea>();
	ASSERT_NE(Cast(k_At, 0.0f), entt::entity {entt::null});
	PlayTheBlast();
	EXPECT_TRUE(ecs::ground_marks::NewestFirst().empty());
	EXPECT_EQ(_rings->GetRings().size(), 3u);
}

// On dry land smoke rises from the blast for four seconds of turns; no steam
TEST_P(BlastWorld, OnDryLandSmokeRisesForFourSeconds)
{
	ASSERT_NE(Cast(k_At), entt::entity {entt::null});
	PlayTheBlast();
	EXPECT_EQ(SpotVisualsStarted(k_Smoke), std::vector {40});
	EXPECT_TRUE(SpotVisualsStarted(k_Steam).empty());
}

// On water white steam rises instead of the smoke
TEST_P(BlastWorld, OnWaterSteamRisesInsteadOfSmoke)
{
	Locator::terrainSystem::emplace<OpenSea>();
	ASSERT_NE(Cast(k_At, 0.0f), entt::entity {entt::null});
	PlayTheBlast();
	EXPECT_EQ(SpotVisualsStarted(k_Steam), std::vector {40});
	EXPECT_TRUE(SpotVisualsStarted(k_Smoke).empty());
}

// The blast makes one reaction for the villagers about it, for its whole life, not one a turn
TEST_P(BlastWorld, TheBlastMakesOneReactionForItsWholeLife)
{
	const auto spell = Cast(k_At);
	ASSERT_NE(spell, entt::entity {entt::null});
	size_t most = 0;
	for (int turn = 0; turn < 70; ++turn)
	{
		Turn();
		const auto& all = ecs::effects::reactions::All();
		const auto made = static_cast<size_t>(
		    std::ranges::count_if(all, [spell](const auto& reaction) { return reaction.initiator == spell; }));
		most = std::max(most, made);
	}
	EXPECT_EQ(most, 1u);
}

// The blast's bang and its camera shake come from the falling beam's column, held at sea level under the point: they
// are at the point's x and z and as high above sea level as the column is when it bangs, however high the land is
// (docs/wiki/magic/lightning-explosion.md#the-visible-beam-sf_beamexplosionfx-pt-138)
TEST_P(BlastWorld, TheBangAndItsShakeAreAtThePointAtSeaLevel)
{
	camera_shake::Reset();
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardPlays&>(Locator::audio::emplace<HeardPlays>());
	ASSERT_NE(Cast(k_At), entt::entity {entt::null});
	const auto low = PlayAndHearTheShakes(70);
	ASSERT_EQ(low.made.size(), 1u);
	EXPECT_EQ(heard.PlaysOf(particles::SoundActionByName("SOUND_SPELL_LASERBEAM_EXPLODE")), 1u);
	// the same blast on land fifteen times as high
	camera_shake::Reset();
	Locator::terrainSystem::emplace<test::FlatDryIsland>(static_cast<uint8_t>(k_GroundAltitude * 10));
	const float high = test::FlatDryIsland::HeightOf(static_cast<uint8_t>(k_GroundAltitude * 10));
	ASSERT_NE(Cast(k_At, high), entt::entity {entt::null});
	const auto higher = PlayAndHearTheShakes(70);
	ASSERT_EQ(higher.made.size(), 1u);
	for (const auto* shakes : {&low, &higher})
	{
		const auto& point = shakes->made.front().point;
		EXPECT_LT(glm::distance(glm::vec2(point.x, point.z), k_At), 0.5f);
		// the column falls from 120 m to 0 in 0.4 s and bangs from 0.35 s: at most 15 m up
		EXPECT_GE(point.y, 0.0f);
		EXPECT_LE(point.y, 15.0f);
	}
	EXPECT_EQ(low.made.front().point.y, higher.made.front().point.y);
	camera_shake::Reset();
}

// Each of the first power-up's seven blasts, the centre's and the six 30 to 50 m round it, has its own whoosh as its beam
// falls, its own bang and its own camera shake at its own point, within 200 m for 0.7 s, up and down; the six come a
// third of a second apart, so several shake at once, and the camera takes the nearest of them
// (CameraShake.TheNearestDecides; docs/wiki/project/progress/miracles/blast.md#sound-and-shake)
TEST_P(BlastWorld, EachOfTheFirstPowerUpsBlastsHasItsOwnWhooshBangAndShake)
{
	camera_shake::Reset();
	const test::RestoreService<Locator::audio> restoreAudio;
	auto& heard = static_cast<HeardPlays&>(Locator::audio::emplace<HeardPlays>());
	ASSERT_NE(Cast(k_At, k_Ground, "miracles.blast_pu1"), entt::entity {entt::null});
	const auto shakes = PlayAndHearTheShakes(70);
	ASSERT_EQ(shakes.made.size(), 7u);
	EXPECT_EQ(heard.PlaysOf(particles::SoundActionByName("SOUND_SPELL_LASERBEAM_1")), 7u);
	EXPECT_EQ(heard.PlaysOf(particles::SoundActionByName("SOUND_SPELL_LASERBEAM_EXPLODE")), 7u);
	EXPECT_GE(shakes.mostAtOnce, 2u);
	int atTheCentre = 0;
	for (const auto& shake : shakes.made)
	{
		EXPECT_EQ(shake.maxDistance, 200.0f);
		EXPECT_EQ(shake.totalMs, 700);
		EXPECT_TRUE(shake.yOnly);
		EXPECT_EQ(shake.amplitude, 1.0f);
		const float out = glm::distance(glm::vec2(shake.point.x, shake.point.z), k_At);
		if (out < 0.5f)
		{
			++atTheCentre;
		}
		else
		{
			EXPECT_GE(out, 29.5f);
			EXPECT_LE(out, 50.5f);
		}
	}
	EXPECT_EQ(atTheCentre, 1);
	camera_shake::Reset();
}

INSTANTIATE_TEST_SUITE_P(Data, BlastWorld, ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
