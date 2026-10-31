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
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::components
{
struct Creature;
}

namespace openblack::ecs::systems
{

class CreatureMindSystem final: public CreatureMindSystemInterface
{
public:
	void ProcessTurn() override;
	void PlanTurn() override;
	void LearnTurn() override;

	void LoadMind(entt::entity creature, std::shared_ptr<const creaturemind::MindFileData> mind) override;
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity creature) const override;
	void ClearLearning(entt::entity creature) override;
	void FocusByScript(entt::entity creature, const glm::vec3& point) override;
	void ReleasedByScript(entt::entity creature) override;
	void FocusOnThingByScript(entt::entity creature, entt::entity thing) override;
	void ReleaseFocusByScript(entt::entity creature) override;
	void SetScriptPlay(entt::entity creature, uint32_t animation, uint32_t plays) override;
	bool PlayForScript(entt::entity creature) override;
	bool HasPlayed(entt::entity creature) override;
	bool MoveForScript(entt::entity creature, const glm::vec3& point, float distance) override;
	bool ScriptDoAction(entt::entity creature, uint32_t action, entt::entity target, std::optional<entt::entity> with) override;
	void AbandonAction(entt::entity creature) override;
	void GiveUpPlan(entt::entity creature) override;
	void SetAgendaPriority(entt::entity creature, float priority) override;
	void ChangeDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change) override;
	void ForceFinish(entt::entity creature) override;
	void NoteNative(CreatureNative native, const CreatureNativeCall& call) override;
	[[nodiscard]] std::optional<CreatureNativeCall> LastNative(CreatureNative native) const override
	{
		return _lastNatives.at(static_cast<size_t>(native));
	}
	void SeeSkill(const glm::vec3& point, size_t skill) override;
	void SeeMiracle(const glm::vec3& point, size_t miracle) override;
	void WatchSkill(entt::entity creature, size_t skill) override;
	void PlayerDid(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object) override;
	void PlayerCastDeed(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	                    MagicType magic) override;
	void EmpathiseWithPlayer(PlayerNames player, CreatureDesires desire, float weight, const glm::vec3& point) override;
	void EmpathiseWithTownDesire(PlayerNames player, TownDesireInfo desire, float weight, const glm::vec3& point) override;
	bool SetKnowsAction(entt::entity creature, uint32_t kind, uint32_t action, bool knows) override;
	bool LearnEverything(entt::entity creature) override;
	bool TryMiracle(entt::entity creature, MagicType type, entt::entity target) override;
	bool ForceActivity(entt::entity creature, const ForcedActivity& forced) override;
	bool SwapMinds(entt::entity from, entt::entity to) override;
	bool TakeFromHand(entt::entity creature, entt::entity object, bool leftHand, glm::vec2 camera) override;
	[[nodiscard]] bool IsTakingFromHand(entt::entity creature) const override;
	[[nodiscard]] bool PlansAction(entt::entity creature, std::string_view action) override;
	[[nodiscard]] bool IsAvailableForReaction(entt::entity creature, uint8_t reaction) override;
	[[nodiscard]] bool SeemsFriendly(entt::entity creature, entt::entity other) override;
	void KnowMiracle(entt::entity creature, size_t miracle) override;
	bool TellCast(entt::entity creature, MagicType type, entt::entity target) override;
	[[nodiscard]] float GetInteractionMagnitude(entt::entity creature) const override;
	void ClearInteractionMagnitude(entt::entity creature) override;
	[[nodiscard]] uint32_t GetActionCount(entt::entity creature, uint32_t action) const override;
	void SetOnlyDesire(entt::entity creature, creature_desires::Desire desire, float seconds) override;
	void ClearOnlyDesire(entt::entity creature) override;
	bool PointOutHighlight(entt::entity creature) override;
	void MoveToStage(entt::entity creature, uint32_t stage) override;
	void ReactToNastyMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn) override;
	void ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn) override;
	void TakeUpReaction(entt::entity creature, entt::entity thing, const creature_reaction_rules::Setup& setup) override;
	void ReactToFire(entt::entity creature, entt::entity burning) override;
	[[nodiscard]] bool NoticeCreatureToInspect(entt::entity creature, entt::entity other, uint32_t turn,
	                                           uint32_t waitTurns) override;
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override;
	[[nodiscard]] std::optional<uint32_t> CurrentActionOf(entt::entity creature) override;
	[[nodiscard]] std::optional<creature_planner::Plan> HeldPlanOf(entt::entity creature) override;

	bool PlayAction(entt::entity creature, size_t animation, std::optional<bool> mirrored = std::nullopt) override;
	bool PlayGesture(entt::entity creature, size_t animation) override;
	void PullFace(entt::entity creature, size_t animation) override;
	std::optional<creature_face::Request> ShowFeeling(entt::entity creature, creature_face::Cue cue) override;
	bool SitDown(entt::entity creature) override;
	void StandUp(entt::entity creature) override;
	void ReceiveFeedback(entt::entity creature, float feedback) override;
	void HandTookHold(entt::entity creature) override;
	void UpdateAttitudeFromFeedback(entt::entity creature, float feedback) override;
	void ChangeDesireSource(entt::entity creature, uint32_t type, float amount) override;
	bool ForceAction(entt::entity creature, size_t animation, bool mirrored, std::optional<creature_face::Request> face,
	                 double interruptsAfter) override;
	bool Sleep(entt::entity creature) override;
	bool Eat(entt::entity creature, std::optional<entt::entity> food) override;
	bool Drink(entt::entity creature) override;
	bool Poo(entt::entity creature) override;
	bool Puke(entt::entity creature) override;
	void EndMeal(entt::entity creature, uint32_t objectType, float foodValue, creature_eating::MealEnd end) override;
	void AteMagicMushroom(entt::entity creature) override;
	bool Faint(entt::entity creature) override;
	void Wake(entt::entity creature) override;
	void FoughtFight(entt::entity creature, bool won) override;

private:
	/// It is made to be high, as after eating a magic mushroom
	bool BecomeHigh(entt::entity creature);
	/// Sets up what a creature has learnt the first time its mind thinks, from its mind file when it has one
	void SetUpLearning(entt::entity creature, components::CreatureMindState& mind);
	/// Gives a creature's mind its desires and what it has learnt if it has not got them yet, as its first turn does, so
	/// that a creature can be told to do something in the turn it was made
	void SetUpMind(entt::entity creature, CreatureType species, components::CreatureMindState& mind);
	/// Takes up a mind file waiting to be loaded
	void TakeUpFile(entt::entity creature, components::CreatureMindState& mind);
	/// Puts the stage a script gave it while its file waited over the file's: the stage, its desires at once, and the
	/// turns at the stage counted from none
	void TakeUpScriptStage(components::CreatureMindState& mind);
	/// How a creature's swap of minds stands this turn: whether the spells on both creatures have gone and whether its
	/// body has taken on its new shape (CreatureMindSwap.cpp)
	[[nodiscard]] static creature_mind::Senses::SwapView SwapViewOf(entt::entity creature);
	/// What the idle mind told a creature carrying out a swap to do this turn
	static void StepSwap(entt::entity creature, const creature_mind::Commands& commands);
	/// What a script taught a creature or took away, in order, on what it has learnt (nothing without the tables). Gives
	/// the advisor's lessons for what an edit to be told of made it know that it did not know before
	[[nodiscard]] static std::vector<creature_help::Message>
	TakeUpScriptKnows(std::span<const components::CreatureMindState::ScriptKnows> edits,
	                  const creature_mind_tables::Tables* tables, float speciesMultiplier, creature_mind_model::Learnt& learnt);
	/// Teaches a skill or a miracle or takes it away, now or once the mind has taken up its file, telling the player of
	/// what it comes to know when asked to
	bool ChangeKnows(entt::entity creature, const components::CreatureMindState::ScriptKnows& edit);
	/// What a script teaches a creature or takes away, in order: at once on a mind set up with no file to take up, else
	/// kept for when it has, telling the player of what it comes to know by the edits to be told of. False for anything
	/// but a creature
	bool ApplyScriptKnows(entt::entity creature, std::span<const components::CreatureMindState::ScriptKnows> edits);
	/// The ordinary skills and the miracle rows CREATURE_LEARN_EVERYTHING teaches
	static constexpr uint32_t k_LearnEverythingSkills = 6;
	static constexpr uint32_t k_LearnEverythingMiracles = 42;
	/// Remembers what the idle mind has started, for feedback to be credited to, and finishes plans that are done
	void FollowAgenda(entt::entity creature, components::CreatureMindState& mind);
	/// Carries out a plan in place of what the creature was doing; returns whether it could
	bool Adopt(entt::entity creature, components::CreatureMindState& mind, const creature_planner::Plan& plan,
	           const creature_plan_actions::Situation& situation);
	/// Gives up the plan carried out, if any
	static void Abandon(components::CreatureMindState& mind);
	/// A joint action (CreatureMindFriends.cpp): the asker asks another creature to be its partner, and the two become
	/// partners when it agrees; the partner takes an order of the asker, which it does and then waits; the asker makes
	/// the partner's mind stand still or go free. The joint action stops for both when either takes up another agenda
	/// or comes to the end of its agenda (see components::CreaturePartner).
	bool RequestPartner(entt::entity asker, entt::entity asked, creature_friendship::RequestKind kind);
	void TakeOrder(entt::entity partner, entt::entity asker, const creature_mind::Step& ordered);
	static void SetDemobilised(entt::entity partner, bool demobilised);
	void EndJointAction(entt::entity creature);
	void KeepJointActions();
	/// What the joint actions with a friend need to know before a friend is chosen: how strong the desires are that let
	/// them start, and whether the nearest town is to be helped or attacked
	void SituateFriends(entt::entity creature, creature_plan_actions::Situation& situation);
	/// What a joint action with a friend needs to know of the friend: where it is and how tall it is, the clear place
	/// that the two use, the coast and the sea where they drink, and the town that they help or attack. Only the joint
	/// actions are told; any other action's situation stays as it is
	void SituateFriend(entt::entity creature, const creature_plan_actions::Executor& executor, entt::entity friendly,
	                   creature_plan_actions::Situation& situation);
	/// The emergencies it looks at before it decides, each turn out of the hand: low life, a fire on its body, and a fire
	/// in the nearest town (CreatureMindEmergencies.cpp). A desire made dominant lasts its seconds at the rate given.
	void StepEmergencies(entt::entity creature, const components::Creature& body, components::CreatureMindState& mind,
	                     bool scripted, float dominantRate);
	/// Whether the player's camera looks at the creature, for whether it saw an action finish
	[[nodiscard]] static bool PlayerLooksAt(entt::entity creature);
	/// Learns what feedback teaches, from what the creature did lately
	void LearnFromFeedback(entt::entity creature, components::CreatureMindState& mind, float feedback);
	/// Remembers an action for feedback; an action forgotten to make room can teach approval (CreatureMindLearning.cpp)
	void RememberAction(entt::entity creature, components::CreatureMindState& mind, creature_learning::Context context);
	/// Held by the player's hand, it shows what it wants once its body is free
	void ShowDesireOnHold(entt::entity creature, components::CreatureMindState& mind, components::CreatureAnimation& animation);
	/// Its desires choose what it does with a thing it holds after the hand gave it something
	void UseGift(entt::entity creature, entt::entity object);
	/// Given something by the hand, once it is free to choose it decides what to do with what it then holds (with
	/// nothing in its hand, nothing), in place of planning that turn: whether it had that to do
	bool UseWhatItWasGiven(entt::entity creature, components::CreatureMindState& mind);
	/// Plans the desires due this turn for one creature, at most once a turn. A desire forced on a thing is planned even
	/// under a script's control, which otherwise stops all planning
	void PlanCreature(entt::entity creature, components::CreatureMindState& mind, bool forcedOnThing = false);
	/// The current action stops as a failure, the desire becomes fully dominant, and the creature plans its desires for
	/// the thing only, at once. Whether it found a plan for that desire
	bool ForceDesireOn(entt::entity creature, creature_desires::Desire desire, entt::entity thing);
	/// The game's synchronised random numbers: from 0 to n - 1, and from 0 to 1
	static uint32_t Random(uint32_t range);
	static float Chance();

	/// One creature sees a miracle (by its magic type) and may learn it from the sightings, unless its mind is stilled
	void WatchMiracle(entt::entity creature, size_t miracle);
	/// A deed of a player, with the miracle it was done with (0 for none), that the player's creatures may copy
	void NoticeDeed(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	                uint32_t magic);
	/// Plans an activity in place of what the creature was doing, getting it up and stopping it first
	bool Replan(entt::entity creature, creature_mind::Activity activity, std::vector<creature_mind::Step> agenda);
	/// The agenda of an action of the game's table on a thing, and the executor that makes it, if it can be done
	std::optional<std::pair<const creature_plan_actions::Executor*, std::vector<creature_mind::Step>>>
	PlanAgenda(entt::entity creature, uint32_t action, std::optional<uint32_t> object,
	           const creature_plan_actions::Situation& situation);
	/// The places of the creature's map of the land that the action uses: where it goes or looks, and the sea
	/// (CreatureMindExploration.cpp)
	static void SituateExploration(entt::entity creature, const creature_plan_actions::Executor& executor,
	                               creature_plan_actions::Situation& situation);
	/// A step of the plan's agenda said that its action counts as done: the plan is completed there, once, before the
	/// rest of its agenda
	void CompleteByStep(entt::entity creature, components::CreatureMindState& mind);

	// Its dances with the villagers of a town (CreatureMindDance.cpp)
	/// The creature's dance as its mind sees it, while a dance step is next or under way. It notes where each dancer
	/// stands, to tell next turn which dancers move.
	[[nodiscard]] static std::optional<creature_mind::Senses::DanceView> DanceViewOf(entt::entity creature,
	                                                                                 const creature_mind::IdleMind& idle);
	/// Carries out a stage of a dance with villagers that the mind ordered
	static void CarryOutDance(entt::entity creature, creature_mind::IdleMind& idle, const creature_mind::DanceOrder& order);
	/// The creature's dance stops when the creature stops dancing in it, as when it gives up its agenda
	static void KeepDanceWithAgenda(entt::entity creature, const creature_mind::IdleMind& idle);
	/// The clear place of a dance with a villager, about the villager; none when there is none
	[[nodiscard]] static std::optional<glm::vec2> DanceAreaFor(entt::entity creature, glm::vec2 villager);
	/// A dance with a villager is about to be planned: a dance of a row the creature notes is noted, and the agenda learns
	/// the clear place about the villager and what the dance's row of the table asks for
	static void SituateDance(entt::entity creature, const creature_plan_actions::Executor& executor, glm::vec2 villager,
	                         creature_plan_actions::Situation& situation);
	/// The creature starts to make a dance that it notes: it notes the turn and the row of the dance
	static void NoteImpressiveDance(entt::entity creature, uint32_t type);
	/// Whether the creature may choose a dance of a row of the dance table now
	[[nodiscard]] static bool MayDanceAgain(entt::entity creature, uint32_t type);

	// The things it has come across (CreatureMindBeliefs.cpp)
	/// What a thing belongs to, as a creature sees it: a villager's, abode's or field's town, an animal's flock, a
	/// creature itself; nothing for anything else
	[[nodiscard]] static std::optional<entt::entity> BelongsTo(entt::entity thing);
	/// It comes across a thing: a thing new to it is remembered, and leads it on to what the thing belongs to; a thing it
	/// knows already is seen again (StirSourcesOnSighting, and WatchVillager for a villager)
	void ComeAcross(entt::entity creature, components::CreatureMindState& mind, entt::entity thing);
	/// Seeing again a thing it has a belief about stirs some of its desire sources (creature_object_beliefs)
	void StirSourcesOnSighting(entt::entity creature, components::CreatureMindState& mind,
	                           creature_object_beliefs::Belief& belief);
	/// Seeing again a villager it has a belief about, a creature free to learn from it watches the villager's skill
	/// (WatchVillagerSkill), then a villager of another player's town or of a neutral one makes it wish more to impress
	/// the villagers
	void WatchVillager(entt::entity creature, components::CreatureMindState& mind, entt::entity villager);
	/// It watches the skill the villager's state shows, when it doesn't know it yet, and goes to follow the villager to
	/// look at it
	void WatchVillagerSkill(entt::entity creature, const components::CreatureMindState& mind, entt::entity villager);
	/// It forgets the things gone from the world
	void ForgetGoneThings(components::CreatureMindState& mind);
	/// How useful a thing the creature has come across is to do an action to for a desire: by what it has learnt of
	/// acting on such things, by how far it is from the creature (or from where its player's leash is tied) and by the
	/// desire's factor for what the thing is; nothing when it is kept within an area and the thing lies outside it (but a
	/// part of a temple, for hanging around at home), or it has not come across the thing
	/// (CreatureMindLearning.cpp)
	[[nodiscard]] float ObjectToActOnUsefulness(entt::entity creature, const components::CreatureMindState& mind,
	                                            creature_desires::Desire desire, entt::entity object);
	/// How useful a thing the creature has come across is to a desire, by what it has learnt of acting on such things
	/// alone; nothing when it is kept within an area and the thing lies outside it, or it has not come across the thing
	/// (CreatureMindLearning.cpp)
	[[nodiscard]] float ActivityUsefulness(entt::entity creature, const components::CreatureMindState& mind,
	                                       creature_desires::Desire desire, entt::entity object);
	/// Its eyes sweep this turn's cells about it, and it comes across everything in those it can see, then the thing it
	/// holds
	void SweepEyes(entt::entity creature, components::CreatureMindState& mind, const components::CreatureAnimation& animation,
	               const components::Transform& transform);

	// Casting miracles (CreatureMindCasting.cpp, over mind_detail's functions of the same names)
	/// The miracle an action of the game's table casts, if any
	static std::optional<uint32_t> CastMagicOf(uint32_t action);
	/// Whether the creature may try the miracle an action casts: seen often enough, able to pay for it, and for a
	/// power-up grown up enough
	bool MayCast(entt::entity creature, const components::CreatureMindState& mind, uint32_t action, bool powerUp);
	/// What the creature casts for an action, the gesture it draws first and its height
	std::optional<creature_plan_actions::CastInfo> CastInfoFor(entt::entity creature, uint32_t action);
	/// How often the creature has seen a miracle and how often it needs to have seen it to cast it
	[[nodiscard]] std::optional<std::pair<float, float>> MiracleSightings(entt::entity creature, MagicType type);
	/// Going near, getting away from or turning to face an object begins, and each turn it goes on
	void StartSubMove(entt::entity creature, const creature_mind::Movement& movement, float seconds);
	void StepSubMove(entt::entity creature, bool animating);
	creature_mind::SubMove SubMoveOf(entt::entity creature);
	/// What it was doing is given up: the action, the walk unless told to keep walking, its agenda and the plan it
	/// carried out
	void StopWhatItWasDoing(entt::entity creature, components::CreatureMindState& mind, bool stopWalking = true);
	/// The start of everything a script has a creature do: it is taken under the script's control if it is not yet,
	/// gives up what it was doing (its walk too, unless it is sent walking), lets go of a miracle it holds, ends a sit
	/// or a casting pose there and then, and obeys its player by following them, at the most a plan can be pressing,
	/// its agenda empty
	void PrepareForScript(entt::entity creature, components::CreatureMindState& mind, bool stopWalking);
	/// A script has the creature carry out an agenda: after the start every scripted action shares, it carries it out
	/// as the plan of obeying its player, which ends, carried out, with the agenda. Whether it could
	bool CarryOutForScript(entt::entity creature, components::CreatureMindState& mind, std::vector<creature_mind::Step> agenda,
	                       bool stopWalking);
	/// The creature tries a miracle at a place of the world, as its casting pose's loop begins: whether it was cast
	bool TryMiracleAt(entt::entity creature, MagicType type, glm::vec3 place);
	/// A try at a miracle that fizzled is shown once it is free to choose what to do next
	void ShowFizzle(entt::entity creature, components::CreatureMindState& mind);
	/// What a stroke or slap left it to do next, done once it is free to choose
	void DoAfterFeedback(entt::entity creature, components::CreatureMindState& mind);
	/// A thing that its plan or its agenda uses is gone before the work is done: it stops and decides again
	void LoseGoneThings(entt::entity creature, components::CreatureMindState& mind);

	/// The game's tables for the minds, taken once the game's data is loaded
	std::optional<creature_mind_tables::Tables> _tables;
	/// For the debug tools: each script native's last call, outside the registry
	std::array<std::optional<CreatureNativeCall>, k_CreatureNativeCount> _lastNatives {};
};

} // namespace openblack::ecs::systems
