/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/Implementations/TempleDestructionSystem.h"
#include "Particles/PlasmaCommand.h"
#include "Physics/TempleHeart.h"
#include "Temple/TempleDestruction.h"

// A temple's destruction on a fake world: what its heart does turn by turn, as the game's steps take it, and the blows
// it passes on

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{

/// Fractions given in turn, and the first triangle of any list
class TurnRandom final: public GameRandomInterface
{
public:
	void Init() noexcept override {}
	void Reset() noexcept override {}
	void Load(const GameRandomSeeds& /*seeds*/) noexcept override {}
	[[nodiscard]] const GameRandomSeeds& Current() const noexcept override { return _seeds; }
	uint32_t GameRand(uint32_t /*n*/, std::source_location /*where*/) noexcept override { return 0; }
	float GameFloatRand(float /*x*/, std::source_location /*where*/) noexcept override { return 0.0f; }
	uint32_t LocalRand(int32_t /*n*/) noexcept override
	{
		++draws;
		return 0;
	}
	float LocalFloatRand(float /*x*/) noexcept override
	{
		++draws;
		return 0.25f;
	}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const noexcept override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) noexcept override {}
	void CountOutsideStep(const std::source_location& /*where*/) noexcept override {}
	[[nodiscard]] uint32_t OutsideStepDraws() const noexcept override { return 0; }
	int32_t CrtRand() noexcept override { return 0; }
	void CrtSrand(uint32_t /*seed*/) noexcept override {}
	[[nodiscard]] uint32_t CrtSeed() const noexcept override { return 0; }

	uint32_t draws {0};

private:
	GameRandomSeeds _seeds {};
};

struct SpotVisual
{
	SpotVisualType type;
	glm::vec3 position;
	int32_t turns;
	entt::entity entity;
	std::optional<PlayerNames> player;
	std::optional<float> scale;
	std::vector<entt::entity> targets;
};

struct Tag
{
	entt::entity thing;
	int sample;
	bool loops;
};

/// Records what the destruction asks of the game, a turn of 100 ms
class FakeWorld final: public temple_world::World
{
public:
	[[nodiscard]] Registry& Entities() override { return registry; }
	[[nodiscard]] uint32_t MillisecondsPerTurn() const override { return 100; }
	[[nodiscard]] uint32_t Turn() const override { return turn; }
	[[nodiscard]] std::optional<PlayerNames> LocalPlayer() const override { return local; }
	[[nodiscard]] bool PlaygroundGame() const override { return playground; }
	[[nodiscard]] entt::entity CitadelHeartOf(PlayerNames player) const override
	{
		return player == heartPlayer ? heart : entt::entity {entt::null};
	}
	[[nodiscard]] std::vector<entt::entity> PartsOf(entt::entity /*heart*/) const override
	{
		std::vector<entt::entity> parts;
		std::ranges::copy_if(otherParts, std::back_inserter(parts),
		                     [this](entt::entity part) { return std::ranges::find(deleted, part) == deleted.end(); });
		if (std::ranges::find(deleted, heart) == deleted.end())
		{
			parts.push_back(heart);
		}
		return parts;
	}
	[[nodiscard]] std::vector<openblack::physics::temple_heart::Town> HeartTowns(PlayerNames /*owner*/) const override
	{
		return towns;
	}
	[[nodiscard]] bool Available(entt::entity thing) const override { return std::ranges::find(gone, thing) == gone.end(); }
	[[nodiscard]] float LifeOf(entt::entity /*thing*/) const override { return heartLife; }
	[[nodiscard]] glm::vec3 MapPointOf(entt::entity thing) const override
	{
		return registry.Get<const Transform>(thing).position;
	}
	[[nodiscard]] float HeightOf(entt::entity /*thing*/) const override { return 30.0f; }
	void TagSound(entt::entity thing, int sample, bool loops) override { tags.push_back({thing, sample, loops}); }
	void PlaySpark(entt::entity /*heart*/, bool hurt) override { sparks.push_back(hurt); }
	void RemoveSoundTags(entt::entity thing, int sample) override { removedTags.emplace_back(thing, sample); }
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, int32_t turns) override
	{
		const auto entity = registry.Create();
		visuals.push_back({type, position, turns, entity, std::nullopt, std::nullopt, {}});
		return entity;
	}
	[[nodiscard]] bool SpotVisualGoing(entt::entity visual) const override
	{
		return std::ranges::find(ended, visual) == ended.end();
	}
	void SetSpotVisualPlayer(entt::entity visual, PlayerNames player) override { Visual(visual).player = player; }
	void SetSpotVisualScale(entt::entity visual, float scale) override { Visual(visual).scale = scale; }
	void AddSpotVisualTarget(entt::entity visual, entt::entity thing) override { Visual(visual).targets.push_back(thing); }
	void PowerGlowByHeartbeat(entt::entity /*visual*/) override { ++glowBeats; }
	void AddPlasma(entt::entity visual, const particles::PlasmaCommand& command) override
	{
		plasma.emplace_back(visual, command);
	}
	[[nodiscard]] std::vector<model_surface::Triangle> DrawnTrianglesOf(entt::entity /*object*/) const override
	{
		// One triangle facing straight up
		model_surface::Triangle triangle;
		triangle[0] = {.position = {0.0f, 0.0f, 0.0f}, .normal = {0.0f, 1.0f, 0.0f}};
		triangle[1] = {.position = {1.0f, 0.0f, 0.0f}, .normal = {0.0f, 1.0f, 0.0f}};
		triangle[2] = {.position = {0.0f, 0.0f, 1.0f}, .normal = {0.0f, 1.0f, 0.0f}};
		return {triangle};
	}
	[[nodiscard]] glm::mat4 PlacementOf(entt::entity /*object*/) const override { return glm::mat4(1.0f); }
	[[nodiscard]] GameRandomInterface* LocalRandom() override { return &random; }
	[[nodiscard]] float GameFloatRand(float range) override
	{
		floatRanges.push_back(range);
		return smokeShare;
	}
	void HoldHeartbeat(float pace) override { heartbeat.push_back(pace); }
	void FadeHeart(entt::entity /*heart*/, uint8_t alpha) override { alphas.push_back(alpha); }
	void ShowShell(entt::entity /*heart*/, bool shown) override { shells.push_back(shown); }
	void DrawHeart(entt::entity /*heart*/, bool drawn) override { heartDrawn.push_back(drawn); }
	void BreakUpModel(entt::entity broken) override { brokenUp.push_back(broken); }
	void ToBeDeleted(entt::entity object) override { deleted.push_back(object); }
	void StartScript(std::string_view name) override { scripts.emplace_back(name); }

	SpotVisual& Visual(entt::entity entity) { return *std::ranges::find(visuals, entity, &SpotVisual::entity); }
	[[nodiscard]] std::vector<SpotVisualType> VisualTypes() const
	{
		std::vector<SpotVisualType> types;
		std::ranges::transform(visuals, std::back_inserter(types), &SpotVisual::type);
		return types;
	}

	Registry registry;
	TurnRandom random;
	uint32_t turn {0};
	std::vector<openblack::physics::temple_heart::Town> towns;
	std::vector<entt::entity> gone;
	float heartLife {1.0f};
	std::vector<bool> sparks;
	entt::entity heart {entt::null};
	PlayerNames heartPlayer {PlayerNames::PLAYER_ONE};
	std::optional<PlayerNames> local {PlayerNames::PLAYER_ONE};
	bool playground {false};
	float smokeShare {0.2f};
	std::vector<entt::entity> otherParts;
	std::vector<Tag> tags;
	std::vector<std::pair<entt::entity, int>> removedTags;
	std::vector<SpotVisual> visuals;
	std::vector<entt::entity> ended;
	std::vector<std::pair<entt::entity, particles::PlasmaCommand>> plasma;
	std::vector<float> floatRanges;
	std::vector<float> heartbeat;
	std::vector<uint8_t> alphas;
	std::vector<bool> shells;
	std::vector<bool> heartDrawn;
	std::vector<entt::entity> brokenUp;
	std::vector<entt::entity> deleted;
	std::vector<std::string> scripts;
	int glowBeats {0};
};

class TempleDestructionSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto owned = std::make_unique<FakeWorld>();
		world = owned.get();
		auto& registry = world->registry;
		heart = registry.Create();
		registry.Assign<Transform>(heart, glm::vec3(100.0f, 10.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Temple>(heart, PlayerNames::PLAYER_ONE);
		registry.Assign<CitadelHeart>(heart).citadel = heart;
		world->heart = heart;
		system = std::make_unique<TempleDestructionSystem>(std::move(owned));
	}

	Temple& TempleOf() { return world->registry.Get<Temple>(heart); }

	/// Turns of the heart until it is deleted, at most a minute's
	int RunToEnd()
	{
		int turns = 0;
		while (std::ranges::find(world->deleted, heart) == world->deleted.end() && turns < 600)
		{
			system->ProcessHeart(heart);
			++turns;
		}
		return turns;
	}

	FakeWorld* world {nullptr};
	std::unique_ptr<TempleDestructionSystem> system;
	entt::entity heart {entt::null};
};

} // namespace

TEST_F(TempleDestructionSystemTest, StartingTheDestructionDeletesTheTemplesOtherPartsNewestFirst)
{
	const auto newer = world->registry.Create();
	const auto older = world->registry.Create();
	world->otherParts = {newer, older};
	system->Start(heart);
	EXPECT_TRUE(TempleOf().destroying);
	EXPECT_EQ(TempleOf().destructionClock, 0.0f);
	EXPECT_EQ(world->deleted, (std::vector<entt::entity> {newer, older}));
}

TEST_F(TempleDestructionSystemTest, AHeartNotBeingDestroyedOrWithoutItsTempleDoesNothing)
{
	system->ProcessHeart(heart);
	EXPECT_TRUE(world->heartbeat.empty());
	EXPECT_TRUE(world->tags.empty());
	// Being destroyed without its temple, the clock stands still
	system->Start(heart);
	world->registry.Get<CitadelHeart>(heart).citadel = entt::null;
	system->ProcessHeart(heart);
	EXPECT_EQ(TempleOf().destructionClock, 0.0f);
	EXPECT_TRUE(world->tags.empty());
}

TEST_F(TempleDestructionSystemTest, TheDestructionTakesTwentyTwoSecondsInTheGamesOrder)
{
	system->Start(heart);
	const int turns = RunToEnd();
	EXPECT_GE(turns, 220);
	EXPECT_LE(turns, 221);
	EXPECT_FALSE(TempleOf().destroying);
	EXPECT_EQ(TempleOf().destructionClock, 0.0f);
	// The loop from the first turn over the temple, then at the explosion its tags go and the explosion sounds once
	ASSERT_EQ(world->tags.size(), 2u);
	EXPECT_EQ(world->tags[0].thing, heart);
	EXPECT_EQ(world->tags[0].sample, 171);
	EXPECT_TRUE(world->tags[0].loops);
	EXPECT_EQ(world->tags[1].sample, 168);
	EXPECT_FALSE(world->tags[1].loops);
	EXPECT_EQ(world->removedTags, (std::vector<std::pair<entt::entity, int>> {{heart, 171}}));
	// The beams' source, the glow at ten seconds over the heart, the explosion and the smoke, each for its turns and the
	// heart's player but the smoke, ten times as big
	EXPECT_EQ(world->VisualTypes(),
	          (std::vector<SpotVisualType> {SpotVisualType::MagicBeamOnCitadel, SpotVisualType::MagicFxOnCitadel,
	                                        SpotVisualType::ExplosionCitadel, SpotVisualType::EvilSmoke}));
	EXPECT_EQ(world->visuals[0].turns, 140);
	EXPECT_EQ(world->visuals[1].turns, 60);
	EXPECT_EQ(world->visuals[1].targets, (std::vector<entt::entity> {heart}));
	EXPECT_EQ(world->visuals[2].turns, 150);
	EXPECT_EQ(world->visuals[2].player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(world->visuals[3].turns, temple_destruction::SmokeTurns(100, 0.2f));
	EXPECT_FALSE(world->visuals[3].player.has_value());
	EXPECT_EQ(world->visuals[3].scale, 10.0f);
	EXPECT_EQ(world->floatRanges, (std::vector<float> {0.4f}));
	// The local player's heartbeat held each turn, then let go; the heart deleted at the end
	EXPECT_EQ(world->heartbeat.size(), static_cast<size_t>(turns) + 1);
	EXPECT_EQ(world->heartbeat.front(), temple_destruction::HeartbeatPace(0.1f));
	EXPECT_EQ(world->heartbeat.back(), 0.0f);
	EXPECT_EQ(world->deleted, (std::vector<entt::entity> {heart}));
	// The shell shown from fourteen seconds and gone with the smoke; the heart drawn whole again at the end
	EXPECT_EQ(std::ranges::count(world->shells, true), 1);
	EXPECT_EQ(world->alphas.back(), 255);
	// The glow beats with the heart from ten seconds on
	EXPECT_GT(world->glowBeats, 100);
}

TEST_F(TempleDestructionSystemTest, TheHeartIsNoLongerDrawnFromALittleAfterTheExplosionUntilItGoes)
{
	system->Start(heart);
	std::vector<float> clocks;
	while (TempleOf().destroying)
	{
		system->ProcessHeart(heart);
		clocks.push_back(TempleOf().destructionClock);
	}
	// Drawn as it starts, then each turn while the clock is not past 21.8 seconds, not drawn after; the last turn, as
	// it ends and goes, also draws it whole again
	ASSERT_EQ(world->heartDrawn.size(), clocks.size() + 2);
	EXPECT_TRUE(world->heartDrawn.front());
	EXPECT_TRUE(world->heartDrawn.back());
	const auto hidden = std::ranges::count(world->heartDrawn, false);
	EXPECT_GE(hidden, 1);
	for (size_t turn = 0; turn + 1 < clocks.size(); ++turn)
	{
		EXPECT_EQ(world->heartDrawn[turn + 1], !(clocks[turn] > temple_destruction::k_HiddenAfter)) << turn;
	}
	EXPECT_EQ(world->deleted, (std::vector<entt::entity> {heart}));
}

TEST_F(TempleDestructionSystemTest, TheBeamsLeapAcrossTheHeartFromItsSecondTurnUntilFourteenSeconds)
{
	system->Start(heart);
	system->ProcessHeart(heart);
	// The first turn only starts the beams' clock
	EXPECT_TRUE(world->plasma.empty());
	EXPECT_TRUE(world->visuals.empty());
	system->ProcessHeart(heart);
	ASSERT_EQ(world->plasma.size(), 1u);
	const auto& [source, beam] = world->plasma.front();
	EXPECT_EQ(source, world->visuals.front().entity);
	EXPECT_EQ(world->visuals.front().player, PlayerNames::PLAYER_ONE);
	const auto look = temple_destruction::BeamLookAt(temple_destruction::BeamShare(TempleOf().destructionClock));
	EXPECT_EQ(beam.life, look.life);
	EXPECT_EQ(beam.speed, look.speed);
	EXPECT_EQ(beam.alpha, look.alpha);
	// Leaving along the surface, arriving against it
	EXPECT_EQ(beam.startTangent, glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_EQ(beam.endTangent, glm::vec3(0.0f, -1.0f, 0.0f));
	RunToEnd();
	const auto beams = world->plasma.size();
	// From fourteen seconds no more
	EXPECT_GT(beams, 40u);
	EXPECT_LT(beams, 70u);
}

TEST_F(TempleDestructionSystemTest, AnEndedSpotVisualIsLetGoAndMadeAgain)
{
	system->Start(heart);
	system->ProcessHeart(heart);
	system->ProcessHeart(heart);
	const auto first = TempleOf().destructionBeamSource;
	ASSERT_TRUE(first != entt::null);
	world->ended.push_back(first);
	// Let go at once, and made again with the next beam
	system->ProcessHeart(heart);
	EXPECT_TRUE(TempleOf().destructionBeamSource != first);
	for (int turn = 0; turn < 5; ++turn)
	{
		system->ProcessHeart(heart);
	}
	EXPECT_TRUE(TempleOf().destructionBeamSource != entt::null);
	EXPECT_EQ(world->visuals.size(), 2u);
}

TEST_F(TempleDestructionSystemTest, StartingAgainStartsTheClockAndAnotherLoopAfresh)
{
	system->Start(heart);
	for (int turn = 0; turn < 50; ++turn)
	{
		system->ProcessHeart(heart);
	}
	system->Start(heart);
	EXPECT_EQ(TempleOf().destructionClock, 0.0f);
	system->ProcessHeart(heart);
	ASSERT_EQ(world->tags.size(), 2u);
	EXPECT_EQ(world->tags[1].sample, 171);
}

TEST_F(TempleDestructionSystemTest, AScriptBreaksUpAHeartBeingDestroyedAndStartsAnyOther)
{
	system->DeleteByScript(heart);
	EXPECT_TRUE(TempleOf().destroying);
	EXPECT_TRUE(world->brokenUp.empty());
	system->ProcessHeart(heart);
	system->DeleteByScript(heart);
	EXPECT_EQ(world->brokenUp, (std::vector<entt::entity> {heart}));
	// Its clock goes on
	EXPECT_GT(TempleOf().destructionClock, 0.0f);
}

TEST_F(TempleDestructionSystemTest, TheLocalPlayerLosingTheirTempleEndsTheGameOnce)
{
	system->EndTurn();
	EXPECT_TRUE(world->scripts.empty());
	system->Start(heart);
	system->EndTurn();
	system->EndTurn();
	EXPECT_EQ(world->scripts, (std::vector<std::string> {"GameOver"}));
	EXPECT_TRUE(world->registry.Context().gameOver);
}

TEST_F(TempleDestructionSystemTest, AnotherPlayersTempleOrAPlaygroundGameDoesNotEndIt)
{
	world->heartPlayer = PlayerNames::PLAYER_TWO;
	world->registry.Get<Temple>(heart).owner = PlayerNames::PLAYER_TWO;
	system->Start(heart);
	system->EndTurn();
	EXPECT_TRUE(world->scripts.empty());
	// Nor does it hold the local player's heartbeat
	system->ProcessHeart(heart);
	EXPECT_TRUE(world->heartbeat.empty());
	world->heartPlayer = PlayerNames::PLAYER_ONE;
	world->registry.Get<Temple>(heart).owner = PlayerNames::PLAYER_ONE;
	world->playground = true;
	system->EndTurn();
	EXPECT_TRUE(world->scripts.empty());
}

namespace
{
/// A building of the temple's player's town, standing at a place, built and half alive
openblack::physics::temple_heart::Town TownWith(entt::entity building)
{
	return {.buildings = {{.entity = building, .available = true, .life = 0.5f, .built = 1.0f}}};
}
} // namespace

TEST_F(TempleDestructionSystemTest, AHeartPassesABlowOnToABuildingBeamingAtItEveryTwoSeconds)
{
	const auto building = world->registry.Create();
	world->registry.Assign<Transform>(building, glm::vec3(130.0f, 5.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->towns = {TownWith(building)};
	world->turn = 21;
	EXPECT_EQ(system->ActualObjectToEffect(heart, true), building);
	// From the top of the heart to the top of the building, from a spot visual at the heart for ever, with a spark
	ASSERT_EQ(world->plasma.size(), 1u);
	const auto& [source, beam] = world->plasma.front();
	ASSERT_EQ(world->visuals.size(), 1u);
	EXPECT_EQ(source, world->visuals.front().entity);
	EXPECT_EQ(world->visuals.front().type, SpotVisualType::MagicBeamOnCitadel);
	EXPECT_EQ(world->visuals.front().turns, -1);
	EXPECT_EQ(world->visuals.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(beam.start, glm::vec3(100.0f, 40.0f, 200.0f));
	EXPECT_EQ(beam.end, glm::vec3(130.0f, 35.0f, 200.0f));
	EXPECT_EQ(beam.startTangent, particles::k_HeartPlasmaStartTangent);
	EXPECT_EQ(beam.endTangent, particles::k_HeartPlasmaEndTangent);
	EXPECT_EQ(beam.life, particles::k_HeartPlasmaLife);
	EXPECT_EQ(beam.speed, particles::k_HeartPlasmaSpeed);
	EXPECT_EQ(beam.alpha, particles::k_HeartPlasmaAlpha);
	EXPECT_EQ(world->sparks, (std::vector<bool> {false}));
	// Not again within two seconds, then again; a blow that breaks no buildings beams at nothing
	world->turn = 41;
	EXPECT_EQ(system->ActualObjectToEffect(heart, true), building);
	world->turn = 42;
	EXPECT_EQ(system->ActualObjectToEffect(heart, true), building);
	world->turn = 100;
	EXPECT_EQ(system->ActualObjectToEffect(heart, false), building);
	EXPECT_EQ(world->plasma.size(), 2u);
	EXPECT_EQ(world->visuals.size(), 1u);
}

TEST_F(TempleDestructionSystemTest, WithNothingToPassItOnToTheHeartTakesTheBlowAndBeamsAtItself)
{
	world->turn = 21;
	EXPECT_EQ(system->ActualObjectToEffect(heart, true), heart);
	ASSERT_EQ(world->plasma.size(), 1u);
	const auto& beam = world->plasma.front().second;
	// To a point of the heart's model, arriving against its surface, without a spark
	EXPECT_EQ(beam.start, glm::vec3(100.0f, 40.0f, 200.0f));
	EXPECT_EQ(beam.end.y, 0.0f);
	EXPECT_EQ(beam.endTangent, glm::vec3(0.0f, -1.0f, 0.0f));
	EXPECT_TRUE(world->sparks.empty());
	EXPECT_TRUE(TempleOf().beamTarget == entt::null);
}

TEST_F(TempleDestructionSystemTest, AHurtHeartBeamsAcrossItselfTheOftenerTheLessLifeItHas)
{
	world->heartLife = 0.5f;
	for (world->turn = 1; world->turn <= 60; ++world->turn)
	{
		system->ProcessHeart(heart);
	}
	// Every 17 turns at half its life, more than the interval after the last: at turns 18, 36 and 54, each with a spark
	ASSERT_EQ(world->plasma.size(), 3u);
	EXPECT_EQ(TempleOf().beamTurn, 54u);
	EXPECT_EQ(world->sparks, (std::vector<bool> {true, true, true}));
	const auto look = openblack::physics::temple_heart::DamageLookAt(0.5f);
	EXPECT_EQ(world->plasma.front().second.life, look.life);
	EXPECT_EQ(world->plasma.front().second.alpha, look.alpha);
	EXPECT_EQ(world->plasma.front().second.speed, particles::k_HeartPlasmaSpeed);
}

TEST_F(TempleDestructionSystemTest, AHeartWithMuchLifeOrABuildingToPassBlowsToBeamsWithoutSoundOrNotAtAll)
{
	world->heartLife = 0.7f;
	for (world->turn = 1; world->turn <= 30; ++world->turn)
	{
		system->ProcessHeart(heart);
	}
	EXPECT_EQ(world->plasma.size(), 1u);
	EXPECT_TRUE(world->sparks.empty());
	world->towns = {TownWith(world->registry.Create())};
	for (; world->turn <= 120; ++world->turn)
	{
		system->ProcessHeart(heart);
	}
	EXPECT_EQ(world->plasma.size(), 1u);
	// A whole heart beams not at all
	world->towns.clear();
	world->heartLife = 1.0f;
	for (; world->turn <= 200; ++world->turn)
	{
		system->ProcessHeart(heart);
	}
	EXPECT_EQ(world->plasma.size(), 1u);
}

TEST_F(TempleDestructionSystemTest, WhatItBeamedAtIsForgottenOnceGoneAndItsSourceOnceEnded)
{
	const auto building = world->registry.Create();
	world->registry.Assign<Transform>(building, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	world->towns = {TownWith(building)};
	world->turn = 21;
	static_cast<void>(system->ActualObjectToEffect(heart, true));
	EXPECT_TRUE(TempleOf().beamTarget == building);
	world->gone.push_back(building);
	world->ended.push_back(TempleOf().beamSource);
	system->ProcessHeart(heart);
	EXPECT_TRUE(TempleOf().beamTarget == entt::null);
	EXPECT_EQ(TempleOf().beamTurn, 0u);
	EXPECT_TRUE(TempleOf().beamSource == entt::null);
}
