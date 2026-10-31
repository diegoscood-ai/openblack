/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureReactionRules.h"
#include "Enums.h"

namespace openblack::creaturemind
{
struct MindFileData;
}
namespace openblack::creature_mind_tables
{
struct Tables;
}

namespace openblack::ecs::systems
{

/// The script natives that act on a creature's desires, body or walk, as the debug tools list their last calls
enum class CreatureNative : uint8_t
{
	TurnOffAllDesires,
	ForceFinish,
	ArsePosition,
	BellyPosition,
	PosValidForCreature,
	/// A script sending a creature in the map to a point
	MoveGameThing,
	/// A script forcing an action on a creature
	DoAction,
};
constexpr size_t k_CreatureNativeCount = 7;

/// For the debug tools only: a script native's last call, kept outside the registry so that it never reaches the
/// state. The thing it was given, by its entity's number (0 for none), the point it gave or was given, whether it did
/// what it is for, and what it said, a message of static text
struct CreatureNativeCall
{
	uint32_t object {0};
	glm::vec3 point {0.0f};
	bool done {false};
	std::string_view note;
	/// How many times it was called
	uint32_t calls {0};
	/// The game turn of the last call, where it is kept
	uint32_t turn {0};
};

/// The creatures' minds: once a game turn their desires grow and fade, and each decides what its body does while it
/// has nothing better to do, and what it looks at (see components::CreatureMindState). Creatures can also be told to
/// do things, which the debug tools use.
class CreatureMindSystemInterface
{
public:
	virtual ~CreatureMindSystemInterface() = default;

	virtual void ProcessTurn() = 0;
	/// Once a game turn after ProcessTurn: each creature plans two of its desires and changes what it does for a plan
	/// pressing enough
	virtual void PlanTurn() = 0;
	/// Once a game turn after PlanTurn: what the leash taught, and copying the player, step on
	virtual void LearnTurn() = 0;

	/// Takes up a mind file's learning, desires and stage of growing up at the next turn
	virtual void LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind) = 0;
	/// The creature's mind as a mind file of the current version, over the file it was loaded from, if it has a mind
	[[nodiscard]] virtual std::optional<creaturemind::MindFileData> SaveMind(entt::entity creature) const = 0;
	/// Swaps the minds of two creatures at once, as a script does: everything the mind file keeps (what each has learnt
	/// and how it feels, its physique and tattoos) moves to the other body, while each body keeps its own name, size and
	/// alignment; the player the first belonged to now owns the second and the other way round, and both carry out the
	/// swap, during which each body takes on the size and alignment the other had. Returns whether it could.
	virtual bool SwapMinds(entt::entity /*from*/, entt::entity /*to*/) { return false; }
	/// Forgets everything learnt: the desires start again as the species' do, with no opinions or examples
	virtual void ClearLearning(entt::entity creature) = 0;
	/// SET_FOCUS on a creature: it is taken under script control if it is not yet, stops what it was doing and turns to
	/// face the point, its head looking there until it is released
	virtual void FocusByScript(entt::entity creature, const glm::vec3& point) = 0;
	/// RELEASE_FROM_SCRIPT on a creature, after its control is cleared: its focus off, and what it was doing given up
	virtual void ReleasedByScript(entt::entity creature) = 0;
	/// SET_FOCUS_ON_OBJECT on a creature: it is taken under script control if it is not yet, stops what it was doing and
	/// turns to face the thing, holding still two seconds once it does, its head on the thing until it is released or
	/// the thing is gone
	virtual void FocusOnThingByScript(entt::entity /*creature*/, entt::entity /*thing*/) {}
	/// RELEASE_OBJECT_FOCUS on a creature: its head is free to look about again; it stays under the script's control
	virtual void ReleaseFocusByScript(entt::entity /*creature*/) {}
	/// SET_SCRIPT_ULONG on a creature: the action it is to play when a script next sets its state, by its number in
	/// the creature's animations, and how many times
	virtual void SetScriptPlay(entt::entity /*creature*/, uint32_t /*animation*/, uint32_t /*plays*/) {}
	/// SET_SCRIPT_STATE on a creature: taken under the script's control, it gives up what it was doing and plays what
	/// the script last gave it, then waits for its body to be free, obeying its player. Whether it could
	virtual bool PlayForScript(entt::entity /*creature*/) { return false; }
	/// MOVE_GAME_THING on a creature in the map: taken under the script's control, it gives up what it was doing and
	/// walks to the point in two goes, its head on the point, obeying its player: within a distance twice when one is given,
	/// else first within a sixth of the way (its height when that is more, else no more than 0.7 of its height and 22), then
	/// within its height. Whether it could
	virtual bool MoveForScript(entt::entity /*creature*/, const glm::vec3& /*point*/, float /*distance*/) { return false; }
	/// CREATURE_DO_ACTION: a script has the creature carry out an action of the game's table (by its row) on a thing,
	/// with another or none, to stage a scene. Taken under the script's control, it gives up what it was doing and
	/// carries the action out as a plan of obeying its player, at the most a plan can be pressing. A creature made in
	/// the same turn has its mind set up for it first. Whether the action could be carried out
	virtual bool ScriptDoAction(entt::entity /*creature*/, uint32_t /*action*/, entt::entity /*target*/,
	                            std::optional<entt::entity> /*with*/)
	{
		return false;
	}
	/// PLAYED on a creature: whether it has played what it was doing (its agenda is over, or it only idles). Saying so,
	/// what is left of its agenda is given up, as the game's answer clears it
	virtual bool HasPlayed(entt::entity /*creature*/) { return true; }
	/// Something stops what the creature is doing, which it gives up as a failure, as a spell freezing it does
	virtual void AbandonAction(entt::entity creature) = 0;
	/// Gives up the plan it carries out and what it was doing for it, the plan's priority then nothing
	virtual void GiveUpPlan(entt::entity creature) = 0;
	/// How pressing what it carries out counts as, which a plan of its own must be more than twice to replace it
	virtual void SetAgendaPriority(entt::entity creature, float priority) = 0;
	/// A script changes the creature's desires. A creature whose mind hasn't thought yet has them set up first, the ones
	/// its stage of growing up brings already active, so that what the script sets stands.
	virtual void ChangeDesires(entt::entity /*creature*/, const std::function<void(creature_desires::Desires&)>& /*change*/) {}
	/// A script makes the creature stop what it does at once, as a failure. The action that its body plays ends at two
	/// times the usual speed, and the creature stops where it is.
	virtual void ForceFinish(entt::entity /*creature*/) {}
	/// For the debug tools: a script native's call is noted, its count added to, and its last call read back
	virtual void NoteNative(CreatureNative /*native*/, const CreatureNativeCall& /*call*/) {}
	[[nodiscard]] virtual std::optional<CreatureNativeCall> LastNative(CreatureNative /*native*/) const { return std::nullopt; }
	/// Creatures that can see the point watch an ordinary skill practised (by its row in the game's table), or a miracle
	/// cast
	virtual void SeeSkill(const glm::vec3& point, size_t skill) = 0;
	virtual void SeeMiracle(const glm::vec3& point, size_t miracle) = 0;
	/// The creature watches an ordinary skill done (by its row in the game's table), as a reaction to the hand or a
	/// villager it sees shows it: it counts towards learning the skill, and the advisor tells of it
	virtual void WatchSkill(entt::entity /*creature*/, size_t /*skill*/) {}
	/// The player did one of the deeds creatures copy (by its row in the game's table) at a point, maybe to something.
	/// Only that player's own creatures watch it.
	virtual void PlayerDid(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object) = 0;
	/// The same for a deed done with a miracle: before the creature copies it, it may count a sighting of the miracle
	virtual void PlayerCastDeed(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	                            MagicType /*magic*/)
	{
		PlayerDid(player, deed, point, object);
	}
	/// The player was seen to want one of the creature's desires, or one of a town's, at a point: the player's own
	/// creature, if it has one and can see the point, thinks the player wants it more by the weight
	virtual void EmpathiseWithPlayer(PlayerNames player, CreatureDesires desire, float weight, const glm::vec3& point) = 0;
	virtual void EmpathiseWithTownDesire(PlayerNames player, TownDesireInfo desire, float weight, const glm::vec3& point) = 0;
	/// CREATURE_SET_KNOWS_ACTION: the creature knows an ordinary skill (kind 0, by its row in the game's table) or a
	/// miracle (kind 1, by its magic type), or no longer does (see creature_watching::SetKnows). A mind not yet set up,
	/// or with a file still to take up, keeps it for when it has, so that it stays over the file's. False for anything
	/// but a creature (nothing is changed)
	virtual bool SetKnowsAction(entt::entity creature, uint32_t kind, uint32_t action, bool knows) = 0;
	/// CREATURE_LEARN_EVERYTHING: the creature knows every ordinary skill and knows about every miracle, without a
	/// miracle's sightings changing; kept for later as SetKnowsAction's. False for anything but a creature
	virtual bool LearnEverything(entt::entity /*creature*/) { return false; }
	/// A nasty miracle struck near the creature, at a point: it grows afraid, then runs from it, or goes to look at it
	/// when it is on the learning leash or not afraid at all, and learns the miracle (by its magic type) by seeing it when
	/// one is given
	virtual void ReactToNastyMagic(entt::entity /*creature*/, const glm::vec3& /*point*/, std::optional<size_t> /*learn*/) {}
	/// It goes to look at a nice miracle, and learns it by seeing it when one is given
	virtual void ReactToNiceMagic(entt::entity /*creature*/, const glm::vec3& /*point*/, std::optional<size_t> /*learn*/) {}
	/// The creature takes up a reaction to a thing (another creature, a flying thing, the hand picking something up): it
	/// comes across the thing and drops what it was doing for the plan the reaction makes (done to the thing or to
	/// itself), as the reaction's kind says
	virtual void TakeUpReaction(entt::entity /*creature*/, entt::entity /*thing*/,
	                            const creature_reaction_rules::Setup& /*setup*/)
	{
	}
	/// The creature takes up a fire's reaction: it comes across the burning thing and what that belongs to (its town,
	/// its flock, or the creature itself when it is the one burning), then goes to put the fire out with the water miracle
	/// when what burns is of enough use to its compassion and it has seen the miracle often enough to cast it, and runs
	/// from the burning thing otherwise
	virtual void ReactToFire(entt::entity /*creature*/, entt::entity /*burning*/) {}
	/// The creature weighs looking another creature over: it comes across it, and when it never has looked it over, or
	/// not for more than the wait in turns, it is due a look, and notes the turn as when it last looked. Whether it is due
	[[nodiscard]] virtual bool NoticeCreatureToInspect(entt::entity /*creature*/, entt::entity /*other*/, uint32_t /*turn*/,
	                                                   uint32_t /*waitTurns*/)
	{
		return false;
	}
	/// The game's tables the minds use, once the game's data is loaded
	[[nodiscard]] virtual const creature_mind_tables::Tables* GetTables() = 0;
	/// The action of the game's table the creature's mind carries out now, which its body goes by; none while it does
	/// nothing that counts as one, or for anything but a creature with a mind
	[[nodiscard]] virtual std::optional<uint32_t> CurrentActionOf(entt::entity /*creature*/) { return std::nullopt; }
	/// The plan the creature holds as others see it (the planner's, else what its idle mind does as a plan of the row it
	/// counts as); none once a plan has ended and before the next, or for anything but a creature with a mind
	[[nodiscard]] virtual std::optional<creature_planner::Plan> HeldPlanOf(entt::entity /*creature*/) { return std::nullopt; }

	/// Plays an action once, unless the creature's body already plays one: mirrored or not as given, else by a coin the
	/// game's random numbers toss (so a caller outside the turn passes it)
	virtual bool PlayAction(entt::entity creature, size_t animation, std::optional<bool> mirrored = std::nullopt) = 0;
	/// Plays a gesture on top of the body, unless one already plays
	virtual bool PlayGesture(entt::entity creature, size_t animation) = 0;
	/// Pulls a face for a few seconds, as told to by hand
	virtual void PullFace(entt::entity creature, size_t animation) = 0;
	/// Pulls the face a feeling or what it is doing calls for, as the creature's mind would; returns the face pulled
	virtual std::optional<creature_face::Request> ShowFeeling(entt::entity creature, creature_face::Cue cue) = 0;
	/// Sits down for a while, unless the body plays an action
	virtual bool SitDown(entt::entity creature) = 0;
	/// Gets up from sitting
	virtual void StandUp(entt::entity creature) = 0;
	/// The player's hand let go of the creature having stroked or slapped it, from -1 (slapped) to 1 (stroked): it
	/// stops what it was doing if slapped, warms or cools to the player, and shows its pleasure or sorrow next. Feedback
	/// too slight to count only has it look at the player.
	virtual void ReceiveFeedback(entt::entity creature, float feedback) = 0;
	/// The player's hand took hold of the creature: its mind stands still while held, it turns to face the player, and
	/// once its body is free it shows what it wants
	virtual void HandTookHold(entt::entity /*creature*/) {}
	/// Feedback that only moves how the creature feels about its player (as being hit by what its player threw does):
	/// it takes the player to want compassion or anger, and warms or cools to the player, from -1 to 1
	virtual void UpdateAttitudeFromFeedback(entt::entity /*creature*/, float /*feedback*/) {}
	/// Every source of a type in the creature's desires goes up or down by an amount
	virtual void ChangeDesireSource(entt::entity /*creature*/, uint32_t /*type*/, float /*amount*/) {}
	/// Plays an action at once, as stroking and slapping make it, unless the body is no further than a share of the way
	/// through what it plays (creature_feedback::MayInterrupt); a face can be pulled with it, which is pulled even when
	/// the action can't play. Returns whether it played.
	virtual bool ForceAction(entt::entity creature, size_t animation, bool mirrored, std::optional<creature_face::Request> face,
	                         double interruptsAfter) = 0;

	/// Sees to a need now, in place of whatever it was doing: sleeps until rested, eats something (the nearest food when
	/// none is given), goes and drinks at the nearest water, has a poo, is sick, or faints. Returns whether it could.
	virtual bool Sleep(entt::entity creature) = 0;
	virtual bool Eat(entt::entity creature, std::optional<entt::entity> food) = 0;
	virtual bool Drink(entt::entity creature) = 0;
	virtual bool Poo(entt::entity creature) = 0;
	virtual bool Puke(entt::entity creature) = 0;
	virtual bool Faint(entt::entity creature) = 0;
	/// A meal has ended as its hands judged it: the game's type of what it ate, its food value, and how it ended. Eaten,
	/// the type's eaten count goes up and the meal takes its hunger down; not food, the type's not-food count goes up,
	/// its hunger is held back and it is sick; poisoned, the same but for the count
	virtual void EndMeal(entt::entity creature, uint32_t objectType, float foodValue, creature_eating::MealEnd end) = 0;
	/// The creature has eaten a magic mushroom: once it is done with what it is doing it becomes high, unless it is about
	/// to already
	virtual void AteMagicMushroom(entt::entity /*creature*/) {}
	/// Wakes it, or ends whatever it is sitting, lying or squatting through
	virtual void Wake(entt::entity creature) = 0;

	/// A fight the creature was in ended, won or lost: fighting satisfies the desire it is for, and its body pays for it
	virtual void FoughtFight(entt::entity creature, bool won) = 0;
	/// The creature tries a miracle at an object, as its casting pose's loop begins or in a fight: in a fight it pays
	/// stamina first; a try short of the sightings it needs fizzles; a miracle it can't cast frustrates it. Whether it
	/// was cast.
	virtual bool TryMiracle(entt::entity /*creature*/, MagicType /*type*/, entt::entity /*target*/) { return false; }
	/// A plan forced on the creature: the desire it serves, what it is done for, the action by its name in the game's
	/// table, the thing it is done to and the thing it is done with
	struct ForcedActivity
	{
		creature_desires::Desire desire {creature_desires::Desire::Fear};
		std::optional<entt::entity> activityObject;
		std::string_view action;
		entt::entity actionObject {entt::null};
		std::optional<entt::entity> instrument;
		/// How pressing the plan is: most often the most a score can be
		float score {std::numeric_limits<float>::max()};
		/// The agenda it carries out, in place of the one the action would make
		std::optional<std::vector<creature_mind::Step>> agenda;
		/// Whether the action's own test of whether it can be done now applies, as when the planner chooses it: a
		/// stroke's do-it-again asks it; a script's, a reaction's and the other forced plans do not
		bool checkedAsChosen {false};
		/// Whether it is remembered for the player's feedback as forced on it by its player: every forced plan but a
		/// reaction's (another creature, a flying thing, the hand picking something up, a fire)
		bool byPlayer {true};
	};
	/// The reaction the creature follows stops (its record refreshed), what it was doing stops as a failure and it
	/// carries out the plan instead: whether it could. A plan carried out is remembered for the player's feedback (unless
	/// nothing says why it wants it), and the desires the plan's desire opposes are held back. A stroke forces one to do
	/// again what it was stroked for.
	virtual bool ForceActivity(entt::entity /*creature*/, const ForcedActivity& /*plan*/) { return false; }
	/// The player's hand gives the creature a thing: it drops the reaction it follows and what it was doing, turns to the
	/// camera at a point on the ground looking curious, and reaches to take the thing from the hand, with its left hand
	/// when asked; then its desires choose what to do with it. Whether it took the giving.
	virtual bool TakeFromHand(entt::entity /*creature*/, entt::entity /*object*/, bool /*leftHand*/, glm::vec2 /*camera*/)
	{
		return false;
	}
	/// Whether the creature is still about taking a thing from the hand: turning to the player or reaching for it
	[[nodiscard]] virtual bool IsTakingFromHand(entt::entity /*creature*/) const { return false; }
	/// Whether the creature's plan is the action of the game's table by that name
	[[nodiscard]] virtual bool PlansAction(entt::entity /*creature*/, std::string_view /*action*/) { return false; }
	/// Whether the creature is free to take up a reaction of a type, as the reactions ask it
	[[nodiscard]] virtual bool IsAvailableForReaction(entt::entity /*creature*/, uint8_t /*reaction*/) { return false; }
	/// Whether another creature seems friendly to the creature: the other creature is not angry with it or afraid of it,
	/// and the creature is not angry with the other
	[[nodiscard]] virtual bool SeemsFriendly(entt::entity /*creature*/, entt::entity /*other*/) { return false; }
	/// For the debug tools: the creature knows a miracle (by its magic type) as if it had learnt it, and is told to cast
	/// one at a thing, going about it as it would by itself
	virtual void KnowMiracle(entt::entity /*creature*/, size_t /*miracle*/) {}
	virtual bool TellCast(entt::entity /*creature*/, MagicType /*type*/, entt::entity /*target*/) { return false; }

	/// How strongly the player last rewarded or punished the creature, and setting it back to 0
	[[nodiscard]] virtual float GetInteractionMagnitude(entt::entity /*creature*/) const { return 0.0f; }
	virtual void ClearInteractionMagnitude(entt::entity /*creature*/) {}
	/// How many times the creature has carried out an action (by the game's numbering) to its end
	[[nodiscard]] virtual uint32_t GetActionCount(entt::entity /*creature*/, uint32_t /*action*/) const { return 0; }
	/// A script makes a desire the creature's only one for some seconds, or takes it away again
	virtual void SetOnlyDesire(entt::entity /*creature*/, creature_desires::Desire /*desire*/, float /*seconds*/) {}
	virtual void ClearOnlyDesire(entt::entity /*creature*/) {}
	/// The creature is told to point out the lesson highlight about it, if there is one; whether one was found
	virtual bool PointOutHighlight(entt::entity /*creature*/) { return false; }
	/// The story moves the creature to a stage of growing up: its desires become the ones its stages so far bring at once,
	/// even for the stage it is at already, none stays held back, and it gives up its plan
	virtual void MoveToStage(entt::entity /*creature*/, uint32_t /*stage*/) {}
};

} // namespace openblack::ecs::systems
