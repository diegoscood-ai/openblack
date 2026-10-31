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
// worship site, a hand taking it, a town's turn of its artefacts' dances, and an artefact whose object goes. The villagers'
// state table is a fake that records the state each dancer is sent to and refuses it, so that nothing else of the
// villager runs; the dances and the spot visuals are fakes that record what goes.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/EventManager.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DanceSystemInterface.h"
#include "ECS/Systems/Implementations/ArtefactSystem.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"
#include "support/LivingActionFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
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
/// A worship site's artefact power-up multiplier, every tribe's in info.dat
constexpr float k_SitePowerUpMultiplier = 0.00001f;

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

/// The dances as far as an artefact that goes asks them: each dance it deletes, kept
class RecordingDances final: public ecs::systems::DanceSystemInterface
{
public:
	std::vector<entt::entity> destroyed;

	entt::entity Create(DanceInfo /*type*/, const map_coords::MapCoords& /*place*/, entt::entity /*owner*/,
	                    uint32_t /*duration*/, bool /*madeByScript*/, entt::entity /*town*/) override
	{
		return entt::null;
	}
	void Destroy(entt::entity dance) override { destroyed.push_back(dance); }
	void ProcessTurn() override {}
};

/// The particles as far as an artefact that goes asks them: each spot visual it deletes at once, kept
class RecordingParticles final: public test::InertParticleSystem
{
public:
	std::vector<entt::entity> deleted;
	std::vector<std::pair<entt::entity, float>> strengths;

	void DeleteSpotVisual(entt::entity object) override { deleted.push_back(object); }
	void SetSpotVisualStrength(entt::entity visual, float strength) override { strengths.emplace_back(visual, strength); }
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
		info->worshipSite.at(0).artifactPowerupMultiplier = k_SitePowerUpMultiplier;
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

	/// The rock an artefact of the town, its dance under way with two dancers and its glow over it
	struct DancingArtefact
	{
		entt::entity a, b, dance, glow;
	};
	DancingArtefact MakeDancingArtefact()
	{
		_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
		DancingArtefact made {.a = MakeVillager(), .b = MakeVillager(), .dance = entt::null, .glow = Reg().Create()};
		made.dance = GiveDance(_rock, Dance::State::Dancing, {made.a, made.b});
		Artefact(_rock).dancing = true;
		Artefact(_rock).effect = made.glow;
		return made;
	}

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

TEST_F(ArtefactSystemTest, AfterADanceInATownThatMostWantsToRelaxTheDancersWaitForTheNextOne)
{
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	// relaxation is the town's most wanted desire
	Reg().Get<Town>(_town).desire.sorted.at(0).index = static_cast<uint32_t>(TownDesireInfo::ForRelaxation);
	const auto a = MakeVillager();
	const auto b = MakeVillager();
	const auto dance = GiveDance(_rock, Dance::State::Dancing, {a, b});
	_system.ProcessTown(_town);
	ASSERT_TRUE(Artefact(_rock).dancing);
	// over once its length has passed: its dancers go back to waiting for another dance
	Table().asked.clear();
	game_clock::SetTurn(k_StartTurn + k_DanceLength);
	_system.ProcessTown(_town);
	EXPECT_FALSE(Artefact(_rock).dancing);
	EXPECT_EQ(Reg().Get<const Dance>(dance).state, Dance::State::Stopped);
	EXPECT_EQ(LastAsked(a), VillagerStates::WaitForArtifactDance);
	EXPECT_EQ(LastAsked(b), VillagerStates::WaitForArtifactDance);
	// a dance that stopped by itself while under way ends with its dancers back to their lives, relaxation or not
	const auto other = MakeRock();
	_system.AddToTown(other, _town, PlayerNames::PLAYER_ONE);
	const auto c = MakeVillager();
	const auto stopped = GiveDance(other, Dance::State::Dancing, {c});
	_system.ProcessTown(_town);
	ASSERT_TRUE(Artefact(other).dancing);
	Reg().Get<Dance>(stopped).state = Dance::State::Stopped;
	Table().asked.clear();
	_system.ProcessTown(_town);
	EXPECT_FALSE(Artefact(other).dancing);
	EXPECT_EQ(LastAsked(c), VillagerStates::DecideWhatToDo);
}

TEST_F(ArtefactSystemTest, AVillagerJoiningTheDanceOfAnArtefactThatHasMovedTakesTheDanceToIt)
{
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	// the town is far away and has no storage pit, so no footpath is laid
	Reg().Assign<Transform>(_town, glm::vec3(3000.0f, 0.0f, 3000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	// its dance was set out where the rock stood before (90, 90), waiting for dancers, with one group anyone may join
	const auto dance = GiveDance(_rock, Dance::State::Dancing, {});
	auto& d = Reg().Get<Dance>(dance);
	d.place = map_coords::FromMetres(glm::vec2(90.0f, 90.0f));
	d.groups.shared = {0};
	const auto villager = MakeVillager();
	Reg().Assign<Transform>(villager, glm::vec3(105.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.OfferRelaxation(_town, villager));
	// the dance has moved to where the rock is now, and the villager waits there for it
	const auto& moved = Reg().Get<const Dance>(dance);
	EXPECT_EQ(moved.place, map_coords::FromMetres(glm::vec2(100.0f, 100.0f)));
	EXPECT_EQ(moved.dancers, 1u);
	EXPECT_EQ(LastAsked(villager), VillagerStates::WaitForArtifactDance);
}

TEST_F(ArtefactSystemTest, EachTimeAnArtefactJoinsATownOrASiteItsPlayersCreatureMayCopyTheDeed)
{
	const test::RestoreService<Locator::events> keepEvents;
	Locator::events::emplace<EventManager>();
	std::vector<ecs::events::PlayerDeedForMimic> deeds;
	Locator::events::value().AddHandler<ecs::events::PlayerDeedForMimic>(
	    [&deeds](const ecs::events::PlayerDeedForMimic& event) { deeds.push_back(event); });
	// joining a town: the player who put it down may be copied making an artefact (deed 14), at the object
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, creature_watching::Deed::MakeArtefact);
	EXPECT_EQ(static_cast<int>(deeds.front().deed), 14);
	EXPECT_EQ(deeds.front().object, _rock);
	EXPECT_EQ(deeds.front().point, glm::vec3(100.0f, 0.0f, 100.0f));
	EXPECT_FALSE(deeds.front().magic.has_value());
	// put down by its town again, by another player: that player's deed, though the artefact stays its owner's
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(deeds.size(), 2u);
	EXPECT_EQ(deeds.back().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(deeds.back().deed, creature_watching::Deed::MakeArtefact);
	// joining a worship site: the same deed
	const auto other = MakeRock();
	const auto site = Reg().Create();
	_system.AddToSite(other, site, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(deeds.size(), 3u);
	EXPECT_EQ(deeds.back().player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(deeds.back().deed, creature_watching::Deed::MakeArtefact);
	EXPECT_EQ(deeds.back().object, other);
	// a land script's artefact of no town: no deed
	_system.AddToNoTown(MakeRock(), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.size(), 3u);
}

TEST_F(ArtefactSystemTest, EveryThousandTurnsASitesArtefactsGainWorthByItsDancersShareOfItsPlayersPeople)
{
	// an artefact worth more than 1 glows: its glow starts in particles that the test does not look at
	const test::RestoreService<Locator::particleSystem> keepParticles;
	Locator::particleSystem::emplace<test::InertParticleSystem>();
	auto& registry = Reg();
	// the player's people: its two towns' adults and children, 200 in all; another player's town is not counted
	registry.Get<Town>(_town).stats.adults = 60;
	registry.Get<Town>(_town).stats.children = 40;
	const auto second = registry.Create();
	auto& secondTown = registry.Assign<Town>(second);
	secondTown.owner = PlayerNames::PLAYER_ONE;
	secondTown.stats.adults = 80;
	secondTown.stats.children = 20;
	const auto rivals = registry.Create();
	auto& rivalTown = registry.Assign<Town>(rivals);
	rivalTown.owner = PlayerNames::PLAYER_TWO;
	rivalTown.stats.adults = 300;
	const auto site = registry.Create();
	registry.Assign<WorshipSite>(site).player = PlayerNames::PLAYER_ONE;
	// two artefacts kept at the site, one in a town and one taken out of the site
	_system.AddToSite(_rock, site, PlayerNames::PLAYER_ONE);
	const auto worthy = MakeRock();
	_system.AddToSite(worthy, site, PlayerNames::PLAYER_ONE);
	Artefact(worthy).value = 2.0f;
	const auto inTown = MakeRock();
	_system.AddToTown(inTown, _town, PlayerNames::PLAYER_ONE);
	const auto taken = MakeRock();
	_system.AddToSite(taken, site, PlayerNames::PLAYER_ONE);
	_system.Taken(taken, PlayerNames::PLAYER_ONE);

	// at turn 1000, 20 dancers of 200 people: the gain is the land balance (1) x 0.00001 x 20 / 200 = 0.000001, and each
	// artefact kept at the site gains 300 x the gain, slowed by 1 / its worth once it is worth more than 1
	constexpr float k_Gain = 0.000001f;
	_system.PowerUpSite(site, 20);
	EXPECT_FLOAT_EQ(Artefact(_rock).value, k_RockImpressiveValue + 300.0f * k_Gain);
	EXPECT_FLOAT_EQ(Artefact(worthy).value, 2.0f + 0.5f * 300.0f * k_Gain);
	EXPECT_FLOAT_EQ(Artefact(inTown).value, k_RockImpressiveValue);
	EXPECT_FLOAT_EQ(Artefact(taken).value, k_RockImpressiveValue);

	// any other turn: nothing
	const float rock = Artefact(_rock).value;
	game_clock::SetTurn(k_StartTurn + 1);
	_system.PowerUpSite(site, 20);
	EXPECT_FLOAT_EQ(Artefact(_rock).value, rock);
	// no dancers: no gain
	game_clock::SetTurn(2 * k_StartTurn);
	_system.PowerUpSite(site, 0);
	EXPECT_FLOAT_EQ(Artefact(_rock).value, rock);
	// the artefact speed of the land balance (its seventh value) scales the gain
	land_balance::Set(6, 2.0f);
	_system.PowerUpSite(site, 20);
	land_balance::Set(6, 1.0f);
	EXPECT_FLOAT_EQ(Artefact(_rock).value, rock + 300.0f * 2.0f * k_Gain);
}

TEST_F(ArtefactSystemTest, AnArtefactWhoseObjectGoesGoesAtItsTownsNextTurnWithItsDanceAndItsGlow)
{
	const test::RestoreService<Locator::danceSystem> keepDances;
	const test::RestoreService<Locator::particleSystem> keepParticles;
	auto& dances = static_cast<RecordingDances&>(Locator::danceSystem::emplace<RecordingDances>());
	auto& particles = static_cast<RecordingParticles&>(Locator::particleSystem::emplace<RecordingParticles>());
	const auto made = MakeDancingArtefact();
	Reg().Destroy(_rock);
	// until its town's turn it is still one of the town's, counted for its owner, and its dance and glow stay
	EXPECT_EQ(_system.CountFor(PlayerNames::PLAYER_ONE), 1u);
	EXPECT_EQ(_system.CountGone(), 1u);
	EXPECT_TRUE(dances.destroyed.empty());
	EXPECT_TRUE(particles.deleted.empty());
	EXPECT_EQ(LastAsked(made.a), std::nullopt);
	// its town's turn lets it go: its dancers back to their lives, then its dance deleted, then its glow at once
	_system.ProcessTown(_town);
	EXPECT_EQ(LastAsked(made.a), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(LastAsked(made.b), VillagerStates::DecideWhatToDo);
	ASSERT_EQ(dances.destroyed.size(), 1u);
	EXPECT_EQ(dances.destroyed.front(), made.dance);
	ASSERT_EQ(particles.deleted.size(), 1u);
	EXPECT_EQ(particles.deleted.front(), made.glow);
	EXPECT_EQ(_system.CountFor(PlayerNames::PLAYER_ONE), 0u);
	EXPECT_EQ(_system.CountGone(), 0u);
	// and nothing of it is left for the next turn
	Table().asked.clear();
	_system.ProcessTown(_town);
	EXPECT_EQ(dances.destroyed.size(), 1u);
	EXPECT_EQ(particles.deleted.size(), 1u);
	EXPECT_TRUE(Table().asked.empty());
}

TEST_F(ArtefactSystemTest, AnArtefactWhoseObjectIsToGoGoesInItsTownsTurn)
{
	const test::RestoreService<Locator::danceSystem> keepDances;
	const test::RestoreService<Locator::particleSystem> keepParticles;
	auto& dances = static_cast<RecordingDances&>(Locator::danceSystem::emplace<RecordingDances>());
	auto& particles = static_cast<RecordingParticles&>(Locator::particleSystem::emplace<RecordingParticles>());
	const auto made = MakeDancingArtefact();
	// marked to go, still there: its town's turn lets it go the same, and does nothing else of its turn
	Reg().Assign<Unavailable>(_rock);
	_system.ProcessTown(_town);
	EXPECT_EQ(LastAsked(made.a), VillagerStates::DecideWhatToDo);
	ASSERT_EQ(dances.destroyed.size(), 1u);
	EXPECT_EQ(dances.destroyed.front(), made.dance);
	ASSERT_EQ(particles.deleted.size(), 1u);
	EXPECT_EQ(particles.deleted.front(), made.glow);
	EXPECT_FALSE(Artefact(_rock).inTown);
	EXPECT_EQ(Artefact(_rock).dance, entt::entity {entt::null});
	EXPECT_EQ(Artefact(_rock).effect, entt::entity {entt::null});
	// freed later, it is not let go again
	Reg().Destroy(_rock);
	EXPECT_EQ(_system.CountGone(), 0u);
}

TEST_F(ArtefactSystemTest, AGlowingArtefactsWorthIsItsGlowsStrength)
{
	const test::RestoreService<Locator::particleSystem> keepParticles;
	auto& particles = static_cast<RecordingParticles&>(Locator::particleSystem::emplace<RecordingParticles>());
	_system.AddToTown(_rock, _town, PlayerNames::PLAYER_ONE);
	const auto glow = Reg().Create();
	Artefact(_rock).effect = glow;
	particles.strengths.clear();
	_system.SetWorth(_rock, 2.5f);
	_system.SetWorth(_rock, 7.0f);
	EXPECT_EQ(Artefact(_rock).value, 7.0f);
	const std::vector<std::pair<entt::entity, float>> expected {{glow, 2.5f}, {glow, 7.0f}};
	EXPECT_EQ(particles.strengths, expected);
}

TEST_F(ArtefactSystemTest, AnArtefactWhoseDanceHasGoneAlreadyLeavesNoDanceToDelete)
{
	const test::RestoreService<Locator::danceSystem> keepDances;
	const test::RestoreService<Locator::particleSystem> keepParticles;
	auto& dances = static_cast<RecordingDances&>(Locator::danceSystem::emplace<RecordingDances>());
	Locator::particleSystem::emplace<RecordingParticles>();
	const auto made = MakeDancingArtefact();
	// its dance went first, in the dances' own turn, once its object had gone
	Reg().Destroy(_rock);
	Reg().Destroy(made.dance);
	_system.ProcessTown(_town);
	EXPECT_TRUE(dances.destroyed.empty());
	EXPECT_EQ(LastAsked(made.a), std::nullopt);
	EXPECT_EQ(_system.CountGone(), 0u);
}

TEST_F(ArtefactSystemTest, AnArtefactInNoTownWhoseObjectGoesIsNotLookedAtAgain)
{
	const test::RestoreService<Locator::danceSystem> keepDances;
	const test::RestoreService<Locator::particleSystem> keepParticles;
	auto& dances = static_cast<RecordingDances&>(Locator::danceSystem::emplace<RecordingDances>());
	auto& particles = static_cast<RecordingParticles&>(Locator::particleSystem::emplace<RecordingParticles>());
	MakeDancingArtefact();
	// taken out of its town by a hand, then gone; and one at a worship site only, gone
	_system.Taken(_rock, PlayerNames::PLAYER_ONE);
	Reg().Destroy(_rock);
	const auto other = MakeRock();
	_system.AddToSite(other, Reg().Create(), PlayerNames::PLAYER_ONE);
	Reg().Destroy(other);
	EXPECT_EQ(_system.CountGone(), 0u);
	_system.ProcessTown(_town);
	// their dances and glows are left to end by themselves
	EXPECT_TRUE(dances.destroyed.empty());
	EXPECT_TRUE(particles.deleted.empty());
}

TEST_F(ArtefactSystemTest, TheLandsClearForgetsTheArtefactsWhoseObjectsHaveGone)
{
	MakeDancingArtefact();
	Reg().Destroy(_rock);
	EXPECT_EQ(_system.CountGone(), 1u);
	_system.ClearMap();
	EXPECT_EQ(_system.CountGone(), 0u);
	EXPECT_EQ(_system.CountFor(PlayerNames::PLAYER_ONE), 0u);
	// and the objects that go with the land are not heard
	const auto other = MakeRock();
	Reg().Assign<TownArtefact>(other, TownArtefact {.town = _town, .inTown = true});
	Reg().Destroy(other);
	EXPECT_EQ(_system.CountGone(), 0u);
}
