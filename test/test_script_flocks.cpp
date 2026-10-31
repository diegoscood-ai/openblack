/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The intro opcodes: the Flock class (ECS/Flocks.h: the member order by each Living's flock order and the leader,
// the tail; Remove, Separate, Merge, RandomMember, PosWithinDomain) and the script containers
// (ECS/ScriptContainers.h: ID_SIZE, ID_ADULT_SIZE, OBJECT_CAPACITY, OBJECT_ADULT_CAPACITY, FLOCK_DETACH,
// CHANGE_INNER_OUTER, GET_FIRST / GET_NEXT_IN_CONTAINER), and FLOCK_ATTACH and FLOCK_DISBAND as a script calls them.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <DanceFile.h>
#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "Common/GameRandomTesting.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalScriptRules.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DanceRules.h"
#include "ECS/Dances.h"
#include "ECS/Flocks.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptContainers.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/Implementations/ScriptObjectsSystem.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/VillagerSpeed.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace flocks = openblack::ecs::flocks;
namespace containers = openblack::ecs::script_containers;

namespace
{
class FlocksTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		// the scripts' logger (ScriptContainers / ScriptHeld log their errors there): a null sink, as in
		// test_interface_interaction; without one SPDLOG_LOGGER_ERROR dereferences a null logger
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// entity 0 is "no thing" for a script id (a script id of 0 finds nothing), as the game has made
		// other things before: the test's first entity must not be a scripted one
		Locator::entitiesRegistry::value().Create();
	}

	void TearDown() override
	{
		game_random::testing::SetGameRand({}, {});
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeVillager(glm::vec3 position)
	{
		const auto e = Reg().Create();
		auto& v = Reg().Assign<Villager>(e);
		v.town = entt::null;
		v.abode = entt::null;
		Reg().Assign<Transform>(e, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	}

	static std::vector<entt::entity> Members(entt::entity flock) { return Reg().Get<const Flock>(flock).members; }
	static uint32_t Id(entt::entity e) { return static_cast<uint32_t>(e); }
};
} // namespace

TEST_F(FlocksTest, CreateIsTheScriptFlock)
{
	const auto f = flocks::Create({10.0f, 2.0f, 20.0f}, flocks::k_ScriptFlockId);
	const auto& flock = Reg().Get<const Flock>(f);
	EXPECT_EQ(flock.id, 0xABA52);
	EXPECT_EQ(flock.domainRadius, 0x50);
	EXPECT_EQ(flock.flockDistance, 0x1E);
	EXPECT_TRUE(flock.domainCentre == glm::vec3(10.0f, 2.0f, 20.0f));
	EXPECT_TRUE(flock.savedDomainCentre == flock.domainCentre);
	EXPECT_EQ(flock.maxMembers, 0u);
	EXPECT_TRUE(flocks::Leader(f) == entt::null);
	// no tail: GetFlockPos is the flock's domain centre
	EXPECT_TRUE(flocks::GetFlockPos(f) == glm::vec3(10.0f, 2.0f, 20.0f));
}

TEST_F(FlocksTest, MembersGoBeforeTheHeadAndTheFirstIsTheLeader)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({1.0f, 0.0f, 1.0f});
	const auto b = MakeVillager({2.0f, 0.0f, 2.0f});
	const auto c = MakeVillager({3.0f, 0.0f, 3.0f});
	EXPECT_TRUE(flocks::AddMember(f, a));
	EXPECT_TRUE(flocks::AddMember(f, b));
	EXPECT_TRUE(flocks::AddMember(f, c));
	// order 0 each: every one before the head; the list head..tail is c, b, a (members reversed: a, b, c)
	EXPECT_EQ(Members(f), (std::vector<entt::entity> {a, b, c}));
	EXPECT_EQ(flocks::MembersFromHead(f), (std::vector<entt::entity> {c, b, a}));
	EXPECT_TRUE(flocks::Leader(f) == a);
	EXPECT_TRUE(flocks::IsLeader(a));
	EXPECT_FALSE(flocks::IsLeader(b));
	EXPECT_TRUE(flocks::FlockOf(b) == f);
	EXPECT_EQ(flocks::Size(f), 3u);
	EXPECT_TRUE(flocks::GetFlockPos(f) == glm::vec3(1.0f, 0.0f, 1.0f));
	// AddMember of a member: it leaves its flock first and comes back as the head
	EXPECT_TRUE(flocks::AddMember(f, a));
	EXPECT_EQ(Members(f), (std::vector<entt::entity> {b, c, a}));
	EXPECT_TRUE(flocks::Leader(f) == b);
}

TEST_F(FlocksTest, AddLeaderTakesTheTail)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	const auto father = MakeVillager({});
	// an empty flock: order 5
	flocks::AddLeader(f, father);
	EXPECT_EQ(flocks::OrderOf(father), 5);
	EXPECT_TRUE(flocks::Leader(f) == father);
	// the next members (0) go before the head: the leader stays the tail
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	EXPECT_EQ(flocks::MembersFromHead(f), (std::vector<entt::entity> {b, a, father}));
	EXPECT_TRUE(flocks::Leader(f) == father);
	// a member made leader: the tail's + 1, moved to the tail
	flocks::AddLeader(f, a);
	EXPECT_EQ(flocks::OrderOf(a), 6);
	EXPECT_EQ(flocks::MembersFromHead(f), (std::vector<entt::entity> {b, father, a}));
	EXPECT_TRUE(flocks::Leader(f) == a);
	// the tail itself: nothing
	flocks::AddLeader(f, a);
	EXPECT_EQ(flocks::OrderOf(a), 6);
}

TEST_F(FlocksTest, RemoveAndDelete)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	// 0: an emptied flock is kept
	EXPECT_TRUE(flocks::RemoveLiving(f, a, false));
	EXPECT_TRUE(flocks::FlockOf(a) == entt::null);
	EXPECT_FALSE(flocks::RemoveLiving(f, a, false)); // not a member any more
	EXPECT_TRUE(flocks::RemoveLiving(f, b, false));
	EXPECT_TRUE(Reg().Valid(f));
	// 1: deleted with its last member
	flocks::AddMember(f, a);
	EXPECT_TRUE(flocks::RemoveLiving(f, a, true));
	EXPECT_FALSE(Reg().Valid(f));
}

TEST_F(FlocksTest, JoiningAnotherFlockDeletesTheEmptiedOne)
{
	const auto first = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto second = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	flocks::AddMember(first, a);
	EXPECT_TRUE(flocks::AddMember(second, a));
	EXPECT_FALSE(Reg().Valid(first)); // leaving it deletes the emptied flock
	EXPECT_TRUE(flocks::FlockOf(a) == second);
}

TEST_F(FlocksTest, SeparateKeepsTheDomain)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	Reg().Get<Flock>(f).domainRadius = 10;
	Reg().Get<Flock>(f).flockDistance = 2;
	const auto a = MakeVillager({5.0f, 0.0f, 6.0f});
	const auto b = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	const auto own = flocks::SeparateIntoNewFlock(f, a, false);
	EXPECT_TRUE(flocks::FlockOf(a) == own);
	EXPECT_EQ(Reg().Get<const Flock>(own).domainRadius, 10);
	EXPECT_EQ(Reg().Get<const Flock>(own).flockDistance, 2);
	EXPECT_EQ(Reg().Get<const Flock>(own).id, -1);
	EXPECT_TRUE(Reg().Get<const Flock>(own).domainCentre == glm::vec3(5.0f, 0.0f, 6.0f));
	EXPECT_EQ(Members(f), (std::vector<entt::entity> {b}));
}

TEST_F(FlocksTest, MergeTakesTheOthersFromItsHead)
{
	const auto keeper = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto other = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	const auto c = MakeVillager({});
	flocks::AddMember(keeper, a);
	flocks::AddMember(other, b);
	flocks::AddMember(other, c); // other: head c, tail b
	Reg().Get<Flock>(keeper).maxMembers = 1;
	Reg().Get<Flock>(other).maxMembers = 2;
	flocks::Merge(keeper, other);
	// c first (the other's head), then b: each before the keeper's head
	EXPECT_EQ(flocks::MembersFromHead(keeper), (std::vector<entt::entity> {b, c, a}));
	EXPECT_FALSE(Reg().Valid(other));
	// maxMembers += the other's at each member (no animal: no clamp)
	EXPECT_EQ(Reg().Get<const Flock>(keeper).maxMembers, 5u);
}

TEST_F(FlocksTest, RandomMemberSkipsTheExcluded)
{
	const auto f = flocks::Create({}, flocks::k_ScriptFlockId);
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	const auto c = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	flocks::AddMember(f, c); // head c, b, tail a
	std::vector<uint32_t> ranges;
	uint32_t next = 0;
	game_random::testing::SetGameRand(
	    [&](uint32_t n) {
		    ranges.push_back(n);
		    return next;
	    },
	    [](float) { return 0.0f; });
	// exclude the tail with 3 members: GameRand(2)
	next = 1;
	EXPECT_TRUE(flocks::RandomMember(f, a) == b);
	EXPECT_EQ(ranges.back(), 2u);
	// r past the others: the excluded one is skipped, nothing after it
	next = 2;
	EXPECT_TRUE(flocks::RandomMember(f, a) == entt::null);
	// no exclude: GameRand(3)
	next = 0;
	EXPECT_TRUE(flocks::RandomMember(f, entt::null) == c);
	EXPECT_EQ(ranges.back(), 3u);
}

TEST_F(FlocksTest, PosWithinDomain)
{
	const auto a = MakeVillager({});
	// no flock: 0
	EXPECT_FALSE(flocks::PosWithinDomain(a, {0.0f, 0.0f}, 1.0f));
	const auto f = flocks::Create({100.0f, 0.0f, 100.0f}, flocks::k_ScriptFlockId);
	Reg().Get<Flock>(f).domainRadius = 3;
	flocks::AddMember(f, a);
	EXPECT_TRUE(flocks::PosWithinDomain(a, {102.0f, 100.0f}, 1.0f));
	// (no exact edge case: GetDistanceInMetres is the table distance, 3 m may come out a hair above 3)
	EXPECT_TRUE(flocks::PosWithinDomain(a, {102.9f, 100.0f}, 1.0f));
	EXPECT_FALSE(flocks::PosWithinDomain(a, {104.0f, 100.0f}, 1.0f));
	EXPECT_TRUE(flocks::PosWithinDomain(a, {104.0f, 100.0f}, 2.0f));
}

TEST_F(FlocksTest, ContainersSizeDetachAndInnerOuter)
{
	const auto f = containers::CreateFlock({50.0f, 0.0f, 50.0f});
	EXPECT_TRUE(containers::IsContainer(f));
	// a slot, controlled by the script only from its first reference
	EXPECT_FALSE(ecs::script_held::IsControlledByScript(f));
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	EXPECT_EQ(containers::Size(Id(f)).value_or(-1.0f), 2.0f);
	EXPECT_FALSE(containers::Size(Id(a)).has_value());
	// CHANGE_INNER_OUTER (1, 3, 0): distance 1, radius 3; 0 keeps a field
	containers::ChangeInnerOuter(Id(f), 1.0f, 3.0f, 0.0f);
	EXPECT_EQ(Reg().Get<const Flock>(f).flockDistance, 1);
	EXPECT_EQ(Reg().Get<const Flock>(f).domainRadius, 3);
	containers::ChangeInnerOuter(Id(f), 0.0f, 10.0f, 2.9f);
	EXPECT_EQ(Reg().Get<const Flock>(f).flockDistance, 1);
	EXPECT_EQ(Reg().Get<const Flock>(f).domainRadius, 10);
	EXPECT_EQ(Reg().Get<const Flock>(f).calm, 2);
	// only a flock takes them here; anything else is left for the caller (a weather thing, or the error)
	EXPECT_TRUE(containers::ChangeInnerOuter(Id(f), 0.0f, 0.0f, 0.0f));
	EXPECT_FALSE(containers::ChangeInnerOuter(Id(a), 1.0f, 3.0f, 0.0f));
	EXPECT_FALSE(containers::ChangeInnerOuter(0, 1.0f, 3.0f, 0.0f));
	EXPECT_EQ(Reg().Get<const Flock>(f).flockDistance, 1);
	EXPECT_EQ(Reg().Get<const Flock>(f).domainRadius, 10);
	// FLOCK_DETACH(a, flock): a villager is only removed, its id pushed; the emptied flock is kept
	EXPECT_EQ(containers::Detach(Id(f), Id(a)), Id(a));
	EXPECT_TRUE(flocks::FlockOf(a) == entt::null);
	EXPECT_EQ(containers::Size(Id(f)).value_or(-1.0f), 1.0f);
	// a Living as the container stands for its flock
	EXPECT_EQ(containers::Detach(Id(b), Id(b)), Id(b));
	EXPECT_EQ(containers::Size(Id(f)).value_or(-1.0f), 0.0f);
	EXPECT_TRUE(Reg().Valid(f));
	// a dead container: 0
	EXPECT_EQ(containers::Detach(0, Id(b)), 0u);
}

TEST_F(FlocksTest, DetachWithoutAnObjPicksAMemberButTheLeader)
{
	const auto f = containers::CreateFlock({});
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b); // head b, tail a
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float) { return 0.0f; });
	EXPECT_EQ(containers::Detach(Id(f), 0), Id(b));
	EXPECT_TRUE(flocks::Leader(f) == a);
	// one member left: no exclude, the leader goes too
	EXPECT_EQ(containers::Detach(Id(f), 0), Id(a));
	EXPECT_EQ(containers::Detach(Id(f), 0), 0u);
}

namespace
{
/// A dance of one group its file names, that shares all its dancers out to that group
entt::entity MakeDance(ecs::Registry& registry)
{
	auto file = std::make_shared<dance::DanceFile>();
	file->groupCount = 1;
	const auto entity = ecs::dances::Create(registry, {.type = DanceInfo::NewDanceAroundPerson}, file);
	auto& groups = registry.Get<Dance>(entity).groups;
	groups.shared = {0};
	ecs::dance_rules::SetWeights(groups);
	return entity;
}
} // namespace

TEST_F(FlocksTest, ADanceIsAContainerOfItsDancers)
{
	const auto d = MakeDance(Reg());
	EXPECT_TRUE(containers::IsContainer(d));
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, a, DanceGroup::k_Men));
	ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, b, DanceGroup::k_Men));
	// ID_SIZE: its dancers
	EXPECT_EQ(containers::Size(Id(d)).value_or(-1.0f), 2.0f);
	// its loop: in the order they joined
	std::vector<entt::entity> walked;
	EXPECT_TRUE(containers::ForEachMember(d, [&walked](entt::entity member) {
		walked.push_back(member);
		return false;
	}));
	EXPECT_EQ(walked, std::vector<entt::entity>({a, b}));
	// FLOCK_DETACH(a, dance): a leaves it, its id pushed; the dance stays
	EXPECT_EQ(containers::Detach(Id(d), Id(a)), Id(a));
	EXPECT_TRUE(ecs::dances::DanceOf(Reg(), a) == entt::null);
	EXPECT_EQ(containers::Size(Id(d)).value_or(-1.0f), 1.0f);
	EXPECT_TRUE(Reg().Valid(d));
}

TEST_F(FlocksTest, DetachFromADanceWithoutAnObjPassesOverWhatItIsDancedAbout)
{
	const auto d = MakeDance(Reg());
	const auto centre = MakeVillager({});
	const auto other = MakeVillager({});
	ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, centre, DanceGroup::k_Men));
	ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, other, DanceGroup::k_Men));
	Reg().Get<Dance>(d).owner = centre;
	// two dancers: the first that is not what it is danced about
	EXPECT_EQ(containers::Detach(Id(d), 0), Id(other));
	// one left: that one, whatever it is
	EXPECT_EQ(containers::Detach(Id(d), 0), Id(centre));
	EXPECT_EQ(containers::Detach(Id(d), 0), 0u);
}

TEST_F(FlocksTest, DisbandingADanceEmptiesIt)
{
	const auto d = MakeDance(Reg());
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, a, DanceGroup::k_Women));
	ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, b, DanceGroup::k_Women));
	containers::Disband(Id(d));
	EXPECT_EQ(ecs::dances::Size(Reg(), d), 0u);
	EXPECT_TRUE(ecs::dances::DanceOf(Reg(), a) == entt::null);
	EXPECT_TRUE(ecs::dances::DanceOf(Reg(), b) == entt::null);
	EXPECT_TRUE(Reg().Valid(d));
}

TEST_F(FlocksTest, AnAnimalAttachedToADanceDancesInIt)
{
	const auto MakeAnimal = [] {
		const auto e = Reg().Create();
		Reg().Assign<Animal>(e);
		Reg().Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<AnimalBrain>(e);
		return e;
	};
	// FLOCK_ATTACH(animal, dance): any living joins the dance and is put in the dance state, not only a villager
	const auto d = MakeDance(Reg());
	const auto animal = MakeAnimal();
	EXPECT_EQ(containers::Attach(Id(animal), Id(d), false), Id(d));
	EXPECT_EQ(ecs::dances::DanceOf(Reg(), animal), d);
	EXPECT_EQ(Reg().Get<const AnimalBrain>(animal).topState, static_cast<uint8_t>(ecs::animal_ai::AnimalState::InDance));
	// A dance whose one group takes nobody: the animal is in no group, and in the dance state all the same
	const auto empty = ecs::dances::Create(Reg(), {.type = DanceInfo::DancePrayInwards}, nullptr);
	const auto other = MakeAnimal();
	EXPECT_EQ(containers::Attach(Id(other), Id(empty), false), Id(empty));
	EXPECT_EQ(ecs::dances::DanceOf(Reg(), other), entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const AnimalBrain>(other).topState, static_cast<uint8_t>(ecs::animal_ai::AnimalState::InDance));
}

TEST_F(FlocksTest, CreateKeepsTheInfoAndThePlayer)
{
	// a flock made with its info and player keeps both; the rest as Create(pos, id)
	const auto f =
	    flocks::Create({1.0f, 0.0f, 2.0f}, flocks::k_DefaultFlockInfo, PlayerNames::PLAYER_TWO, flocks::k_ScriptFlockId);
	const auto& flock = Reg().Get<const Flock>(f);
	ASSERT_TRUE(flock.info.has_value());
	EXPECT_EQ(*flock.info, flocks::k_DefaultFlockInfo);
	ASSERT_TRUE(flock.player.has_value());
	EXPECT_EQ(*flock.player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(flock.id, 0xABA52);
	EXPECT_EQ(flock.domainRadius, 0x50);
	// a flock made from a Living, and Create(pos, id): no info, and the neutral player, as the original
	// (docs/wiki/scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal)
	const auto g = flocks::Create({1.0f, 0.0f, 2.0f}, -1);
	EXPECT_FALSE(Reg().Get<const Flock>(g).info.has_value());
	ASSERT_TRUE(Reg().Get<const Flock>(g).player.has_value());
	EXPECT_EQ(*Reg().Get<const Flock>(g).player, PlayerNames::NEUTRAL);
}

TEST_F(FlocksTest, AnimalSetTownListsAtTheHeadAndForgetUnlinks)
{
	const auto town = Reg().Create();
	Reg().Assign<Town>(town, 3u);
	const auto other = Reg().Create();
	Reg().Assign<Town>(other, 4u);
	const auto MakeAnimal = [] {
		const auto e = Reg().Create();
		Reg().Assign<Animal>(e);
		Reg().Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	};
	const auto a = MakeAnimal();
	const auto b = MakeAnimal();
	// SetTown: the animal's town set, at the head of the town's animal list
	ecs::animal_ai::SetTown(a, town);
	ecs::animal_ai::SetTown(b, town);
	EXPECT_EQ(Reg().Get<const Town>(town).animals, std::vector<entt::entity>({b, a}));
	EXPECT_EQ(Reg().Get<const Animal>(a).town, town);
	// literal: a new town does not take it off the old list
	ecs::animal_ai::SetTown(a, other);
	EXPECT_EQ(Reg().Get<const Animal>(a).town, other);
	EXPECT_EQ(Reg().Get<const Town>(other).animals, std::vector<entt::entity>({a}));
	EXPECT_EQ(Reg().Get<const Town>(town).animals, std::vector<entt::entity>({b, a}));
	// no town: none and no list
	ecs::animal_ai::SetTown(b, entt::null);
	EXPECT_TRUE(Reg().Get<const Animal>(b).town == entt::null);
	// Forget: off the list of its own town only
	ecs::animal_ai::Forget(a);
	EXPECT_TRUE(Reg().Get<const Town>(other).animals.empty());
	EXPECT_EQ(Reg().Get<const Town>(town).animals, std::vector<entt::entity>({b, a}));
	EXPECT_TRUE(Reg().Get<const Animal>(a).town == entt::null);
}

TEST_F(FlocksTest, FirstAndNextInAFlockWalkFromItsHead)
{
	const auto f = containers::CreateFlock({});
	EXPECT_TRUE(containers::FirstMember(Id(f)) == entt::null); // no member yet
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	const auto c = MakeVillager({});
	flocks::AddMember(f, a);
	flocks::AddMember(f, b);
	flocks::AddMember(f, c); // from the head: c, b, a
	EXPECT_TRUE(containers::FirstMember(Id(f)) == c);
	EXPECT_TRUE(containers::NextMember(Id(f), Id(c)) == b);
	EXPECT_TRUE(containers::NextMember(Id(f), Id(b)) == a);
	// after the last, or after a thing that is not a member: none
	EXPECT_TRUE(containers::NextMember(Id(f), Id(a)) == entt::null);
	const auto stranger = MakeVillager({});
	EXPECT_TRUE(containers::NextMember(Id(f), Id(stranger)) == entt::null);
	// no thing to follow, or one that has gone: the first again
	EXPECT_TRUE(containers::NextMember(Id(f), 0) == c);
	const auto gone = Reg().Create();
	Reg().Destroy(gone);
	EXPECT_TRUE(containers::NextMember(Id(f), Id(gone)) == c);
	// not a container ("Invalid container"): none
	EXPECT_TRUE(containers::FirstMember(Id(a)) == entt::null);
	EXPECT_TRUE(containers::NextMember(Id(a), Id(b)) == entt::null);
	EXPECT_TRUE(containers::FirstMember(0) == entt::null);
	EXPECT_TRUE(containers::NextMember(0, Id(b)) == entt::null);
}

TEST_F(FlocksTest, FirstInATownLooksOnlyInItsFirstAbodeAndNextGoesOnToTheHomeless)
{
	constexpr uint32_t k_TownId = 3;
	const auto town = Reg().Create();
	Reg().Assign<Town>(town, k_TownId);
	const auto MakeAbode = [](uint32_t made) {
		const auto e = Reg().Create();
		Reg().Assign<Abode>(e).townId = k_TownId;
		Reg().Assign<ecs::components::ObjectCreationIndex>(e, made);
		return e;
	};
	const auto older = MakeAbode(1);
	const auto newer = MakeAbode(2); // the first of the town's abodes: the newest
	const auto x = MakeVillager({});
	const auto y = MakeVillager({});
	const auto z = MakeVillager({});
	const auto h = MakeVillager({});
	Reg().Get<Abode>(older).inhabitants = {x};
	Reg().Get<Town>(town).homelessVillagers = {h};
	// the first abode has no one: the first homeless, though the other abode has someone
	EXPECT_TRUE(containers::FirstMember(Id(town)) == h);
	EXPECT_TRUE(containers::NextMember(Id(town), Id(h)) == entt::null);
	EXPECT_TRUE(containers::NextMember(Id(town), Id(x)) == h);
	// the abodes' inhabitants each from the head, abode by abode, then the homeless
	Reg().Get<Abode>(newer).inhabitants = {y, z};
	EXPECT_TRUE(containers::FirstMember(Id(town)) == y);
	EXPECT_TRUE(containers::NextMember(Id(town), Id(y)) == z);
	EXPECT_TRUE(containers::NextMember(Id(town), Id(z)) == x);
	EXPECT_TRUE(containers::NextMember(Id(town), Id(x)) == h);
	// no one at all: none
	Reg().Get<Abode>(older).inhabitants.clear();
	Reg().Get<Abode>(newer).inhabitants.clear();
	Reg().Get<Town>(town).homelessVillagers.clear();
	EXPECT_TRUE(containers::FirstMember(Id(town)) == entt::null);
}

TEST_F(FlocksTest, FirstInADanceIsItsFirstGroupsFirstDancerAndThereIsNoNext)
{
	const auto d = MakeDance(Reg());
	EXPECT_TRUE(containers::FirstMember(Id(d)) == entt::null); // no dancer yet
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, a, DanceGroup::k_Men));
	ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, b, DanceGroup::k_Men));
	EXPECT_TRUE(containers::FirstMember(Id(d)) == a);
	// the next after a dancer: none, even with another dancer there
	EXPECT_TRUE(containers::NextMember(Id(d), Id(a)) == entt::null);
	// with nothing to follow: the first
	EXPECT_TRUE(containers::NextMember(Id(d), 0) == a);
}

TEST_F(FlocksTest, TownAndVillagerHealthTotalAddsTheAbodesAndTheVillagers)
{
	const auto town = Reg().Create();
	Reg().Assign<Town>(town, 3u);
	const auto villager = [](float life) {
		const auto e = MakeVillager(glm::vec3(0.0f));
		Reg().Get<Villager>(e).life = life;
		return e;
	};
	// an abode with two inhabitants and a life of its own, a newer one with none and no life set (a full 1), and one
	// homeless villager
	const auto older = Reg().Create();
	auto& olderAbode = Reg().Assign<Abode>(older);
	olderAbode.townId = 3;
	olderAbode.inhabitants = {villager(0.5f), villager(0.25f)};
	Reg().Assign<ObjectCreationIndex>(older, 10u);
	Reg().Assign<Life>(older, 0.75f);
	const auto newer = Reg().Create();
	Reg().Assign<Abode>(newer).townId = 3;
	Reg().Assign<ObjectCreationIndex>(newer, 11u);
	ecs::town_villagers::AddToHomelessList(town, villager(0.125f));
	// an abode of another town does not count
	const auto other = Reg().Create();
	auto& otherAbode = Reg().Assign<Abode>(other);
	otherAbode.townId = 4;
	otherAbode.inhabitants = {villager(1.0f)};

	EXPECT_EQ(containers::TownAndVillagerHealthTotal(town).value_or(0.0f), 2.625f);
	// not a town
	EXPECT_FALSE(containers::TownAndVillagerHealthTotal(older).has_value());
}

TEST_F(FlocksTest, AdultSizeAndCapacityCountATownsOrAnAbodesAdultsAndPlaces)
{
	// one home record: room for 6 adults and 3 children
	auto info = std::make_unique<InfoConstants>();
	auto& home = info->abode.at(0);
	home.abodeNumber = AbodeNumber::A;
	home.tribeType = Tribe::CELTIC;
	home.maxVillagersInAbode = 6;
	home.maxChildrenInAbode = 3;
	Locator::infoConstants::reset(info.release());
	const auto town = Reg().Create();
	Reg().Assign<Town>(town, 1u).stats.adults = 20;
	Reg().Assign<Tribe>(town, Tribe::CELTIC);
	Reg().Context().towns[1] = town;
	const auto MakeHome = [](uint8_t adults, bool counted) {
		const auto e = Reg().Create();
		auto& abode = Reg().Assign<Abode>(e, AbodeNumber::A, 1u, 0u, 0u);
		abode.adultCount = adults;
		abode.addedToTownStats = counted;
		return e;
	};
	const auto home1 = MakeHome(4, true);
	const auto home2 = MakeHome(5, true);
	// not yet made functional: not one of the town's places, but an abode all the same
	const auto plan = MakeHome(0, false);
	// the town: its adults, whether housed or not, and the places of its counted homes
	EXPECT_EQ(containers::AdultSize(Id(town)), 20.0f);
	EXPECT_EQ(containers::AdultCapacity(Id(town)), 12.0f);
	EXPECT_EQ(containers::Capacity(Id(town)), 18.0f);
	// an abode: its adults and its own room
	EXPECT_EQ(containers::AdultSize(Id(home1)), 4.0f);
	EXPECT_EQ(containers::AdultSize(Id(home2)), 5.0f);
	EXPECT_EQ(containers::AdultCapacity(Id(home1)), 6.0f);
	EXPECT_EQ(containers::Capacity(Id(home1)), 9.0f);
	EXPECT_EQ(containers::AdultSize(Id(plan)), 0.0f);
	EXPECT_EQ(containers::AdultCapacity(Id(plan)), 6.0f);
	// a home made functional adds its places at once, before the town's next turn
	Reg().Get<Abode>(plan).addedToTownStats = true;
	EXPECT_EQ(containers::AdultCapacity(Id(town)), 18.0f);
	// a thing that is neither a town nor an abode, no thing and a thing gone: 0
	const auto villager = MakeVillager({});
	EXPECT_EQ(containers::AdultSize(Id(villager)), 0.0f);
	EXPECT_EQ(containers::Capacity(Id(villager)), 0.0f);
	EXPECT_EQ(containers::AdultCapacity(0), 0.0f);
	const auto gone = Reg().Create();
	Reg().Destroy(gone);
	EXPECT_EQ(containers::AdultSize(Id(gone)), 0.0f);
}

TEST_F(FlocksTest, AFootballPitchsSizeIsItsTeamsPlayers)
{
	// two records: a pitch and a home
	auto info = std::make_unique<InfoConstants>();
	info->abode.at(0).abodeType = AbodeType::FootballPitch;
	info->abode.at(1).abodeType = AbodeType::LivingQuarters;
	Locator::infoConstants::reset(info.release());
	const auto MakeAbodeOf = [](AbodeInfo record) {
		const auto e = Reg().Create();
		Reg().Assign<Abode>(e, AbodeNumber::A, 1u, 0u, 0u).info = record;
		return e;
	};
	// ID_SIZE of a pitch: its home and away teams' players, none while no match is played
	const auto pitch = MakeAbodeOf(static_cast<AbodeInfo>(0));
	ASSERT_TRUE(containers::Size(Id(pitch)).has_value());
	EXPECT_EQ(containers::Size(Id(pitch)).value_or(-1.0f), 0.0f);
	// another abode has no size
	const auto home = MakeAbodeOf(static_cast<AbodeInfo>(1));
	EXPECT_FALSE(containers::Size(Id(home)).has_value());
}

namespace
{
/// What a script's creation of an animal did, in order, with a fake animal numbered 7 (0 when the species makes none)
/// and a fake nearest town numbered 3
std::vector<std::string> CreateAnimalLog(int made)
{
	namespace rules = openblack::ecs::animal_script_rules;
	std::vector<std::string> log;
	const int result = rules::CreateScriptAnimal(
	    0,
	    [&log, made]() {
		    log.emplace_back("create");
		    return made;
	    },
	    [&log](int animal, uint16_t radius, uint16_t distance) {
		    log.push_back("domain " + std::to_string(animal) + " " + std::to_string(radius) + " " + std::to_string(distance));
	    },
	    [&log](int animal, float radius) {
		    log.push_back("nearest " + std::to_string(animal) + " " + std::to_string(static_cast<int>(radius)));
		    return 3;
	    },
	    [&log](int animal, int town) { log.push_back("town " + std::to_string(animal) + " " + std::to_string(town)); },
	    [&log](int animal, LivingStates state) {
		    log.push_back("state " + std::to_string(animal) + " " + std::to_string(static_cast<int>(state)));
	    });
	log.push_back("made " + std::to_string(result));
	return log;
}
} // namespace

TEST(AnimalScriptRules, AScriptsAnimalKeepsToASmallDomainTakesTheNearestTownThenIsHeld)
{
	// its own flock's domain radius 2 and flock distance 1, the nearest town within 500 m, then IN_SCRIPT (4): it
	// keeps still until the script moves it or lets it go
	EXPECT_EQ(CreateAnimalLog(7),
	          (std::vector<std::string> {"create", "domain 7 2 1", "nearest 7 500", "town 7 3", "state 7 4", "made 7"}));
}

TEST(AnimalScriptRules, NothingMoreWhenTheSpeciesMakesNoAnimal)
{
	EXPECT_EQ(CreateAnimalLog(0), (std::vector<std::string> {"create", "made 0"}));
}

TEST_F(FlocksTest, TheNativesThatTakeControlAreTheGamesFourteen)
{
	// the script state and its values, MOVE_GAME_THING, FLOCK_ATTACH, POPULATE_CONTAINER, WALK_PATH, SWAP_CREATURE and the
	// five game commands
	EXPECT_EQ(ecs::script_held::k_ControllingNatives,
	          (std::array<uint32_t, 14> {17, 18, 19, 20, 33, 37, 109, 177, 210, 214, 215, 216, 217, 218}));
	EXPECT_TRUE(ecs::script_held::TakesControl(17));
	EXPECT_TRUE(ecs::script_held::TakesControl(37));
	EXPECT_TRUE(ecs::script_held::TakesControl(218));
	// GAME_THING_CLICKED, SET_FOCUS, FLOCK_CREATE, FLOCK_DETACH, FLOCK_DISBAND, DANCE_CREATE, CALL_IN and
	// RELEASE_FROM_SCRIPT do not
	for (const uint32_t native : {16u, 34u, 36u, 38u, 39u, 54u, 55u, 159u})
	{
		EXPECT_FALSE(ecs::script_held::TakesControl(native)) << native;
	}
}

TEST_F(FlocksTest, ANativeThatTakesControlTakesItOfWhatItIsGiven)
{
	ecs::systems::ScriptObjectsSystem objects;
	// a villager a finder handed to the script and a variable keeps: in the script, not under its control
	const auto a = MakeVillager({});
	ecs::script_held::AddScriptThing(a, false);
	ecs::script_held::IncrementReference(a);
	EXPECT_TRUE(ecs::script_held::IsInScript(a));
	EXPECT_FALSE(ecs::script_held::IsControlledByScript(a));
	// CALL_IN only finds it: it stays as it is
	objects.EnterNative(55);
	EXPECT_EQ(objects.Fetch(a), a);
	EXPECT_FALSE(ecs::script_held::IsControlledByScript(a));
	// FLOCK_ATTACH takes control of it, and keeps its one reference
	objects.EnterNative(37);
	EXPECT_EQ(objects.Fetch(a), a);
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(a));
	EXPECT_EQ(Reg().Get<const ScriptHeld>(a).references, 1);
	// a thing given straight to the native, which no variable keeps, takes a slot with no reference, so that the end of
	// the turn gives it back to the game
	const auto b = MakeVillager({});
	EXPECT_EQ(objects.Fetch(b), b);
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(b));
	ASSERT_TRUE(Reg().AllOf<ScriptHeld>(b));
	EXPECT_TRUE(Reg().Get<const ScriptHeld>(b).hasSlot);
	EXPECT_FALSE(Reg().Get<const ScriptHeld>(b).createdByScript);
	EXPECT_EQ(Reg().Get<const ScriptHeld>(b).references, 0);
	// a new land: no native runs yet, so nothing is taken
	objects.ClearForNewLand();
	const auto c = MakeVillager({});
	EXPECT_EQ(objects.Fetch(c), c);
	EXPECT_FALSE(ecs::script_held::IsControlledByScript(c));
}

TEST_F(FlocksTest, ADancerTheAttachTookControlOfIsTheScriptsToGiveBackAtTheDisband)
{
	// The intro's welcome crowd: each villager is found, kept in a variable, attached (FLOCK_ATTACH takes control of it)
	// and the variable moves on to the next one. At the disband it lets go of its last reference and is still controlled,
	// so the end of the turn gives it back to the game
	const auto d = MakeDance(Reg());
	const auto taken = MakeVillager({});
	const auto found = MakeVillager({});
	for (const auto villager : {taken, found})
	{
		ecs::script_held::AddScriptThing(villager, false);
		ecs::script_held::IncrementReference(villager);
	}
	ecs::script_held::TakeControl(taken);
	for (const auto villager : {taken, found})
	{
		// the attach: a dancer, holding a reference; then the variable lets go of it
		ASSERT_TRUE(ecs::dances::AddDancer(Reg(), d, villager, DanceGroup::k_Men));
		ecs::script_held::IncrementReference(villager);
		ecs::script_held::DecrementReference(villager);
		EXPECT_EQ(Reg().Get<const ScriptHeld>(villager).references, 1);
	}
	containers::Disband(Id(d));
	for (const auto villager : {taken, found})
	{
		EXPECT_EQ(ecs::dances::DanceOf(Reg(), villager), entt::entity {entt::null});
		EXPECT_EQ(Reg().Get<const ScriptHeld>(villager).references, 0);
	}
	// the one the attach took is the script's to give back; one no native took control of is left as it is
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(taken));
	EXPECT_FALSE(ecs::script_held::IsControlledByScript(found));
}

namespace
{
/// The flock natives as a script calls them through the natives' table: before each one the scripts' objects learn
/// which native runs, as the game's script machine tells them, so that FLOCK_ATTACH takes control of what it is given
class FlockNatives: public FlocksTest
{
protected:
	void SetUp() override
	{
		FlocksTest::SetUp();
		Locator::vm::emplace<lhvm::LHVM>();
		Locator::scriptObjects::emplace<ecs::systems::ScriptObjectsSystem>();
	}

	/// The native `name`, its arguments pushed by `push` on top of a value of the script's own; the value it gives when
	/// it `gives` one, which must be the only one it leaves above the script's
	lhvm::VMValue Call(std::string_view name, bool gives, const std::function<void(lhvm::LHVM&)>& push)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end()) << name;
		if (native == table.end())
		{
			return {};
		}
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		push(vm);
		Locator::scriptObjects::value().EnterNative(static_cast<uint32_t>(std::distance(table.begin(), native)));
		native->impl();
		const auto given = gives ? vm.Pop() : lhvm::VMValue {};
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left or took a value";
		return given;
	}

	/// FLOCK_ATTACH: the obj is pushed first, then the target and the leader flag. The id it gives
	uint32_t Attach(entt::entity object, entt::entity target, bool asLeader)
	{
		return Call("FLOCK_ATTACH", true,
		            [object, target, asLeader](lhvm::LHVM& vm) {
			            vm.Pusho(Id(object));
			            vm.Pusho(Id(target));
			            vm.Pushb(asLeader);
		            })
		    .uintVal;
	}

	void Disband(entt::entity container)
	{
		Call("FLOCK_DISBAND", false, [container](lhvm::LHVM& vm) { vm.Pusho(Id(container)); });
	}

	static entt::entity MakeAnimal(AnimalInfo species)
	{
		const auto e = Reg().Create();
		Reg().Assign<Animal>(e, species, 0u);
		Reg().Assign<Transform>(e, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<AnimalBrain>(e);
		return e;
	}

	static uint8_t StateOf(entt::entity animal) { return Reg().Get<const AnimalBrain>(animal).topState; }
	static int ReferencesTo(entt::entity thing) { return Reg().Get<const ScriptHeld>(thing).references; }

	static constexpr int32_t k_ScriptsOwn = 7777;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(FlockNatives, AttachAddsAVillagerOrAnAnimalToAFlockAsLeaderOrNotAndGivesTheFlock)
{
	// A living and a flock: AddMember, or AddLeader as leader, then the MOVE_IN_FLOCK script state either way, the
	// flock's id pushed and the living's reference incremented; the obj and the target are taken under the script's
	// control as they are popped (docs/wiki/beings/villager-speed-and-scripts.md#script-flocks-and-containers)
	const auto flock = containers::CreateFlock({50.0f, 0.0f, 50.0f});
	const auto villager = MakeVillager({});
	EXPECT_EQ(Attach(villager, flock, false), Id(flock));
	EXPECT_TRUE(flocks::FlockOf(villager) == flock);
	EXPECT_EQ(ReferencesTo(villager), 1);
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(villager));
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(flock));
	// an animal as its leader: the flock's tail, in the flock state
	const auto sheep = MakeAnimal(AnimalInfo::Sheep);
	EXPECT_EQ(Attach(sheep, flock, true), Id(flock));
	EXPECT_TRUE(flocks::Leader(flock) == sheep);
	EXPECT_EQ(StateOf(sheep), static_cast<uint8_t>(ecs::animal_ai::AnimalState::MoveInFlock));
	EXPECT_EQ(ReferencesTo(sheep), 1);
	// another animal, not as the leader: a member before the head, the leader kept, in the flock state too
	const auto cow = MakeAnimal(AnimalInfo::Cow);
	EXPECT_EQ(Attach(cow, flock, false), Id(flock));
	EXPECT_TRUE(flocks::Leader(flock) == sheep);
	EXPECT_EQ(flocks::MembersFromHead(flock), (std::vector<entt::entity> {cow, villager, sheep}));
	EXPECT_EQ(StateOf(cow), static_cast<uint8_t>(ecs::animal_ai::AnimalState::MoveInFlock));
}

TEST_F(FlockNatives, AttachOfAFlockToAFlockMergesItIntoTheTarget)
{
	// Two flocks merge into the target, and the target is pushed
	// (docs/wiki/beings/villager-speed-and-scripts.md#script-flocks-and-containers)
	const auto from = containers::CreateFlock({});
	const auto into = containers::CreateFlock({});
	const auto a = MakeVillager({});
	const auto b = MakeVillager({});
	flocks::AddMember(from, a);
	flocks::AddMember(into, b);
	EXPECT_EQ(Attach(from, into, false), Id(into));
	EXPECT_TRUE(flocks::FlockOf(a) == into);
	EXPECT_TRUE(flocks::FlockOf(b) == into);
	EXPECT_EQ(flocks::Size(into), 2u);
}

TEST_F(FlockNatives, AttachOfNothingOrToAThingThatIsNoContainerAddsNothing)
{
	// No obj, no target, or a target that is not a dance, a flock or a town: nothing is added
	// (docs/wiki/beings/villager-speed-and-scripts.md#script-flocks-and-containers)
	const auto flock = containers::CreateFlock({});
	const auto villager = MakeVillager({});
	const auto other = MakeVillager({});
	EXPECT_EQ(Attach(entt::entity {0}, flock, false), 0u);
	EXPECT_EQ(Attach(villager, entt::entity {0}, false), 0u);
	EXPECT_EQ(Attach(villager, other, false), 0u);
	EXPECT_TRUE(flocks::FlockOf(villager) == entt::null);
	EXPECT_TRUE(flocks::FlockOf(other) == entt::null);
	EXPECT_EQ(flocks::Size(flock), 0u);
}

TEST_F(FlockNatives, DisbandLetsEveryMemberGoAnAnimalIntoAFlockOfItsOwn)
{
	// From the head: an animal gets a flock of its own and a villager is only removed; each lets go of its reference
	// and a controlled one gets the IN_SCRIPT state; the flock is not deleted
	// (docs/wiki/beings/villager-speed-and-scripts.md#script-flocks-and-containers)
	const auto flock = containers::CreateFlock({});
	const auto villager = MakeVillager({});
	const auto sheep = MakeAnimal(AnimalInfo::Sheep);
	ASSERT_EQ(Attach(villager, flock, false), Id(flock));
	ASSERT_EQ(Attach(sheep, flock, false), Id(flock));
	Disband(flock);
	EXPECT_TRUE(flocks::FlockOf(villager) == entt::null);
	const auto own = flocks::FlockOf(sheep);
	EXPECT_TRUE(own != entt::null && own != flock);
	EXPECT_EQ(StateOf(sheep), static_cast<uint8_t>(ecs::animal_ai::AnimalState::InScript));
	EXPECT_EQ(ReferencesTo(villager), 0);
	EXPECT_EQ(ReferencesTo(sheep), 0);
	EXPECT_TRUE(Reg().Valid(flock));
	EXPECT_EQ(flocks::Size(flock), 0u);
}

TEST_F(FlockNatives, DisbandOfATownOrOfNothingChangesNothing)
{
	// A town or an abode: nothing (docs/wiki/beings/villager-speed-and-scripts.md#script-flocks-and-containers)
	constexpr uint32_t k_TownId = 5;
	const auto town = Reg().Create();
	Reg().Assign<Town>(town, k_TownId);
	const auto villager = MakeVillager({});
	Reg().Get<Town>(town).homelessVillagers = {villager};
	Disband(town);
	EXPECT_EQ(Reg().Get<const Town>(town).homelessVillagers, std::vector<entt::entity> {villager});
	Disband(entt::entity {0});
	EXPECT_TRUE(Reg().Valid(town));
}

TEST_F(FlockNatives, ASpeedSetOnAFlockIsSetOnEachMember)
{
	// SET_PROPERTY on a town, a flock or a dance hands the property to each member: a flock's villager and animal take
	// the speed in metres, 0.5 m being 3276 whole units (docs/wiki/scripts/script-commands.md#properties-and-flags)
	const auto flock = containers::CreateFlock({});
	const auto villager = MakeVillager({});
	Reg().Assign<WallHug>(villager, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
	const auto sheep = MakeAnimal(AnimalInfo::Sheep);
	flocks::AddMember(flock, villager);
	flocks::AddMember(flock, sheep);
	const auto setSpeed = [this](entt::entity thing, float speed) {
		Call("SET_PROPERTY", false, [thing, speed](lhvm::LHVM& vm) {
			vm.Pushi(static_cast<int32_t>(script::ObjectPropertyType::Speed));
			vm.Pusho(Id(thing));
			vm.Pushf(speed);
		});
	};
	setSpeed(flock, 0.5f);
	EXPECT_EQ(ecs::VillagerSpeedInMetres(villager), map_coords::ToMetres(3276));
	EXPECT_EQ(Reg().Get<const AnimalBrain>(sheep).speed, 3276);
	// a thing that keeps no speed is left as it is, and so is a thing that is gone
	const auto other = Reg().Create();
	Reg().Assign<Transform>(other, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	setSpeed(other, 0.7f);
	setSpeed(entt::entity {0}, 0.7f);
	EXPECT_EQ(ecs::VillagerSpeedInMetres(villager), map_coords::ToMetres(3276));
}

TEST_F(FlockNatives, SetIndestructableMarksEachMemberOfATownOrAFlockAndAnyOtherThingItself)
{
	// SET_INDESTRUCTABLE pops the thing, then the flag. A town or a flock hands it to each of its members (a town's
	// villagers in its abodes and its homeless) and stays as it is itself; any other thing takes or loses the mark
	// itself; no thing changes nothing (docs/wiki/scripts/script-commands.md)
	const auto SetIndestructable = [this](entt::entity thing, bool indestructible) {
		Call("SET_INDESTRUCTABLE", false, [thing, indestructible](lhvm::LHVM& vm) {
			vm.Pushb(indestructible);
			vm.Pusho(Id(thing));
		});
	};
	const auto Marked = [](entt::entity e) { return Reg().AllOf<Indestructible>(e); };
	constexpr uint32_t k_TownId = 6;
	const auto town = Reg().Create();
	Reg().Assign<Town>(town, k_TownId);
	const auto abode = Reg().Create();
	Reg().Assign<Abode>(abode).townId = k_TownId;
	Reg().Assign<ecs::components::ObjectCreationIndex>(abode, 1u);
	const auto inside = MakeVillager({});
	const auto homeless = MakeVillager({});
	Reg().Get<Abode>(abode).inhabitants = {inside};
	Reg().Get<Town>(town).homelessVillagers = {homeless};
	SetIndestructable(town, true);
	EXPECT_TRUE(Marked(inside));
	EXPECT_TRUE(Marked(homeless));
	EXPECT_FALSE(Marked(town));
	EXPECT_FALSE(Marked(abode));
	SetIndestructable(town, false);
	EXPECT_FALSE(Marked(inside));
	EXPECT_FALSE(Marked(homeless));

	const auto flock = containers::CreateFlock({});
	const auto sheep = MakeAnimal(AnimalInfo::Sheep);
	const auto cow = MakeAnimal(AnimalInfo::Cow);
	flocks::AddMember(flock, sheep);
	flocks::AddMember(flock, cow);
	SetIndestructable(flock, true);
	EXPECT_TRUE(Marked(sheep));
	EXPECT_TRUE(Marked(cow));
	EXPECT_FALSE(Marked(flock));

	const auto alone = MakeVillager({});
	SetIndestructable(alone, true);
	EXPECT_TRUE(Marked(alone));
	SetIndestructable(alone, false);
	EXPECT_FALSE(Marked(alone));
	SetIndestructable(entt::entity {0}, true);
	EXPECT_FALSE(Marked(entt::entity {0}));
}
