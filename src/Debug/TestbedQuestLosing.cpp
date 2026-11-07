/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Losing the land, as a quest run's chapter on Land 1: the player's temple is destroyed by the game's own harm, and the
// game's end of turn starts the game over, whose script plays to its end. The game over is the game's, not a land's
// script, so the run starts only the land's setup script. The temple's plan is in map coordinates, from where the
// land's script plans it.

#include <cstddef>

#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::TaskWait;
using testbed_land_timeline::ThingNear;

/// The temple's plan, where the land's script plans the player's temple
constexpr glm::vec2 k_TemplePlan {1915.05f, 2508.89f};
/// The player's temple's heart, found at its plan
constexpr ThingNear k_Heart {.type = script::ObjectType::Citadel, .point = k_TemplePlan, .radius = 10.0f};
/// A hit as small as a thrown boulder's most, which a building of the player's takes in the heart's place
constexpr float k_PassedOnHit = 0.2f;
/// The hits aimed at the heart after the first, each of a whole life, half a second apart: the heart passes each on to
/// the next of its god's buildings with life, then to the next homeless villager of its god's towns, and takes it itself
/// once there is neither, as the player's miracles at the temple would wear their own town down first. Enough for every
/// building and person of the player's town, then the heart; the ones after its destruction has begun find no life left
/// to take
constexpr float k_HeartHit = 1.0f;
constexpr size_t k_HeartHits = 90;
constexpr float k_BetweenHits = 0.5f;
constexpr std::string_view k_GameOverScript = "GameOver";

Command HitHeart(float amount, float delay)
{
	return {
	    .kind = Kind::EffectHit, .player = PlayerNames::PLAYER_ONE, .delaySeconds = delay, .amount = amount, .thing = k_Heart};
}

/// The real input chapter's camera: south of the temple's plan and above it, looking at the temple
constexpr glm::vec3 k_FireCameraEye {k_TemplePlan.x, 30.0f, k_TemplePlan.y - 60.0f};
constexpr glm::vec3 k_FireCameraLook {k_TemplePlan.x, 8.0f, k_TemplePlan.y};
/// The fire seeds the player's hand is given, and the time between them, for the recording to cast at the temple
constexpr size_t k_FireSeeds = 3;
constexpr float k_BetweenFireSeeds = 12.0f;

Command WaitTask(std::string_view script, TaskWait wait)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}
} // namespace

void openblack::testbed_scenarios::AddQuestLosingScenarios(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    // the game over's lines that wait for a click are clicked on as the player would
	    {.kind = Kind::ClickTexts, .value = 1},
	    // the temple built at its plan, as the land's script orders it and its building leaves it
	    {.kind = Kind::ScriptBuildBuilding, .delaySeconds = 1.0f, .point = k_TemplePlan, .amount = 1.0f},
	    {.kind = Kind::TempleBuilt, .delaySeconds = 0.5f, .built = 1.0f},
	    // while the player has a town, a hit aimed at the heart is passed on to one of its buildings
	    HitHeart(k_PassedOnHit, 3.0f),
	};
	// [shortcut] the player's own miracles at the temple
	for (size_t i = 0; i < k_HeartHits; ++i)
	{
		commands.push_back(HitHeart(k_HeartHit, i == 0 ? 2.0f : k_BetweenHits));
	}
	commands.push_back(WaitTask(k_GameOverScript, TaskWait::Running));
	commands.push_back(WaitTask(k_GameOverScript, TaskWait::Finished));
	commands.push_back({.kind = Kind::LogGlobals, .delaySeconds = 1.0f, .script = k_GameOverScript});
	// The same start for the real input path (tools/checks/realinput/losing_the_land_fire.txt, replayed through
	// tools/checks/real_input_tap.sh): no pointer commands and no hits of the timeline's own. The temple is built and
	// the camera placed on it; the hand is given a fire seed now and then, as a bubble's comes, and the recording casts
	// each at the temple
	std::vector<Command> fire {
	    {.kind = Kind::ClickTexts, .value = 1},
	    {.kind = Kind::ScriptBuildBuilding, .delaySeconds = 1.0f, .point = k_TemplePlan, .amount = 1.0f},
	    {.kind = Kind::TempleBuilt, .delaySeconds = 0.5f, .built = 1.0f},
	    {.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_FireCameraEye, .look = k_FireCameraLook},
	};
	for (size_t i = 0; i < k_FireSeeds; ++i)
	{
		fire.push_back({.kind = Kind::HoldSeed,
		                .delaySeconds = i == 0 ? 2.0f : k_BetweenFireSeeds,
		                .value = static_cast<size_t>(SpellSeedType::Fire)});
	}
	fire.push_back({.kind = Kind::LogGlobals, .delaySeconds = k_BetweenFireSeeds, .script = k_GameOverScript});
	all.push_back({
	    .id = "quest.losing_the_land_real_fire",
	    .name = "Land 1 quest run: fireballs cast at the player's temple by hand",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 from a quest run of SetupLand1 alone, with the player's mouse from a recording. "
	                   "The temple is built at its plan and the camera placed south of it, looking at it; the hand is "
	                   "given a fire seed three times, twelve seconds apart, as a bubble's seed comes into it.",
	    .expected = "Each seed cast at the temple is a fireball; its blast and fire aimed at the temple's heart or "
	                "parts are passed on, through the heart, to a building of the player's town with more than a "
	                "quarter of its life, beamed at, and the temple itself is not harmed while one is left.",
	    .commands = std::move(fire),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.losing_the_land",
	    .name = "Land 1 quest run: the player's temple destroyed, and the game over",
	    .facet = Facet::Story,
	    .description = "Played on Land 1 from a quest run of SetupLand1 alone (the game over is the game's, started by "
	                   "no land script). The temple is built at its plan. A hit of a fifth is aimed at its heart, then "
	                   "hits of a whole life every half second (a shortcut for the player's own miracles), until the "
	                   "player's town has no building or person left to take them and the heart has none. It waits for "
	                   "the game over's script to run and to end.",
	    .expected = "Each hit is passed on: to the player's buildings, the first with more than a quarter of its life "
	                "first, then to the town's homeless people, and only then to the heart itself. Its life reaching "
	                "nothing starts the temple's destruction, and at the end of that turn the game starts the GameOver "
	                "script, once: the failure music, night falling, the camera's shots of the temple, Nemesis's laugh, "
	                "the evil advisor's 3925 and the good advisor's 1268, the fade to black, Game Over written, game "
	                "time stopped, and the developer function that would take the player to the Save Game room.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
