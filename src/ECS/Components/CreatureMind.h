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
#include <entt/entity/entity.hpp>
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
#include "Enums.h"

namespace openblack::ecs::components
{

/// A plan a saved game had a creature carry out, which the game's load takes up as commanded: its action's row of the
/// game's table, the desire it serves, what it is for, done to and done with (by their numbers), the step of its agenda
/// it was at, for a look at the camera the turns it looks for, and the agenda when the load makes it from the saved one
/// (else the action makes it)
struct LoadedForcedPlan
{
	uint32_t action {0};
	creature_desires::Desire desire {creature_desires::Desire::ObeyPlayer};
	std::optional<uint32_t> activityObject;
	std::optional<uint32_t> object;
	std::optional<uint32_t> instrument;
	size_t step {0};
	/// The steps of its agenda, the ones done included
	size_t count {0};
	std::optional<uint32_t> turns;
	/// At a played pose, the seconds it is held for
	std::optional<float> seconds;
	std::optional<std::vector<creature_mind::Step>> agenda;
};

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
	/// The game turn it last carried out each action to its end, by the action's number, counted with actionCounts;
	/// an action it never carried out has none (as the game's turn 0)
	std::map<uint32_t, uint32_t> actionTurns;
	struct ActionUnderway
	{
		uint32_t serial;
		uint32_t action;
	};
	std::optional<ActionUnderway> underway;
	/// What a stroke or slap leaves it to do next, once it is free to choose: every slap leaves it to show how the slap
	/// made it feel, and a plain stroke or slap of a creature too young to learn from it, one time in two, to be
	/// frightened or show it is happy in its place; a stroke that teaches it to eat what it holds, to do again what it
	/// learns most from. A fizzled miracle takes its place, and it takes a fizzle's
	enum class AfterFeedback : uint8_t
	{
		Frightened,
		Happy,
		ShowSlapFeeling,
		RepeatLastAction,
		/// Shut in, once it has done what it was given to clear its way: it walks on to the kept walk point
		WalkToKeptPoint,
	};
	std::optional<AfterFeedback> afterFeedback;
	/// Given a thing by the player's hand, once it is free to choose it decides what to do with what it then holds,
	/// whether it took the thing or not. It takes the place of what a stroke, a slap or a fizzle left it to do, and a
	/// magic mushroom it eats while this waits does not make it high
	bool useWhatItHolds {false};
	/// The plan it carried out as it took up its last reaction, or as the player's hand gave it something when it
	/// followed no reaction: what a gift may then carry on
	std::optional<creature_planner::Plan> previousPlan;
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
	/// A script's play a saved game had it carry out, which it takes up again once its mind is set up: the plays left,
	/// the one it was at starting again from its beginning
	std::optional<creature_script_play::Request> loadedScriptPlay;
	/// A forced plan a saved game had it carry out, which it takes up again once its mind is set up, at the step it was
	/// at, that step starting again
	std::optional<LoadedForcedPlan> loadedForcedPlan;
	/// The desire of the fight a saved game had it start, which its plan takes up again once its mind is set up, the fight
	/// with its loaded duel's opponent, as the game's load keeps the starter's plan
	std::optional<creature_desires::Desire> loadedFightDesire;
	/// The legs left of a script's move a saved game had it on, each a point and the distance it walks to within of it,
	/// which it takes up again once its mind is set up
	std::vector<std::pair<glm::vec3, float>> loadedScriptMove;
	/// The turn a script's focus had it make in a saved game, which it takes up again once its mind is set up
	std::vector<creature_mind::Step> loadedScriptTurn;
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
	/// The plan forced on it came of its player's action (a god's hand forcing its creature's action sets it)
	bool forcedByPlayerAction {false};
	/// It set out to fight: set as an agenda with a fight is made, cleared as the action it does ends, whether or not it
	/// succeeded. Between the two, while not yet (or no longer) in the fight, it takes no order with the leash
	bool fightStarted {false};
	/// A teleport stone controls it, from turning aside into the stone until the stone lets go of it after the jump, or
	/// it stops what it was doing: it takes up no reaction, and the area it is kept within doesn't call it back
	bool controlledByTeleport {false};
	/// The first search of a walk found it shut in, until it next stops what it was doing (any forced plan stops it):
	/// its routes then keep clear of things by its drawn size, a walk's first search goes no further than the others,
	/// and the area it is kept within does not call it back
	bool trapped {false};
	/// Where the walk that found it shut in was going, on the ground, for the walk on once it has cleared its way
	std::optional<glm::vec2> keptWalkPoint;
	/// The agenda (by the idle mind's count of them) that the creature last set its kept speed for
	uint32_t paceSerial {0};
	/// The turns of its mind not under a script's control since it last stopped what it was doing: what the actions
	/// serving the stopped plan's desire are aged by when it stops
	uint32_t turnsPerformingAction {0};
	/// The actions, by their row of the game's table, whose plan was found to walk it out of the area it is kept within as
	/// it was made: while it is kept within an area it does not take them, until a leash comes off it
	std::vector<bool> unavailableWhenConfined;
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
	/// The mind file was loaded into it by the creature-file load (LOAD_CREATURE, a land script's, a computer god's
	/// start): once taken up, it tells the player the stage the file holds, as a swap of minds does
	bool fileTellsStage {false};
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
	/// Having seen a nasty miracle it knows while a leash of its player's was on, it casts the miracle at the thing it
	/// found near its player's hand once it is free to choose. It takes the place of what a stroke, a slap, a fizzle or a
	/// gift left it to do, and of the high after a magic mushroom; any of those set later takes its place
	struct LeashCast
	{
		entt::entity thing {entt::null};
		MagicType magic {MagicType::None};
	};
	std::optional<LeashCast> leashCast;
	/// The trees it planted others by most recently, by entity number, oldest first: it never takes one of them to plant
	std::vector<uint32_t> plantedBy;
	/// The last deed of the player that it started to copy; none until it starts to copy one. Its wish to follow the
	/// player is planned only once it has one
	std::optional<creature_watching::ObservedDeed> observedDeed;
	/// The thing its player's deed was done to, by entity number, whose noticing it plays at its next decision once free
	std::optional<uint32_t> noticePending;
	/// The action it last noticed one of its player's deeds with, for the debug readout; empty until it notices one
	std::string noticedWith;
	/// Where the miracle stood that it last reacted to by running away or going to look: the place those plans go about
	std::optional<glm::vec2> miraclePlace;
};

/// A creature that learns nothing from the player's strokes and slaps: they only stop what it is doing. A creature a
/// script makes from another is made so; every other creature learns from them
struct CreatureLearningOff
{
};

} // namespace openblack::ecs::components
