/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRegistry.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <numbers>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <glm/geometric.hpp>

#include "3D/FlatLand.h"
#include "3D/LandLightTable.h"
#include "3D/NightLights.h"
#include "Common/HelpText.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "Graphics/HandLight.h"
#include "Graphics/Sun.h"
#include "Help/Spirits.h"
#include "Particles/ParticleTypes.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedDispenserGrid.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

// How to add a scenario: append one to the list Build() returns, in the section of its facet. Give it a new id
// ("facet.what", never reused or renamed, as tests and the command line pick scenarios by it), a name, its facet, a
// description of what it sets up and what to look for, and then its data: the environment (land, hour, weather, body
// time), the framing, the creatures with the bodies, needs and desires they start with, the objects on the land, and
// the commands, which play in turn (commands on objects name them by their place in the scenario's objects). Offsets are from
// the middle of the map, x east and y north; the testbed's camera looks north from 120 units south of the middle, and the lake
// lies beyond the middle (k_Lake, its open water 100 units a side, ringed by 20 units of shallows and a bank). Keep other
// scenarios clear of it. A new facet goes into the Facet enum and its Name. test_testbed_scenarios
// checks the data of every scenario; anything a scenario needs that the runner can't yet do goes into these types and
// TestbedScenarioRunner together.

namespace
{
using Kind = Command::Kind;
using creature_desires::Desire;
namespace animations = creature_layers::animations;

/// How far apart the creatures of a lineup stand
constexpr float k_RowSpacing = 32.0f;
/// The highest hour of the day
constexpr float k_HoursPerDay = 24.0f;
/// Offsets further from the middle of the map than this would be off it
constexpr float k_MaxOffset = 2400.0f;
/// A command that waits for its creature to be free first gives the last command this long to get it going
constexpr float k_SettleSeconds = 0.5f;
/// The last stage of growing up, and how many skills, miracles and deeds to copy the game's tables have
constexpr size_t k_LastPhase = 13;
constexpr size_t k_Skills = 6;
constexpr size_t k_Miracles = 42;
/// The rows of the game's table of dances
constexpr size_t k_DanceTypes = 25;
/// The game's script interface levels (SET_INTERFACE_INTERACTION's 0..15)
constexpr size_t k_InterfaceLevels = 16;
constexpr size_t k_Deeds = 46;
/// The game's spell seeds, and its last gesture (the square wave)
constexpr size_t k_SeedCount = 30;
constexpr size_t k_LastGesture = 23;
/// The seed of every benchmark's crowd, so that runs lay it out the same
constexpr uint32_t k_BenchmarkSeed = 2026;
/// The testbed's lake, from the middle of the map: the middle of its open water, half its width, and the width of the
/// shallows round it
constexpr glm::vec2 k_Lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
constexpr glm::vec2 k_LakeHalf = flat_land::k_LakeHalfExtent;
constexpr float k_ShallowsWidth = 20.0f;
/// The near and far shallows of the lake, as the testbed's camera looks north across it
constexpr float k_NearShallows = k_Lake.y - k_LakeHalf.y - (k_ShallowsWidth * 0.5f);
constexpr float k_FarShallows = k_Lake.y + k_LakeHalf.y + (k_ShallowsWidth * 0.5f);

/// The lake's open water, for the overview to keep in view
std::vector<glm::vec2> LakeInView()
{
	return {k_Lake - k_LakeHalf, k_Lake + k_LakeHalf};
}

Command Go(Kind kind, size_t creature, glm::vec2 point, float delay = 0.0f, bool wait = true)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .point = point};
}

Command Play(Kind kind, size_t creature, size_t animation, float delay, bool wait)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .value = animation};
}

Command Act(Kind kind, size_t creature, float delay = 0.0f, bool wait = false)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait};
}

Command AtHour(float hour, float delay)
{
	return {.kind = Kind::SetHour, .delaySeconds = delay, .hour = hour};
}

/// A command on one of the scenario's objects: picking it up, knocking it down, tying the leash to it
Command OnObject(Kind kind, size_t creature, size_t object, float delay = 0.0f, bool wait = true)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .waitUntilFree = wait, .object = object};
}

Command Stroke(size_t creature, creature_feedback::BodyPart part, float delay)
{
	return {.kind = Kind::HandStroke,
	        .creature = creature,
	        .delaySeconds = delay,
	        .waitUntilFree = true,
	        .bodyPart = static_cast<size_t>(part)};
}

Command Slap(size_t creature, float height, bool gentle, bool sweepsRight, float delay)
{
	return {.kind = Kind::HandSlap,
	        .creature = creature,
	        .delaySeconds = delay,
	        .waitUntilFree = true,
	        .slapHeight = height,
	        .gentle = gentle,
	        .sweepsRight = sweepsRight};
}

/// Pulling the face the creature's mind would for a feeling
Command Feel(size_t creature, creature_face::Cue cue, float delay)
{
	return {.kind = Kind::ShowFeeling, .creature = creature, .delaySeconds = delay, .value = static_cast<size_t>(cue)};
}

Command Leash(size_t creature, LeashType type, float delay)
{
	return {.kind = Kind::PutOnLeash, .creature = creature, .delaySeconds = delay, .leash = type};
}

/// A body wanting nothing: full, rested, watered, comfortable
NeedOverrides Content()
{
	return {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f};
}

/// Every desire gone but one, which is as strong as it gets
std::vector<DesireOverride> OnlyDesire(Desire wanted)
{
	std::vector<DesireOverride> overrides;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto desire = static_cast<Desire>(i);
		overrides.push_back({.desire = desire, .fraction = desire == wanted ? 1.0f : 0.0f});
	}
	return overrides;
}

/// A creature kept content, so that its needs stay out of what the scenario shows
CreatureSetup Content(CreatureType species, glm::vec2 offset, float facing = 0.0f, std::string_view label = {})
{
	return {.label = label, .species = species, .offset = offset, .facingDegrees = facing, .needs = Content(), .hold = true};
}

/// A content creature whose mind is paused, so that it only does what the scenario tells it: its idle mind would
/// otherwise wander off or sit down in the middle of a walk
CreatureSetup Posed(CreatureType species, glm::vec2 offset, float facing = 0.0f, std::string_view label = {})
{
	auto creature = Content(species, offset, facing, label);
	creature.pauseMind = true;
	return creature;
}

/// How far east of the middle the item of a row stands, the first furthest west so that the testbed's camera,
/// looking north, sees the row in order from left to right
float RowX(size_t index, size_t count, float spacing = k_RowSpacing)
{
	return (static_cast<float>(index) - ((static_cast<float>(count) - 1.0f) * 0.5f)) * spacing;
}

/// Walking round the points in turn, each once it has arrived at the last
void WalkRound(std::vector<Command>& commands, size_t creature, std::span<const glm::vec2> points, bool run = false)
{
	for (const auto& point : points)
	{
		commands.push_back(Go(run ? Kind::RunTo : Kind::WalkTo, creature, point, 0.0f, true));
	}
}

std::vector<glm::vec2> Circle(glm::vec2 centre, float radius, size_t points)
{
	std::vector<glm::vec2> circle;
	for (size_t i = 0; i < points; ++i)
	{
		const auto angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(points);
		circle.emplace_back(centre + (radius * glm::vec2(std::cos(angle), std::sin(angle))));
	}
	return circle;
}

void AddIdle(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "idle.fidget",
	    .name = "Idle fidgeting",
	    .facet = Facet::Idle,
	    .description = "A content tiger in the middle of the testbed with nothing to do and nothing it needs.",
	    .expected = "With nothing it needs it has no plan, and like the game's creature it stays where it is and does "
	                "nothing of its own. With the creature window's old idle policy switched on it looks about, pulls "
	                "faces, sits down for a while and every so often wanders off a little way, as openblack's stand-in "
	                "did.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	});

	all.push_back({
	    .id = "idle.fidget_old_policy",
	    .name = "Idle fidgeting, openblack's old idle policy",
	    .facet = Facet::Idle,
	    .description = "idle.fidget's content tiger with openblack's old idle policy switched on from the start, as the "
	                   "creature window's switch puts it, so that the two can be compared from the same exe.",
	    .expected = "Unlike the game's creature in idle.fidget, it looks about, pulls faces, sits down for a while and "
	                "every so often wanders off a little way, as openblack's stand-in did. The policy is put back when "
	                "the scenario stops.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {{.kind = Kind::IdlePolicy, .delaySeconds = 0.0f, .value = 1}},
	});

	std::vector<Command> hangAround;
	const std::array<glm::vec2, 3> spots {glm::vec2 {50.0f, 40.0f}, glm::vec2 {-45.0f, 55.0f}, glm::vec2 {0.0f, -10.0f}};
	for (const auto& spot : spots)
	{
		hangAround.push_back(Go(Kind::WalkTo, 0, spot, 0.5f, true));
		hangAround.push_back(Act(Kind::SitDown, 0, 0.5f, true));
		hangAround.push_back(Act(Kind::StandUp, 0, 6.0f));
	}
	all.push_back({
	    .id = "idle.hang_around",
	    .name = "Hang around",
	    .facet = Facet::Idle,
	    .description = "A tiger walks off a little way and sits there, then on to another spot, as its idle mind does "
	                   "when it hangs around; here its mind is paused and it is told to, so it happens every time.",
	    .expected = "It walks to each of three spots near the middle, sits down there for a while, gets up and goes on "
	                "to the next.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -10.0f}, 180.0f)},
	    .commands = hangAround,
	    .repeatFrom = 0,
	});
}

void AddExpressions(std::vector<Scenario>& all)
{
	// The desires a creature shows with an emote but doesn't act on by itself
	constexpr std::array<Desire, 10> k_Shown {
	    Desire::Impress,   Desire::Compassion, Desire::Anger,
	    Desire::Play,      Desire::Fear,       Desire::Curiosity,
	    Desire::BeFriends, Desire::Sadness,    Desire::AttractAttention,
	    Desire::GetWarmer,
	};
	std::vector<CreatureSetup> emoters;
	for (size_t i = 0; i < k_Shown.size(); ++i)
	{
		const auto row = i / 5;
		auto creature = Content(CreatureType::Tiger, {RowX(i % 5, 5, 36.0f), static_cast<float>(row) * 40.0f}, 0.0f,
		                        creature_desires::Name(k_Shown.at(i)));
		creature.desires = OnlyDesire(k_Shown.at(i));
		emoters.push_back(creature);
	}
	all.push_back({
	    .id = "expressions.desire_emotes",
	    .name = "Desire emotes",
	    .facet = Facet::Expressions,
	    .description = "Ten tigers in two rows, each with one desire as strong as it gets and every other desire gone: "
	                   "impress, compassion, anger, play, fear (front row, left to right), curiosity, be friends, "
	                   "sadness, attract attention and get warmer (back row).",
	    .expected = "Every few seconds each shows its desire with its emote: summoning, feeling nice, taunting, feeling "
	                "playful, frightened, confused, happy, sad, waving and cold. The readout names each one's desire.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = emoters,
	});

	std::vector<Command> faces;
	for (size_t i = 0; i < animations::k_IdleFaceCount; ++i)
	{
		faces.push_back(Play(Kind::PullFace, 0, animations::k_FirstFace + i, 2.5f, false));
	}
	for (size_t i = 0; i < animations::k_GestureCount; ++i)
	{
		faces.push_back(Play(Kind::PlayGesture, 0, animations::k_FirstGesture + i, 3.0f, false));
	}
	all.push_back({
	    .id = "expressions.faces_gestures",
	    .name = "Faces and gestures gallery",
	    .facet = Facet::Expressions,
	    .description = "A tiger whose mind is paused, close up, pulls each of its ten faces in turn, then plays each "
	                   "gesture on top of its body.",
	    .expected =
	        "Smile, grimace, growl, scared, sad, amazed, puzzled, laugh, ooh and aah, each held then easing out of the last, "
	        "then a nod, a shake of the head, a yawn, thirst, squirting water and talking.",
	    .framing = {.shot = Shot::Head, .distance = 1.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = faces,
	    .repeatFrom = 0,
	});

	// Each shows one feeling in its face as the game's minds do: as its activity starts and again every four seconds
	struct Feeling
	{
		std::string_view label;
		std::optional<creature_face::Cue> cue;
	};
	const std::array<Feeling, 7> k_Feelings {{
	    {"stroked", std::nullopt},
	    {"slapped", std::nullopt},
	    {"frightened", creature_face::Cue::Fear},
	    {"angry", creature_face::Cue::Anger},
	    {"exhausted", creature_face::Cue::Exhausted},
	    {"amazed", creature_face::Cue::Amazed},
	    {"curious", creature_face::Cue::Curiosity},
	}};
	std::vector<CreatureSetup> feelers;
	std::vector<Command> feelings {Act(Kind::Stroke, 0, 0.5f), Act(Kind::Slap, 1, 0.1f)};
	for (size_t i = 0; i < k_Feelings.size(); ++i)
	{
		auto creature = Content(CreatureType::Tiger, {RowX(i, k_Feelings.size(), 24.0f), 0.0f}, 0.0f, k_Feelings.at(i).label);
		creature.pauseMind = k_Feelings.at(i).cue.has_value();
		feelers.push_back(creature);
		if (const auto cue = k_Feelings.at(i).cue)
		{
			feelings.push_back(Feel(i, *cue, 0.1f));
		}
	}
	feelings.push_back(Act(Kind::Stop, 0, creature_mind::k_FaceRepeatSeconds));
	all.push_back({
	    .id = "expressions.faces_emotions",
	    .name = "Faces and emotions",
	    .facet = Facet::Expressions,
	    .description = "Seven tigers in a row, each feeling something different: one stroked and one slapped over and "
	                   "over with their minds running, then (paused) frightened, angry, exhausted, amazed and curious, "
	                   "each pulling its feeling's face every four seconds as its mind would.",
	    .expected = "The stroked tiger shows it is happy with a smile, the slapped one sad with a sad face. The "
	                "frightened one is scared or goes ooh, the angry one growls or grimaces, the exhausted one grimaces, "
	                "the amazed one is amazed, the curious one puzzled, amazed or aah. Each face plays once and holds, "
	                "then eases back; no mouth opens and closes over and over. The spawner's mind panel names each "
	                "face's reason.",
	    .framing = {.shot = Shot::Overview, .distance = 0.8f},
	    .creatures = feelers,
	    .commands = feelings,
	    .repeatFrom = 0,
	});

	std::vector<Command> actions;
	for (size_t i = 0; i < animations::k_ActionCount; ++i)
	{
		actions.push_back(Play(Kind::PlayAction, 0, animations::k_FirstAction + i, 0.5f, true));
	}
	all.push_back({
	    .id = "expressions.actions",
	    .name = "Action gallery",
	    .facet = Facet::Expressions,
	    .description = "A tiger whose mind is paused plays every one of its actions in turn, from summoning to the "
	                   "friendly wave and pick me, each once the last has finished.",
	    .expected = "Each action plays through once and the body goes back to standing between them; the spawner's mind "
	                "panel names the one playing.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = actions,
	    .repeatFrom = 0,
	});

	// The same action played as it is and then mirrored, each once the last has finished
	auto asItIs = Play(Kind::PlayAction, 0, animations::k_FirstAction, 0.5f, true);
	asItIs.mirrored = false;
	auto mirrored = asItIs;
	mirrored.mirrored = true;
	all.push_back({
	    .id = "creature.mirrored_action",
	    .name = "An action played as it is, then mirrored",
	    .facet = Facet::Expressions,
	    .description = "A cow whose mind is paused plays its first action as it is, then mirrored.",
	    .expected = "The mirrored action is the first one seen in a mirror: each bone moves as its mirror bone did, the "
	                "pairs coming from the cow's own file, so the bones near the middle of its body (its spine and "
	                "head) move as their partners did too.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Cow, .needs = Content(), .hold = true, .pauseMind = true}},
	    .commands = {asItIs, mirrored},
	    .repeatFrom = 0,
	});
}

void AddSenses(std::vector<Scenario>& all)
{
	const std::array<glm::vec2, 4> loop {glm::vec2 {45.0f, 45.0f}, glm::vec2 {-45.0f, 45.0f}, glm::vec2 {-45.0f, -35.0f},
	                                     glm::vec2 {45.0f, -35.0f}};
	std::vector<Command> commands {Act(Kind::SitDown, 0, 1.0f)};
	WalkRound(commands, 1, loop);
	all.push_back({
	    .id = "senses.watch_walker",
	    .name = "Look-around: watching a walker",
	    .facet = Facet::Senses,
	    .description = "A tiger sits in the middle while a cow walks round and round it.",
	    .expected = "The tiger's head and eyes turn to follow the cow as it goes round, looking away to something else "
	                "now and then and back again; once it loses interest it watches something new.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "watcher"),
	                  Posed(CreatureType::Cow, loop.at(3), 180.0f, "walker")},
	    .commands = commands,
	    .repeatFrom = 0,
	});
}

void AddNeeds(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "needs.thirst",
	    .name = "Thirsty creature finds water",
	    .facet = Facet::Needs,
	    .description = "A parched tiger starts north of the middle, short of the testbed's lake, with its desire for "
	                   "water as strong as it gets.",
	    .expected = "It walks down the bank to the nearest edge of the lake, bends down and drinks; its thirst clears "
	                "and the desire for water is held back for a while.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 60.0f},
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.95f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Water, .fraction = 1.0f}},
	    }},
	});

	all.push_back({
	    .id = "needs.hunger",
	    .name = "Hungry creature finds food",
	    .facet = Facet::Needs,
	    .description = "A hungry tiger with two pots of food on the land in front of it, each half a meal.",
	    .expected = "It walks up to the nearer food and eats it, then, if still hungry, the other; its energy fills up "
	                "and the hunger desire falls.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.2f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 0.9f}},
	    }},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, -50.0f}, .amount = 400},
	                {.type = PotInfo::FoodPot, .offset = {40.0f, -90.0f}, .amount = 400}},
	});

	all.push_back({
	    .id = "needs.starving",
	    .name = "Starving creature faints",
	    .facet = Facet::Needs,
	    .description = "A tiger with almost no energy left and no food anywhere, its body's time ten times as fast.",
	    .expected = "Its hunger desire grows and it shows it is hungry; once its energy runs out it faints where it "
	                "stands, lies out cold, and gets up again with a little energy back.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.02f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 1.0f}},
	    }},
	});

	all.push_back({
	    .id = "needs.sleep_day",
	    .name = "Tired creature sleeps by day",
	    .facet = Facet::Needs,
	    .description = "A worn out tiger at midday, its desire to sleep strong, its body's time ten times as fast.",
	    .expected = "It may yawn, then lies down and sleeps, eyes closed, until it is rested, and gets up again.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.75f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}},
	    }},
	});

	all.push_back({
	    .id = "needs.sleep_night",
	    .name = "Tired creature sleeps at night",
	    .facet = Facet::Needs,
	    .description = "Ten at night, the clock standing still: a tiger a little tired, which the night makes more so.",
	    .expected = "Under the night sky its desire to sleep grows with the dark; it lies down and sleeps, and sleeps "
	                "longer than by day.",
	    .environment = {.hour = 22.0f, .clockRuns = false},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.5f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Tiredness, .fraction = 0.6f}},
	    }},
	});

	all.push_back({
	    .id = "needs.poo",
	    .name = "Needs to poo",
	    .facet = Facet::Needs,
	    .description = "A tiger full of poo with its desire to go as strong as it gets.",
	    .expected = "It may show it needs a poo first, then squats and goes; a poo drops behind it and the desire is "
	                "held back for a while.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.95f},
	        .desires = {{.desire = Desire::Poo, .fraction = 1.0f}},
	    }},
	});

	all.push_back({
	    .id = "needs.puke",
	    .name = "Puke",
	    .facet = Facet::Needs,
	    .description = "A tiger is told to be sick every so often, as eating something bad would make it.",
	    .expected = "It retches and is sick, drops of it flying out in front of it and landing on the ground, where they "
	                "fade.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Act(Kind::Puke, 0, 2.0f), Act(Kind::Puke, 0, 10.0f, true)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "needs.exhaustion",
	    .name = "Exhaustion and fainting from running",
	    .facet = Facet::Needs,
	    .description = "An already tired tiger is made to run back and forth across the testbed, its body's time twenty "
	                   "times as fast.",
	    .expected = "Running tires it; past 0.8 exhaustion it slows down, and at 1 it faints and lies out cold, then is "
	                "carried in a fizz through static, with the teleport's energise sound, to its pen, the middle of the "
	                "testbed, where it rests and gets up again before running on.",
	    .environment = {.bodyTimeScale = 20.0f},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {-150.0f, 0.0f},
	        .facingDegrees = 270.0f,
	        .needs = {.energy = 1.0f, .exhaustion = 0.6f, .dehydration = 0.0f, .poo = 0.0f},
	    }},
	    // Each run is long enough to cross; the commands don't wait for it to be free, as its idle mind sits it down
	    // whenever it stops
	    .commands = {Go(Kind::RunTo, 0, {150.0f, 0.0f}, 0.5f, false), Go(Kind::RunTo, 0, {-150.0f, 0.0f}, 16.0f, false),
	                 Go(Kind::RunTo, 0, {150.0f, 0.0f}, 16.0f, false)},
	    .repeatFrom = 1,

	});

	// The needs left to themselves: nothing is held and no desire is set, only the body's time runs faster, so the
	// needs build up as they would in play and the creature's mind sees to them when it chooses to
	all.push_back({
	    .id = "needs.left_alone",
	    .name = "Looks after itself",
	    .facet = Facet::Needs,
	    .description = "A tiger left to itself with three pots of food about it and the lake to the north, nothing held "
	                   "and no desire set, its body's time ten times as fast.",
	    .expected = "Over a few minutes its energy runs down, it gets hungry and goes to eat a pot of food, picking it up "
	                "first; the meal fills it with poo and it goes; it tires and sleeps, and it gets thirsty and walks to "
	                "the lake to drink. In between it sees to its other desires, such as calling for attention.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger}},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {40.0f, -40.0f}},
	                {.type = PotInfo::FoodPot, .offset = {-60.0f, -30.0f}},
	                {.type = PotInfo::FoodPot, .offset = {20.0f, -90.0f}}},
	});

	all.push_back({
	    .id = "needs.gets_hungry",
	    .name = "Gets hungry and eats",
	    .facet = Facet::Needs,
	    .description = "A tiger half full, two pots of food on the land in front of it, nothing held and no desire set, "
	                   "its body's time ten times as fast.",
	    .expected = "Its energy runs down and its hunger grows by itself until it is the strongest desire; it walks up "
	                "to the nearer pot, picks it up, looks at it and eats it, and its energy fills up.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = {.energy = 0.55f}}},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, -40.0f}},
	                {.type = PotInfo::FoodPot, .offset = {50.0f, -80.0f}}},
	});

	all.push_back({
	    .id = "needs.gets_thirsty",
	    .name = "Gets thirsty and drinks at the lake",
	    .facet = Facet::Needs,
	    .description = "A tiger south of the lake and already rather thirsty, nothing held and no desire set, its body's "
	                   "time ten times as fast.",
	    .expected = "Its thirst grows until its desire for water is the strongest; it walks to the near edge of the lake, "
	                "bends down and drinks, its thirst clears and it doesn't want water for a while.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 80.0f}, .needs = {.dehydration = 0.5f}}},
	});

	all.push_back({
	    .id = "needs.gets_tired",
	    .name = "Gets tired at night and sleeps",
	    .facet = Facet::Needs,
	    .description = "Late evening with the clock running, a tiger a little worn out, nothing held and no desire set, "
	                   "its body's time ten times as fast.",
	    .expected = "As night falls its tiredness grows from the dark and its exhaustion; it may yawn, then lies down and "
	                "sleeps with its eyes closed, resting, and wakes once rested, looking sleepy.",
	    .environment = {.hour = 21.5f, .bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = {.exhaustion = 0.5f}}},
	});

	all.push_back({
	    .id = "needs.needs_to_poo",
	    .name = "Needs to poo after a meal",
	    .facet = Facet::Needs,
	    .description = "A tiger that has just eaten, half full of poo, nothing held and no desire set, its body's time "
	                   "ten times as fast.",
	    .expected = "Its desire to poo grows from the poo inside it; it may show it needs to go, then squats and goes, a "
	                "lump drops behind it and the desire is held back for a while.",
	    .environment = {.bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = {.poo = 0.6f}}},
	});

	all.push_back({
	    .id = "needs.cold",
	    .name = "Cold creature",
	    .facet = Facet::Needs,
	    .description = "A blizzard over the whole island, freezing cold, and a tiger out in it, its body's time ten "
	                   "times as fast. Its other needs are kept met, and its desires to be friends and for attention, "
	                   "which would show before the cold, are held at nothing.",
	    .expected = "Its warmth falls to -1 within seconds, its desire to get warmer grows to its most and it shows it "
	                "is cold with its emote; the snow starts to lie on the land round it.",
	    .environment = {.weather = Weather::Blizzard, .bodyTimeScale = 10.0f},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	        .desires = {{.desire = Desire::BeFriends, .fraction = 0.0f},
	                    {.desire = Desire::AttractAttention, .fraction = 0.0f}},
	        .hold = true,
	    }},
	});

	// Villagers at half life walking to and fro, a walk short enough to arrive before the next, so that each arrival
	// takes them to their next state, one that may pause (having nothing to do, as a villager of no town has; deciding
	// afresh never pauses), and may make them pause for a second; the front row poisoned, which makes them
	// pause more often and pause otherwise
	std::vector<ObjectSetup> pausing;
	for (size_t i = 0; i < 6; ++i)
	{
		const bool poisoned = i < 3;
		const glm::vec2 from {-20.0f + 20.0f * static_cast<float>(i % 3), poisoned ? 20.0f : 40.0f};
		pausing.push_back({.type = VillagerInfo::CelticFarmerMale,
		                   .offset = from,
		                   .walkTo = from + glm::vec2(0.0f, 4.0f),
		                   .walkAfterSeconds = 0.5f,
		                   .walkRepeatSeconds = 5.0f,
		                   .walkFinal = VillagerStates::NothingToDo,
		                   .life = 0.5f,
		                   .poisoned = poisoned});
	}
	all.push_back({
	    .id = "villagers.poisoned_pause",
	    .name = "Poisoned and healthy villagers pausing for a second",
	    .facet = Facet::Needs,
	    .description = "Six villagers at half life, three of them poisoned in the front row and three healthy behind, "
	                   "each walking four metres to and fro every five seconds, arriving with nothing to do before it "
	                   "turns back.",
	    .expected = "Now and then, as a villager's state changes, it stops for a second and goes on. A healthy one "
	                "pauses with one of the two pause clips, picked at random; a poisoned one pauses with its own sick "
	                "clip and picks none, so the random trace has one draw fewer for each poisoned pause. Poisoned "
	                "villagers pause more often: their life counts half.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, 15.0f}, {25.0f, 50.0f}}},
	    .objects = pausing,
	});
}

void AddGrowth(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "growth.timelapse",
	    .name = "Growth time-lapse",
	    .facet = Facet::Growth,
	    .description = "A baby tiger, small and early in growing up, kept fed and still, its body's time a thousand "
	                   "times as fast.",
	    .expected = "It grows fast while young and more slowly as it ages, up to size 2, the most a creature grows by "
	                "itself; the readout shows its size.",
	    .environment = {.bodyTimeScale = 1000.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-50.0f, -40.0f}, {50.0f, 60.0f}}},
	    .creatures = {CreatureSetup {
	        .label = "baby",
	        .species = CreatureType::Tiger,
	        .size = 0.3f,
	        .phase = 3,
	        .needs = Content(),
	        .hold = true,
	        .pauseMind = true,
	    }},
	});

	// A young creature on the move, to show whether it grows while it walks
	auto walker = Posed(CreatureType::Tiger, {-40.0f, 0.0f}, 90.0f, "young walker");
	walker.size = 0.3f;
	walker.phase = 3;
	std::vector<Command> walkAbout;
	const std::array<glm::vec2, 2> ends {glm::vec2 {40.0f, 0.0f}, glm::vec2 {-40.0f, 0.0f}};
	WalkRound(walkAbout, 0, ends);
	all.push_back({
	    .id = "growth.walking_young",
	    .name = "A young creature growing as it walks",
	    .facet = Facet::Growth,
	    .description = "A young tiger, small and early in growing up, kept fed, walks back and forth between two points "
	                   "east and west of the middle, its body's time a hundred times as fast.",
	    .expected = "It grows as it walks as well as when it stands, up to size 2, the most a creature grows by itself; "
	                "the readout shows its size.",
	    .environment = {.bodyTimeScale = 100.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -30.0f}, {60.0f, 50.0f}}},
	    .creatures = {walker},
	    .commands = walkAbout,
	    .repeatFrom = 0,
	});

	// Two creatures carrying the same heavy food about: a small young one, for which it weighs all it can, and a big
	// grown one, many times heavier, for which it weighs little
	auto young = Posed(CreatureType::Tiger, {-32.0f, 10.0f}, 0.0f, "small carrier");
	young.size = 0.3f;
	young.phase = 2;
	young.strength = 0.0f;
	auto grown = Posed(CreatureType::Tiger, {32.0f, 10.0f}, 0.0f, "big carrier");
	grown.size = 2.0f;
	grown.strength = 0.0f;
	all.push_back({
	    .id = "growth.carrying_strength",
	    .name = "Two creatures made stronger by what they carry",
	    .facet = Facet::Growth,
	    .description = "A small young tiger in the west and a big grown one in the east, both as weak as can be, each "
	                   "pick up a pile of magic food and walk to and fro with it, their bodies' time twenty times as "
	                   "fast. The small one is still too young to grow.",
	    .expected = "Both get stronger as they walk carrying the food, by how heavy it is for them: a creature weighs "
	                "its mass, from its size, fatness and strength. For the small one the food weighs as much as "
	                "itself or more, so its strength rises at the most it can, to full within about a minute; the big "
	                "one weighs hundreds of times as much and gains only a little. The readout shows their strength.",
	    .environment = {.bodyTimeScale = 20.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-70.0f, -30.0f}, {70.0f, 50.0f}}},
	    .creatures = {young, grown},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-32.0f, 0.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {32.0f, 0.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), OnObject(Kind::PickUp, 1, 1), Go(Kind::WalkTo, 0, {-50.0f, 0.0f}),
	                 Go(Kind::WalkTo, 1, {50.0f, 0.0f}), Go(Kind::WalkTo, 0, {-15.0f, 0.0f}),
	                 Go(Kind::WalkTo, 1, {15.0f, 0.0f})},
	    .repeatFrom = 2,
	});

	// Two creatures made bigger than the most a body is drawn at: a young one, which does not grow yet, and a grown one
	auto oversizedYoung = Posed(CreatureType::Tiger, {-40.0f, 20.0f}, 0.0f, "young at 6");
	oversizedYoung.size = 6.0f;
	oversizedYoung.phase = 2;
	auto oversizedGrown = Posed(CreatureType::Tiger, {40.0f, 20.0f}, 0.0f, "grown at 6");
	oversizedGrown.size = 6.0f;
	all.push_back({
	    .id = "growth.oversized_brought_back",
	    .name = "Creatures made bigger than a body is drawn",
	    .facet = Facet::Growth,
	    .description = "Two tigers made at size 6, beyond the 4 a creature's body is drawn at: a young one in the west, "
	                   "still too young to grow, and a grown one in the east.",
	    .expected = "Both keep the size they are made with, and both bodies are drawn at 4. On its first growth turn "
	                "the grown one is put back to 2, the most a creature grows to by itself, and is drawn at 2; the "
	                "young one keeps 6 and stays drawn at 4. The readout shows their own sizes.",
	    .framing = {.shot = Shot::Overview, .include = {{-90.0f, -20.0f}, {90.0f, 90.0f}}},
	    .creatures = {oversizedYoung, oversizedGrown},
	});

	// A full creature eating more than it needs, to show the fat it puts on coming into its body
	auto full = Posed(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "full");
	full.fatness = 0.2f;
	full.hold = false;
	all.push_back({
	    .id = "growth.overeating",
	    .name = "A full creature overeating",
	    .facet = Facet::Growth,
	    .description = "A tiger with all the energy it can take and a thin body picks up three piles of food about it "
	                   "one after another and eats each from its hand.",
	    .expected = "Each meal is more than it needs, and what is beyond full makes it fatter; its body fills out "
	                "towards the new fatness a little each turn, from the turn it eats.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {full},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {0.0f, -10.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -5.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {15.0f, -5.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 1, 1.0f),
	                 Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 2, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	struct Shape
	{
		std::string_view label;
		float alignment;
		float fatness;
		float strength;
	};
	constexpr std::array<Shape, 7> k_Shapes {{
	    {"thin", 0.0f, 0.0f, 0.5f},
	    {"fat", 0.0f, 1.0f, 0.5f},
	    {"weak", 0.0f, 0.5f, 0.0f},
	    {"strong", 0.0f, 0.5f, 1.0f},
	    {"evil", -1.0f, 0.5f, 0.5f},
	    {"good", 1.0f, 0.5f, 0.5f},
	    {"neutral", 0.0f, 0.5f, 0.5f},
	}};
	std::vector<CreatureSetup> lineup;
	for (size_t i = 0; i < k_Shapes.size(); ++i)
	{
		const auto& shape = k_Shapes.at(i);
		auto creature = Content(CreatureType::Tiger, {RowX(i, k_Shapes.size()), 0.0f}, 0.0f, shape.label);
		creature.alignment = shape.alignment;
		creature.fatness = shape.fatness;
		creature.strength = shape.strength;
		creature.size = 1.0f;
		creature.pauseMind = true;
		lineup.push_back(creature);
	}
	all.push_back({
	    .id = "growth.morph_lineup",
	    .name = "Morph lineup",
	    .facet = Facet::Growth,
	    .description = "Seven tigers of the same size side by side, left to right: thin, fat, weak, strong, evil, good "
	                   "and neutral.",
	    .expected = "Each body is pulled towards its axis' mesh: a lean and a heavy belly, slight and bulging muscles, "
	                "the evil one hunched and dark and the good one upright and bright, the last as the base mesh.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = lineup,
	});

	std::vector<CreatureSetup> parade;
	constexpr size_t k_Species = static_cast<size_t>(CreatureType::_COUNT) - 1;
	constexpr size_t k_FrontRow = 9;
	std::vector<Command> parading;
	for (size_t i = 0; i < k_Species; ++i)
	{
		const auto row = i < k_FrontRow ? 0 : 1;
		const auto inRow = row == 0 ? i : i - k_FrontRow;
		const auto count = row == 0 ? k_FrontRow : k_Species - k_FrontRow;
		const glm::vec2 offset {RowX(inRow, count, 40.0f), 60.0f + (static_cast<float>(row) * 50.0f)};
		auto creature = Posed(static_cast<CreatureType>(i + 1), offset);
		parade.push_back(creature);
		parading.push_back(Go(Kind::WalkTo, i, offset - glm::vec2(0.0f, 60.0f), i == 0 ? 3.0f : 0.0f, false));
	}
	for (size_t i = 0; i < k_Species; ++i)
	{
		parading.push_back(Act(Kind::FaceCamera, i, i == 0 ? 8.0f : 0.0f));
	}
	all.push_back({
	    .id = "growth.species_parade",
	    .name = "All species parade",
	    .facet = Facet::Growth,
	    .description = "One creature of every species, all at size 1, in two rows: cow, tiger, leopard, wolf, lion, "
	                   "horse, tortoise, zebra and brown bear in front; polar bear, sheep, chimp, ogre, mandrill, rhino, "
	                   "gorilla and giant ape behind.",
	    .expected = "After a moment they all walk forward together, then turn to face the camera, each "
	                "with its own walk.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -10.0f}}},
	    .creatures = parade,
	    .commands = parading,
	});
}

void AddAppearance(std::vector<Scenario>& all)
{
	constexpr std::array<CreatureType, 3> k_Species {CreatureType::GiantApe, CreatureType::Mandrill, CreatureType::Horse};
	constexpr std::array<float, 3> k_Alignments {-1.0f, 0.0f, 1.0f};
	constexpr std::array<std::string_view, 9> k_Labels {"evil ape",      "neutral ape",      "good ape",
	                                                    "evil mandrill", "neutral mandrill", "good mandrill",
	                                                    "evil horse",    "neutral horse",    "good horse"};
	std::vector<CreatureSetup> skins;
	for (size_t row = 0; row < k_Species.size(); ++row)
	{
		for (size_t column = 0; column < k_Alignments.size(); ++column)
		{
			auto creature = Content(k_Species.at(row), {RowX(column, 3, 45.0f), static_cast<float>(row) * 45.0f}, 0.0f,
			                        k_Labels.at((row * 3) + column));
			creature.alignment = k_Alignments.at(column);
			creature.pauseMind = true;
			skins.push_back(creature);
		}
	}
	all.push_back({
	    .id = "appearance.alignment_skins",
	    .name = "Alignment skins and hair",
	    .facet = Facet::Appearance,
	    .description = "Giant apes, mandrills and horses, a row of each from front to back, evil, neutral and good from "
	                   "left to right.",
	    .expected = "Each skin blends towards its evil or good skin with the alignment, the bodies follow it, and the "
	                "hair of the hairy species is drawn over them (the spawner's Show hair turns it off).",
	    .framing = {.shot = Shot::Overview},
	    .creatures = skins,
	});

	auto tattooed = Posed(CreatureType::Tiger, {0.0f, 0.0f}, 30.0f, "tattooed");
	constexpr std::array<glm::u8vec3, 8> k_Colours {{
	    {220, 40, 40},
	    {40, 200, 60},
	    {50, 90, 230},
	    {240, 210, 40},
	    {200, 60, 220},
	    {40, 210, 220},
	    {250, 140, 30},
	    {245, 245, 245},
	}};
	for (size_t site = 0; site < creature_tattoo::k_SlotCount; ++site)
	{
		tattooed.tattoos.push_back({.design = static_cast<uint8_t>(((site * 5) + 1) % creature_tattoo::k_DesignCount),
		                            .site = static_cast<uint8_t>(site),
		                            .colour = k_Colours.at(site)});
	}
	tattooed.wounds = {
	    {.u = 60, .v = 90, .skin = 0, .type = 3, .column = 1},  {.u = 150, .v = 120, .skin = 0, .type = 4, .column = 3},
	    {.u = 200, .v = 70, .skin = 1, .type = 5, .column = 5}, {.u = 90, .v = 180, .skin = 1, .type = 1, .column = 2},
	    {.u = 120, .v = 40, .skin = 2, .type = 3, .column = 6},
	};
	for (uint8_t v = 100; v < 112; ++v)
	{
		tattooed.blood.push_back({.u = 128, .v = v, .skin = 0});
	}
	auto plain = Posed(CreatureType::Tiger, {25.0f, 0.0f}, 330.0f, "unmarked");
	all.push_back({
	    .id = "appearance.tattoos_wounds",
	    .name = "Tattoos and wounds showcase",
	    .facet = Facet::Appearance,
	    .description = "A tiger with a tattoo in every slot, each of a different design and colour, five wounds and "
	                   "burns and a trail of blood, beside an unmarked tiger to compare.",
	    .expected = "The designs sit on the species' tattoo sites in their colours; the wounds start fresh and red and "
	                "heal over time to old scars, the blood running down the skin and fading.",
	    .framing = {.shot = Shot::Follow, .distance = 0.9f},
	    .creatures = {tattooed, plain},
	});

	std::vector<Command> passing {Act(Kind::SitDown, 0, 0.5f)};
	const std::array<glm::vec2, 2> across {glm::vec2 {-45.0f, -30.0f}, glm::vec2 {45.0f, -30.0f}};
	WalkRound(passing, 1, across);
	// Sitting down again once it has got up by itself, so it stays to be looked at
	passing.push_back(Act(Kind::SitDown, 0, 0.0f));
	all.push_back({
	    .id = "appearance.eyes",
	    .name = "Eyes and blinking close-up",
	    .facet = Facet::Appearance,
	    .description = "A tiger, close up, sits facing the camera while a cow walks back and forth in front of it, "
	                   "just behind the camera.",
	    .expected = "It blinks every few seconds; its eyes and head turn to follow the cow from side to side, the eyes "
	                "leading the head.",
	    .framing = {.shot = Shot::Head, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "watcher"),
	                  Posed(CreatureType::Cow, across.at(1), 90.0f, "walker")},
	    .commands = passing,
	    .repeatFrom = 1,
	});
}

/// Dead animals lying until they go in their puffs of smoke
void AddDeadBodies(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "animals.dead_body_smoke",
	    .name = "Dead cows go in puffs of smoke",
	    .facet = Facet::Needs,
	    .description = "Three cows side by side, each facing its own way as it was made, set alight. Each burns to death, "
	                   "lies dead for 600 turns, then goes.",
	    .expected = "As each goes a grey puff of smoke rises where it lay, half its height out from its middle along the way "
	                "it faces, not over its back, and drifts off and fades over three seconds. Run with --frames 4300: "
	                "the shot comes about a second after the first cow goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 15.0f}, {20.0f, 35.0f}}, .distance = 0.4f},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {-12.0f, 25.0f}},
	                {.type = AnimalInfo::Cow, .offset = {0.0f, 25.0f}},
	                {.type = AnimalInfo::Cow, .offset = {12.0f, 25.0f}}},
	    // Burning takes their lives, as a fire does in the game; with no life left at the start they would only go on
	    // grazing, since nothing takes a cow's death from its life alone
	    .fixtures = {.fires = {{.what = size_t {0}}, {.what = size_t {1}}, {.what = size_t {2}}}},
	});
}

void AddLight(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "light.shadows",
	    .name = "Shadows through the day",
	    .facet = Facet::Light,
	    .description = "A tiger, a cow and a giant ape stand still while the hour jumps from morning to midday, the "
	                   "afternoon and dusk, the clock otherwise standing still.",
	    .expected = "Their shadows lie long to one side in the morning, short under them at midday and long to the "
	                "other side in the afternoon, fading as the sun goes down.",
	    .environment = {.hour = 9.0f, .clockRuns = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {40.0f, 0.0f}), Posed(CreatureType::Cow, {0.0f, 0.0f}),
	                  Posed(CreatureType::GiantApe, {-45.0f, 0.0f})},
	    .commands = {AtHour(9.0f, 0.0f), AtHour(12.0f, 5.0f), AtHour(15.0f, 5.0f), AtHour(17.5f, 5.0f), AtHour(20.0f, 5.0f),
	                 AtHour(9.0f, 5.0f)},
	    .repeatFrom = 1,
	});

	// Villagers' gaits by their whole speed: four men and four women walking side by side, each on a limit of its
	// sex's walk or run or just over it
	{
		constexpr std::array<uint16_t, 4> k_MenSpeeds {1966, 1967, 2621, 2622};
		constexpr std::array<uint16_t, 4> k_WomenSpeeds {1311, 1312, 1966, 1967};
		std::vector<ObjectSetup> walkers;
		for (size_t i = 0; i < 4; ++i)
		{
			const float x = -70.0f + (20.0f * static_cast<float>(i));
			walkers.push_back({.type = VillagerInfo::NorseShepherdMale,
			                   .offset = {x, 0.0f},
			                   .walkTo = glm::vec2 {x, 60.0f},
			                   .scriptSpeed = k_MenSpeeds.at(i)});
		}
		for (size_t i = 0; i < 4; ++i)
		{
			const float x = 10.0f + (20.0f * static_cast<float>(i));
			walkers.push_back({.type = VillagerInfo::NorseHousewifeFemale,
			                   .offset = {x, 0.0f},
			                   .walkTo = glm::vec2 {x, 60.0f},
			                   .scriptSpeed = k_WomenSpeeds.at(i)});
		}
		all.push_back({
		    .id = "villagers.gait_limits",
		    .name = "Villagers walk or run by their speed",
		    .facet = Facet::Movement,
		    .description = "Four Norse men, from the left, at whole speeds 1966, 1967, 2621 and 2622, and four Norse women "
		                   "at 1311, 1312, 1966 and 1967, each the script's own, its speed set by a script's SET_PROPERTY "
		                   "of SPEED, walk 60 m straight ahead side by side from a second in.",
		    .expected = "Each plays the clip of its speed: a man walks up to 1966 and runs up to 2621, a woman walks up to "
		                "1311 and runs up to 1966, both limits included, so the walkers on a limit play the slower clip "
		                "and those one over it the faster. The log gives each one's speed as its walk starts.",
		    .framing = {.shot = Shot::Overview, .include = {{-75.0f, 0.0f}, {75.0f, 60.0f}}},
		    .objects = std::move(walkers),
		});
	}

	all.push_back({
	    .id = "villagers.high_detail_eyes",
	    .name = "A villager's eyes in high detail",
	    .facet = Facet::Light,
	    .description = "A Norse shepherd, the opening's father, stands three units in front of a camera looking at his "
	                   "face by day; a script's wide screen comes up and he is drawn in high detail, as the opening "
	                   "draws its family, for twenty seconds, then back to normal.",
	    .expected = "In high detail his body is the opening's father and his eyes open and blink every one to five "
	                "seconds, each eye taking the land's light where he stands with its own shade from the default "
	                "sun, as the original's unlit eyes do.",
	    .environment = {.hour = 12.0f, .clockRuns = false},
	    .framing = {.shot = Shot::Placed, .eye = {3.0f, 2.0f, 0.0f}, .look = {0.0f, 1.2f, 0.0f}},
	    .objects = {{.type = VillagerInfo::NorseShepherdMale, .offset = {0.0f, 0.0f}, .yawDegrees = 180.0f}},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 0.0f, .value = 1},
	                 {.kind = Kind::HighDetail, .delaySeconds = 0.5f, .value = 1, .object = 0},
	                 {.kind = Kind::HighDetail, .delaySeconds = 20.0f, .value = 0, .object = 0}},
	});

	all.push_back({
	    .id = "villagers.high_meshes_at_top_detail",
	    .name = "Villagers, an animal and a bird with their high meshes at the top detail",
	    .facet = Facet::Light,
	    .description = "At the top graphics detail (level 6), by day, a Norse shepherd, a Norse housewife, a sheep and a "
	                   "dead dove stand in a row three units in front of the camera.",
	    .expected = "Each is drawn with its high mesh, finer than the std one the lower levels draw (the shepherd's 263 "
	                "vertices against 112), posed and lit as at every level; its reflection, shadow and the hand's pick "
	                "keep the std mesh.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false, .detailLevel = 6},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 2.0f, -6.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::NorseShepherdMale, .offset = {-1.5f, 0.0f}, .yawDegrees = 180.0f},
	                {.type = VillagerInfo::NorseHousewifeFemale, .offset = {-0.5f, 0.0f}, .yawDegrees = 180.0f},
	                {.type = AnimalInfo::Sheep, .offset = {0.8f, 0.0f}},
	                {.type = AnimalInfo::Dove, .offset = {1.8f, 0.0f}, .life = 0.0f}},
	});

	all.push_back({
	    .id = "light.reflections",
	    .name = "Reflections in the lake",
	    .facet = Facet::Light,
	    .description = "A tiger, a horse and a giant ape stand in the far shallows of the testbed's lake, seen across "
	                   "the water from the south.",
	    .expected = "Only the tiger, the one the player leads, shows upside down in the water in front of it, moving "
	                "as it moves, bluish and unlit, with no eyes; the horse and the ape are not mirrored.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {Posed(CreatureType::Tiger, {k_Lake.x + 30.0f, k_FarShallows}),
	                  Posed(CreatureType::Horse, {k_Lake.x, k_FarShallows}),
	                  Posed(CreatureType::GiantApe, {k_Lake.x - 30.0f, k_FarShallows})},
	});

	auto rival = Posed(CreatureType::Horse, {k_Lake.x - 25.0f, k_FarShallows});
	rival.owner = PlayerNames::PLAYER_TWO;
	all.push_back({
	    .id = "light.creature_reflection",
	    .name = "The player's creature mirrored in the lake",
	    .facet = Facet::Light,
	    .description = "The player's tiger and another player's horse stand in the far shallows of the testbed's lake, "
	                   "seen across the water from the south. After four seconds the tiger walks up the bank behind it.",
	    .expected = "The tiger shows upside down in the water in front of it, in its pose, bluish and unlit, with no "
	                "eyes; the horse is not mirrored. Once the tiger is up the bank, six units above the sea, its "
	                "image is gone.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {Posed(CreatureType::Tiger, {k_Lake.x + 25.0f, k_FarShallows}), rival},
	    .commands = {Go(Kind::WalkTo, 0, {k_Lake.x + 25.0f, k_FarShallows + 25.0f}, 4.0f)},
	});
}

void AddMovement(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "movement.course",
	    .name = "Walk, run, turn and step course",
	    .facet = Facet::Movement,
	    .description = "A tiger goes round a square course: walking one side, running the next, turning on the spot to "
	                   "face the middle, walking, running, then a short step off.",
	    .expected = "Its legs blend between standing, walking and running with its speed, it turns on the spot without "
	                "sliding its feet, and the short last leg is a step rather than a walk; the route shows over the "
	                "land in the spawner.",
	    .framing = {.shot = Shot::Overview, .include = {{70.0f, 70.0f}, {-70.0f, -70.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {60.0f, -60.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {60.0f, 60.0f}, 1.0f), Go(Kind::RunTo, 0, {-60.0f, 60.0f}, 0.5f),
	                 Go(Kind::TurnToFace, 0, {0.0f, 0.0f}, 0.5f), Go(Kind::WalkTo, 0, {-60.0f, -60.0f}, 1.0f),
	                 Go(Kind::RunTo, 0, {50.0f, -60.0f}, 0.5f), Go(Kind::WalkTo, 0, {60.0f, -60.0f}, 0.5f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "movement.lake",
	    .name = "Round the lake and through its shallows",
	    .facet = Facet::Movement,
	    .description = "A tiger on the west bank of the testbed's lake walks to the east bank, then down into the near "
	                   "shallows and back up onto the plain.",
	    .expected = "It never sets foot on the open water, which is too deep: its route bends round the lake on the bank "
	                "or wades its shallows; it walks into the shallows and stands with its feet at the water, then "
	                "climbs the bank.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {Posed(CreatureType::Tiger, {k_Lake.x - k_LakeHalf.x - 50.0f, k_Lake.y}, 90.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {k_Lake.x + k_LakeHalf.x + 50.0f, k_Lake.y}, 1.0f),
	                 Go(Kind::WalkTo, 0, {k_Lake.x + 20.0f, k_NearShallows}, 1.0f),
	                 Go(Kind::WalkTo, 0, {k_Lake.x - k_LakeHalf.x - 50.0f, k_Lake.y}, 1.0f)},
	    .repeatFrom = 0,
	});

	std::vector<ObjectSetup> wall;
	for (int i = -3; i <= 3; ++i)
	{
		wall.push_back({.type = TreeInfo::Conifer, .offset = {static_cast<float>(i) * 12.0f, 0.0f}, .scale = 1.5f});
	}
	wall.push_back({.type = FeatureInfo::FatPilarChalk, .offset = {-60.0f, 0.0f}});
	all.push_back({
	    .id = "movement.route_obstacles",
	    .name = "Route around obstacles",
	    .facet = Facet::Movement,
	    .description = "A small tiger walks back and forth past a row of tall conifers and a pillar of rock, with a cow "
	                   "standing still beyond them in its way.",
	    .expected = "Its route bends round the end of the row of trees, which are taller than it, and round the standing "
	                "cow rather than through them; the route shows over the land in the spawner.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 120.0f}, {0.0f, -100.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {0.0f, -100.0f},
	                                 .facingDegrees = 180.0f,
	                                 .size = 0.6f,
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true},
	                  CreatureSetup {.label = "in the way",
	                                 .species = CreatureType::Cow,
	                                 .offset = {0.0f, 60.0f},
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true}},
	    .objects = wall,
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 120.0f}, 1.5f), Go(Kind::WalkTo, 0, {0.0f, -100.0f}, 1.5f)},
	    .repeatFrom = 0,
	});

	// What a creature's route keeps clear of: a flat rock, a tall boulder and a pot standing in its way, one after another
	all.push_back({
	    .id = "movement.route_round_rocks",
	    .name = "Route round rocks and a pot",
	    .facet = Facet::Movement,
	    .description = "A tiger walks back and forth along a line with a flat rock, a tall boulder and a pot standing on "
	                   "it, one after another.",
	    .expected = "Its route bends round each of them rather than through them, and gives the tall boulder more room "
	                "than the flat rock beside its own body; the route shows over the land in the spawner.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 90.0f}, {0.0f, -90.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {0.0f, -90.0f},
	                                 .facingDegrees = 180.0f,
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -40.0f}},
	                {.type = MobileStaticInfo::Boulder2Chalk, .offset = {0.0f, 0.0f}},
	                {.type = MobileObjectInfo::EgyptPotA, .offset = {0.0f, 40.0f}}},
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 1.5f), Go(Kind::WalkTo, 0, {0.0f, -90.0f}, 1.5f)},
	    .repeatFrom = 0,
	});

	std::vector<ObjectSetup> copse;
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			const auto type = (x + y) % 2 == 0 ? TreeInfo::Bush : TreeInfo::Birch;
			copse.push_back({.type = type,
			                 .offset = {static_cast<float>(x) * 9.0f, static_cast<float>(y) * 9.0f},
			                 .yawDegrees = static_cast<float>((x * 40) + (y * 70))});
		}
	}
	all.push_back({
	    .id = "movement.trampling",
	    .name = "Walking through small trees",
	    .facet = Facet::Movement,
	    .description = "A full grown giant ape walks back and forth through a copse of bushes and birches much shorter "
	                   "than it.",
	    .expected = "It walks straight through the copse rather than round it, as big creatures walk through trees under "
	                "one and a half times their height; the trees stay standing.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 90.0f}, {0.0f, -90.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::GiantApe,
	                                 .offset = {0.0f, -90.0f},
	                                 .facingDegrees = 180.0f,
	                                 .size = 2.0f,
	                                 .needs = Content(),
	                                 .hold = true,
	                                 .pauseMind = true}},
	    .objects = copse,
	    .commands = {Go(Kind::WalkTo, 0, {0.0f, 90.0f}, 1.0f), Go(Kind::WalkTo, 0, {0.0f, -90.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	const auto loop = Circle({0.0f, 0.0f}, 70.0f, 6);
	std::vector<Command> chase {{.kind = Kind::Follow, .creature = 1, .delaySeconds = 1.0f, .value = 0}};
	WalkRound(chase, 0, loop);
	chase.push_back(Go(Kind::FleeFrom, 2, {0.0f, 0.0f}, 0.0f, false));
	chase.push_back(Go(Kind::WalkTo, 2, {0.0f, 20.0f}, 3.0f, true));
	all.push_back({
	    .id = "movement.follow_flee",
	    .name = "Follow and flee",
	    .facet = Facet::Movement,
	    .description = "A cow walks round a circle with a tiger following it; a wolf in the middle keeps running away "
	                   "and coming back.",
	    .expected = "The tiger keeps within a short distance of the cow, catching up when it falls behind; the wolf runs "
	                "off from the middle, then walks back.",
	    .framing = {.shot = Shot::Overview, .include = {{80.0f, 80.0f}, {-80.0f, -80.0f}}},
	    .creatures = {Posed(CreatureType::Cow, loop.back(), 0.0f, "leader"),
	                  Posed(CreatureType::Tiger, loop.back() + glm::vec2(0.0f, -30.0f), 0.0f, "follower"),
	                  Posed(CreatureType::Wolf, {0.0f, 20.0f}, 0.0f, "flees")},
	    .commands = chase,
	    .repeatFrom = 1,
	});
}

void AddFootprints(std::vector<Scenario>& all)
{
	constexpr std::array<CreatureType, 3> k_Walkers {CreatureType::Tiger, CreatureType::Cow, CreatureType::GiantApe};
	std::vector<CreatureSetup> walkers;
	std::vector<Command> trails;
	for (size_t i = 0; i < k_Walkers.size(); ++i)
	{
		const auto y = 30.0f - (static_cast<float>(i) * 30.0f);
		walkers.push_back(Posed(k_Walkers.at(i), {110.0f, y}, 90.0f));
		trails.push_back(Go(Kind::WalkTo, i, {-110.0f, y}, i == 0 ? 1.0f : 0.0f, false));
	}
	for (size_t i = 0; i < k_Walkers.size(); ++i)
	{
		const auto y = 30.0f - (static_cast<float>(i) * 30.0f);
		trails.push_back(Go(Kind::WalkTo, i, {110.0f, y}, 0.0f, true));
	}
	all.push_back({
	    .id = "footprints.trail",
	    .name = "Footprints trail per species",
	    .facet = Facet::Footprints,
	    .description = "A tiger, a cow and a giant ape walk side by side across the testbed and back.",
	    .expected = "Each leaves its own prints where its feet fall: paws for the tiger, hooves for the cow and broad "
	                "feet for the ape, fading after a while.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = walkers,
	    .commands = trails,
	    .repeatFrom = 0,
	});

	std::vector<Command> circling;
	WalkRound(circling, 0, Circle({0.0f, 0.0f}, 50.0f, 8));
	all.push_back({
	    .id = "footprints.april_fools",
	    .name = "1 April smiley footprints",
	    .facet = Facet::Footprints,
	    .description = "As on the first of April, whatever the date: a tiger walks round in a circle.",
	    .expected = "Its footprints are smiley faces.",
	    .environment = {.aprilFools = true},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {50.0f, 0.0f}, 180.0f)},
	    .commands = circling,
	    .repeatFrom = 0,
	});
}

void AddAudio(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "audio.surfaces",
	    .name = "Footsteps on grass and in the shallows",
	    .facet = Facet::Audio,
	    .description = "A tiger walks from the grass down the bank into the shallows of the testbed's lake and back. "
	                   "Select it in the spawner to see its sound log.",
	    .expected = "Its footsteps sound of grass on the land and of splashing in the water; the log's keys show the "
	                "surface changing.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Posed(CreatureType::Tiger, {k_Lake.x, 80.0f}, 180.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {k_Lake.x, k_NearShallows}, 1.0f), Go(Kind::WalkTo, 0, {k_Lake.x, 80.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "audio.snow",
	    .name = "Footsteps in snow",
	    .facet = Facet::Audio,
	    .description = "Snow falls over the whole island while a tiger walks back and forth. Select it in the spawner to "
	                   "see its sound log.",
	    .expected = "Once the snow lies deep enough, its footsteps change from grass to snow.",
	    .environment = {.weather = Weather::Snow},
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Posed(CreatureType::Tiger, {-50.0f, 0.0f}, 270.0f)},
	    .commands = {Go(Kind::WalkTo, 0, {50.0f, 0.0f}, 1.0f), Go(Kind::WalkTo, 0, {-50.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	constexpr std::array<size_t, 5> k_Voiced {animations::k_Angry, animations::k_Happy, animations::k_Hungry,
	                                          animations::k_Taunt, animations::k_Frightened};
	std::vector<Command> voices;
	for (const auto action : k_Voiced)
	{
		for (size_t creature = 0; creature < 3; ++creature)
		{
			voices.push_back(Play(Kind::PlayAction, creature, action, 1.0f, true));
		}
	}
	auto small = Content(CreatureType::Tiger, {45.0f, 0.0f}, 0.0f, "small tiger");
	small.size = 0.5f;
	small.pauseMind = true;
	auto big = Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "big tiger");
	big.size = 2.0f;
	big.pauseMind = true;
	auto cow = Content(CreatureType::Cow, {-50.0f, 0.0f}, 0.0f, "cow");
	cow.pauseMind = true;
	all.push_back({
	    .id = "audio.voices",
	    .name = "Voices by species and size",
	    .facet = Facet::Audio,
	    .description = "A small tiger, a big tiger and a cow take turns to be angry, happy, hungry, taunting and "
	                   "frightened. Select one in the spawner to see its sound log.",
	    .expected = "Each plays its voice for the action from its own bank, the small tiger higher than the big one and "
	                "the cow its own; the log's size key differs between the tigers.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {small, big, cow},
	    .commands = voices,
	    .repeatFrom = 0,
	});

	// Two teleport stones the villagers jump between, the first jumps heard from the land, the later ones while the
	// camera is in the temple
	constexpr glm::vec2 k_StoneA {-40.0f, 20.0f};
	constexpr glm::vec2 k_StoneB {40.0f, 70.0f};
	all.push_back({
	    .id = "audio.temple_quiet",
	    .name = "The land's sound effects while the camera is in the temple",
	    .facet = Facet::Audio,
	    .description = "Two teleport stones; a villager beside the first walks to beside the second, through the stones, "
	                   "and another is put down on the first. Then the temple opens on the creature's room, and the "
	                   "villager walks back. The sound effect trace (OPENBLACK_SFX_TRACE=1) gives the sounds started and "
	                   "those not heard.",
	    .expected = "The first jumps sound where they leave and arrive. In the temple the creature's room's water and "
	                "fire are heard, but the jumps on the land are not.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB}, .distance = 0.8f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -40.0f}, 180.0f, "yours")},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_StoneA + glm::vec2(6.0f, 4.0f),
	                 .walkTo = k_StoneB + glm::vec2(6.0f, 4.0f),
	                 .walkAfterSeconds = 5.0f,
	                 .walkRepeatSeconds = 15.0f,
	                 .walkFinal = VillagerStates::ArrivesHome},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_StoneA + glm::vec2(-8.0f, -4.0f),
	                 .dropOnStoneSeconds = 16.0f}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_StoneA,
	                  .handOffset = k_StoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_StoneB,
	                  .handOffset = k_StoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	    .commands = {{.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 12.0f}},
	});
}

void AddObjects(std::vector<Scenario>& all)
{
	// Balls all round a tiger facing the camera, south: it reaches for each in turn, looks it over and puts it down,
	// then goes back to the middle and faces south again
	constexpr float k_Reach = 10.0f;
	constexpr float k_Diagonal = 7.0f;
	const std::array<glm::vec2, 6> k_Round {glm::vec2 {0.0f, -k_Reach},          glm::vec2 {-k_Diagonal, -k_Diagonal},
	                                        glm::vec2 {k_Diagonal, -k_Diagonal}, glm::vec2 {-k_Reach, 0.0f},
	                                        glm::vec2 {k_Reach, 0.0f},           glm::vec2 {0.0f, k_Reach}};
	std::vector<ObjectSetup> balls;
	std::vector<Command> reaching;
	for (size_t i = 0; i < k_Round.size(); ++i)
	{
		balls.push_back({.type = MobileObjectInfo::Ball, .offset = k_Round.at(i)});
		reaching.push_back(OnObject(Kind::PickUp, 0, i, 1.0f));
		reaching.push_back(Play(Kind::Examine, 0, creature_object_actions::k_KeepAnimationCount - 1, 0.5f, true));
		reaching.push_back(Act(Kind::PutDown, 0, 0.5f, true));
		reaching.push_back(Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f));
		reaching.push_back(Go(Kind::TurnToFace, 0, {0.0f, -60.0f}, 0.0f));
	}
	all.push_back({
	    .id = "objects.reach",
	    .name = "Picking up, looking over, putting down",
	    .facet = Facet::Objects,
	    .description = "A tiger facing the camera with balls in front of it, to its front left and right, either side "
	                   "and behind it. It picks each up in turn, examines it and puts it down, then goes back to the "
	                   "middle.",
	    .expected = "It reaches for each ball the way it lies, in front, to the side or behind, picking it up with its "
	                "hand; holds it up to look at it; and puts it down gently.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = balls,
	    .commands = reaching,
	    .repeatFrom = 0,
	});

	std::vector<Command> examining {OnObject(Kind::PickUp, 0, 0, 1.0f)};
	for (size_t way = 0; way < creature_object_actions::k_KeepAnimationCount; ++way)
	{
		examining.push_back(Play(Kind::Examine, 0, way, 1.0f, true));
	}
	examining.push_back(Act(Kind::PutDown, 0, 1.0f, true));
	all.push_back({
	    .id = "objects.examine",
	    .name = "Looking a thing over four ways",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a pot in front of it and strokes, shakes, smells and examines it in turn, then "
	                   "puts it down.",
	    .expected = "Each way of looking the pot over plays its own animation with the pot held in the hand.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::EgyptPotA, .offset = {0.0f, -k_Reach}}},
	    .commands = examining,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.throw_lob",
	    .name = "Throwing at a target, lobbing and tossing away",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a ball and throws it at a pillar of rock 60 units east, fetches it and lobs it, "
	                   "fetches it again and tosses it away.",
	    .expected = "The throw flies flat and hard towards the pillar; the lob goes up high and comes down near; the "
	                "toss drops it aside. Each comes to rest where it lands.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 30.0f}, {-30.0f, -30.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {-20.0f, 0.0f}, 270.0f)},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-20.0f, -k_Reach}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {60.0f, 0.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Go(Kind::ThrowAt, 0, {60.0f, 0.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::Lob, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Act(Kind::Discard, 0, 0.5f, true), Go(Kind::WalkTo, 0, {-20.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	auto hungry = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	hungry.needs = {.energy = 0.3f};
	hungry.hold = false;
	all.push_back({
	    .id = "objects.eat_held",
	    .name = "Eating by picking food up",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger with three piles of food about it picks each up and eats it from its hand.",
	    .expected = "It walks up to each, picks it up, brings it to its mouth and eats it; the food is gone and its "
	                "energy rises in the spawner.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungry},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {0.0f, -k_Reach}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -5.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {15.0f, -5.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 1, 1.0f),
	                 Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 2, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	// Giving from the player's hand: the hand takes the thing, held in front of the creature, and gives it
	const std::vector<Command> giving {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                                   {.kind = Kind::HandGive, .creature = 0, .delaySeconds = 1.5f}};
	const HandHold offering {.offset = {0.0f, -k_Reach}, .height = 12.0f};
	// Its mind runs, to take the thing and choose what to do with it
	auto hungryThinking = hungry;
	hungryThinking.pauseMind = false;
	all.push_back({
	    .id = "creature.give_food",
	    .name = "Giving food from the hand",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a pile of food and, held in front of a hungry tiger, gives it to it.",
	    .expected = "The hand turns its palm over to offer the food; the tiger turns to the camera looking curious, reaches "
	                "out, and the hand meets its hand as it takes the food; hungry, it then eats it.",
	    .environment = {.cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungryThinking},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	auto curious = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	curious.hold = false;
	curious.pauseMind = false;
	all.push_back({
	    .id = "creature.give_villager",
	    .name = "Giving a villager from the hand",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a villager and, held in front of a tiger that isn't hungry, gives her to it.",
	    .expected = "The hand turns the villager a quarter round to offer her; the tiger turns to the camera, reaches out and "
	                "takes her from the hand, then does with her what its desires say: looks her over the first time if it "
	                "wants nothing much, eats her if hungry, or throws her at the camera if it wants to play.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {curious},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	all.push_back({
	    .id = "objects.pick_up_pots",
	    .name = "Picking up pots and piles",
	    .facet = Facet::Objects,
	    .description = "A tiger before a food pot, a wood pot, a food pile and a pile of magic food, told to pick each up "
	                   "in turn and put it down.",
	    .expected = "It picks up the food pot and the wood pot, each as any thing of their weight, and puts them down; it "
	                "never lifts the food pile or the magic food, whose kinds stay on the land: told to, it stops at once.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, -k_Reach}, .amount = 400},
	                {.type = PotInfo::WoodPot, .offset = {-15.0f, -5.0f}, .amount = 400},
	                {.type = PotInfo::FoodPile, .offset = {15.0f, -5.0f}, .amount = 2500},
	                {.type = PotInfo::MagicFood, .offset = {0.0f, 15.0f}, .amount = 2500}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::PutDown, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 1, 1.0f),
	                 Act(Kind::PutDown, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 2, 1.0f), OnObject(Kind::PickUp, 0, 3, 2.0f)},
	});

	all.push_back({
	    .id = "objects.eat_what_is_not_food",
	    .name = "Eating what is not food",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger picks up a barrel, worth no food in the game's tables, and eats it from its hand; "
	                   "then it picks up a pile of food and eats that.",
	    .expected = "It eats the barrel all the same: the barrel goes as it swallows, and only once the eating is over "
	                "does it find that it was not food. It is sick on the spot and doesn't want food for twenty seconds, "
	                "and from then on it takes no mobile object for food. The pile it eats as any meal, its hunger going "
	                "down by what the meal is worth.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungry},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {0.0f, -k_Reach}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {15.0f, -5.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 1, 4.0f),
	                 Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	all.push_back({
	    .id = "objects.knock_down_trees",
	    .name = "Knocking down trees",
	    .facet = Facet::Objects,
	    .description = "A tiger walks up to each of three trees in a row and knocks it down.",
	    .expected = "It strikes each tree, which falls; its town's view of it shows in the spawner.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}},
	                {.type = TreeInfo::Beech, .offset = {0.0f, 10.0f}},
	                {.type = TreeInfo::Conifer, .offset = {40.0f, 10.0f}}},
	    .commands = {OnObject(Kind::KnockDown, 0, 0, 1.0f), OnObject(Kind::KnockDown, 0, 1, 1.0f),
	                 OnObject(Kind::KnockDown, 0, 2, 1.0f)},
	});

	all.push_back({
	    .id = "objects.point",
	    .name = "Pointing",
	    .facet = Facet::Objects,
	    .description = "A tiger facing the camera points in front of it, to either side, far off and behind it.",
	    .expected = "It turns as needed and points its arm at each point in turn.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Go(Kind::PointAt, 0, {0.0f, -40.0f}, 1.0f), Go(Kind::PointAt, 0, {-40.0f, 0.0f}, 1.0f),
	                 Go(Kind::PointAt, 0, {40.0f, 0.0f}, 1.0f), Go(Kind::PointAt, 0, {0.0f, 200.0f}, 1.0f),
	                 Go(Kind::TurnToFace, 0, {0.0f, -60.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	// Left to the mind: one desire as strong as it gets, with things about to act on
	struct Mood
	{
		std::string_view id;
		std::string_view name;
		Desire desire;
		std::string_view description;
		std::string_view expected;
	};
	const std::array<Mood, 3> k_Moods {{
	    {"objects.curious", "Curious about things", Desire::Curiosity,
	     "A tiger as curious as it gets, wanting nothing else, among a ball, a pot and a barrel.",
	     "It walks up to something nearby, picks it up and looks it over, then mostly puts it down gently and sometimes "
	     "tosses it away, and goes on to the next."},
	    {"objects.playful", "Playing with things", Desire::Play,
	     "A tiger as playful as it gets, wanting nothing else, among a ball, a pot and a barrel.",
	     "It picks something up and throws it about, a good way off in a random direction, and goes after the next."},
	    {"objects.angry", "Angry with things", Desire::Anger,
	     "A tiger as angry as it gets, wanting nothing else, among a ball, a pot and a barrel, with trees nearby.",
	     "It picks something up, now and then shows its anger first, and hurls it at the nearest tree."},
	}};
	for (const auto& mood : k_Moods)
	{
		auto moody = Content(CreatureType::Tiger, {0.0f, 0.0f});
		moody.desires = OnlyDesire(mood.desire);
		all.push_back({
		    .id = mood.id,
		    .name = mood.name,
		    .facet = Facet::Objects,
		    .description = mood.description,
		    .expected = mood.expected,
		    .framing = {.shot = Shot::Overview, .distance = 1.2f},
		    .creatures = {moody},
		    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-15.0f, -10.0f}},
		                {.type = MobileObjectInfo::EgyptPotA, .offset = {15.0f, -10.0f}},
		                {.type = MobileObjectInfo::EgyptBarrel, .offset = {0.0f, 15.0f}},
		                {.type = TreeInfo::Oak, .offset = {-50.0f, 30.0f}},
		                {.type = TreeInfo::Birch, .offset = {50.0f, 30.0f}}},
		});
	}
}

void AddHand(std::vector<Scenario>& all)
{
	using creature_feedback::BodyPart;
	// Every part of the body, the head first
	std::vector<Command> strokes;
	for (size_t part = 0; part < creature_feedback::k_BodyPartCount; ++part)
	{
		strokes.push_back(Stroke(0, static_cast<BodyPart>(part), 1.0f));
	}
	strokes.push_back(Act(Kind::HandLetGo, 0, 1.0f, true));
	all.push_back({
	    .id = "hand.stroke",
	    .name = "Stroking each part of the body",
	    .facet = Facet::Hand,
	    .description = "The hand rests on a tiger facing the camera and strokes its head, armpits, belly, groin, feet and "
	                   "hands in turn, then lets go.",
	    .expected = "Each stroke plays the pleased animation for its part, mirrored for the left side, and its face; the "
	                "panel's reward climbs a tenth a stroke to Good Boy! 90%, and letting go warms it to the player.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = strokes,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "hand.slap",
	    .name = "Slapping gently and hard, high and low",
	    .facet = Facet::Hand,
	    .description = "The hand slaps a tiger facing the camera on the head, the waist and the feet, gently and then "
	                   "hard, from either side, then lets go.",
	    .expected = "Each slap plays its reeling animation for its height, the gentle ones softer, the sweeps to the "
	                "right mirrored; the panel's reward falls to Bad Boy! 100%, and letting go cools it to the player.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Slap(0, 0.85f, true, false, 1.0f), Slap(0, 0.55f, true, true, 1.0f), Slap(0, 0.2f, true, false, 1.0f),
	                 Slap(0, 0.85f, false, true, 1.0f), Slap(0, 0.55f, false, false, 1.0f), Slap(0, 0.2f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "hand.young_creature_stroked_and_slapped",
	    .name = "A creature too young to learn, slapped and stroked",
	    .facet = Facet::Hand,
	    .description = "A tiger at its first stage of growing up is slapped twice and let go of, then stroked three times "
	                   "and let go of.",
	    .expected = "Too young to learn from it, the slap only makes it angrier and more afraid, and the stroke more playful, "
	                "showy and kind; one time in two each leaves it something to do once it is free: frightened (running "
	                "from the player one time in three) after the slap, a happy action after the stroke.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    // its mind running, so that it does what the feedback leaves it to do
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {{.kind = Kind::SetPhase, .creature = 0, .delaySeconds = 0.5f, .value = 1},
	                 Slap(0, 0.85f, false, true, 1.0f),
	                 Slap(0, 0.55f, false, false, 1.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true),
	                 Stroke(0, BodyPart::Head, 4.0f),
	                 Stroke(0, BodyPart::Belly, 1.0f),
	                 Stroke(0, BodyPart::RightHand, 1.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true),
	                 Act(Kind::Stop, 0, 8.0f)},
	});

	// The lesson from a stroke: the mind, not the scenario, does what is stroked, so that it remembers doing it. Kept
	// free (not held), since holding would put its desires back every frame and undo the lesson
	{
		auto curious = CreatureSetup {.species = CreatureType::Tiger, .desires = OnlyDesire(Desire::Curiosity)};
		const auto strokeNow = [](float delay) {
			return Command {.kind = Kind::HandStroke,
			                .creature = 0,
			                .delaySeconds = delay,
			                .waitUntilFree = false,
			                .bodyPart = static_cast<size_t>(BodyPart::Head)};
		};
		all.push_back({
		    .id = "hand.stroked_holding_thing",
		    .name = "Stroked while holding a ball",
		    .facet = Facet::Hand,
		    .description = "A grown tiger whose only desire is curiosity, its own planner choosing what to do, with a ball "
		                   "before it. Five seconds in, while it holds the ball it picked up to examine, the hand strokes "
		                   "its head and lets go at once.",
		    .expected = "The tiger picks the ball up to look at it. The stroke praises what it was doing, the newest thing "
		                "it remembers: since the ball is held, the game's own lesson is that it should eat what it holds, "
		                "for an action whose row allows eating when stroked, whatever the thing is. The log says \"learns "
		                "to eat what it holds\" when it does.",
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {curious},
		    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, 8.0f}}},
		    .commands = {{.kind = Kind::IdlePolicy, .delaySeconds = 0.0f, .value = 0},
		                 strokeNow(5.0f),
		                 Act(Kind::HandLetGo, 0, 0.3f, false)},
		});

		auto fleeing = CreatureSetup {.species = CreatureType::Tiger, .desires = OnlyDesire(Desire::RunAwayFromPlayer)};
		const auto desire = [](Desire which, float amount, float delay) {
			return Command {.kind = Kind::SetDesire,
			                .creature = 0,
			                .delaySeconds = delay,
			                .value = static_cast<size_t>(which),
			                .amount = amount};
		};
		all.push_back({
		    .id = "hand.stroked_after_fleeing",
		    .name = "Stroked after running from the player",
		    .facet = Facet::Hand,
		    .description = "A grown tiger, its own planner choosing what to do, is slapped, so that it thinks less of the "
		                   "player; then its desire to run from the player is made its strongest and its fear and anger "
		                   "nothing. A second after it stops running, the hand strokes its head and lets go at once.",
		    .expected = "The tiger runs away from the player (from the camera's point). The stroke comes after the run, the "
		                "newest thing it remembers, and forgives it: its wish to run from the player is made the least "
		                "of its desires and what set it off counts for nothing. The log says \"is forgiven for running "
		                "away\" when it does.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {fleeing},
		    .commands = {{.kind = Kind::IdlePolicy, .delaySeconds = 0.0f, .value = 0},
		                 Slap(0, 0.55f, true, false, 1.0f),
		                 Act(Kind::HandLetGo, 0, 0.3f, true),
		                 desire(Desire::RunAwayFromPlayer, 1.0f, 0.5f),
		                 desire(Desire::Fear, 0.0f, 0.0f),
		                 desire(Desire::Anger, 0.0f, 0.0f),
		                 Stroke(0, BodyPart::Head, 1.0f),
		                 Act(Kind::HandLetGo, 0, 0.3f, false)},
		});
	}

	auto worn = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	worn.needs = {.energy = 0.595f, .exhaustion = 0.325f, .life = 0.745f};
	all.push_back({
	    .id = "hand.status_panel",
	    .name = "The creature's status panel",
	    .facet = Facet::Hand,
	    .description = "The hand on a tiger whose needs are held at 25% damage, 40% hunger and 32% tiredness strokes it "
	                   "three times, slaps it once gently and once hard, and lets go.",
	    .expected = "While the hand is on it the panel at the left shows Damage 25%, Hunger 40% and Tiredness 32% in "
	                "yellow bars, and the reward going from No Reward 0% to Good Boy! 30% in green, then down past the "
	                "middle to Bad Boy! in red. Once the hand has let go the panel is gone, as in the original: the hand "
	                "only over the creature shows none.",
	    .framing = {.shot = Shot::Follow, .distance = 1.0f},
	    .creatures = {worn},
	    .commands = {Stroke(0, BodyPart::Head, 1.0f), Stroke(0, BodyPart::Belly, 1.0f), Stroke(0, BodyPart::RightHand, 1.0f),
	                 Slap(0, 0.85f, true, false, 2.0f), Slap(0, 0.55f, false, true, 2.0f), Act(Kind::HandLetGo, 0, 3.0f, true)},
	    .repeatFrom = 0,
	});
}

void AddLeash(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "leash.types",
	    .name = "Led on each leash",
	    .facet = Facet::Leash,
	    .description = "A tiger is put on the learning leash, then the aggression leash, then the compassion leash, a "
	                   "while each, and the leash is taken off. Move the hand about to lead it.",
	    .expected = "A rope runs from the hand to its collar, coloured for each leash; on the aggression leash it wants "
	                "to be angry, on the compassion leash to be kind, and on the learning leash it watches the player.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Leash(0, LeashType::Evil, 10.0f), Leash(0, LeashType::Good, 10.0f),
	                 Act(Kind::TakeOffLeash, 0, 10.0f), Act(Kind::Stop, 0, 3.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.pull_to_hand",
	    .name = "Pulled to the hand",
	    .facet = Facet::Leash,
	    .description = "A tiger far off to the north is put on the learning leash. Put the hand on the land near the "
	                   "camera, well away from it.",
	    .expected = "Once the rope is pulled taut it stops what it is doing and walks to the hand, then its mind takes "
	                "over again near the hand.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 150.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f)},
	});

	all.push_back({
	    .id = "leash.tied",
	    .name = "Tied to a tree and a rock",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a tree and told to walk far off, untied back to the "
	                   "hand, then tied to a pillar of rock and told to walk far off again.",
	    .expected = "The rope runs from what it is tied to, and it is kept within the rope's length of it however far "
	                "it is told to walk; untied, the rope goes back to the hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, 60.0f}, {120.0f, 60.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Go(Kind::WalkTo, 0, {-120.0f, 60.0f}, 1.0f, false), Act(Kind::UntieLeash, 0, 8.0f),
	                 OnObject(Kind::TieLeash, 0, 1, 2.0f, false), Go(Kind::WalkTo, 0, {120.0f, 60.0f}, 1.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 8.0f)},
	    .repeatFrom = 0,
	});

	// A tiger on the learning leash, the rock pillar of leash.tied (which the hand cannot lift), and the camera looking
	// steeply down at the pillar's foot from the south, so that the pointer in the middle of the screen puts the hand on
	// the pillar and the land behind the hand within the hand's reach of it
	all.push_back({
	    .id = "leash.double_click_tie",
	    .name = "Tied to a rock with a double click",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash, with a pillar of rock to its east. The pointer rests on the "
	                   "pillar's foot; the right button is clicked once, then clicked again as a double click, and the pointer "
	                   "moves away to the bottom left.",
	    .expected = "The double click ties the leash to the pillar, as the original's hand does: the rope runs from the "
	                "pillar, not from the hand, once the hand moves away, and the tiger is sent to it. The click before it "
	                "taps the pillar.",
	    .framing = {.shot = Shot::Placed, .eye = {40.0f, 30.0f, -7.0f}, .look = {40.0f, 1.0f, 8.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerDoubleClick, .delaySeconds = 0.1f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.15f, 0.85f}}},
	});

	all.push_back({
	    .id = "leash.hand",
	    .name = "Leash your creature with the hand",
	    .facet = Facet::Leash,
	    .description = "Your tiger stands to the north. The hand clicks it with the right button, putting the learning "
	                   "leash on. Move the hand about to lead it; press L or shake the hand to take the leash off "
	                   "again, or right click it yourself to put it back on.",
	    .expected = "A rope runs from the hand to its collar; pulled taut, the tiger walks to the hand. The readout says "
	                "\"on\".",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.someone_elses",
	    .name = "Another player's creature refuses the leash",
	    .facet = Facet::Leash,
	    .description = "Your tiger on the left and player two's lion on the right. Your hand taps the lion, then your "
	                   "tiger.",
	    .expected = "The lion is refused, as it belongs to another player, and the log says so; then your tiger is "
	                "leashed. Right clicking the lion yourself is refused the same way.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}),
	                  [] {
		                  auto lion = Content(CreatureType::Lion, {30.0f, 60.0f});
		                  lion.owner = PlayerNames::PLAYER_TWO;
		                  return lion;
	                  }()},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.only_leashable",
	    .name = "Only the leashable one of two",
	    .facet = Facet::Leash,
	    .description = "You have two tigers: the left one is the one you can lead, the right one isn't. The hand taps "
	                   "the right one, then the left one.",
	    .expected = "The right tiger is refused as it isn't the one you can lead; the left one is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.switch",
	    .name = "Switch the leashable creature",
	    .facet = Facet::Leash,
	    .description = "Your left tiger is on the leash. The right tiger is made the one you can lead, then the hand "
	                   "taps the left one and the right one.",
	    .expected = "Made leashable, the right tiger takes over: the left one's leash comes off and tapping it is "
	                "refused, and the right one is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::MakeLeashable, .creature = 1, .delaySeconds = 4.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.spare_creature_removed",
	    .name = "The spare one of two removed",
	    .facet = Facet::Leash,
	    .description = "You have two tigers: the left one is the one you lead, on the leash; the right one isn't. The "
	                   "right tiger is taken out of the game, a command is given to it, then the hand taps the left one.",
	    .expected = "The game empties a player's creature whichever of theirs goes, so as the right tiger goes you are "
	                "left with no creature: the left tiger is no longer the one you can lead, openblack takes its leash "
	                "off, and tapping it is refused. The command to the right tiger is logged as gone and does nothing.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashState, .creature = 0, .delaySeconds = 2.0f},
	                 {.kind = Kind::RemoveCreature, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashState, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashState, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.newest_takes_the_slot",
	    .name = "The newest of two is the one led",
	    .facet = Facet::Leash,
	    .description = "Two tigers are made for you, the left one first, with the testbed's first-creature rule turned "
	                   "off, so that the game's own rule decides which one you lead. The hand taps the left one, then "
	                   "the right one, and each one's leash state is logged after its tap.",
	    .expected = "As in the game, each creature made for you becomes your creature, the newest winning: the left "
	                "tiger is refused as it isn't the one you can lead, and the right one, made last, is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashState, .creature = 0, .delaySeconds = 2.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashState, .creature = 1, .delaySeconds = 2.0f}},
	    .firstCreatureLeads = false,
	});

	all.push_back({
	    .id = "leash.keys",
	    .name = "The leash shortcuts",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows all three leashes. The keys are pressed in turn: L puts the learning leash "
	                   "on, B steps down to the aggression leash, V steps back up to the learning leash and on up to "
	                   "the compassion leash, and L takes it off, however many leashes it knows.",
	    .expected = "The rope goes on, changes colour three times, and comes off; the readout names each leash. "
	                "Pressing L, V and B yourself does the same.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::LeashKey, .delaySeconds = 1.0f, .value = 0},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 2},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.click_or_hold",
	    .name = "Hold to stroke, right click to leash",
	    .facet = Facet::Leash,
	    .description = "The hand holds on to your tiger with the right button, stroking its head and belly, and lets go; "
	                   "then it clicks the tiger, which puts the leash on. Try it yourself: holding the right button on "
	                   "it for a second strokes it, sweeping the held hand across it slaps it, and a quick right click "
	                   "leashes it.",
	    .expected = "The hold strokes it without leashing it; the click then puts the rope on without stroking it.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f),
	                 Stroke(0, creature_feedback::BodyPart::Belly, 2.5f),
	                 Act(Kind::HandLetGo, 0, 2.0f, true),
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.shake",
	    .name = "Shake the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, and a scribble is then drawn with the empty hand, as "
	                   "the game shakes a leash off. Draw it yourself (move the mouse quickly side to side a few times, "
	                   "no button held) to try it.",
	    .expected = "The rope goes on, and comes off once the scribble is recognised; the log says so.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashShake, .creature = 0, .delaySeconds = 4.0f}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.key_off",
	    .name = "L takes the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, then L is pressed.",
	    .expected = "The rope goes on, then comes off with L.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashKey, .delaySeconds = 4.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	// Going inside the citadel takes the leash off; the world is paused inside, so it comes off once the world goes on
	all.push_back({
	    .id = "leash.off_inside_citadel",
	    .name = "Going inside the citadel takes the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, then the temple opens on the creature's room and is "
	                   "left at once.",
	    .expected = "The rope goes on, and is still on as the temple opens; it comes off at the first turn after "
	                "leaving, and the log says it is off.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashState, .creature = 0, .delaySeconds = 3.0f},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashState, .creature = 0, .delaySeconds = 0.0f},
	                 {.kind = Kind::LeaveTemple, .delaySeconds = 0.0f},
	                 {.kind = Kind::LeashState, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.other_gods_creatures",
	    .name = "Slapping other gods' creatures",
	    .facet = Facet::Leash,
	    .description = "Your tiger on the left, a tortoise of player three (as Khazar's is) in the middle and a wolf of "
	                   "player four (as Lethys's is) on the right. The hand slaps the tortoise and the wolf, as it may "
	                   "any god's creature, then tries to leash the tortoise.",
	    .expected = "The tortoise and the wolf are slapped and reel. Leashing the tortoise is refused, as it isn't "
	                "yours, and the log says so.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-40.0f, 60.0f}),
	                  [] {
		                  auto tortoise = Content(CreatureType::Tortoise, {0.0f, 60.0f});
		                  tortoise.owner = PlayerNames::PLAYER_THREE;
		                  return tortoise;
	                  }(),
	                  [] {
		                  auto wolf = Content(CreatureType::Wolf, {40.0f, 60.0f});
		                  wolf.owner = PlayerNames::PLAYER_FOUR;
		                  return wolf;
	                  }()},
	    .commands = {Slap(1, 0.85f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 1, 2.0f, true),
	                 Slap(2, 0.85f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 2, 2.0f, true),
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.home",
	    .name = "Kept at home",
	    .facet = Facet::Leash,
	    .description = "A tiger is kept within 40 units of where it stands, as a young creature is kept near its home, "
	                   "and told to walk 120 units away.",
	    .expected = "Once it strays past the radius it turns back and walks home again.",
	    .framing = {.shot = Shot::Overview, .include = {{120.0f, 0.0f}, {-40.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 40.0f},
	                 Go(Kind::WalkTo, 0, {120.0f, 0.0f}, 1.0f),
	                 Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 6.0f)},
	    .repeatFrom = 1,
	});
}

Command Fight(Kind kind, size_t creature, size_t value, float delay, float chargeMs = 0.0f)
{
	return {.kind = kind, .creature = creature, .delaySeconds = delay, .value = value, .chargeMs = chargeMs};
}

/// A content tiger for a fight, its owner, strength and alignment given
CreatureSetup Fighter(glm::vec2 offset, float facing, std::string_view label, PlayerNames owner, float strength,
                      float alignment)
{
	auto creature = Content(CreatureType::Tiger, offset, facing, label);
	creature.owner = owner;
	creature.strength = strength;
	creature.alignment = alignment;
	return creature;
}

void AddCombat(std::vector<Scenario>& all)
{
	constexpr size_t k_High = 0;
	constexpr size_t k_Mid = 1;
	constexpr size_t k_Low = 2;
	constexpr size_t k_Forward = 0;
	constexpr size_t k_Back = 1;
	const glm::vec2 left {-25.0f, 40.0f};
	const glm::vec2 right {25.0f, 40.0f};
	const auto red = [&](PlayerNames owner, float strength, float alignment) {
		return Fighter(left, 90.0f, "red", owner, strength, alignment);
	};
	const auto blue = [&](PlayerNames owner, float strength, float alignment) {
		return Fighter(right, 270.0f, "blue", owner, strength, alignment);
	};
	const Framing ringside {.shot = Shot::Overview, .include = {{-45.0f, 40.0f}, {45.0f, 40.0f}}, .distance = 0.8f};

	all.push_back({
	    .id = "combat.ai_duel",
	    .name = "Two tigers fight by themselves",
	    .facet = Facet::Combat,
	    .description = "Two somewhat evil tigers, so aggressive fighters, the red one stronger, are told to fight, and both "
	                   "fight by themselves.",
	    .expected = "They walk to their places either side of the arena, face each other and maybe taunt, then duel: "
	                "blows high, in the middle and low with their hit wobbles and reeling, blocks and steps. The panel at "
	                "the top left shows both fight healths falling. The loser faints, and the winner finishes and shows "
	                "off.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	all.push_back({
	    .id = "combat.player",
	    .name = "Directing the player's creature",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger fights a blue tiger that fights by itself. The red one is given blows "
	                   "high, in the middle and low, a step forward, a block and a fully charged high blow in turn, as "
	                   "clicking the blue tiger's head, middle and legs, the ground ahead, and the red tiger itself would. "
	                   "Click them yourself to direct it too.",
	    .expected = "Each order plays at once: the blow chosen lands at the height asked for, stepping in first when "
	                "out of reach; the block holds until the next order. Left alone for 15 seconds, it fights by "
	                "itself.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_High, 8.0f, 400.0f),
	                 Fight(Kind::FightBlow, 0, k_Mid, 2.5f, 400.0f), Fight(Kind::FightBlow, 0, k_Low, 2.5f, 400.0f),
	                 Fight(Kind::FightStep, 0, k_Forward, 2.5f), Fight(Kind::FightBlock, 0, 0, 2.0f),
	                 Fight(Kind::FightBlow, 0, k_High, 3.0f, creature_fight::k_MaxChargeMs)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "combat.blocking",
	    .name = "Blocking an aggressive opponent",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger blocks while an evil blue tiger, as aggressive as fighters get, attacks "
	                   "it; every twelve seconds the red one steps back out of its block and blocks again.",
	    .expected = "The red tiger raises its guard and holds it. Blows on it make it reel back in its block and take "
	                "a tenth of the damage, leaving no wounds; the blue tiger attacks again and again.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.8f, -0.9f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlock, 0, 0, 7.0f),
	                 Fight(Kind::FightStep, 0, k_Back, 12.0f), Fight(Kind::FightBlock, 0, 0, 1.5f)},
	    .repeatFrom = 2,
	});

	all.push_back({
	    .id = "combat.charged",
	    .name = "Charged and quick blows",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger strikes a good, defensive blue tiger with blows in the middle, in turn "
	                   "let go at once and held the full 1.2 seconds.",
	    .expected = "A quick click's blow plays at half speed and does little; a fully charged blow plays half as fast "
	                "again as normal and takes three times as much off the blue tiger's fight health.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.9f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_Mid, 8.0f, 0.0f),
	                 Fight(Kind::FightBlow, 0, k_Mid, 4.0f, creature_fight::k_MaxChargeMs)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "combat.fight_casts",
	    .name = "Spells cast in a fight",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger, which knows the lightning bolt and the heal, fights a blue tiger that "
	                   "fights by itself, and is given the lightning bolt, then the heal, to cast in the fight.",
	    .expected = "For each, the red tiger raises its arms; as the start of the cast ends it tries the spell, paying "
	                "the fight's stamina for it, then lets go of it on the next update and lowers its arms. The "
	                "lightning, an aggressive spell, strikes the blue tiger and costs half its stamina; the heal, a "
	                "defensive one, would be cast at itself, but three seconds later, with stamina coming back a "
	                "six-hundredth a turn, it has too little left for it, and the try is refused, without hunger.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
	    .commands = {{.kind = Kind::KnowMiracle, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::LightningBolt)},
	                 {.kind = Kind::KnowMiracle, .delaySeconds = 0.0f, .value = static_cast<size_t>(MagicType::Heal)},
	                 Fight(Kind::StartFight, 0, 1, 0.5f),
	                 Fight(Kind::FightAuto, 1, 1, 0.2f),
	                 Fight(Kind::FightCast, 0, static_cast<size_t>(MagicType::LightningBolt), 8.0f),
	                 Fight(Kind::FightCast, 0, static_cast<size_t>(MagicType::Heal), 3.0f)},
	});

	all.push_back({
	    .id = "combat.fight_panel",
	    .name = "The fight's panel",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger makes an arena and fights a blue tiger, both fighting by themselves, "
	                   "seen from the ringside.",
	    .expected = "While the fight is on, the fight's panel at the top left of the screen: a see-through box with "
	                "each tiger's name over its health and stamina bars, the red tiger's row first, as the one that "
	                "made the arena.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 0.5f), Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Fight(Kind::FightAuto, 1, 1, 0.0f)},
	});

	all.push_back({
	    .id = "combat.fight_panel_under_fade",
	    .name = "The fight's panel under a fade",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger makes an arena and fights a blue tiger, both fighting by themselves, from "
	                   "the ringside; eight seconds in, the screen fades to black over ten seconds, as a script's "
	                   "SET_FADE does.",
	    .expected = "As the original draws the end of its frame, the fight's panel is under the fade: it darkens with "
	                "the world, at about two thirds of black by the shot.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 0.5f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Fight(Kind::FightAuto, 1, 1, 0.0f),
	                 {.kind = Kind::ScriptFade, .delaySeconds = 7.3f, .fadeColour = 0x000000, .fadeSeconds = 10.0f}},
	});

	auto worn = Fighter({0.0f, 0.0f}, 0.0f, "worn", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
	worn.hold = false;
	worn.pauseMind = true;
	worn.needs.life = 0.6f;
	worn.needs.exhaustion = 0.7f;
	all.push_back({
	    .id = "combat.faint_recovery",
	    .name = "Knocked out, taken home and back up",
	    .facet = Facet::Combat,
	    .description = "A worn tiger, life 0.6 and exhaustion 0.7, is given a home where it stands, walks 90 units away "
	                   "and is knocked out there as a fight's loser is.",
	    .expected = "It faints and lies out cold with its eyes closed for 12 seconds, is taken home, lies there a few "
	                "seconds more, then rests until it is no more exhausted than 0.3, and gets up.",
	    .framing = {.shot = Shot::Overview, .include = {{90.0f, 30.0f}, {-20.0f, 0.0f}}, .distance = 0.6f},
	    .creatures = {worn},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 400.0f},
	                 Go(Kind::WalkTo, 0, {90.0f, 30.0f}, 0.5f, false),
	                 Act(Kind::KnockOut, 0, 1.0f, true)},
	});

	all.push_back({
	    .id = "combat.leash",
	    .name = "Leashed onto another creature",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger is put on the aggression leash, which is tied to the blue tiger.",
	    .expected = "Tied to another creature, it fights it: both walk to the arena and duel, the red one fighting by "
	                "itself once left alone for three seconds.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Leash(0, LeashType::Evil, 1.0f), Fight(Kind::TieLeashToCreature, 0, 1, 2.0f)},
	});

	auto angry = red(PlayerNames::PLAYER_ONE, 0.6f, -0.3f);
	angry.desires = OnlyDesire(Desire::Anger);
	all.push_back({
	    .id = "combat.anger",
	    .name = "An angry creature picks a fight",
	    .facet = Facet::Combat,
	    .description = "A red tiger as angry as it gets, wanting nothing else, stands near a blue tiger, with angry "
	                   "creatures picking fights.",
	    .expected = "It picks a fight with the blue tiger by itself, and they duel. Once it is over, it waits two "
	                "minutes before picking another.",
	    .environment = {.angerStartsFights = true},
	    .framing = ringside,
	    .creatures = {angry, blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	});

	auto lion = Fighter(left, 90.0f, "evil lion", PlayerNames::PLAYER_TWO, 1.0f, -0.9f);
	lion.species = CreatureType::Lion;
	lion.size = 1.3f;
	auto small = Fighter(right, 270.0f, "small tiger", PlayerNames::PLAYER_ONE, 0.1f, 0.8f);
	small.size = 0.8f;
	small.needs.life = 0.3f;
	// Left to heal as it rests after being knocked out
	small.hold = false;
	all.push_back({
	    .id = "combat.evil_winner",
	    .name = "An evil winner has a poo on the loser",
	    .facet = Facet::Combat,
	    .description = "A big, strong, evil lion fights a small, weak, good tiger with little life left, so starting "
	                   "with little fight health, both by themselves.",
	    .expected = "The tiger soon faints. The lion finishes, walks up to it and has a poo on it, rather than showing "
	                "off as a good winner does.",
	    .framing = ringside,
	    .creatures = {lion, small},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 1, 1, 0.2f)},
	});
}

void AddMind(std::vector<Scenario>& all)
{
	// The game's belief type for villagers, the deed of playing with a toy, and the first skill of the game's table
	constexpr size_t k_VillagerBelief = 6;
	constexpr size_t k_PlayWithToy = 41;
	constexpr size_t k_FirstSkill = 0;
	const auto setDesire = [](Desire desire, float amount, float delay) {
		return Command {.kind = Kind::SetDesire,
		                .creature = 0,
		                .delaySeconds = delay,
		                .value = static_cast<size_t>(desire),
		                .amount = amount};
	};

	all.push_back({
	    .id = "mind.reward_villagers",
	    .name = "Learning: reward eating villagers, punish eating food",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger with a villager right by it, lots of magic food close by and more villagers further "
	                   "off. Every 18 seconds its "
	                   "hunger is set to 40% of its most, enough for one meal. Whatever it eats, as soon as it has eaten, "
	                   "it is stroked if it was a villager and slapped if it was anything else.",
	    .expected = "First it eats the villager beside it and is stroked: with one example, its hunger tree finds "
	                "everything good. Then it eats the nearest food and is slapped: now the tree tells them apart. From then "
	                "on it walks past the food to eat villagers, the "
	                "planner's goal usefulness for villagers near 0.8 and for food near 0. The spawner's Mind panel shows "
	                "the tree and the thoughts.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.3f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = OnlyDesire(Desire::Hunger),
	    }},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {12.0f, -12.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-25.0f, -25.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {25.0f, -30.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {0.0f, -40.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-35.0f, 15.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {35.0f, 20.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {70.0f, -60.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-70.0f, -60.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {80.0f, 30.0f}},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-80.0f, 40.0f}},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {0.0f, 85.0f}},
	                {.type = VillagerInfo::CelticTraderMale, .offset = {60.0f, 75.0f}}},
	    .commands = {{.kind = Kind::RewardIf, .creature = 0, .delaySeconds = 1.0f, .value = k_VillagerBelief},
	                 setDesire(Desire::Hunger, 0.4f, 1.0f),
	                 Act(Kind::Stop, 0, 17.0f)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "mind.community",
	    .name = "Load a chosen creature mind",
	    .facet = Facet::Mind,
	    .description = "A wolf that takes up the mind file last opened with the spawner's Open mind file button (a "
	                   "community-made creature, say), or else the game's own Khazar creature mind, with villagers, "
	                   "food, trees and a ball about it.",
	    .expected = "Its name, desires, opinions and the examples of its decision trees come from the file (the spawner's "
	                "Mind panel lists them, and its thoughts). The planner weighs what it has learnt: it goes for the "
	                "things its trees rate well. If neither file can be read the wolf keeps a fresh mind and the log "
	                "says why.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Wolf, .mindFile = "chosen:KhazarCreature"}},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-30.0f, -30.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {30.0f, -20.0f}},
	                {.type = TreeInfo::Beech, .offset = {-50.0f, 40.0f}},
	                {.type = TreeInfo::Beech, .offset = {55.0f, 45.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {60.0f, -50.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-60.0f, -55.0f}}},
	});

	// The game's Killer mind starts hungry (1.075) and needing to poo (1.2), and has learnt that a cow is good to eat (an
	// eighth) where magic food is worth a tenth; pooing is done to nothing, which counts a tenth
	all.push_back({
	    .id = "mind.learnt_hunger_against_poo",
	    .name = "A learnt hunger against the need to poo",
	    .facet = Facet::Mind,
	    .description = "A tiger with the game's Killer mind, as hungry and as much in need to poo as the file says, a cow and "
	                   "magic food at the same distance from it.",
	    .expected = "Its first plan is its hunger's, to eat the cow, when what it has learnt about the cow weighs in the "
	                "hunger's plan (an eighth against the poo's tenth); its need to poo's when the hunger counts the cow "
	                "only as a thing nothing is known about.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .mindFile = "game:Killer"}},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {-25.0f, 0.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {25.0f, 0.0f}}},
	    .commands = {Act(Kind::Stop, 0, 20.0f)},
	});

	all.push_back({
	    .id = "mind.hungry_takes_what_it_may_eat",
	    .name = "Hunger takes whatever it can eat",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger, wanting nothing else, with a barrel and a pot about it and no food at all.",
	    .expected = "Its hunger goes for the barrel, nearer, though it is worth no food: the creature's eating plans take "
	                "any object it can pick up. It eats the barrel and, as the eating ends, is sick, and its mind counts "
	                "mobile objects as no food; that count bars nothing it eats next.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.3f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = OnlyDesire(Desire::Hunger),
	    }},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {15.0f, -15.0f}},
	                {.type = MobileObjectInfo::EgyptPotA, .offset = {-30.0f, -25.0f}}},
	    .commands = {Act(Kind::Stop, 0, 40.0f)},
	});

	all.push_back({
	    .id = "mind.hungry_eats_from_magic_food",
	    .name = "Eating from a pile of magic food",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger, wanting nothing else, with a pile of magic food and a plain pile of food about it, "
	                   "no town near.",
	    .expected = "When its plan is to eat from a pile, it goes up to the magic food, turns to it and makes a pot of "
	                "wheat at nearly its reach towards it, a thousand times its drawn size's worth, drawn three and three "
	                "quarter times its size; it picks that up, and as its hand takes hold the pile loses as much; then it "
	                "eats it. That plan never takes the plain pile.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.3f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = OnlyDesire(Desire::Hunger),
	    }},
	    .objects = {{.type = PotInfo::MagicFood, .offset = {0.0f, -40.0f}, .amount = 2500},
	                {.type = PotInfo::FoodPile, .offset = {40.0f, -30.0f}, .amount = 2500}},
	    .commands = {Act(Kind::Stop, 0, 60.0f)},
	});

	std::vector<Command> phases;
	for (size_t phase = 1; phase <= k_LastPhase; ++phase)
	{
		phases.push_back({.kind = Kind::SetPhase, .creature = 0, .delaySeconds = 8.0f, .value = phase});
	}
	all.push_back({
	    .id = "mind.phases",
	    .name = "Stages of growing up unlock desires",
	    .facet = Facet::Mind,
	    .description = "A cow that starts at the first stage of growing up and moves on a stage every eight seconds, "
	                   "through all fourteen.",
	    .expected = "At first only fear, tiredness, attention, showing how it is and the like are active; hunger, "
	                "curiosity and play come at the second stage, water at the fourth, anger at the eleventh, compassion "
	                "at the twelfth. The Mind panel's desires list grows, and the planner starts weighing what each new "
	                "desire can do.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Cow, .phase = 0}},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-30.0f, -30.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {30.0f, -20.0f}}},
	    .commands = phases,
	});

	all.push_back({
	    .id = "mind.mimic",
	    .name = "Mimic the player",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger with balls about it. Every 20 seconds the player plays with a toy near it, the "
	                   "one deed creatures copy without the learning leash.",
	    .expected = "Nine times in ten it notices: it turns to look where the player played; a few seconds later it "
	                "picks up a ball and throws it about, copying the player; then for a while it wants to play. The "
	                "Mind panel shows the stage of copying.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {25.0f, -25.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {-30.0f, -20.0f}}},
	    .commands =
	        {{.kind = Kind::PlayerDid, .creature = 0, .delaySeconds = 3.0f, .point = {30.0f, -30.0f}, .value = k_PlayWithToy},
	         Act(Kind::Stop, 0, 20.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "mind.watch_skill",
	    .name = "Learn a skill by watching",
	    .facet = Facet::Mind,
	    .description = "A grown-up lion watches villagers build: it sees the skill now, again 4 seconds later and again "
	                   "4 seconds after that.",
	    .expected = "Once it has watched for longer than the skill takes to learn (6 seconds by the game's table) it "
	                "knows it: its thoughts say it has learnt to build, and the Mind panel marks the skill known.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Lion, .needs = Content()}},
	    .commands = {{.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 1.0f, .value = k_FirstSkill},
	                 {.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 4.0f, .value = k_FirstSkill},
	                 {.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 4.0f, .value = k_FirstSkill}},
	});

	// Timed for the checks' 900 frames of 16 ms, 14.4 seconds, with the shot at 14.1. The focus comes 2 seconds in,
	// once the tiger stands settled; its turn on the spot, half way round, is over in a few seconds, so it holds still
	// facing the point for some 5 seconds before the release at 10 seconds. With nothing it needs it has no plan of its
	// own to take up, so the shot, 4 seconds after the release, still shows it facing the point, towards the camera,
	// its head free of the point again
	all.push_back({
	    .id = "creature.focus_by_script",
	    .name = "A script's focus on a creature, and its release",
	    .facet = Facet::Mind,
	    .description = "A content tiger in the middle of the testbed, its back to the camera. 2 seconds in a script's "
	                   "SET_FOCUS points it at a spot 40 units behind it, between it and the camera; 8 seconds later the "
	                   "script's RELEASE_FROM_SCRIPT gives it back.",
	    .expected = "The turn: the focus takes it under the script's control, it gives up whatever it was doing and turns "
	                "round on the spot until it faces the spot, and the camera. The hold: it then stands still under "
	                "control, its head looking at the spot, and its mind plans nothing of its own (the spawner's Mind "
	                "panel shows the script's plan). The release: it is in its own control again, its head no longer "
	                "held to the spot, and its mind is free to plan again (with nothing it needs, it still stands). The log "
	                "says it is under the script's control after the focus and in its own control after the release.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {{.kind = Kind::ScriptFocus, .creature = 0, .delaySeconds = 2.0f, .point = {0.0f, -40.0f}},
	                 {.kind = Kind::ScriptRelease, .creature = 0, .delaySeconds = 8.0f}},
	});

	// Two content tigers side by side, their backs to the camera. 2 seconds in a script gives the left one a sneeze, an
	// individual action, to play twice and sets its state; half a second later it gives the right one sitting, a static
	// action (38: its start 37, the loop 38 held ten seconds and its end 39), to play once. PLAYED is asked of each
	// every second after, to 13.5 seconds, before the sit is over
	{
		constexpr size_t k_Sneeze = 62;
		constexpr size_t k_Sit = 38;
		const auto played = [](size_t creature) {
			return Command {.kind = Kind::ScriptPlayed, .creature = creature, .delaySeconds = 0.5f};
		};
		Scenario play {
		    .id = "creature.script_play",
		    .name = "A script's play on a creature, and PLAYED",
		    .facet = Facet::Mind,
		    .description = "Two content tigers side by side, their backs to the camera. 2 seconds in a script's "
		                   "SET_SCRIPT_ULONG and SET_SCRIPT_STATE have the left one sneeze twice; half a second later the "
		                   "right one is given sitting once, a static action. The script asks PLAYED of each every second.",
		    .expected = "The sneeze: the left tiger is taken under the script's control, gives up what it was doing and "
		                "sneezes, then sneezes again once the first is over, and stands still under control. PLAYED says "
		                "false while it sneezes and true once the second sneeze is over. The sit: the right tiger sits "
		                "down, stays sitting for ten seconds and gets up again; PLAYED says false the whole time it sits. "
		                "The log says each play was given under the script's control.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {Content(CreatureType::Tiger, {-10.0f, 0.0f}, 180.0f, "left"),
		                  Content(CreatureType::Tiger, {10.0f, 0.0f}, 180.0f, "right")},
		    .commands = {{.kind = Kind::ScriptPlay, .creature = 0, .delaySeconds = 2.0f, .value = k_Sneeze, .plays = 2},
		                 {.kind = Kind::ScriptPlay, .creature = 1, .delaySeconds = 0.5f, .value = k_Sit, .plays = 1}},
		};
		for (int second = 0; second < 11; ++second)
		{
			play.commands.push_back(played(0));
			play.commands.push_back(played(1));
		}
		all.push_back(std::move(play));
	}

	// Two content tigers side by side, their backs to the camera. 2 seconds in a script's MOVE_GAME_THING sends each to
	// a point 60 ahead of it: the left one with no distance, so in the game's two legs, first within a sixth of the
	// way, then within its height; the right one within 5, twice. 9 seconds later a script's SET_FOCUS turns the left one
	// back to where it started
	all.push_back({
	    .id = "creature.script_move",
	    .name = "A script sends a creature somewhere, in two legs",
	    .facet = Facet::Mind,
	    .description = "Two content tigers side by side, their backs to the camera. 2 seconds in a script's MOVE_GAME_THING "
	                   "sends each to a point 60 ahead of it, the left one with no distance and the right one within 5; 9 "
	                   "seconds later a script's SET_FOCUS turns the left one back to where it started.",
	    .expected = "Both are taken under the script's control and walk ahead, their heads on their points' ground until "
	                "they are within 10 of them. The left one stops a first time within a sixth of the way, its height "
	                "if that is more (no more than 0.7 of its height and 22), then walks on and stops within its "
	                "height; the right one goes on until it is within 5, and stops. The focus then turns the left one "
	                "round on the spot to face where it came from, its head on that spot.",
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 60.0f}, {10.0f, 60.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-10.0f, 0.0f}, 180.0f, "left"),
	                  Content(CreatureType::Tiger, {10.0f, 0.0f}, 180.0f, "right")},
	    .commands = {{.kind = Kind::ScriptMove, .creature = 0, .delaySeconds = 2.0f, .point = {-10.0f, 60.0f}},
	                 {.kind = Kind::ScriptMove, .creature = 1, .delaySeconds = 0.0f, .point = {10.0f, 60.0f}, .radius = 5.0f},
	                 {.kind = Kind::ScriptFocus, .creature = 0, .delaySeconds = 9.0f, .point = {-10.0f, 0.0f}}},
	});

	// A content tiger with a villager walking to and fro to its right and a rock ahead of it, as the glade's scripts
	// stage their creatures: 2 seconds in a script's CREATURE_DO_ACTION has it look at the villager for good
	// (LookForever, row 189), 3 seconds later look at the villager's feet without going up to it (LookButDontApproach,
	// row 188), 4 seconds after that sit at the rock (SitDown, row 81, under the script's control)
	{
		constexpr size_t k_LookForever = 189;
		constexpr size_t k_LookButDontApproach = 188;
		constexpr size_t k_SitDown = 81;
		all.push_back({
		    .id = "creature.script_do_action",
		    .name = "A script stages a creature: looking, looking without going up, sitting at a rock",
		    .facet = Facet::Mind,
		    .description = "A content tiger, its back to the camera, a villager walking to and fro 30 to its right and a "
		                   "rock 30 ahead of it. A script's CREATURE_DO_ACTION has it look at the villager for good 2 "
		                   "seconds in, look at the villager's feet without going up to it 3 seconds later, and sit at the "
		                   "rock 4 seconds after that.",
		    .expected = "The look for good: under the script's control it turns to face the villager in about 2 seconds, "
		                "its head on the villager's top, and keeps watching it as it walks. The look without going up: it "
		                "turns to the villager again, then watches its feet for 1.1 to 2.3 seconds and stands still. The "
		                "sit: it walks to the rock, stops within its own radius of it, turns to face it, then down the "
		                "slope, and sits down for 10 to 15 seconds looking about, pulling no face. The log says each "
		                "action was taken up under the script's control.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {{.type = VillagerInfo::CelticFarmerMale,
		                 .offset = {30.0f, -10.0f},
		                 .walkTo = glm::vec2(30.0f, 10.0f),
		                 .walkRepeatSeconds = 6.0f},
		                {.type = MobileStaticInfo::Rock, .offset = {0.0f, 30.0f}}},
		    .commands =
		        {{.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 2.0f, .value = k_LookForever, .object = 0},
		         {.kind = Kind::ScriptDoAction,
		          .creature = 0,
		          .delaySeconds = 3.0f,
		          .value = k_LookButDontApproach,
		          .object = 0},
		         {.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 4.0f, .value = k_SitDown, .object = 1}},
		});
	}

	// Reward chests a script gives, small food, medium food and a heal power-up: on the land, and from the sky
	all.push_back({
	    .id = "rewards.on_land",
	    .name = "Reward chests a script puts on the land",
	    .facet = Facet::Objects,
	    .description = "A script's CREATE_REWARD puts three chests on the land a second in, ten metres apart: a small "
	                   "food, a medium food and a heal power-up reward.",
	    .expected = "Each closed chest stands on the land at once where it was given, its dust made with it but not "
	                "shown, since it did not fall. The log gives each chest's object.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 25.0f}, {15.0f, 35.0f}}, .distance = 0.4f},
	    .commands = {{.kind = Kind::ScriptReward, .delaySeconds = 1.0f, .point = {-10.0f, 30.0f}, .value = 3},
	                 {.kind = Kind::ScriptReward, .delaySeconds = 0.0f, .point = {0.0f, 30.0f}, .value = 4},
	                 {.kind = Kind::ScriptReward, .delaySeconds = 0.0f, .point = {10.0f, 30.0f}, .value = 1}},
	});
	all.push_back({
	    .id = "rewards.from_sky",
	    .name = "A reward chest a script drops from the sky",
	    .facet = Facet::Objects,
	    .description = "A script's CREATE_REWARD drops a small food chest from the sky a second in.",
	    .expected = "The chest appears 150 above the land, falls 50 a second turning half a round a second, and after three "
	                "seconds thumps down on the land with the landing sound. Far from the world's first corner, the "
	                "camera does not shake (the game shakes it only within 200 of the world's origin). A cloud of sandy "
	                "dust shows about it as it lands and again from the next turn, spreading and fading away over two "
	                "and a half seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 25.0f}, {10.0f, 35.0f}}, .distance = 0.4f},
	    .commands = {{.kind = Kind::ScriptReward, .delaySeconds = 1.0f, .point = {0.0f, 30.0f}, .value = 3, .fromSky = true}},
	});

	all.push_back({
	    .id = "script.populate_flock",
	    .name = "A script fills a flock with sheep",
	    .facet = Facet::Objects,
	    .description = "A script makes a flock in front of the camera and fills it with eight sheep (POPULATE_CONTAINER).",
	    .expected = "Eight sheep appear about the flock's place, spread up to 4.8 m either way across and in height but in "
	                "line with it along the depth, each a member of the flock that then moves together; the log gives "
	                "the flock and its eight members.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 15.0f}, {15.0f, 45.0f}}},
	    .commands = {{.kind = Kind::ScriptPopulate,
	                  .delaySeconds = 1.0f,
	                  .point = {0.0f, 30.0f},
	                  .value = static_cast<size_t>(script::ObjectType::Animal),
	                  .amount = 8.0f,
	                  .subtype = static_cast<uint32_t>(AnimalInfo::Sheep)}},
	});

	// A creature's pick-up of an animal by its reach and its weight: a small creature and a full-size one, held, each
	// reaching for a sheep and then a cow before it, putting each down again
	{
		const auto pickUp = [](size_t creature, size_t object) {
			return Command {
			    .kind = Kind::PickUp, .creature = creature, .delaySeconds = 1.0f, .waitUntilFree = true, .object = object};
		};
		const auto putDown = [](size_t creature) {
			return Command {.kind = Kind::PutDown, .creature = creature, .delaySeconds = 1.0f, .waitUntilFree = true};
		};
		all.push_back({
		    .id = "creature.pick_up_by_size",
		    .name = "A creature picks an animal up by its reach and weight",
		    .facet = Facet::Objects,
		    .description = "Two held tigers whose minds are paused, a small one (size 0.25) on the left and a full-size one "
		                   "on the right, each with a sheep and a cow before it. Each reaches for its sheep, puts it down, "
		                   "then reaches for its cow and puts it down.",
		    .expected = "Each attempt goes by the creature's own test, not by whether a hand may hold the animal: the "
		                "animal's radius and the creature's together within the creature's reach, and the animal's weight "
		                "within 0.8 of the creature's. A creature lifts what passes and gives up on what does not ('it "
		                "can't be picked up'); the log gives each attempt's result, the small tiger failing where the "
		                "full-size one lifts.",
		    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -5.0f}, {25.0f, 15.0f}}},
		    .creatures =
		        {{.species = CreatureType::Tiger, .offset = {-15.0f, 0.0f}, .size = 0.25f, .hold = true, .pauseMind = true},
		         {.species = CreatureType::Tiger, .offset = {15.0f, 0.0f}, .hold = true, .pauseMind = true}},
		    .objects = {{.type = AnimalInfo::Sheep, .offset = {-17.0f, 6.0f}},
		                {.type = AnimalInfo::Cow, .offset = {-13.0f, 6.0f}},
		                {.type = AnimalInfo::Sheep, .offset = {13.0f, 8.0f}},
		                {.type = AnimalInfo::Cow, .offset = {17.0f, 8.0f}}},
		    .commands = {pickUp(0, 0), putDown(0), pickUp(0, 1), putDown(0), pickUp(1, 2), putDown(1), pickUp(1, 3),
		                 putDown(1)},
		});
		// The same test of a villager: a very small tiger and a full-size one, each reaching for a villager before it
		all.push_back({
		    .id = "creature.pick_up_villagers",
		    .name = "A creature picks a villager up by its reach and weight",
		    .facet = Facet::Objects,
		    .description = "Two held tigers whose minds are paused, a very small one (size 0.1) on the left and a full-size "
		                   "one on the right, each with a Norse shepherd before it. Each reaches for its villager and puts "
		                   "it down again.",
		    .expected = "Each attempt goes by the creature's own test of things in general, as an animal's does: the "
		                "villager's radius and the creature's together within the creature's reach, and the villager's "
		                "weight (82.5 at full size) within 0.8 of the creature's. Neither villager is at home or "
		                "worshipping, so only the reach and the weight decide. The log gives each attempt's result; the "
		                "full-size tiger lifts its villager, and the very small one gives up ('it can't be picked up') if "
		                "the villager is too heavy or too wide for it.",
		    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -5.0f}, {25.0f, 15.0f}}},
		    .creatures =
		        {{.species = CreatureType::Tiger, .offset = {-15.0f, 0.0f}, .size = 0.1f, .hold = true, .pauseMind = true},
		         {.species = CreatureType::Tiger, .offset = {15.0f, 0.0f}, .hold = true, .pauseMind = true}},
		    .objects = {{.type = VillagerInfo::NorseShepherdMale, .offset = {-15.0f, 4.0f}},
		                {.type = VillagerInfo::NorseShepherdMale, .offset = {15.0f, 8.0f}}},
		    .commands = {pickUp(0, 0), putDown(0), pickUp(1, 1), putDown(1)},
		});
		// The same test of mobile objects: a ball and a cart before a very small tiger and a full-size one
		all.push_back({
		    .id = "creature.pick_up_mobile_objects",
		    .name = "A creature picks a mobile object up by its reach and weight",
		    .facet = Facet::Objects,
		    .description = "Two held tigers whose minds are paused, a very small one (size 0.1) on the left and a full-size "
		                   "one on the right, each with a ball and a cart before it. Each reaches for its ball, puts it "
		                   "down, then reaches for its cart and puts it down.",
		    .expected = "Each attempt goes by the creature's own test of things in general: the object's radius and the "
		                "creature's together within the creature's reach, and the object's weight within 0.8 of the "
		                "creature's. The log gives each attempt's result; the very small tiger gives up ('it can't be "
		                "picked up') on what is too wide or too heavy for it, where the full-size one lifts.",
		    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -5.0f}, {25.0f, 15.0f}}},
		    .creatures =
		        {{.species = CreatureType::Tiger, .offset = {-15.0f, 0.0f}, .size = 0.1f, .hold = true, .pauseMind = true},
		         {.species = CreatureType::Tiger, .offset = {15.0f, 0.0f}, .hold = true, .pauseMind = true}},
		    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-17.0f, 4.0f}},
		                {.type = MobileObjectInfo::Cart, .offset = {-13.0f, 4.0f}},
		                {.type = MobileObjectInfo::Ball, .offset = {13.0f, 8.0f}},
		                {.type = MobileObjectInfo::Cart, .offset = {17.0f, 8.0f}}},
		    .commands = {pickUp(0, 0), putDown(0), pickUp(0, 1), putDown(0), pickUp(1, 2), putDown(1), pickUp(1, 3),
		                 putDown(1)},
		});
	}

	all.push_back({
	    .id = "script.health_of_living",
	    .name = "A script reads the health of animals and villagers",
	    .facet = Facet::Mind,
	    .description = "Three sheep, at full life, half and a quarter, and two Norse villagers, at full life and half, "
	                   "in a row. From 1 second in, a script's GET_PROPERTY of HEALTH reads each in turn, half a "
	                   "second apart, as Land 1's bleating sheep read theirs to know whether they still live.",
	    .expected = "The log gives each one's life as a float, 1.000, 0.500, 0.250, 1.000 and 0.500, rather than 0 for "
	                "all: a script that stops at a health of 0 or less, as each bleating sheep's does, carries on for "
	                "a living one. Nothing on the land changes.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}},
	    .objects = {{.type = AnimalInfo::Sheep, .offset = {-30.0f, -5.0f}},
	                {.type = AnimalInfo::Sheep, .offset = {-15.0f, -5.0f}, .life = 0.5f},
	                {.type = AnimalInfo::Sheep, .offset = {0.0f, -5.0f}, .life = 0.25f},
	                {.type = VillagerInfo::NorseShepherdMale, .offset = {15.0f, 5.0f}},
	                {.type = VillagerInfo::NorseShepherdMale, .offset = {30.0f, 5.0f}, .life = 0.5f}},
	    .commands = {{.kind = Kind::ScriptHealth, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::ScriptHealth, .delaySeconds = 0.5f, .object = 1},
	                 {.kind = Kind::ScriptHealth, .delaySeconds = 0.5f, .object = 2},
	                 {.kind = Kind::ScriptHealth, .delaySeconds = 0.5f, .object = 3},
	                 {.kind = Kind::ScriptHealth, .delaySeconds = 0.5f, .object = 4}},
	});
}

void AddParticles(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "particles.smoke_and_fire",
	    .name = "Smoke, steam and a bonfire",
	    .facet = Facet::Particles,
	    .description = "The spot visuals' smoke, steam, evil smoke and a bonfire, side by side on the plain.",
	    .expected = "Grey smoke and white steam puffs rise, spin and fade; the evil smoke is dark; the bonfire's flames "
	                "flicker in its additive sprites with smoke rising above them.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.type = ParticleType::Smoke, .offset = {-30.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Steam, .offset = {-10.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::EvilSmoke, .offset = {10.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Bonfire, .offset = {30.0f, 0.0f}, .magnitude = 5.0f}},
	});

	all.push_back({
	    .id = "particles.sparkles",
	    .name = "Sparkles and magic",
	    .facet = Facet::Particles,
	    .description = "Short effects started again every few seconds: the failed cast, the hand gripping the land, a "
	                   "magic object made, an object appearing in the second player's colour, and a forest made.",
	    .expected = "Each plays out in sprites from the sprite sheets and starts again: the cross of the failed cast, "
	                "a ring of sparks, a spinning cloud of sparks, sparks fountaining up in green, and falling leaves.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.45f},
	    .particles =
	        {{.type = ParticleType::SpellFail, .offset = {-30.0f, 0.0f}, .height = 5.0f, .restartSeconds = 4.0f},
	         {.type = ParticleType::GripLandscape, .offset = {-15.0f, 0.0f}, .restartSeconds = 3.0f},
	         {.type = ParticleType::MagicObjectCreated,
	          .offset = {0.0f, 0.0f},
	          .height = 3.0f,
	          .magnitude = 3.0f,
	          .restartSeconds = 5.0f},
	         {.type = ParticleType::ObjectAppear,
	          .offset = {15.0f, 0.0f},
	          .magnitude = 3.0f,
	          .player = 1,
	          .restartSeconds = 4.0f},
	         {.type = ParticleType::ForestCreated, .offset = {30.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.models",
	    .name = "Models among the sparks",
	    .facet = Facet::Particles,
	    .description = "Effects whose particles are models: the vortex rising where a magic object is made, the glowing "
	                   "cone of the beam marking a place, the paper streamers of the ticker tape and the fireball's flash.",
	    .expected = "A spinning additive vortex model rises and fades; a glowing cone turns to face the camera wherever it "
	                "looks from; coloured streamers fall from a disk; the flash's model plays through its texture's frames.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_MagicObjectCreated2", .offset = {-30.0f, 0.0f}, .magnitude = 2.0f, .restartSeconds = 5.0f},
	                  {.file = "SF_SeeThisBeam", .offset = {-10.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_TickerTape", .offset = {10.0f, 0.0f}, .height = 15.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_Flash", .offset = {30.0f, 0.0f}, .height = 3.0f, .magnitude = 2.0f, .restartSeconds = 3.0f}},
	});

	all.push_back({
	    .id = "particles.light_maps",
	    .name = "Light on the land",
	    .facet = Facet::Particles,
	    .description = "Effects that light the ground under them, as fire and lightning do, at night so the light shows: "
	                   "the ground effect's ring and the light a landscape vortex leaves, each many cells across.",
	    .expected = "A ring of light spreads over the ground and fades, again and again; a steady pool of coloured light "
	                "lies on the ground beside it, lighting what stands there too.",
	    .environment = {.hour = 23.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, 0.0f}, {120.0f, 0.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_GroundEffect", .offset = {-70.0f, 0.0f}, .magnitude = 2.0f, .restartSeconds = 4.0f},
	                  {.file = "SF_LandscapeVortexLightMap", .offset = {70.0f, 0.0f}}},
	});

	all.push_back({
	    .id = "particles.vortex_and_flies_sounds",
	    .name = "The landscape vortex's and the flies' sounds",
	    .facet = Facet::Particles,
	    .description = "The landscape vortex as it opens and as it closes, and a swarm of flies, side by side and close up, "
	                   "every eight seconds. Their sounds are named in the effect files by sound actions the game's header "
	                   "numbers by counting on from the one before.",
	    .expected = "The vortices play the vortex's hum and the flies their buzz as they start; the debug log names an "
	                "emitter for each.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 30.0f}, {15.0f, 50.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_LandscapeVortexInBefore", .offset = {-8.0f, 40.0f}, .restartSeconds = 8.0f},
	                  {.file = "SF_LandscapeVortexOutAfter", .offset = {8.0f, 40.0f}, .restartSeconds = 8.0f},
	                  {.file = "SF_Flies", .offset = {0.0f, 36.0f}, .height = 2.0f, .restartSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "particles.mist",
	    .name = "Mist over the holders",
	    .facet = Facet::Particles,
	    .description = "The water and storm miracles' mists, as they sit over a holder, and the water miracle in the hand.",
	    .expected = "Puffs of mist, shaped wider than tall, turn to face the camera, shrink seen edge on and grow seen from "
	                "above, and play through their frames of the smoke texture.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.file = "SF_WaterOnHolder", .offset = {-15.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f},
	                  {.file = "SF_LightningStormOnHolder", .offset = {0.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f},
	                  {.file = "SF_WaterInHand", .offset = {15.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f}},
	});

	all.push_back({
	    .id = "particles.symbols_and_heal",
	    .name = "Player symbols and the heal chakra",
	    .facet = Facet::Particles,
	    .description = "The player icon fountain throwing up the casting player's symbol in each player's colour, and the "
	                   "heal miracle's chakra over two creatures.",
	    .expected = "Symbols between two glows of the player's colour, one turning, arc up out of each fountain and fade; "
	                "a chakra appears over each creature and follows it, a burst of sparks under it rising and fading.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.45f},
	    .creatures = {{.offset = {15.0f, 0.0f}}, {.offset = {30.0f, 5.0f}}},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-30.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {-15.0f, 0.0f}, .magnitude = 2.0f, .player = 1},
	                  {.file = "SF_HealChakra",
	                   .targetsCreatures = true,
	                   .offset = {22.0f, 0.0f},
	                   .magnitude = 2.0f,
	                   .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.draw_paths",
	    .name = "Sorted and queued sprites",
	    .facet = Facet::Particles,
	    .description = "The same smoke drawn three ways side by side: each puff in its own place among what blends, the "
	                   "whole effect at its origin as some spot visuals are, and as the miracle in the hand is.",
	    .expected = "The sorted smoke's puffs blend far to near from any side; the queued smoke draws newest first as one, "
	                "so seen from above older puffs cover newer ones; the third matches the queued without a hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.3f},
	    .particles =
	        {{.type = ParticleType::Smoke, .offset = {-20.0f, 0.0f}, .magnitude = 8.0f},
	         {.type = ParticleType::Smoke, .path = psys::DrawPath::Queued, .offset = {0.0f, 0.0f}, .magnitude = 8.0f},
	         {.type = ParticleType::Smoke, .path = psys::DrawPath::Immediate, .offset = {20.0f, 0.0f}, .magnitude = 8.0f}},
	});

	// The scrolls stand 30 units north of the middle; the camera is about 20 units from them, inside the distances at
	// which a scroll is scaled with the distance, so it is drawn as big as from 30 units at its full size
	all.push_back({
	    .id = "particles.highlight_glints",
	    .name = "The challenge scrolls' glints",
	    .facet = Facet::Particles,
	    .description = "A silver and a gold challenge scroll, as a land's script sets them out, stand side by side on the "
	                   "land north of the middle, neither of them lit, with the camera looking at them from close by.",
	    .expected = "Each scroll turns about its upright, half a turn a second, and glints sparkle over it, laid on its "
	                "model: silver ones on the silver scroll and gold ones on the gold. They show only while the scroll "
	                "is drawn, and turn with it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 10.0f}, .look = {0.0f, 1.0f, 30.0f}},
	    .fixtures = {.highlights = {{.info = ecs::script_highlight::Info::Silver, .at = glm::vec2 {-4.0f, 30.0f}},
	                                {.info = ecs::script_highlight::Info::Gold, .at = glm::vec2 {4.0f, 30.0f}}}},
	});

	// The same place and camera as particles.highlight_glints, with one gold scroll in the middle, lit as a tapped
	// scroll is lit
	all.push_back({
	    .id = "particles.highlight_lit",
	    .name = "A lit challenge scroll",
	    .facet = Facet::Particles,
	    .description = "A gold challenge scroll, as a land's script sets it out, stands on the land north of the middle, "
	                   "lit as a tapped scroll is lit, with the camera looking at it from close by.",
	    .expected = "The scroll shows its lit model and turns about its upright. Its active effect plays at its centre, "
	                "and its glow, its gold glints laid on its model and its sparkles show with it while it is drawn.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 10.0f}, .look = {0.0f, 1.0f, 30.0f}},
	    .fixtures = {.highlights = {{.info = ecs::script_highlight::Info::Gold, .at = glm::vec2 {0.0f, 30.0f}, .lit = true}}},
	});

	// The same place and camera as particles.highlight_glints, with one gold scroll in view and one behind the camera,
	// which the object draw list does not draw
	all.push_back({
	    .id = "particles.highlight_offscreen",
	    .name = "A challenge scroll out of view",
	    .facet = Facet::Particles,
	    .description = "Two gold challenge scrolls, as a land's script sets them out: one north of the middle, with the "
	                   "camera looking at it from close by, and one south of the camera, behind it.",
	    .expected = "The scroll in view turns about its upright. The one behind the camera is not drawn, and its pose "
	                "is only set on the frames the object draw list draws it, so it keeps its last pose meanwhile.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 10.0f}, .look = {0.0f, 1.0f, 30.0f}},
	    .fixtures = {.highlights = {{.info = ecs::script_highlight::Info::Gold, .at = glm::vec2 {0.0f, 30.0f}},
	                                {.info = ecs::script_highlight::Info::Gold, .at = glm::vec2 {0.0f, -4.0f}}}},
	});

	// A dispenser of the creature spell Freeze makes one bubble, with the Freeze phial spinning in its middle; the
	// phial's holder effect is the one with glints. The bubble's middle is 6.4 units above the land (as for
	// hand.orb_pick_throw); the camera looks straight at it from about 7 units to the south. The village to the west,
	// out of view, gives the dispenser its town
	all.push_back({
	    .id = "particles.freeze_phial_glints",
	    .name = "The Freeze phial's glints",
	    .facet = Facet::Particles,
	    .description = "A dispenser of the creature spell Freeze stands north of the middle with its one bubble, the Freeze "
	                   "phial inside it, with the camera looking straight at the bubble from close by. Nothing touches it.",
	    .expected = "The phial spins in the middle of the bubble, its picture frames running, and glints sparkle over it, "
	                "laid on its model and turning with it, seen through the bubble.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 7.5f, 18.0f}, .look = {0.0f, 6.4f, 25.0f}},
	    .fixtures = {.dispensers = {{.magic = MagicType::CreatureSpellFreeze, .at = glm::vec2 {0.0f, 25.0f}}},
	                 .villages = {{.at = glm::vec2 {-60.0f, 10.0f}, .huts = 1, .villagers = 0, .storagePit = false}}},
	});
}

void AddEditor(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "editor.one_of_each",
	    .name = "One of each kind to pick",
	    .facet = Facet::Editor,
	    .description = "A tiger, a villager, an oak, a pillar of rock and a ball, spread out in view, for trying the "
	                   "editor (F2) on: the outliner lists each under its kind, a click on the land or in the list "
	                   "picks one, and the inspector shows it.",
	    .expected = "Each picks with a box drawn round it. The Move (E) and Rotate (R) tools drag the picked thing over "
	                "the land and turn it; Ctrl+D copies it beside itself and Delete removes it. The tiger's inspector "
	                "has every creature section.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-25.0f, -10.0f}},
	                {.type = TreeInfo::Oak, .offset = {-40.0f, 20.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 20.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {20.0f, -15.0f}}},
	});

	all.push_back({
	    .id = "editor.orbit_walker",
	    .name = "A walking creature to orbit and follow",
	    .facet = Facet::Editor,
	    .description = "A tiger walks round a square 80 units across, again and again, for trying the editor's cameras "
	                   "on it: pick it, then Orbit (O) or Follow (Shift+O).",
	    .expected = "Orbiting, the camera keeps round the tiger as it walks, turned by dragging with the right button "
	                "and drawn in and out by the wheel. Following, it eases along behind the tiger as it turns each "
	                "corner. Free, or Escape, hands the camera back where it is.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {-40.0f, -40.0f})},
	    .commands = {Go(Kind::WalkTo, 0, {40.0f, -40.0f}, 0.5f), Go(Kind::WalkTo, 0, {40.0f, 40.0f}, 0.5f),
	                 Go(Kind::WalkTo, 0, {-40.0f, 40.0f}, 0.5f), Go(Kind::WalkTo, 0, {-40.0f, -40.0f}, 0.5f)},
	    .repeatFrom = 0,
	});
}

/// A benchmark of a crowd of the size, to compare runs of several sizes and see which costs grow faster than the crowd
/// does
void AddCrowd(std::vector<Scenario>& all, Crowd::Kind kind, size_t count, std::string_view id, std::string_view name)
{
	const bool creatures = kind == Crowd::Kind::Creatures;
	all.push_back({
	    .id = id,
	    .name = name,
	    .facet = Facet::Benchmark,
	    .description = creatures ? "Creatures of every species and of four players, spread evenly over the land round the "
	                               "middle and clear of the lake, the same way every run. Nothing holds them: their minds, "
	                               "needs, walking and route finding all run as in the game."
	                             : "Villagers of every tribe and role in towns of fifty, each a ring of huts about a storage "
	                               "pit, spread evenly over the land round the middle and clear of the lake, the same way "
	                               "every run, and left to go about their lives.",
	    .expected = "They spawn a batch a frame, then the frames are measured once they have settled: the mean, 95th "
	                "percentile and slowest frame and the costliest profiler stages show as it runs, and Save results "
	                "writes them out to compare with the other sizes.",
	    // The crowd is spread over the middle, where the miracle dispensers would stand
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .crowd = Crowd {.kind = kind, .count = count, .seed = k_BenchmarkSeed, .perFrame = creatures ? 100u : 250u},
	});
}

void AddBenchmark(std::vector<Scenario>& all)
{
	// Steps close enough together that a cost growing faster than the crowd shows between them
	AddCrowd(all, Crowd::Kind::Creatures, 10, "benchmark.creatures_10", "10 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 25, "benchmark.creatures_25", "25 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 50, "benchmark.creatures_50", "50 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 100, "benchmark.creatures_100", "100 active creatures");
	AddCrowd(all, Crowd::Kind::Villagers, 100, "benchmark.villagers_100", "100 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 250, "benchmark.villagers_250", "250 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 500, "benchmark.villagers_500", "500 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 1'000, "benchmark.villagers_1000", "1,000 active villagers");
}

std::vector<Scenario> Build()
{
	std::vector<Scenario> all;
	AddIdle(all);
	AddExpressions(all);
	AddSenses(all);
	AddNeeds(all);
	AddDeadBodies(all);
	AddGrowth(all);
	AddAppearance(all);
	AddLight(all);
	AddMovement(all);
	AddFootprints(all);
	AddAudio(all);
	AddObjects(all);
	AddHand(all);
	AddLeash(all);
	AddCombat(all);
	AddMind(all);
	AddParticles(all);
	AddEditor(all);
	AddMiracleScenarios(all);
	AddGlobeScenarios(all);
	AddLifeLightScenarios(all);
	AddCreatureCastingScenarios(all);
	AddBenchmark(all);
	AddCreatureModeScenarios(all);
	AddGestureScenarios(all);
	AddStormScenarios(all);
	AddFlockScenarios(all);
	AddTeleportScenarios(all);
	AddTornadoScenarios(all);
	AddPhysicsScenarios(all);
	AddHandNavigationScenarios(all);
	AddHandLookScenarios(all);
	AddExampleScenarios(all);
	AddGateScenarios(all);
	AddFishScenarios(all);
	AddVortexScenarios(all);
	AddAdvisorScenarios(all);
	AddBirdScenarios(all);
	AddSharkScenarios(all);
	AddAnimalWalkScenarios(all);
	AddVillageTotemScenarios(all);
	AddSkyScenarios(all);
	AddKnockScenarios(all);
	AddFlowersScenarios(all);
	AddHandDemoScenarios(all);
	AddBallScenarios(all);
	AddBeliefScenarios(all);
	AddVirtualInfluenceScenarios(all);
	AddSeedThrowScenarios(all);
	AddTempleScenarios(all);
	AddCreatureExplorationScenarios(all);
	return all;
}

bool InRange(float value, float low, float high)
{
	return value >= low && value <= high;
}

void CheckNeeds(const NeedOverrides& needs, std::vector<std::string>& problems, std::string_view who)
{
	const std::array<std::pair<std::string_view, std::optional<float>>, 5> k_UnitNeeds {{
	    {"exhaustion", needs.exhaustion},
	    {"dehydration", needs.dehydration},
	    {"poo", needs.poo},
	    {"life", needs.life},
	    {"energy", needs.energy},
	}};
	for (const auto& [name, value] : k_UnitNeeds)
	{
		if (value.has_value() && !InRange(*value, 0.0f, creature_physiology::k_MaxGrownSize))
		{
			problems.push_back(fmt::format("{}: {} {} out of range", who, name, *value));
		}
	}
	if (needs.warmth.has_value() && !InRange(*needs.warmth, -1.0f, 1.0f))
	{
		problems.push_back(fmt::format("{}: warmth {} out of range", who, *needs.warmth));
	}
}

bool ValidAnimation(Kind kind, size_t animation)
{
	switch (kind)
	{
	case Kind::PlayAction:
		return animation >= animations::k_FirstAction && animation < animations::k_FirstAction + animations::k_ActionCount;
	case Kind::PlayGesture:
		return animation >= animations::k_FirstGesture && animation < animations::k_FirstGesture + animations::k_GestureCount;
	case Kind::PullFace:
		return animation >= animations::k_FirstFace && animation < animations::k_FirstFace + animations::k_FaceCount;
	default:
		return true;
	}
}

bool ValidOffset(glm::vec2 offset)
{
	return std::abs(offset.x) <= k_MaxOffset && std::abs(offset.y) <= k_MaxOffset;
}

/// What is wrong with what a command acts on, if anything: the object it picks up must be a thing, a pot or pile, an
/// animal or a villager (the creature's own tests of what it can pick up decide the rest), what it knocks down a thing
/// or a tree,
/// the part it strokes, the way it looks something over and the leash put on real ones, the walker a villager and the
/// store a building
std::string_view CommandProblem(const Command& command, std::span<const ObjectSetup> objects)
{
	const auto* object = command.object < objects.size() ? &objects[command.object] : nullptr;
	switch (command.kind)
	{
	case Kind::PickUp:
		return object != nullptr &&
		               (std::holds_alternative<MobileObjectInfo>(object->type) ||
		                std::holds_alternative<PotInfo>(object->type) || std::holds_alternative<AnimalInfo>(object->type) ||
		                std::holds_alternative<VillagerInfo>(object->type))
		           ? ""
		           : "picks up no thing, pot, animal or villager";
	case Kind::KnockDown:
		return object != nullptr && !std::holds_alternative<FeatureInfo>(object->type) ? "" : "knocks down no thing or tree";
	case Kind::TieLeash:
		return object != nullptr ? "" : "ties the leash to nothing";
	case Kind::Examine:
		return command.value < creature_object_actions::k_KeepAnimationCount ? "" : "no such way of looking it over";
	case Kind::HandStroke:
		return command.bodyPart < creature_feedback::k_BodyPartCount ? "" : "no such part of the body";
	case Kind::HandSlap:
		return command.slapHeight > 0.0f && command.slapHeight < creature_feedback::k_SlapAbove ? "" : "slaps above it";
	case Kind::PutOnLeash:
		return command.leash == LeashType::Rope || command.leash == LeashType::Evil || command.leash == LeashType::Good
		           ? ""
		           : "no such leash";
	case Kind::ConfineToHome:
		return command.radius > 0.0f ? "" : "keeps it nowhere";
	case Kind::LeashKey:
		return command.value < 3 ? "" : "no such leash key";
	case Kind::HandTapLeash:
		return command.player < PlayerNames::_COUNT ? "" : "no such player";
	case Kind::FightBlow:
		return command.value < 3 && command.chargeMs >= 0.0f && command.chargeMs <= creature_fight::k_MaxChargeMs
		           ? ""
		           : "no such blow or charge";
	case Kind::FightStep:
		return command.value < 4 ? "" : "no such step";
	case Kind::FightCast:
		return command.value > 0 && command.value <= static_cast<size_t>(MagicType::CreatureSpellItchy) ? "" : "no such spell";
	case Kind::VillagerWalkTo:
		return object != nullptr && std::holds_alternative<VillagerInfo>(object->type) ? "" : "walks no villager";
	case Kind::SpiritOut:
	case Kind::SpiritHome:
		return command.value < static_cast<size_t>(help::spirits::k_Dudes) ? "" : "no such spirit";
	case Kind::SpiritSay:
		return command.value < static_cast<size_t>(help::spirits::k_Dudes) && command.spiritLine >= 1
		           ? ""
		           : "no such spirit or line";
	case Kind::CreateWorshipSite:
		return command.value < static_cast<size_t>(Tribe::_COUNT) ? "" : "no such tribe";
	case Kind::WideScreen:
		return command.value < 2 ? "" : "the bars neither come on nor go";
	case Kind::ShowText:
		return command.value < helptext::k_TextCount ? "" : "no such text";
	case Kind::PlayFilm:
		return command.value < 2 ? "" : "no such film";
	case Kind::ScriptReward:
		return command.value >= 1 && command.value <= 60 ? "" : "no such reward";
	case Kind::ScriptDance:
		if (command.value >= k_DanceTypes)
		{
			return "no such dance";
		}
		return !command.danceAround.has_value() || *command.danceAround < objects.size() ? "" : "dances around no object";
	case Kind::ScriptDanceAttach:
		return object != nullptr ? "" : "attaches no object to the dance";
	case Kind::ScriptInterfaceLevel:
		return command.value < k_InterfaceLevels ? "" : "no such interface level";
	case Kind::ScriptChallengeSnapshot:
		return command.value < 3 ? "" : "no such snapshot";
	case Kind::TownGainsBelief:
		return command.amount > 0.0f ? "" : "gains no belief";
	case Kind::ScriptWalkPath:
		return object != nullptr && InRange(command.pathFrom, 0.0f, 1.0f) && InRange(command.pathTo, 0.0f, 1.0f)
		           ? ""
		           : "walks no object, or past the track's ends";
	case Kind::CreateVortex:
		return command.value < 3 ? "" : "no such land vortex";
	case Kind::StoreTakes:
		return object != nullptr && command.value != command.object && command.value < objects.size() &&
		               std::holds_alternative<AbodeInfo>(objects[command.value].type)
		           ? ""
		           : "takes nothing, or into no building";
	default:
		return "";
	}
}
} // namespace

namespace
{
/// Every command, one row each in the order of Command::Kind: the name it is logged by, whether it is the player's alone
/// and needs no creature, and whether it is the player's mouse (which needs none either). A new command is a new value
/// of the enum, before Count, and a new row here
constexpr auto k_CommandKinds = std::to_array<CommandKindInfo>({
    {.kind = Kind::WalkTo, .name = "walk to"},
    {.kind = Kind::RunTo, .name = "run to"},
    {.kind = Kind::Follow, .name = "follow"},
    {.kind = Kind::FleeFrom, .name = "flee from"},
    {.kind = Kind::TurnToFace, .name = "turn to face"},
    {.kind = Kind::FaceCamera, .name = "face the camera"},
    {.kind = Kind::Stop, .name = "stop"},
    {.kind = Kind::PlayAction, .name = "play action"},
    {.kind = Kind::PlayGesture, .name = "gesture"},
    {.kind = Kind::PullFace, .name = "pull face"},
    {.kind = Kind::ShowFeeling, .name = "show feeling"},
    {.kind = Kind::SitDown, .name = "sit down"},
    {.kind = Kind::StandUp, .name = "stand up"},
    {.kind = Kind::Sleep, .name = "sleep"},
    {.kind = Kind::Wake, .name = "wake"},
    {.kind = Kind::Eat, .name = "eat"},
    {.kind = Kind::Drink, .name = "drink"},
    {.kind = Kind::Poo, .name = "poo"},
    {.kind = Kind::Puke, .name = "puke"},
    {.kind = Kind::Faint, .name = "faint"},
    {.kind = Kind::Stroke, .name = "stroke"},
    {.kind = Kind::Slap, .name = "slap"},
    {.kind = Kind::SetHour, .name = "set hour", .needsNoCreature = true},
    {.kind = Kind::PickUp, .name = "pick up"},
    {.kind = Kind::PutDown, .name = "put down"},
    {.kind = Kind::Discard, .name = "toss away"},
    {.kind = Kind::Lob, .name = "lob"},
    {.kind = Kind::EatHeld, .name = "eat it"},
    {.kind = Kind::Examine, .name = "look it over"},
    {.kind = Kind::ThrowAt, .name = "throw at"},
    {.kind = Kind::KnockDown, .name = "knock down"},
    {.kind = Kind::PointAt, .name = "point at"},
    {.kind = Kind::HandStroke, .name = "hand strokes"},
    {.kind = Kind::HandSlap, .name = "hand slaps"},
    {.kind = Kind::HandLetGo, .name = "hand lets go"},
    {.kind = Kind::PutOnLeash, .name = "put on leash"},
    {.kind = Kind::TieLeash, .name = "tie leash to"},
    {.kind = Kind::UntieLeash, .name = "untie leash"},
    {.kind = Kind::TakeOffLeash, .name = "take off leash"},
    {.kind = Kind::ConfineToHome, .name = "keep at home"},
    {.kind = Kind::MakeLeashable, .name = "make leashable"},
    {.kind = Kind::HandTapLeash, .name = "hand taps to leash"},
    {.kind = Kind::LeashKey, .name = "leash key"},
    {.kind = Kind::LeashShake, .name = "shake the hand"},
    {.kind = Kind::StartFight, .name = "start fight"},
    {.kind = Kind::FightBlow, .name = "fight blow"},
    {.kind = Kind::FightBlock, .name = "fight block"},
    {.kind = Kind::FightStep, .name = "fight step"},
    {.kind = Kind::FightSpecial, .name = "fight special"},
    {.kind = Kind::FightAuto, .name = "fight by itself"},
    {.kind = Kind::FightCast, .name = "fight cast"},
    {.kind = Kind::KnockOut, .name = "knock out"},
    {.kind = Kind::BringRound, .name = "bring round"},
    {.kind = Kind::TieLeashToCreature, .name = "tie leash to creature"},
    {.kind = Kind::SetDesire, .name = "set desire"},
    {.kind = Kind::SetPhase, .name = "set stage"},
    {.kind = Kind::RewardIf, .name = "reward if"},
    {.kind = Kind::SeeSkill, .name = "see skill"},
    {.kind = Kind::SeeMiracle, .name = "see miracle"},
    {.kind = Kind::PlayerDid, .name = "player did"},
    {.kind = Kind::CreatureKey, .name = "press C"},
    {.kind = Kind::DoubleClick, .name = "double click"},
    {.kind = Kind::CameraKeys, .name = "camera keys"},
    {.kind = Kind::ClearCameraView, .name = "clear the view"},
    {.kind = Kind::LeaveCreatureMode, .name = "give the camera back"},
    {.kind = Kind::OpenCreatureCave, .name = "press F5"},
    {.kind = Kind::ApplyTattoo, .name = "tattoo"},
    {.kind = Kind::RemoveTattoo, .name = "take tattoo off"},
    {.kind = Kind::HoldSeed, .name = "hold seed", .needsNoCreature = true},
    {.kind = Kind::DrawGesture, .name = "draw gesture", .needsNoCreature = true},
    {.kind = Kind::SummonSeed, .name = "summon seed", .needsNoCreature = true},
    {.kind = Kind::KnowMiracle, .name = "know miracle"},
    {.kind = Kind::CastMiracle, .name = "cast miracle"},
    {.kind = Kind::PressKey, .name = "press key", .needsNoCreature = true},
    {.kind = Kind::HandTakeFireBall, .name = "hand takes fireball", .needsNoCreature = true},
    {.kind = Kind::PointerTo, .name = "pointer to", .pointer = true},
    {.kind = Kind::PointerPress, .name = "press button", .pointer = true},
    {.kind = Kind::PointerRelease, .name = "let go of button", .pointer = true},
    {.kind = Kind::PointerSweep, .name = "move mouse", .pointer = true},
    {.kind = Kind::WheelTurn, .name = "turn wheel", .pointer = true},
    {.kind = Kind::SetAlignment, .name = "set alignment", .needsNoCreature = true},
    {.kind = Kind::ChangeLand, .name = "change land", .needsNoCreature = true},
    {.kind = Kind::VillagerWalkTo, .name = "villager walk to", .needsNoCreature = true},
    {.kind = Kind::StoreTakes, .name = "store takes", .needsNoCreature = true},
    {.kind = Kind::SetGameSpeed, .name = "set game speed", .needsNoCreature = true},
    {.kind = Kind::PointerDoubleClick, .name = "pointer double click", .pointer = true},
    {.kind = Kind::SetOpenClose, .name = "open or close", .needsNoCreature = true},
    {.kind = Kind::LayGateStone, .name = "lay gate stone", .needsNoCreature = true},
    {.kind = Kind::SpiritOut, .name = "spirit flies out", .needsNoCreature = true},
    {.kind = Kind::SpiritHome, .name = "spirit flies home", .needsNoCreature = true},
    {.kind = Kind::CreateWorshipSite, .name = "create worship site", .needsNoCreature = true},
    {.kind = Kind::WideScreen, .name = "wide screen", .needsNoCreature = true},
    {.kind = Kind::ShowText, .name = "show text", .needsNoCreature = true},
    {.kind = Kind::PlayFilm, .name = "play film", .needsNoCreature = true},
    {.kind = Kind::CreateVortex, .name = "create vortex", .needsNoCreature = true},
    {.kind = Kind::VortexFadeIn, .name = "vortex fades in", .needsNoCreature = true},
    {.kind = Kind::VortexFadeOut, .name = "vortex fades out", .needsNoCreature = true},
    {.kind = Kind::ScriptFade, .name = "script fade", .needsNoCreature = true},
    {.kind = Kind::HighDetail, .name = "high detail", .needsNoCreature = true},
    {.kind = Kind::KeyDown, .name = "key down", .needsNoCreature = true},
    {.kind = Kind::KeyUp, .name = "key up", .needsNoCreature = true},
    {.kind = Kind::IdlePolicy, .name = "idle policy", .needsNoCreature = true},
    {.kind = Kind::ScriptFocus, .name = "script focus"},
    {.kind = Kind::ScriptRelease, .name = "script release"},
    {.kind = Kind::ScriptHealth, .name = "script health", .needsNoCreature = true},
    {.kind = Kind::ScriptTotemProperty, .name = "script totem property", .needsNoCreature = true},
    {.kind = Kind::LeashState, .name = "leash state"},
    {.kind = Kind::ScriptReward, .name = "script reward", .needsNoCreature = true},
    {.kind = Kind::ScriptDance, .name = "script dance", .needsNoCreature = true},
    {.kind = Kind::TapHighlight, .name = "tap highlight", .needsNoCreature = true},
    {.kind = Kind::ScriptDanceAttach, .name = "script dance attach", .needsNoCreature = true},
    {.kind = Kind::ScriptInterfaceLevel, .name = "script interface level", .needsNoCreature = true},
    {.kind = Kind::TempleBuilt, .name = "temple built", .needsNoCreature = true},
    {.kind = Kind::OpenTattooEditor, .name = "open tattoo editor", .needsNoCreature = true},
    {.kind = Kind::Advisor, .name = "tell an advisor", .needsNoCreature = true},
    {.kind = Kind::AbodeBuilt, .name = "abode built", .needsNoCreature = true},
    {.kind = Kind::RemoveCreature, .name = "remove from the game"},
    {.kind = Kind::LeaveTemple, .name = "leave temple", .needsNoCreature = true},
    {.kind = Kind::ScriptPopulate, .name = "script populate", .needsNoCreature = true},
    {.kind = Kind::ScriptPlay, .name = "script play"},
    {.kind = Kind::ScriptPlayed, .name = "script played"},
    {.kind = Kind::ScriptMove, .name = "script move"},
    {.kind = Kind::ScriptDoAction, .name = "script do action"},
    {.kind = Kind::SpiritSay, .name = "spirit says", .needsNoCreature = true},
    {.kind = Kind::ScriptMusic, .name = "script music", .needsNoCreature = true},
    {.kind = Kind::MusicState, .name = "music state", .needsNoCreature = true},
    {.kind = Kind::TownGainsBelief, .name = "town gains belief", .needsNoCreature = true},
    {.kind = Kind::ScriptDeleteTemple, .name = "script delete temple", .needsNoCreature = true},
    {.kind = Kind::ScriptWalkPath, .name = "script walk path", .needsNoCreature = true},
    {.kind = Kind::ScriptChallengeSnapshot, .name = "script challenge snapshot", .needsNoCreature = true},
    {.kind = Kind::ScriptDelete, .name = "script delete", .needsNoCreature = true},
    {.kind = Kind::ScriptSnapshot, .name = "script snapshot", .needsNoCreature = true},
    {.kind = Kind::HandTake, .name = "hand take", .needsNoCreature = true},
    {.kind = Kind::HandGive, .name = "hand give"},
    {.kind = Kind::ReadTip, .name = "read tip", .needsNoCreature = true},
    {.kind = Kind::LookAtTempleScroll, .name = "look at temple scroll", .needsNoCreature = true},
    {.kind = Kind::EndGuidanceWaits, .name = "end the guidance's waits", .needsNoCreature = true},
});

constexpr bool RowsFollowTheEnum()
{
	for (size_t i = 0; i < k_CommandKinds.size(); ++i)
	{
		if (static_cast<size_t>(k_CommandKinds.at(i).kind) != i)
		{
			return false;
		}
	}
	return k_CommandKinds.size() == static_cast<size_t>(Command::Kind::Count);
}
static_assert(RowsFollowTheEnum(), "the command table has one row for every Command::Kind, in its order");
} // namespace

std::span<const CommandKindInfo> testbed_scenarios::CommandKinds()
{
	return k_CommandKinds;
}

bool testbed_scenarios::NeedsNoCreature(Command::Kind kind)
{
	const auto& row = k_CommandKinds.at(static_cast<size_t>(kind));
	return row.needsNoCreature || row.pointer;
}

std::string_view testbed_scenarios::Name(Facet facet)
{
	constexpr std::array<std::string_view, k_FacetCount> k_Names {
	    "Idle",      "Expressions",   "Senses",  "Needs",    "Growth",   "Appearance", "Light",     "Movement", "Footprints",
	    "Audio",     "Objects",       "Hand",    "Leash",    "Combat",   "Mind",       "Particles", "Editor",   "Miracles",
	    "Benchmark", "Creature Mode", "Physics", "Examples", "Advisors", "Sky",        "Animals",
	};
	return k_Names.at(static_cast<size_t>(facet));
}

std::string_view testbed_scenarios::Name(Weather weather)
{
	constexpr std::array<std::string_view, 5> k_Names {"clear", "rain", "thunderstorm", "snow", "blizzard"};
	return k_Names.at(static_cast<size_t>(weather));
}

std::string_view testbed_scenarios::Name(Shot shot)
{
	constexpr std::array<std::string_view, 5> k_Names {"testbed", "overview", "follow", "head", "placed"};
	return k_Names.at(static_cast<size_t>(shot));
}

std::string_view testbed_scenarios::Name(Command::Kind kind)
{
	return k_CommandKinds.at(static_cast<size_t>(kind)).name;
}

bool testbed_scenarios::IsPointerCommand(Command::Kind kind)
{
	return k_CommandKinds.at(static_cast<size_t>(kind)).pointer;
}

bool NeedOverrides::Empty() const
{
	return !energy && !exhaustion && !dehydration && !poo && !life && !warmth && !age;
}

std::span<const Scenario> testbed_scenarios::All()
{
	static const std::vector<Scenario> k_All = Build();
	return k_All;
}

const Scenario* testbed_scenarios::Find(std::string_view id)
{
	const auto all = All();
	const auto found = std::ranges::find(all, id, &Scenario::id);
	return found != all.end() ? &*found : nullptr;
}

std::vector<std::string> testbed_scenarios::Problems(const Scenario& scenario)
{
	std::vector<std::string> problems;
	if (scenario.id.empty() || scenario.name.empty() || scenario.description.empty() || scenario.expected.empty())
	{
		problems.emplace_back("missing its id, name, description or what to expect");
	}
	if (static_cast<size_t>(scenario.facet) >= k_FacetCount)
	{
		problems.emplace_back("no such facet");
	}
	const auto& environment = scenario.environment;
	if (!InRange(environment.hour, 0.0f, k_HoursPerDay) || environment.bodyTimeScale < 0.0f)
	{
		problems.emplace_back("hour or body time out of range");
	}
	if (scenario.creatures.empty() && scenario.particles.empty() && scenario.miracles.empty() && scenario.dispensers.empty() &&
	    !environment.dispenserGrid && !scenario.crowd.has_value() && !environment.playerAlignment.has_value() &&
	    scenario.throws.empty() && scenario.objects.empty() && testbed_fixtures::Empty(scenario.fixtures) &&
	    scenario.birdFlocks.empty() &&
	    std::ranges::none_of(scenario.commands, [](const Command& command) { return NeedsNoCreature(command.kind); }))
	{
		problems.emplace_back("no creatures, things, particles, miracles, dispensers, crowd, fixtures, player's commands or "
		                      "alignment for the hand");
	}
	if ((environment.playerAlignment && !InRange(*environment.playerAlignment, -1.0f, 1.0f)) ||
	    (environment.cursor && (!InRange(environment.cursor->x, 0.0f, 1.0f) || !InRange(environment.cursor->y, 0.0f, 1.0f))))
	{
		problems.emplace_back("alignment or cursor out of range");
	}
	if (scenario.crowd.has_value() && (scenario.crowd->count == 0 || scenario.crowd->perFrame == 0))
	{
		problems.emplace_back("a crowd of no one, or spawned none a frame");
	}
	for (const auto& miracle : scenario.miracles)
	{
		const auto type = static_cast<size_t>(miracle.type);
		if (type == 0 || type >= k_Miracles)
		{
			problems.emplace_back("a miracle of no such magic type");
		}
		if (miracle.target == MiracleCast::Target::Creature && miracle.creature >= scenario.creatures.size())
		{
			problems.emplace_back("a miracle cast on a creature that isn't there");
		}
		if (!ValidOffset(miracle.point) || !ValidOffset(miracle.handOffset) || miracle.delaySeconds < 0.0f ||
		    miracle.holdSeconds.value_or(0.0f) < 0.0f || miracle.repeatSeconds.value_or(1.0f) <= 0.0f)
		{
			problems.emplace_back("a miracle's points or times out of range");
		}
	}
	for (const auto& dispenser : scenario.dispensers)
	{
		const auto type = static_cast<size_t>(dispenser.type);
		if (type == 0 || type >= k_Miracles || !ValidOffset(dispenser.offset))
		{
			problems.emplace_back("a dispenser of no such magic type, or off the map");
		}
	}
	for (const auto& particle : scenario.particles)
	{
		if (particle.file.empty() && particles::ParticleTypeFile(particle.type).empty())
		{
			problems.emplace_back("a particle type that has no file");
		}
		if (particle.targetsCreatures && scenario.creatures.empty())
		{
			problems.emplace_back("a particle effect given creatures to act on, without creatures");
		}
		if (particle.magnitude < 0.0f || particle.restartSeconds < 0.0f)
		{
			problems.emplace_back("a particle effect's magnitude or restart out of range");
		}
	}
	// the creature fixtures are numbered after the scenario's own creatures, and commands may name them
	const auto creatures = scenario.creatures.size() + scenario.fixtures.creatures.size();
	const auto& framing = scenario.framing;
	if ((framing.shot == Shot::Follow || framing.shot == Shot::Head) && framing.creature >= creatures)
	{
		problems.emplace_back("the camera frames a creature that isn't there");
	}
	// A handful of bare towns at most, each on the field
	constexpr size_t k_MostBareTowns = 4;
	if (scenario.bareTowns.size() > k_MostBareTowns ||
	    !std::ranges::all_of(scenario.bareTowns, [](const BareTown& bare) { return ValidOffset(bare.offset); }) ||
	    !std::ranges::all_of(scenario.bareTowns,
	                         [](const BareTown& bare) { return !bare.belief || InRange(*bare.belief, 0.0f, 1.0f); }))
	{
		problems.emplace_back("a bare town is off the map, has a belief out of range, or there are too many");
	}
	if (framing.distance <= 0.0f || !std::ranges::all_of(framing.include, ValidOffset))
	{
		problems.emplace_back("the framing's distance or points are out of range");
	}
	for (size_t i = 0; i < scenario.creatures.size(); ++i)
	{
		const auto& creature = scenario.creatures.at(i);
		const auto who = fmt::format("creature {}", i);
		const auto species = static_cast<int32_t>(creature.species);
		if (species < 1 || species >= static_cast<int32_t>(CreatureType::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such species", who));
		}
		if (static_cast<size_t>(creature.owner) >= static_cast<size_t>(PlayerNames::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such owner", who));
		}
		if (!ValidOffset(creature.offset))
		{
			problems.push_back(fmt::format("{}: off the map", who));
		}
		if ((creature.alignment && !InRange(*creature.alignment, -1.0f, 1.0f)) ||
		    (creature.fatness && !InRange(*creature.fatness, 0.0f, 1.0f)) ||
		    (creature.strength && !InRange(*creature.strength, 0.0f, 1.0f)) ||
		    (creature.size && !(std::isfinite(*creature.size) && *creature.size >= 0.0f)))
		{
			problems.push_back(fmt::format("{}: body out of range", who));
		}
		if (!creature.mindFile.empty() && !creature.mindFile.starts_with("chosen:") && !creature.mindFile.starts_with("game:"))
		{
			problems.push_back(fmt::format("{}: a mind file is chosen: or game:", who));
		}
		if (creature.phase && *creature.phase > k_LastPhase)
		{
			problems.push_back(fmt::format("{}: no such stage of growing up", who));
		}
		CheckNeeds(creature.needs, problems, who);
		for (const auto& desire : creature.desires)
		{
			if (static_cast<size_t>(desire.desire) >= creature_desires::k_DesireCount || !InRange(desire.fraction, 0.0f, 1.0f))
			{
				problems.push_back(fmt::format("{}: desire out of range", who));
			}
		}
		for (const auto& tattoo : creature.tattoos)
		{
			if (tattoo.design >= creature_tattoo::k_DesignCount || tattoo.Empty())
			{
				problems.push_back(fmt::format("{}: no such tattoo design or site", who));
			}
		}
		if (creature.tattoos.size() > creature_tattoo::k_SlotCount)
		{
			problems.push_back(fmt::format("{}: more tattoos than slots", who));
		}
		for (const auto& mark : creature.wounds)
		{
			if (mark.type >= creature_marks::k_WoundLifetimes.size() || mark.column >= 8 || mark.skin >= 4)
			{
				problems.push_back(fmt::format("{}: no such wound", who));
			}
		}
	}
	for (const auto& object : scenario.objects)
	{
		const bool valid = std::visit(
		    []<typename T>(T type) {
			    const auto index = static_cast<int32_t>(type);
			    return index >= 0 && index < static_cast<int32_t>(T::_COUNT);
		    },
		    object.type);
		if (!valid || object.scale <= 0.0f || !ValidOffset(object.offset))
		{
			problems.emplace_back("an object of no such kind, size or place");
		}
		if (object.hungry && !std::holds_alternative<AnimalInfo>(object.type))
		{
			problems.emplace_back("a hungry object that is no animal");
		}
	}
	for (const auto& throwing : scenario.throws)
	{
		if (throwing.object >= scenario.objects.size() || (throwing.from.has_value() && !ValidOffset(*throwing.from)) ||
		    throwing.height < 0.0f)
		{
			problems.emplace_back("a throw of no such object, or from no such place");
		}
	}
	for (size_t i = 0; i < scenario.commands.size(); ++i)
	{
		const auto& command = scenario.commands.at(i);
		const auto what = fmt::format("command {} ({})", i, Name(command.kind));
		if (!NeedsNoCreature(command.kind) && command.creature >= creatures)
		{
			problems.push_back(fmt::format("{}: no such creature", what));
		}
		if (command.kind == Kind::SetAlignment && !InRange(command.alignment, -1.0f, 1.0f))
		{
			problems.push_back(fmt::format("{}: alignment out of range", what));
		}
		if (command.kind == Kind::TempleBuilt && !InRange(command.built, 0.0f, 1.0f))
		{
			problems.push_back(fmt::format("{}: the temple built past its ends", what));
		}
		if (command.kind == Kind::AbodeBuilt &&
		    (command.object >= scenario.objects.size() || !scenario.objects.at(command.object).underConstruction ||
		     !std::holds_alternative<AbodeInfo>(scenario.objects.at(command.object).type) ||
		     !InRange(command.built, 0.0f, 1.0f)))
		{
			problems.push_back(fmt::format("{}: builds no abode being built, or past its ends", what));
		}
		if (command.kind == Kind::SetGameSpeed && !(command.gameSpeed > 0.0f))
		{
			problems.push_back(fmt::format("{}: no game speed above 0", what));
		}
		if (((command.kind == Kind::SetOpenClose || command.kind == Kind::HighDetail || command.kind == Kind::ScriptHealth ||
		      command.kind == Kind::ScriptTotemProperty || command.kind == Kind::ScriptDelete) &&
		     command.object >= scenario.objects.size()) ||
		    (command.kind == Kind::LayGateStone &&
		     (command.object >= scenario.objects.size() || command.value >= scenario.objects.size())))
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if (command.kind == Kind::ChangeLand && (command.landScript.empty() || !scenario.fixtures.writesGameData))
		{
			problems.push_back(
			    fmt::format("{}: no land's script, or the scenario does not say it writes the game's data", what));
		}
		if (command.kind == Kind::TapHighlight && scenario.fixtures.highlights.empty())
		{
			problems.push_back(fmt::format("{}: taps no highlight, the scenario sets none out", what));
		}
		if ((command.kind == Kind::Follow || command.kind == Kind::StartFight || command.kind == Kind::TieLeashToCreature) &&
		    (command.value >= creatures || command.value == command.creature))
		{
			problems.push_back(fmt::format("{}: acts on no other creature", what));
		}
		if (!ValidAnimation(command.kind, command.value))
		{
			problems.push_back(fmt::format("{}: animation {} isn't of its kind", what, command.value));
		}
		if (const auto problem = CommandProblem(command, scenario.objects); !problem.empty())
		{
			problems.push_back(fmt::format("{}: {}", what, problem));
		}
		if (!ValidOffset(command.point) || command.delaySeconds < 0.0f || !InRange(command.hour, 0.0f, k_HoursPerDay))
		{
			problems.push_back(fmt::format("{}: point, delay or hour out of range", what));
		}
		if ((command.kind == Kind::SetDesire &&
		     (command.value >= creature_desires::k_DesireCount || !InRange(command.amount, 0.0f, 1.0f))) ||
		    (command.kind == Kind::SetPhase && command.value > k_LastPhase) ||
		    (command.kind == Kind::ShowFeeling && command.value >= creature_face::k_CueCount) ||
		    (command.kind == Kind::SeeSkill && command.value >= k_Skills) ||
		    ((command.kind == Kind::SeeMiracle || command.kind == Kind::KnowMiracle || command.kind == Kind::CastMiracle) &&
		     command.value >= k_Miracles) ||
		    (command.kind == Kind::CastMiracle &&
		     (command.atCreature.has_value() ? *command.atCreature >= creatures : command.object >= scenario.objects.size())) ||
		    (command.kind == Kind::PlayerDid && command.value >= k_Deeds) ||
		    (command.kind == Kind::CameraKeys && (command.value >= 4 || command.amount <= 0.0f)) ||
		    (command.kind == Kind::PointerTo &&
		     (!InRange(command.point.x, 0.0f, 1.0f) || !InRange(command.point.y, 0.0f, 1.0f))) ||
		    ((command.kind == Kind::PointerPress || command.kind == Kind::PointerRelease) &&
		     (command.value < 1 || command.value > 3)) ||
		    (command.kind == Kind::PointerSweep && command.amount <= 0.0f) ||
		    (command.kind == Kind::OpenCreatureCave && command.value >= creature_cave::k_PageCount) ||
		    ((command.kind == Kind::ApplyTattoo || command.kind == Kind::RemoveTattoo) &&
		     (command.value >= creature_tattoo::k_DesignCount || command.bodyPart >= creature_tattoo::k_SlotCount)) ||
		    ((command.kind == Kind::HoldSeed || command.kind == Kind::SummonSeed) && command.value >= k_SeedCount) ||
		    (command.kind == Kind::DrawGesture && (command.value == 0 || command.value > k_LastGesture)))
		{
			problems.push_back(fmt::format("{}: value {} out of range", what, command.value));
		}
	}
	if (scenario.repeatFrom.has_value() && *scenario.repeatFrom >= scenario.commands.size())
	{
		problems.emplace_back("repeats from a command that isn't there");
	}
	return problems;
}

glm::vec2 testbed_scenarios::MapPoint(glm::vec2 middle, glm::vec2 offset)
{
	return middle + offset;
}

float testbed_scenarios::CreatureHeight(float size)
{
	// A creature of size 1 stands about 15 units tall, whatever its species' mesh
	constexpr float k_HeightPerSize = 15.0f;
	return k_HeightPerSize * std::max(size, 0.05f);
}

testbed_scenarios::HandLightReading testbed_scenarios::ReadHandLight(uint32_t landColour)
{
	const auto rgb = landColour & 0xFFFFFFu;
	const auto sum = static_cast<int32_t>(((rgb >> 16) & 0xFFu) + ((rgb >> 8) & 0xFFu) + (rgb & 0xFFu));
	// As the night lights take it: the colour as fractions, the gate and the strength from them
	const auto colour = LandLightTable::ToColour(rgb);
	return {
	    .landColour = rgb,
	    .sum = sum,
	    .mean = sum / 3,
	    .dark = night_lights::IsDark(colour),
	    .strength = graphics::HandLight::GetStrength(colour),
	};
}

std::string testbed_scenarios::Describe(const HandLightReading& reading, float hour)
{
	return fmt::format("the hour is {:.3f}: the land's colour {:06X}, sum {}, mean {}, {}, the hand light's strength {:.4f}",
	                   hour, reading.landColour, reading.sum, reading.mean, reading.dark ? "dark" : "not dark",
	                   reading.strength);
}

std::optional<testbed_scenarios::SunGlareReading>
testbed_scenarios::ReadSunGlare(float hour, glm::vec3 camera, float nearClip,
                                const std::function<bool(const glm::vec3&)>& landInTheWay)
{
	const auto placement = graphics::sun::Place(hour);
	if (!placement)
	{
		return std::nullopt;
	}
	// The points the glare looks at, and the offset each comes from, as the renderer takes them
	const auto points = graphics::sun::GlareSamplePoints(placement->position, camera, nearClip);
	SunGlareReading reading {.sunHeight = placement->position.y, .samples = points.size()};
	for (size_t i = 0; i < points.size(); ++i)
	{
		if (landInTheWay(points.at(i)))
		{
			reading.hiddenByLand.push_back(graphics::sun::k_GlareSamples.at(i));
		}
	}
	return reading;
}

std::string testbed_scenarios::Describe(const SunGlareReading& reading)
{
	std::vector<std::string> offsets;
	offsets.reserve(reading.hiddenByLand.size());
	for (const auto& offset : reading.hiddenByLand)
	{
		offsets.push_back(fmt::format("({:.0f}, {:.0f})", offset.x, offset.y));
	}
	return fmt::format("the sun stands {:.0f} high: the land hides {} of the glare's {} samples{}{}", reading.sunHeight,
	                   reading.hiddenByLand.size(), reading.samples, offsets.empty() ? "" : ", ", fmt::join(offsets, ", "));
}

CameraPlacement testbed_scenarios::Overview(glm::vec3 centre, glm::vec2 halfSize, glm::vec2 fieldOfView, float distanceFactor)
{
	// Looking down from the south at this angle, far enough back for the box's width to fit across the view and its
	// depth, foreshortened, up and down it, with a margin round it for the creatures' heights
	constexpr float k_Pitch = 0.65f;
	constexpr float k_MinDistance = 40.0f;
	constexpr float k_Margin = 25.0f;
	const auto halfAngles = glm::clamp(fieldOfView * 0.5f, 0.1f, 1.5f);
	// The near side of the box is closer to the camera than its middle, so its width takes a wider angle
	const auto across = ((halfSize.x + k_Margin) / std::tan(halfAngles.x)) + ((halfSize.y + k_Margin) * std::cos(k_Pitch));
	const auto upAndDown = (halfSize.y + k_Margin) * std::sin(k_Pitch) / std::tan(halfAngles.y);
	const auto distance = distanceFactor * std::max({across, upAndDown, k_MinDistance});
	return {
	    .origin = centre + glm::vec3(0.0f, distance * std::sin(k_Pitch), -distance * std::cos(k_Pitch)),
	    .focus = centre,
	};
}

CameraPlacement testbed_scenarios::Follow(glm::vec3 position, float height, float distanceFactor)
{
	constexpr float k_MinDistance = 25.0f;
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	const auto distance = distanceFactor * std::max(height * 3.5f, k_MinDistance);
	return {.origin = focus + glm::vec3(0.0f, distance * 0.45f, -distance * 0.9f), .focus = focus};
}

CameraPlacement testbed_scenarios::Head(glm::vec3 position, glm::vec2 ahead, float height, float distanceFactor)
{
	const auto length = glm::length(ahead);
	const auto forward = length > 0.0f ? ahead / length : glm::vec2(0.0f, -1.0f);
	const glm::vec3 forward3 {forward.x, 0.0f, forward.y};
	// The head is high on the body and a little forward of its middle
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.75f, 0.0f) + (forward3 * (height * 0.3f));
	const auto distance = distanceFactor * height;
	return {.origin = focus + (forward3 * distance) + glm::vec3(0.0f, height * 0.15f, 0.0f), .focus = focus};
}

Bounds testbed_scenarios::BoundsOf(std::span<const glm::vec2> points, glm::vec2 minimumHalfSize)
{
	if (points.empty())
	{
		return {.centre = glm::vec2(0.0f), .halfSize = minimumHalfSize};
	}
	auto low = points.front();
	auto high = points.front();
	for (const auto& point : points)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	return {.centre = (low + high) * 0.5f, .halfSize = glm::max((high - low) * 0.5f, minimumHalfSize)};
}

void testbed_scenarios::Apply(const NeedOverrides& overrides, creature_physiology::Needs& needs, float size)
{
	const auto set = [](float& value, const std::optional<float>& wanted, float low, float high) {
		if (wanted.has_value())
		{
			value = std::clamp(*wanted, low, high);
		}
	};
	set(needs.energy, overrides.energy, 0.0f, std::max(1.0f, size));
	set(needs.exhaustion, overrides.exhaustion, 0.0f, 1.0f);
	set(needs.dehydration, overrides.dehydration, 0.0f, 1.0f);
	set(needs.poo, overrides.poo, 0.0f, 1.0f);
	set(needs.life, overrides.life, 0.0f, 1.0f);
	set(needs.warmth, overrides.warmth, -1.0f, 1.0f);
	if (overrides.age.has_value())
	{
		needs.age = *overrides.age;
	}
}

void testbed_scenarios::Apply(std::span<const DesireOverride> overrides, creature_desires::Desires& desires)
{
	for (const auto& override : overrides)
	{
		auto& state = desires[override.desire];
		state.activated = true;
		state.value = std::clamp(override.fraction, 0.0f, 1.0f) * std::max(state.max, 0.0f);
	}
}

std::vector<size_t> testbed_scenarios::Advance(Timeline& timeline, std::span<const Command> commands,
                                               std::optional<size_t> repeatFrom, float seconds,
                                               const std::function<bool(const Command& command)>& isFree)
{
	std::vector<size_t> due;
	if (commands.empty() || timeline.next >= commands.size())
	{
		timeline.done = true;
	}
	if (timeline.done)
	{
		return due;
	}
	timeline.seconds += seconds;
	timeline.sinceGiven += seconds;
	// At most a round of the commands at once, so commands that go round again without waiting can't run away
	while (!timeline.done && due.size() < commands.size())
	{
		const auto& command = commands[timeline.next];
		if (command.waitUntilFree && !timeline.freed)
		{
			if (timeline.sinceGiven < k_SettleSeconds || !isFree(command))
			{
				// Its delay counts from when the creature is free
				timeline.seconds = 0.0f;
				break;
			}
			timeline.freed = true;
		}
		if (timeline.seconds < command.delaySeconds)
		{
			break;
		}
		due.push_back(timeline.next);
		timeline.seconds = 0.0f;
		timeline.sinceGiven = 0.0f;
		timeline.freed = false;
		if (++timeline.next >= commands.size())
		{
			if (repeatFrom.has_value() && *repeatFrom < commands.size())
			{
				timeline.next = *repeatFrom;
			}
			else
			{
				timeline.done = true;
			}
		}
	}
	return due;
}

bool testbed_scenarios::KeepsDispenserGrid(const Scenario& scenario)
{
	if (!scenario.environment.dispenserGrid)
	{
		return false;
	}
	const auto onGrid = [](const auto& setup) { return testbed_dispensers::InGridArea(setup.offset); };
	return std::ranges::none_of(scenario.creatures, onGrid) && std::ranges::none_of(scenario.objects, onGrid);
}
