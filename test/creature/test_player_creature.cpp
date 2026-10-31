/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player's own creature: the profile's mind file (the creature-file setting) and the script natives that act on
// that creature

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSize.h"
#include "Creature/CreatureSpells.h"
#include "Debug/StateHash.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/AudioQueries.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAutoscale.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFriends.h"
#include "ECS/Components/CreatureHelp.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/CreatureSizeLimits.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "ECS/Systems/Implementations/PlayerProfileSystem.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "TestTempFolder.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/FileSystemFakes.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureLeash;
using openblack::ecs::components::CreatureMindState;

namespace
{
class ProfileCreatureTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		static std::atomic<int> s_count {0};
		_root = openblack::test::UniqueTempPath("openblack_test_player_creature_" + std::to_string(s_count++));
		std::filesystem::create_directories(_root / "Scripts" / "CreatureMind");
		Locator::filesystem::value().SetGamePath(_root);
		Locator::config::emplace();
	}
	void TearDown() override
	{
		std::error_code ec;
		std::filesystem::remove_all(_root, ec);
	}

	void WriteMind(const std::string& file) const
	{
		std::ofstream(_root / "Scripts" / "CreatureMind" / file, std::ios::binary).put('\0');
	}

	// first, so that they go last
	const test::RestoreService<Locator::config> _config;
	const test::ScopedDefaultFileSystem _fileSystem;
	std::filesystem::path _root;
};

/// The leash service as the natives see it: the creature a player leads is what the test sets, and the calls that
/// teach, hand over or house a creature are recorded
class RecordingLeash final: public systems::LeashSystemInterface
{
public:
	std::optional<entt::entity> playersCreature;
	std::vector<std::pair<entt::entity, LeashType>> known;
	/// The leashes the creatures were told they do not know
	std::vector<std::pair<entt::entity, LeashType>> forgotten;
	std::vector<std::pair<entt::entity, bool>> leashable;
	std::vector<std::pair<entt::entity, glm::vec3>> homes;
	/// The creatures kept near their home, with the radius, and those told of a stage they were moved to
	std::vector<std::pair<entt::entity, float>> confined;
	std::vector<std::pair<entt::entity, uint32_t>> released;

	void ProcessTurn() override {}
	void Update(float) override {}
	[[nodiscard]] bool Knows(entt::entity creature, LeashType type) const override
	{
		return std::ranges::find(known, std::make_pair(creature, type)) != known.end();
	}
	void SetKnown(entt::entity creature, LeashType type, bool isKnown) override
	{
		if (isKnown)
		{
			known.emplace_back(creature, type);
			return;
		}
		forgotten.emplace_back(creature, type);
		std::erase(known, std::make_pair(creature, type));
	}
	[[nodiscard]] bool IsLeashable(entt::entity) const override { return false; }
	bool SetLeashable(entt::entity creature, bool value) override
	{
		leashable.emplace_back(creature, value);
		return true;
	}
	void SetOwner(entt::entity, PlayerNames) override {}
	void ClaimOnArrival(entt::entity) override {}
	[[nodiscard]] creature_leash::Refusal WhyNot(PlayerNames, entt::entity, LeashType) const override
	{
		return creature_leash::Refusal::None;
	}
	[[nodiscard]] std::optional<Refused> LastRefusal(PlayerNames) const override { return std::nullopt; }
	bool PutOn(entt::entity, LeashType) override { return false; }
	void TakeOff(entt::entity) override {}
	bool Toggle(entt::entity) override { return false; }
	bool ChangeType(entt::entity, LeashType) override { return false; }
	bool TieTo(entt::entity, entt::entity) override { return false; }
	void UntieToHand(entt::entity) override {}
	void ReturnToHand(entt::entity) override {}
	void SetWorks(entt::entity, bool) override {}
	[[nodiscard]] bool Works(entt::entity) const override { return false; }
	void PullAwayFromAction(entt::entity) override {}
	void ActOn(entt::entity, entt::entity) override {}
	void ConfineToHome(entt::entity creature, float radius) override { confined.emplace_back(creature, radius); }
	void ClearConfinement(entt::entity) override {}
	void ReleaseFromHome(entt::entity creature, uint32_t stage) override { released.emplace_back(creature, stage); }
	void SetHome(entt::entity creature, const glm::vec3& home) override { homes.emplace_back(creature, home); }
	[[nodiscard]] bool FreeOfHome(entt::entity) const override { return true; }
	[[nodiscard]] creature_leash::HomeKeeping HomeKeepingOf(entt::entity) const override { return {}; }
	[[nodiscard]] bool IsLeashed(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> TiedTo(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] LeashType TypeOf(entt::entity) const override { return LeashType::None; }
	[[nodiscard]] LeashType Picked(entt::entity) const override { return LeashType::None; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames) const override { return playersCreature; }
	bool PressKey(PlayerNames, creature_leash::LeashKey) override { return false; }
	bool TapCreature(PlayerNames, entt::entity) override { return false; }
	bool Shake(PlayerNames) override { return false; }
};

/// A mind system that saves one file for every creature, and does nothing else
class SavingMind final: public systems::CreatureMindSystemInterface
{
public:
	std::optional<creaturemind::MindFileData> file;
	/// The creatures told to point out a highlight, and whose last reward or punishment was forgotten
	std::vector<entt::entity> pointedOut;
	std::vector<entt::entity> cleared;
	/// The stages the creatures were moved to
	std::vector<std::pair<entt::entity, uint32_t>> stages;

	bool PointOutHighlight(entt::entity creature) override
	{
		pointedOut.push_back(creature);
		return true;
	}
	void ClearInteractionMagnitude(entt::entity creature) override { cleared.push_back(creature); }
	void MoveToStage(entt::entity creature, uint32_t stage) override { stages.emplace_back(creature, stage); }

	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return file; }
	void ClearLearning(entt::entity) override {}
	void FocusByScript(entt::entity, const glm::vec3&) override {}
	void ReleasedByScript(entt::entity) override {}
	void AbandonAction(entt::entity) override {}
	void GiveUpPlan(entt::entity) override {}
	void SetAgendaPriority(entt::entity, float) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(PlayerNames, size_t, const glm::vec3&, std::optional<entt::entity>) override {}
	void EmpathiseWithPlayer(PlayerNames, CreatureDesires, float, const glm::vec3&) override {}
	void EmpathiseWithTownDesire(PlayerNames, TownDesireInfo, float, const glm::vec3&) override {}
	bool SetKnowsAction(entt::entity, uint32_t, uint32_t, bool) override { return false; }
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	bool PlayAction(entt::entity, size_t, std::optional<bool>) override { return false; }
	bool PlayGesture(entt::entity, size_t) override { return false; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return false; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity, float) override {}
	bool ForceAction(entt::entity, size_t, bool, std::optional<creature_face::Request>, double) override { return false; }
	bool Sleep(entt::entity) override { return false; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return false; }
	bool Drink(entt::entity) override { return false; }
	bool Poo(entt::entity) override { return false; }
	bool Puke(entt::entity) override { return false; }
	void EndMeal(entt::entity, uint32_t, float, creature_eating::MealEnd) override {}
	bool Faint(entt::entity) override { return false; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity, bool) override {}
};

/// A world with a creature of the first player and a thing that is not a creature
class PlayerCreatureNativesTest: public ::testing::Test
{
protected:
	PlayerCreatureNativesTest()
	    : creature(test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE))
	    , other(Reg().Create())
	{
		Reg().Assign<components::Transform>(other, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	}

	static ecs::Registry& Reg() { return test::creature_world::World::Registry(); }

	test::creature_world::World world;
	RecordingLeash leash;
	entt::entity creature;
	entt::entity other;
};
} // namespace

TEST(PlayerCreature, ProfileMindPathJoinsTheFolderAndTheFile)
{
	const auto path = player_creature::ProfileMindPath("C4ba71b36.erc", std::filesystem::path("Scripts") / "CreatureMind");
	ASSERT_TRUE(path.has_value());
	EXPECT_EQ(*path, std::filesystem::path("Scripts") / "CreatureMind" / "C4ba71b36.erc");
}

TEST(PlayerCreature, AnEmptyFileNamesNoMind)
{
	EXPECT_FALSE(player_creature::ProfileMindPath("", "Scripts").has_value());
}

TEST(PlayerCreature, TheSaveWritesTheMindAndThePhysiqueBesideIt)
{
	const auto folder = std::filesystem::path("Scripts") / "CreatureMind";
	const auto targets = player_creature::SaveTargetsOf("C4ba71b36.erc", folder);
	ASSERT_TRUE(targets.mind.has_value());
	EXPECT_EQ(*targets.mind, folder / "C4ba71b36.erc");
	EXPECT_EQ(targets.physique, folder / "PhysiqueC4ba71b36.erc");
}

TEST(PlayerCreature, WithNoProfileFileOnlyThePhysiqueIsSavedAsPhysique)
{
	// the original joins the empty name as it is: the mind's path is the folder, which cannot be written, and the
	// physique's is the file "Physique"
	const auto folder = std::filesystem::path("Scripts") / "CreatureMind";
	const auto targets = player_creature::SaveTargetsOf("", folder);
	EXPECT_FALSE(targets.mind.has_value());
	EXPECT_EQ(targets.physique, folder / "Physique");
}

TEST(PlayerCreature, TheDefaultSettingIsTheFirstProfilesCreature)
{
	EXPECT_EQ(EngineConfig {}.profileCreatureFile, "C4ba71b36.erc");
}

TEST_F(ProfileCreatureTest, TheProfileHasACreatureWhenItsFileExists)
{
	WriteMind("Cacab45d4.erc");
	Locator::config::value().profileCreatureFile = "Cacab45d4.erc";
	EXPECT_TRUE(player_creature::ProfileHasCreature());
}

TEST_F(ProfileCreatureTest, AMissingFileIsNoCreature)
{
	Locator::config::value().profileCreatureFile = "C551091b1.erc";
	EXPECT_FALSE(player_creature::ProfileHasCreature());
}

TEST_F(ProfileCreatureTest, TheCreatureFileIsAKeptProfilesOwnElseTheSetting)
{
	const test::RestoreService<Locator::playerProfileSystem> profiles;
	Locator::config::value().profileCreatureFile = "Cacab45d4.erc";
	// without profiles, and with a run of the checks' profile (kept nowhere), the setting
	Locator::playerProfileSystem::reset();
	EXPECT_EQ(player_creature::ProfileCreatureFile(), "Cacab45d4.erc");
	Locator::playerProfileSystem::emplace<systems::PlayerProfileSystem>();
	EXPECT_EQ(player_creature::ProfileCreatureFile(), "Cacab45d4.erc");

	// the setting's creature is kept in the game's Scripts/CreatureMind, as it always was
	EXPECT_EQ(player_creature::CurrentMindFolder(), _root / "Scripts" / "CreatureMind");

	// a player's kept profiles: the setting until one is played, then its own file, in the profile's own folder
	test::MemoryFileSystem files;
	const auto profilesFolder = _root / "Profiles";
	auto& kept = static_cast<systems::PlayerProfileSystem&>(Locator::playerProfileSystem::emplace<systems::PlayerProfileSystem>(
	    &files, player_profiles::Store {.folder = profilesFolder, .runProfile = {}},
	    systems::PlayerProfileSystem::Machine {.accountName = {}, .ticks = [] { return 0u; }}));
	EXPECT_EQ(player_creature::ProfileCreatureFile(), "Cacab45d4.erc");
	EXPECT_EQ(player_creature::CurrentMindFolder(), _root / "Scripts" / "CreatureMind");
	kept.CreateProfile(u"Ann", 0);
	EXPECT_EQ(player_creature::ProfileCreatureFile(), "C4a5c8bc8.erc");
	EXPECT_EQ(player_creature::CurrentMindFolder(), profilesFolder / "_A_n_n");
	EXPECT_FALSE(player_creature::ProfileHasCreature());
	// a file of that name in the game's folder is not the profile's creature
	WriteMind("C4a5c8bc8.erc");
	EXPECT_FALSE(player_creature::ProfileHasCreature());
	std::filesystem::create_directories(profilesFolder / "_A_n_n");
	std::ofstream(profilesFolder / "_A_n_n" / "C4a5c8bc8.erc", std::ios::binary).put('\0');
	EXPECT_TRUE(player_creature::ProfileHasCreature());
	Locator::playerProfileSystem::reset();
}

TEST_F(ProfileCreatureTest, TheNightVoicesTryTheNameOfTheProfilePlayed)
{
	// none without profiles, or before one is played; then the played profile's name
	const test::RestoreService<Locator::playerProfileSystem> profiles;
	Locator::playerProfileSystem::reset();
	EXPECT_TRUE(ecs::audio_queries::ProfileName().empty());
	test::MemoryFileSystem files;
	auto& kept = static_cast<systems::PlayerProfileSystem&>(Locator::playerProfileSystem::emplace<systems::PlayerProfileSystem>(
	    &files, player_profiles::Store {.folder = _root / "Profiles", .runProfile = {}},
	    systems::PlayerProfileSystem::Machine {.accountName = {}, .ticks = [] { return 0u; }}));
	EXPECT_TRUE(ecs::audio_queries::ProfileName().empty());
	kept.CreateProfile(u"Ann", 0);
	EXPECT_EQ(ecs::audio_queries::ProfileName(), u"Ann");
	Locator::playerProfileSystem::reset();
}

TEST(PlayerCreature, ANewProfilesCreatureIsKeptInItsOwnFolderTheSettingsInTheGames)
{
	const auto game = std::filesystem::path("game") / "Scripts" / "CreatureMind";
	const auto player = std::filesystem::path("work") / "Profiles" / "_A_n_n";
	EXPECT_EQ(player_creature::ProfileMindFolder(player, game), player);
	EXPECT_EQ(player_creature::ProfileMindFolder(std::nullopt, game), game);
	EXPECT_EQ(*player_creature::ProfileMindPath("C4a5c8bc8.erc", player_creature::ProfileMindFolder(player, game)),
	          player / "C4a5c8bc8.erc");
}

TEST_F(ProfileCreatureTest, AnEmptySettingIsNoCreature)
{
	WriteMind("C4ba71b36.erc");
	Locator::config::value().profileCreatureFile.clear();
	EXPECT_FALSE(player_creature::ProfileHasCreature());
}

TEST_F(PlayerCreatureNativesTest, CallPlayerCreatureGivesTheCreatureThePlayerLeads)
{
	EXPECT_FALSE(player_creature::PlayersCreature(leash, PlayerNames::PLAYER_ONE).has_value());
	leash.playersCreature = creature;
	EXPECT_EQ(player_creature::PlayersCreature(leash, PlayerNames::PLAYER_ONE), creature);
}

TEST_F(PlayerCreatureNativesTest, DevFunctionsSixAndSevenGoToThePlayersCreaturesMind)
{
	SavingMind minds;
	// Taken without the player's creature, doing nothing
	EXPECT_TRUE(player_creature::DevFunction(leash, &minds, 6, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(player_creature::DevFunction(leash, &minds, 7, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(minds.pointedOut.empty());
	EXPECT_TRUE(minds.cleared.empty());

	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, &minds, 6, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(minds.pointedOut, std::vector<entt::entity> {creature});
	EXPECT_TRUE(minds.cleared.empty());
	EXPECT_TRUE(player_creature::DevFunction(leash, &minds, 7, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(minds.cleared, std::vector<entt::entity> {creature});
	// Without a mind they do nothing, and neither touches the leashes
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 6, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 7, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(minds.pointedOut.size(), 1u);
	EXPECT_EQ(minds.cleared.size(), 1u);
	EXPECT_TRUE(leash.known.empty());
	EXPECT_TRUE(leash.leashable.empty());
	// One not ported is refused
	EXPECT_FALSE(player_creature::DevFunction(leash, &minds, 5, PlayerNames::PLAYER_ONE));
}

TEST_F(PlayerCreatureNativesTest, DevFunctionsTwoAndThreeTeachTheLeashes)
{
	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 2, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(leash.Knows(creature, LeashType::Rope));
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 3, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(leash.Knows(creature, LeashType::Good));
	EXPECT_TRUE(leash.Knows(creature, LeashType::Evil));
	ASSERT_EQ(leash.leashable.size(), 1u);
	EXPECT_EQ(leash.leashable[0], std::make_pair(creature, true));
}

TEST(PlayerCreature, AHomeIsKeptInTheFixedPointOnTheGround)
{
	const auto home = player_creature::HomeOnGround(glm::vec3(1896.5f, 29.48f, 2520.06f), 7.0f);
	EXPECT_EQ(home.x, map_coords::ToMetres(map_coords::ToFixed(1896.5f)));
	EXPECT_EQ(home.z, map_coords::ToMetres(map_coords::ToFixed(2520.06f)));
	EXPECT_EQ(home.y, 7.0f);
	// the fixed point truncates towards 0, so less than one of its units is no distance at all
	EXPECT_EQ(player_creature::HomeOnGround(glm::vec3(0.0001f, 0.0f, -0.0001f), 0.0f), glm::vec3(0.0f));
}

TEST_F(PlayerCreatureNativesTest, SetCreatureHomeHousesOnlyACreature)
{
	player_creature::SetHome(leash, Reg(), other, glm::vec3(1.0f));
	player_creature::SetHome(leash, Reg(), entt::null, glm::vec3(1.0f));
	EXPECT_TRUE(leash.homes.empty());
	player_creature::SetHome(leash, Reg(), creature, glm::vec3(1.0f, 2.0f, 3.0f));
	ASSERT_EQ(leash.homes.size(), 1u);
	EXPECT_EQ(leash.homes[0].first, creature);
	EXPECT_EQ(leash.homes[0].second, glm::vec3(1.0f, 2.0f, 3.0f));
}

TEST_F(PlayerCreatureNativesTest, TheLeashSystemKeepsTheHomeItIsGiven)
{
	systems::LeashSystem system;
	system.SetHome(other, glm::vec3(5.0f));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<CreatureLeash>(other));
	system.SetHome(creature, glm::vec3(1896.5f, 30.0f, 2520.0f));
	EXPECT_EQ(Reg().Get<CreatureLeash>(creature).home, glm::vec3(1896.5f, 30.0f, 2520.0f));
}

TEST_F(PlayerCreatureNativesTest, SetCreatureDevStageMovesACreatureToAStage)
{
	player_creature::SetDevelopmentStage(Reg(), creature, 0);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 0u);
	player_creature::SetDevelopmentStage(Reg(), creature, player_creature::k_LastDevelopmentStage);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 13u);
}

TEST_F(PlayerCreatureNativesTest, SetCreatureDevStageLeavesAStageOutOfRangeAndOtherThingsAlone)
{
	player_creature::SetDevelopmentStage(Reg(), creature, 4);
	player_creature::SetDevelopmentStage(Reg(), creature, 14);
	player_creature::SetDevelopmentStage(Reg(), creature, -1);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 4u);
	player_creature::SetDevelopmentStage(Reg(), other, 2);
	EXPECT_FALSE(std::as_const(Reg()).AllOf<CreatureMindState>(other));
}

TEST_F(PlayerCreatureNativesTest, SetCreatureDevStageGoesToTheLeashesAndTheMindAtOnce)
{
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	const test::RestoreService<Locator::creatureMindSystem> restoreMinds;
	auto& leashes = static_cast<RecordingLeash&>(Locator::leashSystem::emplace<RecordingLeash>());
	auto& minds = static_cast<SavingMind&>(Locator::creatureMindSystem::emplace<SavingMind>());
	// moved to a stage, and again to the stage it is at: each move is told
	player_creature::SetDevelopmentStage(Reg(), creature, 5);
	player_creature::SetDevelopmentStage(Reg(), creature, 5);
	const std::vector<std::pair<entt::entity, uint32_t>> k_TwiceToFive {{creature, 5u}, {creature, 5u}};
	EXPECT_EQ(minds.stages, k_TwiceToFive);
	EXPECT_EQ(leashes.released, k_TwiceToFive);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 5u);
	// past the last stage or below the first is refused (the original would run past its table), as is anything else
	player_creature::SetDevelopmentStage(Reg(), creature, 14);
	player_creature::SetDevelopmentStage(Reg(), creature, -1);
	player_creature::SetDevelopmentStage(Reg(), other, 2);
	EXPECT_EQ(minds.stages, k_TwiceToFive);
	EXPECT_EQ(leashes.released, k_TwiceToFive);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 5u);
}

TEST_F(PlayerCreatureNativesTest, TheLeashSystemLetsAKeptCreatureGoFromTheLeavingStage)
{
	systems::LeashSystem system;
	system.ConfineToHome(creature, creature_leash::k_HomeConfinement);
	auto& leashes = Reg().Get<CreatureLeash>(creature);
	leashes.returning = true;
	system.ReleaseFromHome(creature, creature_leash::k_YoungUntilPhase - 1);
	EXPECT_EQ(leashes.confinementRadius, creature_leash::k_HomeConfinement);
	system.ReleaseFromHome(creature, creature_leash::k_YoungUntilPhase);
	EXPECT_EQ(leashes.confinementRadius, 0.0f);
	// only the radius goes
	EXPECT_TRUE(leashes.returning);
	system.ReleaseFromHome(other, 13);
	EXPECT_FALSE(std::as_const(Reg()).AllOf<CreatureLeash>(other));
}

TEST_F(PlayerCreatureNativesTest, SetCreatureNameWaitsForAMindNotYetSetUp)
{
	// the creature has not thought yet: the name waits for its mind
	EXPECT_TRUE(player_creature::SetName(Reg(), creature, u"Khalen"));
	const auto& mind = Reg().Get<CreatureMindState>(creature);
	EXPECT_FALSE(mind.learnt.has_value());
	ASSERT_TRUE(mind.scriptName.has_value());
	EXPECT_EQ(*mind.scriptName, u"Khalen");
}

TEST_F(PlayerCreatureNativesTest, SetCreatureNameNamesASetUpMindAtOnce)
{
	auto& mind = Reg().Get<CreatureMindState>(creature);
	mind.learnt.emplace().name = u"Matey";
	EXPECT_TRUE(player_creature::SetName(Reg(), creature, u"Laetes"));
	EXPECT_EQ(mind.learnt->name, u"Laetes");
	EXPECT_FALSE(mind.scriptName.has_value());
}

TEST_F(PlayerCreatureNativesTest, SetCreatureNameWaitsForAFileStillToBeTakenUp)
{
	auto& mind = Reg().Get<CreatureMindState>(creature);
	mind.learnt.emplace();
	mind.pendingFile = std::make_shared<const creaturemind::MindFileData>();
	EXPECT_TRUE(player_creature::SetName(Reg(), creature, u"Khalen"));
	EXPECT_TRUE(mind.learnt->name.empty());
	ASSERT_TRUE(mind.scriptName.has_value());
	EXPECT_EQ(*mind.scriptName, u"Khalen");
}

TEST_F(PlayerCreatureNativesTest, SetCreatureNameLeavesOtherThingsAlone)
{
	EXPECT_FALSE(player_creature::SetName(Reg(), other, u"Khalen"));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<CreatureMindState>(other));
}

TEST_F(PlayerCreatureNativesTest, DevFunctionTwoTeachesTheRopeLeash)
{
	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 2, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(leash.known.size(), 1u);
	EXPECT_EQ(leash.known[0], std::make_pair(creature, LeashType::Rope));
	EXPECT_TRUE(leash.leashable.empty());
}

TEST_F(PlayerCreatureNativesTest, DevFunctionThreeTeachesTheOtherLeashesAndHandsTheCreatureOver)
{
	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 3, PlayerNames::PLAYER_ONE));
	ASSERT_EQ(leash.known.size(), 2u);
	EXPECT_EQ(leash.known[0], std::make_pair(creature, LeashType::Evil));
	EXPECT_EQ(leash.known[1], std::make_pair(creature, LeashType::Good));
	ASSERT_EQ(leash.leashable.size(), 1u);
	EXPECT_EQ(leash.leashable[0], std::make_pair(creature, true));
}

TEST_F(PlayerCreatureNativesTest, DevFunctionWithoutACreatureDoesNothing)
{
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 2, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 3, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(leash.known.empty());
	EXPECT_TRUE(leash.leashable.empty());
}

TEST_F(PlayerCreatureNativesTest, DevFunctionOneStartsThePlayersCreatureGrowingUp)
{
	SavingMind minds;
	// Without the player's creature nothing is done
	EXPECT_TRUE(player_creature::DevFunction(leash, &minds, 1, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(minds.stages.empty());
	EXPECT_TRUE(leash.released.empty());
	EXPECT_TRUE(leash.confined.empty());

	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, &minds, 1, PlayerNames::PLAYER_ONE));
	const std::vector<std::pair<entt::entity, uint32_t>> k_ToTheFirstStage {{creature, 0u}};
	EXPECT_EQ(minds.stages, k_ToTheFirstStage);
	EXPECT_EQ(leash.released, k_ToTheFirstStage);
	const std::vector<std::pair<entt::entity, float>> k_KeptAtHome {{creature, creature_leash::k_HomeConfinement}};
	EXPECT_EQ(leash.confined, k_KeptAtHome);
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 0u);
	EXPECT_TRUE(leash.known.empty());
	// Without a mind service the stage is still taken
	Reg().Get<CreatureMindState>(creature).developmentPhase = 7;
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 1, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 0u);
	EXPECT_EQ(leash.confined.size(), 2u);
}

TEST_F(PlayerCreatureNativesTest, DevFunctionElevenForgetsTheHelpsMessagesAndTenDoesNothing)
{
	const auto toldOf = [](entt::entity thing) {
		auto& memory = Reg().AssignOrReplace<components::CreatureHelpMemory>(thing).memory;
		creature_help::MarkTold(memory, creature_help::Kind::PhysicalProblem, 4, 1000);
		return &memory;
	};
	auto* memory = toldOf(creature);
	// Without the player's creature nothing is forgotten
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 11, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(memory->entries.at(static_cast<size_t>(creature_help::Kind::PhysicalProblem)).size(), 1u);

	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 10, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(memory->entries.at(static_cast<size_t>(creature_help::Kind::PhysicalProblem)).size(), 1u);
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 11, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(memory->entries.at(static_cast<size_t>(creature_help::Kind::PhysicalProblem)).empty());
	EXPECT_TRUE(leash.known.empty());
	EXPECT_TRUE(leash.leashable.empty());
	// A creature the help never spoke of has nothing to forget
	Reg().Remove<components::CreatureHelpMemory>(creature);
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 11, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(Reg().AllOf<components::CreatureHelpMemory>(creature));
}

TEST_F(PlayerCreatureNativesTest, DevFunctionsEightAndNineLetThePlayersCreatureDieOrNot)
{
	// A creature can die from the start
	EXPECT_TRUE(Reg().Get<Creature>(creature).canDie);
	// Without the player's creature nothing is done
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 9, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(Reg().Get<Creature>(creature).canDie);

	leash.playersCreature = creature;
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 9, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(Reg().Get<Creature>(creature).canDie);
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 9, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(Reg().Get<Creature>(creature).canDie);
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 8, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(Reg().Get<Creature>(creature).canDie);
	EXPECT_TRUE(leash.known.empty());
	EXPECT_TRUE(leash.leashable.empty());

	// Something that is not a creature in the player's slot is left alone
	leash.playersCreature = other;
	EXPECT_TRUE(player_creature::DevFunction(leash, nullptr, 9, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<Creature>(other));
}

TEST_F(PlayerCreatureNativesTest, OtherDevFunctionsAreNotPorted)
{
	leash.playersCreature = creature;
	for (const int32_t function : {0, 4, 5, 12, 13, -1})
	{
		EXPECT_FALSE(player_creature::DevFunction(leash, nullptr, function, PlayerNames::PLAYER_ONE)) << function;
	}
	EXPECT_TRUE(leash.known.empty());
	EXPECT_TRUE(leash.leashable.empty());
}

TEST_F(PlayerCreatureNativesTest, CreatureInDevScriptMarksOnlyACreature)
{
	EXPECT_FALSE(Reg().Get<Creature>(creature).inDevScript);
	player_creature::SetInDevScript(Reg(), creature, true);
	EXPECT_TRUE(Reg().Get<Creature>(creature).inDevScript);
	player_creature::SetInDevScript(Reg(), creature, false);
	EXPECT_FALSE(Reg().Get<Creature>(creature).inDevScript);
	player_creature::SetInDevScript(Reg(), other, true);
	EXPECT_FALSE(std::as_const(Reg()).AllOf<Creature>(other));
}

TEST(PlayerCreature, AnAutoscaleStepGoesHalfwayTowardsTheOtherCreaturesSizeTimesTheFactor)
{
	// Khazar's creature follows the player's by 1.2, Lethys's by 1.5
	EXPECT_FLOAT_EQ(creature_size::AutoscaleStep(1.0f, 1.0f, 1.2f), 1.1f);
	EXPECT_FLOAT_EQ(creature_size::AutoscaleStep(0.4f, 0.8f, 1.5f), 0.8f);
	// a bigger creature shrinks towards it as well
	EXPECT_FLOAT_EQ(creature_size::AutoscaleStep(1.6f, 1.0f, 1.2f), 1.4f);
}

TEST(PlayerCreature, AnAutoscaleStepIsCappedAtTwo)
{
	EXPECT_FLOAT_EQ(creature_size::AutoscaleStep(1.9f, 2.0f, 1.5f), creature_size::k_AutoscaleLargest);
	EXPECT_FLOAT_EQ(creature_size::AutoscaleStep(2.0f, 2.0f, 1.0f), 2.0f);
	EXPECT_LT(creature_size::AutoscaleStep(1.9f, 1.9f, 1.0f), 2.0f);
	// a size that is not a number is not capped
	EXPECT_TRUE(std::isnan(creature_size::AutoscaleStep(std::numeric_limits<float>::quiet_NaN(), 1.0f, 1.2f)));
}

TEST_F(PlayerCreatureNativesTest, CreatureAutoscaleSetsOnlyACreature)
{
	EXPECT_TRUE(player_creature::SetAutoscale(Reg(), creature, true, 1.2f));
	const auto& autoscale = std::as_const(Reg()).Get<const components::CreatureAutoscale>(creature);
	EXPECT_TRUE(autoscale.enabled);
	EXPECT_FLOAT_EQ(autoscale.factor, 1.2f);
	EXPECT_TRUE(player_creature::SetAutoscale(Reg(), creature, false, 1.5f));
	EXPECT_FALSE(std::as_const(Reg()).Get<const components::CreatureAutoscale>(creature).enabled);
	EXPECT_FALSE(player_creature::SetAutoscale(Reg(), other, true, 1.2f));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<components::CreatureAutoscale>(other));
}

TEST_F(PlayerCreatureNativesTest, AnAutoscaledCreatureStepsTowardsTheLocalCreaturesSizeEachTurn)
{
	const auto rival = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f), PlayerNames::PLAYER_TWO);
	Reg().Get<Creature>(creature).size = 1.0f;
	Reg().Get<Creature>(rival).size = 0.4f;
	ASSERT_TRUE(player_creature::SetAutoscale(Reg(), rival, true, 1.5f));
	player_creature::Autoscale(Reg(), creature, {});
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 0.95f);
	player_creature::Autoscale(Reg(), creature, {});
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 1.225f);
	// the local creature itself is left as it is
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(creature).size, 1.0f);
}

TEST_F(PlayerCreatureNativesTest, AnAutoscaledCreatureFollowsTheLocalCreaturesSizeBeforeItsSpells)
{
	const auto rival = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f), PlayerNames::PLAYER_TWO);
	Reg().Get<Creature>(creature).size = 1.8f;
	auto& spells = Reg().AssignOrReplaceState<components::CreatureSpells>(creature);
	spells.spells[creature_spells::Spell::Big].phase = creature_spells::Phase::Holding;
	spells.spells[creature_spells::Spell::Big].before = 1.0f;
	Reg().Get<Creature>(rival).size = 1.0f;
	ASSERT_TRUE(player_creature::SetAutoscale(Reg(), rival, true, 1.2f));
	player_creature::Autoscale(Reg(), creature, {});
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 1.1f);
}

TEST_F(PlayerCreatureNativesTest, AnAutoscaledCreatureUnderASizeSpellChangesTheSizeTheSpellPutsBack)
{
	const auto rival = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f), PlayerNames::PLAYER_TWO);
	Reg().Get<Creature>(creature).size = 1.0f;
	Reg().Get<Creature>(rival).size = 0.5f;
	auto& spells = Reg().AssignOrReplaceState<components::CreatureSpells>(rival);
	spells.spells[creature_spells::Spell::Small].phase = creature_spells::Phase::Holding;
	spells.spells[creature_spells::Spell::Small].before = 0.9f;
	ASSERT_TRUE(player_creature::SetAutoscale(Reg(), rival, true, 1.2f));
	player_creature::Autoscale(Reg(), creature, {});
	// the step starts from its size now, and is kept for when the spell ends; its size now is the spell's
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 0.5f);
	const auto& after = std::as_const(Reg()).Get<const components::CreatureSpells>(rival);
	EXPECT_FLOAT_EQ(after.spells[creature_spells::Spell::Small].before, 0.85f);
}

TEST_F(PlayerCreatureNativesTest, NoAutoscaleWhenOffInALockedSelectOrWithoutTheLocalCreature)
{
	const auto rival = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f), PlayerNames::PLAYER_TWO);
	Reg().Get<Creature>(creature).size = 1.0f;
	Reg().Get<Creature>(rival).size = 0.4f;
	const auto none = [](entt::entity) { return false; };
	ASSERT_TRUE(player_creature::SetAutoscale(Reg(), rival, false, 1.5f));
	player_creature::Autoscale(Reg(), creature, none);
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 0.4f);
	ASSERT_TRUE(player_creature::SetAutoscale(Reg(), rival, true, 1.5f));
	// in a hand's locked select its turn stops short of the step
	player_creature::Autoscale(Reg(), creature, [rival](entt::entity entity) { return entity == rival; });
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 0.4f);
	player_creature::Autoscale(Reg(), std::nullopt, none);
	player_creature::Autoscale(Reg(), other, none);
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 0.4f);
	// another creature in the locked select leaves this one to its step; no test at all is none in one
	player_creature::Autoscale(Reg(), creature, [this](entt::entity entity) { return entity == creature; });
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 0.95f);
	player_creature::Autoscale(Reg(), creature, {});
	EXPECT_FLOAT_EQ(Reg().Get<Creature>(rival).size, 1.225f);
}

namespace
{
/// A mind file read as the cache keeps it, of a species row, with a size and an alignment
creature::CreatureMind Mind(uint32_t row)
{
	creature::CreatureMind mind;
	mind.result = creaturemind::MindResult::Success;
	mind.data.speciesRow = row;
	mind.data.alignment = 0.25f;
	mind.data.physique.strength = 0.75f;
	mind.data.physique.size = 2.0f;
	return mind;
}

uint64_t HashOf(const ecs::Registry& registry)
{
	state_hash::Hasher h;
	player_creature::HashCreatures(h, registry);
	return h.Value();
}
} // namespace

TEST(PlayerCreature, ALoadedMindGivesItsSpeciesAndBody)
{
	const auto mind = Mind(14);
	const auto plan = player_creature::PlanLoad(false, &mind, glm::vec2(1850.0f, 1300.0f));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->species, CreatureType::Mandrill);
	EXPECT_EQ(plan->size, 2.0f);
	EXPECT_EQ(plan->alignment, 0.25f);
	EXPECT_EQ(plan->strength, 0.75f);
}

TEST(PlayerCreature, ACreatureIsLoadedInTheMiddleOfThePointsCell)
{
	const auto mind = Mind(2);
	for (const auto point : {glm::vec2(1850.0f, 1300.0f), glm::vec2(1859.9f, 1309.99f), glm::vec2(1850.0001f, 1300.5f)})
	{
		const auto plan = player_creature::PlanLoad(false, &mind, point);
		ASSERT_TRUE(plan.has_value());
		EXPECT_EQ(plan->position, glm::vec3(1855.0f, 0.0f, 1305.0f)) << point.x << " " << point.y;
	}
}

TEST(PlayerCreature, NoCreatureIsLoadedWhenThePlayerHasOneOrTheFileGivesNone)
{
	const auto mind = Mind(14);
	EXPECT_FALSE(player_creature::PlanLoad(true, &mind, glm::vec2(0.0f)).has_value());
	EXPECT_FALSE(player_creature::PlanLoad(false, nullptr, glm::vec2(0.0f)).has_value());
	auto unread = Mind(14);
	unread.result = creaturemind::MindResult::ErrCantOpen;
	EXPECT_FALSE(player_creature::PlanLoad(false, &unread, glm::vec2(0.0f)).has_value());
	const auto noSpecies = Mind(17);
	EXPECT_FALSE(player_creature::PlanLoad(false, &noSpecies, glm::vec2(0.0f)).has_value());
}

TEST_F(PlayerCreatureNativesTest, ALiveCreaturesMindFileTakesItsBodyAndServices)
{
	SavingMind minds;
	minds.file = creaturemind::MindFileData {};
	minds.file->physique.age = 88;
	minds.file->leashFlags = std::array<uint32_t, 4> {7, 0, 0, 0};
	auto& self = Reg().Get<Creature>(creature);
	self.fatness = 0.6f;
	self.inDevScript = true;
	Reg().Get<components::CreatureMorph>(creature).shownFatness = 0.55f;
	Reg().Get<components::CreatureTattoos>(creature).slots[1] = {.design = 13, .site = 5, .colour = {0xAB, 0xCD, 0xEF}};
	Reg().AssignOrReplaceState<components::CreatureFightHealth>(creature, components::CreatureFightHealth {.health = 0.5f});
	test::creature_loop_fakes::CallLog log;
	test::creature_loop_fakes::FakePhysiology bodies {log};
	bodies.needs[creature] = {.age = 90, .turns = 202500, .energy = 0.25f, .exhaustion = 0.375f};
	leash.known = {{creature, LeashType::Rope}, {creature, LeashType::Good}};

	const auto saved = player_creature::ToMindFile(std::as_const(Reg()), minds, &bodies, &leash, creature);
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(saved->physique.fatness, 0.6f);
	EXPECT_EQ(saved->physique.previousFatness, 0.55f);
	EXPECT_EQ(saved->physique.age, 90u);
	EXPECT_EQ(saved->physique.turns, 202500u);
	EXPECT_EQ(saved->physique.energy, 0.25f);
	EXPECT_EQ(saved->physique.needs[1], 0.375f);
	EXPECT_EQ(saved->fightHealth, 0.5f);
	EXPECT_EQ(saved->inDevScript, 1u);
	EXPECT_EQ(saved->leashFlags, (std::array<uint32_t, 4> {7, 0, 1, 1}));
	ASSERT_TRUE(saved->tattooSlots.has_value());
	EXPECT_EQ(saved->tattooSlots->at(0), 0xF0u);
	EXPECT_EQ(saved->tattooSlots->at(1), 0xABCDEF5Du);

	// without the services the file's needs and leash flags stay
	const auto alone = player_creature::ToMindFile(std::as_const(Reg()), minds, nullptr, nullptr, creature);
	ASSERT_TRUE(alone.has_value());
	EXPECT_EQ(alone->physique.age, 88u);
	EXPECT_EQ(alone->leashFlags, (std::array<uint32_t, 4> {7, 0, 0, 0}));
	// nothing for anything but a creature, or a mind not set up
	EXPECT_FALSE(player_creature::ToMindFile(std::as_const(Reg()), minds, &bodies, &leash, other).has_value());
	minds.file.reset();
	EXPECT_FALSE(player_creature::ToMindFile(std::as_const(Reg()), minds, &bodies, &leash, creature).has_value());
}

TEST(PlayerCreature, ALoadPlansFatnessIsTheFiles)
{
	auto mind = Mind(14);
	mind.data.physique.fatness = 0.4f;
	mind.data.physique.previousFatness = 0.37f;
	const auto plan = player_creature::PlanLoad(false, &mind, glm::vec2(1850.0f, 1300.0f));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->fatness, 0.4f);
	EXPECT_EQ(plan->previousFatness, 0.37f);
	EXPECT_EQ(player_creature::PlanScriptLoad(7, &mind, glm::vec2(0.0f))->fatness, 0.4f);
}

TEST(PlayerCreature, ALoadPlanTakesTheLeashesTheFileKeeps)
{
	auto mind = Mind(14);
	EXPECT_FALSE(player_creature::PlanLoad(false, &mind, glm::vec2(0.0f))->leashes.has_value());
	mind.data.leashFlags = std::array<uint32_t, 4> {0, 1, 0, 1};
	const auto plan = player_creature::PlanLoad(false, &mind, glm::vec2(1850.0f, 1300.0f));
	ASSERT_TRUE(plan.has_value());
	ASSERT_TRUE(plan->leashes.has_value());
	EXPECT_TRUE(plan->leashes->evil);
	EXPECT_FALSE(plan->leashes->rope);
	EXPECT_TRUE(plan->leashes->good);
	EXPECT_TRUE(player_creature::PlanScriptLoad(7, &mind, glm::vec2(0.0f))->leashes.has_value());
}

TEST_F(PlayerCreatureNativesTest, ALoadedCreatureKnowsTheLeashesItsFileSaysAndForgetsTheOthers)
{
	player_creature::LoadPlan plan {.species = CreatureType::Mandrill, .strength = 0.5f, .position = glm::vec3(0.0f)};
	player_creature::RestoreLeashes(leash, creature, plan);
	EXPECT_TRUE(leash.known.empty());
	EXPECT_TRUE(leash.forgotten.empty());
	plan.leashes = creature_mind_body::Leashes {.evil = false, .rope = true, .good = true};
	player_creature::RestoreLeashes(leash, creature, plan);
	EXPECT_EQ(leash.known,
	          (std::vector<std::pair<entt::entity, LeashType>> {{creature, LeashType::Rope}, {creature, LeashType::Good}}));
	EXPECT_EQ(leash.forgotten, (std::vector<std::pair<entt::entity, LeashType>> {{creature, LeashType::Evil}}));
}

TEST_F(PlayerCreatureNativesTest, ALoadedCreatureTakesTheFilesFatnessAndShowsItEasedTwice)
{
	const auto mind = Mind(0);
	auto plan = *player_creature::PlanLoad(false, &mind, glm::vec2(100.0f));
	plan.fatness = 0.4f;
	plan.previousFatness = 0.37f;
	player_creature::RestoreBody(Reg(), nullptr, creature, plan);
	const auto& self = Reg().Get<Creature>(creature);
	EXPECT_EQ(self.fatness, 0.4f);
	const auto& morph = Reg().Get<components::CreatureMorph>(creature);
	EXPECT_FLOAT_EQ(morph.shownFatness, 0.39f);
	const auto drawn = creature_morph::FromAttributes(self.alignment, morph.shownFatness, self.strength,
	                                                  archetypes::CreatureArchetype::SpeciesStrength(self.species));
	EXPECT_EQ(morph.drawn.evilGood, drawn.evilGood);
	EXPECT_EQ(morph.drawn.thinFat, drawn.thinFat);
	EXPECT_EQ(morph.drawn.weakStrong, drawn.weakStrong);
	// anything but a creature is left alone
	player_creature::RestoreBody(Reg(), nullptr, other, plan);
	EXPECT_FALSE(Reg().AllOf<components::CreatureMorph>(other));
}

// The profile's creature comes onto the land through static: gone at once, then back in full sight over three seconds
TEST(PlayerCreature, TheLoadedCreatureFizzesOutAtOnceThenBackInOverThreeSeconds)
{
	test::creature_loop_fakes::CallLog log;
	test::creature_loop_fakes::FakeFizz fizz {log};
	const auto creature = static_cast<entt::entity>(5);
	player_creature::FizzInLoaded(fizz, creature);
	using Set = test::creature_loop_fakes::FakeFizz::Set;
	EXPECT_EQ(fizz.sets, (std::vector<Set> {{.creature = creature, .target = 1.0f, .seconds = 0.0f, .goesForGood = false},
	                                        {.creature = creature, .target = 0.0f, .seconds = 3.0f, .goesForGood = false}}));
}

// The mind file's reader puts the tattoos the file keeps on the creature made from it, whether the profile's creature or
// a script's, and a file too old to keep them leaves its skin bare
TEST_F(PlayerCreatureNativesTest, ACreatureMadeFromAMindFileWearsTheFilesTattoos)
{
	auto mind = Mind(0);
	auto plan = *player_creature::PlanLoad(false, &mind, glm::vec2(100.0f));
	EXPECT_FALSE(plan.tattoos.has_value());
	const auto revision = Reg().Get<components::CreatureTattoos>(creature).revision;
	player_creature::RestoreBody(Reg(), nullptr, creature, plan);
	EXPECT_EQ(Reg().Get<components::CreatureTattoos>(creature).revision, revision);

	std::array<uint32_t, creature_tattoo::k_SlotCount> words {};
	words.fill(0xF0u);
	words[1] = 0xABCDEF5Du;
	mind.data.tattooSlots = words;
	plan = *player_creature::PlanLoad(false, &mind, glm::vec2(100.0f));
	ASSERT_TRUE(plan.tattoos.has_value());
	ASSERT_TRUE(player_creature::PlanScriptLoad(7, &mind, glm::vec2(0.0f))->tattoos.has_value());
	player_creature::RestoreBody(Reg(), nullptr, creature, plan);
	const auto& tattoos = Reg().Get<components::CreatureTattoos>(creature);
	EXPECT_EQ(tattoos.slots, *plan.tattoos);
	EXPECT_EQ(tattoos.slots[1], creature_tattoo::FromWord(0xABCDEF5Du));
	EXPECT_EQ(tattoos.revision, revision + 1);
}

TEST_F(PlayerCreatureNativesTest, ALoadedCreaturesBodyTakesTheFilesNeedsBeforeItsFirstTurn)
{
	test::creature_loop_fakes::CallLog log;
	test::creature_loop_fakes::FakePhysiology bodies {log};
	// as the species starts it
	bodies.needs[creature] = {.warmth = 0.3f, .energy = 1.0f, .life = 1.0f, .meals = 2};
	auto mind = Mind(0);
	mind.data.physique.turns = 198003;
	mind.data.physique.age = 88;
	mind.data.physique.energy = 0.847755f;
	mind.data.physique.itchiness = 0.25f;
	mind.data.physique.needs = {0.0f, 0.31391f, 0.112615f, 0.55f, 0.75f, 0.95f};
	const auto plan = *player_creature::PlanLoad(false, &mind, glm::vec2(100.0f));
	player_creature::RestoreBody(Reg(), &bodies, creature, plan);
	EXPECT_EQ(bodies.needsSet, 1);
	const auto& needs = bodies.needs.at(creature);
	EXPECT_EQ(needs.turns, 198003u);
	EXPECT_EQ(needs.age, 88u);
	EXPECT_EQ(needs.energy, 0.847755f);
	EXPECT_EQ(needs.itchiness, 0.25f);
	EXPECT_EQ(needs.poo, 0.0f);
	EXPECT_EQ(needs.exhaustion, 0.31391f);
	EXPECT_EQ(needs.dehydration, 0.112615f);
	// not in the file
	EXPECT_EQ(needs.warmth, 0.3f);
	EXPECT_EQ(needs.life, 1.0f);
	EXPECT_EQ(needs.meals, 2u);
	// without a body nothing is set
	player_creature::RestoreBody(Reg(), &bodies, other, plan);
	EXPECT_EQ(bodies.needsSet, 1);
}

// A body swapped into takes the other mind file's strength, fatness and tattoos, and keeps its own species, size and
// alignment
TEST_F(PlayerCreatureNativesTest, ASwappedBodyTakesTheFilesStrengthAndTattoosButKeepsItsSizeAndAlignment)
{
	auto& self = Reg().Get<Creature>(creature);
	self.size = 1.7f;
	self.alignment = -0.6f;
	self.strength = 0.2f;
	const auto species = self.species;
	const auto revision = Reg().Get<components::CreatureTattoos>(creature).revision;
	creature_tattoo::Slots slots {};
	slots[2] = {.design = 7, .site = 2, .colour = {1, 2, 3}};
	const creature_mind_body::Body body {.species = CreatureType::Sheep,
	                                     .alignment = 0.9f,
	                                     .strength = 0.8f,
	                                     .size = 0.4f,
	                                     .tattoos = slots,
	                                     .fatness = 0.3f,
	                                     .previousFatness = 0.3f};
	player_creature::TakeUpBody(Reg(), nullptr, creature, body);
	EXPECT_EQ(self.strength, 0.8f);
	EXPECT_EQ(self.fatness, 0.3f);
	EXPECT_EQ(self.size, 1.7f);
	EXPECT_EQ(self.alignment, -0.6f);
	EXPECT_EQ(self.species, species);
	const auto& tattoos = Reg().Get<components::CreatureTattoos>(creature);
	EXPECT_EQ(tattoos.slots[2], slots[2]);
	EXPECT_EQ(tattoos.revision, revision + 1);
	// a file without tattoos leaves them as they are
	auto bare = body;
	bare.tattoos.reset();
	player_creature::TakeUpBody(Reg(), nullptr, creature, bare);
	EXPECT_EQ(Reg().Get<components::CreatureTattoos>(creature).revision, revision + 1);
	// anything but a creature is left alone
	player_creature::TakeUpBody(Reg(), nullptr, other, body);
	EXPECT_FALSE(Reg().AllOf<Creature>(other));
}

TEST_F(PlayerCreatureNativesTest, TheHashPartFollowsEachCreaturesFields)
{
	const auto before = HashOf(Reg());
	Reg().Get<Creature>(creature).inDevScript = true;
	const auto inDevScript = HashOf(Reg());
	EXPECT_NE(inDevScript, before);
	Reg().Get<CreatureMindState>(creature).developmentPhase = 3;
	const auto staged = HashOf(Reg());
	EXPECT_NE(staged, inDevScript);
	Reg().Get<Creature>(creature).size = 1.5f;
	const auto sized = HashOf(Reg());
	EXPECT_NE(sized, staged);
	if (!std::as_const(Reg()).AllOf<CreatureLeash>(creature))
	{
		Reg().AssignState<CreatureLeash>(creature);
	}
	Reg().Get<CreatureLeash>(creature).known.flip(0);
	EXPECT_NE(HashOf(Reg()), sized);
}

TEST(PlayerCreature, TheHashPartIsEmptyWithoutACreature)
{
	const ecs::Registry registry;
	EXPECT_EQ(HashOf(registry), state_hash::Hasher {}.Value());
}

TEST(PlayerCreature, TheGameFoldersFirstProfileCreatureIsAMandrill)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		game = std::getenv("OPENBLACK_TEST_GAME_PATH");
	}
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	if (!spdlog::get("game"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::ScopedDefaultFileSystem fileSystem;
	Locator::filesystem::value().SetGamePath(game);
	const auto path =
	    Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true) / std::string(k_DefaultProfileCreatureFile);
	const auto mind = resources::CreatureMindLoader {}(resources::CreatureMindLoader::FromDiskTag {}, path);
	ASSERT_TRUE(mind->Loaded());
	const auto plan = player_creature::PlanLoad(false, mind.get(), glm::vec2(1850.0f, 1300.0f));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->species, CreatureType::Mandrill);
	EXPECT_TRUE(plan->size.has_value());
}

TEST(PlayerCreature, AScriptLoadTakesTheScriptsSpeciesAndTheFilesBody)
{
	// Khazar's: a Giant Ape's file (row 0) loaded as a tortoise (type 7)
	const auto mind = Mind(0);
	const auto plan = player_creature::PlanScriptLoad(7, &mind, glm::vec2(2540.93f, 1916.63f));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->species, CreatureType::Tortoise);
	EXPECT_EQ(plan->size, 2.0f);
	EXPECT_EQ(plan->alignment, 0.25f);
	EXPECT_EQ(plan->strength, 0.75f);
	// type 0 is the first row of the creature tables
	EXPECT_EQ(player_creature::PlanScriptLoad(0, &mind, glm::vec2(0.0f))->species, CreatureType::GiantApe);
	// the file's own row is not read, even when it is no species
	const auto noSpecies = Mind(17);
	EXPECT_EQ(player_creature::PlanScriptLoad(4, &noSpecies, glm::vec2(0.0f))->species, CreatureType::Wolf);
}

TEST(PlayerCreature, AScriptLoadIsMadeAtThePointItselfNotItsCellsMiddle)
{
	const auto mind = Mind(0);
	const auto plan = player_creature::PlanScriptLoad(7, &mind, glm::vec2(2540.93f, 1916.63f));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->position.x, map_coords::ToMetres(map_coords::ToFixed(2540.93f)));
	EXPECT_EQ(plan->position.z, map_coords::ToMetres(map_coords::ToFixed(1916.63f)));
	EXPECT_EQ(plan->position.y, 0.0f);
	EXPECT_NE(plan->position, player_creature::PlanLoad(false, &mind, glm::vec2(2540.93f, 1916.63f))->position);
}

TEST(PlayerCreature, NoScriptLoadWithoutAReadFileOrOfATypeOutOfTheTables)
{
	const auto mind = Mind(0);
	EXPECT_FALSE(player_creature::PlanScriptLoad(7, nullptr, glm::vec2(0.0f)).has_value());
	auto unread = Mind(0);
	unread.result = creaturemind::MindResult::ErrCantOpen;
	EXPECT_FALSE(player_creature::PlanScriptLoad(7, &unread, glm::vec2(0.0f)).has_value());
	for (const int32_t type : {17, 18, -1, 1000})
	{
		EXPECT_FALSE(player_creature::PlanScriptLoad(type, &mind, glm::vec2(0.0f)).has_value()) << type;
	}
	EXPECT_TRUE(player_creature::PlanScriptLoad(16, &mind, glm::vec2(0.0f)).has_value());
}

TEST_F(PlayerCreatureNativesTest, AScriptLoadedCreatureIsLedKnowsTheLeashesAndIsGrownUp)
{
	player_creature::SettleScriptLoaded(leash, Reg(), creature);
	ASSERT_EQ(leash.leashable.size(), 1u);
	EXPECT_EQ(leash.leashable[0], std::make_pair(creature, true));
	ASSERT_EQ(leash.known.size(), 3u);
	EXPECT_EQ(leash.known[0], std::make_pair(creature, LeashType::Rope));
	EXPECT_EQ(leash.known[1], std::make_pair(creature, LeashType::Evil));
	EXPECT_EQ(leash.known[2], std::make_pair(creature, LeashType::Good));
	EXPECT_EQ(Reg().Get<CreatureMindState>(creature).developmentPhase, 13u);
}

namespace
{
/// LOAD_CREATURE as the native calls it: a game folder of its own with the mind files the test writes, the world's
/// registry and caches, a flat land and a recording leash put in the locator, and the scripting log's lines kept
class ScriptLoadCreatureTest: public ::testing::Test
{
protected:
	ScriptLoadCreatureTest()
	    : _folder(std::string("openblack_test_script_load_creature_") +
	              ::testing::UnitTest::GetInstance()->current_test_info()->name())
	    , _lines(std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(16))
	{
		Locator::filesystem::value().SetGamePath(_folder.Path());
		Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {1});
		leash = &static_cast<RecordingLeash&>(Locator::leashSystem::emplace<RecordingLeash>());
		_log = spdlog::get("scripting");
		if (!_log)
		{
			_log = spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		_log->sinks().push_back(_lines);
		// the players the land has made
		for (const auto name : {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO})
		{
			Reg().Assign<components::Player>(Reg().Create(), name);
		}
	}
	~ScriptLoadCreatureTest() override { std::erase(_log->sinks(), _lines); }
	ScriptLoadCreatureTest(const ScriptLoadCreatureTest&) = delete;
	ScriptLoadCreatureTest& operator=(const ScriptLoadCreatureTest&) = delete;
	ScriptLoadCreatureTest(ScriptLoadCreatureTest&&) = delete;
	ScriptLoadCreatureTest& operator=(ScriptLoadCreatureTest&&) = delete;

	static ecs::Registry& Reg() { return test::creature_world::World::Registry(); }

	/// A mind file of a species row in the game's mind folder
	void WriteMind(const std::string& file, uint32_t row) const
	{
		creaturemind::MindFileData data;
		data.speciesRow = row;
		data.alignment = 0.25f;
		data.physique.strength = 0.75f;
		data.physique.size = 2.0f;
		_folder.Write(std::filesystem::path("Scripts") / "CreatureMind" / file, creaturemind::Write(data));
	}

	[[nodiscard]] static size_t Creatures()
	{
		size_t count = 0;
		std::as_const(Reg()).Each<const Creature>([&count](entt::entity, const Creature&) { ++count; });
		return count;
	}

	/// A creature as the game makes one, moved to no stage: at the first stage, its desires made, each off and not held
	/// back, and counted as set for the stage, so that its mind's first turn brings none of the first stage's
	static void ExpectAtFirstStageWithEveryDesireOff(entt::entity creature)
	{
		const auto& mind = std::as_const(Reg()).Get<const CreatureMindState>(creature);
		EXPECT_EQ(mind.developmentPhase, 0u);
		EXPECT_EQ(mind.desiresPhase, std::optional<uint32_t>(0u));
		ASSERT_TRUE(mind.desires.has_value());
		EXPECT_TRUE(std::ranges::none_of(mind.desires->desires, [](const auto& desire) { return desire.activated; }));
		EXPECT_TRUE(std::ranges::all_of(mind.desires->desires, [](const auto& desire) { return desire.suppressedTurns == 0; }));
	}

	[[nodiscard]] bool Logged(const std::string& text) const
	{
		const auto lines = _lines->last_formatted();
		return std::ranges::any_of(lines, [&text](const std::string& line) { return line.find(text) != std::string::npos; });
	}

	// first, so that they go last
	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	const test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	const test::RestoreService<Locator::leashSystem> _restoreLeash;
	test::creature_block::TempFolder _folder;
	std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> _lines;
	std::shared_ptr<spdlog::logger> _log;

public:
	RecordingLeash* leash {nullptr};
};
} // namespace

TEST_F(ScriptLoadCreatureTest, TheScriptsSpeciesIsMadeAtThePointOnTheGroundThenLedAndGrownUp)
{
	// Khazar's: a Giant Ape's file (row 0) loaded as a tortoise (type 7)
	WriteMind("KhazarCreature", 0);
	const auto creature =
	    player_creature::ScriptLoadCreature(7, "KhazarCreature", PlayerNames::PLAYER_TWO, glm::vec2(2540.93f, 1916.63f));
	ASSERT_TRUE(creature != entt::null);
	const auto& registry = std::as_const(Reg());
	const auto& made = registry.Get<const Creature>(creature);
	EXPECT_EQ(made.species, CreatureType::Tortoise);
	EXPECT_EQ(made.owner, PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(made.alignment, 0.25f);
	EXPECT_FLOAT_EQ(made.strength, 0.75f);
	EXPECT_FLOAT_EQ(made.size, 2.0f);
	const auto& transform = registry.Get<const components::Transform>(creature);
	EXPECT_EQ(transform.position.x, map_coords::ToMetres(map_coords::ToFixed(2540.93f)));
	EXPECT_EQ(transform.position.z, map_coords::ToMetres(map_coords::ToFixed(1916.63f)));
	EXPECT_EQ(transform.position.y, test::WaterCellIsland::k_HeightMarker);
	// settled once made: the one its player leads, knowing the three leashes, at the last stage
	ASSERT_EQ(leash->leashable.size(), 1u);
	EXPECT_EQ(leash->leashable[0], std::make_pair(creature, true));
	ASSERT_EQ(leash->known.size(), 3u);
	EXPECT_EQ(leash->known[0], std::make_pair(creature, LeashType::Rope));
	EXPECT_EQ(leash->known[1], std::make_pair(creature, LeashType::Evil));
	EXPECT_EQ(leash->known[2], std::make_pair(creature, LeashType::Good));
	EXPECT_EQ(registry.Get<const CreatureMindState>(creature).developmentPhase, 13u);
	EXPECT_FALSE(Logged("Player has already his creature loaded"));
}

TEST_F(ScriptLoadCreatureTest, APlayerWhoLeadsACreatureIsToldSoAndTheNewOneIsLedInstead)
{
	WriteMind("LethysCreature", 0);
	const auto old = test::creature_world::World::MakeCreature();
	leash->playersCreature = old;
	const auto creature =
	    player_creature::ScriptLoadCreature(4, "LethysCreature", PlayerNames::PLAYER_ONE, glm::vec2(1000.0f, 1000.0f));
	EXPECT_TRUE(Logged("LOAD_CREATURE: Player has already his creature loaded, but I overwrite it"));
	ASSERT_TRUE(creature != entt::null);
	EXPECT_NE(creature, old);
	EXPECT_EQ(std::as_const(Reg()).Get<const Creature>(creature).species, CreatureType::Wolf);
	ASSERT_EQ(leash->leashable.size(), 1u);
	EXPECT_EQ(leash->leashable[0], std::make_pair(creature, true));
	EXPECT_EQ(Creatures(), 2u);
}

TEST_F(ScriptLoadCreatureTest, ACreatureMadeFromAMindFileFacesAngleZero)
{
	// the original's new creature body starts at heading 0 and the mind file gives none
	WriteMind("KhazarCreature", 0);
	const auto creature =
	    player_creature::ScriptLoadCreature(7, "KhazarCreature", PlayerNames::PLAYER_TWO, glm::vec2(2540.93f, 1916.63f));
	ASSERT_TRUE(creature != entt::null);
	EXPECT_EQ(archetypes::CreatureArchetype::k_MadeYAngle, 0.0f);
	EXPECT_EQ(std::as_const(Reg()).Get<const components::Transform>(creature).rotation, glm::mat3(1.0f));
}

TEST_F(ScriptLoadCreatureTest, ACreatureMadeWhereItsPlayerHasNoTempleIsHousedWhereItIsMade)
{
	WriteMind("KhazarCreature", 0);
	const auto creature =
	    player_creature::ScriptLoadCreature(7, "KhazarCreature", PlayerNames::PLAYER_TWO, glm::vec2(2540.93f, 1916.63f));
	ASSERT_TRUE(creature != entt::null);
	const auto& made = std::as_const(Reg()).Get<const components::Transform>(creature).position;
	ASSERT_EQ(leash->homes.size(), 1u);
	EXPECT_EQ(leash->homes[0].first, creature);
	EXPECT_EQ(leash->homes[0].second, player_creature::HomeWhereMade(made, test::WaterCellIsland::k_HeightMarker));
}

TEST_F(ScriptLoadCreatureTest, NoFileNoNameOrATypeOutOfTheTablesMakesNoCreature)
{
	WriteMind("KhazarCreature", 0);
	EXPECT_TRUE(player_creature::ScriptLoadCreature(7, "NobodysCreature", PlayerNames::PLAYER_TWO, glm::vec2(100.0f)) ==
	            entt::null);
	EXPECT_TRUE(player_creature::ScriptLoadCreature(7, "", PlayerNames::PLAYER_TWO, glm::vec2(100.0f)) == entt::null);
	EXPECT_TRUE(player_creature::ScriptLoadCreature(17, "KhazarCreature", PlayerNames::PLAYER_TWO, glm::vec2(100.0f)) ==
	            entt::null);
	EXPECT_EQ(Creatures(), 0u);
	EXPECT_TRUE(leash->leashable.empty());
	EXPECT_TRUE(leash->known.empty());
}

TEST_F(ScriptLoadCreatureTest, TheFileStepMakesTheSameCreatureForEveryCaller)
{
	// CREATE_CREATURE_FROM_FILE's and a computer god's creature: the step LOAD_CREATURE ends with, and no message
	WriteMind("ComputerControlledCreature", 0);
	const auto creature = player_creature::MakeFromMindFile(4, "ComputerControlledCreature", PlayerNames::PLAYER_THREE,
	                                                        glm::vec2(2002.70f, 2960.62f));
	ASSERT_TRUE(creature != entt::null);
	const auto& registry = std::as_const(Reg());
	const auto& made = registry.Get<const Creature>(creature);
	EXPECT_EQ(made.species, CreatureType::Wolf);
	EXPECT_EQ(made.owner, PlayerNames::PLAYER_THREE);
	EXPECT_FLOAT_EQ(made.alignment, 0.25f);
	EXPECT_FLOAT_EQ(made.size, 2.0f);
	ASSERT_EQ(leash->leashable.size(), 1u);
	EXPECT_EQ(leash->leashable[0], std::make_pair(creature, true));
	EXPECT_EQ(leash->known.size(), 3u);
	EXPECT_EQ(registry.Get<const CreatureMindState>(creature).developmentPhase, 13u);
	EXPECT_FALSE(Logged("Player has"));
}

TEST(PlayerCreature, EveryOneOfTheEightPlayersCanHaveACreatureLoadedWhetherTheLandUsesThemOrNot)
{
	for (uint8_t i = 0; i < static_cast<uint8_t>(PlayerNames::_COUNT); ++i)
	{
		EXPECT_TRUE(player_creature::LoadsCreaturesFor(static_cast<PlayerNames>(i))) << static_cast<int>(i);
	}
	EXPECT_FALSE(player_creature::LoadsCreaturesFor(PlayerNames::_COUNT));
}

TEST_F(ScriptLoadCreatureTest, APlayerTheLandHasNotMadeGetsTheCreatureAllTheSame)
{
	WriteMind("NemesisCreature", 0);
	const auto creature =
	    player_creature::ScriptLoadCreature(5, "NemesisCreature", PlayerNames::PLAYER_THREE, glm::vec2(100.0f));
	ASSERT_TRUE(creature != entt::null);
	EXPECT_FALSE(Logged("Player has"));
	EXPECT_EQ(Creatures(), 1u);
	EXPECT_EQ(std::as_const(Reg()).Get<const Creature>(creature).owner, PlayerNames::PLAYER_THREE);
	ASSERT_EQ(leash->leashable.size(), 1u);
	EXPECT_EQ(leash->leashable[0], std::make_pair(creature, true));
}

TEST_F(ScriptLoadCreatureTest, AScriptsCreateMakesANewCreatureOfNobodyOutOfReactions)
{
	// the glade's cow, by (2233, 3152), on the land as the create switch gives it
	const glm::vec3 ground(2233.0f, test::WaterCellIsland::k_HeightMarker, 3152.0f);
	const auto creature = player_creature::MakeForScript(CreatureType::Cow, ground);
	ASSERT_TRUE(creature != entt::null);
	const auto& registry = std::as_const(Reg());
	const auto& made = registry.Get<const Creature>(creature);
	EXPECT_EQ(made.species, CreatureType::Cow);
	EXPECT_EQ(made.owner, PlayerNames::_COUNT);
	EXPECT_FALSE(made.leashable);
	EXPECT_TRUE(registry.AllOf<components::CreatureReactionsOff>(creature));
	EXPECT_FLOAT_EQ(made.size, ecs::archetypes::CreatureArchetype::StartScale(CreatureType::Cow));
	EXPECT_EQ(registry.Get<const components::Transform>(creature).position, ground);
	// no leash is given and nobody is told it leads it
	EXPECT_TRUE(leash->leashable.empty());
	EXPECT_TRUE(leash->known.empty());
	// its home is where it is made
	ASSERT_EQ(leash->homes.size(), 1u);
	EXPECT_EQ(leash->homes[0].first, creature);
	EXPECT_EQ(leash->homes[0].second, ground);
	// every other creature takes up reactions
	EXPECT_FALSE(registry.AllOf<components::CreatureReactionsOff>(test::creature_world::World::MakeCreature()));
	// it is at the first stage of growing up, with every desire off and no stage's desires brought: the glade's
	// creatures stay so until one is chosen and its growing up starts
	ExpectAtFirstStageWithEveryDesireOff(creature);
}

TEST(PlayerCreature, TheLandScriptsNumberTheGamesPlayersAndMakeACreatureOnlyForOneWithout)
{
	EXPECT_EQ(player_creature::PlayerOfNumber(0), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(player_creature::PlayerOfNumber(7), PlayerNames::NEUTRAL);
	for (const int32_t number : {8, 9, -1, 1000})
	{
		EXPECT_EQ(player_creature::PlayerOfNumber(number), PlayerNames::_COUNT) << number;
	}
	EXPECT_TRUE(player_creature::LandScriptMakesFor(PlayerNames::PLAYER_TWO, false));
	EXPECT_FALSE(player_creature::LandScriptMakesFor(PlayerNames::PLAYER_TWO, true));
	EXPECT_FALSE(player_creature::LandScriptMakesFor(PlayerNames::NEUTRAL, true));
	EXPECT_TRUE(player_creature::LandScriptMakesFor(PlayerNames::_COUNT, true));
}

TEST(PlayerCreature, ACreatureOfNoPlayerIsHousedExactlyWhereItIsMadeOnTheGround)
{
	// a point on the map's fixed point that a second round trip through it would move by one unit
	const float x = map_coords::ToMetres(8090858);
	ASSERT_NE(map_coords::ToFixed(x), 8090858);
	const glm::vec3 madeAt {x, 40.0f, 2000.0f};
	const auto home = player_creature::HomeWhereMade(madeAt, 29.5f);
	EXPECT_EQ(home, glm::vec3(x, 29.5f, 2000.0f));
	// not rounded again, as a script's home point is
	EXPECT_NE(home.x, player_creature::HomeOnGround(madeAt, 29.5f).x);
}

TEST(PlayerCreature, TheMapScriptsPointIsItsCellsMiddle)
{
	EXPECT_EQ(player_creature::CellMiddle(0, 0), glm::vec2(5.0f, 5.0f));
	EXPECT_EQ(player_creature::CellMiddle(59, 143), glm::vec2(595.0f, 1435.0f));
	// the cell numbers are taken as 16-bit
	EXPECT_EQ(player_creature::CellMiddle(0x10000 + 3, 2), player_creature::CellMiddle(3, 2));
}

TEST_F(ScriptLoadCreatureTest, TheLandScriptsCreateCreatureMakesANewCreatureOfTheTypeForThePlayer)
{
	// comp.txt's: a cow (type 1) for the second player
	const auto creature = player_creature::MakeForLandScript(1, glm::vec2(596.74f, 1435.19f), PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(creature != entt::null);
	const auto& registry = std::as_const(Reg());
	const auto& made = registry.Get<const Creature>(creature);
	EXPECT_EQ(made.species, CreatureType::Cow);
	EXPECT_EQ(made.owner, PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(registry.AllOf<components::CreatureReactionsOff>(creature));
	EXPECT_FLOAT_EQ(made.size, ecs::archetypes::CreatureArchetype::StartScale(CreatureType::Cow));
	const auto& transform = registry.Get<const components::Transform>(creature);
	EXPECT_EQ(transform.position, glm::vec3(596.74f, test::WaterCellIsland::k_HeightMarker, 1435.19f));
	// no leash is given
	EXPECT_TRUE(leash->known.empty());
	// a number past the players: nobody's
	const auto nobodys = player_creature::MakeForLandScript(0, glm::vec2(600.0f), player_creature::PlayerOfNumber(9));
	ASSERT_TRUE(nobodys != entt::null);
	EXPECT_EQ(registry.Get<const Creature>(nobodys).owner, PlayerNames::_COUNT);
	EXPECT_EQ(registry.Get<const Creature>(nobodys).species, CreatureType::GiantApe);
	EXPECT_TRUE(player_creature::MakeForLandScript(17, glm::vec2(600.0f), PlayerNames::PLAYER_TWO) == entt::null);
	EXPECT_EQ(Creatures(), 2u);
	// both are moved to no stage as they are made
	ExpectAtFirstStageWithEveryDesireOff(creature);
	ExpectAtFirstStageWithEveryDesireOff(nobodys);
}

TEST(PlayerCreature, TheLandTwoAndFiveMindFilesAreOneAndRead)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		game = std::getenv("OPENBLACK_TEST_GAME_PATH");
	}
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	if (!spdlog::get("game"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::ScopedDefaultFileSystem fileSystem;
	Locator::filesystem::value().SetGamePath(game);
	const auto folder = Locator::filesystem::value().GetPath<filesystem::Path::CreatureMind>(true);
	const auto bytesOf = [&folder](const char* file) {
		std::ifstream in(folder / file, std::ios::binary);
		return std::vector<char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	};
	const auto khazar = bytesOf("KhazarCreature");
	if (khazar.empty())
	{
		GTEST_SKIP() << "no KhazarCreature in the game folder";
	}
	EXPECT_EQ(bytesOf("LethysCreature"), khazar);
	EXPECT_EQ(bytesOf("NemesisCreature"), khazar);
	const auto mind =
	    resources::CreatureMindLoader {}(resources::CreatureMindLoader::FromDiskTag {}, folder / "KhazarCreature");
	ASSERT_TRUE(mind->Loaded());
	EXPECT_EQ(mind->data.version, 25u);
	EXPECT_EQ(mind->data.speciesRow, 0u);
	const auto plan = player_creature::PlanScriptLoad(7, mind.get(), glm::vec2(2540.93f, 1916.63f));
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->species, CreatureType::Tortoise);
}

namespace
{
/// A temple mesh's special points, the pen point at its place and the others elsewhere
std::vector<glm::mat4> SpecialPoints(glm::vec3 pen)
{
	std::vector<glm::mat4> points(player_creature::k_TemplePenPoint + 1, glm::mat4(1.0f));
	points.back()[3] = glm::vec4(pen, 1.0f);
	return points;
}
} // namespace

TEST(PlayerCreature, ATemplesPenPointTurnsAndMovesWithIt)
{
	const components::Transform temple {
	    .position = glm::vec3(100.0f, 5.0f, 200.0f),
	    .rotation = glm::mat3(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f),
	    .scale = glm::vec3(2.0f),
	};
	const auto pen = player_creature::TemplePenPoint(temple, SpecialPoints(glm::vec3(1.0f, 0.0f, 3.0f)));
	ASSERT_TRUE(pen.has_value());
	EXPECT_EQ(*pen, temple.position + temple.rotation * glm::vec3(2.0f, 0.0f, 6.0f));
}

TEST(PlayerCreature, AMeshWithoutThePenPointKeepsNoCreature)
{
	const components::Transform temple {.position = glm::vec3(0.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
	auto points = SpecialPoints(glm::vec3(1.0f));
	points.pop_back();
	EXPECT_FALSE(player_creature::TemplePenPoint(temple, points).has_value());
	EXPECT_FALSE(player_creature::TemplePenPoint(temple, {}).has_value());
}

TEST(PlayerCreature, ANewCreatureIsHousedAtItsTemplesFirstPointTurnedAndMovedWithIt)
{
	EXPECT_EQ(player_creature::k_TempleMadeHomePoint, 0u);
	const components::Transform temple {
	    .position = glm::vec3(100.0f, 5.0f, 200.0f),
	    .rotation = glm::mat3(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f),
	    .scale = glm::vec3(2.0f),
	};
	auto points = SpecialPoints(glm::vec3(9.0f));
	points.front()[3] = glm::vec4(1.0f, 0.0f, 3.0f, 1.0f);
	const auto first = player_creature::TemplePoint(temple, points, player_creature::k_TempleMadeHomePoint);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(*first, temple.position + temple.rotation * glm::vec3(2.0f, 0.0f, 6.0f));
	EXPECT_EQ(player_creature::TemplePoint(temple, points, player_creature::k_TemplePenPoint),
	          player_creature::TemplePenPoint(temple, points));
	EXPECT_FALSE(player_creature::TemplePoint(temple, {}, player_creature::k_TempleMadeHomePoint).has_value());
}

TEST(PlayerCreature, ANewCreaturesHomeIsItsTemplesFirstPointElseWhereItIsMadeOnTheGround)
{
	std::vector<glm::vec2> asked;
	const auto groundAt = [&asked](glm::vec2 point) {
		asked.push_back(point);
		return 29.5f;
	};
	// a point the creation has put on the fixed point already, which a second round trip through it would move
	const glm::vec3 madeAt {map_coords::ToMetres(8090858), 40.0f, map_coords::ToMetres(13107200)};
	const auto alone = player_creature::MadeHome(madeAt, std::nullopt, groundAt);
	EXPECT_EQ(alone, player_creature::HomeWhereMade(madeAt, 29.5f));
	EXPECT_EQ(alone.x, madeAt.x);
	const glm::vec3 temple {1895.97f, 12.0f, 2520.75f};
	const auto byTemple = player_creature::MadeHome(madeAt, temple, groundAt);
	EXPECT_EQ(byTemple, player_creature::HomeOnGround(temple, 29.5f));
	// the ground is the one at the home's own point, through the fixed point
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[0], glm::vec2(alone.x, alone.z));
	EXPECT_EQ(asked[1], glm::vec2(byTemple.x, byTemple.z));
}

TEST_F(PlayerCreatureNativesTest, TheHomeFollowsTheTemplesPenOnTheGround)
{
	const auto penOf = [](PlayerNames owner) {
		return owner == PlayerNames::PLAYER_ONE ? std::optional(glm::vec3(1895.97f, 40.0f, 2520.75f)) : std::nullopt;
	};
	player_creature::FollowTemplePens(leash, Reg(), penOf, [](glm::vec2) { return 29.5f; });
	ASSERT_EQ(leash.homes.size(), 1u);
	EXPECT_EQ(leash.homes[0].first, creature);
	EXPECT_EQ(leash.homes[0].second, player_creature::HomeOnGround(glm::vec3(1895.97f, 0.0f, 2520.75f), 29.5f));
}

TEST_F(PlayerCreatureNativesTest, WithoutATempleTheHomeIsLeftAsItWas)
{
	player_creature::FollowTemplePens(
	    leash, Reg(), [](PlayerNames) { return std::optional<glm::vec3> {}; }, [](glm::vec2) { return 0.0f; });
	EXPECT_TRUE(leash.homes.empty());
}

TEST(PlayerCreature, WithoutACreatureNoTempleIsLookedAt)
{
	const ecs::Registry registry;
	RecordingLeash leash;
	bool asked = false;
	player_creature::FollowTemplePens(
	    leash, registry,
	    [&asked](PlayerNames) {
		    asked = true;
		    return std::optional(glm::vec3(0.0f));
	    },
	    [](glm::vec2) { return 0.0f; });
	EXPECT_FALSE(asked);
	EXPECT_TRUE(leash.homes.empty());
}

TEST(PlayerCreature, Land1sHomeIsBetweenThePensWalls)
{
	// the temple of Land 1 and the script's home for the creature
	const glm::vec2 temple(1915.05f, 2508.89f);
	EXPECT_TRUE(player_creature::BetweenPenWalls(temple, 36.0f, glm::vec2(1896.5f, 2520.06f)));
	EXPECT_TRUE(player_creature::BetweenPenWalls(temple, 36.0f, glm::vec2(1895.97f, 2520.75f)));
	// the temple's other side
	EXPECT_FALSE(player_creature::BetweenPenWalls(temple, 36.0f, glm::vec2(1934.0f, 2497.0f)));
}

TEST(PlayerCreature, InThePenACreatureIsDrawnDownToANewbornsSize)
{
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 0.87f, true), 0.22f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 14.0f, true), 0.22f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 15.0f, true), 0.22f + (2.0f - 0.22f) * 0.5f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 16.0f, true), 2.0f);
}

TEST(PlayerCreature, OutsideThePenACreatureIsDrawnAtItsOwnSize)
{
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 16.5f, true), 2.0f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, 1.0f, false), 2.0f);
	EXPECT_EQ(player_creature::PenDrawnSize(2.0f, std::numeric_limits<float>::quiet_NaN(), true), 2.0f);
}

TEST(PlayerCreature, AFriendIsAddedOnceAtTheFront)
{
	const auto a = static_cast<entt::entity>(1);
	const auto b = static_cast<entt::entity>(2);
	std::vector<entt::entity> friends;
	EXPECT_TRUE(player_creature::AddFriend(friends, a));
	EXPECT_TRUE(player_creature::AddFriend(friends, b));
	EXPECT_FALSE(player_creature::AddFriend(friends, a));
	EXPECT_EQ(friends, (std::vector<entt::entity> {b, a}));
}

TEST_F(PlayerCreatureNativesTest, ForceFriendsMakesTwoCreaturesEachOthersFriends)
{
	const auto rival = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f), PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(player_creature::ForceFriends(Reg(), creature, rival, true));
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFriends>(creature).friends,
	          (std::vector<entt::entity> {rival}));
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFriends>(rival).friends,
	          (std::vector<entt::entity> {creature}));
	// again, the other way round: each is in the other's list once
	EXPECT_TRUE(player_creature::ForceFriends(Reg(), rival, creature, true));
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFriends>(creature).friends.size(), 1u);
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFriends>(rival).friends.size(), 1u);
}

TEST_F(PlayerCreatureNativesTest, ForceFriendsPutsTheNewestFriendFirst)
{
	const auto rival = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f), PlayerNames::PLAYER_TWO);
	const auto third = test::creature_world::World::MakeCreature(glm::vec3(300.0f, 0.0f, 300.0f), PlayerNames::PLAYER_THREE);
	ASSERT_TRUE(player_creature::ForceFriends(Reg(), creature, rival, true));
	ASSERT_TRUE(player_creature::ForceFriends(Reg(), creature, third, true));
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFriends>(creature).friends,
	          (std::vector<entt::entity> {third, rival}));
}

TEST_F(PlayerCreatureNativesTest, ACreatureMadeItsOwnFriendIsInItsListOnce)
{
	EXPECT_TRUE(player_creature::ForceFriends(Reg(), creature, creature, true));
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFriends>(creature).friends,
	          (std::vector<entt::entity> {creature}));
}

TEST_F(PlayerCreatureNativesTest, ForceFriendsOffTakesNoFriendAway)
{
	const auto rival = test::creature_world::World::MakeCreature(glm::vec3(200.0f, 0.0f, 200.0f), PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(player_creature::ForceFriends(Reg(), creature, rival, false));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<components::CreatureFriends>(creature));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<components::CreatureFriends>(rival));
	ASSERT_TRUE(player_creature::ForceFriends(Reg(), creature, rival, true));
	EXPECT_TRUE(player_creature::ForceFriends(Reg(), creature, rival, false));
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFriends>(creature).friends,
	          (std::vector<entt::entity> {rival}));
}

TEST_F(PlayerCreatureNativesTest, ForceFriendsNeedsTwoCreatures)
{
	EXPECT_FALSE(player_creature::ForceFriends(Reg(), creature, other, true));
	EXPECT_FALSE(player_creature::ForceFriends(Reg(), other, creature, true));
	EXPECT_FALSE(player_creature::ForceFriends(Reg(), creature, entt::null, true));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<components::CreatureFriends>(creature));
	EXPECT_FALSE(std::as_const(Reg()).AllOf<components::CreatureFriends>(other));
}

namespace
{
using openblack::script::ObjectPropertyType;

/// The ten creature properties, strength, alignment and warmth to fight health, in the enum's order. The Land 2
/// creatures' set-up scripts set all but the alignment to 0.2, and the alignment later from the player's creature's
constexpr std::array k_CreatureProperties {
    ObjectPropertyType::Strength,
    ObjectPropertyType::Alignment,
    ObjectPropertyType::CreatureWarmth,
    ObjectPropertyType::CreatureFatness,
    ObjectPropertyType::CreatureEnergy,
    ObjectPropertyType::CreatureItchiness,
    ObjectPropertyType::CreatureAmountOfPoo,
    ObjectPropertyType::CreatureExhaustion,
    ObjectPropertyType::CreatureDehydration,
    ObjectPropertyType::CreatureFightHealth,
    ObjectPropertyType::CreatureMinSize,
    ObjectPropertyType::CreatureMaxSize,
};

class CreaturePropertyTest: public PlayerCreatureNativesTest
{
protected:
	CreaturePropertyTest() { bodies.needs[creature] = creature_physiology::Needs {}; }

	[[nodiscard]] std::optional<float> Get(entt::entity thing, ObjectPropertyType property) const
	{
		return player_creature::GetCreatureProperty(std::as_const(Reg()), &bodies, thing, property);
	}
	bool Set(entt::entity thing, ObjectPropertyType property, float value)
	{
		return player_creature::SetCreatureProperty(Reg(), &bodies, thing, property, value);
	}

	test::creature_loop_fakes::CallLog log;
	test::creature_loop_fakes::FakePhysiology bodies {log};
};
} // namespace

TEST(PlayerCreature, APropertyIsClampedInTheOriginalsOrder)
{
	EXPECT_EQ(player_creature::ClampProperty(-2.0f, -1.0f, 1.0f), -1.0f);
	EXPECT_EQ(player_creature::ClampProperty(2.0f, -1.0f, 1.0f), 1.0f);
	EXPECT_EQ(player_creature::ClampProperty(0.3f, -1.0f, 1.0f), 0.3f);
	EXPECT_EQ(player_creature::ClampProperty(-1.0f, -1.0f, 1.0f), -1.0f);
	EXPECT_EQ(player_creature::ClampProperty(1.0f, 0.0f, 1.0f), 1.0f);
	// not a number is below the lowest
	EXPECT_EQ(player_creature::ClampProperty(std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f), 0.0f);
	EXPECT_EQ(player_creature::ClampProperty(std::numeric_limits<float>::quiet_NaN(), -1.0f, 1.0f), -1.0f);
}

TEST(PlayerCreature, TheCreaturePropertiesAreStrengthAlignmentAndTheBodys)
{
	for (const auto property : k_CreatureProperties)
	{
		EXPECT_TRUE(player_creature::IsCreatureProperty(property));
	}
	// the age is the body's too
	EXPECT_TRUE(player_creature::IsCreatureProperty(ObjectPropertyType::Age));
	EXPECT_FALSE(player_creature::IsCreatureProperty(ObjectPropertyType::Height));
	EXPECT_FALSE(player_creature::IsCreatureProperty(ObjectPropertyType::ZPos));
	EXPECT_FALSE(player_creature::IsCreatureProperty(ObjectPropertyType::CreatureCanGoThroughVortex));
}

TEST_F(CreaturePropertyTest, EachPropertyIsSetToTheValueGiven)
{
	for (const auto property : k_CreatureProperties)
	{
		EXPECT_TRUE(Set(creature, property, 0.2f)) << static_cast<int>(property);
		EXPECT_EQ(Get(creature, property), 0.2f) << static_cast<int>(property);
	}
	const auto& body = std::as_const(Reg()).Get<const Creature>(creature);
	EXPECT_EQ(body.strength, 0.2f);
	EXPECT_EQ(body.alignment, 0.2f);
	EXPECT_EQ(body.fatness, 0.2f);
	const auto& needs = bodies.needs.at(creature);
	EXPECT_EQ(needs.warmth, 0.2f);
	EXPECT_EQ(needs.energy, 0.2f);
	EXPECT_EQ(needs.itchiness, 0.2f);
	EXPECT_EQ(needs.poo, 0.2f);
	EXPECT_EQ(needs.exhaustion, 0.2f);
	EXPECT_EQ(needs.dehydration, 0.2f);
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFightHealth>(creature).health, 0.2f);
}

TEST_F(CreaturePropertyTest, ANewAlignmentStepsTheShownFatnessAndRedrawsTheBodyAtOnce)
{
	auto& body = Reg().Get<Creature>(creature);
	auto& morph = Reg().Get<components::CreatureMorph>(creature);
	body.fatness = 0.9f;
	morph.shownFatness = 0.5f;
	const auto drawnBefore = morph.drawn;
	const auto revisionBefore = morph.revision;
	ASSERT_EQ(drawnBefore.evilGood, 0.0f);

	// far enough on the evil to good axis: the fatness shown takes one step of 0.01, and all three axes take their
	// targets, the animations with them
	ASSERT_TRUE(Set(creature, ObjectPropertyType::Alignment, 0.8f));
	EXPECT_EQ(morph.shownFatness, 0.5f + 0.01f);
	EXPECT_EQ(morph.drawn.evilGood, 0.8f);
	EXPECT_EQ(morph.drawn.thinFat, (2.0f * (0.5f + 0.01f)) - 1.0f);
	EXPECT_EQ(morph.drawn.weakStrong, creature_pose::TargetMorph(std::as_const(Reg()), creature).weakStrong);
	EXPECT_EQ(morph.revision, revisionBefore + 1);

	// less than 0.03 from what is drawn, on every axis: the fatness shown steps again, and the shape stays as drawn
	const auto drawnThen = morph.drawn;
	ASSERT_TRUE(Set(creature, ObjectPropertyType::Alignment, 0.81f));
	EXPECT_EQ(morph.shownFatness, (0.5f + 0.01f) + 0.01f);
	EXPECT_EQ(morph.drawn.evilGood, drawnThen.evilGood);
	EXPECT_EQ(morph.drawn.thinFat, drawnThen.thinFat);
	EXPECT_EQ(morph.drawn.weakStrong, drawnThen.weakStrong);
	EXPECT_EQ(morph.revision, revisionBefore + 1);
	EXPECT_EQ(body.alignment, 0.81f);
}

TEST_F(CreaturePropertyTest, ANewStrengthOrFatnessWaitsForTheBodysTurn)
{
	auto& morph = Reg().Get<components::CreatureMorph>(creature);
	morph.shownFatness = 0.5f;
	const auto drawnBefore = morph.drawn;
	const auto revisionBefore = morph.revision;
	ASSERT_TRUE(Set(creature, ObjectPropertyType::Strength, 1.0f));
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureFatness, 1.0f));
	EXPECT_EQ(morph.shownFatness, 0.5f);
	EXPECT_EQ(morph.drawn.evilGood, drawnBefore.evilGood);
	EXPECT_EQ(morph.drawn.thinFat, drawnBefore.thinFat);
	EXPECT_EQ(morph.drawn.weakStrong, drawnBefore.weakStrong);
	EXPECT_EQ(morph.revision, revisionBefore);
}

TEST_F(CreaturePropertyTest, ItsAgeIsItsBodysWholeNumber)
{
	bodies.needs.at(creature).age = 7;
	EXPECT_EQ(Get(creature, ObjectPropertyType::Age), 7.0f);
	// cut towards 0, as the game turns a float into a whole number
	ASSERT_TRUE(Set(creature, ObjectPropertyType::Age, 16.7f));
	EXPECT_EQ(bodies.needs.at(creature).age, 16u);
	EXPECT_EQ(Get(creature, ObjectPropertyType::Age), 16.0f);
	ASSERT_TRUE(Set(creature, ObjectPropertyType::Age, -0.5f));
	EXPECT_EQ(bodies.needs.at(creature).age, 0u);
	// anything else has none
	EXPECT_FALSE(Get(other, ObjectPropertyType::Age).has_value());
	EXPECT_FALSE(Set(other, ObjectPropertyType::Age, 3.0f));
}

TEST_F(CreaturePropertyTest, ItsSmallestAndLargestSizeAreTheDefaultsUntilAScriptSetsThem)
{
	const auto& lookup = std::as_const(Reg());
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMinSize), 0.2f);
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMaxSize), 2.4f);
	// reading them gives the creature nothing of its own
	EXPECT_FALSE(lookup.AllOf<components::CreatureSizeLimits>(creature));
	// set as given, the other one kept
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureMaxSize, 5.0f));
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMaxSize), 5.0f);
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMinSize), 0.2f);
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureMinSize, -1.0f));
	EXPECT_EQ(lookup.Get<const components::CreatureSizeLimits>(creature).limits.smallest, -1.0f);
	EXPECT_EQ(lookup.Get<const components::CreatureSizeLimits>(creature).limits.largest, 5.0f);
	// they are no need of its body
	EXPECT_EQ(bodies.needsSet, 0);
}

TEST_F(CreaturePropertyTest, WithAPartnerItsSmallestAndLargestSizeGoByThePartnersSize)
{
	const auto partner = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	// both drawn at the size they were made with
	ASSERT_EQ(std::as_const(Reg()).Get<const components::CreatureDrawPose>(creature).size, 1.0f);
	ASSERT_EQ(std::as_const(Reg()).Get<const components::CreatureDrawPose>(partner).size, 1.0f);
	Reg().AssignOrReplaceState<components::CreaturePartner>(creature, components::CreaturePartner {.partner = partner});
	// big aims for 1.8 times the partner's drawn size, small no higher than its smallest
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMaxSize), 1.0f * creature_spells::k_BigPartnerFactor);
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMinSize), 0.2f);
	// the partner's drawn size is held to the sizes a body may take
	Reg().Get<components::CreatureDrawPose>(partner).size = 10.0f;
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMaxSize), 2.4f);
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureMaxSize, 9.0f));
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMaxSize), 4.0f * creature_spells::k_BigPartnerFactor);
	// the script's own limit is kept as given
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureSizeLimits>(creature).limits.largest, 9.0f);
	// a partner that is no creature counts as none
	Reg().Get<components::CreaturePartner>(creature).partner = other;
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureMaxSize), 9.0f);
}

TEST(PlayerCreature, AHeightsSizeIsTheHeightTimesAFifteenth)
{
	EXPECT_EQ(player_creature::SizeForHeight(15.0f), 1.0f);
	EXPECT_EQ(player_creature::SizeForHeight(30.0f), 2.0f);
	// multiplied by the float one fifteenth, as the original: 45 gives one step above 3, not 45 / 15
	EXPECT_EQ(player_creature::SizeForHeight(45.0f), std::bit_cast<float>(0x40400001u));
	EXPECT_NE(player_creature::SizeForHeight(45.0f), 45.0f / 15.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(player_creature::k_SizePerHeight), 0x3D888889u);
}

TEST_F(PlayerCreatureNativesTest, AScriptsScaleIsTheCreaturesSizeAndItsDrawnSizeWithinTheBodysLimits)
{
	const auto& lookup = std::as_const(Reg());
	const auto species = lookup.Get<const Creature>(creature).species;
	ASSERT_TRUE(player_creature::SetCreatureScale(Reg(), creature, 2.5f));
	EXPECT_EQ(lookup.Get<const Creature>(creature).size, 2.5f);
	EXPECT_EQ(lookup.Get<const components::Transform>(creature).scale,
	          glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(species, 2.5f)));
	// above 4 the body is drawn at 4, below 0.05 at 0.05; its own size is kept as given
	ASSERT_TRUE(player_creature::SetCreatureScale(Reg(), creature, 5.0f));
	EXPECT_EQ(lookup.Get<const Creature>(creature).size, 5.0f);
	EXPECT_EQ(lookup.Get<const components::Transform>(creature).scale,
	          glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(species, creature_morph::k_MaxScale)));
	ASSERT_TRUE(player_creature::SetCreatureScale(Reg(), creature, 0.01f));
	EXPECT_EQ(lookup.Get<const Creature>(creature).size, 0.01f);
	EXPECT_EQ(lookup.Get<const components::Transform>(creature).scale,
	          glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(species, creature_morph::k_MinScale)));
	// anything else is left alone
	EXPECT_FALSE(player_creature::SetCreatureScale(Reg(), other, 2.0f));
	EXPECT_EQ(lookup.Get<const components::Transform>(other).scale, glm::vec3(1.0f));
}

TEST_F(PlayerCreatureNativesTest, AScriptsScaleGivesTheBodyItsSizeAtOnce)
{
	const auto& lookup = std::as_const(Reg());
	ASSERT_TRUE(player_creature::SetCreatureScale(Reg(), creature, 5.0f));
	const auto& pose = lookup.Get<const components::CreatureDrawPose>(creature);
	// drawn at the most, its own size given as given, and drawn as its Transform is
	EXPECT_EQ(pose.size, creature_morph::k_MaxScale);
	EXPECT_EQ(pose.given, 5.0f);
	EXPECT_EQ(pose.scale, lookup.Get<const components::Transform>(creature).scale);
	EXPECT_FALSE(pose.inPen);
}

TEST_F(PlayerCreatureNativesTest, EachTurnABodyOutOfAPenIsGivenItsOwnSizeKeptBetweenItsLimits)
{
	const auto& lookup = std::as_const(Reg());
	auto& metrics = Reg().Get<components::CreatureBodyMetrics>(creature);
	metrics.restHeight = 30.0f;
	// a size spell's 2.4 is drawn as it is, though the creature grows no bigger than 2 on its own
	Reg().Get<Creature>(creature).size = 2.4f;
	player_creature::ShrinkInPens();
	const auto& pose = lookup.Get<const components::CreatureDrawPose>(creature);
	EXPECT_EQ(pose.size, 2.4f);
	EXPECT_EQ(pose.given, 2.4f);
	EXPECT_EQ(pose.scale, glm::vec3(creature_morph::DrawnScale(2.4f, 30.0f)));
	EXPECT_FALSE(pose.inPen);
	EXPECT_EQ(ecs::creature_pose::DrawnSize(lookup, creature), 2.4f);

	// above the most: drawn at the most, the walk going by the size given
	Reg().Get<Creature>(creature).size = 5.0f;
	player_creature::ShrinkInPens();
	EXPECT_EQ(pose.size, creature_morph::k_MaxScale);
	EXPECT_EQ(ecs::creature_pose::GivenSize(lookup, creature), 5.0f);
	EXPECT_EQ(pose.scale, glm::vec3(creature_morph::DrawnScale(creature_morph::k_MaxScale, 30.0f)));

	// a thing that is no creature's body is given nothing
	player_creature::GiveBodySize(Reg(), other, 1.0f, false);
	EXPECT_EQ(lookup.TryGet<const components::CreatureDrawPose>(other), nullptr);
}

TEST_F(PlayerCreatureNativesTest, AScriptsHeightIsTheCreaturesSizeAndLeavesItsDrawnBody)
{
	const auto& lookup = std::as_const(Reg());
	const auto drawn = lookup.Get<const components::Transform>(creature).scale;
	ASSERT_TRUE(player_creature::SetCreatureHeight(Reg(), creature, 90.0f));
	// no limit on the size, and the drawn body as it was
	EXPECT_EQ(lookup.Get<const Creature>(creature).size, player_creature::SizeForHeight(90.0f));
	EXPECT_EQ(lookup.Get<const components::Transform>(creature).scale, drawn);
	EXPECT_FALSE(player_creature::SetCreatureHeight(Reg(), other, 30.0f));
	EXPECT_EQ(lookup.Get<const components::Transform>(other).scale, glm::vec3(1.0f));
}

TEST_F(PlayerCreatureNativesTest, AScriptsHeightIsDrawnAtOnceUnclampedUntilTheBodyIsResized)
{
	const auto& lookup = std::as_const(Reg());
	ASSERT_TRUE(player_creature::SetCreatureScale(Reg(), creature, 1.0f));
	const auto scale = lookup.Get<const components::CreatureDrawPose>(creature).scale;
	// 90 is a size of 6, above the body's most: drawn at it as it is, its matrix as it was
	ASSERT_TRUE(player_creature::SetCreatureHeight(Reg(), creature, 90.0f));
	const auto& pose = lookup.Get<const components::CreatureDrawPose>(creature);
	EXPECT_EQ(pose.size, player_creature::SizeForHeight(90.0f));
	EXPECT_EQ(pose.scale, scale);
	// so the height reads back as it was set
	EXPECT_EQ(player_creature::CreatureHeightProperty(lookup, creature),
	          player_creature::SizeForHeight(90.0f) * player_creature::k_HeightPerSize);
	// the next resize keeps it within the body's limits again
	player_creature::ShrinkInPens();
	EXPECT_EQ(pose.size, creature_morph::k_MaxScale);
}

TEST_F(PlayerCreatureNativesTest, ACreaturesHeightIsFifteenTimesTheSizeItIsDrawnAt)
{
	const auto& lookup = std::as_const(Reg());
	ASSERT_TRUE(player_creature::SetCreatureScale(Reg(), creature, 0.5f));
	EXPECT_EQ(player_creature::CreatureHeightProperty(lookup, creature), 7.5f);
	// the drawn size, not its own: below the least it is drawn at the least
	ASSERT_TRUE(player_creature::SetCreatureScale(Reg(), creature, 0.01f));
	EXPECT_EQ(player_creature::CreatureHeightProperty(lookup, creature), creature_morph::k_MinScale * 15.0f);
	// anything else has no creature's height
	EXPECT_FALSE(player_creature::CreatureHeightProperty(lookup, other).has_value());
}

TEST_F(CreaturePropertyTest, OnlyTheWarmthAndTheEnergyAreClamped)
{
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureWarmth, 5.0f));
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureWarmth), 1.0f);
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureWarmth, -5.0f));
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureWarmth), -1.0f);
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureEnergy, 2.0f));
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureEnergy), 1.0f);
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureEnergy, -0.5f));
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureEnergy), 0.0f);
	for (const auto property : {ObjectPropertyType::Strength, ObjectPropertyType::Alignment,
	                            ObjectPropertyType::CreatureFatness, ObjectPropertyType::CreatureItchiness,
	                            ObjectPropertyType::CreatureAmountOfPoo, ObjectPropertyType::CreatureExhaustion,
	                            ObjectPropertyType::CreatureDehydration, ObjectPropertyType::CreatureFightHealth})
	{
		ASSERT_TRUE(Set(creature, property, 3.0f));
		EXPECT_EQ(Get(creature, property), 3.0f) << static_cast<int>(property);
		ASSERT_TRUE(Set(creature, property, -3.0f));
		EXPECT_EQ(Get(creature, property), -3.0f) << static_cast<int>(property);
	}
}

TEST_F(CreaturePropertyTest, ANeedIsSetThroughTheBodyKeepingTheOthers)
{
	bodies.needs[creature].exhaustion = 0.4f;
	bodies.needs[creature].poo = 0.6f;
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureExhaustion, 1.0f));
	EXPECT_EQ(bodies.needsSet, 1);
	EXPECT_EQ(bodies.needs.at(creature).exhaustion, 1.0f);
	EXPECT_EQ(bodies.needs.at(creature).poo, 0.6f);
	// strength, alignment, fatness and fight health are not the body's needs
	ASSERT_TRUE(Set(creature, ObjectPropertyType::Strength, 0.7f));
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureFightHealth, 0.7f));
	EXPECT_EQ(bodies.needsSet, 1);
}

TEST_F(CreaturePropertyTest, AFightHealthIsFullUntilSetAndTheFightersInAFight)
{
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureFightHealth), 1.0f);
	EXPECT_FALSE(std::as_const(Reg()).AllOf<components::CreatureFightHealth>(creature));
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureFightHealth, 0.2f));
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureFightHealth), 0.2f);

	auto& fighting = Reg().AssignState<components::CreatureFighting>(creature);
	fighting.fighter.health = 0.7f;
	EXPECT_EQ(Get(creature, ObjectPropertyType::CreatureFightHealth), 0.7f);
	ASSERT_TRUE(Set(creature, ObjectPropertyType::CreatureFightHealth, 0.4f));
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFighting>(creature).fighter.health, 0.4f);
	// the one kept outside the fight is left alone
	EXPECT_EQ(std::as_const(Reg()).Get<const components::CreatureFightHealth>(creature).health, 0.2f);
}

TEST_F(CreaturePropertyTest, OnlyACreatureHasCreatureProperties)
{
	bodies.needs[other] = creature_physiology::Needs {};
	for (const auto property : k_CreatureProperties)
	{
		EXPECT_FALSE(Get(other, property).has_value()) << static_cast<int>(property);
		EXPECT_FALSE(Set(other, property, 0.2f)) << static_cast<int>(property);
		EXPECT_FALSE(Get(entt::null, property).has_value()) << static_cast<int>(property);
		EXPECT_FALSE(Set(entt::null, property, 0.2f)) << static_cast<int>(property);
	}
	EXPECT_EQ(bodies.needsSet, 0);
	EXPECT_FALSE(std::as_const(Reg()).AllOf<components::CreatureFightHealth>(other));
	// nor another property on a creature
	EXPECT_FALSE(Get(creature, ObjectPropertyType::Scale).has_value());
	EXPECT_FALSE(Set(creature, ObjectPropertyType::Scale, 2.0f));
}

TEST_F(CreaturePropertyTest, ACreatureWithoutABodyHasNoNeeds)
{
	bodies.needs.clear();
	EXPECT_FALSE(Get(creature, ObjectPropertyType::CreatureEnergy).has_value());
	EXPECT_FALSE(Set(creature, ObjectPropertyType::CreatureEnergy, 0.5f));
	// its strength is its own
	EXPECT_EQ(Get(creature, ObjectPropertyType::Strength), std::as_const(Reg()).Get<const Creature>(creature).strength);
}

TEST_F(CreaturePropertyTest, WithoutTheBodiesOnlyTheNeedsAreMissing)
{
	using enum ObjectPropertyType;
	for (const auto property : {Strength, Alignment, CreatureFatness, CreatureFightHealth})
	{
		EXPECT_TRUE(player_creature::SetCreatureProperty(Reg(), nullptr, creature, property, 0.3f))
		    << static_cast<int>(property);
		EXPECT_EQ(player_creature::GetCreatureProperty(std::as_const(Reg()), nullptr, creature, property), 0.3f)
		    << static_cast<int>(property);
	}
	for (const auto property :
	     {Age, CreatureWarmth, CreatureEnergy, CreatureItchiness, CreatureAmountOfPoo, CreatureExhaustion, CreatureDehydration})
	{
		EXPECT_FALSE(player_creature::SetCreatureProperty(Reg(), nullptr, creature, property, 0.3f))
		    << static_cast<int>(property);
		EXPECT_FALSE(player_creature::GetCreatureProperty(std::as_const(Reg()), nullptr, creature, property).has_value())
		    << static_cast<int>(property);
	}
	EXPECT_EQ(bodies.needsSet, 0);
}
