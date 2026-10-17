/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <MindFile.h>

#include "Creature/CreatureCreation.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureTownCompassion.h"
#include "Creature/LeashRules.h"
#include "Creature/PerceivedDesires.h"

namespace openblack::ecs::components
{

/// What a creature wants, what it is doing about it, and what it looks at
struct CreatureMindState
{
	/// The last stage of growing up
	static constexpr uint32_t k_FullyGrownUp = 13;

	/// The desires, drawn from the species' tables as the creature is made (without the tables, set up when the mind
	/// first thinks)
	std::optional<creature_desires::Desires> desires;
	/// The innate personality and a value for each previous action, drawn as the creature is made. Kept for the readers
	/// still to be ported: nothing reads them yet
	std::array<float, creature_creation::k_PersonalityCount> personality {};
	std::vector<uint32_t> previousActions;
	/// The desire made dominant over the others for a while, if any
	creature_spell_mind::Cheat dominantDesire {};
	creature_mind::IdleMind idle {};
	creature_look::Target look {};
	/// Whether the head turns to what it watches this turn
	bool lookingAbout {false};

	/// Seconds since the player last paid it any attention
	float secondsAlone {0.0f};
	/// Seconds since the player last stroked or slapped it, and which
	std::optional<float> feedbackSeconds;
	bool feedbackWasStroke {false};
	/// How it feels about the player, which each stroke or slap nudges, and the average of what it has been given
	float attitudeToPlayer {0.0f};
	float averageFeedback {0.0f};
	/// The last feedback the player gave it as the hand let go, from -1 (slapped) to 1 (stroked), and what it was doing
	/// then, for it to learn from
	struct Feedback
	{
		float value;
		creature_mind::Activity activity;
	};
	std::optional<Feedback> lastFeedback;
	/// What a plain stroke or slap of a creature too young to learn from it leaves it to do next, once it is free to
	/// choose: be frightened, or show it is happy. A fizzled miracle takes its place, and it takes a fizzle's
	enum class AfterFeedback : uint8_t
	{
		Frightened,
		Happy,
	};
	std::optional<AfterFeedback> afterFeedback;
	/// What it thinks its player wants, from the player's miracles it saw, the strokes and slaps it was given and the
	/// town needs it saw the player's villagers see to
	creature_perceived_desires::PerceivedDesires perceivedDesires {};

	/// How far the creature has grown up, which decides the desires it has and which of its body's needs it feels, and
	/// the stage they were last set for. Until the game's story moves it on, a creature starts fully grown up.
	uint32_t developmentPhase {k_FullyGrownUp};
	std::optional<uint32_t> desiresPhase;

	/// While paused, the mind leaves the body alone to be posed by hand
	bool paused {false};
	/// Where a script has told it to look (SET_FOCUS): its head looks there every turn until the script releases it
	std::optional<glm::vec3> scriptFocus;
	/// What a script last gave it to play (SET_SCRIPT_ULONG), which it plays when the script next sets its state
	creature_script_play::Request scriptPlay {};
	/// Where it is in its look on the way to a point, set from its species the first time it looks so
	std::optional<creature_mind::WalkLook> walkLook;

	/// What it has learnt and remembers, set up with the desires (from its mind file when it has one)
	std::optional<creature_mind_model::Learnt> learnt;
	/// The plans it weighs, and the one it carries out
	creature_planner::PlannerState planner {};
	/// Which of a town's desires its compassion helps with
	creature_town_compassion::State townCompassion {};
	/// The agenda carries out the planner's plan; a step of it ate, which charged the body for its action already
	bool planActive {false};
	bool satisfiedByEffect {false};
	/// A step of the plan's agenda completed it, before the rest of the agenda
	bool completedByStep {false};
	/// The agenda carrying out the plan, and the last agenda remembered for feedback, by the idle mind's count of them
	uint32_t planSerial {0};
	uint32_t agendaSeen {0};
	/// Turns its agenda's steps have run, counted on from step to step, which a walk up to something checks every fifty
	/// for having got stuck
	uint32_t stepTurns {0};
	/// Game turns the mind has thought, and the turn it last planned
	uint32_t turn {0};
	uint32_t plannedTurn {0};
	/// The trainer of the debug tools: each thing it does to something, once done, it is stroked for if the thing is of
	/// this kind (by the game's belief types) and slapped for otherwise
	std::optional<uint32_t> trainer;
	/// The kinds of thing it has been given from the hand with no desire for them, which it looked over the first time
	std::set<uint32_t> givenKinds;
	/// A mind file to take up at the next turn, from the creature's mind resource or the debug tools; whether the
	/// resource has been looked at
	std::shared_ptr<const creaturemind::MindFileData> pendingFile;
	bool resourceChecked {false};
	/// A name the script gave it before its mind was set up or took up its file, put over the file's once it has
	std::optional<std::u16string> scriptName;
	/// What a script taught it or took away (a skill or a miracle, see creature_watching::SetKnows) before its mind was
	/// set up or took up its file, in order, put over the file's once it has
	struct ScriptKnows
	{
		uint32_t kind;
		uint32_t action;
		bool knows;
	};
	std::vector<ScriptKnows> scriptKnows;

	/// What the leash tells the mind: the desire it forces, whether the creature is following it to the hand, and what
	/// the player has shown it on the leash
	creature_leash::MindHooks leash {};
};

} // namespace openblack::ecs::components
