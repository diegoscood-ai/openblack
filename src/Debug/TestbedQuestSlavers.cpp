/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's silver scroll The Slavers as quest runs on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"): after the arrival (quest=SetupLand2/EnterLand2), the Greek village is handed to the player, as
// winning it would, and the quest's watcher is started on it, as the land's control script starts it. The scroll at
// the villager's hut is pressed on with the hand and the introduction's texts are clicked on. One run trades animals
// at the slavers' pen until every slave is free, with a slave and a circus animal taken from them on the way; one
// trades until a single slave is left, takes that one and then brings a dead animal; one takes two slaves and then
// kills a slaver, and lets the war on the village play; one kills every slaver once they reach the village; one
// declares the war by thefts alone and starts it again by a theft and by a killing. The camp lies outside the player's
// influence but for the animal pen's ring, and its animals live far from it, so each animal is put into the pen by a script's
// set position, where a player would carry it; a circus animal inside the ring is taken with the hand and thrown out, through
// the mouse. The points are map coordinates, taken from the places the quest's scripts name and the animals the land's map
// script makes.

#include <cstdint>

#include <string_view>
#include <vector>

#include "TestbedLand2Quests.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
using testbed_land_timeline::ScriptArgument;

/// The Greek village south-west of Khazar's home, where the land's map script makes it
constexpr glm::vec2 k_Village {2248.68f, 2240.36f};
constexpr ThingNear k_VillageThing {.type = script::ObjectType::Town, .point = k_Village, .radius = 30.0f};
/// The belief the village is given, more than anyone holds in it
constexpr float k_WinningBelief = 2.0f;
/// The villager's hut west of the village centre, where the scroll is put up
constexpr glm::vec2 k_Hut {2182.6f, 2209.541f};
/// The slavers' two pens on the hill south of the village: the animal pen, where animals are brought, and the slave pen
constexpr glm::vec2 k_AnimalPen {2276.1667f, 2001.518f};
constexpr glm::vec2 k_SlavePen {2234.8267f, 1974.1627f};
/// The slavers' camp fire
constexpr glm::vec2 k_Camp {2255.874f, 1975.426f};
/// Where a slave taken from the pen is put down: 25 south of the pen's centre, more than the 10 the slavers watch
constexpr glm::vec2 k_SlaveDrop {2234.8267f, 1949.1627f};
/// A spot 40 south of the animal pen, out of the 10 the slavers look in, where an animal is left before it is killed
constexpr glm::vec2 k_Staging {2276.1667f, 1961.518f};
/// Where a circus animal taken from the pen is thrown: 40 north of the pen's centre, out of the 10 the slavers look in,
/// and in view of the camera south of the pen
constexpr glm::vec2 k_ThrowAway {2276.1667f, 2041.518f};
/// How long the Action button is held on a thing before it is let go: past the grab's wait, so the hand picks it up
constexpr float k_GrabHoldSeconds = 0.6f;
/// A living thing is pointed at half its height above its feet, at its body, as a player aims at it: the camera's ray
/// to its feet passes under a horse's belly
constexpr float k_BodyAimShare = 0.5f;
/// An effect's blow that leaves a villager no life
constexpr float k_DeadlyBlow = 10.0f;

/// The scripts' sub-types of the animals and the slaves used
constexpr uint32_t k_Lion = 0;
constexpr uint32_t k_Tiger = 1;
constexpr uint32_t k_Cow = 8;
constexpr uint32_t k_Horse = 9;
/// A Greek housewife, three of whom are among the slaves and none among the slavers
constexpr uint32_t k_Housewife = 42;
/// The camera above the village and back from it, with its centre on the screen
constexpr float k_VillageEyeHeight = 60.0f;
constexpr float k_VillageEyeBack = 70.0f;
/// The slavers, Egyptian shepherds
constexpr uint32_t k_Slaver = 39;

/// The five lions the quest makes east of the pen and lets go, a herd of horses north of the village, the tigers
/// north-west of it and the cows west of it, where the land's map script makes them
constexpr ThingNear k_LionThing {
    .type = script::ObjectType::Animal, .subtype = k_Lion, .point = {2456.1826f, 2046.8511f}, .radius = 80.0f};
constexpr ThingNear k_HorseThing {
    .type = script::ObjectType::Animal, .subtype = k_Horse, .point = {2262.0f, 2440.0f}, .radius = 30.0f};
constexpr ThingNear k_TigerThing {
    .type = script::ObjectType::Animal, .subtype = k_Tiger, .point = {2124.0f, 2467.0f}, .radius = 30.0f};
constexpr ThingNear k_CowThing {
    .type = script::ObjectType::Animal, .subtype = k_Cow, .point = {2225.0f, 2155.0f}, .radius = 20.0f};
/// A cow left at the staging spot, and a living horse of the circus at the pen
constexpr ThingNear k_StagedCow {.type = script::ObjectType::Animal, .subtype = k_Cow, .point = k_Staging, .radius = 5.0f};
constexpr ThingNear k_CircusHorse {.type = script::ObjectType::Animal,
                                   .subtype = k_Horse,
                                   .point = k_AnimalPen,
                                   .radius = 15.0f,
                                   .aliveOnly = true,
                                   .aimShare = k_BodyAimShare};
/// A slave in the slave pen, and a slaver at the camp
constexpr ThingNear k_SlaveThing {
    .type = script::ObjectType::Villager, .subtype = k_Housewife, .point = k_SlavePen, .radius = 5.0f};
constexpr ThingNear k_SlaverThing {.type = script::ObjectType::Villager, .subtype = k_Slaver, .point = k_Camp, .radius = 30.0f};
/// Any adult slave in the slave pen, and the child among them (the forester, aged 8): the slavers' rounds never come
/// within 7 of its centre
constexpr ThingNear k_AnySlave {.type = script::ObjectType::Villager, .point = k_SlavePen, .radius = 5.0f};
constexpr ThingNear k_ChildSlave {.type = script::ObjectType::VillagerChild, .point = k_SlavePen, .radius = 5.0f};
/// A slaver in the village, once they have marched on it
constexpr ThingNear k_SlaverInVillage {.type = script::ObjectType::Villager,
                                       .subtype = k_Slaver,
                                       .point = k_Village,
                                       .radius = 100.0f,
                                       .aimShare = k_BodyAimShare};
/// A living slaver in or near the village, passing over the ones already dead
constexpr ThingNear k_LiveSlaverInVillage {
    .type = script::ObjectType::Villager, .subtype = k_Slaver, .point = k_Village, .radius = 150.0f, .aliveOnly = true};
/// The slavers left once the chief is dead
constexpr int k_SlaversAfterChief = 4;
/// The chief, as the scripts' global names him, wherever he walks
constexpr ThingNear k_Chief {.type = script::ObjectType::Villager, .point = k_Camp, .radius = 300.0f, .global = "ChiefGuard"};
/// How many times the village is looked at after the first arrival, 5 seconds apart, and how many rounds a recording
/// is given to take the chief
constexpr int k_ArrivalLooks = 12;
constexpr int k_ChiefRounds = 12;

/// How long an uncertain beat is given before the timeline goes on: the trade scenes take about 20 seconds
constexpr float k_TradeSeconds = 30.0f;

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals = "SlaverFinished SlaverTotalLions SlaverTotalTigers SlaverTotalWolves "
                                       "SlaverTotalLeopard SlaverTotalTortoise SlaverTotalZebra SlaverTotalHorse "
                                       "SlaverVictimCount";
constexpr std::string_view k_Scripts = "SlaversCheck TheSlaversMain SlaversIntro SlaverChiefGuard SlaverGuardOne "
                                       "SlaverGuardTwo SlaverGuardThree SlaverGuardFour SlaverCheckAnimal "
                                       "AllSlavesReleased SlaversSlaveWarning SlaversAnimalWarning SlaversFinalWarning "
                                       "SlaverKilled SlaversVeryAngry TownAttack SlaverGoMad SlaversKilledEnough "
                                       "GiveSpellDispenserReward";
/// How long the camp is watched after the introduction, for the slavers' rounds about it: the chief's takes about a
/// minute
constexpr float k_RoundsSeconds = 75.0f;

/// The camera south of the animal pen and above it, with the pen on the screen, as the slavers' trade asks
Command CameraOnPen()
{
	return CameraOn(k_AnimalPen, 30.0f, 40.0f, 0.0f);
}

/// A thing of the land moved to a point by the scripts' own SET_POSITION, as a script moves it
Command MoveThingTo(const ThingNear& thing, glm::vec2 to, float delay = 1.0f)
{
	return {.kind = Kind::MoveThing, .delaySeconds = delay, .point = to, .thing = thing};
}

/// A script's SET_PROPERTY of HEALTH to 0 on the thing: it dies
Command Kill(const ThingNear& thing, float delay = 1.0f)
{
	return {.kind = Kind::ScriptSetHealth, .delaySeconds = delay, .amount = 0.0f, .thing = thing};
}

/// The village takes more belief in the player than in anyone else, as a gift gives it; its next fold hands it over
Command WinVillage()
{
	return {.kind = Kind::TownGainsBelief,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = 1.0f,
	        .amount = k_WinningBelief,
	        .thing = k_VillageThing};
}

/// The start: the village handed to the player and the watcher started on it, the scroll pressed on and the
/// introduction let play to its end. Without the press, the camera is left on the scroll for a recording's press
std::vector<Command> Introduction(bool pressScroll = true)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    // [shortcut] the Greek village takes more belief in the player than in anyone else, as winning it over would
	    // give it; its next fold hands it to the player (a direct change of hands is undone by that fold)
	    WinVillage(),
	    // the quest's watcher, started on the village as the land's control script starts it
	    {.kind = Kind::StartScript,
	     .script = "SlaversCheck",
	     .scriptArguments = {ScriptArgument {.kind = ScriptArgument::Kind::Thing,
	                                         .thingType = static_cast<uint8_t>(script::ObjectType::Town),
	                                         .thingPoint = k_Village,
	                                         .thingRadius = 30.0f}}},
	    WaitTask("ChallengeHighlightNotify"),
	    {.kind = Kind::LogGlobals, .delaySeconds = 1.0f, .value = 60, .global = k_Globals, .script = k_Scripts},
	    // [camera] the scroll over the villager's hut
	    CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 4.0f),
	};
	// [real] the scroll pressed on
	if (pressScroll)
	{
		PressScroll(commands, k_Hut);
	}
	// the introduction: the villager's line, the camp, the chief's three lines and the camera given back
	commands.push_back(WaitText(1755));
	commands.push_back(WaitTask("SlaversIntro", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	return commands;
}

/// An animal put into the animal pen, once no trade or warning is playing, with the camera on the pen; then the trade
/// waited for to its end
void Deliver(std::vector<Command>& commands, const ThingNear& animal)
{
	commands.push_back(WaitTask("SlaverCheckAnimal", TaskWait::NotRunning));
	commands.push_back(CameraOnPen());
	// [shortcut] the animal put into the pen, where a player would carry it
	commands.push_back(MoveThingTo(animal, k_AnimalPen));
	commands.push_back(WaitTask("SlaverCheckAnimal", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 0.5f));
}

/// A slave taken out of the slave pen and put down outside it; then the warning it draws waited for to its end
void TakeSlave(std::vector<Command>& commands, const ThingNear& slave, std::string_view warning)
{
	commands.push_back(WaitTask("SlaverCheckAnimal", TaskWait::NotRunning));
	commands.push_back(CameraOnPen());
	// [shortcut] a slave put down 25 south of the slave pen, where a player would carry one; the pen is out of the
	// player's influence
	commands.push_back(MoveThingTo(slave, k_SlaveDrop));
	commands.push_back(WaitTask(warning, TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 0.5f));
}

/// A living horse of the circus taken from the animal pen, which the pen's influence ring lets the hand reach: the
/// pointer on its body, the Action button pressed once the hand is over it and held past the grab's wait, which picks
/// it up, and let go, the horse kept in the hand; then the hand's own throw at a spot north of the pen. Then the scene
/// the smaller circus draws waited for to its end, and the throw tried again in case the scene kept the hand from it.
/// If the press never comes, a run with OPENBLACK_HAND_TRACE=1 logs what the hand's pick looked at under the pointer
void StealHorse(std::vector<Command>& commands, std::string_view scene)
{
	commands.push_back(WaitTask("SlaverCheckAnimal", TaskWait::NotRunning));
	commands.push_back(CameraOnPen());
	// [real] the horse taken into the hand and thrown out of the pen, through the mouse
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_CircusHorse});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.3f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = k_CircusHorse});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 0.5f, .point = k_ThrowAway});
	commands.push_back(WaitTask(scene, TaskWait::Finished));
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 0.5f, .point = k_ThrowAway});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 0.5f));
}

/// A dead cow brought: a cow left beside the pen, killed, then put into the pen with the camera on it
void DeliverDeadCow(std::vector<Command>& commands)
{
	commands.push_back(WaitTask("SlaverCheckAnimal", TaskWait::NotRunning));
	// [shortcut] a cow put beside the pen, and then killed by a script's health of 0, where a player would kill one
	commands.push_back(MoveThingTo(k_CowThing, k_Staging));
	commands.push_back(Kill(k_StagedCow));
	commands.push_back(CameraOnPen());
	// [shortcut] the dead cow put into the pen, where a player would carry it
	commands.push_back(MoveThingTo(k_StagedCow, k_AnimalPen));
}

/// [shortcut] An effect's blow at a slaver, which leaves him no life, where the player's miracle or the creature would
/// strike him
Command Blow(const ThingNear& slaver, float delay)
{
	return {.kind = Kind::EffectHit,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = delay,
	        .amount = k_DeadlyBlow,
	        .thing = slaver};
}

/// Waits until at least that many of the village have been killed by the slavers
Command WaitVictims(float count)
{
	return {.kind = Kind::WaitGlobal,
	        .waitUntilFree = true,
	        .amount = count,
	        .global = "SlaverVictimCount",
	        .compare = Compare::AtLeast};
}

std::vector<Command> Trades()
{
	auto commands = Introduction();
	// [camera] the camp, watched while the slavers go about their rounds
	commands.push_back(CameraOn(k_Camp, 30.0f, 40.0f, 0.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RoundsSeconds));
	// a lion: worth two slaves
	Deliver(commands, k_LionThing);
	// a slave taken: the chief's first warning
	TakeSlave(commands, k_SlaveThing, "SlaversSlaveWarning");
	// two horses, worth a slave each, and a third, of a kind the slavers have enough of
	Deliver(commands, k_HorseThing);
	Deliver(commands, k_HorseThing);
	Deliver(commands, k_HorseThing);
	// a cow, which the slavers do not want
	Deliver(commands, k_CowThing);
	// a dead cow; the trade, if the dead cow is taken for one, is given a fixed time
	DeliverDeadCow(commands);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_TradeSeconds));
	// a horse of the circus taken with the hand: the circus is smaller, which the slavers take for a theft; the animal
	// warning and a recount of the kinds kept
	StealHorse(commands, "SlaversAnimalWarning");
	// the tiger, worth two slaves. With one slave left (a lion of the quest's own pack walked into the pen in an
	// earlier run and was paid for) it frees him alone and the slaves are all free; with three left it frees two
	Deliver(commands, k_TigerThing);
	// a horse, paid for again after the recount: it frees the last if any is left
	commands.push_back(WaitTask("SlaverCheckAnimal", TaskWait::NotRunning));
	commands.push_back(CameraOnPen());
	// [shortcut] the horse put into the pen, where a player would carry it
	commands.push_back(MoveThingTo(k_HorseThing, k_AnimalPen));
	// the slaves freed: the circus finale, the log at 1, and the dispenser's reward with the evil advisor's lines, which
	// the finale's script waits for
	commands.push_back(WaitTask("AllSlavesReleased", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	return commands;
}

std::vector<Command> LastStolen()
{
	auto commands = Introduction();
	// two lions, worth two slaves each, and a horse, worth one: two of the adults and the child left. A trade pays only
	// an adult, and only an adult taken out counts as an escape; the child goes only with a trade that finds no adult
	Deliver(commands, k_LionThing);
	Deliver(commands, k_LionThing);
	Deliver(commands, k_HorseThing);
	// the last two adults taken: the chief's first and final warnings, and with the child left no win at the thefts
	TakeSlave(commands, k_AnySlave, "SlaversSlaveWarning");
	TakeSlave(commands, k_AnySlave, "SlaversFinalWarning");
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_TradeSeconds));
	// the next animal brought: its trade finds no adult and takes the child, which empties the pen; the win follows the
	// trade, with the circus finale and the reward
	Deliver(commands, k_HorseThing);
	commands.push_back(WaitTask("AllSlavesReleased", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	return commands;
}

std::vector<Command> War()
{
	auto commands = Introduction();
	// a horse, so the circus has an animal when the war comes; worth one slave, so the slaves left never number four,
	// as the slavers do once one is dead
	Deliver(commands, k_HorseThing);
	// two slaves taken: the chief's first and final warnings
	TakeSlave(commands, k_SlaveThing, "SlaversSlaveWarning");
	TakeSlave(commands, k_SlaveThing, "SlaversFinalWarning");
	// the slaver nearest the camp's centre, the chief, struck dead
	commands.push_back(Blow(k_SlaverThing, 2.0f));
	// the evil advisor's remark and the chief's declaration of war; then the march on the village
	commands.push_back(WaitText(1772));
	commands.push_back(WaitTask("SlaversVeryAngry", TaskWait::Finished));
	// [camera] the village the slavers march on
	commands.push_back(CameraOn(k_Village, k_VillageEyeHeight, k_VillageEyeBack, 0.0f));
	// the good advisor's lines on their arrival, then the killing
	commands.push_back(WaitText(1773));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	// [real] some seconds into the hunt, the slaver nearest the village centre followed, taken into the hand by his
	// body and, held a while, thrown down at the centre: his victim is let go
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 8.0f, .amount = 1.5f, .thing = k_SlaverInVillage});
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 2.0f, .value = 1});
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 0.5f, .thing = k_SlaverInVillage});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.3f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = k_SlaverInVillage});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = k_GrabHoldSeconds, .value = k_ActionButton});
	commands.push_back(CameraOn(k_Village, k_VillageEyeHeight, k_VillageEyeBack, 0.0f));
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 4.0f, .point = k_Village});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	// the killing until ten of the village are dead
	commands.push_back(WaitVictims(10.0f));
	// the slavers stopped where they stand and the log entry updated, a moment after the tenth death
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
	return commands;
}

std::vector<Command> AllDead()
{
	auto commands = Introduction();
	// as the war run: a horse traded, two slaves taken, the chief struck dead and the war declared
	Deliver(commands, k_HorseThing);
	TakeSlave(commands, k_SlaveThing, "SlaversSlaveWarning");
	TakeSlave(commands, k_SlaveThing, "SlaversFinalWarning");
	commands.push_back(Blow(k_SlaverThing, 2.0f));
	commands.push_back(WaitText(1772));
	commands.push_back(WaitTask("SlaversVeryAngry", TaskWait::Finished));
	// [camera] the village the slavers march on; the good advisor's lines on their arrival
	commands.push_back(CameraOn(k_Village, k_VillageEyeHeight, k_VillageEyeBack, 0.0f));
	commands.push_back(WaitText(1773));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	// the four slavers left struck dead one by one in the village: the posse empties and the war ends in their deaths
	for (int slaver = 0; slaver < k_SlaversAfterChief; ++slaver)
	{
		commands.push_back(Blow(k_LiveSlaverInVillage, 3.0f));
	}
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 10.0f));
	return commands;
}

std::vector<Command> TheftWar()
{
	auto commands = Introduction();
	// a horse, worth a slave: the circus has an animal to be taken. One circus horse stands in the pen at each theft,
	// so the hand's pick under the pointer finds the horse the pointer is on
	Deliver(commands, k_HorseThing);
	// a slave taken, then the circus horse: the two thefts are counted apart, a warning each
	TakeSlave(commands, k_AnySlave, "SlaversSlaveWarning");
	StealHorse(commands, "SlaversAnimalWarning");
	// a second slave: the final warning; a second horse brought, worth a slave too, and taken from the circus: war,
	// declared by a theft
	TakeSlave(commands, k_AnySlave, "SlaversFinalWarning");
	Deliver(commands, k_HorseThing);
	StealHorse(commands, "SlaversVeryAngry");
	// [camera] the camp, watched: the slavers' camp rounds go on, walking each back to his spot, so the posse stays
	commands.push_back(CameraOn(k_Camp, 30.0f, 40.0f, 0.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 30.0f));
	// a third slave taken: war again, a second attack, which waits for the posse as the first does
	TakeSlave(commands, k_AnySlave, "SlaversVeryAngry");
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.5f));
	// a horse brought in the war: the pen still trades, the chief called back to it
	Deliver(commands, k_HorseThing);
	// the slaver nearest the camp's centre struck dead: the evil advisor's remark and war a third time. The slavers
	// are marked finished, so their rounds end and the posse marches on the village
	commands.push_back(Blow(k_SlaverThing, 2.0f));
	commands.push_back(WaitText(1763));
	commands.push_back(WaitTask("SlaversVeryAngry", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	// [camera] the village; the attacks' arrivals one after the other, each with the good advisor's line, and the
	// count of the dead read between them
	commands.push_back(CameraOn(k_Village, k_VillageEyeHeight, k_VillageEyeBack, 0.0f));
	commands.push_back(WaitText(1773));
	for (int look = 0; look < k_ArrivalLooks; ++look)
	{
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
	}
	return commands;
}

/// The war by thefts played for a recording: the start without the scroll's press, which the recording makes; three
/// slaves put down outside the slave pen, the third declaring the war; then, round after round, the camera behind the
/// chief and then south of the animal pen, for the recording to take him with the hand and throw him north, downhill.
/// The chief's round brings him within the pen's influence ring at his watch spot
std::vector<Command> TheftKillForRecording()
{
	auto commands = Introduction(false);
	TakeSlave(commands, k_AnySlave, "SlaversSlaveWarning");
	TakeSlave(commands, k_AnySlave, "SlaversFinalWarning");
	TakeSlave(commands, k_AnySlave, "SlaversVeryAngry");
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	for (int round = 0; round < k_ChiefRounds; ++round)
	{
		// [camera] behind the chief, for his take
		commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 2.0f, .amount = 1.0f, .thing = k_Chief});
		commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 5.0f, .value = 1});
		// [camera] south of the animal pen, the north up the screen, for his throw
		commands.push_back(CameraOnPen());
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
	}
	return commands;
}

} // namespace

void testbed_scenarios::land2_quests::AddSlaversScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.the_slavers",
	    .name = "Land 2 quest run: The Slavers, the slaves bought free",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). The texts that wait "
	                   "for a click are clicked on, the arrival's too. Once the arrival is over it hands the Greek "
	                   "village to the player and starts the quest's watcher on it, as the land's control script does; "
	                   "presses on the scroll at the villager's hut with the hand and lets the introduction play. Then, "
	                   "with the camera on the animal pen, it puts into the pen by a script's set position a lion, three "
	                   "horses, a cow and a dead cow, one at a time once each trade is over; after the first lion it puts "
	                   "a slave down outside the slave pen. After the dead cow the hand takes a horse of the circus, "
	                   "through the mouse, and throws it out of the pen; then a tiger and a horse are put into the pen.",
	    .expected = "The scroll's press plays the introduction (texts 1775, 1753, 1754 and 1755) and the log at 0. The "
	                "lion pays two slaves (1759), each horse one (1758) until the third, of which there are plenty "
	                "(1779); the cow is not wanted (1757) and the dead cow is turned away (1756 or 1771); each turned "
	                "away is beamed off. The slave taken joins the village and draws the chief's warning (1762); the "
	                "horse taken from the circus draws the animal warning (1761) and a recount, after which the "
	                "slavers keep one horse. The tiger pays what is left, up to two, and the horse, paid for again, the "
	                "last if any is left: the finale (1765, 1760, 1777), the log at 1 and the wolf pack dispenser's "
	                "reward (5253 to 5255); the end check logs SlaverFinished=1. Unverified: that the horses, the "
	                "cows and the tiger are where the land's map script makes them.",
	    .commands = Trades(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_slavers_last_stolen",
	    .name = "Land 2 quest run: The Slavers, the last adults stolen",
	    .facet = Facet::Story,
	    .description = "As quest.the_slavers up to the introduction's end. Then, with the camera on the animal pen, two "
	                   "lions and a horse put into the pen by a script's set position, once each trade is over; then two "
	                   "adult slaves put down outside the slave pen, one at a time once its warning is over; then a "
	                   "horse put into the pen.",
	    .expected = "The lions pay two slaves each (1759) and the horse one (1758): two adults and the child left. Each "
	                "adult taken draws its warning, the first (1762) and the final one; with the child still in the "
	                "pen nothing is won at the thefts (AllSlavesReleased does not run, SlaverFinished=0). The horse's "
	                "trade finds no adult and takes the child: the pen is empty after it, and the finale (1765, 1760, "
	                "1777) and the reward follow. As the original, the child is never paid for by a trade nor counted "
	                "as escaped; it is only taken by a trade that finds no adult.",
	    .commands = LastStolen(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_slavers_war",
	    .name = "Land 2 quest run: The Slavers, a slaver killed and the war",
	    .facet = Facet::Story,
	    .description = "Plays the start as quest.the_slavers does and trades a horse; then puts two slaves down outside "
	                   "the slave pen one after the other, each once the warning before is over, strikes the slaver "
	                   "nearest the camp's centre dead with an effect's blow and watches the village from above. Some "
	                   "seconds into the slavers' hunt the hand takes the slaver nearest the village centre, through "
	                   "the mouse, and throws him down at the centre.",
	    .expected = "The first slave taken draws the chief's warning (1762), the second his final warning (1778); the "
	                "dead slaver draws the evil advisor's remark (1763) and the declaration of war (1772) by another "
	                "slaver if the chief was the one struck. The horse is driven to the village and set loose, the "
	                "scroll moves into the village, and the slavers march there: a house is set alight and the good "
	                "advisor says 1773 and 1774. Each slaver then kills the village's people; the one taken into the "
	                "hand lets his victim go; at ten dead they stand frozen and the log is at 1 with alignment -1. "
	                "Unverified: how long the march takes, that the village has ten people within the slavers' reach, "
	                "and that the slaver taken was then going for a victim. Known: the circus horse is not driven to "
	                "the village (it stays in the pen, still held by the script), so the circus is never set loose.",
	    .commands = War(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_slavers_all_dead",
	    .name = "Land 2 quest run: The Slavers, every slaver killed",
	    .facet = Facet::Story,
	    .description = "As quest.the_slavers_war up to the slavers' arrival in the village: a horse traded, two slaves "
	                   "put down outside the slave pen and the chief struck dead with an effect's blow. Then, with the "
	                   "camera on the village, the four slavers left are struck dead the same way, one every few "
	                   "seconds, each the living slaver nearest the village centre.",
	    .expected = "The war plays as in quest.the_slavers_war up to the good advisor's 1773. Each slaver struck dead "
	                "shrinks the posse; once it is empty the attack ends and the log entry is updated to success 1 "
	                "with alignment -0.65. Unverified: that every slaver is within 150 of the village centre when "
	                "struck.",
	    .commands = AllDead(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_slavers_theft_war",
	    .name = "Land 2 quest run: The Slavers, war declared by thefts",
	    .facet = Facet::Story,
	    .description = "Plays the start as quest.the_slavers does and trades a horse; then, each once the scene before "
	                   "is over, puts a slave down outside the slave pen, takes the circus horse with the hand and throws "
	                   "it out of the pen, puts a second slave down, trades a second horse and takes it the same way. "
	                   "With the camera on the camp for 30 seconds it puts a third slave down, trades a horse and strikes "
	                   "the slaver nearest the camp's centre dead with an effect's blow; then watches the village.",
	    .expected = "The thefts draw the chief's slave warning (1762), his animal warning (1761), his final warning "
	                "(1778) and, at the fourth, the declaration of war (1772); the camp rounds walk the slavers back to "
	                "their spots, so the posse stays at the camp. The third slave taken declares war again (1772), a "
	                "second attack that waits too. The horse is still traded in the war, the chief called back to the "
	                "pen. The dead slaver draws the evil advisor's remark (1763), the slavers marked finished, and war "
	                "a third time: the rounds end, the posse marches and each attack's arrival has the good advisor's "
	                "1773 and sets the count of the dead to none. Unverified: how long the march takes.",
	    .commands = TheftWar(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "realinput.the_slavers_theft_kill",
	    .name = "Land 2 quest run for a recording: The Slavers, war by thefts and the chief killed by the hand",
	    .facet = Facet::Story,
	    .description = "For tools/checks/real_input_tap.sh, with no pointer commands: plays the start as "
	                   "quest.the_slavers does but leaves the scroll's press to the recording, puts three slaves down "
	                   "outside the slave pen one at a time once each scene before is over, then, round after round, "
	                   "follows the chief for 5 seconds and puts the camera south of the animal pen, for the "
	                   "recording to take the chief with the hand and throw him.",
	    .expected = "The recording's press starts the introduction. The slaves draw the slave warning (1762), the "
	                "final warning (1778) and the war (1772), and the posse stays at the camp. A chief thrown to his "
	                "death draws the evil advisor's remark (1763), war again and the march. Unverified: that the chief "
	                "is within the animal pen's influence when the recording presses on him.",
	    .commands = TheftKillForRecording(),
	    .onLand = true,
	});
}
