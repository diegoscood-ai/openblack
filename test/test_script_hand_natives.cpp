/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' natives about the player's hand, as a script calls them: GET_HAND_POSITION gives the hand's place, x
// then y then z; IS_SPELL_CHARGING and IS_THAT_SPELL_CHARGING ask whether any icon of a player's temple holds chants,
// of any miracle or of one; CLEAR_PLAYER_SPELL_CHARGING stops the hand's charges at that temple and gives their chants
// back to their worship site; IS_LOCKED_INTERACTION asks whether a thing is in the hand's locked select, and
// GET_INTERACTION_MAGNITUDE how strongly the hand last stroked or slapped a creature. docs/wiki/hand/picking.md,
// docs/wiki/magic/worship.md#the-players-view-of-its-icons, docs/wiki/hand/states.md#what-the-scripts-read-of-it,
// docs/wiki/creature/players-creature.md

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class ScriptHandNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::vm::emplace<lhvm::LHVM>();
		_hand = &static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
		_mind = &static_cast<test::creature_fakes::FakeMind&>(
		    Locator::creatureMindSystem::emplace<test::creature_fakes::FakeMind>());
		// the fire seed's plain miracle is the fireball, the storm seed's the lightning bolt
		auto info = std::make_unique<InfoConstants>();
		info->spellSeed.at(static_cast<size_t>(SpellSeedType::Fire)).magicTypes = {MagicType::Fireball, MagicType::None,
		                                                                           MagicType::None, MagicType::None};
		info->spellSeed.at(static_cast<size_t>(SpellSeedType::Storm)).magicTypes = {MagicType::LightningBolt, MagicType::None,
		                                                                            MagicType::None, MagicType::None};
		Locator::infoConstants::reset(info.release());
	}

	[[nodiscard]] ecs::Registry& Reg() const { return *_registry; }

	/// The player's temple with one worship site holding `battery` chants; returns the site
	entt::entity AddTempleWithSite(PlayerNames player, float battery)
	{
		const auto citadel = Reg().Create();
		Reg().Assign<Temple>(citadel, player);
		auto& worship = Reg().Assign<CitadelWorship>(citadel);
		const auto site = Reg().Create();
		worship.sites.at(0) = site;
		auto& component = Reg().Assign<WorshipSite>(site);
		component.citadel = citadel;
		component.player = player;
		component.battery = battery;
		return site;
	}

	/// An icon of the seed at the site, holding `stored` chants, charged by `chargingFor`'s hand when given
	entt::entity AddIcon(entt::entity site, SpellSeedType seed, float stored, std::optional<PlayerNames> chargingFor)
	{
		const auto icon = Reg().Create();
		const auto owner = Reg().Get<const WorshipSite>(site).player;
		Reg().Assign<SpellIcon>(icon, SpellIcon {.seedType = seed, .player = owner});
		Reg().Assign<WorshipSpellIcon>(icon, WorshipSpellIcon {
		                                         .site = site,
		                                         .charging = chargingFor.has_value(),
		                                         .chargingFor = chargingFor.value_or(PlayerNames::NEUTRAL),
		                                         .hasChargingInterface = chargingFor.has_value(),
		                                         .chantStore = stored,
		                                     });
		Reg().Get<WorshipSite>(site).icons.push_back(icon);
		return icon;
	}

	/// The native as a script calls it, with its arguments pushed by `push` and the stack counts the game gives it
	void Call(std::string_view name, int32_t in, uint32_t out, const std::function<void(lhvm::LHVM&)>& push)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		ASSERT_NE(native, table.end()) << name;
		EXPECT_EQ(native->stackIn, in) << name;
		EXPECT_EQ(native->stackOut, out) << name;
		auto& vm = Locator::vm::value();
		vm.Pushi(k_ScriptsOwn);
		push(vm);
		native->impl();
	}

	/// A native that gives one value of the type: the value, checked to be the only one above the script's own
	lhvm::VMValue CallGiving(std::string_view name, int32_t in, const std::function<void(lhvm::LHVM&)>& push,
	                         lhvm::DataType expectedType)
	{
		Call(name, in, 1, push);
		auto& vm = Locator::vm::value();
		lhvm::DataType type {};
		const auto given = vm.Pop(type);
		EXPECT_EQ(type, expectedType) << name;
		EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << name << " left more than one value";
		return given;
	}

	bool IsSpellCharging(float scriptPlayer)
	{
		return CallGiving(
		           "IS_SPELL_CHARGING", 1, [scriptPlayer](lhvm::LHVM& vm) { vm.Pushf(scriptPlayer); }, lhvm::DataType::Boolean)
		           .intVal != 0;
	}

	/// The player is pushed first and the miracle last
	bool IsThatSpellCharging(float scriptPlayer, MagicType type)
	{
		return CallGiving(
		           "IS_THAT_SPELL_CHARGING", 2,
		           [scriptPlayer, type](lhvm::LHVM& vm) {
			           vm.Pushf(scriptPlayer);
			           vm.Pushi(static_cast<int32_t>(type));
		           },
		           lhvm::DataType::Boolean)
		           .intVal != 0;
	}

	void ClearPlayerSpellCharging(float scriptPlayer)
	{
		Call("CLEAR_PLAYER_SPELL_CHARGING", 1, 0, [scriptPlayer](lhvm::LHVM& vm) { vm.Pushf(scriptPlayer); });
		EXPECT_EQ(Locator::vm::value().Pop().intVal, k_ScriptsOwn) << "CLEAR_PLAYER_SPELL_CHARGING took or left a value";
	}

	bool IsLockedInteraction(uint32_t object)
	{
		return CallGiving(
		           "IS_LOCKED_INTERACTION", 1, [object](lhvm::LHVM& vm) { vm.Pusho(object); }, lhvm::DataType::Boolean)
		           .intVal != 0;
	}

	float GetInteractionMagnitude(uint32_t object)
	{
		return CallGiving(
		           "GET_INTERACTION_MAGNITUDE", 1, [object](lhvm::LHVM& vm) { vm.Pusho(object); }, lhvm::DataType::Float)
		    .floatVal;
	}

	/// A thing the scripts can name by its id: the registry's first entity is made first, as its id 0 is the scripts'
	/// "no thing"
	entt::entity AddThing()
	{
		if (!_firstMade)
		{
			static_cast<void>(Reg().Create());
			_firstMade = true;
		}
		return Reg().Create();
	}

	[[nodiscard]] static uint32_t Id(entt::entity thing) { return static_cast<uint32_t>(thing); }

	static constexpr int32_t k_ScriptsOwn = 7777;
	/// The script's number of the first player (its game player is the script's number less one)
	static constexpr float k_ScriptPlayerOne = 1.0f;
	static constexpr float k_ScriptPlayerTwo = 2.0f;

	test::RestoreService<Locator::entitiesRegistry> _keepRegistry;
	test::RestoreService<Locator::vm> _keepVm;
	test::RestoreService<Locator::handSystem> _keepHand;
	test::RestoreService<Locator::creatureMindSystem> _keepMind;
	test::RestoreService<Locator::infoConstants> _keepInfo;
	chlapi::CHLApi _api;
	ecs::Registry* _registry {nullptr};
	test::creature_fakes::FakeHand* _hand {nullptr};
	test::creature_fakes::FakeMind* _mind {nullptr};
	bool _firstMade {false};
};
} // namespace

TEST_F(ScriptHandNatives, TheHandsPositionIsGivenXThenYThenZ)
{
	// the original pushes the hand object's own place as three vector values, x first
	const auto hand = Reg().Create();
	Reg().Assign<Transform>(hand, glm::vec3(1500.5f, 32.25f, 2250.75f), glm::mat3(1.0f), glm::vec3(1.0f));
	_hand->left = hand;
	Call("GET_HAND_POSITION", 0, 3, [](lhvm::LHVM&) {});
	auto& vm = Locator::vm::value();
	lhvm::DataType type {};
	EXPECT_EQ(vm.Pop(type).floatVal, 2250.75f);
	EXPECT_EQ(type, lhvm::DataType::Vector);
	EXPECT_EQ(vm.Pop(type).floatVal, 32.25f);
	EXPECT_EQ(type, lhvm::DataType::Vector);
	EXPECT_EQ(vm.Pop(type).floatVal, 1500.5f);
	EXPECT_EQ(type, lhvm::DataType::Vector);
	EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "GET_HAND_POSITION left more than three values";
}

TEST_F(ScriptHandNatives, ASpellIsChargingWhileAnIconOfThePlayersTempleHoldsChants)
{
	// the original asks every icon of the player's six worship sites for chants above zero: the icon's charging flag
	// is not read
	EXPECT_FALSE(IsSpellCharging(k_ScriptPlayerOne));
	const auto site = AddTempleWithSite(PlayerNames::PLAYER_ONE, 0.0f);
	const auto icon = AddIcon(site, SpellSeedType::Fire, 0.0f, PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(IsSpellCharging(k_ScriptPlayerOne));
	Reg().Get<WorshipSpellIcon>(icon).chantStore = 0.5f;
	EXPECT_TRUE(IsSpellCharging(k_ScriptPlayerOne));
	// chants kept in an icon no hand charges still count
	auto& charge = Reg().Get<WorshipSpellIcon>(icon);
	charge.charging = false;
	charge.hasChargingInterface = false;
	EXPECT_TRUE(IsSpellCharging(k_ScriptPlayerOne));
	// another player's temple
	EXPECT_FALSE(IsSpellCharging(k_ScriptPlayerTwo));
}

TEST_F(ScriptHandNatives, ThatSpellIsChargingWhenTheIconOfItsSeedHoldsChants)
{
	// the icon's miracle is its seed's first one; the script pushes the player, then the miracle
	const auto site = AddTempleWithSite(PlayerNames::PLAYER_ONE, 0.0f);
	const auto fire = AddIcon(site, SpellSeedType::Fire, 100.0f, PlayerNames::PLAYER_ONE);
	AddIcon(site, SpellSeedType::Storm, 0.0f, std::nullopt);
	EXPECT_TRUE(IsThatSpellCharging(k_ScriptPlayerOne, MagicType::Fireball));
	EXPECT_FALSE(IsThatSpellCharging(k_ScriptPlayerOne, MagicType::LightningBolt));
	// a power-up of the seed is not the icon's miracle
	EXPECT_FALSE(IsThatSpellCharging(k_ScriptPlayerOne, MagicType::FireballPowerUpOne));
	EXPECT_FALSE(IsThatSpellCharging(k_ScriptPlayerTwo, MagicType::Fireball));
	Reg().Get<WorshipSpellIcon>(fire).chantStore = 0.0f;
	EXPECT_FALSE(IsThatSpellCharging(k_ScriptPlayerOne, MagicType::Fireball));
}

TEST_F(ScriptHandNatives, ClearingThePlayersChargingGivesTheChantsBackToTheSite)
{
	// each icon the hand charges stops, back at the plain miracle, and its chants go back into its site's battery; an
	// icon no hand charges keeps its chants
	const auto site = AddTempleWithSite(PlayerNames::PLAYER_ONE, 20.0f);
	const auto charged = AddIcon(site, SpellSeedType::Fire, 300.0f, PlayerNames::PLAYER_ONE);
	Reg().Get<WorshipSpellIcon>(charged).powerUp = 1;
	const auto kept = AddIcon(site, SpellSeedType::Storm, 50.0f, std::nullopt);
	ClearPlayerSpellCharging(k_ScriptPlayerOne);
	const auto& stopped = Reg().Get<const WorshipSpellIcon>(charged);
	EXPECT_FALSE(stopped.charging);
	EXPECT_FALSE(stopped.hasChargingInterface);
	EXPECT_EQ(stopped.powerUp, -1);
	EXPECT_EQ(stopped.chantStore, 0.0f);
	EXPECT_EQ(Reg().Get<const WorshipSite>(site).battery, 320.0f);
	EXPECT_EQ(Reg().Get<const WorshipSpellIcon>(kept).chantStore, 50.0f);
	// another player's temple is not touched
	const auto other = AddTempleWithSite(PlayerNames::PLAYER_TWO, 0.0f);
	const auto theirs = AddIcon(other, SpellSeedType::Fire, 70.0f, PlayerNames::PLAYER_TWO);
	ClearPlayerSpellCharging(k_ScriptPlayerOne);
	EXPECT_TRUE(Reg().Get<const WorshipSpellIcon>(theirs).charging);
	EXPECT_EQ(Reg().Get<const WorshipSpellIcon>(theirs).chantStore, 70.0f);
}

TEST_F(ScriptHandNatives, ClearingAnotherPlayersChargingStopsOnlyTheLocalHandsChargesAtItsTemple)
{
	// at the second player's temple: the icon this computer's hand charges stops, the one its own player charges goes on
	const auto site = AddTempleWithSite(PlayerNames::PLAYER_TWO, 0.0f);
	const auto mine = AddIcon(site, SpellSeedType::Fire, 40.0f, PlayerNames::PLAYER_ONE);
	const auto theirs = AddIcon(site, SpellSeedType::Storm, 60.0f, PlayerNames::PLAYER_TWO);
	ClearPlayerSpellCharging(k_ScriptPlayerTwo);
	EXPECT_FALSE(Reg().Get<const WorshipSpellIcon>(mine).charging);
	EXPECT_EQ(Reg().Get<const WorshipSpellIcon>(mine).chantStore, 0.0f);
	EXPECT_TRUE(Reg().Get<const WorshipSpellIcon>(theirs).charging);
	EXPECT_EQ(Reg().Get<const WorshipSpellIcon>(theirs).chantStore, 60.0f);
}

TEST_F(ScriptHandNatives, AThingIsLockedInAnInteractionWhileItIsInTheHandsLockedSelect)
{
	// the original pushes the thing's locked flag as a boolean, whatever the thing is: set for the object of the hand's
	// locked select from its start until its end
	const auto creature = AddThing();
	Reg().Assign<Creature>(creature);
	const auto pile = AddThing();
	EXPECT_FALSE(IsLockedInteraction(Id(creature)));
	_hand->lockedSelect = creature;
	EXPECT_TRUE(IsLockedInteraction(Id(creature)));
	EXPECT_FALSE(IsLockedInteraction(Id(pile)));
	_hand->lockedSelect = pile;
	EXPECT_TRUE(IsLockedInteraction(Id(pile)));
	EXPECT_FALSE(IsLockedInteraction(Id(creature)));
	// no thing: its error message and false
	EXPECT_FALSE(IsLockedInteraction(0));
	Reg().Destroy(pile);
	EXPECT_FALSE(IsLockedInteraction(Id(pile)));
}

TEST_F(ScriptHandNatives, TheInteractionMagnitudeIsHowStronglyTheCreatureWasLastRewardedOrPunished)
{
	// the original pushes the creature mind's last feedback as a float: above 0 a stroke, below 0 a slap
	const auto creature = AddThing();
	Reg().Assign<Creature>(creature);
	EXPECT_EQ(GetInteractionMagnitude(Id(creature)), 0.0f);
	_mind->interactionMagnitudes[creature] = 0.75f;
	EXPECT_EQ(GetInteractionMagnitude(Id(creature)), 0.75f);
	_mind->interactionMagnitudes[creature] = -2.5f;
	EXPECT_EQ(GetInteractionMagnitude(Id(creature)), -2.5f);
	// a thing that is not a creature ("Thing not creature!") and no thing ("Thing not found!") give 0
	const auto rock = AddThing();
	_mind->interactionMagnitudes[rock] = 1.0f;
	EXPECT_EQ(GetInteractionMagnitude(Id(rock)), 0.0f);
	EXPECT_EQ(GetInteractionMagnitude(0), 0.0f);
}
