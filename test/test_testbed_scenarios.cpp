/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <SDL_scancode.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/FlatLand.h"
#include "3D/SkyType.h"
#include "Camera/NearClipping.h"
#include "Common/HelpText.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureLayers.h"
#include "Debug/TestbedDispenserGrid.h"
#include "Debug/TestbedFixtures.h"
#include "Debug/TestbedHeldKeys.h"
#include "Debug/TestbedScenarioRegistry.h"
#include "ECS/PlayerColours.h"
#include "ECS/VortexSave.h"
#include "Graphics/Moon.h"
#include "Graphics/Sun.h"
#include "Input/KeyBindings.h"
#include "ScriptHeaders/ScriptEnums.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
constexpr float k_Epsilon = 1e-4f;

Command Wait(size_t creature, float delay, bool untilFree)
{
	return {.kind = Command::Kind::Stop, .creature = creature, .delaySeconds = delay, .waitUntilFree = untilFree};
}

const auto k_AlwaysFree = [](const Command& /*command*/) { return true; };
} // namespace

TEST(TestbedScenarios, IdsAreUniqueAndFindable)
{
	std::set<std::string_view> ids;
	for (const auto& scenario : All())
	{
		EXPECT_TRUE(ids.insert(scenario.id).second) << "Duplicate id " << scenario.id;
		EXPECT_EQ(Find(scenario.id), &scenario);
	}
	EXPECT_EQ(Find("no.such.scenario"), nullptr);
}

TEST(TestbedScenarios, EveryFacetHasScenarios)
{
	std::array<size_t, k_FacetCount> counts {};
	for (const auto& scenario : All())
	{
		++counts.at(static_cast<size_t>(scenario.facet));
	}
	for (size_t i = 0; i < k_FacetCount; ++i)
	{
		EXPECT_GT(counts.at(i), 0u) << "No scenario shows " << Name(static_cast<Facet>(i));
		EXPECT_FALSE(Name(static_cast<Facet>(i)).empty());
	}
}

TEST(TestbedScenarios, EveryScenarioIsWellFormed)
{
	for (const auto& scenario : All())
	{
		const auto problems = Problems(scenario);
		std::string all;
		for (const auto& problem : problems)
		{
			all += problem + "\n";
		}
		EXPECT_TRUE(problems.empty()) << scenario.id << ":\n" << all;
		// Ids name their facet, so the list and the command line agree
		EXPECT_EQ(scenario.id.find('.'), scenario.id.find_first_of('.'));
		EXPECT_NE(scenario.id.find('.'), std::string_view::npos) << scenario.id;
	}
}

TEST(TestbedScenarios, CoversTheCreatureFeatures)
{
	// The scenarios asked for by name, which later work must keep
	for (const auto* id : {"idle.fidget",
	                       "idle.hang_around",
	                       "expressions.desire_emotes",
	                       "expressions.faces_gestures",
	                       "senses.watch_walker",
	                       "needs.thirst",
	                       "needs.hunger",
	                       "needs.starving",
	                       "needs.sleep_day",
	                       "needs.sleep_night",
	                       "needs.poo",
	                       "needs.puke",
	                       "needs.exhaustion",
	                       "needs.cold",
	                       "growth.timelapse",
	                       "growth.morph_lineup",
	                       "appearance.alignment_skins",
	                       "growth.species_parade",
	                       "appearance.tattoos_wounds",
	                       "appearance.eyes",
	                       "light.shadows",
	                       "light.reflections",
	                       "light.rain_splashes",
	                       "light.rain_splashes_night",
	                       "movement.course",
	                       "movement.route_obstacles",
	                       "movement.route_round_rocks",
	                       "movement.trampling",
	                       "footprints.trail",
	                       "footprints.april_fools",
	                       "audio.surfaces",
	                       "audio.voices"})
	{
		EXPECT_NE(Find(id), nullptr) << id;
	}
	// Every species in the parade
	const auto* parade = Find("growth.species_parade");
	ASSERT_NE(parade, nullptr);
	std::set<CreatureType> species;
	for (const auto& creature : parade->creatures)
	{
		species.insert(creature.species);
	}
	EXPECT_EQ(species.size(), static_cast<size_t>(CreatureType::_COUNT) - 1);
	// Thirst needs water to find: the testbed's lake within the creature's search for it, 30 cells round it
	const auto lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	const auto* thirst = Find("needs.thirst");
	ASSERT_NE(thirst, nullptr);
	ASSERT_FALSE(thirst->creatures.empty());
	EXPECT_LT(glm::distance(thirst->creatures.front().offset, lake) - flat_land::k_LakeHalfExtent.y, 300.0f);
	EXPECT_NE(Find("light.reflections"), nullptr);
	// the player's creature in the shallows, another player's beside it, and the walk up the bank
	const auto* mirrored = Find("light.creature_reflection");
	ASSERT_NE(mirrored, nullptr);
	ASSERT_EQ(mirrored->creatures.size(), 2u);
	EXPECT_EQ(mirrored->creatures[0].owner, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(mirrored->creatures[1].owner, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(mirrored->commands.size(), 1u);
	EXPECT_TRUE(Problems(*mirrored).empty());
	EXPECT_NE(Find("movement.lake"), nullptr);
}

TEST(TestbedScenarios, NothingStartsOnTheLakesOpenWater)
{
	const auto lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	const auto onOpenWater = [lake](glm::vec2 offset) {
		const auto from = glm::abs(offset - lake);
		return from.x < flat_land::k_LakeHalfExtent.x && from.y < flat_land::k_LakeHalfExtent.y;
	};
	for (const auto& scenario : All())
	{
		for (const auto& creature : scenario.creatures)
		{
			EXPECT_FALSE(onOpenWater(creature.offset)) << scenario.id;
		}
		// the sun glare scenario stands a rock pillar in the lake on purpose: a fixed object that hides part of the sun
		// over the water and never moves; a shark (a whale object) lives in the water
		for (const auto& object : scenario.objects)
		{
			const auto* mobile = std::get_if<MobileObjectInfo>(&object.type);
			if (scenario.id == "sky.sun_glare_behind_bank" || (mobile != nullptr && *mobile == MobileObjectInfo::Whale))
			{
				continue;
			}
			EXPECT_FALSE(onOpenWater(object.offset)) << scenario.id;
		}
		for (const auto& command : scenario.commands)
		{
			if (command.kind == Command::Kind::WalkTo || command.kind == Command::Kind::RunTo)
			{
				EXPECT_FALSE(onOpenWater(command.point)) << scenario.id;
			}
		}
	}
}

TEST(TestbedScenarios, CoversObjectsTheHandAndLeashes)
{
	for (const auto* id :
	     {"objects.reach", "objects.examine", "objects.throw_lob", "objects.eat_held", "objects.knock_down_trees",
	      "objects.point", "objects.curious", "objects.playful", "objects.angry", "hand.stroke", "hand.slap",
	      "hand.status_panel", "leash.types", "leash.pull_to_hand", "leash.tied", "leash.home"})
	{
		EXPECT_NE(Find(id), nullptr) << id;
	}
	const auto commandsOf = [](std::string_view id) {
		std::set<Command::Kind> kinds;
		for (const auto& command : Find(id)->commands)
		{
			kinds.insert(command.kind);
		}
		return kinds;
	};

	// Reaching in every direction round the creature: in front, both sides and behind
	const auto* reach = Find("objects.reach");
	bool front = false;
	bool behind = false;
	bool left = false;
	bool right = false;
	for (const auto& object : reach->objects)
	{
		front = front || object.offset.y < 0.0f;
		behind = behind || object.offset.y > 0.0f;
		left = left || object.offset.x < 0.0f;
		right = right || object.offset.x > 0.0f;
	}
	EXPECT_TRUE(front && behind && left && right);
	EXPECT_TRUE(commandsOf("objects.reach").contains(Command::Kind::PutDown));

	// Every way of looking a thing over
	std::set<size_t> ways;
	for (const auto& command : Find("objects.examine")->commands)
	{
		if (command.kind == Command::Kind::Examine)
		{
			ways.insert(command.value);
		}
	}
	EXPECT_EQ(ways.size(), 4u);
	for (const auto kind : {Command::Kind::ThrowAt, Command::Kind::Lob, Command::Kind::Discard})
	{
		EXPECT_TRUE(commandsOf("objects.throw_lob").contains(kind));
	}
	EXPECT_TRUE(commandsOf("objects.eat_held").contains(Command::Kind::EatHeld));
	// a pot of each class and two piles of the land, each picked up in turn
	const auto* pots = Find("objects.pick_up_pots");
	ASSERT_NE(pots, nullptr);
	EXPECT_TRUE(Problems(*pots).empty());
	ASSERT_EQ(pots->objects.size(), 4u);
	EXPECT_EQ(std::get<PotInfo>(pots->objects[2].type), PotInfo::FoodPile);
	EXPECT_EQ(std::get<PotInfo>(pots->objects[3].type), PotInfo::MagicFood);
	EXPECT_TRUE(commandsOf("objects.pick_up_pots").contains(Command::Kind::PickUp));
	// what is not food is eaten first, then food: a barrel, worth none, and a pile of food
	const auto* notFood = Find("objects.eat_what_is_not_food");
	ASSERT_NE(notFood, nullptr);
	EXPECT_TRUE(Problems(*notFood).empty());
	EXPECT_TRUE(commandsOf("objects.eat_what_is_not_food").contains(Command::Kind::EatHeld));
	ASSERT_EQ(notFood->objects.size(), 2u);
	EXPECT_EQ(std::get<MobileObjectInfo>(notFood->objects[0].type), MobileObjectInfo::EgyptBarrel);
	EXPECT_EQ(std::get<MobileObjectInfo>(notFood->objects[1].type), MobileObjectInfo::MagicFood);
	// left to its mind, a hungry tiger with no food but a barrel and a pot
	const auto* mayEat = Find("mind.hungry_takes_what_it_may_eat");
	ASSERT_NE(mayEat, nullptr);
	EXPECT_TRUE(Problems(*mayEat).empty());
	ASSERT_EQ(mayEat->objects.size(), 2u);
	EXPECT_EQ(std::get<MobileObjectInfo>(mayEat->objects[0].type), MobileObjectInfo::EgyptBarrel);
	// a pile of magic food, eaten from in portions, and a plain pile no portion is made from
	const auto* magicFood = Find("mind.hungry_eats_from_magic_food");
	ASSERT_NE(magicFood, nullptr);
	EXPECT_TRUE(Problems(*magicFood).empty());
	ASSERT_EQ(magicFood->objects.size(), 2u);
	EXPECT_EQ(std::get<PotInfo>(magicFood->objects[0].type), PotInfo::MagicFood);
	EXPECT_EQ(std::get<PotInfo>(magicFood->objects[1].type), PotInfo::FoodPile);
	// a storage pit full of food, a ripe field and a tree, eaten from where they stand
	const auto* fromStore = Find("mind.eats_from_store");
	ASSERT_NE(fromStore, nullptr);
	EXPECT_TRUE(Problems(*fromStore).empty());
	ASSERT_EQ(fromStore->objects.size(), 1u);
	EXPECT_EQ(std::get<AbodeInfo>(fromStore->objects[0].type), AbodeInfo::CelticStoragePit);
	const auto* fromField = Find("mind.eats_from_field");
	ASSERT_NE(fromField, nullptr);
	EXPECT_TRUE(Problems(*fromField).empty());
	ASSERT_EQ(fromField->objects.size(), 1u);
	EXPECT_TRUE(fromField->objects[0].ripeCrop);
	const auto* fromTree = Find("mind.eats_from_tree");
	ASSERT_NE(fromTree, nullptr);
	EXPECT_TRUE(Problems(*fromTree).empty());
	ASSERT_EQ(fromTree->commands.size(), 1u);
	EXPECT_EQ(fromTree->commands[0].kind, Command::Kind::ScriptDoAction);
	EXPECT_EQ(fromTree->commands[0].value, 94u);
	EXPECT_TRUE(commandsOf("objects.knock_down_trees").contains(Command::Kind::KnockDown));
	EXPECT_TRUE(commandsOf("objects.kick_tree").contains(Command::Kind::Kick));
	EXPECT_TRUE(commandsOf("objects.point").contains(Command::Kind::PointAt));
	// The moods are the mind's own, with nothing told
	EXPECT_TRUE(Find("objects.curious")->commands.empty());
	EXPECT_FALSE(Find("objects.angry")->creatures.front().desires.empty());

	// Every part of the body stroked; slaps high and low, gentle and hard
	std::set<size_t> parts;
	for (const auto& command : Find("hand.stroke")->commands)
	{
		if (command.kind == Command::Kind::HandStroke)
		{
			parts.insert(command.bodyPart);
		}
	}
	EXPECT_EQ(parts.size(), creature_feedback::k_BodyPartCount);
	std::set<std::pair<bool, bool>> slaps;
	for (const auto& command : Find("hand.slap")->commands)
	{
		if (command.kind == Command::Kind::HandSlap)
		{
			slaps.insert({command.gentle, command.slapHeight < creature_feedback::k_FeetBelow});
		}
	}
	EXPECT_EQ(slaps.size(), 4u);
	EXPECT_TRUE(commandsOf("hand.slap").contains(Command::Kind::HandLetGo));

	// The status panel's creature has needs held to read off it
	const auto& worn = Find("hand.status_panel")->creatures.front();
	EXPECT_TRUE(worn.hold);
	EXPECT_TRUE(worn.needs.life.has_value() && worn.needs.energy.has_value() && worn.needs.exhaustion.has_value());

	// Every leash, tied up and kept at home
	std::set<LeashType> leashes;
	for (const auto& command : Find("leash.types")->commands)
	{
		if (command.kind == Command::Kind::PutOnLeash)
		{
			leashes.insert(command.leash);
		}
	}
	EXPECT_EQ(leashes, (std::set<LeashType> {LeashType::Evil, LeashType::Rope, LeashType::Good}));
	EXPECT_TRUE(commandsOf("leash.tied").contains(Command::Kind::TieLeash));
	EXPECT_TRUE(commandsOf("leash.tied").contains(Command::Kind::UntieLeash));
	EXPECT_TRUE(commandsOf("leash.home").contains(Command::Kind::ConfineToHome));
}

TEST(TestbedScenarios, CoversFights)
{
	const auto commandsOf = [](std::string_view id) {
		std::set<Command::Kind> kinds;
		const auto* scenario = Find(id);
		EXPECT_NE(scenario, nullptr) << id;
		if (scenario != nullptr)
		{
			for (const auto& command : scenario->commands)
			{
				kinds.insert(command.kind);
			}
		}
		return kinds;
	};
	EXPECT_TRUE(commandsOf("combat.ai_duel").contains(Command::Kind::FightAuto));
	EXPECT_TRUE(commandsOf("combat.player").contains(Command::Kind::FightBlow));
	EXPECT_TRUE(commandsOf("combat.player").contains(Command::Kind::FightStep));
	EXPECT_TRUE(commandsOf("combat.blocking").contains(Command::Kind::FightBlock));
	EXPECT_TRUE(commandsOf("combat.faint_recovery").contains(Command::Kind::KnockOut));
	EXPECT_TRUE(commandsOf("combat.leash").contains(Command::Kind::TieLeashToCreature));
	// The player's mouse clicks the opponent and the player's own creature through the real input, neither creature
	// fighting by itself
	const auto* clicks = Find("combat.player_clicks");
	ASSERT_NE(clicks, nullptr);
	std::set<size_t> pressedOn;
	std::set<size_t> buttons;
	for (const auto& command : clicks->commands)
	{
		EXPECT_FALSE(command.kind == Command::Kind::FightBlow || command.kind == Command::Kind::FightBlock);
		if (command.kind == Command::Kind::PointerPress && command.atCreature.has_value())
		{
			pressedOn.insert(*command.atCreature);
			buttons.insert(command.value);
		}
	}
	EXPECT_EQ(pressedOn, (std::set<size_t> {0, 1}));
	EXPECT_EQ(buttons, (std::set<size_t> {1, 3}));
	EXPECT_TRUE(commandsOf("combat.player_clicks").contains(Command::Kind::FightAuto));
	// The player draws the lightning's gesture with the mouse in the fight, which the creature knows, and no debug cast
	const auto* miracle = Find("combat.fight_miracle_gesture");
	ASSERT_NE(miracle, nullptr);
	const auto drawsTheLightning = [](const Command& command) {
		return command.kind == Command::Kind::DrawGesture && command.value == static_cast<size_t>(GestureType::ForkUp);
	};
	EXPECT_TRUE(std::ranges::any_of(miracle->commands, drawsTheLightning));
	EXPECT_TRUE(std::ranges::any_of(miracle->commands, [](const Command& command) {
		return command.kind == Command::Kind::DrawGesture && command.value == static_cast<size_t>(GestureType::Star);
	}));
	EXPECT_TRUE(commandsOf("combat.fight_miracle_gesture").contains(Command::Kind::KnowMiracle));
	EXPECT_FALSE(commandsOf("combat.fight_miracle_gesture").contains(Command::Kind::FightCast));

	// Blows let go at once and held the full charge
	std::set<float> charges;
	for (const auto& command : Find("combat.charged")->commands)
	{
		if (command.kind == Command::Kind::FightBlow)
		{
			charges.insert(command.chargeMs);
		}
	}
	EXPECT_EQ(charges, (std::set<float> {0.0f, creature_fight::k_MaxChargeMs}));
	// Only the scenario about it lets anger start fights, so other angry creatures keep to themselves
	for (const auto& scenario : All())
	{
		EXPECT_EQ(scenario.environment.angerStartsFights, scenario.id == "combat.anger") << scenario.id;
	}
	// The worn creature is left to rest
	EXPECT_FALSE(Find("combat.faint_recovery")->creatures.front().hold);
}

TEST(TestbedScenarios, FightCommandsAreChecked)
{
	using Kind = Command::Kind;
	Scenario scenario {
	    .id = "test.fight",
	    .name = "Fight",
	    .description = "A fight",
	    .expected = "Blows",
	    .creatures = {{.species = CreatureType::Tiger}, {.species = CreatureType::Tiger, .offset = {40.0f, 0.0f}}},
	    .commands = {{.kind = Kind::StartFight, .value = 1},
	                 {.kind = Kind::FightBlow, .value = 2, .chargeMs = 1200.0f},
	                 {.kind = Kind::FightStep, .value = 3},
	                 {.kind = Kind::FightCast, .value = 41},
	                 {.kind = Kind::TieLeashToCreature, .value = 1}},
	};
	EXPECT_TRUE(Problems(scenario).empty());
	EXPECT_EQ(Name(Kind::FightCast), "fight cast");
	scenario.commands = {{.kind = Kind::StartFight, .value = 0},         {.kind = Kind::FightBlow, .value = 3},
	                     {.kind = Kind::FightBlow, .chargeMs = 2000.0f}, {.kind = Kind::FightStep, .value = 4},
	                     {.kind = Kind::FightCast, .value = 0},          {.kind = Kind::FightCast, .value = 42},
	                     {.kind = Kind::TieLeashToCreature, .value = 2}};
	EXPECT_EQ(Problems(scenario).size(), 7u);
}

TEST(TestbedScenarios, TheSpareCreatureScenarioRemovesTheOneNotLedThenTapsTheOneLed)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("leash.spare_creature_removed");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 2u);
	// the testbed's rule leaves the left tiger, made first, the one led
	EXPECT_TRUE(scenario->firstCreatureLeads);
	const std::vector<Kind> kinds {Kind::HandTapLeash, Kind::LeashState, Kind::RemoveCreature,
	                               Kind::LeashState,   Kind::LeashState, Kind::HandTapLeash};
	ASSERT_EQ(scenario->commands.size(), kinds.size());
	for (size_t i = 0; i < kinds.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, kinds[i]) << i;
	}
	// the right tiger, the one not led, is the one removed, and then told something once it has gone
	EXPECT_EQ(scenario->commands[2].creature, 1u);
	EXPECT_EQ(scenario->commands[3].creature, 1u);
	// the rest is the left tiger's
	for (const size_t i : {0u, 1u, 4u, 5u})
	{
		EXPECT_EQ(scenario->commands[i].creature, 0u) << i;
	}
	EXPECT_FALSE(NeedsNoCreature(Kind::RemoveCreature));
	EXPECT_EQ(Name(Kind::RemoveCreature), "remove from the game");
}

TEST(TestbedScenarios, TheNewestTakesTheSlotScenarioLeavesTheLeashToTheGamesRule)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("leash.newest_takes_the_slot");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_FALSE(scenario->firstCreatureLeads);
	ASSERT_EQ(scenario->creatures.size(), 2u);
	for (const auto& creature : scenario->creatures)
	{
		EXPECT_EQ(creature.owner, PlayerNames::PLAYER_ONE);
		EXPECT_FALSE(creature.leashable.has_value());
	}
	const std::vector<Kind> kinds {Kind::HandTapLeash, Kind::LeashState, Kind::HandTapLeash, Kind::LeashState};
	ASSERT_EQ(scenario->commands.size(), kinds.size());
	for (size_t i = 0; i < kinds.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, kinds[i]) << i;
	}
	// every other scenario keeps the testbed's rule, so its creature on the leash is the one it was written for
	const auto off = std::ranges::count_if(All(), [](const Scenario& each) { return !each.firstCreatureLeads; });
	EXPECT_EQ(off, 1);
}

TEST(TestbedScenarios, EveryCommandHasAName)
{
	// one row for every kind, in the enum's order, each with a name of its own
	const auto kinds = CommandKinds();
	ASSERT_EQ(kinds.size(), static_cast<size_t>(Command::Kind::Count));
	std::set<std::string_view> names;
	for (size_t i = 0; i < kinds.size(); ++i)
	{
		EXPECT_EQ(static_cast<size_t>(kinds[i].kind), i);
		EXPECT_FALSE(kinds[i].name.empty()) << i;
		EXPECT_EQ(Name(kinds[i].kind), kinds[i].name);
		EXPECT_TRUE(names.insert(kinds[i].name).second) << kinds[i].name;
		EXPECT_EQ(NeedsNoCreature(kinds[i].kind), kinds[i].needsNoCreature || kinds[i].pointer) << kinds[i].name;
		EXPECT_EQ(IsPointerCommand(kinds[i].kind), kinds[i].pointer) << kinds[i].name;
	}
}

TEST(TestbedScenarios, TheCommandTableKeepsTheNamesAndTheNoCreatureCommands)
{
	// As the commands stood when they were gathered into one table (joint104): the first, the last, and every command
	// that needs no creature
	using Kind = Command::Kind;
	EXPECT_EQ(Name(Kind::WalkTo), "walk to");
	EXPECT_EQ(Name(Kind::LeaveTemple), "leave temple");
	EXPECT_EQ(Name(Kind::RemoveCreature), "remove from the game");
	for (const auto kind : {Kind::SetHour,
	                        Kind::HoldSeed,
	                        Kind::DrawGesture,
	                        Kind::SummonSeed,
	                        Kind::PressKey,
	                        Kind::HandTakeFireBall,
	                        Kind::SetAlignment,
	                        Kind::ChangeLand,
	                        Kind::VillagerWalkTo,
	                        Kind::StoreTakes,
	                        Kind::SetGameSpeed,
	                        Kind::SetOpenClose,
	                        Kind::LayGateStone,
	                        Kind::SpiritOut,
	                        Kind::SpiritHome,
	                        Kind::CreateWorshipSite,
	                        Kind::WideScreen,
	                        Kind::ShowText,
	                        Kind::PlayFilm,
	                        Kind::CreateVortex,
	                        Kind::VortexFadeIn,
	                        Kind::VortexFadeOut,
	                        Kind::VortexCrossingFile,
	                        Kind::ScriptFade,
	                        Kind::HighDetail,
	                        Kind::KeyDown,
	                        Kind::KeyUp,
	                        Kind::ScriptHealth,
	                        Kind::ScriptTotemProperty,
	                        Kind::ScriptMusic,
	                        Kind::MusicState,
	                        Kind::ScriptReward,
	                        Kind::ScriptDance,
	                        Kind::TapHighlight,
	                        Kind::ScriptDanceAttach,
	                        Kind::ScriptInterfaceLevel,
	                        Kind::TempleBuilt,
	                        Kind::OpenTattooEditor,
	                        Kind::Advisor,
	                        Kind::AbodeBuilt,
	                        Kind::LeaveTemple,
	                        Kind::ScriptWalkPath,
	                        Kind::PointerTo,
	                        Kind::PointerPress,
	                        Kind::PointerRelease,
	                        Kind::PointerSweep,
	                        Kind::WheelTurn,
	                        Kind::PointerDoubleClick})
	{
		EXPECT_TRUE(NeedsNoCreature(kind)) << Name(kind);
	}
}

TEST(TestbedScenarios, TheAdvisorScenariosTellBothAdvisorsWithoutACreature)
{
	using Kind = Command::Kind;
	for (const auto* id : {"advisors.out", "advisors.cling", "advisors.point_and_act", "advisors.appear_and_vanish"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		EXPECT_EQ(scenario->facet, Facet::Advisors) << id;
		EXPECT_TRUE(scenario->creatures.empty()) << id;
		// Both advisors are told, the good one first
		ASSERT_GE(scenario->commands.size(), 2u) << id;
		EXPECT_EQ(scenario->commands[0].value, 0u) << id;
		EXPECT_EQ(scenario->commands[1].value, 1u) << id;
		for (const auto& command : scenario->commands)
		{
			EXPECT_EQ(command.kind, Kind::Advisor) << id;
		}
	}
	EXPECT_TRUE(NeedsNoCreature(Kind::Advisor));
	// a script's flock filled with sheep, through FLOCK_CREATE and POPULATE_CONTAINER
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptPopulate));
	EXPECT_EQ(Name(Kind::ScriptPopulate), "script populate");
	const auto* populate = Find("script.populate_flock");
	ASSERT_NE(populate, nullptr);
	EXPECT_TRUE(Problems(*populate).empty());
	ASSERT_EQ(populate->commands.size(), 1u);
	EXPECT_EQ(populate->commands[0].kind, Kind::ScriptPopulate);
	EXPECT_EQ(populate->commands[0].amount, 8.0f);
	EXPECT_EQ(Name(Kind::Advisor), "tell an advisor");
}

TEST(TestbedScenarios, TheScriptCreateScenarioMakesTheGladesThreeCreatures)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptCreate));
	EXPECT_EQ(Name(Kind::ScriptCreate), "script create");
	const auto* scenario = Find("script.create_creatures");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	ASSERT_EQ(scenario->commands.size(), 3u);
	const std::array<uint32_t, 3> species {1, 0, 2};
	for (size_t i = 0; i < species.size(); ++i)
	{
		EXPECT_EQ(scenario->commands.at(i).kind, Kind::ScriptCreate) << i;
		EXPECT_EQ(scenario->commands.at(i).value, static_cast<size_t>(script::ObjectType::Creature)) << i;
		EXPECT_EQ(scenario->commands.at(i).subtype, species.at(i)) << i;
	}
}

TEST(TestbedScenarios, TheScriptCreateSettlesScenarioMakesEachMobileThingThatStartsItsPhysics)
{
	using Kind = Command::Kind;
	using script::ObjectType;
	const auto* scenario = Find("script.create_settles");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	const std::array<ObjectType, 6> types {ObjectType::Poo,      ObjectType::MobileObject, ObjectType::Ark,
	                                       ObjectType::Scaffold, ObjectType::Ball,         ObjectType::Rock};
	ASSERT_EQ(scenario->commands.size(), types.size());
	for (size_t i = 0; i < types.size(); ++i)
	{
		EXPECT_EQ(scenario->commands.at(i).kind, Kind::ScriptCreate) << i;
		EXPECT_EQ(scenario->commands.at(i).value, static_cast<size_t>(types.at(i))) << i;
	}
	EXPECT_EQ(scenario->commands.at(1).subtype, static_cast<uint32_t>(MobileObjectInfo::Cauldron));
	EXPECT_EQ(scenario->commands.at(5).subtype, static_cast<uint32_t>(MobileStaticInfo::Rock));
}

TEST(TestbedScenarios, TheScaffoldFoundsTownScenarioSetsAScaffoldOfFiveBuildingAwayFromEveryTown)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::ScaffoldSetActive));
	EXPECT_EQ(Name(Kind::ScaffoldSetActive), "scaffold set active");
	const auto* scenario = Find("building.scaffold_founds_town");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	ASSERT_EQ(scenario->commands.size(), 1u);
	const auto& command = scenario->commands.front();
	EXPECT_EQ(command.kind, Kind::ScaffoldSetActive);
	// the village centre's five scaffolds
	EXPECT_EQ(command.value, 5u);
	// further from the only village than a scaffold looks for a town to build for (50), and the wood within a town's
	// reach for its forests (250)
	ASSERT_EQ(scenario->fixtures.villages.size(), 1u);
	ASSERT_EQ(scenario->fixtures.trees.size(), 1u);
	const auto village = std::get<glm::vec2>(scenario->fixtures.villages.front().at);
	const auto wood = std::get<glm::vec2>(scenario->fixtures.trees.front().at);
	EXPECT_GT(glm::distance(command.point, village), 100.0f);
	EXPECT_LT(glm::distance(command.point, wood), 100.0f);
}

TEST(TestbedScenarios, TheFireDrawsMournersScenarioSetsAlightTheHutByTheMournersOfADeadVillager)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::VillagerReactions));
	EXPECT_EQ(Name(Kind::VillagerReactions), "villager reactions");
	const auto* scenario = Find("miracles.fire_draws_mourners");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	ASSERT_EQ(scenario->objects.size(), 5u);
	const auto& hut = scenario->objects[0];
	const auto& dead = scenario->objects[1];
	EXPECT_TRUE(std::holds_alternative<AbodeInfo>(hut.type));
	// a villager of the town that dies at once
	EXPECT_TRUE(std::holds_alternative<VillagerInfo>(dead.type));
	EXPECT_TRUE(dead.joinTown);
	EXPECT_EQ(dead.life, std::optional<float>(0.0f));
	// three of its town within the death's reach (60) of it, far enough to walk up to it, and within the fire's reach
	// (35, the spread's half of the x and z distances) of the hut
	for (size_t i = 2; i < 5; ++i)
	{
		const auto& mourner = scenario->objects[i];
		EXPECT_TRUE(std::holds_alternative<VillagerInfo>(mourner.type)) << i;
		EXPECT_TRUE(mourner.joinTown) << i;
		EXPECT_FALSE(mourner.life.has_value()) << i;
		EXPECT_GT(glm::distance(mourner.offset, dead.offset), 20.0f) << i;
		EXPECT_LT(glm::distance(mourner.offset, dead.offset), 60.0f) << i;
		const auto apart = glm::abs(mourner.offset - hut.offset);
		EXPECT_LT((apart.x + apart.y) * 0.5f, 35.0f) << i;
	}
	// the fireball at the hut, before the mourners have mourned for the switch's ten seconds
	ASSERT_EQ(scenario->miracles.size(), 1u);
	EXPECT_EQ(scenario->miracles[0].type, MagicType::Fireball);
	EXPECT_EQ(scenario->miracles[0].point, hut.offset);
	EXPECT_LT(scenario->miracles[0].delaySeconds, 10.0f);
	// twice as fast, and the readouts before and after the switch, the last within 900 frames of 16 ms
	ASSERT_EQ(scenario->commands.size(), 6u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::SetGameSpeed);
	EXPECT_EQ(scenario->commands[0].gameSpeed, 2.0f);
	float at = 0.0f;
	std::vector<float> readouts;
	for (const auto& command : scenario->commands)
	{
		at += command.delaySeconds;
		if (command.kind == Kind::VillagerReactions)
		{
			readouts.push_back(at);
		}
	}
	EXPECT_EQ(readouts, (std::vector<float> {3.0f, 9.0f, 14.0f, 20.0f, 26.0f}));
	EXPECT_LT(readouts.back(), 900.0f * 0.016f * scenario->commands[0].gameSpeed);
}

TEST(TestbedScenarios, TheShieldStruckPeopleScenarioStrikesASecondShieldWhoseMiddleIsUnderTheFirst)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("miracles.shield_struck_people");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	// the town's people, whom another player's lightning attacks first so that they want the shield's protection
	size_t villagers = 0;
	for (const auto& object : scenario->objects)
	{
		if (std::holds_alternative<VillagerInfo>(object.type))
		{
			EXPECT_TRUE(object.joinTown);
			++villagers;
		}
	}
	EXPECT_EQ(villagers, 5u);
	ASSERT_EQ(scenario->miracles.size(), 3u);
	const auto& lightning = scenario->miracles[0];
	const auto& first = scenario->miracles[1];
	const auto& second = scenario->miracles[2];
	EXPECT_EQ(lightning.type, MagicType::LightningBolt);
	EXPECT_EQ(lightning.player, PlayerNames::PLAYER_TWO);
	EXPECT_LT(lightning.delaySeconds, first.delaySeconds);
	// the first shield cast at a point (radius 30), the second through the hand at a smaller circle, both the player's;
	// the second's middle under the first, its far side out of it
	EXPECT_EQ(first.type, MagicType::Shield);
	EXPECT_FALSE(first.byHand);
	EXPECT_EQ(second.type, MagicType::Shield);
	EXPECT_TRUE(second.byHand);
	ASSERT_TRUE(second.circleRadius.has_value());
	EXPECT_EQ(first.player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(second.player, PlayerNames::PLAYER_ONE);
	const float apart = glm::distance(first.point, second.point);
	EXPECT_LT(apart, 30.0f);
	EXPECT_GT(apart + *second.circleRadius, 30.0f);
	// the strikes need no creature, and each is at the second shield, nearer it than the first
	EXPECT_TRUE(NeedsNoCreature(Kind::StrikeShield));
	EXPECT_EQ(Name(Kind::StrikeShield), "strike shield");
	// twice as fast; the strikes at 24 and 26 seconds, the first once the people, amazed by a shield from about 12.5
	// seconds in the scene's runs, have been so for the switch's ten seconds, and the readouts before and after them,
	// the last within 900 frames of 16 ms
	constexpr float k_AmazedFromSeconds = 12.5f;
	ASSERT_EQ(scenario->commands.size(), 9u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::SetGameSpeed);
	EXPECT_EQ(scenario->commands[0].gameSpeed, 2.0f);
	float at = 0.0f;
	std::vector<float> readouts;
	std::vector<float> strikes;
	for (const auto& command : scenario->commands)
	{
		at += command.delaySeconds;
		if (command.kind == Kind::VillagerReactions)
		{
			readouts.push_back(at);
		}
		else if (command.kind == Kind::StrikeShield)
		{
			strikes.push_back(at);
			EXPECT_EQ(command.point, second.point);
			EXPECT_LT(glm::distance(command.point, second.point), glm::distance(command.point, first.point));
		}
	}
	EXPECT_EQ(readouts, (std::vector<float> {3.0f, 10.0f, 16.0f, 21.0f, 25.0f, 27.0f}));
	EXPECT_EQ(strikes, (std::vector<float> {24.0f, 26.0f}));
	EXPECT_GE(strikes.front() - k_AmazedFromSeconds, 10.0f);
	EXPECT_LT(readouts.at(3), strikes.front());
	EXPECT_GT(readouts.at(4), strikes.front());
	EXPECT_LT(readouts.back(), 900.0f * 0.016f * scenario->commands[0].gameSpeed);
}

TEST(TestbedScenarios, TheLastFightsTimelineSetsTheMirrorsFightHealthToNoneFiveTimesFifteenSecondsApart)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptOpponentProperty));
	EXPECT_EQ(Name(Kind::ScriptOpponentProperty), "script opponent property");
	const auto* scenario = Find("quest.so_this_is_a_fight_to_the_death");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	// each one more than the 10 seconds the script waits after a heal from the one before, and drawn casts after the last
	constexpr size_t k_FightHealth = 33;
	float at = 0.0f;
	std::vector<float> drops;
	float lastCast = 0.0f;
	for (const auto& command : scenario->commands)
	{
		at += command.delaySeconds;
		if (command.kind == Kind::ScriptOpponentProperty)
		{
			EXPECT_EQ(command.player, PlayerNames::PLAYER_ONE);
			EXPECT_EQ(command.value, k_FightHealth);
			EXPECT_EQ(command.amount, 0.0f);
			drops.push_back(at);
		}
		else if (command.kind == Kind::DrawGesture)
		{
			lastCast = at;
		}
	}
	ASSERT_EQ(drops.size(), 5u);
	for (size_t i = 1; i < drops.size(); ++i)
	{
		EXPECT_GT(drops.at(i) - drops.at(i - 1), 10.0f);
	}
	EXPECT_GT(lastCast, drops.back());
}

TEST(TestbedScenarios, TheLandColourIndexScenarioShowsTheSecondLandsColoursOnThreeTemples)
{
	const auto* scenario = Find("worship.land_colour_index");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_TRUE(scenario->commands.empty());
	ASSERT_EQ(scenario->environment.landNumber, std::optional<int32_t>(2));
	const auto land = *scenario->environment.landNumber;
	// the three players' temples, built whole so that their borders show
	const auto& temples = scenario->fixtures.temples;
	ASSERT_EQ(temples.size(), 3u);
	for (const auto& temple : temples)
	{
		EXPECT_EQ(temple.built, 1.0f);
	}
	EXPECT_EQ(temples.at(0).owner, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(temples.at(1).owner, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(temples.at(2).owner, PlayerNames::PLAYER_THREE);
	// the first keeps its own red; the second is drawn yellow and the third blue, not their own green and magenta
	EXPECT_EQ(ecs::player_colours::Remapped(PlayerNames::PLAYER_ONE, land), PlayerNames::PLAYER_ONE);
	EXPECT_EQ(ecs::player_colours::Colour(PlayerNames::PLAYER_TWO, land), 0xFFFFFD47u);
	EXPECT_EQ(ecs::player_colours::Colour(PlayerNames::PLAYER_THREE, land), 0xFF4777FFu);
	EXPECT_NE(ecs::player_colours::Colour(PlayerNames::PLAYER_TWO, land),
	          ecs::player_colours::k_Colours.at(static_cast<size_t>(PlayerNames::PLAYER_TWO)));
	// a land number that is none of the lands is a problem
	auto other = *scenario;
	other.environment.landNumber = 7;
	EXPECT_FALSE(Problems(other).empty());
}

TEST(TestbedScenarios, TheScriptPlayScenarioGivesAnIndividualAndAStaticActionAndAsksPlayed)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.script_play");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_EQ(scenario->facet, Facet::Mind);
	ASSERT_EQ(scenario->creatures.size(), 2u);
	ASSERT_GE(scenario->commands.size(), 4u);
	const auto& sneeze = scenario->commands[0];
	const auto& sit = scenario->commands[1];
	EXPECT_EQ(sneeze.kind, Kind::ScriptPlay);
	EXPECT_EQ(sit.kind, Kind::ScriptPlay);
	EXPECT_EQ(sneeze.creature, 0u);
	EXPECT_EQ(sit.creature, 1u);
	// An individual action (52 and over) twice, a static one (51 and below) once
	EXPECT_GE(sneeze.value, 52u);
	EXPECT_EQ(sneeze.plays, 2u);
	EXPECT_LE(sit.value, 51u);
	EXPECT_EQ(sit.plays, 1u);
	// Then only PLAYED readouts, of both
	for (size_t i = 2; i < scenario->commands.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, Kind::ScriptPlayed) << i;
	}
	for (const auto kind : {Kind::ScriptPlay, Kind::ScriptPlayed})
	{
		EXPECT_FALSE(NeedsNoCreature(kind));
	}
	EXPECT_EQ(Name(Kind::ScriptPlay), "script play");
	EXPECT_EQ(Name(Kind::ScriptPlayed), "script played");
}

TEST(TestbedScenarios, TheScriptMoveScenarioSendsOneInTheGamesTwoLegsAndOneWithinADistanceThenFocusesTheFirst)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.script_move");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 2u);
	ASSERT_EQ(scenario->commands.size(), 3u);
	const auto& legs = scenario->commands[0];
	const auto& within = scenario->commands[1];
	const auto& focus = scenario->commands[2];
	EXPECT_EQ(legs.kind, Kind::ScriptMove);
	EXPECT_EQ(within.kind, Kind::ScriptMove);
	EXPECT_EQ(focus.kind, Kind::ScriptFocus);
	// No distance for the game's two legs, a distance for the other; each sent 60 ahead of where it stands
	EXPECT_FLOAT_EQ(legs.radius, 0.0f);
	EXPECT_GT(within.radius, 0.0f);
	for (size_t i = 0; i < 2; ++i)
	{
		const auto& move = scenario->commands[i];
		EXPECT_FLOAT_EQ(move.point.y - scenario->creatures[move.creature].offset.y, 60.0f) << i;
	}
	// The focus turns the first back to where it started
	EXPECT_EQ(focus.creature, 0u);
	EXPECT_EQ(focus.point, scenario->creatures[0].offset);
	EXPECT_FALSE(NeedsNoCreature(Kind::ScriptMove));
	EXPECT_EQ(Name(Kind::ScriptMove), "script move");
}

TEST(TestbedScenarios, TheScriptDoActionScenarioForcesTheTwoLooksOnTheVillagerAndTheSitAtTheRock)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.script_do_action");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 1u);
	ASSERT_EQ(scenario->objects.size(), 2u);
	EXPECT_TRUE(std::holds_alternative<VillagerInfo>(scenario->objects[0].type));
	EXPECT_TRUE(std::holds_alternative<MobileStaticInfo>(scenario->objects[1].type));
	ASSERT_EQ(scenario->commands.size(), 3u);
	// LookForever and LookButDontApproach at the villager, SitDown at the rock, by their rows of the game's table
	const std::array<std::pair<size_t, size_t>, 3> k_Done {{{189, 0}, {188, 0}, {81, 1}}};
	for (size_t i = 0; i < k_Done.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, Kind::ScriptDoAction) << i;
		EXPECT_EQ(scenario->commands[i].value, k_Done[i].first) << i;
		EXPECT_EQ(scenario->commands[i].object, k_Done[i].second) << i;
	}
	EXPECT_FALSE(NeedsNoCreature(Kind::ScriptDoAction));
	EXPECT_EQ(Name(Kind::ScriptDoAction), "script do action");
}

TEST(TestbedScenarios, TheStompAndEatScenarioForcesRowThirteenOnTheCowWithNothingToDoItWith)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.script_stomp_and_eat");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_EQ(scenario->facet, Facet::Mind);
	ASSERT_EQ(scenario->creatures.size(), 1u);
	ASSERT_EQ(scenario->objects.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<AnimalInfo>(scenario->objects[0].type));
	EXPECT_EQ(std::get<AnimalInfo>(scenario->objects[0].type), AnimalInfo::Cow);
	// the creature, row 13, the cow and nothing to do it with, as the third land's script forces it
	ASSERT_EQ(scenario->commands.size(), 1u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::ScriptDoAction);
	EXPECT_EQ(scenario->commands[0].value, 13u);
	EXPECT_EQ(scenario->commands[0].object, 0u);
	EXPECT_FALSE(scenario->commands[0].with.has_value());
	EXPECT_TRUE(scenario->logReactions);
}

TEST(TestbedScenarios, TheDiePermanentlyScenarioForcesRowTwoHundredSeventeenOnTheCreatureItself)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.script_die_permanently");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_EQ(scenario->facet, Facet::Mind);
	ASSERT_EQ(scenario->creatures.size(), 1u);
	EXPECT_TRUE(scenario->objects.empty());
	// the creature, row 217, done to itself and with nothing, as the second and fifth lands' scripts force it
	ASSERT_EQ(scenario->commands.size(), 1u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::ScriptDoAction);
	EXPECT_EQ(scenario->commands[0].value, 217u);
	EXPECT_EQ(scenario->commands[0].atCreature, 0u);
	EXPECT_FALSE(scenario->commands[0].with.has_value());
}

TEST(TestbedScenarios, TheDieScenarioForcesRowNineOnTheCreatureItself)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.script_die");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_EQ(scenario->facet, Facet::Mind);
	ASSERT_EQ(scenario->creatures.size(), 1u);
	EXPECT_TRUE(scenario->objects.empty());
	// the creature, row 9, done to itself and with nothing, as the fourth and fifth lands' scripts force it
	ASSERT_EQ(scenario->commands.size(), 1u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::ScriptDoAction);
	EXPECT_EQ(scenario->commands[0].value, 9u);
	EXPECT_EQ(scenario->commands[0].atCreature, 0u);
	EXPECT_FALSE(scenario->commands[0].with.has_value());
	EXPECT_TRUE(scenario->logReactions);
}

TEST(TestbedScenarios, TheOddActionScenariosTellTheCreatureTheirRowsThroughTheScript)
{
	using Kind = Command::Kind;
	// ShowLearntLesson then LookConfused; BehaveStrangely twice, by their rows of the game's table
	const std::array<std::pair<std::string_view, std::array<size_t, 2>>, 2> k_Told {
	    {{"mind.odd_lesson_and_confused", {86, 92}}, {"mind.odd_behave_strangely", {213, 213}}}};
	for (const auto& [id, rows] : k_Told)
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		ASSERT_EQ(scenario->creatures.size(), 1u) << id;
		ASSERT_EQ(scenario->commands.size(), rows.size()) << id;
		for (size_t i = 0; i < rows.size(); ++i)
		{
			EXPECT_EQ(scenario->commands[i].kind, Kind::ScriptDoAction) << id << " " << i;
			EXPECT_EQ(scenario->commands[i].value, rows[i]) << id << " " << i;
		}
	}
}

TEST(TestbedScenarios, TheScriptFocusScenarioTurnsTheCreatureAroundThenReleasesItBeforeTheShot)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.focus_by_script");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_EQ(scenario->facet, Facet::Mind);
	// One content tiger with its mind running, its back to the camera (facing north)
	ASSERT_EQ(scenario->creatures.size(), 1u);
	const auto& tiger = scenario->creatures.front();
	EXPECT_FLOAT_EQ(tiger.facingDegrees, 180.0f);
	EXPECT_FALSE(tiger.pauseMind);
	EXPECT_TRUE(tiger.hold);
	ASSERT_EQ(scenario->commands.size(), 2u);
	const auto& focus = scenario->commands[0];
	const auto& release = scenario->commands[1];
	EXPECT_EQ(focus.kind, Kind::ScriptFocus);
	EXPECT_EQ(release.kind, Kind::ScriptRelease);
	EXPECT_EQ(focus.creature, 0u);
	EXPECT_EQ(release.creature, 0u);
	// The point is straight behind it, to the south
	EXPECT_FLOAT_EQ(focus.point.x, tiger.offset.x);
	EXPECT_LT(focus.point.y, tiger.offset.y);
	// A hold of several seconds once the turn is over, and the release before the shot of the checks' 900 frames of
	// 16 ms, taken 20 frames before the end
	constexpr float k_ShotSeconds = 880.0f * 0.016f;
	EXPECT_GE(release.delaySeconds, 6.0f);
	EXPECT_LT(focus.delaySeconds + release.delaySeconds + 2.0f, k_ShotSeconds);
	for (const auto kind : {Kind::ScriptFocus, Kind::ScriptRelease})
	{
		EXPECT_FALSE(NeedsNoCreature(kind));
	}
	EXPECT_EQ(Name(Kind::ScriptFocus), "script focus");
	EXPECT_EQ(Name(Kind::ScriptRelease), "script release");

	// Both act on one of the scenario's creatures
	Scenario wrong = *scenario;
	wrong.commands[0].creature = 1;
	wrong.commands[1].creature = 1;
	EXPECT_EQ(Problems(wrong).size(), 2u);
}

TEST(TestbedScenarios, TheFightPanelScenariosFadeTheScreenAsAScriptDoes)
{
	using Kind = Command::Kind;
	const auto* plain = Find("combat.fight_panel");
	const auto* faded = Find("combat.fight_panel_under_fade");
	ASSERT_NE(plain, nullptr);
	ASSERT_NE(faded, nullptr);
	EXPECT_TRUE(Problems(*plain).empty());
	EXPECT_TRUE(Problems(*faded).empty());
	EXPECT_EQ(faded->facet, Facet::Combat);
	// The same fight, then a fade to black over ten seconds
	ASSERT_EQ(faded->commands.size(), plain->commands.size() + 1);
	const auto& fade = faded->commands.back();
	EXPECT_EQ(fade.kind, Kind::ScriptFade);
	EXPECT_EQ(fade.value, 0u);
	EXPECT_EQ(fade.fadeColour, 0u);
	EXPECT_FLOAT_EQ(fade.fadeSeconds, 10.0f);
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptFade));
	EXPECT_EQ(Name(Kind::ScriptFade), "script fade");
}

TEST(TestbedScenarios, TheHighDetailEyesScenarioDrawsTheFatherUnderAWideScreen)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("villagers.high_detail_eyes");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->objects.size(), 1u);
	EXPECT_EQ(std::get<VillagerInfo>(scenario->objects.front().type), VillagerInfo::NorseShepherdMale);
	ASSERT_EQ(scenario->commands.size(), 3u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::WideScreen);
	EXPECT_EQ(scenario->commands[1].kind, Kind::HighDetail);
	EXPECT_EQ(scenario->commands[1].value, 1u);
	EXPECT_EQ(scenario->commands[2].kind, Kind::HighDetail);
	EXPECT_EQ(scenario->commands[2].value, 0u);
	EXPECT_TRUE(NeedsNoCreature(Kind::HighDetail));
	EXPECT_EQ(Name(Kind::HighDetail), "high detail");

	// The object must be one of the scenario's
	Scenario wrong = *scenario;
	wrong.commands[1].object = 1;
	EXPECT_EQ(Problems(wrong).size(), 1u);
}

TEST(TestbedScenarios, TheSkipKeyScenarioHoldsTheKeypadsEnterForASecond)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("dialogue.skip_key");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->commands.size(), 3u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::ShowText);
	EXPECT_EQ(scenario->commands[1].kind, Kind::KeyDown);
	EXPECT_EQ(scenario->commands[1].value, static_cast<size_t>(SDL_SCANCODE_KP_ENTER));
	EXPECT_EQ(scenario->commands[2].kind, Kind::KeyUp);
	EXPECT_EQ(scenario->commands[2].value, scenario->commands[1].value);
	EXPECT_FLOAT_EQ(scenario->commands[2].delaySeconds, 1.0f);
	for (const auto kind : {Kind::KeyDown, Kind::KeyUp})
	{
		EXPECT_TRUE(NeedsNoCreature(kind));
	}
	EXPECT_EQ(Name(Kind::KeyDown), "key down");
	EXPECT_EQ(Name(Kind::KeyUp), "key up");
}

TEST(TestbedHeldKeys, AKeyLeftDownIsLetGoWhenTheScenarioStops)
{
	HeldKeys keys;
	EXPECT_TRUE(keys.Down(88));
	EXPECT_FALSE(keys.Down(88)); // already down
	EXPECT_TRUE(keys.Down(40));
	EXPECT_TRUE(keys.Up(40));
	EXPECT_FALSE(keys.Up(40)); // not down any more
	// the one never let go is given back to be let go, and none is held into the next scenario
	EXPECT_EQ(keys.TakeAll(), std::vector<uint32_t> {88});
	EXPECT_FALSE(keys.IsDown(88));
	EXPECT_TRUE(keys.TakeAll().empty());
}

TEST(TestbedHeldKeys, CreatureModesCameraKeysAreTheBoundShiftOrCtrlAndCursorKey)
{
	const auto& bindings = input::k_DefaultKeyBindings;
	// Shift (the game's Tilt/Rotate On) or Ctrl (Zoom On) first, then the cursor key: left, right, up, down
	EXPECT_EQ(CameraKeysOf(bindings, 0, false), (std::array {SDL_SCANCODE_RSHIFT, SDL_SCANCODE_LEFT}));
	EXPECT_EQ(CameraKeysOf(bindings, 1, false), (std::array {SDL_SCANCODE_RSHIFT, SDL_SCANCODE_RIGHT}));
	EXPECT_EQ(CameraKeysOf(bindings, 2, true), (std::array {SDL_SCANCODE_RCTRL, SDL_SCANCODE_UP}));
	EXPECT_EQ(CameraKeysOf(bindings, 3, true), (std::array {SDL_SCANCODE_RCTRL, SDL_SCANCODE_DOWN}));
	EXPECT_FALSE(CameraKeysOf(bindings, 4, false).has_value());
	// A key bound only with a modifier is not a key of its own
	EXPECT_FALSE(PlainKeyOf(bindings, input::BindableActionMap::QUICK_SAVE).has_value());
	// Rebound, the keys follow the bindings
	auto rebound = bindings;
	for (auto& binding : rebound)
	{
		if (binding.action == input::BindableActionMap::MOVE_LEFT)
		{
			binding.key = input::KeyChord {SDL_SCANCODE_J};
		}
	}
	EXPECT_EQ(CameraKeysOf(rebound, 0, false), (std::array {SDL_SCANCODE_RSHIFT, SDL_SCANCODE_J}));
}

TEST(TestbedScenarios, CreatureModeIsDoubleClickedAndTurnedWithTheCameraKeys)
{
	using Kind = Command::Kind;
	const auto* other = Find("creature_mode.other_god");
	ASSERT_NE(other, nullptr);
	EXPECT_TRUE(Problems(*other).empty());
	ASSERT_EQ(other->creatures.size(), 2u);
	EXPECT_EQ(other->creatures[1].owner, PlayerNames::PLAYER_TWO);
	ASSERT_FALSE(other->commands.empty());
	// The other god's lion is double clicked
	EXPECT_EQ(other->commands[0].kind, Kind::DoubleClick);
	EXPECT_EQ(other->commands[0].creature, 1u);
	const auto* orbit = Find("creature_mode.orbit");
	ASSERT_NE(orbit, nullptr);
	EXPECT_TRUE(Problems(*orbit).empty());
	size_t held = 0;
	for (const auto& command : orbit->commands)
	{
		if (command.kind == Kind::CameraKeys)
		{
			++held;
			EXPECT_TRUE(CameraKeysOf(input::k_DefaultKeyBindings, command.value, command.ctrl).has_value());
			EXPECT_GT(command.amount, 0.0f);
		}
	}
	EXPECT_EQ(held, 5u);
	// A camera key out of the four, or held for no time, is a problem
	Scenario wrong = *orbit;
	wrong.commands = {{.kind = Kind::CameraKeys, .value = 4, .amount = 1.0f}};
	EXPECT_FALSE(Problems(wrong).empty());
	wrong.commands = {{.kind = Kind::CameraKeys, .value = 0, .amount = 0.0f}};
	EXPECT_FALSE(Problems(wrong).empty());
}

TEST(TestbedScenarios, TwoVorticesLevelTheLakesBankWhereTheirSquaresMeet)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("miracles.vortex_levels_uneven_land");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Miracles);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	ASSERT_EQ(scenario->commands.size(), 4u);
	const auto& commands = scenario->commands;
	// The way out of a land first, the way in a second later; the second fades out at 12 seconds, the first at 20
	EXPECT_EQ(commands[0].kind, Kind::CreateVortex);
	EXPECT_EQ(commands[0].value, 0u);
	EXPECT_EQ(commands[1].kind, Kind::CreateVortex);
	EXPECT_EQ(commands[1].value, 1u);
	EXPECT_EQ(commands[2].kind, Kind::VortexFadeOut);
	EXPECT_EQ(commands[2].value, 1u);
	EXPECT_EQ(commands[3].kind, Kind::VortexFadeOut);
	EXPECT_EQ(commands[3].value, 0u);
	EXPECT_FLOAT_EQ(commands[0].delaySeconds + commands[1].delaySeconds + commands[2].delaySeconds, 12.0f);
	EXPECT_FLOAT_EQ(commands[0].delaySeconds + commands[1].delaySeconds + commands[2].delaySeconds + commands[3].delaySeconds,
	                20.0f);
	for (const auto kind : {Kind::CreateVortex, Kind::VortexFadeIn, Kind::VortexFadeOut})
	{
		EXPECT_TRUE(NeedsNoCreature(kind));
	}
	EXPECT_EQ(Name(Kind::CreateVortex), "create vortex");
	EXPECT_EQ(Name(Kind::VortexFadeIn), "vortex fades in");
	EXPECT_EQ(Name(Kind::VortexFadeOut), "vortex fades out");

	// The squares, 11 cells round each vortex's cell, share five columns, and the land there is not flat
	const auto cellOf = [](glm::vec2 offset) {
		const auto point = MapPoint(flat_land::k_MapMiddle, offset);
		return glm::ivec2(static_cast<int>(point.x * 0.1f), static_cast<int>(point.y * 0.1f));
	};
	const auto first = cellOf(commands[0].point);
	const auto second = cellOf(commands[1].point);
	const auto low = glm::max(first, second) - 5;
	const auto high = glm::min(first, second) + 5;
	EXPECT_EQ(high.x - low.x + 1, 5);
	EXPECT_EQ(high.y - low.y + 1, 11);
	std::set<uint8_t> altitudes;
	for (int x = low.x; x <= high.x; ++x)
	{
		for (int z = low.y; z <= high.y; ++z)
		{
			altitudes.insert(flat_land::Altitude(x, z));
		}
	}
	EXPECT_GT(altitudes.size(), 1u);

	// Only the two land vortices and the volcano's mouth can be made
	auto broken = *scenario;
	broken.commands = {{.kind = Kind::CreateVortex, .value = 3}};
	EXPECT_EQ(Problems(broken).size(), 1u);
}

TEST(TestbedScenarios, TheTempleQuietensTheLandsSoundEffects)
{
	using Kind = Command::Kind;
	// Two teleport stones cast by hand, a villager walking through them and one dropped on the first, then the
	// temple's rooms opened and the creature's room entered at once
	const auto* scenario = Find("audio.temple_quiet");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Audio);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_FALSE(scenario->environment.dispenserGrid);
	ASSERT_EQ(scenario->miracles.size(), 2u);
	for (const auto& miracle : scenario->miracles)
	{
		EXPECT_EQ(miracle.type, MagicType::Teleport);
		EXPECT_TRUE(miracle.byHand);
	}
	ASSERT_EQ(scenario->objects.size(), 2u);
	EXPECT_TRUE(scenario->objects[0].walkTo.has_value());
	EXPECT_TRUE(scenario->objects[0].walkRepeatSeconds.has_value());
	EXPECT_TRUE(scenario->objects[1].dropOnStoneSeconds.has_value());
	ASSERT_EQ(scenario->creatures.size(), 1u);
	ASSERT_EQ(scenario->commands.size(), 2u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::OpenTempleRooms);
	EXPECT_EQ(scenario->commands[1].kind, Kind::OpenCreatureCave);
	EXPECT_EQ(scenario->commands[1].creature, 0u);
	EXPECT_EQ(scenario->commands[1].delaySeconds, 0.0f);
	// The temple opens after the first walk through the stones and before the dropped villager and the walk back
	EXPECT_GT(scenario->commands[0].delaySeconds, scenario->objects[0].walkAfterSeconds);
	EXPECT_LT(scenario->commands[0].delaySeconds, *scenario->objects[1].dropOnStoneSeconds);
	EXPECT_LT(scenario->commands[0].delaySeconds,
	          scenario->objects[0].walkAfterSeconds + *scenario->objects[0].walkRepeatSeconds);
}

TEST(TestbedScenarios, ShowsTheDialogueUnderTheFallingSpellsFilm)
{
	using Kind = Command::Kind;
	// Two texts, then the falling spell's film on the second text's frame, with no creature and the clock standing still
	const auto* scenario = Find("dialogue.under_falling_film");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Advisors);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_FALSE(scenario->environment.clockRuns);
	ASSERT_EQ(scenario->commands.size(), 3u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::ShowText);
	EXPECT_EQ(scenario->commands[1].kind, Kind::ShowText);
	EXPECT_EQ(scenario->commands[2].kind, Kind::PlayFilm);
	EXPECT_EQ(scenario->commands[2].value, 1u);
	EXPECT_EQ(scenario->commands[2].delaySeconds, 0.0f);
	EXPECT_TRUE(NeedsNoCreature(Kind::ShowText));
	EXPECT_TRUE(NeedsNoCreature(Kind::PlayFilm));
	EXPECT_EQ(Name(Kind::ShowText), "show text");
	EXPECT_EQ(Name(Kind::PlayFilm), "play film");

	// A text past the last one and a third film are found
	auto broken = *scenario;
	broken.commands = {{.kind = Kind::ShowText, .value = helptext::k_TextCount}, {.kind = Kind::PlayFilm, .value = 2}};
	EXPECT_EQ(Problems(broken).size(), 2u);
}

TEST(TestbedScenarios, TapsADidYouKnowSignForItsTipBubble)
{
	using ecs::script_highlight::Info;
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::TapHighlight));
	EXPECT_EQ(Name(Kind::TapHighlight), "tap highlight");
	for (const auto* id : {"help.tip_bubble", "help.tip_bubble_edge"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_EQ(scenario->facet, Facet::Advisors) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty()) << id;
		EXPECT_TRUE(scenario->creatures.empty()) << id;
		EXPECT_FALSE(scenario->environment.clockRuns) << id;
		// One did-you-know sign with a tip, and nothing else on the land
		const auto& highlights = scenario->fixtures.highlights;
		ASSERT_EQ(highlights.size(), 1u) << id;
		EXPECT_EQ(highlights[0].info, Info::DidYouKnow) << id;
		ASSERT_TRUE(highlights[0].tip.has_value()) << id;
		EXPECT_LT(highlights[0].tip->text, helptext::k_TextCount) << id;
		auto rest = scenario->fixtures;
		rest.highlights.clear();
		EXPECT_TRUE(testbed_fixtures::Empty(rest)) << id;
		// The hand taps the sign after a second
		ASSERT_EQ(scenario->commands.size(), 1u) << id;
		EXPECT_EQ(scenario->commands[0].kind, Kind::TapHighlight) << id;
		EXPECT_EQ(scenario->commands[0].point, std::get<glm::vec2>(highlights[0].at)) << id;
		EXPECT_FLOAT_EQ(scenario->commands[0].delaySeconds, 1.0f) << id;
		ASSERT_EQ(scenario->framing.shot, Shot::Placed) << id;
	}
	// The edge's sign stands west of the camera's line, the other on it
	EXPECT_LT(std::get<glm::vec2>(Find("help.tip_bubble_edge")->fixtures.highlights[0].at).x, -10.0f);
	EXPECT_EQ(std::get<glm::vec2>(Find("help.tip_bubble")->fixtures.highlights[0].at).x, 0.0f);

	// A tap with no highlight set out, and a tip on a scroll, are found
	auto broken = *Find("help.tip_bubble");
	broken.fixtures.highlights.clear();
	EXPECT_EQ(Problems(broken).size(), 1u);
	auto scroll = Find("help.tip_bubble")->fixtures;
	scroll.highlights[0].info = Info::Gold;
	EXPECT_EQ(testbed_fixtures::Problems(scroll, std::nullopt).size(), 1u);
}

TEST(TestbedScenarios, OpensTheCreatureGateAndLaysItsStones)
{
	EXPECT_TRUE(NeedsNoCreature(Command::Kind::SetOpenClose));
	EXPECT_TRUE(NeedsNoCreature(Command::Kind::LayGateStone));
	EXPECT_EQ(Name(Command::Kind::SetOpenClose), "open or close");
	EXPECT_EQ(Name(Command::Kind::LayGateStone), "lay gate stone");
	for (const auto* id : {"objects.creature_gate_open", "objects.creature_gate_close"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_EQ(scenario->facet, Facet::Objects) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		// the gate and the plinth, then the three stones
		ASSERT_EQ(scenario->objects.size(), 5u) << id;
		EXPECT_TRUE(std::holds_alternative<AnimatedStaticInfo>(scenario->objects[0].type)) << id;
		EXPECT_TRUE(std::holds_alternative<AnimatedStaticInfo>(scenario->objects[1].type)) << id;
		EXPECT_TRUE(std::holds_alternative<MobileStaticInfo>(scenario->objects[2].type)) << id;
	}
	// the commands' objects are checked, with no creature in the scenario
	Scenario scenario {
	    .id = "test.gate",
	    .name = "Gate",
	    .description = "A plinth and a stone",
	    .expected = "Laid",
	    .objects = {{.type = AnimatedStaticInfo::GateStonePlinth}, {.type = MobileStaticInfo::GateTotemTiger}},
	    .commands = {{.kind = Command::Kind::LayGateStone, .value = 0, .object = 1},
	                 {.kind = Command::Kind::SetOpenClose, .value = 1, .object = 0}},
	};
	EXPECT_TRUE(Problems(scenario).empty());
	scenario.commands = {{.kind = Command::Kind::LayGateStone, .value = 2, .object = 1},
	                     {.kind = Command::Kind::LayGateStone, .value = 0, .object = 2},
	                     {.kind = Command::Kind::SetOpenClose, .value = 1, .object = 2}};
	EXPECT_EQ(Problems(scenario).size(), 3u);
}

// Two bookmarks dropped in front of a still camera and one on a creature that walks and goes, logged, turned off and
// on, hidden behind the cinema bars, and a flight back to the first from another view
TEST(TestbedScenarios, DropsTwoCameraBookmarksAndLogsTheirMarkers)
{
	using Kind = Command::Kind;
	for (const auto kind :
	     {Kind::SetCameraBookmark, Kind::ShowCameraBookmarks, Kind::LogCameraBookmarks, Kind::TapCameraBookmarkKey})
	{
		EXPECT_TRUE(NeedsNoCreature(kind)) << Name(kind);
	}
	const auto* scenario = Find("hand.camera_bookmarks");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 1u);
	const auto& commands = scenario->commands;
	const auto count = [&commands](Kind kind) { return std::ranges::count(commands, kind, &Command::kind); };
	EXPECT_EQ(count(Kind::SetCameraBookmark), 3);
	EXPECT_EQ(std::ranges::count_if(
	              commands,
	              [](const Command& command) { return command.kind == Kind::SetCameraBookmark && command.atCreature == 0u; }),
	          1);
	EXPECT_EQ(count(Kind::WalkTo), 1);
	EXPECT_EQ(count(Kind::RemoveCreature), 1);
	// Shift with key 3 twice (follow, stop), key 1 alone (the flight), Shift and Ctrl with key 2 (the clear), Backspace
	const auto taps = [&commands](size_t key, bool shift, bool ctrl) {
		return std::ranges::count_if(commands, [&](const Command& command) {
			return command.kind == Kind::TapCameraBookmarkKey && command.value == key && command.shift == shift &&
			       command.ctrl == ctrl;
		});
	};
	EXPECT_EQ(taps(2, true, false), 2);
	EXPECT_EQ(taps(0, false, false), 1);
	EXPECT_EQ(taps(1, true, true), 1);
	EXPECT_EQ(taps(8, false, false), 1);
	EXPECT_GE(count(Kind::LogCameraBookmarks), 3);
	EXPECT_EQ(count(Kind::ShowCameraBookmarks), 2);
	EXPECT_EQ(count(Kind::WideScreen), 2);
	// the two are the first two bookmarks, apart, and the camera is placed and looks between them
	EXPECT_EQ(commands.front().value, 0u);
	EXPECT_EQ(commands.at(1).value, 1u);
	EXPECT_NE(commands.front().point, commands.at(1).point);
	EXPECT_EQ(scenario->framing.shot, Shot::Placed);
	// the markers are on again, and the bars gone, when it ends
	const auto last = [&commands](Kind kind) { return std::ranges::find(std::views::reverse(commands), kind, &Command::kind); };
	EXPECT_EQ(last(Kind::ShowCameraBookmarks)->value, 1u);
	EXPECT_EQ(last(Kind::WideScreen)->value, 0u);
}

TEST(TestbedScenarios, PutsTheHandAwayOverTreesWithAScriptsCinemaBars)
{
	EXPECT_TRUE(NeedsNoCreature(Command::Kind::WideScreen));
	EXPECT_EQ(Name(Command::Kind::WideScreen), "wide screen");
	const auto* scenario = Find("hand.put_away_over_trees");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Hand);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	// a rock for the hand to carry, and palms together on the other side of the middle
	ASSERT_EQ(scenario->objects.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<MobileStaticInfo>(scenario->objects.front().type));
	// small enough to lift: a volcanic rock's radius is 3.91 at scale 1, and the hand lifts none over 3.6
	EXPECT_LE(scenario->objects.front().scale * 3.9134355f, 3.6f);
	ASSERT_EQ(scenario->fixtures.trees.size(), 1u);
	EXPECT_EQ(scenario->fixtures.trees.front().type, TreeInfo::Palm);
	EXPECT_GT(scenario->fixtures.trees.front().count, 1u);
	// the right button takes the rock before the bars come on
	const auto& commands = scenario->commands;
	const auto barsOn = std::ranges::find_if(
	    commands, [](const Command& command) { return command.kind == Command::Kind::WideScreen && command.value == 1; });
	const auto barsOff = std::ranges::find_if(
	    commands, [](const Command& command) { return command.kind == Command::Kind::WideScreen && command.value == 0; });
	ASSERT_NE(barsOn, commands.end());
	ASSERT_NE(barsOff, commands.end());
	ASSERT_LT(barsOn, barsOff);
	EXPECT_TRUE(std::any_of(commands.begin(), barsOn, [](const Command& command) {
		return command.kind == Command::Kind::PointerPress && command.value == 3;
	}));
	// the button is held past the hand's grab wait (225 ms), so the press takes the rock up rather than tapping it
	const auto press = std::ranges::find_if(
	    commands, [](const Command& command) { return command.kind == Command::Kind::PointerPress && command.value == 3; });
	ASSERT_NE(press + 1, commands.end());
	EXPECT_EQ((press + 1)->kind, Command::Kind::PointerRelease);
	EXPECT_GT((press + 1)->delaySeconds, 0.225f);
	// the pointer moves while they are up, and comes back to where the bars found it, over the palms
	glm::vec2 moved {0.0f};
	size_t sweeps = 0;
	float upFor = 0.0f;
	for (auto command = barsOn + 1; command <= barsOff; ++command)
	{
		upFor += command->delaySeconds;
		if (command->kind == Command::Kind::PointerSweep)
		{
			moved += command->point;
			++sweeps;
		}
	}
	EXPECT_GT(sweeps, 1u);
	EXPECT_NEAR(moved.x, 0.0f, k_Epsilon);
	EXPECT_NEAR(moved.y, 0.0f, k_Epsilon);
	EXPECT_FLOAT_EQ(upFor, 3.5f);
	// and the hand carries the rock away once they have gone
	EXPECT_EQ(commands.back().kind, Command::Kind::PointerSweep);
	// the bars only come on or go
	auto broken = *scenario;
	broken.commands = {{.kind = Command::Kind::WideScreen, .value = 2}};
	EXPECT_EQ(Problems(broken).size(), 1u);
}

TEST(TestbedScenarios, PullsBeechesByTheGripAndTheCrownAndOneTooHeavyToComeOut)
{
	const auto pressed = [](const Scenario& scenario) {
		return std::ranges::any_of(scenario.commands, [](const Command& command) {
			return command.kind == Command::Kind::PointerPress && command.value == 3;
		});
	};
	for (const auto* id : {"hand.tug_at_grip", "hand.tug_at_crown", "hand.tug_let_go", "hand.tug_heavy_tree"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_EQ(scenario->facet, Facet::Hand) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		EXPECT_TRUE(scenario->creatures.empty()) << id;
		// one beech, pressed with the right button, which is let go at the end
		ASSERT_EQ(scenario->objects.size(), 1u) << id;
		ASSERT_TRUE(std::holds_alternative<TreeInfo>(scenario->objects.front().type)) << id;
		EXPECT_EQ(std::get<TreeInfo>(scenario->objects.front().type), TreeInfo::Beech) << id;
		EXPECT_TRUE(pressed(*scenario)) << id;
		EXPECT_EQ(scenario->commands.back().kind, Command::Kind::PointerRelease) << id;
		// the camera looks at the point under the pointer, never level: a level line through the middle of the screen
		// never meets the pull's level plane
		EXPECT_EQ(scenario->framing.shot, Shot::Placed) << id;
		EXPECT_NE(scenario->framing.eye.y, scenario->framing.look.y) << id;
	}
	// The beech weighs scale^3 x 1000: the heavy one more than the hand's greatest pull (600000) over gravity lets it
	// lift, the others less
	const auto weight = [](const char* id) {
		const float scale = Find(id)->objects.front().scale;
		return scale * scale * scale * 1000.0f;
	};
	EXPECT_GT(weight("hand.tug_heavy_tree") * 9.81f, 600000.0f);
	EXPECT_LT(weight("hand.tug_at_grip") * 9.81f, 600000.0f);
	EXPECT_LT(weight("hand.tug_at_crown") * 9.81f, 600000.0f);
	// pressed at the grip (a tenth of its 18 metres up) and at the crown, far above it but below its top leaves (about 13
	// metres up)
	EXPECT_FLOAT_EQ(Find("hand.tug_at_grip")->framing.look.y, 1.8f);
	EXPECT_GT(Find("hand.tug_at_crown")->framing.look.y, 10.0f);
	EXPECT_LT(Find("hand.tug_at_crown")->framing.look.y, 13.0f);
}

TEST(TestbedScenarios, DrawsTheShieldsFirstCircleWithThePointerOnTheLand)
{
	const auto* scenario = Find("miracles.gesture_circle_shield");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	// the pointer comes onto the land before the first circle is drawn, so that the press is inside the influence
	const auto& commands = scenario->commands;
	const auto firstDraw =
	    std::ranges::find_if(commands, [](const Command& command) { return command.kind == Command::Kind::DrawGesture; });
	ASSERT_NE(firstDraw, commands.end());
	EXPECT_TRUE(std::any_of(commands.begin(), firstDraw,
	                        [](const Command& command) { return command.kind == Command::Kind::PointerTo; }));
	// the repeat starts at the seed given again, after the first circle
	ASSERT_TRUE(scenario->repeatFrom.has_value());
	ASSERT_LT(*scenario->repeatFrom, commands.size());
	EXPECT_EQ(commands[*scenario->repeatFrom].kind, Command::Kind::HoldSeed);
	EXPECT_GT(*scenario->repeatFrom, static_cast<size_t>(firstDraw - commands.begin()));
}

TEST(TestbedScenarios, SweepsTheHandFromTheLandAcrossTheLakeToTheSky)
{
	const auto* scenario = Find("hand.land_to_sky");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Hand);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_TRUE(scenario->objects.empty());
	// the camera south of the lake, looking north across it
	ASSERT_EQ(scenario->framing.shot, Shot::Placed);
	const glm::vec2 lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	EXPECT_LT(scenario->framing.eye.z, lake.y - flat_land::k_LakeHalfExtent.y);
	EXPECT_GT(scenario->framing.look.z, lake.y + flat_land::k_LakeHalfExtent.y);
	// the pointer starts low over the plane and sweeps up past the horizon, then part of the way back down
	ASSERT_EQ(scenario->commands.size(), 3u);
	EXPECT_EQ(scenario->commands[0].kind, Command::Kind::PointerTo);
	EXPECT_GT(scenario->commands[0].point.y, 0.8f);
	EXPECT_EQ(scenario->commands[1].kind, Command::Kind::PointerSweep);
	EXPECT_LT(scenario->commands[0].point.y + scenario->commands[1].point.y, 0.1f);
	EXPECT_EQ(scenario->commands[2].kind, Command::Kind::PointerSweep);
	EXPECT_GT(scenario->commands[2].point.y, 0.0f);
	// the sweep back starts once the sweep up is over
	EXPECT_GT(scenario->commands[2].delaySeconds, scenario->commands[1].amount);
}

TEST(TestbedScenarios, OpensTheVorticesAsALandsScriptDoes)
{
	using Kind = Command::Kind;
	const auto* bank = Find("story.vortex_levels_the_bank");
	const auto* hole = Find("story.vortex_opens_a_hole");
	const auto* volcano = Find("story.volcano_vortex_keeps_the_land");
	ASSERT_NE(bank, nullptr);
	ASSERT_NE(hole, nullptr);
	ASSERT_NE(volcano, nullptr);
	for (const auto* scenario : {bank, hole, volcano})
	{
		EXPECT_TRUE(Problems(*scenario).empty()) << scenario->id;
		EXPECT_EQ(scenario->commands.front().kind, Kind::CreateVortex) << scenario->id;
	}
	// Two ways out of a land, both told to fade out 20 seconds after they open
	ASSERT_EQ(bank->commands.size(), 4u);
	EXPECT_EQ(bank->commands[1].kind, Kind::CreateVortex);
	EXPECT_EQ(bank->commands[1].delaySeconds, 0.0f);
	EXPECT_EQ(bank->commands[2].kind, Kind::VortexFadeOut);
	EXPECT_FLOAT_EQ(bank->commands[2].delaySeconds, 20.0f);
	// The volcano's mouth is the third kind, and stays
	ASSERT_EQ(volcano->commands.size(), 1u);
	EXPECT_EQ(volcano->commands.front().value, 2u);
	// No fourth kind
	Scenario wrong = *volcano;
	wrong.commands.front().value = 3;
	EXPECT_EQ(Problems(wrong).size(), 1u);
}

TEST(TestbedScenarios, AWayOutPullsThingsInAndAWayInThrowsThemOut)
{
	using Kind = Command::Kind;
	const auto* in = Find("story.vortex_pulls_things_in");
	const auto* out = Find("story.vortex_throws_things_out");
	ASSERT_NE(in, nullptr);
	ASSERT_NE(out, nullptr);
	for (const auto* scenario : {in, out})
	{
		EXPECT_TRUE(Problems(*scenario).empty()) << scenario->id;
		EXPECT_EQ(scenario->commands.front().kind, Kind::CreateVortex) << scenario->id;
	}
	// the way out of a land, among seven things; the way into one, told to fade out
	EXPECT_EQ(in->commands.front().value, 0u);
	EXPECT_EQ(in->objects.size(), 7u);
	EXPECT_EQ(out->commands.front().value, 1u);
	ASSERT_EQ(out->commands.size(), 2u);
	EXPECT_EQ(out->commands[1].kind, Kind::VortexFadeOut);
	// a rock out of reach, thrown at the open way out of a land
	const auto* thrown = Find("story.vortex_takes_what_is_thrown_in");
	ASSERT_NE(thrown, nullptr);
	EXPECT_TRUE(Problems(*thrown).empty());
	ASSERT_EQ(thrown->throws.size(), 1u);
	EXPECT_GT(thrown->throws.front().delaySeconds, 8.0f); // after its 1 second and its 7 second fade in
}

TEST(TestbedScenarios, AWayInThrowsOutACrossingFileOfItsOwn)
{
	using Kind = Command::Kind;
	EXPECT_EQ(Name(Kind::VortexCrossingFile), "vortex crossing file");
	const auto* scenario = Find("story.vortex_throws_out_the_last_crossing");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	// a way into a land, given its file in the same frame, before its first emission
	ASSERT_EQ(scenario->commands.size(), 2u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::CreateVortex);
	EXPECT_EQ(scenario->commands[0].value, 1u);
	const auto& file = scenario->commands[1];
	EXPECT_EQ(file.kind, Kind::VortexCrossingFile);
	EXPECT_EQ(file.delaySeconds, 0.0f);
	EXPECT_EQ(file.value, 0u);
	// a farmer, a cow, a blank line, a rock, and a farmer on a last line with no new line after it
	const auto reader = ecs::vortex_save::ReaderFrom(file.crossingFile);
	ASSERT_EQ(reader.lines.size(), 5u);
	EXPECT_TRUE(reader.lines[0].starts_with("CREATE_TOWN_VILLAGER(-1, \"0.00,0.00\", CELTIC_FARMER, 30)"));
	EXPECT_TRUE(reader.lines[1].starts_with("CREATE_NEW_ANIMAL("));
	EXPECT_TRUE(reader.lines[2].empty());
	EXPECT_TRUE(reader.lines[3].starts_with("CREATE_MOBILE_STATIC("));
	EXPECT_EQ(reader.lines[4], "CREATE_TOWN_VILLAGER(-1, \"0.00,0.00\", CELTIC_FARMER, 40)");
	EXPECT_FALSE(reader.lastLineEnded);
	// a file is needed
	Scenario wrong = *scenario;
	wrong.commands[1].crossingFile = {};
	EXPECT_EQ(Problems(wrong).size(), 1u);
}

TEST(TestbedScenarios, PutsAFishFarmOnTheLakesShoreWithAHutOfItsTown)
{
	for (const auto* id : {"hand.fish_farm", "hand.fish_scoop", "hand.fish_scare", "needs.fish_and_eat"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		// The hut first, then the farm
		ASSERT_EQ(scenario->objects.size(), 2u) << id;
		EXPECT_TRUE(std::holds_alternative<AbodeInfo>(scenario->objects[0].type)) << id;
		EXPECT_EQ(std::get<FishFarmInfo>(scenario->objects[1].type), FishFarmInfo::Normal) << id;
		// A building holds the scenario town's usual stock unless told otherwise
		EXPECT_FALSE(scenario->objects[0].storedFood.has_value()) << id;
	}
}

TEST(TestbedScenarios, SlidesATownCentresTotemWithTheHand)
{
	EXPECT_TRUE(NeedsNoCreature(Command::Kind::CreateWorshipSite));
	EXPECT_EQ(Name(Command::Kind::CreateWorshipSite), "create worship site");
	// the totem hidden and shown again by a script's HEALTH and BUILT_PERCENTAGE, on the town centre's totem
	const auto* hidden = Find("objects.village_totem_hidden");
	ASSERT_NE(hidden, nullptr);
	EXPECT_TRUE(Problems(*hidden).empty());
	EXPECT_EQ(Name(Command::Kind::ScriptTotemProperty), "script totem property");
	EXPECT_EQ(std::ranges::count(hidden->commands, Command::Kind::ScriptTotemProperty, &Command::kind), 4);
	const auto* scenario = Find("objects.village_totem_drag");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Objects);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	// the town centre and a villager of its town, the player's temple, and its site built before the hand presses
	ASSERT_EQ(scenario->objects.size(), 2u);
	EXPECT_TRUE(std::holds_alternative<AbodeInfo>(scenario->objects[0].type));
	EXPECT_TRUE(scenario->objects[1].joinTown);
	EXPECT_EQ(scenario->fixtures.temples.size(), 1u);
	// its town stays the player's: a town that believes in no one goes to the neutral player at its first fold
	ASSERT_TRUE(scenario->townBelief.has_value());
	EXPECT_GT(*scenario->townBelief, 0.0f);
	ASSERT_FALSE(scenario->commands.empty());
	EXPECT_EQ(scenario->commands.front().kind, Command::Kind::CreateWorshipSite);
	// the Action button down, the mouse up the screen, the button let go
	std::vector<Command::Kind> pointer;
	for (const auto& command : scenario->commands)
	{
		if (command.kind == Command::Kind::PointerPress || command.kind == Command::Kind::PointerSweep ||
		    command.kind == Command::Kind::PointerRelease)
		{
			pointer.push_back(command.kind);
		}
	}
	EXPECT_EQ(pointer, (std::vector {Command::Kind::PointerPress, Command::Kind::PointerSweep, Command::Kind::PointerRelease}));
	// a tribe the game does not have is found
	auto broken = *scenario;
	broken.commands = {{.kind = Command::Kind::CreateWorshipSite, .value = static_cast<size_t>(Tribe::_COUNT)}};
	EXPECT_EQ(Problems(broken).size(), 1u);
}

TEST(TestbedScenarios, StandsATownCentresTotemWithoutAndWithAWorshipSite)
{
	const auto* bare = Find("objects.village_totem");
	const auto* worship = Find("objects.village_totem_worship");
	ASSERT_NE(bare, nullptr);
	ASSERT_NE(worship, nullptr);
	EXPECT_TRUE(Problems(*bare).empty());
	EXPECT_TRUE(Problems(*worship).empty());
	// The same centre; only the second has a villager and the player's temple, which opens the worship site
	ASSERT_EQ(bare->objects.size(), 1u);
	ASSERT_EQ(worship->objects.size(), 2u);
	EXPECT_EQ(std::get<AbodeInfo>(bare->objects[0].type), AbodeInfo::NorseTownCentre);
	EXPECT_EQ(worship->objects[0].offset, bare->objects[0].offset);
	EXPECT_TRUE(worship->objects[1].joinTown);
	EXPECT_TRUE(bare->fixtures.temples.empty());
	EXPECT_EQ(worship->fixtures.temples.size(), 1u);
}

TEST(TestbedScenarios, ShowsATownCentresTotemWearingThePlayersCreatureIcon)
{
	const auto* scenario = Find("objects.village_totem_creature_icon");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Objects);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	// the town centre of the player's town, and the player's creature, its profile's, kept away from the shot
	ASSERT_FALSE(scenario->objects.empty());
	EXPECT_TRUE(std::holds_alternative<AbodeInfo>(scenario->objects[0].type));
	ASSERT_TRUE(scenario->townBelief.has_value());
	EXPECT_GT(*scenario->townBelief, 0.0f);
	ASSERT_EQ(scenario->fixtures.creatures.size(), 1u);
	EXPECT_EQ(scenario->fixtures.creatures[0].owner, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(scenario->fixtures.creatures[0].file.empty());
	EXPECT_EQ(scenario->fixtures.creatures[0].hold, testbed_fixtures::Hold::Penned);
	// a placed shot looking north at the totem in the middle of the centre
	ASSERT_EQ(scenario->framing.shot, Shot::Placed);
	EXPECT_FLOAT_EQ(scenario->framing.look.x, scenario->objects[0].offset.x);
	EXPECT_FLOAT_EQ(scenario->framing.look.z, scenario->objects[0].offset.y);
	EXPECT_LT(scenario->framing.eye.z, scenario->framing.look.z);
}

TEST(TestbedScenarios, WalksPoisonedAndHealthyVillagersToAndFro)
{
	const auto* scenario = Find("villagers.poisoned_pause");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	ASSERT_EQ(scenario->objects.size(), 6u);
	size_t poisoned = 0;
	for (const auto& villager : scenario->objects)
	{
		EXPECT_TRUE(std::holds_alternative<VillagerInfo>(villager.type));
		EXPECT_EQ(villager.life, std::optional<float>(0.5f));
		// Each walks to and fro, arriving each time, so that its state changes every few seconds
		ASSERT_TRUE(villager.walkTo.has_value());
		ASSERT_TRUE(villager.walkRepeatSeconds.has_value());
		EXPECT_LE(*villager.walkRepeatSeconds, 5.0f);
		// Arriving into a state that may pause
		EXPECT_EQ(villager.walkFinal, VillagerStates::NothingToDo);
		poisoned += villager.poisoned ? 1 : 0;
	}
	EXPECT_EQ(poisoned, 3u);
}

TEST(TestbedScenarios, WalksMenAndWomenOnAndOverTheirGaitLimits)
{
	const auto* scenario = Find("villagers.gait_limits");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->objects.size(), 8u);
	const std::array<uint16_t, 8> speeds {1966, 1967, 2621, 2622, 1311, 1312, 1966, 1967};
	for (size_t i = 0; i < 8; ++i)
	{
		const auto& walker = scenario->objects[i];
		EXPECT_EQ(std::get<VillagerInfo>(walker.type),
		          i < 4 ? VillagerInfo::NorseShepherdMale : VillagerInfo::NorseHousewifeFemale);
		ASSERT_TRUE(walker.scriptSpeed.has_value());
		EXPECT_EQ(*walker.scriptSpeed, speeds.at(i));
		ASSERT_TRUE(walker.walkTo.has_value());
		EXPECT_EQ(walker.walkTo->x, walker.offset.x);
	}
}

TEST(TestbedScenarios, SetsThreeCowsSideBySideAlight)
{
	const auto* scenario = Find("animals.dead_body_smoke");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->objects.size(), 3u);
	for (const auto& cow : scenario->objects)
	{
		EXPECT_EQ(std::get<AnimalInfo>(cow.type), AnimalInfo::Cow);
	}
	// Each of them burns
	ASSERT_EQ(scenario->fixtures.fires.size(), 3u);
	for (size_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::get<size_t>(scenario->fixtures.fires[i].what), i);
	}
}

// A burning tree with a cow either side of it, and a cow set alight on its own away from them
TEST(TestbedScenarios, SetsATreeAndACowAlightForTheAnimalsToRunFrom)
{
	const auto* scenario = Find("animals.fire_flight");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->objects.size(), 4u);
	EXPECT_EQ(std::get<TreeInfo>(scenario->objects[0].type), TreeInfo::Beech);
	for (size_t i = 1; i < 4; ++i)
	{
		EXPECT_EQ(std::get<AnimalInfo>(scenario->objects[i].type), AnimalInfo::Cow);
	}
	ASSERT_EQ(scenario->fixtures.fires.size(), 2u);
	EXPECT_EQ(std::get<size_t>(scenario->fixtures.fires[0].what), 0u);
	EXPECT_EQ(std::get<size_t>(scenario->fixtures.fires[1].what), 3u);
	EXPECT_TRUE(scenario->commands.empty());
}

TEST(TestbedScenarios, ScriptDancesAreChecked)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptDance));
	EXPECT_EQ(Name(Kind::ScriptDance), "script dance");
	Scenario dance {
	    .id = "broken.dance",
	    .name = "Broken",
	    .description = "Broken on purpose",
	    .expected = "Every problem found",
	    .objects = {{.type = MobileObjectInfo::Ball}},
	    .commands = {{.kind = Kind::ScriptDance, .value = 25}, {.kind = Kind::ScriptDance, .danceAround = 1}},
	};
	EXPECT_EQ(Problems(dance).size(), 2u);
	// The last of the game's dances, around nothing or around the scenario's object, are fine
	dance.commands = {{.kind = Kind::ScriptDance, .value = 24, .danceTime = 30.0f},
	                  {.kind = Kind::ScriptDance, .value = 0, .danceAround = 0}};
	EXPECT_TRUE(Problems(dance).empty());
	// Attaching to the dance needs an object of the scenario's
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptDanceAttach));
	EXPECT_EQ(Name(Kind::ScriptDanceAttach), "script dance attach");
	dance.commands = {{.kind = Kind::ScriptDanceAttach, .object = 1}};
	EXPECT_EQ(Problems(dance).size(), 1u);
	dance.commands = {{.kind = Kind::ScriptDanceAttach, .object = 0}};
	EXPECT_TRUE(Problems(dance).empty());
}

TEST(TestbedScenarios, TheTattooEditorIsOpenedAndASymbolDragged)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::OpenTattooEditor));
	EXPECT_EQ(Name(Kind::OpenTattooEditor), "open tattoo editor");
	const auto* scenario = Find("creature_mode.cave_tattoo_editor");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	// the temple's rooms opened and the cave at once, then the editor, then the left button down on a symbol, the
	// drag and the let-go
	ASSERT_EQ(scenario->commands.size(), 7u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::OpenTempleRooms);
	EXPECT_EQ(scenario->commands[0].delaySeconds, 1.0f);
	EXPECT_EQ(scenario->commands[1].kind, Kind::OpenCreatureCave);
	EXPECT_EQ(scenario->commands[1].delaySeconds, 0.0f);
	EXPECT_EQ(scenario->commands[2].kind, Kind::OpenTattooEditor);
	EXPECT_EQ(scenario->commands[4].kind, Kind::PointerPress);
	EXPECT_EQ(scenario->commands[4].value, 1u);
	EXPECT_EQ(scenario->commands[5].kind, Kind::PointerSweep);
	EXPECT_EQ(scenario->commands[6].kind, Kind::PointerRelease);
	EXPECT_EQ(scenario->commands[6].value, 1u);
	// its clock runs in the temple, which pauses the game
	EXPECT_TRUE(scenario->clockWhilePaused);
	// the same drag held, for a shot with the symbol under the pointer: no let-go
	const auto* dragging = Find("creature_mode.cave_tattoo_editor_dragging");
	ASSERT_NE(dragging, nullptr);
	EXPECT_TRUE(Problems(*dragging).empty());
	ASSERT_EQ(dragging->commands.size(), 6u);
	EXPECT_EQ(dragging->commands[0].kind, Kind::OpenTempleRooms);
	EXPECT_EQ(dragging->commands[5].kind, Kind::PointerSweep);
	EXPECT_TRUE(dragging->clockWhilePaused);
}

TEST(TestbedScenarios, TheBelieversGainedScenarioGivesTheTownBeliefTwiceWithThePointerOnTheLand)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::TownGainsBelief));
	EXPECT_EQ(Name(Kind::TownGainsBelief), "town gains belief");
	const auto* scenario = Find("objects.believers_gained");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	// the town is made by its centre, and believes in the player under the cap
	ASSERT_TRUE(scenario->townBelief.has_value());
	EXPECT_LT(*scenario->townBelief, 10.0f);
	ASSERT_EQ(scenario->commands.size(), 5u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::PointerTo);
	EXPECT_EQ(scenario->commands[1].kind, Kind::TownGainsBelief);
	EXPECT_EQ(scenario->commands[3].kind, Kind::TownGainsBelief);
	// a gain of nothing is a problem
	auto none = *scenario;
	none.commands = {{.kind = Kind::TownGainsBelief, .amount = 0.0f}};
	EXPECT_EQ(Problems(none).size(), 1u);
}

TEST(TestbedScenarios, TheAltarNeedsScenarioBuildsTheSiteThenSendsTheWholeTownToWorship)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::SetWorshipShare));
	EXPECT_EQ(Name(Kind::SetWorshipShare), "set worship share");
	const auto* scenario = Find("worship.altar_needs");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->commands.size(), 2u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::CreateWorshipSite);
	EXPECT_EQ(scenario->commands[1].kind, Kind::SetWorshipShare);
	EXPECT_EQ(scenario->commands[1].amount, 1.0f);
	// a share is a fraction of the town's people
	auto beyond = *scenario;
	beyond.commands = {{.kind = Kind::SetWorshipShare, .amount = 1.5f}, {.kind = Kind::SetWorshipShare, .amount = -0.1f}};
	EXPECT_EQ(Problems(beyond).size(), 2u);
	beyond.commands = {{.kind = Kind::SetWorshipShare, .amount = 0.0f}};
	EXPECT_TRUE(Problems(beyond).empty());
}

TEST(TestbedScenarios, ScriptInterfaceLevelsAreChecked)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptInterfaceLevel));
	EXPECT_EQ(Name(Kind::ScriptInterfaceLevel), "script interface level");
	Scenario levels {
	    .id = "broken.levels",
	    .name = "Broken",
	    .description = "Broken on purpose",
	    .expected = "Every problem found",
	    .commands = {{.kind = Kind::ScriptInterfaceLevel, .value = 16}},
	};
	EXPECT_EQ(Problems(levels).size(), 1u);
	// The normal level and the game's last one are fine
	levels.commands = {{.kind = Kind::ScriptInterfaceLevel, .value = 0}, {.kind = Kind::ScriptInterfaceLevel, .value = 15}};
	EXPECT_TRUE(Problems(levels).empty());
	// C under the level that leaves only grabbing, double clicks and turning, then under the normal level
	const auto* scenario = Find("creature_mode.c_key_interface_level");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->commands.size(), 4u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::ScriptInterfaceLevel);
	EXPECT_EQ(scenario->commands[0].value, 13u);
	EXPECT_EQ(scenario->commands[1].kind, Kind::PressKey);
	EXPECT_EQ(scenario->commands[2].value, 0u);
	EXPECT_EQ(scenario->commands[3].kind, Kind::PressKey);
}

TEST(TestbedScenarios, BuildsThePlayersTempleOnlyBetweenNothingAndWhole)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::TempleBuilt));
	EXPECT_EQ(Name(Kind::TempleBuilt), "temple built");
	Scenario temple {
	    .id = "test.temple_built",
	    .name = "Temple built",
	    .description = "Broken on purpose",
	    .expected = "Every problem found",
	    .commands = {{.kind = Kind::TempleBuilt, .built = -0.1f}, {.kind = Kind::TempleBuilt, .built = 1.5f}},
	};
	EXPECT_EQ(Problems(temple).size(), 2u);
	temple.commands = {{.kind = Kind::TempleBuilt, .built = 0.0f}, {.kind = Kind::TempleBuilt, .built = 1.0f}};
	EXPECT_TRUE(Problems(temple).empty());
}

TEST(TestbedScenarios, BuildsOnlyAnAbodeBeingBuiltAndOnlyBetweenNothingAndWhole)
{
	using Kind = Command::Kind;
	EXPECT_TRUE(NeedsNoCreature(Kind::AbodeBuilt));
	EXPECT_EQ(Name(Kind::AbodeBuilt), "abode built");
	Scenario abode {
	    .id = "test.abode_built",
	    .name = "Abode built",
	    .description = "Broken on purpose",
	    .expected = "Every problem found",
	    .objects = {{.type = AbodeInfo::CelticHut}, {.type = AbodeInfo::CelticHut, .underConstruction = true}},
	    .commands = {{.kind = Kind::AbodeBuilt, .object = 0, .built = 0.5f},
	                 {.kind = Kind::AbodeBuilt, .object = 1, .built = 1.5f},
	                 {.kind = Kind::AbodeBuilt, .object = 2, .built = 0.5f}},
	};
	EXPECT_EQ(Problems(abode).size(), 3u);
	abode.commands = {{.kind = Kind::AbodeBuilt, .object = 1, .built = 0.0f},
	                  {.kind = Kind::AbodeBuilt, .object = 1, .built = 1.0f}};
	EXPECT_TRUE(Problems(abode).empty());
	const auto* example = Find("examples.abode_partly_built");
	ASSERT_NE(example, nullptr);
	EXPECT_TRUE(Problems(*example).empty());
}

TEST(TestbedScenarios, TheCitadelScenarioOpensTheTempleWithTheLeashOnAndLeavesAtOnce)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("leash.off_inside_citadel");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	const std::vector<Kind> kinds {Kind::HandTapLeash, Kind::LeashState,  Kind::OpenTempleRooms, Kind::OpenCreatureCave,
	                               Kind::LeashState,   Kind::LeaveTemple, Kind::LeashState};
	ASSERT_EQ(scenario->commands.size(), kinds.size());
	for (size_t i = 0; i < kinds.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, kinds[i]) << i;
	}
	// the rooms open after the same wait the cave had; the world is paused inside the temple, so the commands in it
	// are given with no delay
	EXPECT_EQ(scenario->commands[2].delaySeconds, 1.0f);
	EXPECT_EQ(scenario->commands[3].delaySeconds, 0.0f);
	EXPECT_EQ(scenario->commands[4].delaySeconds, 0.0f);
	EXPECT_EQ(scenario->commands[5].delaySeconds, 0.0f);
	EXPECT_TRUE(NeedsNoCreature(Kind::LeaveTemple));
	EXPECT_EQ(Name(Kind::LeaveTemple), "leave temple");
}

TEST(TestbedScenarios, GivesRewardChestsOnTheLandAndFromTheSky)
{
	using Kind = Command::Kind;
	const auto* land = Find("rewards.on_land");
	const auto* sky = Find("rewards.from_sky");
	ASSERT_NE(land, nullptr);
	ASSERT_NE(sky, nullptr);
	EXPECT_TRUE(Problems(*land).empty());
	EXPECT_TRUE(Problems(*sky).empty());
	ASSERT_EQ(land->commands.size(), 3u);
	for (const auto& command : land->commands)
	{
		EXPECT_EQ(command.kind, Kind::ScriptReward);
		EXPECT_FALSE(command.fromSky);
	}
	ASSERT_EQ(sky->commands.size(), 1u);
	EXPECT_TRUE(sky->commands.front().fromSky);
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptReward));
	EXPECT_EQ(Name(Kind::ScriptReward), "script reward");
	// A type the scripts can't give is a problem
	Scenario wrong = *sky;
	wrong.commands.front().value = 61;
	EXPECT_EQ(Problems(wrong).size(), 1u);
}

// The fire carried to a creature by hand is played through the real input path: no miracle thrown and no hand moved by
// the testbed, only a tree set alight and the creature taught the water miracle
TEST(TestbedScenarios, LeavesTheRealInputFireToTheHand)
{
	using Kind = Command::Kind;
	const auto* fire = Find("realinput.fire");
	ASSERT_NE(fire, nullptr);
	EXPECT_TRUE(Problems(*fire).empty());
	EXPECT_TRUE(fire->miracles.empty());
	ASSERT_EQ(fire->creatures.size(), 1u);
	ASSERT_EQ(fire->objects.size(), 1u);
	ASSERT_EQ(fire->commands.size(), 2u);
	EXPECT_EQ(fire->commands[0].kind, Kind::KnowMiracle);
	EXPECT_EQ(fire->commands[1].kind, Kind::SetAlight);
	EXPECT_EQ(fire->commands[1].value, 1u);
}

TEST(TestbedScenarios, GivesRewardChestsToTapOpen)
{
	using Kind = Command::Kind;
	const auto* row = Find("story.reward_chests");
	const auto* close = Find("story.reward_chest_opens");
	ASSERT_NE(row, nullptr);
	ASSERT_NE(close, nullptr);
	EXPECT_TRUE(Problems(*row).empty());
	EXPECT_TRUE(Problems(*close).empty());
	// Six kinds from the sky, a second apart
	ASSERT_EQ(row->commands.size(), 6u);
	for (const auto& command : row->commands)
	{
		EXPECT_EQ(command.kind, Kind::ScriptReward);
		EXPECT_TRUE(command.fromSky);
		EXPECT_FLOAT_EQ(command.delaySeconds, 1.0f);
	}
	// One toy ball put on the land
	ASSERT_EQ(close->commands.size(), 1u);
	EXPECT_EQ(close->commands.front().kind, Kind::ScriptReward);
	EXPECT_FALSE(close->commands.front().fromSky);
	EXPECT_EQ(close->commands.front().value, static_cast<size_t>(RewardObjectInfo::RewardObjectToyBall));
}

TEST(TestbedScenarios, PutsFlocksOfBirdsAboutTheirHomes)
{
	for (const auto* id : {"birds.every_kind", "birds.close_up", "birds.seagulls_lake", "birds.many_flocks"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		EXPECT_EQ(scenario->facet, Facet::Animals) << id;
		EXPECT_FALSE(scenario->birdFlocks.empty()) << id;
	}
	// Six kinds in every_kind, the seagulls over the lake; 30 flocks of 20 in many_flocks
	EXPECT_EQ(Find("birds.every_kind")->birdFlocks.size(), 6u);
	EXPECT_EQ(Find("birds.every_kind")->birdFlocks.back().kind, AnimalInfo::Seagull);
	const auto& many = Find("birds.many_flocks")->birdFlocks;
	ASSERT_EQ(many.size(), 30u);
	EXPECT_EQ(many.front().count, 20u);
	EXPECT_EQ(Name(Facet::Animals), "Animals");
	// A scenario of only birds is not empty
	Scenario birds {.id = "x.y", .name = "n", .description = "d", .expected = "e", .birdFlocks = {{.kind = AnimalInfo::Crow}}};
	EXPECT_TRUE(Problems(birds).empty());
}

TEST(TestbedScenarios, PutsASharkInTheLakeAndWalksItAlongATrack)
{
	using Kind = Command::Kind;
	const auto lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	for (const auto* id : {"sharks.swimming", "sharks.walks_track"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		EXPECT_EQ(scenario->facet, Facet::Animals) << id;
		// One shark, made as a script's CREATE of a whale makes it, in the lake's open water
		ASSERT_EQ(scenario->objects.size(), 1u) << id;
		const auto* type = std::get_if<MobileObjectInfo>(&scenario->objects[0].type);
		ASSERT_NE(type, nullptr) << id;
		EXPECT_EQ(*type, MobileObjectInfo::Whale) << id;
		const auto from = glm::abs(scenario->objects[0].offset - lake);
		EXPECT_LT(from.x, flat_land::k_LakeHalfExtent.x) << id;
		EXPECT_LT(from.y, flat_land::k_LakeHalfExtent.y) << id;
	}
	// The swimming one is told nothing
	EXPECT_TRUE(Find("sharks.swimming")->commands.empty());

	// The other walks a stretch of track 0 forwards, again and again
	const auto* walks = Find("sharks.walks_track");
	ASSERT_EQ(walks->commands.size(), 1u);
	const auto& walk = walks->commands[0];
	EXPECT_EQ(walk.kind, Kind::ScriptWalkPath);
	EXPECT_EQ(walk.object, 0u);
	EXPECT_EQ(walk.value, 0u);
	EXPECT_FLOAT_EQ(walk.pathFrom, 0.39f);
	EXPECT_FLOAT_EQ(walk.pathTo, 0.408f);
	EXPECT_FALSE(walk.pathBackwards);
	EXPECT_EQ(walks->repeatFrom, std::optional<size_t>(0));
	// A walk starts between 0.1 and 0.4 seconds before the shot of the checks' 900 frames of 16 ms, taken 20 frames
	// before the end, so that the shot falls within its four turns
	const float shot = 880.0f * 0.016f;
	const float last = std::floor(shot / walk.delaySeconds) * walk.delaySeconds;
	EXPECT_GT(last, shot - 0.4f);
	EXPECT_LT(last, shot - 0.1f);
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptWalkPath));
	EXPECT_EQ(Name(Kind::ScriptWalkPath), "script walk path");

	// A walk of an object the scenario doesn't have, or past the track's ends, is a problem
	Scenario wrong = *walks;
	wrong.commands[0].object = 1;
	EXPECT_EQ(Problems(wrong).size(), 1u);
	wrong.commands[0].object = 0;
	wrong.commands[0].pathTo = 1.5f;
	EXPECT_EQ(Problems(wrong).size(), 1u);
}

TEST(TestbedScenarios, PutsDovesOrBatsAboutATemple)
{
	const auto* doves = Find("birds.temple_doves");
	const auto* bats = Find("birds.temple_bats");
	ASSERT_NE(doves, nullptr);
	ASSERT_NE(bats, nullptr);
	EXPECT_TRUE(Problems(*doves).empty());
	EXPECT_TRUE(Problems(*bats).empty());
	EXPECT_EQ(doves->facet, Facet::Animals);
	ASSERT_EQ(doves->fixtures.temples.size(), 1u);
	ASSERT_EQ(bats->fixtures.temples.size(), 1u);
	EXPECT_GT(doves->environment.playerAlignment.value_or(0.0f), 0.0f);
	EXPECT_LT(bats->environment.playerAlignment.value_or(0.0f), 0.0f);
	// The evil player turns good a minute in
	ASSERT_EQ(bats->commands.size(), 1u);
	EXPECT_EQ(bats->commands.front().kind, Command::Kind::SetAlignment);
	EXPECT_GT(bats->commands.front().alignment, 0.0f);
}

TEST(TestbedScenarios, LaysTwoDeadDovesBeforeATiger)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("birds.dead_doves");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->objects.size(), 2u);
	for (const auto& dove : scenario->objects)
	{
		EXPECT_EQ(std::get<AnimalInfo>(dove.type), AnimalInfo::Dove);
		ASSERT_TRUE(dove.life.has_value());
		EXPECT_EQ(*dove.life, 0.0f);
	}
	ASSERT_EQ(scenario->commands.size(), 2u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::PickUp);
	EXPECT_EQ(scenario->commands[1].kind, Kind::PutDown);
}

TEST(TestbedScenarios, ACowGoesRoundAPillarWhileAnOverlappingPillarIsDeletedOrNot)
{
	using Kind = Command::Kind;
	const auto* deleted = Find("animals.orbit_leaves_at_once");
	const auto* kept = Find("animals.orbit_goes_round");
	ASSERT_NE(deleted, nullptr);
	ASSERT_NE(kept, nullptr);
	for (const auto* scenario : {deleted, kept})
	{
		EXPECT_TRUE(Problems(*scenario).empty()) << scenario->id;
		EXPECT_EQ(scenario->facet, Facet::Animals) << scenario->id;
		EXPECT_TRUE(scenario->creatures.empty()) << scenario->id;
		// A hungry cow, the big pillar and the small one in its middle, its circle within the big one's
		ASSERT_EQ(scenario->objects.size(), 3u) << scenario->id;
		const auto& cow = scenario->objects[0];
		EXPECT_EQ(std::get<AnimalInfo>(cow.type), AnimalInfo::Cow);
		EXPECT_TRUE(cow.hungry);
		const auto& big = scenario->objects[1];
		const auto& small = scenario->objects[2];
		EXPECT_EQ(std::get<FeatureInfo>(big.type), FeatureInfo::FatPilarChalk);
		EXPECT_EQ(std::get<FeatureInfo>(small.type), FeatureInfo::FatPilarChalk);
		EXPECT_EQ(small.offset, big.offset);
		EXPECT_LT(small.scale, big.scale);
		// The pillars east of the cow and a little south of its way; the food behind it, to the west-south-west, within
		// the 35 m that draws a hungry grazer
		EXPECT_GT(big.offset.x, cow.offset.x);
		EXPECT_LT(big.offset.y, cow.offset.y);
		ASSERT_EQ(scenario->fixtures.piles.size(), 1u);
		const auto& pile = scenario->fixtures.piles.front();
		EXPECT_EQ(pile.type, PotInfo::FoodPile);
		const auto food = std::get<glm::vec2>(pile.at);
		EXPECT_LT(food.x, cow.offset.x);
		EXPECT_LT(food.y, cow.offset.y);
		EXPECT_LT(glm::distance(food, cow.offset), 35.0f);
		EXPECT_EQ(scenario->objects[1].offset, kept->objects[1].offset);
	}
	// Only the first deletes the small pillar, through a script, once the cow goes round the big one
	EXPECT_TRUE(kept->commands.empty());
	ASSERT_EQ(deleted->commands.size(), 1u);
	EXPECT_EQ(deleted->commands.front().kind, Kind::ScriptDelete);
	EXPECT_EQ(deleted->commands.front().object, 2u);
	EXPECT_GT(deleted->commands.front().delaySeconds, 0.0f);
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptDelete));
	EXPECT_EQ(Name(Kind::ScriptDelete), "script delete");
	// A script deleting no object, and a hungry thing that is no animal, are problems
	Scenario wrong = *deleted;
	wrong.commands.front().object = 3;
	EXPECT_EQ(Problems(wrong).size(), 1u);
	wrong = *kept;
	wrong.objects[1].hungry = true;
	EXPECT_EQ(Problems(wrong).size(), 1u);
}

TEST(TestbedScenarios, ASmallAndAFullSizeCreatureReachForASheepAndACow)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.pick_up_by_size");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 2u);
	EXPECT_LT(scenario->creatures[0].size.value_or(1.0f), scenario->creatures[1].size.value_or(1.0f));
	ASSERT_EQ(scenario->objects.size(), 4u);
	ASSERT_EQ(scenario->commands.size(), 8u);
	for (size_t i = 0; i < scenario->commands.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, i % 2 == 0 ? Kind::PickUp : Kind::PutDown);
		EXPECT_EQ(scenario->commands[i].creature, i < 4 ? 0u : 1u);
	}
}

TEST(TestbedScenarios, AVerySmallAndAFullSizeCreatureReachForAVillager)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.pick_up_villagers");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 2u);
	EXPECT_LT(scenario->creatures[0].size.value_or(1.0f), scenario->creatures[1].size.value_or(1.0f));
	ASSERT_EQ(scenario->objects.size(), 2u);
	for (const auto& villager : scenario->objects)
	{
		EXPECT_TRUE(std::holds_alternative<VillagerInfo>(villager.type));
	}
	ASSERT_EQ(scenario->commands.size(), 4u);
	for (size_t i = 0; i < scenario->commands.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, i % 2 == 0 ? Kind::PickUp : Kind::PutDown);
		EXPECT_EQ(scenario->commands[i].creature, i < 2 ? 0u : 1u);
	}
}

TEST(TestbedScenarios, AVerySmallAndAFullSizeCreatureReachForABallAndACart)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.pick_up_mobile_objects");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 2u);
	EXPECT_LT(scenario->creatures[0].size.value_or(1.0f), scenario->creatures[1].size.value_or(1.0f));
	ASSERT_EQ(scenario->objects.size(), 4u);
	for (const auto& thing : scenario->objects)
	{
		EXPECT_TRUE(std::holds_alternative<MobileObjectInfo>(thing.type));
	}
	ASSERT_EQ(scenario->commands.size(), 8u);
	for (size_t i = 0; i < scenario->commands.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, i % 2 == 0 ? Kind::PickUp : Kind::PutDown);
		EXPECT_EQ(scenario->commands[i].creature, i < 4 ? 0u : 1u);
	}
}

TEST(TestbedScenarios, StrokesACreatureForWhatItsMindJustDid)
{
	using Kind = Command::Kind;
	const auto* holding = Find("hand.stroked_holding_thing");
	const auto* fleeing = Find("hand.stroked_after_fleeing");
	ASSERT_NE(holding, nullptr);
	ASSERT_NE(fleeing, nullptr);
	EXPECT_TRUE(Problems(*holding).empty());
	EXPECT_TRUE(Problems(*fleeing).empty());
	for (const auto* scenario : {holding, fleeing})
	{
		// the mind decides and remembers: neither held nor paused, the planner choosing
		ASSERT_EQ(scenario->creatures.size(), 1u);
		EXPECT_FALSE(scenario->creatures.front().hold);
		EXPECT_FALSE(scenario->creatures.front().pauseMind);
		ASSERT_GE(scenario->commands.size(), 2u);
		// a stroke, then the hand lets go at once
		EXPECT_EQ(scenario->commands.back().kind, Kind::HandLetGo);
		EXPECT_EQ(scenario->commands[scenario->commands.size() - 2].kind, Kind::HandStroke);
	}
	// the ball is a thing, not food; the stroke comes while it is held, not when the tiger is free
	ASSERT_EQ(holding->objects.size(), 1u);
	EXPECT_EQ(std::get<MobileObjectInfo>(holding->objects.front().type), MobileObjectInfo::Ball);
	EXPECT_FALSE(holding->commands[0].waitUntilFree);
	// the slap first, then the stroke once the run is over
	EXPECT_EQ(fleeing->commands[0].kind, Kind::HandSlap);
	EXPECT_TRUE(fleeing->commands[fleeing->commands.size() - 2].waitUntilFree);
}

TEST(TestbedScenarios, WalksACreaturePastTheCreatureGate)
{
	const auto* scenario = Find("creature.walks_past_creature_gate");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 1u);
	ASSERT_EQ(scenario->objects.size(), 1u);
	const auto* gate = std::get_if<AnimatedStaticInfo>(&scenario->objects[0].type);
	ASSERT_NE(gate, nullptr);
	EXPECT_EQ(*gate, AnimatedStaticInfo::NorseGate);
	// it walks across, the gate is opened once it has arrived, and it walks back
	ASSERT_EQ(scenario->commands.size(), 3u);
	EXPECT_EQ(scenario->commands[0].kind, Command::Kind::WalkTo);
	EXPECT_EQ(scenario->commands[1].kind, Command::Kind::SetOpenClose);
	EXPECT_EQ(scenario->commands[1].value, 1u);
	EXPECT_TRUE(scenario->commands[1].waitUntilFree);
	EXPECT_EQ(scenario->commands[2].kind, Command::Kind::WalkTo);
	// the two ends of the walk are on either side of the gate
	EXPECT_LT(scenario->commands[0].point.y * scenario->commands[2].point.y, 0.0f);
}

TEST(TestbedScenarios, TiesTheLeashWithAPointerDoubleClick)
{
	EXPECT_TRUE(IsPointerCommand(Command::Kind::PointerDoubleClick));
	EXPECT_TRUE(NeedsNoCreature(Command::Kind::PointerDoubleClick));
	EXPECT_EQ(Name(Command::Kind::PointerDoubleClick), "pointer double click");
	const auto* scenario = Find("leash.double_click_tie");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Leash);
	EXPECT_TRUE(Problems(*scenario).empty());
	// a right click, then a right double click, each let go
	std::vector<Command::Kind> pointer;
	for (const auto& command : scenario->commands)
	{
		if (IsPointerCommand(command.kind) && command.kind != Command::Kind::PointerTo)
		{
			pointer.push_back(command.kind);
		}
	}
	const std::vector<Command::Kind> clicks {Command::Kind::PointerPress, Command::Kind::PointerRelease,
	                                         Command::Kind::PointerDoubleClick, Command::Kind::PointerRelease};
	EXPECT_EQ(pointer, clicks);
	// the camera looks steeply down at the pillar's foot, so the middle of the screen is on it
	ASSERT_EQ(scenario->objects.size(), 1u);
	ASSERT_EQ(scenario->framing.shot, Shot::Placed);
	EXPECT_FLOAT_EQ(scenario->framing.look.x, scenario->objects[0].offset.x);
	EXPECT_NEAR(scenario->framing.look.z, scenario->objects[0].offset.y, 2.0f);
	const auto down = scenario->framing.eye - scenario->framing.look;
	EXPECT_GT(down.y, std::abs(down.z));
}

TEST(TestbedScenarios, CommandsOnThingsAreChecked)
{
	using Kind = Command::Kind;
	Scenario broken {
	    .id = "broken.things",
	    .name = "Broken",
	    .description = "Broken on purpose",
	    .expected = "Every problem found",
	    .creatures = {CreatureSetup {}},
	    .objects = {{.type = FeatureInfo::FatPilarChalk}, {.type = MobileObjectInfo::Ball}, {.type = AnimalInfo::Dove}},
	    .commands = {{.kind = Kind::PickUp, .object = 0},
	                 {.kind = Kind::KnockDown, .object = 0},
	                 {.kind = Kind::TieLeash, .object = 3},
	                 {.kind = Kind::Examine, .value = 4},
	                 {.kind = Kind::HandStroke, .bodyPart = creature_feedback::k_BodyPartCount},
	                 {.kind = Kind::HandSlap, .slapHeight = 2.0f},
	                 {.kind = Kind::PutOnLeash, .leash = LeashType::None},
	                 {.kind = Kind::ConfineToHome, .radius = 0.0f}},
	};
	EXPECT_EQ(Problems(broken).size(), 8u);

	// Picking up a thing, an animal or a villager, knocking it down and tying to anything are fine
	broken.objects.push_back({.type = VillagerInfo::NorseShepherdMale});
	broken.commands = {{.kind = Kind::PickUp, .object = 1},           {.kind = Kind::PickUp, .object = 2},
	                   {.kind = Kind::PickUp, .object = 3},           {.kind = Kind::KnockDown, .object = 1},
	                   {.kind = Kind::TieLeash, .object = 0},         {.kind = Kind::PutOnLeash, .leash = LeashType::Good},
	                   {.kind = Kind::ConfineToHome, .radius = 10.0f}};
	EXPECT_TRUE(Problems(broken).empty());
}

TEST(TestbedScenarios, PointerCommandsAreChecked)
{
	using Kind = Command::Kind;
	// The mouse needs no creature, but its points are on the screen and its buttons are the mouse's three
	Scenario mouse {
	    .id = "broken.mouse",
	    .name = "Broken",
	    .description = "Broken on purpose",
	    .expected = "Every problem found",
	    .commands = {{.kind = Kind::PointerTo, .point = {1.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .value = 0},
	                 {.kind = Kind::PointerRelease, .value = 4},
	                 {.kind = Kind::PointerSweep, .point = {0.1f, 0.0f}, .amount = 0.0f}},
	};
	EXPECT_EQ(Problems(mouse).size(), 4u);

	mouse.commands = {{.kind = Kind::PointerTo, .point = {0.75f, 0.5f}},
	                  {.kind = Kind::PointerPress, .value = 2},
	                  {.kind = Kind::PointerSweep, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                  {.kind = Kind::PointerRelease, .value = 2},
	                  {.kind = Kind::WheelTurn, .value = 3}};
	EXPECT_TRUE(Problems(mouse).empty());
	EXPECT_TRUE(IsPointerCommand(Kind::WheelTurn));
	EXPECT_FALSE(IsPointerCommand(Kind::WalkTo));
}

TEST(TestbedScenarios, CoversTheHandFindingItsWay)
{
	for (const auto* id : {"hand.rotate_release", "hand.two_buttons", "hand.drag", "hand.zoom", "hand.click", "hand.edge_hover",
	                       "hand.edge_rotate", "hand.edge_pan", "hand.top_pitch", "hand.fast_pan"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_EQ(scenario->facet, Facet::Hand) << id;
		EXPECT_TRUE(scenario->creatures.empty()) << id;
		EXPECT_TRUE(std::ranges::all_of(scenario->commands, [](const Command& command) {
			return IsPointerCommand(command.kind);
		})) << id;
	}
}

TEST(TestbedScenarios, SweepsTheHandLightThroughTheEvening)
{
	// The clock stands still and only the commands move the hour, through the part of the evening where the whole mean
	// of the land's colour runs from 119 to 105, with the hand in the middle of the window, the camera looking down at
	// it and the land's colour that of a neutral player
	const auto* scenario = Find("hand.night_light");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Hand);
	EXPECT_FALSE(scenario->environment.clockRuns);
	EXPECT_FLOAT_EQ(scenario->environment.hour, 16.72f);
	ASSERT_TRUE(scenario->environment.playerAlignment.has_value());
	EXPECT_EQ(*scenario->environment.playerAlignment, 0.0f);
	ASSERT_TRUE(scenario->environment.cursor.has_value());
	EXPECT_EQ(*scenario->environment.cursor, glm::vec2(0.5f));
	EXPECT_EQ(scenario->framing.shot, Shot::Placed);
	EXPECT_TRUE(scenario->logHandLight);
	EXPECT_TRUE(scenario->creatures.empty());
	ASSERT_FALSE(scenario->commands.empty());
	EXPECT_TRUE(std::ranges::all_of(scenario->commands, [](const Command& command) {
		return command.kind == Command::Kind::SetHour && command.hour >= 16.72f && command.hour <= 17.36f;
	}));
	EXPECT_NEAR(std::ranges::max(scenario->commands, {}, &Command::hour).hour, 17.359f, 1e-4f);
	ASSERT_TRUE(scenario->repeatFrom.has_value());
	EXPECT_EQ(*scenario->repeatFrom, 0u);

	// At the checks' 16 ms step, the hour of the shot of a 900 frame run, 20 frames before its end
	Timeline timeline;
	float hour = scenario->environment.hour;
	for (int frame = 0; frame < 880; ++frame)
	{
		for (const auto index :
		     Advance(timeline, scenario->commands, scenario->repeatFrom, 0.016f, [](const Command&) { return true; }))
		{
			hour = scenario->commands[index].hour;
		}
	}
	EXPECT_NEAR(hour, 17.044f, 1e-4f);
}

TEST(TestbedScenarios, ReadsTheHandLightFromTheLandColour)
{
	// A land's colour of the evening sweep with a mean of 112 (sum 337), the alpha left out
	const auto evening = ReadHandLight(0xFF866467u);
	EXPECT_EQ(evening.landColour, 0x866467u);
	EXPECT_EQ(evening.sum, 337);
	EXPECT_EQ(evening.mean, 112);
	EXPECT_TRUE(evening.dark);
	EXPECT_GT(evening.strength, 0.0f);
	EXPECT_LT(evening.strength, 1.0f);
	EXPECT_NE(Describe(evening, 17.044f).find("the hour is 17.044: the land's colour 866467, sum 337, mean 112, dark"),
	          std::string::npos);

	// A mean of 120 is not yet dark, and the hand carries no light
	const auto dusk = ReadHandLight(0x9D6C61u);
	EXPECT_EQ(dusk.sum, 362);
	EXPECT_EQ(dusk.mean, 120);
	EXPECT_FALSE(dusk.dark);
	EXPECT_EQ(dusk.strength, 0.0f);

	// A mean of 105 or less is full strength
	const auto night = ReadHandLight(0x695A6Eu);
	EXPECT_EQ(night.mean, 101);
	EXPECT_TRUE(night.dark);
	EXPECT_EQ(night.strength, 1.0f);
}

TEST(TestbedScenarios, FliesTheAdvisorSpiritsOutAndHome)
{
	using Kind = Command::Kind;
	// Both spirits fly out and home again, over and over, with no creature and the clock standing still
	const auto* scenario = Find("advisors.trails");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Advisors);
	EXPECT_EQ(Name(Facet::Advisors), "Advisors");
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_FALSE(scenario->environment.clockRuns);
	ASSERT_EQ(scenario->commands.size(), 4u);
	for (size_t spirit = 0; spirit < 2; ++spirit)
	{
		EXPECT_EQ(std::ranges::count_if(
		              scenario->commands,
		              [spirit](const Command& command) { return command.kind == Kind::SpiritOut && command.value == spirit; }),
		          1)
		    << spirit;
		EXPECT_EQ(std::ranges::count_if(
		              scenario->commands,
		              [spirit](const Command& command) { return command.kind == Kind::SpiritHome && command.value == spirit; }),
		          1)
		    << spirit;
	}
	ASSERT_TRUE(scenario->repeatFrom.has_value());
	EXPECT_EQ(*scenario->repeatFrom, 0u);
	EXPECT_TRUE(NeedsNoCreature(Kind::SpiritOut));
	EXPECT_TRUE(NeedsNoCreature(Kind::SpiritHome));
	EXPECT_EQ(Name(Kind::SpiritOut), "spirit flies out");
	EXPECT_EQ(Name(Kind::SpiritHome), "spirit flies home");

	// A third spirit is found
	auto broken = *scenario;
	broken.commands = {{.kind = Kind::SpiritOut, .value = 2}, {.kind = Kind::SpiritHome, .value = 2}};
	EXPECT_EQ(Problems(broken).size(), 2u);
}

TEST(TestbedScenarios, TheGoodAdvisorSpiritPointsFromTheScreenThenFromTheWorld)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("advisors.point");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Advisors);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_FALSE(scenario->environment.clockRuns);
	// Out, pointing near the screen, pointing from the world at the same spot, home; over and over
	ASSERT_EQ(scenario->commands.size(), 4u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::SpiritOut);
	EXPECT_EQ(scenario->commands[3].kind, Kind::SpiritHome);
	// both points are the script's point at the land, the second one out in the world
	for (const size_t i : {1u, 2u})
	{
		EXPECT_EQ(scenario->commands[i].kind, Kind::Advisor) << i;
		EXPECT_EQ(scenario->commands[i].advisor, Command::AdvisorAction::PointAtLand) << i;
	}
	EXPECT_FALSE(scenario->commands[1].gentle);
	EXPECT_TRUE(scenario->commands[2].gentle);
	EXPECT_EQ(scenario->commands[1].point, scenario->commands[2].point);
	for (const auto& command : scenario->commands)
	{
		EXPECT_EQ(command.value, 0u); // the good spirit
	}
	ASSERT_TRUE(scenario->repeatFrom.has_value());
	EXPECT_EQ(*scenario->repeatFrom, 0u);
}

TEST(TestbedScenarios, TheGoodAdvisorSpiritComesOutAndSaysALine)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("advisors.talk");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Advisors);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_FALSE(scenario->environment.clockRuns);
	// Out, the line said once it is out, home after the line is over; over and over
	ASSERT_EQ(scenario->commands.size(), 3u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::SpiritOut);
	EXPECT_EQ(scenario->commands[1].kind, Kind::SpiritSay);
	EXPECT_EQ(scenario->commands[2].kind, Kind::SpiritHome);
	EXPECT_GT(scenario->commands[1].delaySeconds, 0.0f);
	EXPECT_EQ(scenario->commands[1].spiritLine, 1u);
	EXPECT_GT(scenario->commands[2].delaySeconds, 5.2f); // the line lasts 5.2 s
	for (const auto& command : scenario->commands)
	{
		EXPECT_EQ(command.value, 0u); // the good spirit
	}
	ASSERT_TRUE(scenario->repeatFrom.has_value());
	EXPECT_EQ(*scenario->repeatFrom, 0u);
	EXPECT_TRUE(NeedsNoCreature(Kind::SpiritSay));
	EXPECT_EQ(Name(Kind::SpiritSay), "spirit says");

	// A third spirit, and a line with no number, are found
	auto broken = *scenario;
	broken.commands = {{.kind = Kind::SpiritSay, .value = 2, .spiritLine = 1}, {.kind = Kind::SpiritSay, .value = 0}};
	EXPECT_EQ(Problems(broken).size(), 2u);
}

TEST(TestbedScenarios, TheGoodAdvisorSpiritSaysALineWithGestures)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("advisors.gesture");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Advisors);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_FALSE(scenario->environment.clockRuns);
	// Out, the line with the gestures once it is out, home after the line is over; over and over
	ASSERT_EQ(scenario->commands.size(), 3u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::SpiritOut);
	EXPECT_EQ(scenario->commands[1].kind, Kind::SpiritSay);
	EXPECT_EQ(scenario->commands[2].kind, Kind::SpiritHome);
	EXPECT_GT(scenario->commands[1].delaySeconds, 0.0f);
	EXPECT_EQ(scenario->commands[1].spiritLine, 26u);
	EXPECT_GT(scenario->commands[2].delaySeconds, 2.0f); // the line lasts 2 s
	for (const auto& command : scenario->commands)
	{
		EXPECT_EQ(command.value, 0u); // the good spirit
	}
	ASSERT_TRUE(scenario->repeatFrom.has_value());
	EXPECT_EQ(*scenario->repeatFrom, 0u);
}

TEST(TestbedScenarios, FacesTheSunOverTheLakeBank)
{
	namespace sun = openblack::graphics::sun;
	// The clock stands still with the sun up, the camera placed and the land's hiding of the glare logged
	const auto* scenario = Find("sky.sun_glare_behind_bank");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Sky);
	EXPECT_EQ(Name(Facet::Sky), "Sky");
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_TRUE(scenario->commands.empty());
	EXPECT_FALSE(scenario->environment.clockRuns);
	EXPECT_EQ(scenario->environment.weather, Weather::Clear);
	EXPECT_TRUE(scenario->logSunGlare);
	EXPECT_EQ(scenario->framing.shot, Shot::Placed);
	const auto placement = sun::Place(scenario->environment.hour);
	ASSERT_TRUE(placement.has_value());
	EXPECT_NEAR(placement->position.y, 4425.0f, 0.1f);

	// The eye and the point it looks at both stand over the lake's open water, whose bed is the land under them, so
	// their heights are above 0; the camera looks straight at the sun's middle
	const auto lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	const auto inTheLake = [&lake](glm::vec2 offset) {
		const auto from = glm::abs(offset - lake);
		return from.x < flat_land::k_LakeHalfExtent.x && from.y < flat_land::k_LakeHalfExtent.y;
	};
	const glm::vec2 eye(scenario->framing.eye.x, scenario->framing.eye.z);
	const glm::vec2 look(scenario->framing.look.x, scenario->framing.look.z);
	EXPECT_TRUE(inTheLake(eye));
	EXPECT_TRUE(inTheLake(look));
	const glm::vec3 camera(flat_land::k_MapMiddle.x + eye.x, scenario->framing.eye.y, flat_land::k_MapMiddle.y + eye.y);
	const auto towardsTheSun = glm::normalize(placement->position - camera);
	const glm::vec3 lookedAt(flat_land::k_MapMiddle.x + look.x, scenario->framing.look.y, flat_land::k_MapMiddle.y + look.y);
	EXPECT_GT(glm::dot(glm::normalize(lookedAt - camera), towardsTheSun), 0.99999f);

	// One pillar of rock in the water, on the line to the sun 40 units out, nearer than the depth that hides a sample
	ASSERT_EQ(scenario->objects.size(), 1u);
	ASSERT_TRUE(std::holds_alternative<FeatureInfo>(scenario->objects[0].type));
	EXPECT_EQ(std::get<FeatureInfo>(scenario->objects[0].type), FeatureInfo::FatPilarChalk);
	const auto pillar = scenario->objects[0].offset;
	EXPECT_TRUE(inTheLake(pillar));
	const auto alongTheGround = glm::normalize(glm::vec2(towardsTheSun.x, towardsTheSun.z));
	EXPECT_NEAR(glm::dot(pillar - eye, alongTheGround), 40.0f, 0.1f);
	EXPECT_NEAR(glm::length(pillar - eye), 40.0f, 0.1f);
	// The near plane of a camera this high over the water, and the depth it hides a sample within
	const float nearClip = near_clipping::NearPlane(scenario->framing.eye.y, false);
	EXPECT_LT(glm::length(pillar - eye), sun::GlareHidingDepth(nearClip) / 2.0f);
}

TEST(TestbedScenarios, ReadsWhichGlareSamplesTheLandHides)
{
	// At the scenario's hour, land up to 250 below the sun's middle in the way: the samples 500 below it
	const auto* scenario = Find("sky.sun_glare_behind_bank");
	ASSERT_NE(scenario, nullptr);
	const glm::vec3 camera(flat_land::k_MapMiddle.x + scenario->framing.eye.x, scenario->framing.eye.y,
	                       flat_land::k_MapMiddle.y + scenario->framing.eye.z);
	const auto reading = ReadSunGlare(scenario->environment.hour, camera, 1.0f,
	                                  [](const glm::vec3& point) { return point.y < 4425.0f - 250.0f; });
	ASSERT_TRUE(reading.has_value());
	EXPECT_NEAR(reading->sunHeight, 4425.0f, 0.1f);
	EXPECT_EQ(reading->samples, 5u);
	// Both lower corners
	ASSERT_EQ(reading->hiddenByLand.size(), 2u);
	EXPECT_EQ(reading->hiddenByLand[0], glm::vec2(-500.0f, -500.0f));
	EXPECT_EQ(reading->hiddenByLand[1], glm::vec2(500.0f, -500.0f));
	EXPECT_EQ(Describe(*reading),
	          "the sun stands 4425 high: the land hides 2 of the glare's 5 samples, (-500, -500), (500, -500)");

	// Nothing in the way, and the sun down
	const auto clear = ReadSunGlare(scenario->environment.hour, camera, 1.0f, [](const glm::vec3&) { return false; });
	ASSERT_TRUE(clear.has_value());
	EXPECT_TRUE(clear->hiddenByLand.empty());
	EXPECT_EQ(Describe(*clear), "the sun stands 4425 high: the land hides 0 of the glare's 5 samples");
	EXPECT_FALSE(ReadSunGlare(1.0f, camera, 1.0f, [](const glm::vec3&) { return true; }).has_value());
}

TEST(TestbedScenarios, PicksUpAndThrowsAMiraclesBubble)
{
	const auto* scenario = Find("hand.orb_pick_throw");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Hand);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	// A dispenser, with the village that gives it its town, and the pointer alone driving the hand: no hand demo
	ASSERT_EQ(scenario->fixtures.dispensers.size(), 1u);
	EXPECT_FALSE(scenario->fixtures.villages.empty());
	EXPECT_FALSE(scenario->fixtures.handDemo.has_value());
	EXPECT_TRUE(std::ranges::all_of(scenario->commands, [](const Command& command) { return IsPointerCommand(command.kind); }));
	// The right button goes down twice, to pick the bubble up and to throw it
	EXPECT_EQ(std::ranges::count_if(
	              scenario->commands,
	              [](const Command& command) { return command.kind == Command::Kind::PointerPress && command.value == 3; }),
	          2);
}

TEST(TestbedScenarios, SetsOutARowOfTreesByDayAndAtNight)
{
	const auto* day = Find("sky.trees_by_day");
	const auto* night = Find("sky.trees_at_night");
	ASSERT_NE(day, nullptr);
	ASSERT_NE(night, nullptr);
	for (const auto* scenario : {day, night})
	{
		EXPECT_EQ(scenario->facet, Facet::Sky) << scenario->id;
		EXPECT_TRUE(Problems(*scenario).empty()) << scenario->id;
		EXPECT_TRUE(scenario->creatures.empty()) << scenario->id;
		EXPECT_TRUE(scenario->commands.empty()) << scenario->id;
		EXPECT_FALSE(scenario->environment.clockRuns) << scenario->id;
		EXPECT_EQ(scenario->environment.weather, Weather::Clear) << scenario->id;
		EXPECT_FALSE(KeepsDispenserGrid(*scenario)) << scenario->id;
		ASSERT_EQ(scenario->framing.shot, Shot::Placed) << scenario->id;
		// Five trees of different kinds and a pillar of rock, all ahead of the camera and in its view
		ASSERT_EQ(scenario->objects.size(), 6u) << scenario->id;
		std::set<int> kinds;
		const auto& eye = scenario->framing.eye;
		const auto ahead = glm::normalize(scenario->framing.look - eye);
		for (size_t i = 0; i < scenario->objects.size(); ++i)
		{
			const auto& object = scenario->objects[i];
			if (i < 5)
			{
				ASSERT_TRUE(std::holds_alternative<TreeInfo>(object.type)) << scenario->id;
				kinds.insert(static_cast<int>(std::get<TreeInfo>(object.type)));
			}
			else
			{
				ASSERT_TRUE(std::holds_alternative<FeatureInfo>(object.type)) << scenario->id;
			}
			const auto toObject = glm::normalize(object.offset - glm::vec2(eye.x, eye.z));
			EXPECT_GT(glm::dot(toObject, glm::normalize(glm::vec2(ahead.x, ahead.z))), 0.9f) << scenario->id;
		}
		EXPECT_EQ(kinds.size(), 5u) << scenario->id;
	}
	// The two differ only in the hour and the hand that carries the light at night
	EXPECT_EQ(day->framing.eye, night->framing.eye);
	EXPECT_EQ(day->framing.look, night->framing.look);
	for (size_t i = 0; i < day->objects.size(); ++i)
	{
		EXPECT_EQ(day->objects[i].type, night->objects[i].type);
		EXPECT_EQ(day->objects[i].offset, night->objects[i].offset);
	}
	// By day the sky type is 0 and the light stays the sun; at midnight it is over 1.5, where the light goes to the hand.
	// The thresholds are the default day's (as the testbed runs it), given here so that no other test's cycle is read
	const sky_type::Thresholds defaultDay {0.786f, 1.206f, 1.626f, 2.046f};
	EXPECT_EQ(sky_type::At(day->environment.hour, defaultDay), 0.0f);
	EXPECT_GT(sky_type::At(night->environment.hour, defaultDay), 1.5f);
	EXPECT_FALSE(day->hand.has_value());
	ASSERT_TRUE(night->hand.has_value());
	// The hand is held by the east end of the row
	EXPECT_LT(glm::distance(night->hand->offset, night->objects[4].offset), 10.0f);
}

TEST(TestbedScenarios, LooksAtTheMoonAtNight)
{
	const auto* midnight = Find("sky.moon_midnight");
	const auto* night = Find("sky.moon_night");
	const auto* overSea = Find("sky.moon_over_sea");
	ASSERT_NE(midnight, nullptr);
	ASSERT_NE(night, nullptr);
	ASSERT_NE(overSea, nullptr);
	// The moon at midnight, where each of them looks at it, stands within 30 degrees of the middle of the view
	const auto moonAtMidnight = graphics::moon::Place(0.0f);
	ASSERT_TRUE(moonAtMidnight.has_value());
	const auto towardsTheMoon = glm::normalize(glm::vec2(moonAtMidnight->offset.x, moonAtMidnight->offset.z));
	for (const auto* scenario : {midnight, night, overSea})
	{
		EXPECT_EQ(scenario->facet, Facet::Sky) << scenario->id;
		EXPECT_TRUE(Problems(*scenario).empty()) << scenario->id;
		EXPECT_TRUE(scenario->creatures.empty()) << scenario->id;
		EXPECT_TRUE(scenario->commands.empty()) << scenario->id;
		EXPECT_EQ(scenario->environment.weather, Weather::Clear) << scenario->id;
		EXPECT_FALSE(KeepsDispenserGrid(*scenario)) << scenario->id;
		ASSERT_EQ(scenario->framing.shot, Shot::Placed) << scenario->id;
		EXPECT_FALSE(scenario->objects.empty()) << scenario->id;
		const auto ahead = scenario->framing.look - scenario->framing.eye;
		// (the cosine of 30 degrees)
		EXPECT_GT(glm::dot(glm::normalize(glm::vec2(ahead.x, ahead.z)), towardsTheMoon), 0.866f) << scenario->id;
	}
	// At midnight the clock stands still; through the night it runs from before the moon rises
	EXPECT_FALSE(midnight->environment.clockRuns);
	EXPECT_EQ(midnight->environment.hour, 0.0f);
	EXPECT_FALSE(overSea->environment.clockRuns);
	EXPECT_EQ(overSea->environment.hour, 0.0f);
	EXPECT_TRUE(night->environment.clockRuns);
	EXPECT_FALSE(graphics::moon::Place(night->environment.hour).has_value());
	// Over the water the camera stands over the lake's open water and looks level across it
	const auto lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	const auto fromLake = glm::abs(glm::vec2(overSea->framing.eye.x, overSea->framing.eye.z) - lake);
	EXPECT_LT(fromLake.x, flat_land::k_LakeHalfExtent.x);
	EXPECT_LT(fromLake.y, flat_land::k_LakeHalfExtent.y);
	EXPECT_EQ(overSea->framing.eye.y, overSea->framing.look.y);
}

TEST(TestbedScenarios, SetsOutTheChallengeScrollsInView)
{
	using ecs::script_highlight::Info;
	const auto* scenario = Find("particles.highlight_glints");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Particles);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	// A silver and a gold scroll, the two rows with glints, and nothing else on the land
	const auto& highlights = scenario->fixtures.highlights;
	ASSERT_EQ(highlights.size(), 2u);
	EXPECT_EQ(highlights[0].info, Info::Silver);
	EXPECT_EQ(highlights[1].info, Info::Gold);
	auto rest = scenario->fixtures;
	rest.highlights.clear();
	EXPECT_TRUE(testbed_fixtures::Empty(rest));
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_TRUE(scenario->objects.empty());
	EXPECT_TRUE(scenario->commands.empty());
	EXPECT_FALSE(KeepsDispenserGrid(*scenario));
	// The placed camera looks towards each scroll from between 10 and 30 units, where a scroll's scale follows the
	// distance. The testbed is flat, so the heights above the land are heights in the world
	ASSERT_EQ(scenario->framing.shot, Shot::Placed);
	const auto& eye = scenario->framing.eye;
	const auto ahead = glm::normalize(scenario->framing.look - eye);
	for (const auto& highlight : highlights)
	{
		const auto& at = std::get<glm::vec2>(highlight.at);
		const glm::vec3 toScroll = glm::vec3(at.x, 0.0f, at.y) - eye;
		const float distance = glm::length(toScroll);
		EXPECT_GE(distance, 10.0f);
		EXPECT_LE(distance, 30.0f);
		EXPECT_GT(glm::dot(toScroll / distance, ahead), 0.95f);
	}
}

TEST(TestbedScenarios, SetsOutALitGoldScrollInView)
{
	using ecs::script_highlight::Info;
	const auto* scenario = Find("particles.highlight_lit");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Particles);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	// One gold scroll, lit, and nothing else on the land
	const auto& highlights = scenario->fixtures.highlights;
	ASSERT_EQ(highlights.size(), 1u);
	EXPECT_EQ(highlights[0].info, Info::Gold);
	EXPECT_TRUE(highlights[0].lit);
	auto rest = scenario->fixtures;
	rest.highlights.clear();
	EXPECT_TRUE(testbed_fixtures::Empty(rest));
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_TRUE(scenario->objects.empty());
	EXPECT_TRUE(scenario->commands.empty());
	EXPECT_FALSE(KeepsDispenserGrid(*scenario));
	// The camera of particles.highlight_glints, looking at the scroll from between 10 and 30 units
	const auto* glints = Find("particles.highlight_glints");
	ASSERT_NE(glints, nullptr);
	ASSERT_EQ(scenario->framing.shot, Shot::Placed);
	EXPECT_EQ(scenario->framing.eye, glints->framing.eye);
	EXPECT_EQ(scenario->framing.look, glints->framing.look);
	const auto& eye = scenario->framing.eye;
	const auto ahead = glm::normalize(scenario->framing.look - eye);
	const auto& at = std::get<glm::vec2>(highlights[0].at);
	const glm::vec3 toScroll = glm::vec3(at.x, 0.0f, at.y) - eye;
	const float distance = glm::length(toScroll);
	EXPECT_GE(distance, 10.0f);
	EXPECT_LE(distance, 30.0f);
	EXPECT_GT(glm::dot(toScroll / distance, ahead), 0.95f);
	// The glints scenario's scrolls stay unlit
	for (const auto& highlight : glints->fixtures.highlights)
	{
		EXPECT_FALSE(highlight.lit);
	}
}

TEST(TestbedScenarios, SetsOutAGoldScrollInViewAndOneBehindTheCamera)
{
	using ecs::script_highlight::Info;
	const auto* scenario = Find("particles.highlight_offscreen");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Particles);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	// Two gold scrolls, unlit, and nothing else on the land
	const auto& highlights = scenario->fixtures.highlights;
	ASSERT_EQ(highlights.size(), 2u);
	auto rest = scenario->fixtures;
	rest.highlights.clear();
	EXPECT_TRUE(testbed_fixtures::Empty(rest));
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_TRUE(scenario->objects.empty());
	EXPECT_TRUE(scenario->commands.empty());
	EXPECT_FALSE(KeepsDispenserGrid(*scenario));
	// The camera of particles.highlight_glints: the first scroll ahead of it, the second behind it
	const auto* glints = Find("particles.highlight_glints");
	ASSERT_NE(glints, nullptr);
	ASSERT_EQ(scenario->framing.shot, Shot::Placed);
	EXPECT_EQ(scenario->framing.eye, glints->framing.eye);
	EXPECT_EQ(scenario->framing.look, glints->framing.look);
	const auto& eye = scenario->framing.eye;
	const auto ahead = glm::normalize(scenario->framing.look - eye);
	const auto towards = [&](size_t i) {
		const auto& at = std::get<glm::vec2>(highlights[i].at);
		return glm::normalize(glm::vec3(at.x, 0.0f, at.y) - eye);
	};
	for (const auto& highlight : highlights)
	{
		EXPECT_EQ(highlight.info, Info::Gold);
		EXPECT_FALSE(highlight.lit);
	}
	EXPECT_GT(glm::dot(towards(0), ahead), 0.95f);
	EXPECT_LT(glm::dot(towards(1), ahead), -0.5f);
}

TEST(TestbedScenarios, SetsOutAFreezePhialInView)
{
	const auto* scenario = Find("particles.freeze_phial_glints");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Particles);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	// One dispenser of the creature spell Freeze, making one bubble only, and the village that gives it its town
	const auto& dispensers = scenario->fixtures.dispensers;
	ASSERT_EQ(dispensers.size(), 1u);
	EXPECT_EQ(dispensers[0].magic, MagicType::CreatureSpellFreeze);
	EXPECT_EQ(dispensers[0].periodTurns, 0u);
	ASSERT_EQ(scenario->fixtures.villages.size(), 1u);
	auto rest = scenario->fixtures;
	rest.dispensers.clear();
	rest.villages.clear();
	EXPECT_TRUE(testbed_fixtures::Empty(rest));
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_TRUE(scenario->objects.empty());
	EXPECT_TRUE(scenario->commands.empty());
	EXPECT_FALSE(KeepsDispenserGrid(*scenario));
	// The placed camera looks at the bubble's middle above the dispenser from close by, and the village is out of view
	ASSERT_EQ(scenario->framing.shot, Shot::Placed);
	const auto& eye = scenario->framing.eye;
	const auto& look = scenario->framing.look;
	const auto& at = std::get<glm::vec2>(dispensers[0].at);
	EXPECT_FLOAT_EQ(look.x, at.x);
	EXPECT_FLOAT_EQ(look.z, at.y);
	const float distance = glm::length(look - eye);
	EXPECT_GE(distance, 5.0f);
	EXPECT_LE(distance, 15.0f);
	const auto ahead = (look - eye) / distance;
	const auto& village = std::get<glm::vec2>(scenario->fixtures.villages[0].at);
	const glm::vec3 toVillage = glm::vec3(village.x, 0.0f, village.y) - eye;
	EXPECT_LT(glm::dot(glm::normalize(toVillage), ahead), 0.5f);
}

TEST(TestbedScenarios, ProblemsAreFound)
{
	Scenario broken {
	    .id = "broken",
	    .name = "Broken",
	    .description = "Broken on purpose",
	    .expected = "Every problem found",
	    .framing = {.shot = Shot::Follow, .creature = 3},
	    .creatures = {CreatureSetup {.species = CreatureType::Unknown, .desires = {{.fraction = 2.0f}}}},
	    .objects = {{.type = TreeInfo::_COUNT}},
	    .commands = {{.kind = Command::Kind::PlayAction, .creature = 1, .value = creature_layers::animations::k_FirstFace},
	                 {.kind = Command::Kind::Follow, .creature = 0, .value = 0}},
	    .repeatFrom = 5,
	};
	// The framing, species, desire, object, command creature, animation, follow and repeat
	EXPECT_GE(Problems(broken).size(), 8u);
}

TEST(TestbedScenarios, BoundsTakeInEveryPoint)
{
	const std::array<glm::vec2, 3> points {glm::vec2 {-10.0f, 0.0f}, glm::vec2 {30.0f, 20.0f}, glm::vec2 {10.0f, -40.0f}};
	const auto bounds = BoundsOf(points, glm::vec2(5.0f));
	EXPECT_NEAR(bounds.centre.x, 10.0f, k_Epsilon);
	EXPECT_NEAR(bounds.centre.y, -10.0f, k_Epsilon);
	EXPECT_NEAR(bounds.halfSize.x, 20.0f, k_Epsilon);
	EXPECT_NEAR(bounds.halfSize.y, 30.0f, k_Epsilon);
	// A single point, or none, still gets the least size
	const std::array<glm::vec2, 1> one {glm::vec2 {3.0f, 4.0f}};
	EXPECT_EQ(BoundsOf(one, glm::vec2(30.0f)).halfSize, glm::vec2(30.0f));
	EXPECT_EQ(BoundsOf(one, glm::vec2(30.0f)).centre, one.front());
	EXPECT_EQ(BoundsOf({}, glm::vec2(12.0f)).halfSize, glm::vec2(12.0f));
}

TEST(TestbedScenarios, OverviewSeesTheWholeBox)
{
	const glm::vec3 centre {100.0f, 5.0f, 200.0f};
	const glm::vec2 halfSize {120.0f, 30.0f};
	const glm::vec2 fieldOfView {1.2f, 0.7f};
	const auto placement = Overview(centre, halfSize, fieldOfView, 1.0f);
	EXPECT_EQ(placement.focus, centre);
	// From the south, above
	EXPECT_LT(placement.origin.z, centre.z);
	EXPECT_GT(placement.origin.y, centre.y);
	// Far enough back that the box's ends are within half the field of view across either side
	const auto distance = glm::distance(placement.origin, centre);
	EXPECT_LE(std::atan(halfSize.x / distance), (fieldOfView.x * 0.5f) + k_Epsilon);
	// A deep box is framed by the view up and down instead
	const auto deep = Overview(centre, {10.0f, 300.0f}, fieldOfView, 1.0f);
	EXPECT_GT(glm::distance(deep.origin, centre), distance);
	// Further for more
	EXPECT_GT(glm::distance(Overview(centre, halfSize, fieldOfView, 2.0f).origin, centre), distance * 1.9f);
}

TEST(TestbedScenarios, FollowAndHeadShotsScaleWithTheCreature)
{
	const glm::vec3 at {10.0f, 0.0f, 10.0f};
	const auto small = Follow(at, CreatureHeight(0.5f), 1.0f);
	const auto big = Follow(at, CreatureHeight(2.0f), 1.0f);
	EXPECT_GT(glm::distance(big.origin, big.focus), glm::distance(small.origin, small.focus));
	EXPECT_LT(small.origin.z, at.z);

	// In front of the face, looking back at it, at head height
	const glm::vec2 ahead {1.0f, 0.0f};
	const auto height = CreatureHeight(1.0f);
	const auto head = Head(at, ahead, height, 1.0f);
	EXPECT_GT(head.origin.x, head.focus.x);
	EXPECT_GT(head.focus.y, at.y + (height * 0.5f));
	EXPECT_NEAR(head.origin.z, at.z, k_Epsilon);
	// An unnormalised or missing facing still works
	EXPECT_NEAR(Head(at, {4.0f, 0.0f}, height, 1.0f).origin.x, head.origin.x, k_Epsilon);
	EXPECT_LT(Head(at, {0.0f, 0.0f}, height, 1.0f).origin.z, at.z);
}

TEST(TestbedScenarios, NeedOverridesApplyOnlyWhatIsGiven)
{
	creature_physiology::Needs needs {};
	needs.energy = 0.5f;
	needs.life = 0.7f;
	Apply(NeedOverrides {.dehydration = 0.9f, .warmth = -3.0f, .age = 12}, needs, 1.0f);
	EXPECT_FLOAT_EQ(needs.dehydration, 0.9f);
	EXPECT_FLOAT_EQ(needs.warmth, -1.0f);
	EXPECT_EQ(needs.age, 12u);
	EXPECT_FLOAT_EQ(needs.energy, 0.5f);
	EXPECT_FLOAT_EQ(needs.life, 0.7f);

	// Energy fills up to the creature's size, as a meal does
	Apply(NeedOverrides {.energy = 1.8f}, needs, 1.0f);
	EXPECT_FLOAT_EQ(needs.energy, 1.0f);
	Apply(NeedOverrides {.energy = 1.8f}, needs, 2.0f);
	EXPECT_FLOAT_EQ(needs.energy, 1.8f);
	EXPECT_TRUE(NeedOverrides {}.Empty());
	EXPECT_FALSE(NeedOverrides {.poo = 0.0f}.Empty());
}

TEST(TestbedScenarios, DesireOverridesAreFractionsOfTheirMaxima)
{
	creature_desires::Desires desires {};
	auto& water = desires[creature_desires::Desire::Water];
	water.max = 4.0f;
	water.activated = false;
	desires[creature_desires::Desire::Hunger].max = 2.0f;
	const std::array<DesireOverride, 2> overrides {{
	    {.desire = creature_desires::Desire::Water, .fraction = 0.5f},
	    {.desire = creature_desires::Desire::Hunger, .fraction = 1.5f},
	}};
	Apply(overrides, desires);
	EXPECT_TRUE(water.activated);
	EXPECT_FLOAT_EQ(water.value, 2.0f);
	EXPECT_FLOAT_EQ(desires[creature_desires::Desire::Hunger].value, 2.0f);
	EXPECT_FLOAT_EQ(desires[creature_desires::Desire::Fear].value, 0.0f);
}

TEST(TestbedScenarios, TimelineGivesCommandsAsTheirDelaysPass)
{
	const std::vector<Command> commands {Wait(0, 1.0f, false), Wait(0, 0.0f, false), Wait(0, 2.0f, false)};
	Timeline timeline;
	EXPECT_TRUE(Advance(timeline, commands, std::nullopt, 0.5f, k_AlwaysFree).empty());
	// The second follows the first at once
	EXPECT_EQ(Advance(timeline, commands, std::nullopt, 0.5f, k_AlwaysFree), (std::vector<size_t> {0, 1}));
	EXPECT_TRUE(Advance(timeline, commands, std::nullopt, 1.9f, k_AlwaysFree).empty());
	EXPECT_EQ(Advance(timeline, commands, std::nullopt, 0.1f, k_AlwaysFree), (std::vector<size_t> {2}));
	EXPECT_TRUE(timeline.done);
	EXPECT_TRUE(Advance(timeline, commands, std::nullopt, 10.0f, k_AlwaysFree).empty());
}

TEST(TestbedScenarios, TimelineWaitsForTheCreatureToBeFree)
{
	const std::vector<Command> commands {Wait(0, 0.0f, false), Wait(1, 1.0f, true)};
	Timeline timeline;
	bool free = false;
	const auto isFree = [&free](const Command& command) {
		EXPECT_EQ(command.creature, 1u);
		return free;
	};
	EXPECT_EQ(Advance(timeline, commands, std::nullopt, 0.1f, isFree), (std::vector<size_t> {0}));
	// Busy for a long time: the delay only counts once it is free
	EXPECT_TRUE(Advance(timeline, commands, std::nullopt, 5.0f, isFree).empty());
	free = true;
	EXPECT_TRUE(Advance(timeline, commands, std::nullopt, 0.5f, isFree).empty());
	// Once it has been free, being busy again (an idle fidget) doesn't hold the command up
	free = false;
	EXPECT_TRUE(Advance(timeline, commands, std::nullopt, 0.4f, isFree).empty());
	EXPECT_EQ(Advance(timeline, commands, std::nullopt, 0.2f, isFree), (std::vector<size_t> {1}));
}

TEST(TestbedScenarios, TimelineGivesTheLastCommandTimeToStart)
{
	// A creature just told to walk hasn't started yet; the next command doesn't take it as free at once
	const std::vector<Command> commands {Wait(0, 0.0f, false), Wait(0, 0.0f, true)};
	Timeline timeline;
	EXPECT_EQ(Advance(timeline, commands, std::nullopt, 0.0f, k_AlwaysFree), (std::vector<size_t> {0}));
	EXPECT_TRUE(Advance(timeline, commands, std::nullopt, 0.1f, k_AlwaysFree).empty());
	EXPECT_EQ(Advance(timeline, commands, std::nullopt, 1.0f, k_AlwaysFree), (std::vector<size_t> {1}));
}

TEST(TestbedScenarios, TimelineGoesRoundAgain)
{
	const std::vector<Command> commands {Wait(0, 1.0f, false), Wait(0, 1.0f, false), Wait(0, 1.0f, false)};
	Timeline timeline;
	std::vector<size_t> given;
	for (int i = 0; i < 7; ++i)
	{
		const auto due = Advance(timeline, commands, 1, 1.0f, k_AlwaysFree);
		given.insert(given.end(), due.begin(), due.end());
	}
	EXPECT_EQ(given, (std::vector<size_t> {0, 1, 2, 1, 2, 1, 2}));
	EXPECT_FALSE(timeline.done);

	// Commands without delays that go round again don't run away within one frame
	const std::vector<Command> instant {Wait(0, 0.0f, false), Wait(0, 0.0f, false)};
	Timeline spinning;
	EXPECT_EQ(Advance(spinning, instant, 0, 0.1f, k_AlwaysFree).size(), instant.size());
}

TEST(TestbedScenarios, MapPointsAreFromTheMiddle)
{
	EXPECT_EQ(MapPoint({2560.0f, 2560.0f}, {10.0f, -20.0f}), glm::vec2(2570.0f, 2540.0f));
	EXPECT_FLOAT_EQ(CreatureHeight(1.0f), 15.0f);
	EXPECT_FLOAT_EQ(CreatureHeight(2.0f), 30.0f);
}

TEST(TestbedDispenserGrid, HasOneDispenserOfEveryMiracleTheCreatureSpellsAmongThem)
{
	const auto types = testbed_dispensers::GridMagicTypes();
	const std::set<MagicType> unique(types.begin(), types.end());
	EXPECT_EQ(unique.size(), types.size());
	for (const auto spell : {MagicType::CreatureSpellFreeze, MagicType::CreatureSpellSmall, MagicType::CreatureSpellBig,
	                         MagicType::CreatureSpellWeak, MagicType::CreatureSpellStrong, MagicType::CreatureSpellInvisible,
	                         MagicType::CreatureSpellCompassion, MagicType::CreatureSpellAngry, MagicType::CreatureSpellItchy})
	{
		EXPECT_TRUE(unique.contains(spell));
	}
	for (const auto miracle : {MagicType::Fireball, MagicType::LightningBolt, MagicType::Heal, MagicType::Food, MagicType::Wood,
	                           MagicType::Water, MagicType::Shield})
	{
		EXPECT_TRUE(unique.contains(miracle));
	}
}

TEST(TestbedDispenserGrid, LaysOutRowsSouthOfTheMiddleClearOfTheLake)
{
	// The testbed's lake lies north of the middle, its shore reaching out round its open water
	const float lakeSouthEdge =
	    static_cast<float>(flat_land::k_LakeMinZ - flat_land::k_ShoreCells) * flat_land::k_CellSize - flat_land::k_MapMiddle.y;
	// The testbed's camera starts south of the middle
	constexpr float k_CameraBack = 120.0f;
	using testbed_dispensers::k_GridColumns;
	using testbed_dispensers::k_GridOrigin;
	using testbed_dispensers::k_GridSpacing;
	const auto count = testbed_dispensers::GridMagicTypes().size();
	const auto offsets = testbed_dispensers::GridOffsets(count);
	ASSERT_EQ(offsets.size(), count);
	EXPECT_EQ(offsets.front(), k_GridOrigin);
	// Along a row eastwards, then the next row a step south
	EXPECT_EQ(offsets.at(1), k_GridOrigin + glm::vec2(k_GridSpacing.x, 0.0f));
	EXPECT_EQ(offsets.at(k_GridColumns), k_GridOrigin + glm::vec2(0.0f, k_GridSpacing.y));
	for (size_t i = 0; i < offsets.size(); ++i)
	{
		EXPECT_LT(offsets[i].y, 0.0f) << i;
		EXPECT_LT(offsets[i].y, lakeSouthEdge) << i;
		EXPECT_GT(offsets[i].y, -k_CameraBack + testbed_dispensers::k_GridClearance) << i;
		EXPECT_TRUE(testbed_dispensers::InGridArea(offsets[i])) << i;
		for (size_t j = i + 1; j < offsets.size(); ++j)
		{
			// Far enough apart for the hand to tap each bubble
			EXPECT_GE(glm::distance(offsets[i], offsets[j]),
			          std::min(std::abs(k_GridSpacing.x), std::abs(k_GridSpacing.y)) - k_Epsilon);
		}
	}
	// Centred east to west on the middle
	EXPECT_NEAR(k_GridOrigin.x + k_GridSpacing.x * static_cast<float>(k_GridColumns - 1) * 0.5f, 0.0f, k_Epsilon);
}

TEST(TestbedScenarios, CoversTheMiracles)
{
	std::set<MagicType> cast;
	bool dispensers = false;
	for (const auto& scenario : All())
	{
		if (scenario.facet != Facet::Miracles)
		{
			continue;
		}
		dispensers = dispensers || scenario.id == "miracles.dispensers";
		for (const auto& miracle : scenario.miracles)
		{
			cast.insert(miracle.type);
		}
	}
	EXPECT_TRUE(dispensers);
	for (const auto miracle :
	     {MagicType::Fireball, MagicType::LightningBolt, MagicType::Heal, MagicType::Food, MagicType::Wood, MagicType::Water,
	      MagicType::Shield, MagicType::CreatureSpellFreeze, MagicType::CreatureSpellSmall, MagicType::CreatureSpellBig,
	      MagicType::CreatureSpellWeak, MagicType::CreatureSpellStrong, MagicType::CreatureSpellInvisible,
	      MagicType::CreatureSpellCompassion, MagicType::CreatureSpellAngry, MagicType::CreatureSpellItchy})
	{
		EXPECT_TRUE(cast.contains(miracle)) << static_cast<int>(miracle);
	}
}

TEST(TestbedScenarios, TheHandCastsFreezeOnACreatureAsItCastsBig)
{
	const auto* big = Find("miracles.hand_big_on_creature");
	const auto* freeze = Find("miracles.hand_freeze_on_creature");
	ASSERT_NE(big, nullptr);
	ASSERT_NE(freeze, nullptr);
	EXPECT_EQ(freeze->fixtures.handSeed, MagicType::CreatureSpellFreeze);
	EXPECT_EQ(big->fixtures.handSeed, MagicType::CreatureSpellBig);
	ASSERT_EQ(freeze->creatures.size(), 1);
	EXPECT_TRUE(freeze->creatures.front().pauseMind);
	EXPECT_EQ(freeze->commands.size(), big->commands.size());
}

TEST(TestbedScenarios, TheHandGripsACreatureMidCastAsItCastsLightning)
{
	const auto* lightning = Find("miracles.creature_casts_lightning");
	const auto* grip = Find("miracles.hand_grips_casting_creature");
	ASSERT_NE(lightning, nullptr);
	ASSERT_NE(grip, nullptr);
	EXPECT_EQ(grip->facet, Facet::Miracles);
	EXPECT_TRUE(Problems(*grip).empty());
	// the lightning scenario's tiger and rock, told the same
	ASSERT_EQ(grip->creatures.size(), lightning->creatures.size());
	EXPECT_EQ(grip->creatures.front().species, lightning->creatures.front().species);
	EXPECT_EQ(grip->creatures.front().offset, lightning->creatures.front().offset);
	ASSERT_EQ(grip->objects.size(), lightning->objects.size());
	EXPECT_EQ(grip->objects.front().offset, lightning->objects.front().offset);
	ASSERT_EQ(grip->commands.size(), lightning->commands.size() + 3);
	for (size_t i = 0; i < lightning->commands.size(); ++i)
	{
		EXPECT_EQ(grip->commands[i].kind, lightning->commands[i].kind);
		EXPECT_EQ(grip->commands[i].value, lightning->commands[i].value);
		EXPECT_EQ(grip->commands[i].delaySeconds, lightning->commands[i].delaySeconds);
	}
	// then the pointer onto the tiger, and the Action button pressed there while the bolt crackles and let go after
	const auto first = lightning->commands.size();
	EXPECT_EQ(grip->commands[first].kind, Command::Kind::PointerTo);
	EXPECT_EQ(grip->commands[first + 1].kind, Command::Kind::PointerPress);
	EXPECT_EQ(grip->commands[first + 1].value, 3u);
	EXPECT_EQ(grip->commands[first + 2].kind, Command::Kind::PointerRelease);
	EXPECT_EQ(grip->commands[first + 2].value, 3u);
	float pressedAt = 0.0f;
	for (size_t i = 0; i <= first + 1; ++i)
	{
		pressedAt += grip->commands[i].delaySeconds;
	}
	EXPECT_FLOAT_EQ(pressedAt, 6.5f);
	EXPECT_GT(grip->commands[first + 2].delaySeconds, 0.0f);
}

TEST(TestbedScenarios, TheStraightTeleportRoutesAVillagerThroughBothStones)
{
	const auto* scenario = Find("miracles.teleport_straight_two_points");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Miracles);
	EXPECT_TRUE(Problems(*scenario).empty());
	// Two teleports cast straight at their points by the first player
	ASSERT_EQ(scenario->miracles.size(), 2u);
	for (const auto& miracle : scenario->miracles)
	{
		EXPECT_EQ(miracle.type, MagicType::Teleport);
		EXPECT_FALSE(miracle.byHand);
		EXPECT_EQ(miracle.target, MiracleCast::Target::Point);
		EXPECT_EQ(miracle.player, PlayerNames::PLAYER_ONE);
	}
	const auto& farStone = scenario->miracles[0];
	const auto& nearStone = scenario->miracles[1];
	ASSERT_EQ(scenario->objects.size(), 1u);
	const auto& walker = scenario->objects.front();
	ASSERT_TRUE(walker.walkTo.has_value());
	EXPECT_TRUE(walker.joinTown);
	// A walk to decide afresh takes up no reaction on its way
	EXPECT_NE(walker.walkFinal, VillagerStates::DecideWhatToDo);
	// A stone's reaction reaches only who is already walking as it is made: the far stone first, the near one once
	// the villager is on its way, near enough the start for the reaction's reach and the far one out of it
	EXPECT_LT(farStone.delaySeconds, walker.walkAfterSeconds);
	EXPECT_GT(nearStone.delaySeconds, walker.walkAfterSeconds);
	constexpr float k_TeleportReactionReach = 35.0f;
	const auto reach = [](glm::vec2 a, glm::vec2 b) {
		const auto d = glm::abs(a - b);
		return (d.x + d.y) * 0.5f;
	};
	EXPECT_LE(reach(walker.offset, nearStone.point), k_TeleportReactionReach);
	EXPECT_GT(reach(walker.offset, farStone.point), k_TeleportReactionReach);
	// The game's detour rule, 1.2 x (|living - near| + |far - goal|) < |living - goal| on its fast distance, holds from
	// the start and at the near stone
	const auto fast = [](glm::vec2 a, glm::vec2 b) {
		const auto d = glm::abs(a - b);
		return std::max(d.x, d.y) + std::min(d.x, d.y) * 0.5f;
	};
	for (const auto from : {walker.offset, nearStone.point})
	{
		EXPECT_LT(1.2f * (fast(from, nearStone.point) + fast(farStone.point, *walker.walkTo)), fast(from, *walker.walkTo));
	}
	// The trace that shows the stones used is named
	EXPECT_NE(scenario->description.find("OPENBLACK_TELEPORT_TRACE=1"), std::string_view::npos);
}

TEST(TestbedScenarios, TheStraightTornadoIsCastAtTheCreatureTheHandOneMeets)
{
	const auto* straight = Find("miracles.tornado_straight_at_creature");
	const auto* byHand = Find("miracles.tornado_creature");
	ASSERT_NE(straight, nullptr);
	ASSERT_NE(byHand, nullptr);
	EXPECT_EQ(straight->facet, Facet::Miracles);
	EXPECT_TRUE(Problems(*straight).empty());
	// One tornado cast straight at the scenario's creature, with no circle drawn
	ASSERT_EQ(straight->miracles.size(), 1u);
	const auto& cast = straight->miracles.front();
	EXPECT_EQ(cast.type, MagicType::Tornado);
	EXPECT_FALSE(cast.byHand);
	EXPECT_FALSE(cast.circleRadius.has_value());
	EXPECT_EQ(cast.target, MiracleCast::Target::Creature);
	EXPECT_EQ(cast.creature, 0u);
	// The tiger of the cast through the hand, where that one stands
	ASSERT_EQ(straight->creatures.size(), 1u);
	ASSERT_EQ(byHand->creatures.size(), 1u);
	EXPECT_EQ(straight->creatures.front().species, CreatureType::Tiger);
	EXPECT_EQ(straight->creatures.front().offset, byHand->creatures.front().offset);
}

TEST(TestbedDispenserGrid, StaysUnlessAScenarioAsksForNoneOrStandsOnIt)
{
	Scenario scenario;
	EXPECT_TRUE(KeepsDispenserGrid(scenario));
	EXPECT_FALSE(testbed_dispensers::InGridArea({0.0f, 0.0f}));
	scenario.creatures.push_back({.offset = {0.0f, 30.0f}});
	EXPECT_TRUE(KeepsDispenserGrid(scenario));
	scenario.objects.push_back({.offset = testbed_dispensers::k_GridOrigin});
	EXPECT_FALSE(KeepsDispenserGrid(scenario));
	scenario.objects.clear();
	scenario.environment.dispenserGrid = false;
	EXPECT_FALSE(KeepsDispenserGrid(scenario));
	// The scenario of the dispensers keeps them
	const auto* dispensers = Find("miracles.dispensers");
	ASSERT_NE(dispensers, nullptr);
	EXPECT_TRUE(KeepsDispenserGrid(*dispensers));
}

TEST(TestbedScenarios, BenchmarksMeasureCrowdsOfEachSize)
{
	const std::array<std::pair<const char*, size_t>, 8> benchmarks {{
	    {"benchmark.creatures_10", 10},
	    {"benchmark.creatures_25", 25},
	    {"benchmark.creatures_50", 50},
	    {"benchmark.creatures_100", 100},
	    {"benchmark.villagers_100", 100},
	    {"benchmark.villagers_250", 250},
	    {"benchmark.villagers_500", 500},
	    {"benchmark.villagers_1000", 1'000},
	}};
	for (const auto& [id, count] : benchmarks)
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_EQ(scenario->facet, Facet::Benchmark);
		ASSERT_TRUE(scenario->crowd.has_value()) << id;
		EXPECT_EQ(scenario->crowd->count, count);
		EXPECT_GT(scenario->crowd->perFrame, 0u);
		EXPECT_EQ(scenario->crowd->kind, std::string_view(id).find("creatures") != std::string_view::npos
		                                     ? Crowd::Kind::Creatures
		                                     : Crowd::Kind::Villagers);
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		// Nothing else on the land, so that only the crowd's size changes between them, and the same layout each run
		EXPECT_TRUE(scenario->creatures.empty());
		EXPECT_TRUE(scenario->objects.empty());
		EXPECT_TRUE(scenario->commands.empty());
		EXPECT_FALSE(scenario->environment.dispenserGrid);
		EXPECT_EQ(scenario->crowd->seed, Find("benchmark.creatures_100")->crowd->seed);
	}
	EXPECT_EQ(Name(Facet::Benchmark), "Benchmark");
	// No more than 100 creatures or 1,000 villagers
	size_t crowds = 0;
	for (const auto& scenario : All())
	{
		if (scenario.crowd.has_value())
		{
			++crowds;
			EXPECT_LE(scenario.crowd->count, scenario.crowd->kind == Crowd::Kind::Creatures ? 100u : 1'000u) << scenario.id;
		}
	}
	// The benchmarks' crowds, and the town the draw list's example makes
	EXPECT_EQ(crowds, benchmarks.size() + 1);
	// Only benchmarks have crowds, but for that example, named here: its town is made a member a frame so that the shot
	// shows things made after the list's last rebuild left out of the draw until its next one
	for (const auto& scenario : All())
	{
		const bool example = scenario.id == "examples.objects_join_the_draw_list";
		EXPECT_EQ(scenario.crowd.has_value(), scenario.facet == Facet::Benchmark || example) << scenario.id;
	}
}

TEST(TestbedScenarios, DrawListExampleMakesItsTownBeforeTheListsNextRebuild)
{
	const auto* scenario = Find("examples.objects_join_the_draw_list");
	ASSERT_NE(scenario, nullptr);
	ASSERT_TRUE(scenario->crowd.has_value());
	EXPECT_EQ(scenario->crowd->kind, Crowd::Kind::Villagers);
	EXPECT_EQ(scenario->crowd->perFrame, 1u);
	// Nothing that could ask the list for a rebuild meanwhile: no creature, command, miracle or hand demo
	EXPECT_TRUE(scenario->creatures.empty());
	EXPECT_TRUE(scenario->commands.empty());
	EXPECT_TRUE(scenario->miracles.empty());
	EXPECT_FALSE(scenario->fixtures.handDemo.has_value());

	const auto layout = LayOutVillagers(scenario->crowd->count, scenario->crowd->seed);
	ASSERT_EQ(layout.towns.size(), 1u);
	EXPECT_EQ(layout.towns.front().offset, glm::vec2(0.0f));
	// A member a frame: the town, its pit, its huts and its villagers are all made within 62 frames of 16 ms, before the
	// list's next rebuild by turns, which comes 1.0 s (62.5 frames) in at the earliest
	const auto members = layout.towns.size() + layout.abodes.size() + layout.villagers.size();
	EXPECT_EQ(members, 62u);
	// The whole town is inside what the overview frames
	ASSERT_EQ(scenario->framing.include.size(), 2u);
	const auto low = scenario->framing.include.front();
	const auto high = scenario->framing.include.back();
	const auto framed = [&low, &high](glm::vec2 point) {
		return point.x >= low.x && point.y >= low.y && point.x <= high.x && point.y <= high.y;
	};
	for (const auto& abode : layout.abodes)
	{
		EXPECT_TRUE(framed(abode.offset)) << abode.offset.x << ", " << abode.offset.y;
	}
	for (const auto& villager : layout.villagers)
	{
		EXPECT_TRUE(framed(villager.offset)) << villager.offset.x << ", " << villager.offset.y;
	}
}

TEST(TestbedScenarios, CrowdsAreChecked)
{
	Scenario scenario {.id = "benchmark.bad", .name = "Bad", .description = "-", .expected = "-"};
	scenario.environment.dispenserGrid = false;
	EXPECT_FALSE(Problems(scenario).empty());
	scenario.crowd = Crowd {.count = 10, .perFrame = 5};
	EXPECT_TRUE(Problems(scenario).empty());
	scenario.crowd->perFrame = 0;
	EXPECT_FALSE(Problems(scenario).empty());
	scenario.crowd = Crowd {.count = 0, .perFrame = 5};
	EXPECT_FALSE(Problems(scenario).empty());
}

TEST(TestbedScenarios, TheBurningCowStandsInsideItsOwnFiresReach)
{
	// Two cows, the first set alight by the only fire fixture: it starts the fire's reaction, 35 m, and the other cow
	// is well inside that reach
	const auto* burning = Find("miracles.burning_cow");
	ASSERT_NE(burning, nullptr);
	EXPECT_EQ(burning->facet, Facet::Miracles);
	EXPECT_TRUE(Problems(*burning).empty());
	ASSERT_EQ(burning->objects.size(), 2u);
	for (const auto& cow : burning->objects)
	{
		const auto* type = std::get_if<AnimalInfo>(&cow.type);
		ASSERT_NE(type, nullptr);
		EXPECT_EQ(*type, AnimalInfo::Cow);
	}
	EXPECT_LT(glm::distance(burning->objects[0].offset, burning->objects[1].offset), 35.0f);
	ASSERT_EQ(burning->fixtures.fires.size(), 1u);
	const auto* what = std::get_if<size_t>(&burning->fixtures.fires.front().what);
	ASSERT_NE(what, nullptr);
	EXPECT_EQ(*what, 0u);
	EXPECT_TRUE(burning->creatures.empty());
	EXPECT_TRUE(burning->miracles.empty());
}

TEST(TestbedScenarios, PhysicsScenariosThrowTheirOwnThings)
{
	// The things lying still are thrown by nothing; the others throw only their own objects, and keep throwing them
	const auto* idle = Find("physics.idle");
	ASSERT_NE(idle, nullptr);
	EXPECT_EQ(idle->facet, Facet::Physics);
	EXPECT_EQ(idle->objects.size(), 40u);
	EXPECT_TRUE(idle->throws.empty());
	for (const auto* id : {"physics.throw_one", "physics.throw_many", "physics.building_breaks", "physics.rock_split",
	                       "physics.thrown_villager_lands", "physics.burning_rock_split", "physics.villagers_watch_throw"})
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_EQ(scenario->facet, Facet::Physics) << id;
		EXPECT_FALSE(scenario->throws.empty()) << id;
		for (const auto& throwing : scenario->throws)
		{
			EXPECT_LT(throwing.object, scenario->objects.size()) << id;
			EXPECT_TRUE(throwing.repeatSeconds.has_value()) << id;
		}
	}
	EXPECT_EQ(Find("physics.throw_many")->throws.size(), 60u);
	// a hut of the town and three of its villagers: a boulder once, then another again and again from 30 s
	const auto* repaired = Find("physics.building_repaired");
	ASSERT_NE(repaired, nullptr);
	EXPECT_EQ(repaired->facet, Facet::Physics);
	ASSERT_EQ(repaired->throws.size(), 2u);
	EXPECT_FALSE(repaired->throws[0].repeatSeconds.has_value());
	EXPECT_TRUE(repaired->throws[1].repeatSeconds.has_value());
	EXPECT_EQ(std::get<AbodeInfo>(repaired->objects[0].type), AbodeInfo::CelticHut);
	EXPECT_TRUE(std::ranges::all_of(std::span(repaired->objects).subspan(1, 3), &ObjectSetup::joinTown));
	// The burning rocks are the hurled rocks, each set alight by a fire fixture
	const auto* burning = Find("physics.burning_rock_split");
	const auto* hurled = Find("physics.rock_split");
	EXPECT_EQ(burning->objects.size(), hurled->objects.size());
	EXPECT_EQ(burning->throws.size(), hurled->throws.size());
	ASSERT_EQ(burning->fixtures.fires.size(), burning->objects.size());
	for (size_t i = 0; i < burning->fixtures.fires.size(); ++i)
	{
		const auto* what = std::get_if<size_t>(&burning->fixtures.fires[i].what);
		ASSERT_NE(what, nullptr) << i;
		EXPECT_EQ(*what, i);
	}
	EXPECT_TRUE(Problems(*burning).empty());
	// Three huts and two boulders thrown at each, framed closer so that their knocks are within earshot
	const auto* huts = Find("physics.building_breaks");
	EXPECT_EQ(huts->objects.size(), 9u);
	EXPECT_EQ(huts->throws.size(), 6u);
	EXPECT_LT(huts->framing.distance, 1.0f);
	// One villager of the town, thrown again and again from the west onto the field
	const auto* thrown = Find("physics.thrown_villager_lands");
	ASSERT_EQ(thrown->objects.size(), 1u);
	EXPECT_TRUE(std::holds_alternative<VillagerInfo>(thrown->objects.front().type));
	EXPECT_TRUE(thrown->objects.front().joinTown);
	ASSERT_EQ(thrown->throws.size(), 1u);
	EXPECT_TRUE(thrown->throws.front().from.has_value());
	EXPECT_TRUE(Problems(*thrown).empty());
	// A rock lobbed over five villagers of the town, each whole cells from where it is let go: two nearer to it than it
	// flies in two seconds, two further but within the 25 m a thrown thing's reaction reaches (the cells whose middles
	// are that near, and half the sum of the two distances along the map), one beyond
	const auto* watched = Find("physics.villagers_watch_throw");
	ASSERT_NE(watched, nullptr);
	ASSERT_EQ(watched->objects.size(), 6u);
	EXPECT_TRUE(std::holds_alternative<MobileStaticInfo>(watched->objects.front().type));
	ASSERT_EQ(watched->throws.size(), 1u);
	const auto& lob = watched->throws.front();
	EXPECT_EQ(lob.object, 0u);
	ASSERT_TRUE(lob.from.has_value());
	const float twoSeconds = 2.0f * glm::length(lob.velocity);
	std::vector<int> kinds;
	for (size_t i = 1; i < watched->objects.size(); ++i)
	{
		const auto& villager = watched->objects.at(i);
		EXPECT_TRUE(std::holds_alternative<VillagerInfo>(villager.type)) << i;
		EXPECT_TRUE(villager.joinTown) << i;
		const glm::vec2 apart = villager.offset - *lob.from;
		EXPECT_EQ(std::fmod(apart.x, 10.0f), 0.0f) << i;
		EXPECT_EQ(std::fmod(apart.y, 10.0f), 0.0f) << i;
		const bool reached = glm::length(apart) <= 25.0f && (std::abs(apart.x) + std::abs(apart.y)) * 0.5f <= 25.0f;
		kinds.push_back(!reached ? 2 : glm::length(apart) < twoSeconds ? 0 : 1);
	}
	EXPECT_EQ(kinds, (std::vector<int> {0, 0, 1, 1, 2}));
	EXPECT_TRUE(Problems(*watched).empty());
	// One of each kind the water treats differently, each dropped once into the lake's open water
	const auto* sinking = Find("physics.sinking");
	ASSERT_NE(sinking, nullptr);
	EXPECT_EQ(sinking->facet, Facet::Physics);
	EXPECT_EQ(sinking->objects.size(), 12u);
	ASSERT_EQ(sinking->throws.size(), 12u);
	for (const auto& throwing : sinking->throws)
	{
		EXPECT_LT(throwing.object, sinking->objects.size());
		EXPECT_FALSE(throwing.repeatSeconds.has_value());
	}
	EXPECT_TRUE(Problems(*sinking).empty());
	// A villager and a sheep thrown up over the open water once each, for their reflections in flight
	// a dying villager's soul in front of a chest from the sky and a rock thrown up behind it
	const auto* tail = Find("physics.tail_order");
	ASSERT_NE(tail, nullptr);
	EXPECT_EQ(tail->facet, Facet::Physics);
	ASSERT_EQ(tail->objects.size(), 2u);
	ASSERT_EQ(tail->throws.size(), 1u);
	EXPECT_TRUE(Problems(*tail).empty());
	const auto* flying = Find("physics.flying_reflections");
	ASSERT_NE(flying, nullptr);
	EXPECT_EQ(flying->facet, Facet::Physics);
	ASSERT_EQ(flying->objects.size(), 2u);
	ASSERT_EQ(flying->throws.size(), 2u);
	for (const auto& throwing : flying->throws)
	{
		EXPECT_LT(throwing.object, flying->objects.size());
		EXPECT_GT(throwing.velocity.y, 0.0f);
		EXPECT_FALSE(throwing.repeatSeconds.has_value());
	}
	EXPECT_TRUE(Problems(*flying).empty());
	EXPECT_EQ(Name(Facet::Physics), "Physics");

	// A scenario of things alone is well formed; a throw of an object it hasn't, or from off the map, is found
	Scenario things {.id = "physics.test", .name = "Things", .description = "-", .expected = "-"};
	things.environment.dispenserGrid = false;
	EXPECT_FALSE(Problems(things).empty());
	things.objects = {{.type = MobileStaticInfo::RockChalk}};
	EXPECT_TRUE(Problems(things).empty());
	things.throws = {{.object = 0, .velocity = {0.0f, 9.0f, 16.0f}}};
	EXPECT_TRUE(Problems(things).empty());
	things.throws = {{.object = 1}, {.from = glm::vec2 {1.0e6f, 0.0f}}, {.height = -1.0f}};
	EXPECT_EQ(Problems(things).size(), 3u);
}

TEST(TestbedScenarios, AnImmovableRockAndAMovableOneLetGoInTheAir)
{
	const auto* scenario = Find("physics.immovable_let_go");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Physics);
	EXPECT_TRUE(Problems(*scenario).empty());
	// two rocks, the first of them immovable
	ASSERT_EQ(scenario->objects.size(), 2u);
	EXPECT_EQ(std::get<MobileStaticInfo>(scenario->objects[0].type), MobileStaticInfo::RockChalk);
	EXPECT_EQ(std::get<MobileStaticInfo>(scenario->objects[1].type), MobileStaticInfo::RockChalk);
	EXPECT_TRUE(scenario->objects[0].immovable);
	EXPECT_FALSE(scenario->objects[1].immovable);
	// each let go once, high up, with no speed
	ASSERT_EQ(scenario->throws.size(), 2u);
	for (size_t i = 0; i < scenario->throws.size(); ++i)
	{
		const auto& throwing = scenario->throws[i];
		EXPECT_EQ(throwing.object, i);
		EXPECT_FLOAT_EQ(throwing.height, 10.0f);
		EXPECT_EQ(throwing.velocity, glm::vec3(0.0f));
		EXPECT_FALSE(throwing.repeatSeconds.has_value());
	}
}

TEST(TestbedScenarios, ATreePutDownGentlyByAGoodAndAnEvilPlayer)
{
	constexpr std::array<std::pair<std::string_view, float>, 2> k_Players {
	    {{"physics.tree_replanted_good", 1.0f}, {"physics.tree_replanted_evil", -1.0f}}};
	for (const auto& [id, alignment] : k_Players)
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_EQ(scenario->facet, Facet::Physics) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		ASSERT_TRUE(scenario->environment.playerAlignment.has_value()) << id;
		EXPECT_EQ(*scenario->environment.playerAlignment, alignment) << id;
		// one oak and nothing of a town, so the tree is planted outside one
		ASSERT_EQ(scenario->objects.size(), 1u) << id;
		EXPECT_EQ(std::get<TreeInfo>(scenario->objects[0].type), TreeInfo::Oak) << id;
		EXPECT_TRUE(testbed_fixtures::Empty(scenario->fixtures)) << id;
		EXPECT_TRUE(scenario->bareTowns.empty()) << id;
		// let go of once, a little up and with no speed, so that it lands rather than being thrown
		ASSERT_EQ(scenario->throws.size(), 1u) << id;
		const auto& throwing = scenario->throws.front();
		EXPECT_EQ(throwing.object, 0u) << id;
		EXPECT_TRUE(throwing.from.has_value()) << id;
		EXPECT_GT(throwing.height, 0.0f) << id;
		EXPECT_EQ(throwing.velocity, glm::vec3(0.0f)) << id;
		EXPECT_FALSE(throwing.repeatSeconds.has_value()) << id;
	}
}

TEST(TestbedScenarios, ThrowsARockAndAToyBallAtAStandingCreature)
{
	const auto* scenario = Find("physics.rock_strikes_creature");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Physics);
	EXPECT_TRUE(Problems(*scenario).empty());
	// The player's own creature, standing still with its mind left alone, so that its own player's throws strike it
	ASSERT_EQ(scenario->creatures.size(), 1u);
	const auto& creature = scenario->creatures.front();
	EXPECT_EQ(creature.owner, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(creature.pauseMind);
	// A rock and a toy ball, each thrown at it every eight seconds, four seconds apart
	ASSERT_EQ(scenario->objects.size(), 2u);
	EXPECT_EQ(std::get<MobileStaticInfo>(scenario->objects[0].type), MobileStaticInfo::RockChalk);
	EXPECT_EQ(std::get<MobileStaticInfo>(scenario->objects[1].type), MobileStaticInfo::ToyBall);
	ASSERT_EQ(scenario->throws.size(), 2u);
	for (size_t i = 0; i < scenario->throws.size(); ++i)
	{
		const auto& throwing = scenario->throws[i];
		EXPECT_EQ(throwing.object, i);
		ASSERT_TRUE(throwing.repeatSeconds.has_value());
		EXPECT_FLOAT_EQ(*throwing.repeatSeconds, 8.0f);
		ASSERT_TRUE(throwing.from.has_value());
		// from the west, towards the creature
		EXPECT_LT(throwing.from->x, creature.offset.x);
		EXPECT_GT(throwing.velocity.x, 0.0f);
	}
	EXPECT_FLOAT_EQ(scenario->throws[1].delaySeconds - scenario->throws[0].delaySeconds, 4.0f);
}

TEST(TestbedScenarios, ASmallAndABigCreatureCarryTheSameFoodAbout)
{
	const auto* scenario = Find("growth.carrying_strength");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Growth);
	EXPECT_TRUE(Problems(*scenario).empty());
	// a small one too young to grow and a big grown one, both as weak as can be
	ASSERT_EQ(scenario->creatures.size(), 2u);
	const auto& young = scenario->creatures[0];
	const auto& grown = scenario->creatures[1];
	ASSERT_TRUE(young.phase.has_value());
	EXPECT_LT(*young.phase, 3u);
	EXPECT_GE(*young.phase, 1u);
	EXPECT_FALSE(grown.phase.has_value());
	ASSERT_TRUE(young.size.has_value() && grown.size.has_value());
	EXPECT_LT(*young.size, *grown.size);
	EXPECT_EQ(young.strength, std::optional(0.0f));
	EXPECT_EQ(grown.strength, std::optional(0.0f));
	// each picks up a pile of magic food, then both walk to and fro with it
	ASSERT_EQ(scenario->objects.size(), 2u);
	for (const auto& object : scenario->objects)
	{
		EXPECT_EQ(std::get<MobileObjectInfo>(object.type), MobileObjectInfo::MagicFood);
	}
	ASSERT_GE(scenario->commands.size(), 4u);
	for (size_t i = 0; i < 2; ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, Command::Kind::PickUp);
		EXPECT_EQ(scenario->commands[i].creature, i);
		EXPECT_EQ(scenario->commands[i].object, i);
	}
	ASSERT_TRUE(scenario->repeatFrom.has_value());
	EXPECT_EQ(*scenario->repeatFrom, 2u);
	for (size_t i = *scenario->repeatFrom; i < scenario->commands.size(); ++i)
	{
		EXPECT_EQ(scenario->commands[i].kind, Command::Kind::WalkTo);
		EXPECT_TRUE(scenario->commands[i].waitUntilFree);
	}
	EXPECT_GT(scenario->environment.bodyTimeScale, 1.0f);
}

TEST(TestbedScenarios, AYoungAndAGrownCreatureAreMadeBiggerThanABodyIsDrawn)
{
	const auto* scenario = Find("growth.oversized_brought_back");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Growth);
	// made at 6, beyond the 4 a body is drawn at, which is a size a scenario may make a creature at
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 2u);
	for (const auto& creature : scenario->creatures)
	{
		EXPECT_EQ(creature.size, std::optional(6.0f));
		EXPECT_TRUE(creature.pauseMind);
	}
	// the first too young to grow, the second grown up
	ASSERT_TRUE(scenario->creatures[0].phase.has_value());
	EXPECT_LT(*scenario->creatures[0].phase, 3u);
	EXPECT_FALSE(scenario->creatures[1].phase.has_value());
	// a size that is no size is still out of range
	auto broken = *scenario;
	broken.creatures[0].size = -1.0f;
	EXPECT_FALSE(Problems(broken).empty());
	broken.creatures[0].size = std::numeric_limits<float>::infinity();
	EXPECT_FALSE(Problems(broken).empty());
}

TEST(TestbedScenarios, CoversCastingThroughTheHand)
{
	// Every way the hand casts is tried through the action button: thrown, at a circle, held, placed, on a creature,
	// and from a seed summoned from worship
	bool thrown = false;
	bool circle = false;
	bool held = false;
	bool onCreature = false;
	bool worship = false;
	std::set<MagicType> withoutEffects;
	for (const auto& scenario : All())
	{
		for (const auto& miracle : scenario.miracles)
		{
			thrown =
			    thrown || (miracle.byHand && miracle.type == MagicType::Fireball && glm::length(miracle.throwVelocity) > 0.0f);
			circle = circle || (miracle.byHand && miracle.circleRadius.has_value());
			held = held || (miracle.byHand && miracle.holdSeconds.has_value());
			onCreature = onCreature || (miracle.byHand && miracle.target == MiracleCast::Target::Creature);
			worship = worship || (miracle.byHand && miracle.fromWorship);
			withoutEffects.insert(miracle.type);
		}
	}
	EXPECT_TRUE(thrown);
	EXPECT_TRUE(circle);
	EXPECT_TRUE(held);
	EXPECT_TRUE(onCreature);
	EXPECT_TRUE(worship);
	// The miracles with no particle effect of their own, which used to go on their first turn
	for (const auto type : {MagicType::Teleport, MagicType::FlockFlying, MagicType::FlockGround, MagicType::PhysicalShield})
	{
		EXPECT_TRUE(withoutEffects.contains(type)) << static_cast<int>(type);
	}
}

TEST(TestbedScenarios, ExamplesAreWellFormedFixtures)
{
	// Each example, with the presses its hand demo is expected to have
	const std::array<std::pair<const char*, std::optional<size_t>>, 9> examples {{
	    {"examples.miracle_near_village", 1},
	    {"examples.leashed_creature", std::nullopt},
	    {"examples.storm_lightning", std::nullopt},
	    {"examples.hand_throw_target", 4},
	    {"examples.land_change_creature", std::nullopt},
	    {"creature.walks_in_temple_pen", std::nullopt},
	    {"creature.fights_in_temple_pen", std::nullopt},
	    {"examples.temple_leash_posts", std::nullopt},
	    {"examples.objects_join_the_draw_list", std::nullopt},
	}};
	for (const auto& [id, presses] : examples)
	{
		const auto* scenario = Find(id);
		ASSERT_NE(scenario, nullptr) << id;
		EXPECT_EQ(scenario->facet, Facet::Examples) << id;
		EXPECT_TRUE(Problems(*scenario).empty()) << id;
		EXPECT_FALSE(testbed_fixtures::Empty(scenario->fixtures)) << id;
		EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, presses).empty()) << id;
		EXPECT_EQ(scenario->fixtures.handDemo.has_value(), presses.has_value()) << id;
		// Only the land change writes into the game's folder
		EXPECT_EQ(scenario->fixtures.writesGameData, std::string_view(id) == "examples.land_change_creature") << id;
	}
	EXPECT_EQ(Name(Facet::Examples), "Examples");
	// A press the demo does not have is found
	EXPECT_FALSE(testbed_fixtures::Problems(Find("examples.hand_throw_target")->fixtures, 3).empty());
}

TEST(TestbedScenarios, TheTempleExampleShowsAllThreePosts)
{
	const auto* example = Find("examples.temple_leash_posts");
	ASSERT_NE(example, nullptr);
	// The first player's temple, and the profile's creature (the first player's) knowing the three posts' leashes
	ASSERT_EQ(example->fixtures.temples.size(), 1u);
	EXPECT_EQ(example->fixtures.temples.front().owner, PlayerNames::PLAYER_ONE);
	ASSERT_EQ(example->fixtures.creatures.size(), 1u);
	const auto& creature = example->fixtures.creatures.front();
	EXPECT_TRUE(creature.file.empty());
	for (const auto type : {LeashType::Evil, LeashType::Rope, LeashType::Good})
	{
		EXPECT_NE(std::ranges::find(creature.knows, type), creature.knows.end()) << static_cast<int>(type);
	}
	// No timeline: nothing but the scene, the same every run
	EXPECT_TRUE(example->commands.empty());
}

TEST(TestbedScenarios, LandChangesAreChecked)
{
	const auto* example = Find("examples.land_change_creature");
	ASSERT_NE(example, nullptr);
	ASSERT_EQ(example->commands.size(), 1u);
	EXPECT_EQ(example->commands.front().kind, Command::Kind::ChangeLand);
	EXPECT_EQ(example->commands.front().landScript, "Land2.txt");
	EXPECT_TRUE(NeedsNoCreature(Command::Kind::ChangeLand));
	EXPECT_EQ(Name(Command::Kind::ChangeLand), "change land");

	Scenario scenario {
	    .id = "test.land_change",
	    .name = "Land change",
	    .description = "A land change",
	    .expected = "Checked",
	    .commands = {{.kind = Command::Kind::ChangeLand, .landScript = "Land2.txt"}},
	    .fixtures = {.writesGameData = true},
	};
	EXPECT_TRUE(Problems(scenario).empty());
	// It saves the creature into the game's folder, so the scenario must say it writes there
	scenario.fixtures.writesGameData = false;
	EXPECT_EQ(Problems(scenario).size(), 1u);
	scenario.fixtures.writesGameData = true;
	scenario.commands.front().landScript = {};
	EXPECT_EQ(Problems(scenario).size(), 1u);
}

TEST(TestbedScenarios, TheSpeedExampleSetsTheSpeedThenChangesLand)
{
	const auto* example = Find("examples.speed_across_land");
	ASSERT_NE(example, nullptr);
	EXPECT_EQ(example->facet, Facet::Examples);
	EXPECT_TRUE(Problems(*example).empty());
	ASSERT_EQ(example->commands.size(), 2u);
	EXPECT_EQ(example->commands.front().kind, Command::Kind::SetGameSpeed);
	EXPECT_FLOAT_EQ(example->commands.front().gameSpeed, 2.0f);
	EXPECT_EQ(example->commands.back().kind, Command::Kind::ChangeLand);
	EXPECT_EQ(example->commands.back().landScript, "Land2.txt");
	EXPECT_TRUE(example->fixtures.writesGameData);
	EXPECT_TRUE(NeedsNoCreature(Command::Kind::SetGameSpeed));
	EXPECT_EQ(Name(Command::Kind::SetGameSpeed), "set game speed");

	// A speed of 0 or below would stop the game or run it backwards
	Scenario scenario {
	    .id = "test.game_speed",
	    .name = "Game speed",
	    .description = "A game speed",
	    .expected = "Checked",
	    .commands = {{.kind = Command::Kind::SetGameSpeed, .gameSpeed = 0.5f}},
	};
	EXPECT_TRUE(Problems(scenario).empty());
	scenario.commands.front().gameSpeed = 0.0f;
	EXPECT_EQ(Problems(scenario).size(), 1u);
	scenario.commands.front().gameSpeed = -1.0f;
	EXPECT_EQ(Problems(scenario).size(), 1u);
}

TEST(TestbedScenarios, CommandsMayNameTheCreatureFixtures)
{
	// The creature fixtures are numbered after the scenario's own creatures
	const auto* fight = Find("creature.fights_in_temple_pen");
	ASSERT_NE(fight, nullptr);
	ASSERT_EQ(fight->creatures.size(), 1u);
	ASSERT_EQ(fight->fixtures.creatures.size(), 1u);
	EXPECT_EQ(fight->commands.front().creature, 1u);
	EXPECT_TRUE(Problems(*fight).empty());
	// One past them is still no creature
	auto beyond = *fight;
	beyond.commands.front().creature = 2;
	EXPECT_FALSE(Problems(beyond).empty());
}

TEST(TestbedScenarios, AnEmptyTownBesideAVillageIsWellFormed)
{
	const auto* scenario = Find("creature.compassion_empty_town");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->bareTowns.size(), 1u);
	// nearer the tiger than the village's middle (the scenario's town stands at the map's middle), and believing in the
	// player as the village does
	ASSERT_FALSE(scenario->creatures.empty());
	const auto tiger = scenario->creatures.front().offset;
	EXPECT_LT(glm::distance(scenario->bareTowns.front().offset, tiger), glm::length(tiger));
	EXPECT_EQ(scenario->bareTowns.front().belief, scenario->townBelief);
	// a bare town off the map is a problem
	auto off = *scenario;
	off.bareTowns.front().offset = {1.0e6f, 0.0f};
	EXPECT_FALSE(Problems(off).empty());
}

TEST(TestbedScenarios, AVillagesFarHurtFarmerIsWellFormed)
{
	const auto* scenario = Find("creature.compassion_far_hurt_villager");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	// the hurt farmer belongs to the village and lies farther than the old 200 m reach
	ASSERT_EQ(scenario->objects.size(), 3u);
	EXPECT_TRUE(scenario->objects.back().joinTown);
	EXPECT_GT(glm::length(scenario->objects.back().offset), 200.0f);
}

TEST(TestbedScenarios, VillagerTreeGoneIsWellFormed)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("examples.villager_tree_gone");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Examples);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
	// A villager, the tree on its way and the storage pit that takes it, with no creature
	EXPECT_TRUE(scenario->creatures.empty());
	ASSERT_EQ(scenario->objects.size(), 3u);
	EXPECT_TRUE(std::holds_alternative<VillagerInfo>(scenario->objects[0].type));
	EXPECT_TRUE(std::holds_alternative<TreeInfo>(scenario->objects[1].type));
	EXPECT_TRUE(std::holds_alternative<AbodeInfo>(scenario->objects[2].type));
	// The tree stands on the straight line from the villager to its goal, between them
	ASSERT_EQ(scenario->commands.size(), 2u);
	const auto& walk = scenario->commands[0];
	const auto& take = scenario->commands[1];
	EXPECT_EQ(walk.kind, Kind::VillagerWalkTo);
	EXPECT_EQ(walk.object, 0u);
	const auto from = scenario->objects[0].offset;
	const auto tree = scenario->objects[1].offset;
	const auto along = glm::dot(tree - from, glm::normalize(walk.point - from));
	EXPECT_GT(along, 0.0f);
	EXPECT_LT(along, glm::length(walk.point - from));
	EXPECT_NEAR(glm::length(tree - from), along, 0.01f);
	// The pit takes the tree once the villager walks round it
	EXPECT_EQ(take.kind, Kind::StoreTakes);
	EXPECT_EQ(take.object, 1u);
	EXPECT_EQ(take.value, 2u);
	EXPECT_TRUE(take.waitUntilFree);
	EXPECT_TRUE(NeedsNoCreature(Kind::VillagerWalkTo));
	EXPECT_TRUE(NeedsNoCreature(Kind::StoreTakes));
	EXPECT_EQ(Name(Kind::VillagerWalkTo), "villager walk to");
	EXPECT_EQ(Name(Kind::StoreTakes), "store takes");

	// A walk of no villager, and a store that is no building or is the thing taken, are found
	auto broken = *scenario;
	broken.commands = {{.kind = Kind::VillagerWalkTo, .object = 1},
	                   {.kind = Kind::StoreTakes, .value = 0, .object = 1},
	                   {.kind = Kind::StoreTakes, .value = 1, .object = 1},
	                   {.kind = Kind::StoreTakes, .value = 2, .object = 3}};
	EXPECT_EQ(Problems(broken).size(), 4u);
}

TEST(TestbedScenarios, TheHealthScenarioReadsEachLivingThingOnceBeforeTheShot)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("script.health_of_living");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->creatures.empty());
	ASSERT_EQ(scenario->objects.size(), 5u);
	// Three sheep at full life, half and a quarter, then two villagers at full life and half
	for (size_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::get<AnimalInfo>(scenario->objects[i].type), AnimalInfo::Sheep) << i;
	}
	EXPECT_FALSE(scenario->objects[0].life.has_value());
	EXPECT_FLOAT_EQ(*scenario->objects[1].life, 0.5f);
	EXPECT_FLOAT_EQ(*scenario->objects[2].life, 0.25f);
	EXPECT_TRUE(std::holds_alternative<VillagerInfo>(scenario->objects[3].type));
	EXPECT_TRUE(std::holds_alternative<VillagerInfo>(scenario->objects[4].type));
	EXPECT_FALSE(scenario->objects[3].life.has_value());
	EXPECT_FLOAT_EQ(*scenario->objects[4].life, 0.5f);
	// Each read once, in order, all before the shot of the checks' 900 frames of 16 ms, taken 20 frames before the end
	ASSERT_EQ(scenario->commands.size(), 5u);
	float seconds = 0.0f;
	for (size_t i = 0; i < scenario->commands.size(); ++i)
	{
		const auto& command = scenario->commands[i];
		EXPECT_EQ(command.kind, Kind::ScriptHealth) << i;
		EXPECT_EQ(command.object, i);
		seconds += command.delaySeconds;
	}
	EXPECT_LT(seconds, 880.0f * 0.016f);
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptHealth));
	EXPECT_EQ(Name(Kind::ScriptHealth), "script health");

	// A read of an object the scenario doesn't have is a problem
	Scenario wrong = *scenario;
	wrong.commands[0].object = 5;
	EXPECT_EQ(Problems(wrong).size(), 1u);
}

TEST(TestbedScenarios, TheItchyOnALeashedCreatureScenarioLeashesAThinkingTargetBeforeTheCast)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("miracles.creature_itchy_on_leashed_creature");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 2u);
	// The target thinks for itself, and its wants are not set again every frame, so that what the spell does to them shows
	const auto& target = scenario->creatures[1];
	EXPECT_FALSE(target.pauseMind);
	EXPECT_FALSE(target.hold);
	// The target, not the caster, is the creature the player leads, so the leash is taken
	EXPECT_EQ(target.owner, scenario->creatures[0].owner);
	EXPECT_EQ(target.leashable, std::optional<bool>(true));
	EXPECT_EQ(scenario->creatures[0].leashable, std::optional<bool>(false));
	ASSERT_EQ(scenario->commands.size(), 5u);
	const auto& leash = scenario->commands[0];
	const auto& cast = scenario->commands[2];
	EXPECT_EQ(leash.kind, Kind::PutOnLeash);
	EXPECT_EQ(leash.creature, 1u);
	EXPECT_EQ(scenario->commands[1].kind, Kind::KnowMiracle);
	EXPECT_EQ(cast.kind, Kind::CastMiracle);
	EXPECT_EQ(cast.creature, 0u);
	EXPECT_EQ(cast.atCreature, 1u);
	EXPECT_EQ(cast.value, static_cast<size_t>(MagicType::CreatureSpellItchy));
	// The leash's state on the target is logged twice after the cast, the second before the shot of the checks' 900
	// frames of 16 ms, taken 20 frames before the end
	float seconds = 0.0f;
	for (const auto& command : scenario->commands)
	{
		seconds += command.delaySeconds;
	}
	EXPECT_EQ(scenario->commands[3].kind, Kind::LeashState);
	EXPECT_EQ(scenario->commands[4].kind, Kind::LeashState);
	EXPECT_EQ(scenario->commands[4].creature, 1u);
	EXPECT_LT(seconds, 880.0f * 0.016f);
	EXPECT_EQ(Name(Kind::LeashState), "leash state");
	// Within the five heights a tiger casts from, so it casts without walking
	EXPECT_LT(glm::distance(target.offset, scenario->creatures[0].offset), 75.0f);
}

TEST(TestbedScenarios, TheFoodCastScenarioCastsFoodIntoTheTownsStoragePit)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("miracles.creature_casts_food_on_town");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Miracles);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 1u);
	ASSERT_EQ(scenario->commands.size(), 2u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::KnowMiracle);
	EXPECT_EQ(scenario->commands[0].value, static_cast<size_t>(MagicType::Food));
	const auto& cast = scenario->commands[1];
	EXPECT_EQ(cast.kind, Kind::CastMiracle);
	EXPECT_EQ(cast.value, static_cast<size_t>(MagicType::Food));
	// Cast at the storage pit, which, as every building of a scenario, belongs to the scenario's town, as the huts do
	ASSERT_LT(cast.object, scenario->objects.size());
	ASSERT_TRUE(std::holds_alternative<AbodeInfo>(scenario->objects[cast.object].type));
	EXPECT_EQ(std::get<AbodeInfo>(scenario->objects[cast.object].type), AbodeInfo::CelticStoragePit);
	for (const auto& object : scenario->objects)
	{
		EXPECT_TRUE(std::holds_alternative<AbodeInfo>(object.type));
	}
}

TEST(TestbedScenarios, TheWaterCastScenarioCastsWaterOnAWheatField)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("miracles.creature_waters_field");
	ASSERT_NE(scenario, nullptr);
	EXPECT_EQ(scenario->facet, Facet::Miracles);
	EXPECT_TRUE(Problems(*scenario).empty());
	ASSERT_EQ(scenario->creatures.size(), 1u);
	ASSERT_EQ(scenario->commands.size(), 2u);
	EXPECT_EQ(scenario->commands[0].kind, Kind::KnowMiracle);
	EXPECT_EQ(scenario->commands[0].value, static_cast<size_t>(MagicType::Water));
	const auto& cast = scenario->commands[1];
	EXPECT_EQ(cast.kind, Kind::CastMiracle);
	EXPECT_EQ(cast.value, static_cast<size_t>(MagicType::Water));
	ASSERT_LT(cast.object, scenario->objects.size());
	ASSERT_TRUE(std::holds_alternative<FieldTypeInfo>(scenario->objects[cast.object].type));
	EXPECT_EQ(std::get<FieldTypeInfo>(scenario->objects[cast.object].type), FieldTypeInfo::Wheat);
}

TEST(TestbedScenarios, MakesTheFourthLandsPuzzleTotemsAsItsScriptsDo)
{
	using Kind = Command::Kind;
	EXPECT_EQ(Name(Kind::ScriptPuzzleTotem), "script puzzle totem");
	EXPECT_EQ(Name(Kind::PuzzleTotemHeights), "puzzle totem heights");
	EXPECT_TRUE(NeedsNoCreature(Kind::ScriptPuzzleTotem));
	EXPECT_TRUE(NeedsNoCreature(Kind::PuzzleTotemHeights));
	// five bell towers at the Totem Puzzle's angles, at full height and unpickupable
	const auto* towers = Find("story.bell_towers");
	ASSERT_NE(towers, nullptr);
	EXPECT_TRUE(Problems(*towers).empty());
	std::vector<float> angles;
	for (const auto& command : towers->commands)
	{
		if (command.kind == Kind::ScriptPuzzleTotem)
		{
			EXPECT_EQ(command.value, static_cast<size_t>(AbodeInfo::Belltower));
			EXPECT_FLOAT_EQ(command.built, 1.0f);
			EXPECT_TRUE(command.unpickupable);
			angles.push_back(command.angleDegrees);
		}
	}
	EXPECT_EQ(angles, (std::vector<float> {340.0f, 10.0f, 50.0f, 80.0f, 100.0f}));
	// the undead village's totem, sunk, in the player's influence, dragged up by the hand
	const auto* drag = Find("story.undead_totem_drag");
	ASSERT_NE(drag, nullptr);
	EXPECT_TRUE(Problems(*drag).empty());
	ASSERT_FALSE(drag->commands.empty());
	EXPECT_EQ(drag->commands.front().kind, Kind::ScriptPuzzleTotem);
	EXPECT_EQ(drag->commands.front().value, static_cast<size_t>(AbodeInfo::AfricanTotem));
	EXPECT_FLOAT_EQ(drag->commands.front().built, 0.0f);
	EXPECT_EQ(std::ranges::count(drag->commands, Kind::PointerSweep, &Command::kind), 1);
	ASSERT_TRUE(drag->townBelief.has_value());
}

// The creature watching the player's hand pour water: every step goes through the pointer and the Action button, as a
// player plays it (the globe tapped, then the water poured over the creature), and the creatures' sightings are logged
TEST(TestbedScenarios, TheHandPouringWaterByTheCreatureIsPlayedThroughThePointer)
{
	using Kind = Command::Kind;
	const auto* scenario = Find("creature.watches_hand_pour_water");
	ASSERT_NE(scenario, nullptr);
	EXPECT_TRUE(Problems(*scenario).empty());
	EXPECT_TRUE(scenario->logSightings);
	ASSERT_EQ(scenario->creatures.size(), 1u);
	ASSERT_EQ(scenario->dispensers.size(), 1u);
	EXPECT_EQ(scenario->dispensers.front().type, MagicType::Water);
	EXPECT_TRUE(scenario->dispensers.front().bubbleHeight.has_value());
	EXPECT_TRUE(std::ranges::all_of(scenario->commands, [](const Command& command) {
		return command.kind == Kind::PointerTo || command.kind == Kind::PointerPress || command.kind == Kind::PointerRelease;
	}));
	EXPECT_EQ(std::ranges::count(scenario->commands, Kind::PointerPress, &Command::kind), 2);
}
