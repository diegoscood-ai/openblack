/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The artefacts' system over a synthetic town, rock and dance in a registry of the test's own, the system made here,
// not reached through the locator (docs/wiki/beings/towns.md#artefacts): a new artefact's worth, joining a town or a
// worship site, a hand taking it, and a town's turn of its artefacts' dances. The villagers' state table is a fake that
// records the state each dancer is sent to and refuses it, so that nothing else of the villager runs.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ArtefactSystem.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/LivingActionFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using Index = LivingAction::Index;

namespace
{
/// A rock's worth when it becomes an artefact and its artefact multiplier, from the object tables
constexpr float k_RockImpressiveValue = 0.01f;
constexpr float k_RockArtefactMultiplier = 0.0002f;
/// The dance round a thing under 7 m wide
constexpr auto k_SmallDance = DanceInfo::DanceAroundObjectDiameterLess_7;
constexpr uint32_t k_DanceLength = 900;
constexpr uint32_t k_StartTurn = 1000;

/// A state table that records each state a villager is sent to from outside its state logic and refuses it (its old
/// state's exit says no), so that the villager stays as it was
class RecordingStateTable final: public test::LivingActionFakeBase
{
public:
	mutable std::vector<std::pair<const LivingAction*, VillagerStates>> asked;

	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	[[nodiscard]] VillagerStates VillagerGetFinalState(const LivingAction& /*action*/) const override
	{
		return VillagerStates::InvalidState;
	}
	void VillagerSetState(LivingAction& /*action*/, Index /*index*/, VillagerStates /*state*/,
	                      bool /*skipTransition*/) const override
	{
	}
	uint32_t VillagerCallState(LivingAction& /*action*/, Index /*index*/) const override { return 0; }
	uint32_t VillagerCallEntryState(LivingAction& /*action*/, VillagerStates /*row*/, VillagerStates /*final*/,
	                                VillagerStates /*next*/) const override
	{
		return 1;
	}
	uint32_t VillagerCallExitState(LivingAction& action, VillagerStates /*row*/, VillagerStates next) const override
	{
		asked.emplace_back(&action, next);
		return 0;
	}
	int VillagerCallOutOfAnimation(LivingAction& /*action*/, Index /*index*/) const override { return -1; }
	bool VillagerCallValidate(LivingAction& /*action*/, Index /*index*/) const override { return false; }
};

class ArtefactSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		auto& rock = info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock));
		rock.villagerImpressiveValue = k_RockImpressiveValue;
		rock.artifactMultiplier = k_RockArtefactMultiplier;
		auto& dance = info->dance.at(static_cast<size_t>(k_SmallDance));
		dance.duration = k_DanceLength;
		dance.maximumDancers = 20;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::livingActionSystem::emplace<RecordingStateTable>();
		game_clock::SetTurn(k_StartTurn);
		auto& registry = Reg();
		_town = registry.Create();
		auto& town = registry.Assign<Town>(_town);
		town.id = 1;
		town.owner = PlayerNames::PLAYER_ONE;
		_rock = MakeRock();
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
		Locator::livingActionSystem::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	static RecordingStateTable& Table() { return static_cast<RecordingStateTable&>(Locator::livingActionSystem::value()); }

	static entt::entity MakeRock()
	{
		auto& registry = Reg();
		const auto rock = registry.Create();
		registry.Assign<Transform>(rock, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<MobileStatic>(rock, MobileStaticInfo::Rock);
		return rock;
	}

	/// A villager deciding what to do
	static entt::entity MakeVillager()
	{
		auto& registry = Reg();
		const auto villager = registry.Create();
		registry.Assign<Villager>(villager);
		registry.Assign<LivingAction>(villager, VillagerStates::DecideWhatToDo, static_cast<uint16_t>(0));
		return villager;
	}

	/// The small dance round the artefact, in the state given, with its dancers in one group
	entt::entity GiveDance(entt::entity object, Dance::State state, const std::vector<entt::entity>& dancers)
	{
		auto& registry = Reg();
		const auto dance = registry.Create();
		auto& d = registry.Assign<Dance>(dance);
		d.type = k_SmallDance;
		d.owner = object;
		d.state = state;
		d.duration = k_DanceLength;
		d.dancers = static_cast<uint32_t>(dancers.size());
		DanceGroup group;
		group.dancers = dancers;
		d.groups.all.push_back(std::move(group));
		registry.Get<TownArtefact>(object).dance = dance;
		return dance;
	}

	/// The last state the villager was sent to from outside its state logic, if any
	static std::optional<VillagerStates> LastAsked(entt::entity villager)
	{
		const auto* action = &Reg().Get<const LivingAction>(villager);
		std::optional<VillagerStates> last;
		for (const auto& [who, state] : Table().asked)
		{
			if (who == action)
			{
				last = state;
			}
		}
		return last;
	}

	static TownArtefact& Artefact(entt::entity object) { return Reg().Get<TownArtefact>(object); }

	ecs::systems::ArtefactSystem _system;
	entt::entity _town {entt::null};
	entt::entity _rock {entt::null};
};
} // namespace

TEST_F(ArtefactSystemTest, ANewArtefactIsWorthWhatItsKindImpressesVillagers)
{
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(Reg().AllOf<TownArtefact>(_rock));
	// a rock's 0.01 from the object tables
	EXPECT_FLOAT_EQ(Artefact(_rock).value, k_RockImpressiveValue);
	// the same at a worship site
	const auto other = MakeRock();
	const auto site = Reg().Create();
	_system.AddToSite(other, site, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(Artefact(other).value, k_RockImpressiveValue);
}

TEST_F(ArtefactSystemTest, AnArtefactBelongsToWhoPutItDownAndJoinsItsTownOnce)
{
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	const auto& artefact = Artefact(_rock);
	EXPECT_EQ(artefact.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(artefact.town, _town);
	EXPECT_TRUE(artefact.inTown);
	EXPECT_FALSE(artefact.inSite);
	EXPECT_EQ(_system.CountFor(PlayerNames::PLAYER_ONE), 1u);
	// put down by its town again, by someone else: it stays one artefact, with its owner and its worth
	Artefact(_rock).value = 0.5f;
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Artefact(_rock).player, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(Artefact(_rock).value, 0.5f);
	EXPECT_EQ(_system.CountFor(PlayerNames::PLAYER_ONE), 1u);
	EXPECT_EQ(_system.CountFor(PlayerNames::PLAYER_TWO), 0u);
	// one put down by a worship site joins the site's artefacts and no town's
	const auto other = MakeRock();
	const auto site = Reg().Create();
	_system.AddToSite(other, site, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Artefact(other).player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Artefact(other).site, site);
	EXPECT_TRUE(Artefact(other).inSite);
	EXPECT_FALSE(Artefact(other).inTown);
	EXPECT_EQ(_system.CountFor(PlayerNames::PLAYER_TWO), 0u);
}

TEST_F(ArtefactSystemTest, TakingAnArtefactTakesItOutOfItsTownStopsItsDanceAndMakesTheTakerItsOwner)
{
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	const auto a = MakeVillager();
	const auto b = MakeVillager();
	GiveDance(_rock, Dance::State::Dancing, {a, b});
	Artefact(_rock).dancing = true;
	Artefact(_rock).value = 1.5f;
	_system.Taken(_rock, PlayerNames::PLAYER_TWO);
	const auto& artefact = Artefact(_rock);
	EXPECT_FALSE(artefact.inTown);
	EXPECT_FALSE(artefact.dancing);
	// its dancers go back to their lives
	EXPECT_EQ(LastAsked(a), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(LastAsked(b), VillagerStates::DecideWhatToDo);
	// whoever took it owns it, and it keeps its worth
	EXPECT_EQ(artefact.player, PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(artefact.value, 1.5f);
	EXPECT_EQ(_system.CountFor(PlayerNames::PLAYER_ONE), 0u);
	// the same out of a worship site
	const auto other = MakeRock();
	const auto site = Reg().Create();
	_system.AddToSite(other, site, PlayerNames::PLAYER_ONE);
	_system.Taken(other, PlayerNames::PLAYER_TWO);
	EXPECT_FALSE(Artefact(other).inSite);
	EXPECT_EQ(Artefact(other).player, PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(Artefact(other).value, k_RockImpressiveValue);
}

TEST_F(ArtefactSystemTest, ATownsTurnLooksAfterItsOwnArtefactsAndNotASitesOnes)
{
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	const auto dancer = MakeVillager();
	GiveDance(_rock, Dance::State::Dancing, {dancer});
	// one at a worship site, its dance under way: no town's turn starts it
	const auto other = MakeRock();
	const auto site = Reg().Create();
	_system.AddToSite(other, site, PlayerNames::PLAYER_ONE);
	const auto siteDancer = MakeVillager();
	GiveDance(other, Dance::State::Dancing, {siteDancer});
	_system.ProcessTown(_town);
	EXPECT_TRUE(Artefact(_rock).dancing);
	EXPECT_FALSE(Artefact(other).dancing);
	EXPECT_EQ(LastAsked(siteDancer), std::nullopt);
}

TEST_F(ArtefactSystemTest, WhenTheDanceStartsEveryDancerDancesForTheDancesLength)
{
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	const auto a = MakeVillager();
	const auto b = MakeVillager();
	const auto dance = GiveDance(_rock, Dance::State::Dancing, {a, b});
	_system.ProcessTown(_town);
	EXPECT_TRUE(Artefact(_rock).dancing);
	EXPECT_EQ(Artefact(_rock).danceStarted, k_StartTurn);
	EXPECT_EQ(LastAsked(a), VillagerStates::ArtifactDance);
	EXPECT_EQ(LastAsked(b), VillagerStates::ArtifactDance);
	// still dancing on the last turn of its length
	Table().asked.clear();
	game_clock::SetTurn(k_StartTurn + k_DanceLength - 1);
	_system.ProcessTown(_town);
	EXPECT_TRUE(Artefact(_rock).dancing);
	EXPECT_EQ(Reg().Get<const Dance>(dance).state, Dance::State::Dancing);
	EXPECT_EQ(LastAsked(a), std::nullopt);
	// over once its length has passed: the town doesn't most want to relax, so its dancers go back to their lives
	game_clock::SetTurn(k_StartTurn + k_DanceLength);
	_system.ProcessTown(_town);
	EXPECT_FALSE(Artefact(_rock).dancing);
	EXPECT_EQ(Reg().Get<const Dance>(dance).state, Dance::State::Stopped);
	EXPECT_EQ(LastAsked(a), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(LastAsked(b), VillagerStates::DecideWhatToDo);
}

TEST_F(ArtefactSystemTest, ADanceLeftWithNoDancersEnds)
{
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	const auto dance = GiveDance(_rock, Dance::State::Dancing, {});
	Artefact(_rock).dancing = true;
	_system.ProcessTown(_town);
	EXPECT_EQ(Reg().Get<const Dance>(dance).state, Dance::State::Stopped);
	EXPECT_FALSE(Artefact(_rock).dancing);
	// and one that has not started yet ends the same
	const auto other = MakeRock();
	_system.AddToTown(other, _town, PlayerNames::PLAYER_ONE);
	const auto unstarted = GiveDance(other, Dance::State::Dancing, {});
	_system.ProcessTown(_town);
	EXPECT_EQ(Reg().Get<const Dance>(unstarted).state, Dance::State::Stopped);
	EXPECT_FALSE(Artefact(other).dancing);
}
