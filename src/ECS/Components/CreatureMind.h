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
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <MindFile.h>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureCreation.h"
#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreaturePlayerAttention.h"
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
	/// The first stage of growing up, which a creature the game makes is at
	static constexpr uint32_t k_FirstStage = 0;
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
	/// The things it has come across, and how far its eyes have swept the cells about it
	creature_object_beliefs::Store beliefs;
	creature_object_beliefs::Sweep sweep {};
	/// The last moving thing it came across, for its look on the way to a far point
	creature_object_beliefs::MovingSeen movingSeen {};
	/// Whether the head turns to what it watches this turn
	bool lookingAbout {false};

	/// Game turns its player's camera was looking at it, which over the turns it has lived is the attention its god has
	/// paid it
	uint32_t turnsLookedAt {0};
	/// How interesting it finds its player, which follows the player's hand each turn
	creature_desire_sources::PlayerInterest playerInterest {};
	/// How fast the player's hand moves, which follows the hand by a part each turn
	creature_player_attention::HandSpeed handSpeed {};
	/// Where the player's hand nearest it last had a place, which the look at the hand watches while the hand has none
	glm::vec3 lastHandPoint {creature_player_attention::k_HandPointBeforeAny};
	/// Game turns in a row its player's camera has not been looking at it, which make it lonely
	uint32_t turnsNotLookedAt {0};
	/// Game turns since its player last sent it to act on something with the leash, which leave it short of anything to
	/// do with its player
	uint32_t turnsSinceMadeToAct {0};
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
	/// How strongly the player last rewarded (above 0) or punished (below 0) it: the feedback the hand let go with when
	/// it was more than slight, or a blow from something its own player threw. Scripts read it, and one sets it back to 0
	/// to wait for the next.
	float interactionMagnitude {0.0f};
	/// How many times it has carried out each action of the game's table to its end, by the action's number, for the
	/// scripts to read; and the action of what the idle mind is carrying out, by its count of agendas
	std::map<uint32_t, uint32_t> actionCounts;
	struct ActionUnderway
	{
		uint32_t serial;
		uint32_t action;
	};
	std::optional<ActionUnderway> underway;
	/// What a plain stroke or slap of a creature too young to learn from it leaves it to do next, once it is free to
	/// choose: be frightened, or show it is happy. A fizzled miracle takes its place, and it takes a fizzle's
	enum class AfterFeedback : uint8_t
	{
		Frightened,
		Happy,
	};
	std::optional<AfterFeedback> afterFeedback;
	/// Given a thing by the player's hand, once it is free to choose it decides what to do with what it then holds,
	/// whether it took the thing or not. It takes the place of what a stroke, a slap or a fizzle left it to do, and a
	/// magic mushroom it eats while this waits does not make it high
	bool useWhatItHolds {false};
	/// What it thinks its player wants, from the player's miracles it saw, the strokes and slaps it was given and the
	/// town needs it saw the player's villagers see to
	creature_perceived_desires::PerceivedDesires perceivedDesires {};

	/// How far the creature has grown up, which decides the desires it has and which of its body's needs it feels, and
	/// the stage they were last set for. A creature the game makes starts at the first stage with every desire off, until
	/// a script or its mind file moves it (player_creature::StartAtFirstStage); one made by the debug tools or a test
	/// starts fully grown up, its desires set for that stage when its mind first thinks.
	uint32_t developmentPhase {k_FullyGrownUp};
	std::optional<uint32_t> desiresPhase;

	/// While paused, the mind leaves the body alone to be posed by hand
	bool paused {false};
	/// The player's hand took hold of it: once it has turned to the player it shows what it wants, and the help says it
	bool showDesireOnHold {false};
	/// Where a script has told it to look (SET_FOCUS): its head looks there every turn until the script releases it
	std::optional<glm::vec3> scriptFocus;
	/// The thing a script has told it to look at (SET_FOCUS_ON_OBJECT): its head looks at where the thing stands every
	/// turn until the script releases it or the thing is gone. A creature has one focus at a time: a point or a thing
	std::optional<entt::entity> scriptFocusThing;
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
	/// It carries out a plan its player ordered with the leash (the thing it was sent to act on, held up, looked over
	/// or planned for): while the plan has steps left its mind chooses nothing for itself, and a walk it is sent on with
	/// the leash then runs and waits there. Cleared once the plan has no step left, and as what it does stops as a
	/// failure or it stops what it was doing. Not the leash's obeying, which keeps the mind off the body for a walk
	bool commanded {false};
	/// An emergency started (badly hurt, on fire, a fire in the nearest town), and the creature did not stop what it does
	/// since then. It looks at no new emergency and does not react to things then.
	bool emergencyRunning {false};
	bool satisfiedByEffect {false};
	/// A step of the plan's agenda completed it, before the rest of the agenda
	bool completedByStep {false};
	/// The action of the last plan carried out to its end (0 until one is), which a town judges the creature by
	uint32_t lastCompletedAction {0};
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
	/// A stage of growing up a script gave it while a mind file waited to be taken up, put over the file's stage once it
	/// has been: the file's stage and desires first, then the script's stage
	std::optional<uint32_t> scriptStage;
	/// The name it was given as it was made (ecs::creature_naming), shown while its mind keeps none: a mind file's name
	/// replaces it, unless a computer player owns the creature
	std::u16string givenName;
	/// What a script taught it or took away (a skill or a miracle, see creature_watching::SetKnows) before its mind was
	/// set up or took up its file, in order, put over the file's once it has
	struct ScriptKnows
	{
		uint32_t kind;
		uint32_t action;
		bool knows;
		/// Whether the advisor tells the player of a skill or miracle it comes to know by it
		bool tell {false};
		/// A miracle taught counts as seen as often as it must be (CREATURE_SET_KNOWS_ACTION), or is only known
		/// about (CREATURE_LEARN_EVERYTHING)
		bool countsSightings {true};
	};
	std::vector<ScriptKnows> scriptKnows;

	/// What the leash tells the mind: the desire it forces, whether the creature is following it to the hand, and what
	/// the player has shown it on the leash
	creature_leash::MindHooks leash {};

	/// Where the thing stood that the player did something to which the creature last set about copying (the land's
	/// corner until then): it takes no tree from near there to plant
	glm::vec2 copiedAt {0.0f};
	/// It ate a magic mushroom, and becomes high once it is done with what it is doing
	bool highPending {false};
	/// The trees it planted others by most recently, by entity number, oldest first: it never takes one of them to plant
	std::vector<uint32_t> plantedBy;
};

/// A creature that learns nothing from the player's strokes and slaps: they only stop what it is doing. A creature a
/// script makes from another is made so; every other creature learns from them
struct CreatureLearningOff
{
};

} // namespace openblack::ecs::components
