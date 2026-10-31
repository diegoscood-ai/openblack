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
#include "TestbedLandTimeline.h"

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
	                "nothing of its own.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
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

	auto other = Content(CreatureType::Cow, {0.0f, -40.0f}, 180.0f, "other");
	other.owner = PlayerNames::PLAYER_TWO;
	all.push_back({
	    .id = "creature.inspect_creature",
	    .name = "A creature meeting another creature",
	    .facet = Facet::Senses,
	    .description = "The player's tiger stands facing south, towards the camera; another player's cow stands 40 metres "
	                   "south of it, facing north.",
	    .expected = "Nothing yet: no creature here makes the reaction that shows it to the others as it is made, so neither "
	                "notices the other this way. Once that is ported, each creature, as curious about the other as it "
	                "has learnt to be, drops what it was doing and goes up to the other to four times its own height, "
	                "turns to face it with a curious look and watches it for 5 to 15 seconds, then takes no further "
	                "notice of the same creature for a minute.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "tiger"), other},
	});
	all.push_back({
	    .id = "creature.look_at_flying",
	    .name = "A creature watching a thing thrown past it",
	    .facet = Facet::Senses,
	    .description = "The player's tiger stands facing south, towards the camera. Three seconds in, the player's hand "
	                   "throws a barrel from 20 metres to its left, 12 metres in front of it, across its view.",
	    .expected = "As the barrel flies, the tiger drops what it was doing; half the time it points at it two seconds, "
	                "amazed, its head following the barrel; then it watches it, amazed, while it is still in the air, "
	                "for no more than four seconds. It takes no notice of another thing thrown for a minute and a half.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -12.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "tiger")},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {-20.0f, -12.0f}}},
	    .throws = {{.object = 0, .height = 4.0f, .velocity = {10.0f, 10.0f, 0.0f}, .delaySeconds = 3.0f}},
	});
}

void AddNeeds(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "needs.on_fire_drinks",
	    .name = "A creature on fire goes to the sea",
	    .facet = Facet::Needs,
	    .description = "A player's tiger stands north of the middle, short of the testbed's lake, and catches fire. It "
	                   "does not know the water miracle.",
	    .expected = "At once it stops what it does and walks to the nearest edge of the lake, then bends down and drinks. "
	                "A player's creature walks, because it goes back to its walk speed as it stops what it did.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}}},
	    .commands = {{.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f}},
	});
	all.push_back({
	    .id = "needs.on_fire_drinks_neutral",
	    .name = "A creature of no player on fire runs to the sea",
	    .facet = Facet::Needs,
	    .description = "A tiger of no player stands north of the middle, short of the testbed's lake, and catches fire.",
	    .expected = "At once it stops what it does and runs to the nearest edge of the lake, then bends down and drinks.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}, .owner = PlayerNames::NEUTRAL}},
	    .commands = {{.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f}},
	});
	all.push_back({
	    .id = "needs.on_fire_casts_water",
	    .name = "A creature on fire casts water on itself",
	    .facet = Facet::Needs,
	    .description = "A player's tiger that knows the water miracle well stands north of the middle, and catches fire.",
	    .expected = "At once it stops what it does, finds a clear place near it and casts the water miracle on itself. "
	                "The fire on it goes out.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}}},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f}},
	});
	all.push_back({
	    .id = "needs.badly_hurt",
	    .name = "A badly hurt creature wants to restore its health",
	    .facet = Facet::Needs,
	    .description = "A player's tiger with three tenths of its life stands north of the middle.",
	    .expected = "Its desire to restore its health becomes the strongest for ten seconds, and most other desires stay "
	                "down. Each time that it stops what it does with low life, the log shows that it is badly hurt "
	                "again.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}, .needs = {.life = 0.3f}}},
	});
	all.push_back({
	    .id = "needs.town_fire_water",
	    .name = "A creature puts out a fire in the nearest town",
	    .facet = Facet::Needs,
	    .description = "A kind player's tiger that knows the water miracle well stands near a hut of the scenario's town, "
	                   "in the middle of the map. The hut catches fire.",
	    .expected = "At once it stops what it does, walks near the hut and casts the water miracle on it. The fire goes "
	                "out.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {40.0f, 44.0f},
	        .desires = {{.desire = Desire::Compassion, .fraction = 1.0f}, {.desire = Desire::Anger, .fraction = 0.0f}}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {66.0f, 44.0f}}},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f, .value = 1}},
	});

	// Played through the real input path only (tools/checks/real_input_tap.sh): no pointer commands, so the replayed
	// window events drive the hand
	all.push_back({
	    .id = "realinput.fire",
	    .name = "A burning tree carried by hand to a creature",
	    .facet = Facet::Needs,
	    .description = "A burning beech stands close in front of the camera, and a kind player's tiger that knows the water "
	                   "miracle well stands 40 m north of it, out of the fire's reach. Pick the tree up with the Action "
	                   "button and put it down by the tiger.",
	    .expected = "Out of the hand by the tiger the tree burns on, and the tiger takes up the fire: the log says which "
	                "way it went (\"Creature ... takes up the fire on ...\"). Caring for nothing a lone tree belongs to, "
	                "it runs from the burning tree.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 10.0f, -15.0f}, .look = {0.0f, 0.5f, 0.0f}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 40.0f},
	        .desires = {{.desire = Desire::Compassion, .fraction = 1.0f}, {.desire = Desire::Anger, .fraction = 0.0f}}}},
	    .objects = {{.type = TreeInfo::Beech, .offset = {0.0f, 0.0f}}},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)},
	         {.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f, .value = 1}},
	});

	// Played through the real input path only (tools/checks/real_input_tap.sh): no pointer commands, so the replayed
	// key press reaches the game
	all.push_back({
	    .id = "realinput.villager_names",
	    .name = "Villagers' names shown with the names key",
	    .facet = Facet::Appearance,
	    .description = "Four villagers of the player's town stand close in front of the camera; the farmer and the "
	                   "housewife are named villagers, given the first free name of their sex. Press the names key (N).",
	    .expected = "The names grow in over about a third of a second above the two named villagers' heads, each in a "
	                "dark see-through box with a pointer down to its villager, in the town owner's colour; the other "
	                "two show none. Pressing N again shrinks them away.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -14.0f}, .look = {0.0f, 1.5f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-3.0f, 0.0f}, .joinTown = true, .named = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {3.0f, 0.0f}, .joinTown = true, .named = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-1.0f, 4.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {1.5f, -2.0f}, .joinTown = true}},
	});

	all.push_back({
	    .id = "needs.thirst",
	    .name = "Thirsty creature finds water",
	    .facet = Facet::Needs,
	    .description = "A parched tiger starts north of the middle, short of the testbed's lake, with its desire for "
	                   "water as strong as it gets.",
	    .expected = "It walks down the bank to the nearest edge of the lake, bends down and drinks; its thirst clears "
	                "and the desire for water is held back for a while.",
	    .environment = {.lakeWaterPoint = true},
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
	    .environment = {.bodyTimeScale = 10.0f, .lakeWaterPoint = true},
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
	    .environment = {.bodyTimeScale = 10.0f, .lakeWaterPoint = true},
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

	// A burning tree with cows by it, and a cow on fire: an animal of a kind that reacts to a fire runs from the burning
	// thing as from any frightening thing, a burning one from its own fire
	all.push_back({
	    .id = "animals.fire_flight",
	    .name = "Animals run from a fire",
	    .facet = Facet::Animals,
	    .description = "A beech set alight with a cow standing 6 m either side of it, and a third cow set alight on its "
	                   "own 30 m west of the tree.",
	    .expected = "As the tree catches, the two cows by it run from it; the burning cow runs from its own fire until it "
	                "burns to death.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 10.0f}, {15.0f, 40.0f}}, .distance = 0.4f},
	    .objects = {{.type = TreeInfo::Beech, .offset = {0.0f, 25.0f}},
	                {.type = AnimalInfo::Cow, .offset = {-6.0f, 25.0f}},
	                {.type = AnimalInfo::Cow, .offset = {6.0f, 25.0f}},
	                {.type = AnimalInfo::Cow, .offset = {-30.0f, 25.0f}}},
	    .fixtures = {.fires = {{.what = size_t {0}}, {.what = size_t {3}}}},
	});

	// A good tiger that only wants to eat, with villagers near it. An evil tiger that only wants to be nice, by a hut of
	// its town that a fireball sets alight. The minds run, and the desires stay as given.
	auto eater = Content(CreatureType::Tiger, {-60.0f, 0.0f}, 0.0f, "good, hungry");
	eater.alignment = 0.6f;
	eater.needs.energy = 0.2f;
	eater.desires = OnlyDesire(Desire::Hunger);
	auto helper = Content(CreatureType::Tiger, {73.0f, 49.0f}, 270.0f, "evil, compassionate");
	helper.alignment = -0.6f;
	helper.desires = OnlyDesire(Desire::Compassion);
	all.push_back({
	    .id = "appearance.deed_alignment",
	    .name = "Deeds move the alignment",
	    .facet = Facet::Appearance,
	    .description = "On the left, a good tiger that only wants to eat stands by six villagers. On the right, an evil "
	                   "tiger that only wants to be nice knows the water miracle and stands by a hut of its town. A "
	                   "fireball sets the hut alight.",
	    .expected = "Each turn that the left tiger eats a villager, its alignment moves to evil. Each turn that the right "
	                "tiger puts out the fire, its alignment moves to good. The change is slow: run the game fast "
	                "and watch the alignment in the spawner. The bodies and skins follow it.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {eater, helper},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-48.0f, -12.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-72.0f, -12.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-60.0f, -25.0f}},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {-90.0f, 10.0f}},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-30.0f, 10.0f}},
	                {.type = VillagerInfo::CelticTraderMale, .offset = {-60.0f, 30.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {66.0f, 44.0f}},
	                {.type = TreeInfo::Birch, .offset = {53.0f, 43.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {63.0f, 37.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {56.0f, 42.0f}, .joinTown = true}},
	    .miracles = {{.type = MagicType::Fireball,
	                  .point = {66.0f, 44.0f},
	                  .handOffset = {66.0f, 34.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 4.0f}},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 1, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Water)}},
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

	// The rain's splashes, as raffclar's testbed shows them
	for (const bool night : {false, true})
	{
		all.push_back({
		    .id = night ? "light.rain_splashes_night" : "light.rain_splashes",
		    .name = night ? "Rain splashes at night" : "Rain splashes on the land and the lake",
		    .facet = Facet::Light,
		    .description = night ? "A heavy rain falls over the whole island at eleven at night. The camera looks low over "
		                           "the grass, a birch, the shallows and the open water of the testbed's lake."
		                         : "A heavy rain falls over the whole island at noon. The camera looks low over the grass, a "
		                           "birch, the shallows and the open water of the testbed's lake.",
		    .expected = night ? "Small rings come and go on the grass and on the water where the rain falls. The rings "
		                        "take the dark colour of the land's light at night, so they show less than by day."
		                      : "Small rings come and go on the grass and on the water where the rain falls. Each ring "
		                        "grows and fades in less than a second. The rings are white by day.",
		    .environment =
		        {.hour = night ? 23.0f : 12.0f, .clockRuns = false, .weather = Weather::Rain, .dispenserGrid = false},
		    .framing = {.shot = Shot::Placed,
		                .eye = {k_Lake.x, 6.0f, k_NearShallows - 30.0f},
		                .look = {k_Lake.x, 0.0f, k_NearShallows + 10.0f}},
		    .objects = {{.type = TreeInfo::Birch, .offset = {k_Lake.x + 8.0f, k_NearShallows - 14.0f}}},
		});
	}
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
	    .commands = {{.kind = Kind::OpenTempleRooms, .delaySeconds = 12.0f},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 0.0f}},
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
	    .id = "objects.pickup_kinds",
	    .name = "The kinds of thing a creature picks up",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with, in front of it from left to right, a toy ball, a rock, an idol, a birch and a "
	                   "magic mushroom. It picks up and throws the toy ball and then the rock to the west, tries to pick up "
	                   "the idol, pulls up the birch and puts it down, then picks up the mushroom and puts it down.",
	    .expected = "The toy ball and the rock are lifted and thrown (the rock if it is light enough for the tiger). The "
	                "idol is refused at once: of the statics only toys are picked up. The birch creaks as it comes out of "
	                "the ground and is put down again. The mushroom is lifted and put down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {25.0f, -15.0f}, {-60.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-12.0f, -k_Reach}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {-6.0f, -k_Reach}},
	                {.type = MobileStaticInfo::Idol, .offset = {0.0f, -k_Reach}},
	                {.type = TreeInfo::Birch, .offset = {6.0f, -k_Reach}},
	                {.type = FeatureInfo::MagicMushroom, .offset = {12.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Go(Kind::ThrowAt, 0, {-60.0f, -10.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 1, 1.0f), Go(Kind::ThrowAt, 0, {-60.0f, 10.0f}, 4.0f),
	                 OnObject(Kind::PickUp, 0, 2, 1.0f), OnObject(Kind::PickUp, 0, 3, 1.0f), Act(Kind::PutDown, 0, 1.0f, true),
	                 OnObject(Kind::PickUp, 0, 4, 1.0f), Act(Kind::PutDown, 0, 1.0f, true)},
	});

	// The actions a creature does with particular kinds of thing, each told by a script's CREATURE_DO_ACTION by its row
	// of the game's table: giving wood from a tree to a storage pit (32), planting a tree (249), throwing a living thing
	// about (263), hurling (15) and throwing a ball at something (291)
	{
		constexpr size_t k_GiveWoodFromTreeToStoragePit = 32;
		constexpr size_t k_PlantTree = 249;
		constexpr size_t k_ThrowAround = 263;
		constexpr size_t k_Hurl = 15;
		constexpr size_t k_ThrowBallAtObject = 291;
		all.push_back({
		    .id = "objects.give_wood",
		    .name = "Giving a tree to a store as wood",
		    .facet = Facet::Objects,
		    .description = "A grown tiger, a birch to its left and its town's empty storage pit off to the east. A script "
		                   "tells it to give wood from the birch to the pit, as the creature does by itself out of "
		                   "compassion for a town short of wood.",
		    .expected = "It walks to the birch and pulls it up (the tree creaks), carries it towards the pit until it is "
		                "near enough to throw, turns to face the pit with a look of compassion and throws the tree in. "
		                "Whether the pit's wood goes up is what this scenario checks: the log's pit lines say.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
		                {.type = AbodeInfo::CelticStoragePit, .offset = {90.0f, 0.0f}, .storedFood = 0, .storedWood = 0},
		                {.type = VillagerInfo::CelticFarmerMale, .offset = {100.0f, -12.0f}, .joinTown = true}},
		    .commands = {{.kind = Kind::ScriptDoAction,
		                  .delaySeconds = 1.0f,
		                  .value = k_GiveWoodFromTreeToStoragePit,
		                  .object = 1,
		                  .with = 0}},
		});
		all.push_back({
		    .id = "objects.plant_tree",
		    .name = "Planting a tree by something",
		    .facet = Facet::Objects,
		    .description = "A grown tiger, a birch in front of it and a rock off to the north-east. A script tells it to "
		                   "plant the birch by the rock, as the creature does by itself out of compassion for a town.",
		    .expected = "It pulls up the birch (the tree creaks), walks right up to the rock, turns to face it and puts the "
		                "birch down beside it. Whether the tree takes root there is what this scenario checks.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {50.0f, 50.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = TreeInfo::Birch, .offset = {0.0f, -k_Reach}},
		                {.type = MobileStaticInfo::RockChalk, .offset = {50.0f, 50.0f}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_PlantTree, .object = 1, .with = 0}},
		});
		all.push_back({
		    .id = "objects.kind_throws",
		    .name = "Throwing the kinds of thing about",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with a villager, a rock and a toy ball in front of it, a hut off to the north and "
		                   "a birch to the north-east. A script tells it in turn to throw the villager about, to hurl the "
		                   "rock at the hut, and to throw the toy ball at the birch.",
		    .expected = "It picks up the villager and throws her off to one side. It picks up the rock, goes to where it "
		                "can throw at the hut and hurls the rock at it, angry. Half the time it jumps for joy, then picks "
		                "up the toy ball and throws it at the birch, angry, sometimes jumping for joy again.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -15.0f}, {0.0f, -90.0f}, {50.0f, -70.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-12.0f, -k_Reach}},
		                {.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}},
		                {.type = MobileStaticInfo::ToyBall, .offset = {12.0f, -k_Reach}},
		                {.type = AbodeInfo::CelticHut, .offset = {0.0f, -90.0f}},
		                {.type = TreeInfo::Birch, .offset = {50.0f, -70.0f}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_ThrowAround, .object = 0},
		                 {.kind = Kind::ScriptDoAction,
		                  .delaySeconds = 6.0f,
		                  .waitUntilFree = true,
		                  .value = k_Hurl,
		                  .object = 3,
		                  .with = 1},
		                 {.kind = Kind::ScriptDoAction,
		                  .delaySeconds = 6.0f,
		                  .waitUntilFree = true,
		                  .value = k_ThrowBallAtObject,
		                  .object = 4,
		                  .with = 2}},
		});
		// The punishment lesson's throw with the hand already full: the hurl picks up what it is given only when it
		// holds nothing, so it throws what it holds instead, and the lesson that waits for the villager to fly waits on
		constexpr size_t k_StrokeToyFirst = 323;
		all.push_back({
		    .id = "objects.hurl_hand_full",
		    .name = "Told to hurl a villager while holding a toy",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with a cuddly toy in front of it, a villager to its left and a rock off to the "
		                   "north-east. A script tells it to stroke the toy, which it keeps in its hand, then, as the "
		                   "creature's punishment lesson does with its trainer, to hurl the villager at the rock.",
		    .expected = "It picks the toy up and strokes it. Told to hurl the villager, it does not pick her up, since "
		                "its hand is full: it goes to where it can throw at the rock and throws the toy at it, angry. "
		                "The villager never leaves the ground, as in the original game.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {40.0f, -50.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -k_Reach}},
		                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-12.0f, -k_Reach}},
		                {.type = MobileStaticInfo::RockChalk, .offset = {40.0f, -50.0f}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_StrokeToyFirst, .object = 0},
		                 {.kind = Kind::ScriptDoAction,
		                  .delaySeconds = 1.0f,
		                  .waitUntilFree = true,
		                  .value = k_Hurl,
		                  .object = 2,
		                  .with = 1}},
		});
	}

	// The second set: wood to a building site (247), mending a house (35), kicking a ball about (290), breaking a rock
	// (266), smashing a stone in half (159), stroking a toy (323), throwing a die (324), taking a toy home (322) and a
	// throwing game with the player (171), each told by a script's CREATURE_DO_ACTION by its row
	{
		constexpr size_t k_GiveWoodFromTreeToBuildingSite = 247;
		constexpr size_t k_HelpRepairHouse = 35;
		constexpr size_t k_KickBallAround = 290;
		constexpr size_t k_BreakRock = 266;
		constexpr size_t k_SmashStonesInHalf = 159;
		constexpr size_t k_StrokeToy = 323;
		constexpr size_t k_ThrowDie = 324;
		constexpr size_t k_TakeToyHome = 322;
		constexpr size_t k_PlayThrowingGameWithPlayer = 171;
		all.push_back({
		    .id = "objects.give_wood_site",
		    .name = "Giving a tree to a damaged building as wood",
		    .facet = Facet::Objects,
		    .description = "A grown tiger, a birch to its left and its town's hut off to the east, hurt to six tenths of its "
		                   "life so that its town wants it mended. A script tells it to give wood from the birch to the "
		                   "building's site.",
		    .expected = "It walks to the birch and pulls it up, carries it towards the hut until it is near enough to throw, "
		                "turns to face it with a look of compassion and throws the tree in. Whether the hut's site gets the "
		                "wood is what this scenario checks.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
		                {.type = AbodeInfo::CelticHut, .offset = {90.0f, 0.0f}, .life = 0.6f}},
		    .commands = {{.kind = Kind::ScriptDoAction,
		                  .delaySeconds = 1.0f,
		                  .value = k_GiveWoodFromTreeToBuildingSite,
		                  .object = 1,
		                  .with = 0}},
		});
		all.push_back({
		    .id = "objects.repair_house",
		    .name = "Helping repair a house",
		    .facet = Facet::Objects,
		    .description = "A grown tiger, a birch to its left and its town's hut off to the east, hurt to six tenths of its "
		                   "life. A script tells it to help repair the hut with the birch.",
		    .expected = "It walks to the birch and picks it up, walks right up to the hut, turns to it and puts the tree "
		                "down by it; the hut gets a fifth of its life back (to eight tenths) and the tree is gone. Whether "
		                "the repair ends at full life is what this scenario checks.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
		                {.type = AbodeInfo::CelticHut, .offset = {90.0f, 0.0f}, .life = 0.6f}},
		    .commands =
		        {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_HelpRepairHouse, .object = 1, .with = 0}},
		});
		all.push_back({
		    .id = "objects.kick_ball",
		    .name = "Kicking a ball about",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with a toy ball in front of it. A script tells it to kick the ball about.",
		    .expected = "It walks up to the ball, turns to it and kicks it: the ball flies off ahead of it, half upwards, "
		                "and bounces away. A quarter of the time it then points at the ball a second, else it waits up to a "
		                "second. It goes after the ball and kicks it again, two to four times in all, sometimes jumping for "
		                "joy at the end. The kicks make no sound.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 10.0f}, {20.0f, -150.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_KickBallAround, .object = 0}},
		});
		all.push_back({
		    .id = "objects.break_rock",
		    .name = "Breaking a rock",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with a rock in front of it. A script tells it to break the rock.",
		    .expected = "It picks the rock up and throws it off to one side; as the rock flies it breaks into two halves, "
		                "which fly on as it flew and come down apart.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {80.0f, 80.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_BreakRock, .object = 0}},
		});
		all.push_back({
		    .id = "objects.smash_stone",
		    .name = "Smashing a stone in half",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with a rock in front of it. A script tells it to smash the stone in half.",
		    .expected = "Angry, it reaches for the rock with a blow and stamps on it: the rock breaks into two halves with "
		                "a smashing sound.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_SmashStonesInHalf, .object = 0}},
		});
		all.push_back({
		    .id = "objects.stroke_toy",
		    .name = "Stroking a cuddly toy",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with a cuddly toy in front of it. A script tells it to stroke the toy.",
		    .expected = "With a look of compassion it picks the toy up and strokes it in its hand, looking at it, then "
		                "keeps holding it.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -k_Reach}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_StrokeToy, .object = 0}},
		});
		all.push_back({
		    .id = "objects.throw_die",
		    .name = "Throwing a die",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with a die in front of it. A script tells it to throw the die.",
		    .expected = "It picks the die up and throws it a little way off to one side, watches it fly, amazed, until it "
		                "lands (three to four seconds at most), then jumps for joy two times in three, else looks sad.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyDie, .offset = {0.0f, -k_Reach}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_ThrowDie, .object = 0}},
		});
		all.push_back({
		    .id = "objects.take_toy_home",
		    .name = "Taking a toy home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger given a home where it stands, with a toy ball 80 away to the north. A script "
		                   "tells it to take the toy home.",
		    .expected = "With a look of compassion it walks to the ball and picks it up, walks back to its home and sets "
		                "the ball down within five of it; now and then it tosses it away instead.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 15.0f}, {15.0f, -90.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -80.0f}}},
		    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 400.0f},
		                 {.kind = Kind::ScriptDoAction, .delaySeconds = 1.0f, .value = k_TakeToyHome, .object = 0}},
		});
		all.push_back({
		    .id = "objects.throw_to_player",
		    .name = "Throwing a toy to the player",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's with a toy ball in front of it, the hand over the land before "
		                   "it. A script tells it to play a throwing game with the player with the ball (the script's "
		                   "do-action needs a thing to act on: the ball is both).",
		    .expected = "It picks the ball up and throws it at the player's hand, where the hand was as it set about it, "
		                "looking at it; then it jumps for joy.",
		    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
		    .commands = {{.kind = Kind::ScriptDoAction,
		                  .delaySeconds = 4.0f,
		                  .value = k_PlayThrowingGameWithPlayer,
		                  .object = 0,
		                  .with = 0}},
		});
	}

	auto stoner = Content(CreatureType::Tiger, {0.0f, 0.0f});
	stoner.desires = {{.desire = creature_desires::Desire::GetHigh, .fraction = 1.0f}};
	all.push_back({
	    .id = "objects.magic_mushroom",
	    .name = "Eating a magic mushroom",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a magic mushroom in front of it, wanting to get high as much as it can. It "
	                   "picks the mushroom up and eats it by itself.",
	    .expected = "Once it has eaten it, it becomes high: amazed, it turns to the camera and talks; its eyes go wide and "
	                "dart about, and smiling it stares at the camera, plays dazed and confused actions, half the time "
	                "jumps for joy, and half the time walks to a clear place, sits there sadly ten to fifteen seconds and "
	                "starts up frightened. At last it turns to the camera amazed, talks and plays a dazed action again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {stoner},
	    .objects = {{.type = FeatureInfo::MagicMushroom, .offset = {0.0f, -k_Reach}}},
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

	// A one-shot globe floating on its own in front of the tiger, which it picks up and eats from its hand
	{
		constexpr glm::vec2 k_Globe {0.0f, -k_Reach};
		const testbed_land_timeline::ThingNear globe {
		    .type = script::ObjectType::MobileObject, .point = k_Globe, .radius = 4.0f};
		all.push_back({
		    .id = "objects.eat_a_globe",
		    .name = "Eating a one-shot globe",
		    .facet = Facet::Objects,
		    .description = "A food globe floats on its own ten metres in front of a hungry tiger. The tiger picks it up "
		                   "and eats it from its hand.",
		    .expected = "The globe goes into its hand with the horn of plenty spinning inside, and as it bites both go "
		                "from sight together; once the eating is over the globe is gone, and nothing of it, the seed "
		                "inside included, is left in the air or on the land.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {hungry},
		    .dispensers = {{.type = MagicType::Food, .offset = k_Globe, .bubbleHeight = 3.0f}},
		    .commands = {{.kind = Kind::PickUp, .creature = 0, .delaySeconds = 1.0f, .waitUntilFree = true, .thing = globe},
		                 Act(Kind::EatHeld, 0, 0.5f, true)},
		});
	}

	all.push_back({
	    .id = "objects.knock_down_trees",
	    .name = "Knocking down trees",
	    .facet = Facet::Objects,
	    .description = "A tiger walks up to each of three trees in a row and knocks it down.",
	    .expected = "It strikes each tree, which stays standing: a knock-down breaks homes, not trees, as in the game. "
	                "Its town's view of it shows in the spawner.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}},
	                {.type = TreeInfo::Beech, .offset = {0.0f, 10.0f}},
	                {.type = TreeInfo::Conifer, .offset = {40.0f, 10.0f}}},
	    .commands = {OnObject(Kind::KnockDown, 0, 0, 1.0f), OnObject(Kind::KnockDown, 0, 1, 1.0f),
	                 OnObject(Kind::KnockDown, 0, 2, 1.0f)},
	});

	all.push_back({
	    .id = "objects.kick_tree",
	    .name = "Kicking a tree",
	    .facet = Facet::Objects,
	    .description = "A tiger kicks the beech standing in front of it.",
	    .expected = "The beech flies off ahead of it, half up and half ahead, and lands lying down as a dead tree, as a "
	                "thrown tree does. It is not felled as a forester fells a tree.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Beech, .offset = {0.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::Kick, 0, 0, 1.0f)},
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

	// The game's table row of stealing a thing and taking it to the temple, and the player's temple far to the west,
	// turned as Land 1's (36 radians, in the map script's thousandths)
	constexpr size_t k_StealObjectAndTakeToCitadel = 265;
	all.push_back({
	    .id = "objects.steal_toy",
	    .name = "Stealing a toy",
	    .facet = Facet::Objects,
	    .description = "A grown tiger of the player's, whose temple stands far to the west, with a cuddly toy in front "
	                   "of it, out of the influence of the player. A script tells it to steal the toy and take it to the "
	                   "temple.",
	    .expected = "It picks the toy up and looks at it, walks home and looks ahead, and puts the toy down within two "
	                "times its height of its home, the pen before the temple.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::ScriptDoAction,
	                  .creature = 0,
	                  .delaySeconds = 1.0f,
	                  .value = k_StealObjectAndTakeToCitadel,
	                  .object = 0}},
	    .fixtures = {.temples = {{.at = glm::vec2 {-300.0f, 100.0f}, .rotation = 36000}}},
	});
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
		    .commands = {strokeNow(5.0f), Act(Kind::HandLetGo, 0, 0.3f, false)},
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
		    .commands = {Slap(0, 0.55f, true, false, 1.0f), Act(Kind::HandLetGo, 0, 0.3f, true),
		                 desire(Desire::RunAwayFromPlayer, 1.0f, 0.5f), desire(Desire::Fear, 0.0f, 0.0f),
		                 desire(Desire::Anger, 0.0f, 0.0f), Stroke(0, BodyPart::Head, 1.0f),
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

	// A tiger on the learning leash, untied, an oak and a resting rock the hand can lift, all in a town's influence. The
	// camera looks at each in turn so that the pointer in the middle of the screen is on it, and the right button is
	// clicked quickly there: the leash's order on a thing, through the grab state of the hand's press
	all.push_back({
	    .id = "leash.tap_thing_quick",
	    .name = "Sent to a tree and a rock with quick clicks",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash, not tied, with an oak to its north-west and a chalk rock to its "
	                   "north-east, in the influence of a Celtic town. The camera looks at the oak's trunk and the right "
	                   "button is pressed and let go 0.1 s later; then it looks at the rock and does the same.",
	    .expected = "Each quick click is a tap, as in the original: the log gives the grab of the oak (a tree's pull, "
	                "never held) and its end within 225 ms, then the tiger told to act on the oak, and it acknowledges "
	                "the order; the same for the rock, with its tug hold still on at the release. Nothing is held at any "
	                "time, the oak stays rooted and the tiger goes to each in turn.",
	    .framing = {.shot = Shot::Placed, .eye = {-10.0f, 8.0f, 18.0f}, .look = {-10.0f, 2.0f, 30.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-10.0f, 30.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {10.0f, 30.0f}},
	                {.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 48.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, 40.0f}, .joinTown = true}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::CameraTo, .delaySeconds = 4.0f, .eye = {10.0f, 6.0f, 20.0f}, .look = {10.0f, 0.5f, 30.0f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 Act(Kind::LeashState, 0, 4.0f)},
	    .townBelief = 10.0f,
	    .logHandState = true,
	    .logHandGrab = true,
	});

	// A tiger on the learning leash, untied, and the camera looking at open land north of it, so that the pointer in the
	// middle of the screen is on the land; a quick click there sends the tiger to the point, which its player sees marked
	all.push_back({
	    .id = "leash.tap_land_marker",
	    .name = "The marker over where the leash sent it",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash, not tied, with open land 30 m north of it. The camera looks at that "
	                   "land and the right button is pressed and let go 0.1 s later.",
	    .expected = "As in the original: the tiger acknowledges the order and walks to the point. Over the point, 2 m up, "
	                "a yellow ring facing the camera holds the tiger's paw print, turned a quarter turn, both throbbing with "
	                "the camera bookmarks' markers, until the walk is over. The leash panel's \"Target marker\" line gives "
	                "its place, the ring's size and footprint picture 4.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, -10.0f}, .look = {0.0f, 0.0f, 30.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 Act(Kind::LeashState, 0, 3.0f)},
	    .logHandState = true,
	});

	// A singing stone the hand can lift, resting in a town's influence, clicked quickly and then a little longer: the
	// tap's window of a resting thing, the tug hold
	all.push_back({
	    .id = "objects.tap_stone_quick",
	    .name = "A singing stone clicked quickly, then held a little",
	    .facet = Facet::Objects,
	    .description = "A singing stone (the first, pickupable) rests in the influence of a Celtic town, the camera looking "
	                   "at it. The right button is pressed on it and let go 0.08 s later; three seconds on, pressed again "
	                   "and let go 0.2 s later.",
	    .expected = "The first click is a tap: the log gives the grab of the stone with its tug hold on until the "
	                "release, then the stone as the thing clicked, and nothing held. The second press is taken into the "
	                "hand only if the log's tug hold went off at least one frame before the release (the hold ends 0.13 s "
	                "of frames into the press, and the stone is taken the frame after); otherwise it too is a tap.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 20.0f}, .look = {0.0f, 0.5f, 30.0f}},
	    .objects = {{.type = MobileStaticInfo::SingingStone_1, .offset = {0.0f, 30.0f}},
	                {.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 48.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, 40.0f}, .joinTown = true}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.08f, .value = 3},
	                 {.kind = Kind::PointerPress, .delaySeconds = 3.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3}},
	    .townBelief = 10.0f,
	    .logHandState = true,
	    .logHandGrab = true,
	});

	// The same stone with no pointer commands, for the real input path: a recorded mouse (tools/checks/real_input_tap.sh)
	// or a person's clicks the stone through the window's events
	all.push_back({
	    .id = "objects.tap_stone_by_hand",
	    .name = "A singing stone to click by hand",
	    .facet = Facet::Objects,
	    .description = "A singing stone (the first, pickupable) rests in the influence of a Celtic town, the camera looking "
	                   "at it from the south so that it is in the middle of the screen. Nothing drives the pointer: click it "
	                   "with the Action button, quickly, then hold it a little.",
	    .expected = "A quick click is a tap: the stone stays where it stands, the hand tugs and nothing shows in it, and the "
	                "log gives the stone as the thing clicked. A press held past the tug hold takes it into the hand.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 20.0f}, .look = {0.0f, 0.5f, 30.0f}},
	    .objects = {{.type = MobileStaticInfo::SingingStone_1, .offset = {0.0f, 30.0f}},
	                {.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 48.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, 40.0f}, .joinTown = true}},
	    .townBelief = 10.0f,
	    .logHandState = true,
	    .logHandGrab = true,
	});

	// The hand's acts the lands' quests ask of the player, set out for the real input path: a recorded mouse
	// (tools/checks/realinput/hand_quests.txt through tools/checks/real_input_tap.sh) or a person's clicks, through the
	// window's events, with no pointer commands. A fixed camera keeps each thing at its own point of the 1024x768 screen,
	// none in front of another; the town centre, which gives the town and most of its influence, stands behind the camera,
	// and the two villagers are told to stand where they are made, so that they stay where they are
	all.push_back({
	    .id = "realinput.hand_quests",
	    .name = "The hand's quest acts, to do by hand",
	    .facet = Facet::Objects,
	    .description = "A Celtic town believing in the player, its town centre behind the camera, with a singing stone, a "
	                   "villager standing still, a chalk rock, a pillar of rock far beyond it, a pile of food and an empty "
	                   "storage pit, seen from the south. Nothing drives the pointer: with the Action button, tap the "
	                   "stone; pick the villager up and put it down gently on open land; pick the rock up and throw it "
	                   "towards the pillar; scoop food from the pile and press the handful onto the storage pit.",
	    .expected = "A second in, the log gives each object's foot, middle and top on the screen. Then it gives each act "
	                "through the hand: the stone as the thing clicked with nothing held, the villager and then the rock "
	                "held (hand state 24), let go, the villager put down and the rock thrown, a handful of food held and "
	                "then nothing held as the pit takes it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 13.0f, 0.0f}, .look = {0.0f, 0.0f, 22.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, -30.0f}},
	                {.type = MobileStaticInfo::SingingStone_1, .offset = {-8.0f, 12.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {2.0f, 12.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, -22.0f}, .joinTown = true},
	                {.type = MobileStaticInfo::RockChalk, .offset = {-2.0f, 22.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {-22.0f, 54.0f}},
	                {.type = PotInfo::FoodPile, .offset = {9.0f, 14.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {16.0f, 34.0f}, .storedFood = 0, .storedWood = 0}},
	    .commands = {{.kind = Kind::VillagerWalkTo, .point = {2.0f, 12.0f}, .object = 2},
	                 {.kind = Kind::VillagerWalkTo, .point = {8.0f, -22.0f}, .object = 3}},
	    .townBelief = 10.0f,
	    .logHandState = true,
	    .logHandGrab = true,
	    .logScreenPoints = true,
	});

	// The hand's own acts, page by page, set out for the real input path as realinput.hand_quests is (a recorded mouse,
	// tools/checks/realinput/hand_pages.txt through tools/checks/real_input_tap.sh, or a person's clicks), with no
	// pointer commands. The same fixed camera, each thing in its own part of the screen and none in front of another;
	// the town centre behind the camera. The log gives the tooltip shown with its words, the hand's clip and the camera,
	// so that hovering, tapping, pulling, scooping and gripping the land each say what they did. The beech is at its
	// full size: at half its size it weighs an eighth as much, and the press at its grip pulls it straight out
	all.push_back({
	    .id = "realinput.hand_pages",
	    .name = "The hand's acts, to do by hand",
	    .facet = Facet::Hand,
	    .description = "A Celtic town believing in the player, its town centre behind the camera, with a hut far ahead, a "
	                   "villager standing still, a chalk rock, a beech, a pile of food and a storage pit holding "
	                   "food and wood, seen from the south. Nothing drives the pointer: with the Action button, hover over "
	                   "the land and each thing; knock on the hut twice; tap the rock; pull at the beech and let go, then "
	                   "pull it up by its crown and plant it on open land; pick the villager up and throw it; scoop food "
	                   "from the pile and pour it out; rest at the left and top edges; then grip the land with the Move "
	                   "button and drag it, and double click the land.",
	    .expected = "A second in, the log gives each object's foot, middle and top on the screen. Then each act through "
	                "the hand: the tooltip and the hand's clip over the land, each thing and the edges; the hut tapped "
	                "with the tap-house clip and two knocking sounds; the rock split; the beech pulled and left rooted, "
	                "then pulled free, held and replanted; the villager held and thrown; a handful scooped, its amount "
	                "growing every turn, and put down as a pile; the land gripped (hand state 20, clip Cgrip) with the "
	                "camera following, and the camera's flight to the land double clicked.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 13.0f, 0.0f}, .look = {0.0f, 0.0f, 22.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, -30.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-4.0f, 44.0f}, .yawDegrees = 180.0f},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {2.0f, 12.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, -22.0f}, .joinTown = true},
	                {.type = MobileStaticInfo::RockChalk, .offset = {12.0f, 22.0f}},
	                {.type = TreeInfo::Beech, .offset = {-9.0f, 14.0f}},
	                {.type = PotInfo::FoodPile, .offset = {8.0f, 13.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {24.0f, 44.0f}, .storedFood = 300, .storedWood = 200}},
	    .commands = {{.kind = Kind::VillagerWalkTo, .point = {2.0f, 12.0f}, .object = 2},
	                 {.kind = Kind::VillagerWalkTo, .point = {8.0f, -22.0f}, .object = 3}},
	    .townBelief = 10.0f,
	    .logHandState = true,
	    .logHandGrab = true,
	    .logScreenPoints = true,
	    .logToolTip = true,
	    .logHandClip = true,
	    .logCamera = true,
	});

	// A scaffold held over open land of the player's town, set out for the real input path (a recorded mouse through
	// tools/checks/real_input_tap.sh, or a person's clicks), with no pointer commands: the see-through ghost of the
	// building it would make there stands at the hand. The scaffold came out of the yard of the workshop behind it, which
	// still keeps its place. The fixed camera and the town centre behind it are realinput.hand_quests' own, so each thing
	// is at its own point of the 1024x768 screen
	all.push_back({
	    .id = "realinput.scaffold_phantom",
	    .name = "The ghost of a held scaffold's building, by hand",
	    .facet = Facet::Objects,
	    .description = "A Celtic town believing in the player, its town centre behind the camera, with a workshop at the "
	                   "back left and, in front of it, a scaffold worth one that was taken out of the workshop's yard, two "
	                   "villagers told to stand behind the camera, seen from the south. Nothing drives the pointer: with "
	                   "the Action button, pick the scaffold up and hold it still over the open land to the right.",
	    .expected = "A second in, the log gives the scaffold's foot, middle and top on the screen. Then the hand holds it "
	                "(hand state 24). Held still over the open land, a see-through ghost of the town building it would "
	                "make grows in at the hand over four turns and turns slowly.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 13.0f, 0.0f}, .look = {0.0f, 0.0f, 22.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, -30.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = {-14.0f, 34.0f}},
	                {.type = MobileObjectInfo::OldScaffold, .offset = {-6.0f, 14.0f}, .takenFromYardOf = 1},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, -22.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, -22.0f}, .joinTown = true}},
	    .commands = {{.kind = Kind::VillagerWalkTo, .point = {-8.0f, -22.0f}, .object = 3},
	                 {.kind = Kind::VillagerWalkTo, .point = {8.0f, -22.0f}, .object = 4}},
	    .townBelief = 10.0f,
	    .logHandState = true,
	    .logHandGrab = true,
	    .logScreenPoints = true,
	});
	// The same with a Norse town, its town centre, workshop and villagers Norse: a scaffold worth one held over its open
	// land offers the Norse hut, the long low wooden house (the first of the houses worth one, all equally wanted)
	all.push_back({
	    .id = "realinput.scaffold_phantom_norse",
	    .name = "The ghost of a held scaffold's building, by hand, in a Norse town",
	    .facet = Facet::Objects,
	    .description = "realinput.scaffold_phantom with a Norse town: its town centre behind the camera, a workshop at the "
	                   "back left and, in front of it, a scaffold worth one that was taken out of the workshop's yard, two "
	                   "villagers told to stand behind the camera, seen from the south. Nothing drives the pointer: with "
	                   "the Action button, pick the scaffold up and hold it still over the open land to the right.",
	    .expected = "A second in, the log gives the scaffold's foot, middle and top on the screen. Then the hand holds it "
	                "(hand state 24). Held still over the open land, a see-through ghost of the Norse hut grows in at the "
	                "hand over four turns and turns slowly.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 13.0f, 0.0f}, .look = {0.0f, 0.0f, 22.0f}},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = {0.0f, -30.0f}},
	                {.type = AbodeInfo::NorseWorkshop, .offset = {-14.0f, 34.0f}},
	                {.type = MobileObjectInfo::OldScaffold, .offset = {-6.0f, 14.0f}, .takenFromYardOf = 1},
	                {.type = VillagerInfo::NorseFarmerMale, .offset = {-8.0f, -22.0f}, .joinTown = true},
	                {.type = VillagerInfo::NorseHousewifeFemale, .offset = {8.0f, -22.0f}, .joinTown = true}},
	    .commands = {{.kind = Kind::VillagerWalkTo, .point = {-8.0f, -22.0f}, .object = 3},
	                 {.kind = Kind::VillagerWalkTo, .point = {8.0f, -22.0f}, .object = 4}},
	    .townBelief = 10.0f,
	    .townTribe = Tribe::NORSE,
	    .logHandState = true,
	    .logHandGrab = true,
	    .logScreenPoints = true,
	});

	// A reward chest tapped through the real input path (tools/checks/realinput/clicks.txt through
	// tools/checks/real_input_tap.sh), with no pointer commands: a toy ball chest put down on the open land a second in,
	// as a challenge's script puts one down without dropping it, in hand_quests' camera and at a point of its screen
	all.push_back({
	    .id = "realinput.clicks",
	    .name = "A reward chest to tap by hand",
	    .facet = Facet::Objects,
	    .description = "A toy ball reward chest is put down on the open land 24 m north of the camera's foot a second in, "
	                   "seen from the south as in realinput.hand_quests. Nothing drives the pointer: with the Action "
	                   "button, tap the closed chest, then tap the open one.",
	    .expected = "The first tap plays the tap sound and the two opening sounds, and the closed chest gives way to the "
	                "open one, the toy ball beside it; the second tap, on the open chest, plays the tap sound only. The "
	                "log gives the thing clicked and the hand's state each time.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 13.0f, 0.0f}, .look = {0.0f, 0.0f, 22.0f}},
	    .commands = {{.kind = Kind::ScriptReward,
	                  .delaySeconds = 1.0f,
	                  .point = {0.0f, 24.0f},
	                  .value = static_cast<size_t>(RewardObjectInfo::RewardObjectToyBall)}},
	    .logHandState = true,
	    .logHandGrab = true,
	});

	// A tiger on the learning leash tied to a pillar of rock straight north of it, facing it, and the camera south of the
	// tiger in the same north-south line, looking north past the pillar's foot. The tied tiger wanders within its
	// rope, so the pointer rests where the rope meets the pillar, the end that does not move. The hand takes hold of
	// the rope, pulls it towards the camera and lets go
	all.push_back({
	    .id = "leash.tied_pull",
	    .name = "A tied leash's rope pulled and let go",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash, facing a pillar of rock 40 m north of it, tied to it. The camera "
	                   "stands 20 m south of the tiger, 30 m up, looking north at the land 30 m beyond the tiger, so "
	                   "the rope runs to the pillar in the middle of the screen. The pointer rests at the rope's end on "
	                   "the pillar, which stays put while the tied tiger wanders; the "
	                   "right button goes down, the pointer moves a quarter of the screen down over a second and the "
	                   "button is let go half a second later.",
	    .expected = "As in the original: with the pointer on the rope the log gives the rope under the hand and the "
	                "hand's clip Chold_fingers; the press takes hold of the rope (action state 21, the leash pulled: "
	                "two more ropes run from the hand to the tiger's collar and to the pillar). Let go, the state is "
	                "back to 0, the tied lengths are the hand's distance from the pillar (at least twice its radius) "
	                "and half of it, far below the 180 / 90 it was tied with, and the tiger is sent to the land under "
	                "the hand.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, -20.0f}, .look = {0.0f, 0.0f, 30.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 40.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.5f, 0.328125f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, 0.25f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 Act(Kind::LeashState, 0, 4.0f)},
	    .logHandState = true,
	    .logLeashPull = true,
	});

	// leash.tied_pull's tiger, pillar and camera, with the leash put on, tied and its rope taken hold of through the
	// mouse alone: a click on the tiger, the pointer on the pillar where the rope will meet it, a click and a double
	// click there, then a press on the rope's end that is held, the pointer drawn down, until the shot
	constexpr testbed_land_timeline::ThingNear k_Tiger {.type = script::ObjectType::Creature, .point = {0.0f, 0.0f}};
	all.push_back({
	    .id = "leash.tied_pull_held",
	    .name = "A tied leash's rope held by the hand",
	    .facet = Facet::Leash,
	    .description = "A tiger facing a pillar of rock 40 m north of it, the camera as in leash.tied_pull. The right "
	                   "button clicks the tiger; then the pointer rests on the pillar, the button is clicked, then "
	                   "clicked again as a double click; four seconds later it goes down on the rope's end at the "
	                   "pillar, the pointer moves a quarter of the screen down over a second and the button stays down "
	                   "to the end.",
	    .expected = "As in the original: the click puts the learning leash on, the double click ties it to the "
	                "pillar. The press takes hold of the rope (action state 21) and from then on the leash is drawn "
	                "as two ropes from the hand, one to the tiger's collar and one to the pillar, with no rope "
	                "straight from the pillar to the tiger; each rope as solid as the tied rope was before the press, "
	                "faded only at its two ends.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, -20.0f}, .look = {0.0f, 0.0f, 30.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 40.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_Tiger},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3, .thing = k_Tiger},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 Act(Kind::LeashState, 0, 1.0f),
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.328125f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerDoubleClick, .delaySeconds = 0.1f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 Act(Kind::LeashState, 0, 1.0f),
	                 {.kind = Kind::PointerPress, .delaySeconds = 4.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, 0.25f}, .amount = 1.0f},
	                 Act(Kind::LeashState, 0, 2.0f)},
	    .logHandState = true,
	    .logLeashPull = true,
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
	    .expected = "The rope goes on, changes colour three times, each a turn after its key, and comes off; the "
	                "readout says each step was sent. Pressing L, V and B yourself does the same.",
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
	                 {.kind = Kind::OpenTempleRooms, .delaySeconds = 1.0f},
	                 {.kind = Kind::OpenCreatureCave, .creature = 0, .delaySeconds = 0.0f},
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
	    .id = "combat.player_clicks",
	    .name = "Directing the player's creature with the mouse",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger fights a blue tiger, and neither fights by itself, as in Land 1's fight "
	                   "lesson. Once they duel and the camera watches them, the mouse is put on the blue tiger's middle "
	                   "and the Action (right) button is clicked there and held 0.4 seconds; then on the red tiger's "
	                   "middle and clicked; then on the blue tiger's head with the grab (left) button. The clicks go "
	                   "through the same input as the player's own.",
	    .expected = "Each click goes to the fight and to nothing else: the hand takes hold of neither tiger. The click "
	                "on the blue tiger's middle makes the red one strike there, stepping in first when out of reach, and "
	                "the blue tiger reels; the click on the red tiger makes it block; the grab button's click on the "
	                "blue tiger's head strikes high in place of anything queued. The log shows the hand's action state "
	                "16 in the fight, 22 or 23 while a click is held.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
	                 Fight(Kind::FightAuto, 0, 0, 0.2f),
	                 Fight(Kind::FightAuto, 1, 0, 0.0f),
	                 {.kind = Kind::PointerTo, .delaySeconds = 8.0f, .amount = 0.5f, .atCreature = 1},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.3f, .value = 3, .amount = 0.5f, .atCreature = 1},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.4f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .amount = 0.5f, .atCreature = 0},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.3f, .value = 3, .amount = 0.5f, .atCreature = 0},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .amount = 0.85f, .atCreature = 1},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.3f, .value = 1, .amount = 0.85f, .atCreature = 1},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 1}},
	});

	all.push_back({
	    .id = "combat.fight_miracle_gesture",
	    .name = "Casting a fight miracle with the mouse",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger, which knows the lightning bolt, fights a blue tiger, and neither fights by "
	                   "itself, as in Land 1's fight lesson. Once they duel and the camera watches them, the lightning's "
	                   "gesture (the fork up) is drawn with the mouse across the middle of the screen, with no button "
	                   "held, through the same input as the player's own; six seconds later, the special move's (the "
	                   "star).",
	    .expected = "The gesture is recognised as the fight's lightning: its trail shows, a mark appears on the blue tiger "
	                "where the line from the red tiger's rear to the blue one's meets it, and the gesture window gives "
	                "it as sent to the fight. At the next turn the red tiger raises its arms and casts the lightning at "
	                "the blue tiger, which reels and loses fight health. The star is recognised as the special move: its "
	                "trail shows, another mark appears on the blue tiger, and the red tiger strikes its special move "
	                "at it. The log shows the hand's action state 16.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
	    .commands = {{.kind = Kind::KnowMiracle, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::LightningBolt)},
	                 Fight(Kind::StartFight, 0, 1, 0.5f),
	                 Fight(Kind::FightAuto, 0, 0, 0.2f),
	                 Fight(Kind::FightAuto, 1, 0, 0.0f),
	                 {.kind = Kind::DrawGesture, .delaySeconds = 8.0f, .value = static_cast<size_t>(GestureType::ForkUp)},
	                 {.kind = Kind::DrawGesture, .delaySeconds = 6.0f, .value = static_cast<size_t>(GestureType::Star)}},
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

	// Big against small: the game never resizes a creature for a fight; it only refuses one outside half to twice its
	// height
	auto big = Fighter(left, 90.0f, "big tiger", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
	big.size = 1.5f;
	auto smaller = Fighter(right, 270.0f, "small tiger", PlayerNames::PLAYER_TWO, 0.5f, 0.0f);
	smaller.size = 0.8f;
	all.push_back({
	    .id = "combat.big_vs_small",
	    .name = "A big creature fights a small one",
	    .facet = Facet::Combat,
	    .description = "A big tiger (size 1.5) fights a small one (size 0.8), just under twice its height, both by "
	                   "themselves.",
	    .expected = "The fight starts and both keep their sizes throughout: the arena is sized for the big one, its "
	                "blows reach further and hit harder, and the small one has to step in to land its own.",
	    .framing = ringside,
	    .creatures = {big, smaller},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Fight(Kind::FightAuto, 1, 1, 0.0f)},
	});
	auto tiny = Fighter(right, 270.0f, "tiny tiger", PlayerNames::PLAYER_TWO, 0.5f, 0.0f);
	tiny.size = 0.6f;
	all.push_back({
	    .id = "combat.too_small",
	    .name = "A creature too small to fight",
	    .facet = Facet::Combat,
	    .description = "A big tiger (size 1.5) is told to fight a tiny one (size 0.6), less than half its height.",
	    .expected = "The tiny tiger refuses: no arena appears and neither moves to fight (the command reports "
	                "\"refused\").",
	    .framing = ringside,
	    .creatures = {big, tiny},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f)},
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
	                "on it walks past the food to eat villagers, its hunger tree of what to act on worth near 0.8 for "
	                "villagers and near 0 for food. The spawner's Mind panel shows the tree and the thoughts.",
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

	all.push_back({
	    .id = "mind.eats_from_store",
	    .name = "Eating from a storage pit",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger of size 1.3, wanting nothing else, with its town's storage pit in front of it, full "
	                   "of food, and no other food. A creature eats from a pit no wider than fifteen times its drawn size.",
	    .expected = "When its plan is to eat from the pit, it walks to within 1.4 times its height of the pit's food pile, "
	                "turns to it for a tenth of a second, and from then on its plan acts on itself. It makes a pot of "
	                "wheat towards the pile, a thousand times its drawn size's worth, picks it up, the pit loses as much, "
	                "and it eats it; its hunger falls.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .size = 1.3f,
	        .needs = {.energy = 0.3f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = OnlyDesire(Desire::Hunger),
	    }},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -50.0f}, .storedFood = 5000}},
	    .commands = {Act(Kind::Stop, 0, 60.0f)},
	});

	all.push_back({
	    .id = "mind.eats_from_field",
	    .name = "Eating from a field",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger, wanting nothing else, with a ripe wheat field of its town in front of it and no "
	                   "other food.",
	    .expected = "When its plan is to eat from the field, it walks to within its height of the field, turns to it for a "
	                "tenth of a second, and from then on its plan acts on itself. It makes a pot of wheat and eats it. The "
	                "pot holds no food, as in the game (a field keeps no store of food), so it spits it out and is not "
	                "hungry for a while; the field's crop stays as it was.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.3f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = OnlyDesire(Desire::Hunger),
	    }},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, -40.0f}, .ripeCrop = true}},
	    .commands = {Act(Kind::Stop, 0, 60.0f)},
	});

	// The game's table row of eating from a tree
	constexpr size_t k_EatFromTree = 94;
	all.push_back({
	    .id = "mind.eats_from_tree",
	    .name = "Eating from a tree",
	    .facet = Facet::Mind,
	    .description = "A tiger with an oak in front of it, told by a script to eat from the tree, as a script's "
	                   "CREATURE_DO_ACTION does.",
	    .expected = "It walks to within the two radii and a fifth more of the tree, turns to it for a tenth of a second, "
	                "makes a pot of fruit and eats it. The pot holds no food, as in the game, so it spits it out; the tree "
	                "stays as it was. Left to itself, it eats only from a tree at least fifteen times its drawn size tall.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, -30.0f}}},
	    .commands = {{.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 1.0f, .value = k_EatFromTree, .object = 0}},
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

	// A tiger of the player and a lion of no player, as the guide is on the first land
	const auto guideLion = [](glm::vec2 offset, float facing) {
		auto lion = Content(CreatureType::Lion, offset, facing, "guide");
		lion.owner = PlayerNames::NEUTRAL;
		return lion;
	};
	auto befriender = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "befriends");
	befriender.desires = OnlyDesire(Desire::BeFriends);
	auto befriended = guideLion({25.0f, 0.0f}, 270.0f);
	befriended.desires = OnlyDesire(Desire::BeFriends);
	all.push_back({
	    .id = "friends.wave_with_guide",
	    .name = "Make friends with the guide",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 50 apart and face each other. As a script of the first land "
	                   "does, both want only to be friends, held there.",
	    .expected = "The tiger smiles, howls or waves at the lion. To wave, it asks the lion to play first; the lion agrees "
	                "when it wants to be friends with the tiger more than it fears it or is angry with it. Then the lion "
	                "stands still in its mind, turns to the tiger and waves back as the tiger waves, from where they "
	                "stand. The Mind panel's Attitudes show the partners while they wave.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {befriender, befriended},
	});
	auto missing = Content(CreatureType::Tiger, {-60.0f, 0.0f}, 90.0f, "misses its friend");
	missing.desires = OnlyDesire(Desire::MissFriend);
	all.push_back({
	    .id = "friends.miss_friend",
	    .name = "Miss a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger that wants only to see a friend it misses, held there, stands 120 from a lion of no player.",
	    .expected = "The tiger pines for the lion: with a sad face it turns to it and points at it, then plays its sad "
	                "action. Or it looks for the lion: it points at it and plays its sad action, then walks near it, to "
	                "three times its own height.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {missing, guideLion({60.0f, 0.0f}, 270.0f)},
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

	// A content tiger with a cow 30 ahead of it: 2 seconds in a script's CREATURE_DO_ACTION has it stomp on the cow and eat
	// it (StompAndEat, row 13), as the third land's scripts force on Lethys's creature with the cattle, with nothing to do
	// it with
	{
		constexpr size_t k_StompAndEat = 13;
		all.push_back({
		    .id = "creature.script_stomp_and_eat",
		    .name = "A script has a creature stomp on a cow and eat it",
		    .facet = Facet::Mind,
		    .description = "A content tiger, its back to the camera, and a cow 30 ahead of it. 2 seconds in a script's "
		                   "CREATURE_DO_ACTION has the tiger stomp on the cow and eat it, with nothing to do it with, as "
		                   "the third land's scripts have Lethys's creature do to the cattle.",
		    .expected = "Under the script's control, angry, it walks up to the cow looking at it and knocks it down; from "
		                "the turn after the blow until the knock-down ends the cow is crushed each turn (the log's "
		                "reactions show the crush). The tiger then picks the cow up, looking at it on the way, and eats "
		                "it, pulling no face. The log says the action was taken up under the script's control.",
		    .framing = {.shot = Shot::Overview, .include = {{0.0f, 30.0f}}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {{.type = AnimalInfo::Cow, .offset = {0.0f, 30.0f}}},
		    .commands =
		        {{.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 2.0f, .value = k_StompAndEat, .object = 0}},
		    .logReactions = true,
		});
	}

	// A content tiger: 2 seconds in a script's CREATURE_DO_ACTION has it die for good (DiePermanently, row 217), done to
	// itself, as the second land's death ray on Khazar's creature and the fifth land's opening force it
	{
		constexpr size_t k_DiePermanently = 217;
		all.push_back({
		    .id = "creature.script_die_permanently",
		    .name = "A script has a creature die for good",
		    .facet = Facet::Mind,
		    .description = "A content tiger, its back to the camera. 2 seconds in a script's CREATURE_DO_ACTION has it die "
		                   "for good, done to itself, as the second land's scripts have Khazar's creature do after the "
		                   "death ray and the fifth land's opening has the player's creature do.",
		    .expected = "Under the script's control, it falls down as in a faint and lies there with its eyes closed, "
		                "not looking about, and does not get up while the scenario lasts. The log says the action was "
		                "taken up under the script's control.",
		    .framing = {.shot = Shot::Follow, .distance = 1.2f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .commands = {{.kind = Kind::ScriptDoAction,
		                  .creature = 0,
		                  .delaySeconds = 2.0f,
		                  .value = k_DiePermanently,
		                  .atCreature = 0}},
		});
	}

	// A content tiger of the player: 2 seconds in a script's CREATURE_DO_ACTION has it die (Die, row 9), done to itself,
	// as the fourth land's ogre fight and the fifth land's big fight force it on the player's creature that lost
	{
		constexpr size_t k_Die = 9;
		all.push_back({
		    .id = "creature.script_die",
		    .name = "A script has a creature die: it faints, is carried home and comes round",
		    .facet = Facet::Mind,
		    .description = "A content tiger, its back to the camera. 2 seconds in a script's CREATURE_DO_ACTION has it die, "
		                   "done to itself, as the fourth land's ogre fight and the fifth land's big fight have the player's "
		                   "creature do when it lost.",
		    .expected = "Under the script's control, it falls down as in a faint and lies there with its eyes closed. "
		                "From the turn it lies it waits four times the size it is drawn at and eight seconds more, then "
		                "fizzes out of sight for two seconds, is moved to its temple's pen (with no temple, to the middle of "
		                "the land), fizzes back in there over two seconds, lies three seconds more and gets up. The log "
		                "says the action was taken up under the script's control.",
		    .framing = {.shot = Shot::Follow, .distance = 1.2f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .commands = {{.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 2.0f, .value = k_Die, .atCreature = 0}},
		    .logReactions = true,
		});
	}

	// The actions the first land's lesson scripts force on a creature, each by a script's CREATURE_DO_ACTION of its row:
	// wave at a thing (row 91), be frightened on the spot (row 174), and lie dead for good (row 261)
	{
		constexpr size_t k_WaveAtObject = 91;
		constexpr size_t k_BeFrightenedOnTheSpot = 174;
		constexpr size_t k_DeadForever = 261;
		all.push_back({
		    .id = "creature.lesson_actions",
		    .name = "A script makes a creature wave at a villager, be frightened on the spot, then lie dead for good",
		    .facet = Facet::Mind,
		    .description = "A content tiger, its back to the camera, and a villager standing 40 ahead of it. A script's "
		                   "CREATURE_DO_ACTION has it wave at the villager 2 seconds in, be frightened on the spot 10 "
		                   "seconds later, and lie dead for good 8 seconds after that.",
		    .expected = "The wave: it walks up to within three and a half of its heights of the villager, smiling, turns to "
		                "face it for 1.4 seconds watching it, then waves. The fright: a frightened start, three seconds "
		                "looking frightened, and another start. Dead for good: it falls down as in a faint and lies there "
		                "with its eyes closed, and does not get up while the scenario lasts.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 40.0f}}},
		    .commands =
		        {{.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 2.0f, .value = k_WaveAtObject, .object = 0},
		         {.kind = Kind::ScriptDoAction,
		          .creature = 0,
		          .delaySeconds = 10.0f,
		          .value = k_BeFrightenedOnTheSpot,
		          .object = 0},
		         {.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 8.0f, .value = k_DeadForever, .object = 0}},
		});
	}

	// The actions with which a creature gets its player's attention, or tells the player what it thinks of the player,
	// each given by a script's CREATURE_DO_ACTION of its row. A script action needs a thing to act on, though most of
	// these actions do not use it: a birch stands by for them
	{
		constexpr size_t k_FollowPlayer = 22;
		constexpr size_t k_ShowPlayerAnObject = 24;
		constexpr size_t k_ThrowAtCamera = 69;
		constexpr size_t k_GoToMiddleOfScreen = 88;
		constexpr size_t k_GoToHand = 89;
		constexpr size_t k_ShowHowNice = 167;
		constexpr size_t k_PointAtHand = 169;
		constexpr size_t k_BeSilly = 172;
		constexpr size_t k_BeCross = 207;
		constexpr size_t k_PrayToPlayer = 318;
		constexpr float k_Reach = 10.0f;
		const ObjectSetup birch {.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}};
		const auto doAction = [](size_t action, float delay) {
			return Command {.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = delay, .value = action, .object = 0};
		};
		const auto doActionWith = [](size_t action, float delay) {
			return Command {
			    .kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = delay, .value = action, .object = 0, .with = 0};
		};
		const auto slap = [](float delay) { return Command {.kind = Kind::Slap, .creature = 0, .delaySeconds = delay}; };
		all.push_back({
		    .id = "mind.go_to_hand",
		    .name = "Going to the player's hand",
		    .facet = Facet::Mind,
		    .description = "A player's tiger stands to one side of the land in view. Keep the hand still on the land far "
		                   "from it, more than an eighth of the view across. A script tells it to go to the hand after two "
		                   "seconds.",
		    .expected = "It walks to the hand, looking at it, and stops four metres past its own radius from the hand. It "
		                "follows the hand when the hand moves, for thirty seconds at most. One time in two it then jumps "
		                "for joy. With the hand near it the action can not start.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {-60.0f, 0.0f}, 90.0f)},
		    .objects = {birch},
		    .commands = {doAction(k_GoToHand, 2.0f)},
		});
		all.push_back({
		    .id = "mind.point_at_hand",
		    .name = "Pointing at the player's hand",
		    .facet = Facet::Mind,
		    .description = "A player's tiger stands side on to the camera. Keep the hand on the land beside it. A script "
		                   "tells it to point at the hand after two seconds and again after eight seconds.",
		    .expected = "Each time it turns to the camera, looking at it, then points for a second at where the hand was "
		                "as it began, looking at that place, amazed.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 0.6f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
		    .objects = {birch},
		    .commands = {doAction(k_PointAtHand, 2.0f), doAction(k_PointAtHand, 6.0f)},
		});
		all.push_back({
		    .id = "mind.follow_player",
		    .name = "Following the player when the camera looks away",
		    .facet = Facet::Mind,
		    .description = "A player's tiger stands in the middle of the map. The camera stands to its north and looks "
		                   "further north, away from it. A script tells it to follow the player after two seconds.",
		    .expected = "It walks to where the camera looks, looking at the camera, until it is near that place or thirty "
		                "seconds go by. It turns to the camera and plays the clip that asks the player to look at it.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 150.0f}, .look = {0.0f, 0.0f, 400.0f}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f)},
		    .objects = {birch},
		    .commands = {doAction(k_FollowPlayer, 2.0f)},
		});
		all.push_back({
		    .id = "mind.go_to_middle_of_screen",
		    .name = "Going to the middle of the screen",
		    .facet = Facet::Mind,
		    .description = "A player's tiger stands near the edge of the view, the camera looking at the land beside it. "
		                   "A script tells it to go to the middle of the screen after two seconds.",
		    .expected = "It walks to where the camera looks, looking at the camera, then turns to the camera.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -120.0f}, .look = {0.0f, 0.0f, 0.0f}},
		    .creatures = {Content(CreatureType::Tiger, {35.0f, 10.0f}, 0.0f)},
		    .objects = {birch},
		    .commands = {doAction(k_GoToMiddleOfScreen, 2.0f)},
		});
		all.push_back({
		    .id = "mind.be_cross",
		    .name = "Being cross with the player",
		    .facet = Facet::Mind,
		    .description = "A player's tiger is slapped four times, so that its attitude to the player falls below minus "
		                   "a tenth. A script then tells it to be cross with the player.",
		    .expected = "It walks to where the camera looks, looking at the camera, turns to the camera, then one time in "
		                "two makes a rude sign, else taunts the player.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 0.6f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {birch},
		    .commands = {slap(0.5f), slap(0.1f), slap(0.1f), slap(0.1f), doAction(k_BeCross, 2.0f)},
		});
		all.push_back({
		    .id = "mind.show_how_nice",
		    .name = "Showing the player how nice it thinks the player is",
		    .facet = Facet::Mind,
		    .description = "A player's tiger is stroked, and a script tells it to show the player how nice it thinks the "
		                   "player is. Then it is slapped four times and told again.",
		    .expected = "The first time it turns to the camera and waves. The second time it turns to the camera and plays "
		                "the sad clip. It looks at the camera all the time.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 0.6f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {birch},
		    .commands = {{.kind = Kind::Stroke, .creature = 0, .delaySeconds = 0.5f},
		                 doAction(k_ShowHowNice, 1.5f),
		                 slap(8.0f),
		                 slap(0.1f),
		                 slap(0.1f),
		                 slap(0.1f),
		                 doAction(k_ShowHowNice, 2.0f)},
		});
		all.push_back({
		    .id = "mind.pray_to_player",
		    .name = "Praying to the player",
		    .facet = Facet::Mind,
		    .description = "A player's tiger stands facing away from the camera. A script tells it to pray to the player "
		                   "after two seconds.",
		    .expected = "It turns to the camera, looking at it, then kneels and prays.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 0.6f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {birch},
		    .commands = {doAction(k_PrayToPlayer, 2.0f)},
		});
		all.push_back({
		    .id = "mind.be_silly",
		    .name = "Being silly with the player",
		    .facet = Facet::Mind,
		    .description = "A player's tiger stands facing away from the camera. A script tells it to be silly with the "
		                   "player after two seconds.",
		    .expected = "Four times: it turns to the camera and talks, fluttering its eyelids; three times in four it shows "
		                "that it feels playful; one time in two it laughs and talks again; then it pulls a silly face for "
		                "five seconds.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 0.6f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {birch},
		    .commands = {doAction(k_BeSilly, 2.0f)},
		});
		all.push_back({
		    .id = "mind.throw_at_camera",
		    .name = "Throwing a thing at the camera",
		    .facet = Facet::Mind,
		    .description = "A player's tiger with a ball in front of it. A script tells it to throw the ball at the "
		                   "camera.",
		    .expected = "It picks the ball up, looking at it, then throws it at the camera, looking at the camera.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, -k_Reach}}},
		    .commands = {doActionWith(k_ThrowAtCamera, 4.0f)},
		});
		all.push_back({
		    .id = "mind.show_player_object",
		    .name = "Showing the player a thing",
		    .facet = Facet::Mind,
		    .description = "A player's tiger with a ball in front of it, the camera looking away from it. A script tells "
		                   "it to show the player the ball.",
		    .expected = "It picks the ball up, walks to where the camera looks, looking at the camera, turns to the camera "
		                "and puts the ball down. Its face shows its attitude to the player as it turns and puts the ball "
		                "down.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 150.0f}, .look = {0.0f, 0.0f, 400.0f}},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, -k_Reach}}},
		    .commands = {doActionWith(k_ShowPlayerAnObject, 4.0f)},
		});
	}

	// A script tells a tiger to sit at a rock far ahead (SitDown, row 81), and the rock is deleted before it gets there
	{
		constexpr size_t k_SitDown = 81;
		all.push_back({
		    .id = "mind.lost_thing",
		    .name = "The creature loses the thing it goes to",
		    .facet = Facet::Mind,
		    .description = "A content tiger, its back to the camera, a rock 60 ahead of it. A script's CREATURE_DO_ACTION "
		                   "has it sit at the rock 2 seconds in; 3 seconds later a script's OBJECT_DELETE deletes the "
		                   "rock, before the tiger gets there.",
		    .expected = "It walks to the rock. The turn after the rock goes, it stops: its plan and the rest of its agenda "
		                "are given up, the log says it lost a thing that it used, and the good advisor says that it lost the "
		                "object it was looking for. The creature window's planner keeps the rock's number as a script's "
		                "target object, which GET_TARGET_OBJECT gives as 0 once it is gone.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 60.0f}}},
		    .commands = {{.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 2.0f, .value = k_SitDown, .object = 0},
		                 {.kind = Kind::ScriptDelete, .delaySeconds = 3.0f, .object = 0}},
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
	// Reward chests to tap open: a row of the common kinds from the sky, and one close up on the land
	{
		constexpr float k_Spacing = 6.0f;
		constexpr float k_RowNorth = 20.0f;
		const auto rowSpot = [](int index) { return glm::vec2((static_cast<float>(index) - 2.5f) * k_Spacing, k_RowNorth); };
		const auto chest = [](RewardObjectInfo type) { return static_cast<size_t>(type); };
		all.push_back({
		    .id = "story.reward_chests",
		    .name = "Reward chests of each common kind",
		    .facet = Facet::Objects,
		    .description = "Six reward chests for the player drop from the sky in a row in front of the camera, a second "
		                   "apart, as a challenge's script drops them: large food, large wood, a toy ball, a weeping "
		                   "stone, the heal seed and the heal power-up on its own. Tap each with the Action button to "
		                   "open it.",
		    .expected = "Each falls 150 m in 3 seconds turning half a round a second and thumps down with its landing "
		                "sound and a puff of sandy dust. A tap plays the tap sound; on the land it then plays the two "
		                "opening sounds, the closed chest gives way to the open one and its lid lifts once, and its dust "
		                "goes. Food and wood are poured at the chest onto a new pile; the ball and the weeping stone "
		                "appear where the chest stands; the heal seed floats a one-shot globe 2 m above the chest when "
		                "the player can't cast heal, and its power-up's gesture shows on the land beside the chest when "
		                "they can; the power-up chest shows only its gesture. A second tap on an opened chest does "
		                "nothing. Each opened chest goes 200 game turns later. Debug > Magic, Reward chests, lists them.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 22.0f, -14.0f}, .look = {0.0f, 0.0f, k_RowNorth}},
		    .commands = {{.kind = Kind::ScriptReward,
		                  .delaySeconds = 1.0f,
		                  .point = rowSpot(0),
		                  .value = chest(RewardObjectInfo::LargeFood),
		                  .fromSky = true},
		                 {.kind = Kind::ScriptReward,
		                  .delaySeconds = 1.0f,
		                  .point = rowSpot(1),
		                  .value = chest(RewardObjectInfo::LargeWood),
		                  .fromSky = true},
		                 {.kind = Kind::ScriptReward,
		                  .delaySeconds = 1.0f,
		                  .point = rowSpot(2),
		                  .value = chest(RewardObjectInfo::RewardObjectToyBall),
		                  .fromSky = true},
		                 {.kind = Kind::ScriptReward,
		                  .delaySeconds = 1.0f,
		                  .point = rowSpot(3),
		                  .value = chest(RewardObjectInfo::WeepingStone),
		                  .fromSky = true},
		                 {.kind = Kind::ScriptReward,
		                  .delaySeconds = 1.0f,
		                  .point = rowSpot(4),
		                  .value = chest(RewardObjectInfo::SpellSeedHeal),
		                  .fromSky = true},
		                 {.kind = Kind::ScriptReward,
		                  .delaySeconds = 1.0f,
		                  .point = rowSpot(5),
		                  .value = chest(RewardObjectInfo::HealPu1),
		                  .fromSky = true}},
		});
		all.push_back({
		    .id = "story.reward_chest_opens",
		    .name = "A reward chest opening close up",
		    .facet = Facet::Objects,
		    .description = "A toy ball reward chest stands on the land close in front of the camera, as a challenge's "
		                   "script puts one down without dropping it. Tap it with the Action button.",
		    .expected = "The tap plays the tap sound and the two opening sounds; the closed chest gives way to the open "
		                "one, whose lid lifts once and stays open, and the toy ball appears where it stands. The opened "
		                "chest goes 200 game turns later.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -7.0f}, .look = {0.0f, 0.5f, 0.0f}},
		    .commands = {{.kind = Kind::ScriptReward,
		                  .delaySeconds = 1.0f,
		                  .point = {0.0f, 0.0f},
		                  .value = chest(RewardObjectInfo::RewardObjectToyBall)}},
		});
		// The same close up with a chest that gives no object, so that nothing stands where the opened chest does
		all.push_back({
		    .id = "story.reward_chest_light",
		    .name = "An opened reward chest's light close up",
		    .facet = Facet::Objects,
		    .description = "A small belief reward chest stands on the land close in front of the camera, as a challenge's "
		                   "script puts one down without dropping it, with no pointer commands. Tap it with the Action "
		                   "button (through the real input path: tools/checks/real_input_tap.sh).",
		    .expected = "The opened chest stays where it stood, its lid lifting once, and its cone of light shines in its "
		                "own blue over what is behind it, its texture turning slowly; nothing comes out to stand over it.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -7.0f}, .look = {0.0f, 0.5f, 0.0f}},
		    .commands = {{.kind = Kind::ScriptReward,
		                  .delaySeconds = 1.0f,
		                  .point = {0.0f, 0.0f},
		                  .value = chest(RewardObjectInfo::SmallBelief)}},
		});
	}

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

	all.push_back({
	    .id = "script.create_creatures",
	    .name = "A script makes creatures that belong to nobody",
	    .facet = Facet::Objects,
	    .description = "A script makes a cow, a giant ape and a tiger of the creatures' kind in front of the camera, one "
	                   "a second (CREATE), as the first land's glade makes the three creatures to choose from.",
	    .expected = "The three creatures appear where they are made, standing on the land, each as big as its species "
	                "starts. Each belongs to nobody: the creature spawner's list gives its owner as Nobody and nobody "
	                "leads it, and the selected one's \"Reactions off\" box is ticked. The log gives each one and says "
	                "its reactions are off.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, 15.0f}, {25.0f, 45.0f}}},
	    .commands = {{.kind = Kind::ScriptCreate,
	                  .delaySeconds = 1.0f,
	                  .point = {-15.0f, 30.0f},
	                  .value = static_cast<size_t>(script::ObjectType::Creature),
	                  .subtype = 1},
	                 {.kind = Kind::ScriptCreate,
	                  .delaySeconds = 1.0f,
	                  .point = {0.0f, 30.0f},
	                  .value = static_cast<size_t>(script::ObjectType::Creature),
	                  .subtype = 0},
	                 {.kind = Kind::ScriptCreate,
	                  .delaySeconds = 1.0f,
	                  .point = {15.0f, 30.0f},
	                  .value = static_cast<size_t>(script::ObjectType::Creature),
	                  .subtype = 2}},
	});

	// The holders of the singing stones a script makes, as Land 1's circle makes them: three in a row, one a second, and a
	// fourth at the place of the middle one
	{
		const auto holder = [](float x) {
			return Command {.kind = Command::Kind::ScriptCreate,
			                .delaySeconds = 1.0f,
			                .point = {x, 30.0f},
			                .value = static_cast<size_t>(script::ObjectType::MobileStatic),
			                .subtype = static_cast<uint32_t>(MobileStaticInfo::SingingStoneBase)};
		};
		all.push_back({
		    .id = "story.singing_stone_holders",
		    .name = "The singing stones' holders a script makes",
		    .facet = Facet::Story,
		    .description = "A script's CREATE makes three singing stone holders, one a second from a second in, 12 m apart "
		                   "in a row in front of the camera, then a fourth at the place of the middle one.",
		    .expected = "Each holder shows only as the print of its model painted on the land, a mark about 11 m across; "
		                "no model stands there. The fourth holder adds no second print.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {20.0f, 40.0f}}},
		    .commands = {holder(-12.0f), holder(0.0f), holder(12.0f), holder(0.0f)},
		});
	}

	// The mobile things a script's create starts the physics of, one a second in a row in front of the camera
	{
		const auto create = [](float x, script::ObjectType type, uint32_t subtype) {
			return Command {.kind = Command::Kind::ScriptCreate,
			                .delaySeconds = 1.0f,
			                .point = {x, 30.0f},
			                .value = static_cast<size_t>(type),
			                .subtype = subtype};
		};
		all.push_back({
		    .id = "script.create_settles",
		    .name = "What a script makes settles on the land",
		    .facet = Facet::Objects,
		    .description = "A script makes, one a second in a row in front of the camera (CREATE), a lump of poo, a "
		                   "cauldron (a mobile object), the ark, a scaffold, a football and a rock.",
		    .expected = "Each one goes into the physics as it is made, standing still: it settles on the land at once, "
		                "turned to its slope, and comes to rest there. The log gives each one.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-35.0f, 15.0f}, {35.0f, 45.0f}}},
		    .commands = {create(-25.0f, script::ObjectType::Poo, 5000),
		                 create(-15.0f, script::ObjectType::MobileObject, static_cast<uint32_t>(MobileObjectInfo::Cauldron)),
		                 create(-5.0f, script::ObjectType::Ark, 5000), create(5.0f, script::ObjectType::Scaffold, 5000),
		                 create(15.0f, script::ObjectType::Ball, 5000),
		                 create(25.0f, script::ObjectType::Rock, static_cast<uint32_t>(MobileStaticInfo::Rock))},
		});
	}

	// A script's point in front of a camera that looks straight down: the view's x is pushed a little off zero before
	// it is made a unit direction, so the point moves along x by the distance times a ten-thousandth over the camera's
	// height, here a metre
	all.push_back({
	    .id = "script.facing_camera_point",
	    .name = "A script makes a rock in front of a camera looking straight down",
	    .facet = Facet::Objects,
	    .description = "The camera is put 0.1 m straight above a point in front of the testbed's camera, looking down at "
	                   "it, for a script's GET_FACING_CAMERA_POSITION of 1000 m, and given back; the script makes a rock "
	                   "there (CREATE), on the land, as the challenge scripts send a god's hand to such a point.",
	    .expected = "The rock is made a metre west of the point (the log gives where), as the original's view direction "
	                "has its x pushed to a ten-thousandth when the camera looks straight down; it settles there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 15.0f}, {15.0f, 45.0f}}},
	    .commands = {{.kind = Kind::ScriptCreate,
	                  .delaySeconds = 1.0f,
	                  .point = {0.0f, 30.0f},
	                  .value = static_cast<size_t>(script::ObjectType::Rock),
	                  .subtype = static_cast<uint32_t>(MobileStaticInfo::Rock),
	                  .eye = {0.0f, 0.1f, 0.0f},
	                  .facingDistance = 1000.0f}},
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

	// The odd actions, by their rows in the game's table, as a script's CREATURE_DO_ACTION gives them
	{
		constexpr size_t k_ShowLearntLesson = 86;
		constexpr size_t k_LookConfused = 92;
		constexpr size_t k_BehaveStrangely = 213;
		all.push_back({
		    .id = "mind.odd_lesson_and_confused",
		    .name = "Creature shows a lesson and looks confused",
		    .facet = Facet::Mind,
		    .description = "A player's tiger stands facing away from the camera, a birch beside it. One second in, a "
		                   "script's CREATURE_DO_ACTION tells it to show a lesson that it learnt. Seven seconds later "
		                   "another tells it to look confused.",
		    .expected = "Each time the script takes it under its control first. First it scratches its head once and "
		                "smiles. Then it turns round to the camera, scratches its head and looks confused. It looks "
		                "puzzled and keeps its eyes on the camera.",
		    .framing = {.shot = Shot::Testbed},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
		    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
		    .commands =
		        {{.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 1.0f, .value = k_ShowLearntLesson, .object = 0},
		         {.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 7.0f, .value = k_LookConfused, .object = 0}},
		});
		all.push_back({
		    .id = "mind.odd_behave_strangely",
		    .name = "Creature behaves strangely",
		    .facet = Facet::Mind,
		    .description = "A player's tiger stands facing the camera, a birch beside it and two villagers 20 to its "
		                   "right. One second in, a script's CREATURE_DO_ACTION tells it to behave strangely, and 19 "
		                   "seconds later a script tells it again.",
		    .expected = "Each time the script takes it under its control first. It stares at the camera for five seconds "
		                "with wide eyes that dart about, then it is happy. One time in two it then faints, the villagers "
		                "near it reacting as to a death, and gets up as soon as it lies, its life unchanged; it then "
		                "stands, out cold to its mind, until the second script takes it over. The other time it looks "
		                "confused two times with a grimace, first with sleepy eyes, then with the wide, darting eyes "
		                "again.",
		    .framing = {.shot = Shot::Testbed},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f)},
		    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}},
		                {.type = VillagerInfo::CelticFarmerMale, .offset = {20.0f, -5.0f}},
		                {.type = VillagerInfo::CelticFarmerMale, .offset = {20.0f, 5.0f}}},
		    .commands =
		        {{.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 1.0f, .value = k_BehaveStrangely, .object = 0},
		         {.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 19.0f, .value = k_BehaveStrangely, .object = 0}},
		});
	}

	// The player's evil, big tiger swaps minds with a small, neutral sheep that belongs to nobody, as The Lost Flock's
	// sheep offers; then back again
	auto swapper = Content(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f, "player's tiger");
	swapper.alignment = -0.9f;
	swapper.size = 1.5f;
	swapper.tattoos.push_back({.design = 3, .site = 0, .colour = {220, 40, 40}});
	swapper.tattoos.push_back({.design = 7, .site = 2, .colour = {40, 90, 230}});
	auto sheep = Content(CreatureType::Sheep, {15.0f, 0.0f}, 270.0f, "sheep");
	sheep.owner = PlayerNames::NEUTRAL;
	sheep.alignment = 0.0f;
	sheep.size = 0.6f;
	all.push_back({
	    .id = "mind.creature_swap",
	    .name = "Swapping creatures",
	    .facet = Facet::Mind,
	    .description = "The player's evil tiger (size 1.5, two tattoos) and a neutral sheep of nobody's (size 0.6) face "
	                   "each other. Three seconds in, a script swaps their minds, as The Lost Flock's sheep lets the "
	                   "player; twenty seconds later it swaps them back.",
	    .expected = "At the swap both play their mind-swap action and the beam starts; the tattoos move to the sheep at "
	                "once while each keeps its own name. Over about ten seconds the sheep grows to the tiger's size and "
	                "turns evil, while the tiger shrinks to the sheep's size and turns neutral. The sheep is now the "
	                "player's creature, which the leash and the spawner show. Swapped back, it all goes the other way.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {swapper, sheep},
	    .commands = {{.kind = Kind::SwapMinds, .creature = 0, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::SwapMinds, .creature = 1, .delaySeconds = 20.0f, .value = 0}},
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
	         {.type = ParticleType::Smoke, .path = particles::DrawPath::Queued, .offset = {0.0f, 0.0f}, .magnitude = 8.0f},
	         {.type = ParticleType::Smoke, .path = particles::DrawPath::Immediate, .offset = {20.0f, 0.0f}, .magnitude = 8.0f}},
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

	// The same place and camera as particles.highlight_glints, with one silver scroll in the middle, lit as a tapped
	// scroll is lit: a started silver scroll's glow shows on every frame it is drawn, where a gold one's shows almost
	// never (only at the very top of its pulse)
	all.push_back({
	    .id = "particles.highlight_lit_silver",
	    .name = "A lit silver challenge scroll",
	    .facet = Facet::Particles,
	    .description = "A silver challenge scroll, as a land's script sets it out, stands on the land north of the "
	                   "middle, lit as a tapped scroll is lit, with the camera looking at it from close by.",
	    .expected = "The scroll shows its lit model and turns about its upright. A faint pale blue column of light "
	                "stands at its middle, three times as tall as it is wide and turned to face the camera, added over "
	                "the land behind it; its active effect, its silver glints and its soft sprite show with it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, 10.0f}, .look = {0.0f, 1.0f, 30.0f}},
	    .fixtures = {.highlights = {{.info = ecs::script_highlight::Info::Silver, .at = glm::vec2 {0.0f, 30.0f}, .lit = true}}},
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

/// A creature rests to get better, heals itself, goes or runs home, and goes into, prays at or looks at its temple
void AddRestAndHome(std::vector<Scenario>& all)
{
	// The player's temple west of the middle, turned as Land 1's (36 radians, in the map script's thousandths), and the
	// place east of its pen where the tiger starts
	constexpr glm::vec2 k_Temple {-300.0f, 100.0f};
	constexpr int32_t k_TempleTurn = 36000;
	constexpr glm::vec2 k_EastOfHome {-100.0f, 40.0f};
	// The game's table rows of the actions a script tells the tiger to do
	constexpr size_t k_HealHimself = 58;
	constexpr size_t k_RestToGetBetter = 59;
	constexpr size_t k_RunHome = 170;
	constexpr size_t k_GoHomeToRecover = 179;
	constexpr size_t k_LookAtCitadel = 190;
	constexpr size_t k_EnterCitadel = 205;
	constexpr size_t k_PrayAtCitadel = 319;
	constexpr size_t k_BringHome = 84;
	const testbed_fixtures::Fixtures withTemple {.temples = {{.at = k_Temple, .rotation = k_TempleTurn}}};
	// A hurt and tired tiger whose body is not held, so that the rest heals it
	const auto hurt = [](glm::vec2 offset, float facing = 90.0f) {
		auto tiger = Content(CreatureType::Tiger, offset, facing);
		tiger.needs.life = 0.3f;
		tiger.needs.exhaustion = 0.6f;
		tiger.hold = false;
		return tiger;
	};
	// A script tells the tiger to do the action, to the birch or to the temple
	const auto told = [](size_t action, bool onTemple = false) {
		return Command {.kind = Kind::ScriptDoAction,
		                .creature = 0,
		                .delaySeconds = 1.0f,
		                .value = action,
		                .object = 0,
		                .onTemple = onTemple};
	};
	const auto birchBy = [](glm::vec2 offset) {
		return ObjectSetup {.type = TreeInfo::Birch, .offset = offset + glm::vec2(0.0f, -25.0f)};
	};

	all.push_back({
	    .id = "needs.rest_to_get_better",
	    .name = "Hurt creature rests to get better",
	    .facet = Facet::Needs,
	    .description = "A hurt and tired tiger (life 0.3, tiredness 0.6) stands by a birch. After one second a script "
	                   "tells it to rest to get better.",
	    .expected = "It turns to face down the slope and lies down to sleep, with its eyes closed. Each turn its life and "
	                "its tiredness get better by its sleep rates. It gets up only when its life is full and it is not "
	                "tired at all, and never before three seconds. Then it looks about, dazed, with a grimace.",
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {hurt({0.0f, 0.0f})},
	    .objects = {birchBy({0.0f, 0.0f})},
	    .commands = {told(k_RestToGetBetter)},
	});

	all.push_back({
	    .id = "needs.go_home_to_recover",
	    .name = "Hurt creature goes home to recover",
	    .facet = Facet::Needs,
	    .description = "A hurt and tired tiger stands east of the pen of its player's temple. After one second a script "
	                   "tells it to go home to recover.",
	    .expected = "It walks home to the pen and stops within its height (no more than 5) of it. Then it faces down the "
	                "slope, lies down and rests until its life is full and it is not tired at all, and gets up dazed.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {hurt(k_EastOfHome)},
	    .objects = {birchBy(k_EastOfHome)},
	    .commands = {told(k_GoHomeToRecover)},
	    .fixtures = withTemple,
	});

	all.push_back({
	    .id = "needs.restore_health",
	    .name = "Hurt creature chooses how to get better",
	    .facet = Facet::Needs,
	    .description = "A badly hurt tiger (life 0.2) stands east of the pen of its player's temple. Its only desire is to "
	                   "restore its health, as strong as it gets. Nothing frightening is near it.",
	    .expected = "It chooses an action of its list to restore its health: it rests where it is, or goes home to "
	                "recover, or heals itself with the heal miracle when it knows it. It does not stand idle.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {[&] {
		    auto tiger = hurt(k_EastOfHome);
		    tiger.needs.life = 0.2f;
		    tiger.needs.exhaustion = 0.0f;
		    tiger.desires = OnlyDesire(Desire::RestoreHealth);
		    return tiger;
	    }()},
	    .fixtures = withTemple,
	});

	all.push_back({
	    .id = "needs.heal_itself",
	    .name = "Creature heals itself with a miracle",
	    .facet = Facet::Needs,
	    .description = "A hurt tiger that knows the heal miracle stands by a birch. A second later a script tells it to "
	                   "heal itself.",
	    .expected = "It casts the heal miracle at itself, where it stands, with a grimace, and holds the miracle for "
	                "three seconds. Then its life is full at once.",
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {hurt({0.0f, 0.0f})},
	    .objects = {birchBy({0.0f, 0.0f})},
	    .commands = {{.kind = Kind::KnowMiracle, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Heal)},
	                 told(k_HealHimself)},
	});

	all.push_back({
	    .id = "mind.go_home",
	    .name = "Creature far from home goes home",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple, more than 20 from it. Its only desire is to "
	                   "stay near home, as strong as it gets.",
	    .expected = "It walks home to the pen and stops within its height (no more than 5) of it. Nearer than 20 to its "
	                "home it does not choose to go home.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {[&] {
		    auto tiger = Content(CreatureType::Tiger, k_EastOfHome, 90.0f);
		    tiger.desires = OnlyDesire(Desire::StayNearHome);
		    return tiger;
	    }()},
	    .fixtures = withTemple,
	});

	all.push_back({
	    .id = "mind.run_home",
	    .name = "Frightened creature runs home",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple. After one second a script tells it to run "
	                   "home.",
	    .expected = "Half the time it first shows that it is frightened, with its eyes wide open. Then it runs home and "
	                "stops within two times its height of the pen.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {birchBy(k_EastOfHome)},
	    .commands = {told(k_RunHome)},
	    .fixtures = withTemple,
	});

	all.push_back({
	    .id = "mind.enter_temple",
	    .name = "Creature goes into its temple",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple. After one second a script tells it to go "
	                   "into its temple.",
	    .expected = "It walks home to the pen and stops within its height. It turns to face the temple with a smile. Then "
	                "it walks towards the temple and stops short of it by 1.3 times a third of the temple's radius.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {birchBy(k_EastOfHome)},
	    .commands = {told(k_EnterCitadel)},
	    .fixtures = withTemple,
	});

	all.push_back({
	    .id = "mind.pray_at_temple",
	    .name = "Creature prays at its temple",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple. After one second a script tells it to pray "
	                   "at its temple.",
	    .expected = "It walks home to the pen and stops within its height. Then it prays (the pray animation, with its "
	                "sounds).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {birchBy(k_EastOfHome)},
	    .commands = {told(k_PrayAtCitadel)},
	    .fixtures = withTemple,
	});

	all.push_back({
	    .id = "mind.bring_thing_home",
	    .name = "Creature brings a thing home",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple, far from it, with a barrel beside it. "
	                   "After one second a script tells it to bring the barrel home.",
	    .expected = "It picks the barrel up, looking at it, walks home looking ahead, and puts the barrel down with the "
	                "put-down clip within five metres of the pen.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = k_EastOfHome + glm::vec2(0.0f, -8.0f)}},
	    .commands = {told(k_BringHome)},
	    .fixtures = withTemple,
	});

	all.push_back({
	    .id = "mind.look_at_temple",
	    .name = "Creature looks at its temple",
	    .facet = Facet::Mind,
	    .description = "A tiger stands east of the pen of its player's temple, facing away from it. After one second a "
	                   "script tells it to look at the temple.",
	    .expected = "It turns to face the temple (two seconds) and looks at its top for three seconds. One time in three "
	                "it then points at the temple for two seconds, amazed. Then it looks at the foot of the temple for "
	                "three seconds and at its top for two seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, k_EastOfHome, 90.0f)},
	    .objects = {birchBy(k_EastOfHome)},
	    .commands = {told(k_LookAtCitadel, true)},
	    .fixtures = withTemple,
	});
}

std::vector<Scenario> Build()
{
	std::vector<Scenario> all;
	AddIdle(all);
	AddExpressions(all);
	AddSenses(all);
	AddNeeds(all);
	AddRestAndHome(all);
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
	AddCreatureDanceScenarios(all);
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
	AddPuzzleTotemScenarios(all);
	AddWorldSoundScenarios(all);
	AddSkyScenarios(all);
	AddKnockScenarios(all);
	AddStatisticsScenarios(all);
	AddFlowersScenarios(all);
	AddHandDemoScenarios(all);
	AddBallScenarios(all);
	AddBeliefScenarios(all);
	AddVirtualInfluenceScenarios(all);
	AddSeedThrowScenarios(all);
	AddTempleScenarios(all);
	AddCreatureExplorationScenarios(all);
	AddCreatureTownHelpScenarios(all);
	AddFriendActionScenarios(all);
	AddScaffoldScenarios(all);
	AddStoryScenarios(all);
	AddSilverScrollScenarios(all);
	AddLand2QuestScenarios(all);
	AddQuestPiedPiperScenarios(all);
	AddQuestOgreScenarios(all);
	AddQuestGuideImpressTownScenarios(all);
	AddQuestGreedyFarmerScenarios(all);
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

/// What is wrong with a command on the scripts, if anything: a wait waits until free, a global or a script is named,
/// and a task's wait or the clicks are one of theirs
std::string_view ScriptCommandProblem(const Command& command)
{
	namespace timeline = testbed_land_timeline;
	const bool wait = command.kind == Kind::WaitGlobal || command.kind == Kind::WaitTask || command.kind == Kind::WaitText ||
	                  command.kind == Kind::WaitCamera;
	if (wait && !command.waitUntilFree)
	{
		return "a wait that does not wait until free";
	}
	switch (command.kind)
	{
	case Kind::WaitGlobal:
	case Kind::SetGlobal:
		return !command.global.empty() && command.global.find(' ') == std::string_view::npos ? "" : "names no global";
	case Kind::WaitTask:
		if (command.script.empty() || command.script.find(' ') != std::string_view::npos)
		{
			return "names no script";
		}
		return command.value < timeline::k_TaskWaits ? "" : "no such wait for a task";
	case Kind::StartScript:
		return !command.script.empty() && command.script.find(' ') == std::string_view::npos ? "" : "names no script";
	case Kind::WaitText:
		return command.value > 0 && command.value < helptext::k_TextCount ? "" : "no such text";
	case Kind::WaitCamera:
		return command.radius > 0.0f ? "" : "within no distance";
	case Kind::LogGlobals:
		return !timeline::Names(command.global).empty() || !timeline::Names(command.script).empty() ? "" : "logs nothing";
	case Kind::ClickTexts:
		return command.value < 2 ? "" : "the clicks neither start nor stop";
	case Kind::CameraFollow:
	case Kind::HandOver:
		return command.value < 2 && command.amount > 0.0f ? "" : "neither starts nor stops, or at no distance";
	case Kind::SetCreatureSize:
		return command.amount > 0.0f ? "" : "a size of nothing";
	case Kind::TownOwner:
		return command.thing.has_value() && command.thing->type == script::ObjectType::Town &&
		               command.player < PlayerNames::_COUNT
		           ? ""
		           : "takes over no town, or for no player";
	case Kind::KnockDownTown:
		return command.thing.has_value() && command.thing->type == script::ObjectType::Town ? "" : "knocks down no town";
	case Kind::TraceComputerMind:
		return command.player < PlayerNames::NEUTRAL && command.value < 2 ? ""
		                                                                  : "traces no computer player, or neither on nor off";
	default:
		return "";
	}
}

/// What is wrong with what a command acts on, if anything: the object it picks up must be a thing, a pot or pile, an
/// animal or a villager, or a mobile object named by its place (the creature's own tests of what it can pick up decide the
/// rest), what it knocks down a thing or a tree, the part it strokes, the way it looks something over and the leash put on real
/// ones, the walker a villager and the store a building
std::string_view CommandProblem(const Command& command, std::span<const ObjectSetup> objects)
{
	const auto* object = command.object < objects.size() ? &objects[command.object] : nullptr;
	switch (command.kind)
	{
	case Kind::PickUp:
		// a thing named by its script type and place, rather than one of the scenario's objects, is a mobile object (a
		// one-shot globe floating on its own is one)
		if (command.thing.has_value())
		{
			return command.thing->type == script::ObjectType::MobileObject ? "" : "picks up no mobile object";
		}
		return object != nullptr &&
		               (std::holds_alternative<MobileObjectInfo>(object->type) ||
		                std::holds_alternative<PotInfo>(object->type) || std::holds_alternative<AnimalInfo>(object->type) ||
		                std::holds_alternative<VillagerInfo>(object->type) ||
		                std::holds_alternative<MobileStaticInfo>(object->type) ||
		                std::holds_alternative<TreeInfo>(object->type) ||
		                (std::holds_alternative<FeatureInfo>(object->type) &&
		                 std::get<FeatureInfo>(object->type) == FeatureInfo::MagicMushroom))
		           ? ""
		           : "picks up no thing, pot, animal, villager, static, tree or magic mushroom";
	case Kind::KnockDown:
		return object != nullptr && !std::holds_alternative<FeatureInfo>(object->type) ? "" : "knocks down no thing or tree";
	case Kind::Kick:
		return object != nullptr ? "" : "kicks nothing";
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
	case Kind::ScriptMakeArtefact:
		return object != nullptr ? "" : "makes no object an artefact";
	case Kind::ScriptInterfaceLevel:
		return command.value < k_InterfaceLevels ? "" : "no such interface level";
	case Kind::ScriptChallengeSnapshot:
		return command.value < 3 ? "" : "no such snapshot";
	case Kind::TownGainsBelief:
		return command.amount > 0.0f && command.player < PlayerNames::_COUNT &&
		               (!command.thing.has_value() || command.thing->type == script::ObjectType::Town)
		           ? ""
		           : "gains no belief, for no player or in no town";
	case Kind::SetWorshipShare:
		return InRange(command.amount, 0.0f, 1.0f) ? "" : "no such share";
	case Kind::ScriptWalkPath:
		return object != nullptr && InRange(command.pathFrom, 0.0f, 1.0f) && InRange(command.pathTo, 0.0f, 1.0f)
		           ? ""
		           : "walks no object, or past the track's ends";
	case Kind::CreateVortex:
		return command.value < 3 ? "" : "no such land vortex";
	case Kind::VortexCrossingFile:
		return !command.crossingFile.empty() ? "" : "gives no crossing file";
	case Kind::StoreTakes:
		return object != nullptr && command.value != command.object && command.value < objects.size() &&
		               std::holds_alternative<AbodeInfo>(objects[command.value].type)
		           ? ""
		           : "takes nothing, or into no building";
	default:
		return ScriptCommandProblem(command);
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
    {.kind = Kind::VortexCrossingFile, .name = "vortex crossing file", .needsNoCreature = true},
    {.kind = Kind::ScriptFade, .name = "script fade", .needsNoCreature = true},
    {.kind = Kind::HighDetail, .name = "high detail", .needsNoCreature = true},
    {.kind = Kind::KeyDown, .name = "key down", .needsNoCreature = true},
    {.kind = Kind::KeyUp, .name = "key up", .needsNoCreature = true},
    {.kind = Kind::ScriptFocus, .name = "script focus"},
    {.kind = Kind::ScriptRelease, .name = "script release"},
    {.kind = Kind::ScriptHealth, .name = "script health", .needsNoCreature = true},
    {.kind = Kind::ScriptSetHealth, .name = "script set health", .needsNoCreature = true},
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
    {.kind = Kind::SwapMinds, .name = "swap minds"},
    {.kind = Kind::ScriptPuzzleTotem, .name = "script puzzle totem", .needsNoCreature = true},
    {.kind = Kind::PuzzleTotemHeights, .name = "puzzle totem heights", .needsNoCreature = true},
    {.kind = Kind::ScriptCreate, .name = "script create", .needsNoCreature = true},
    {.kind = Kind::Kick, .name = "kick"},
    {.kind = Kind::OpenTempleRooms, .name = "open the temple's rooms", .needsNoCreature = true},
    {.kind = Kind::SetAlight, .name = "set alight"},
    {.kind = Kind::HandGrab, .name = "hand grab"},
    {.kind = Kind::ScriptDesiresOff, .name = "script desires off"},
    {.kind = Kind::ScriptDesire, .name = "script desire"},
    {.kind = Kind::ScriptReaction, .name = "script reaction"},
    {.kind = Kind::ScriptCreateFromCreature, .name = "script create from creature"},
    {.kind = Kind::ScriptSpeed, .name = "script speed"},
    {.kind = Kind::SetWorshipShare, .name = "set worship share", .needsNoCreature = true},
    {.kind = Kind::WaitGlobal, .name = "wait for a script global", .needsNoCreature = true},
    {.kind = Kind::WaitTask, .name = "wait for a script's task", .needsNoCreature = true},
    {.kind = Kind::WaitText, .name = "wait for a text", .needsNoCreature = true},
    {.kind = Kind::SetGlobal, .name = "set a script global", .needsNoCreature = true},
    {.kind = Kind::StartScript, .name = "start a script", .needsNoCreature = true},
    {.kind = Kind::LogGlobals, .name = "log script globals", .needsNoCreature = true},
    {.kind = Kind::ClickTexts, .name = "click texts", .needsNoCreature = true},
    {.kind = Kind::CameraTo, .name = "camera to", .needsNoCreature = true},
    {.kind = Kind::CameraFollow, .name = "camera follows", .needsNoCreature = true},
    {.kind = Kind::SetCreatureSize, .name = "set the creature's size"},
    {.kind = Kind::MoveThing, .name = "move a thing", .needsNoCreature = true},
    {.kind = Kind::HandOver, .name = "hand over a point", .needsNoCreature = true},
    {.kind = Kind::HandDrop, .name = "hand drops", .needsNoCreature = true},
    {.kind = Kind::WaitCamera, .name = "wait for the camera", .needsNoCreature = true},
    {.kind = Kind::ScaffoldSetActive, .name = "scaffold set active", .needsNoCreature = true},
    {.kind = Kind::HandPlantScaffold, .name = "hand plants a scaffold", .needsNoCreature = true},
    {.kind = Kind::ScriptMakeArtefact, .name = "script makes an artefact", .needsNoCreature = true},
    {.kind = Kind::SetCameraBookmark, .name = "set a camera bookmark", .needsNoCreature = true},
    {.kind = Kind::ShowCameraBookmarks, .name = "show camera bookmarks", .needsNoCreature = true},
    {.kind = Kind::LogCameraBookmarks, .name = "log camera bookmarks", .needsNoCreature = true},
    {.kind = Kind::TapCameraBookmarkKey, .name = "tap a camera bookmark key", .needsNoCreature = true},
    {.kind = Kind::TownOwner, .name = "a town changes hands", .needsNoCreature = true},
    {.kind = Kind::ReplayChallenge, .name = "replay a challenge from the log", .needsNoCreature = true},
    {.kind = Kind::KnockDownTown, .name = "knock a town down", .needsNoCreature = true},
    {.kind = Kind::HandThrowAt, .name = "hand throws at a point", .pointer = true},
    {.kind = Kind::ScriptBuildBuilding, .name = "script builds a building", .needsNoCreature = true},
    {.kind = Kind::TraceComputerMind, .name = "trace a computer mind", .needsNoCreature = true},
    {.kind = Kind::VillagerReactions, .name = "villager reactions", .needsNoCreature = true},
    {.kind = Kind::StrikeShield, .name = "strike shield", .needsNoCreature = true},
    {.kind = Kind::ScriptOpponentProperty, .name = "script opponent property", .needsNoCreature = true},
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
	    "Benchmark", "Creature Mode", "Physics", "Examples", "Advisors", "Sky",        "Animals",   "Story",
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

namespace
{
/// Whether a command acts on one of the scenario's objects, by its place among them
bool ActsOnObject(const Command& command)
{
	switch (command.kind)
	{
	case Kind::PickUp:
	case Kind::KnockDown:
	case Kind::Kick:
	case Kind::TieLeash:
	case Kind::VillagerWalkTo:
	case Kind::StoreTakes:
	case Kind::SetOpenClose:
	case Kind::LayGateStone:
	case Kind::HighDetail:
	case Kind::ScriptHealth:
	case Kind::ScriptSetHealth:
	case Kind::ScriptTotemProperty:
	case Kind::ScriptWalkPath:
	case Kind::ScriptDelete:
	case Kind::ScriptDanceAttach:
	case Kind::ScriptMakeArtefact:
	case Kind::AbodeBuilt:
	case Kind::HandTake:
		return true;
	case Kind::CastMiracle:
		return !command.atCreature.has_value();
	case Kind::ScriptDoAction:
		return !command.atCreature.has_value() && !command.onTemple;
	case Kind::SetAlight:
		return command.value == 1;
	case Kind::ScriptDance:
		return command.danceAround.has_value();
	default:
		return false;
	}
}

/// Whether a command's point is a place on the land, rather than on the screen or not used
bool PointIsOnTheLand(Kind kind)
{
	switch (kind)
	{
	case Kind::WalkTo:
	case Kind::RunTo:
	case Kind::FleeFrom:
	case Kind::TurnToFace:
	case Kind::ThrowAt:
	case Kind::PointAt:
	case Kind::PlayerDid:
	case Kind::ScriptFocus:
	case Kind::ScriptMove:
	case Kind::ScriptReward:
	case Kind::ScriptCreate:
	case Kind::ScriptPopulate:
	case Kind::ScriptDance:
	case Kind::ScriptPuzzleTotem:
	case Kind::ScriptCreateFromCreature:
	case Kind::CreateVortex:
	case Kind::TapHighlight:
	case Kind::MoveThing:
	case Kind::HandOver:
	case Kind::WaitCamera:
	case Kind::ScaffoldSetActive:
	case Kind::HandPlantScaffold:
	case Kind::SetCameraBookmark:
	case Kind::DrawGesture:
	case Kind::HandThrowAt:
	case Kind::ScriptBuildBuilding:
	case Kind::StrikeShield:
		return true;
	default:
		return false;
	}
}

/// What is wrong with a scenario written for a land (Scenario::onLand): it sets nothing out, its commands act on the
/// player's creature alone and on no object or town of its own, and its points are on the land
std::vector<std::string> LandProblems(const Scenario& scenario)
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
	if (!scenario.creatures.empty() || !scenario.objects.empty() || !scenario.particles.empty() ||
	    !scenario.dispensers.empty() || !scenario.miracles.empty() || !scenario.throws.empty() || !scenario.footpaths.empty() ||
	    !scenario.landScript.empty() || !testbed_fixtures::Empty(scenario.fixtures) || scenario.crowd.has_value() ||
	    !scenario.bareTowns.empty() || !scenario.birdFlocks.empty() || scenario.hand.has_value() ||
	    scenario.townBelief.has_value() || scenario.tribalPower.has_value())
	{
		problems.emplace_back("a land timeline sets nothing out: the land is the game's");
	}
	if (scenario.commands.empty())
	{
		problems.emplace_back("a land timeline with no commands");
	}
	for (size_t i = 0; i < scenario.commands.size(); ++i)
	{
		const auto& command = scenario.commands.at(i);
		const auto what = fmt::format("command {} ({})", i, Name(command.kind));
		if (!NeedsNoCreature(command.kind) && command.creature != 0)
		{
			problems.push_back(fmt::format("{}: a land's only creature is the player's, creature 0", what));
		}
		if (ActsOnObject(command) && !command.thing.has_value())
		{
			problems.push_back(fmt::format("{}: acts on an object of the scenario's, which a land has none of", what));
		}
		if (command.kind == Kind::LayGateStone && !command.other.has_value())
		{
			problems.push_back(fmt::format("{}: lays the stone in no plinth of the land", what));
		}
		for (const auto& pick : {command.thing, command.other})
		{
			if (pick.has_value() && (!testbed_land_timeline::OnTheLand(pick->point) || !(pick->radius > 0.0f) ||
			                         pick->type == script::ObjectType::None))
			{
				problems.push_back(fmt::format("{}: picks a thing of no type, off the land or within no radius", what));
			}
		}
		if (command.kind == Kind::CameraTo && (!testbed_land_timeline::OnTheLand({command.eye.x, command.eye.z}) ||
		                                       !testbed_land_timeline::OnTheLand({command.look.x, command.look.z})))
		{
			problems.push_back(fmt::format("{}: the camera's eye or look is off the land", what));
		}
		if (command.kind == Kind::Follow || command.kind == Kind::StartFight || command.kind == Kind::TieLeashToCreature ||
		    command.kind == Kind::SwapMinds || command.atCreature.has_value())
		{
			problems.push_back(fmt::format("{}: acts on another creature, which a land timeline has none of", what));
		}
		if ((command.kind == Kind::TownGainsBelief && !command.thing.has_value()) || command.kind == Kind::SetWorshipShare)
		{
			problems.push_back(fmt::format("{}: acts on the scenario's town, which a land has none of", what));
		}
		if (command.kind == Kind::ChangeLand && command.landScript.empty())
		{
			problems.push_back(fmt::format("{}: no land's script", what));
		}
		if (PointIsOnTheLand(command.kind) && !testbed_land_timeline::OnTheLand(command.point))
		{
			problems.push_back(fmt::format("{}: the point is off the land", what));
		}
		if (command.delaySeconds < 0.0f || !InRange(command.hour, 0.0f, k_HoursPerDay))
		{
			problems.push_back(fmt::format("{}: delay or hour out of range", what));
		}
		if (const auto problem = ScriptCommandProblem(command); !problem.empty())
		{
			problems.push_back(fmt::format("{}: {}", what, problem));
		}
	}
	if (scenario.repeatFrom.has_value() && *scenario.repeatFrom >= scenario.commands.size())
	{
		problems.emplace_back("repeats from a command that isn't there");
	}
	return problems;
}
} // namespace

std::vector<std::string> testbed_scenarios::Problems(const Scenario& scenario)
{
	if (scenario.onLand)
	{
		return LandProblems(scenario);
	}
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
	if (environment.landNumber && (*environment.landNumber < 1 || *environment.landNumber > 6))
	{
		problems.emplace_back("a land number that is not one of the lands, 1 to 6");
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
		if (particle.targetObject.has_value() && *particle.targetObject >= scenario.objects.size())
		{
			problems.emplace_back("a particle effect given an object the scenario does not have");
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
	for (const auto& footpath : scenario.footpaths)
	{
		const auto outOfRange = [&scenario](size_t object) { return object >= scenario.objects.size(); };
		if ((footpath.startsAt.has_value() && outOfRange(*footpath.startsAt)) ||
		    std::ranges::any_of(footpath.leadsFrom, outOfRange) ||
		    !std::ranges::all_of(footpath.nodes, [](glm::vec2 node) { return ValidOffset(node); }) || footpath.nodes.empty() ||
		    footpath.leadsFrom.empty())
		{
			problems.emplace_back("a footpath with no nodes, a node off the map or an object that isn't there");
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

CameraPlacement testbed_scenarios::Placed(glm::vec3 eye, glm::vec3 look, glm::vec2 origin,
                                          const std::function<float(glm::vec2)>& groundAt)
{
	const auto eyeAt = MapPoint(origin, {eye.x, eye.z});
	const auto lookAt = MapPoint(origin, {look.x, look.z});
	return {
	    .origin = {eyeAt.x, groundAt(eyeAt) + eye.y, eyeAt.y},
	    .focus = {lookAt.x, groundAt(lookAt) + look.y, lookAt.y},
	};
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
