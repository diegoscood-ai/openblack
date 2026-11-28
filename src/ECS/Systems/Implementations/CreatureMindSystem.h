/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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
	void BeginCreatureTurns() override;
	void ProcessCreatureTurn(entt::entity creature) override;
	void EndCreatureTurns() override;
	void ProcessCheatDesire(entt::entity creature) override;
	void ProcessReaction(entt::entity creature) override;
	void ChangeHowNice(entt::entity creature, entt::entity other, float change) override;

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
	void FoundNoWay(entt::entity creature) override;
	[[nodiscard]] std::optional<uint32_t> ActionToClear(entt::entity creature, entt::entity thing) override;
	void ShutIn(entt::entity creature, std::span<const entt::entity> met, glm::vec2 destination) override;
	void KeepToHelpWorshippers(entt::entity creature, entt::entity sign, bool always) override;
	void GiveUpPlan(entt::entity creature) override;
	void SetAgendaPriority(entt::entity creature, float priority) override;
	void ChangeDesires(entt::entity creature, const std::function<void(creature_desires::Desires&)>& change) override;
	void ForceFinish(entt::entity creature) override;
	void Hurry(entt::entity creature) override;
	void NoteNative(CreatureNative native, const CreatureNativeCall& call) override;
	[[nodiscard]] std::optional<CreatureNativeCall> LastNative(CreatureNative native) const override
	{
		return _lastNatives.at(static_cast<size_t>(native));
	}
	void SeeSkill(const glm::vec3& point, size_t skill) override;
	void SeeMiracle(const glm::vec3& point, size_t miracle) override;
	void WatchSkill(entt::entity creature, size_t skill) override;
	bool ShowLightbulb(entt::entity creature, float progress) override;
	void PlayerDid(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object) override;
	void PlayerCastDeed(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	                    MagicType magic) override;
	void EmpathiseWithPlayer(PlayerNames player, CreatureDesires desire, float weight, const glm::vec3& point) override;
	void EmpathiseWithTownDesire(PlayerNames player, TownDesireInfo desire, float weight, const glm::vec3& point) override;
	bool SetKnowsAction(entt::entity creature, uint32_t kind, uint32_t action, bool knows) override;
	bool LearnEverything(entt::entity creature) override;
	bool TryMiracle(entt::entity creature, MagicType type, entt::entity target) override;
	bool ForceActivity(entt::entity creature, const ForcedActivity& forced) override;
	bool ForceDesireOn(entt::entity creature, creature_desires::Desire desire, entt::entity thing) override;
	[[nodiscard]] bool IsCommanded(entt::entity creature) const override;
	[[nodiscard]] bool IsForcedByPlayerAction(entt::entity creature) const override;
	void SetForcedByPlayerAction(entt::entity creature, bool forced) override;
	bool ObeyGodOrder(entt::entity creature, uint32_t action, entt::entity object) override;
	void MakeDesireDominant(entt::entity creature, creature_desires::Desire desire, float seconds, bool needsToo) override;
	void EndDominantDesire(entt::entity creature) override;
	void LearnAction(entt::entity creature, uint32_t kind, uint32_t action) override;
	[[nodiscard]] bool KnowsAction(entt::entity creature, uint32_t kind, uint32_t action) const override;
	bool SwapMinds(entt::entity from, entt::entity to) override;
	bool TakeFromHand(entt::entity creature, entt::entity object, bool leftHand, glm::vec2 camera) override;
	[[nodiscard]] bool IsTakingFromHand(entt::entity creature) const override;
	[[nodiscard]] bool PlansAction(entt::entity creature, std::string_view action) override;
	[[nodiscard]] bool IsAvailableForReaction(entt::entity creature, uint8_t reaction) override;
	[[nodiscard]] bool SeemsFriendly(entt::entity creature, entt::entity other) override;
	[[nodiscard]] bool SeemsFriendlyNow(entt::entity creature, entt::entity other) const override;
	void KnowMiracle(entt::entity creature, size_t miracle) override;
	bool TellCast(entt::entity creature, MagicType type, entt::entity target) override;
	[[nodiscard]] float GetInteractionMagnitude(entt::entity creature) const override;
	void ClearInteractionMagnitude(entt::entity creature) override;
	[[nodiscard]] uint32_t GetActionCount(entt::entity creature, uint32_t action) const override;
	void PerformedFight(entt::entity creature) override;
	void SetOnlyDesire(entt::entity creature, creature_desires::Desire desire, float seconds) override;
	void ClearOnlyDesire(entt::entity creature) override;
	bool PointOutHighlight(entt::entity creature) override;
	void MoveToStage(entt::entity creature, uint32_t stage) override;
	void ReactToNastyMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn) override;
	void CastOnLeashLater(entt::entity creature, MagicType magic) override;
	void ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn) override;
	void TakeUpReaction(entt::entity creature, entt::entity thing, const creature_reaction_rules::Setup& setup) override;
	void ReactToFire(entt::entity creature, entt::entity burning) override;
	void TakeUpTeleport(entt::entity creature, entt::entity stone) override;
	void MoveByTeleport(entt::entity creature, const glm::vec3& arrival, const glm::vec3& destination) override;
	void ComeAcrossThing(entt::entity creature, entt::entity thing) override;
	void UpdateLongRangeVision(entt::entity creature) override;
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
	void SquatFires(entt::entity creature, glm::vec3 bottom) override;
	bool Puke(entt::entity creature) override;
	void EndMeal(entt::entity creature, uint32_t objectType, float foodValue, creature_eating::MealEnd end) override;
	void AteMagicMushroom(entt::entity creature) override;
	bool Faint(entt::entity creature) override;
	void Wake(entt::entity creature) override;
	void FoughtFight(entt::entity creature, bool won) override;
	void FightOrdered(entt::entity creature, entt::entity asker) override;

	// The things it has come across (CreatureMindBeliefs.cpp)
	/// What a thing belongs to, as a creature sees it: a villager's, abode's or field's town, an animal's flock, a
	/// creature itself; nothing for anything else
	[[nodiscard]] static std::optional<entt::entity> BelongsTo(entt::entity thing);

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
	/// Takes up a forced plan a saved game had it carry out: the action's own agenda, at the step it was at, as a plan
	/// forced on it; nothing else that forcing a plan does (what it did ends, the plan is remembered) is done again
	void TakeUpLoadedForcedPlan(entt::entity creature, components::CreatureMindState& mind,
	                            const components::LoadedForcedPlan& loaded);
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
	/// As what the creature does stops: the actions serving the stopped plan's desire aged by the turns it ran, the count
	/// started again
	static void AgeOnStop(components::CreatureMindState& mind);
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
	/// After the hand let go of it, the plan under way ends as an action that failed and its agenda is made again from
	/// its first step, the plan kept
	void StartPlanAgain(entt::entity creature, components::CreatureMindState& mind);
	/// Gives up the plan carried out, if any
	static void Abandon(components::CreatureMindState& mind);
	/// A joint action (CreatureMindFriends.cpp): the asker asks another creature to be its partner, and the two become
	/// partners when it agrees; the partner takes an order of the asker, which it does and then waits; the asker makes
	/// the partner's mind stand still or go free. The joint action stops for both when either takes up another agenda
	/// or comes to the end of its agenda (see components::CreaturePartner).
	bool RequestPartner(entt::entity asker, entt::entity asked, creature_friendship::RequestKind kind);
	void TakeOrder(entt::entity partner, entt::entity asker, const creature_mind::Step& ordered);
	/// The partner stops what it does and obeys the asker, the agenda its own: its plan is to obey the asker, remembered
	/// for the player's feedback. Whether it took the agenda
	bool Obey(entt::entity partner, entt::entity asker, std::vector<creature_mind::Step> agenda);
	static void SetDemobilised(entt::entity partner, bool demobilised);
	void EndJointAction(entt::entity creature);
	/// Each creature's joint action kept or stopped by what its agenda did; only `only`'s when given
	void KeepJointActions(std::optional<entt::entity> only = std::nullopt);
	/// One creature's mind for the turn (the shared values from BeginCreatureTurns), its plans, and its learning
	void ThinkCreature(entt::entity entity);
	/// The creatures its minds found passing out, knocked out as a fight's loser is
	void KnockOutPassingOut();
	void PlanCreatureTurn(entt::entity entity);
	void LearnCreatureTurn(entt::entity creature);
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
	/// After the emergencies and before it decides: a step that looks at the people called to its dance draws the one
	/// it looks at this turn (CreatureMindDance.cpp)
	void DrawDependentLook(entt::entity creature, components::CreatureMindState& mind);
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
	/// Once it is free to choose, the cast on the leash that waited is carried out, in place of planning that turn:
	/// whether one waited
	bool CastOnLeash(entt::entity creature, components::CreatureMindState& mind);
	/// Plans the desires due this turn for one creature, at most once a turn. A desire forced on a thing is the only one
	/// planned, whether or not it may be planned now, and even under a script's control, which otherwise stops all
	/// planning
	void PlanCreature(entt::entity creature, components::CreatureMindState& mind,
	                  std::optional<creature_desires::Desire> forcedDesire = std::nullopt);
	/// The desire of the best plan its desires find for one thing, as the planner searches a thing it is sent to act on,
	/// choosing and changing nothing; none when they find nothing to do with it
	[[nodiscard]] std::optional<creature_desires::Desire>
	DesireFoundOn(entt::entity creature, components::CreatureMindState& mind, entt::entity thing);
	/// How it lets go of what it holds before a step that needs its hands free: put down or tossed away
	[[nodiscard]] creature_mind::ObjectOrder::Kind LetGoBeforeStep(entt::entity creature, components::CreatureMindState& mind,
	                                                               entt::entity held);
	/// A turn of looking another creature over: what each would do about the other is weighed, and on finding a desire
	/// to act on it drops the look and acts on it; true when it did
	bool WeighWatchedCreature(entt::entity creature, components::CreatureMindState& mind, entt::entity other);
	/// The game's synchronised random numbers: from 0 to n - 1, and from 0 to 1
	static uint32_t Random(uint32_t range);
	static float Chance();

	/// One creature sees a miracle (by its magic type) and may learn it from the sightings, unless its mind is stilled
	void WatchMiracle(entt::entity creature, size_t miracle);
	/// A deed of a player, with the miracle it was done with (0 for none), that the player's creatures may copy
	void NoticeDeed(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object,
	                uint32_t magic);
	/// At its decision once free, a creature copying its player takes a pass of copying: while the stage lasts, noticing
	/// plays the deed table's noticing action on the thing the deed was done to, copying the action forces a copy of it,
	/// and copying the desire forces the desire; once the stage is over it moves on to the next one, or stops. Whether
	/// it was given an action, so that it decides nothing else this turn
	bool PassMimicry(entt::entity creature, components::CreatureMindState& mind);
	/// Noticing, it plays the deed table's noticing action on the thing the deed was done to, made curious about it:
	/// whether it did
	bool ForceNotice(entt::entity creature, components::CreatureMindState& mind);
	/// Copying the action, it is given the first of the deed's copy actions it can do: whether it was
	bool ForceCopy(entt::entity creature, components::CreatureMindState& mind);
	/// As a new plan ends what the creature was doing: its duel ends, and a fight it made still being set up is left by
	/// both, as the game's stop ends a fight
	static void EndFightAsItStops(entt::entity creature);
	/// Plans an activity in place of what the creature was doing, getting it up and stopping it first
	/// `fightStopped`: the caller's own stop has already ended its fight (a forced plan's), which the game's stop ends once
	bool Replan(entt::entity creature, creature_mind::Activity activity, std::vector<creature_mind::Step> agenda,
	            bool endWhatItDoes = true, bool fightStopped = false);
	/// What an agenda knows of the camera, as a plan's own making sees it: where its god's camera is, the game's turns in
	/// a second, the moon and whether the creature has a player and is evil (CreatureMindLearning.cpp)
	static void SeeCameraFor(entt::entity creature, creature_plan_actions::Situation& situation);
	/// The agenda of an action of the game's table on a thing, and the executor that makes it, if it can be done
	std::optional<std::pair<const creature_plan_actions::Executor*, std::vector<creature_mind::Step>>>
	PlanAgenda(entt::entity creature, uint32_t action, std::optional<uint32_t> object,
	           const creature_plan_actions::Situation& situation, const creature_mind::Random& random = {});
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
	/// The creature's dance stops when the agenda that started it ends, when the creature gives it up, or when a new agenda
	/// replaces it
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
	/// It comes across a thing: a thing new to it is remembered, and leads it on to what the thing belongs to; a thing it
	/// knows already is seen again (StirSourcesOnSighting, WatchVillager for a villager and NoteAttentionOf for another
	/// creature)
	void ComeAcross(entt::entity creature, components::CreatureMindState& mind, entt::entity thing);
	/// It weighs a thing its eyes' sweep meets as the thing to keep in mind to look at, when it sees it and heads towards it
	void ConsiderThingToLookAt(entt::entity creature, components::CreatureMindState& mind, entt::entity thing, float look,
	                           uint16_t heading, float range);
	/// As its place on the map is filed, it weighs the nearest mountains and sea as the thing to keep in mind to look at
	void ConsiderPlacesToLookAt(entt::entity creature, components::CreatureMindState& mind);
	/// Setting off to fight another creature, or to tell it to go away, the other comes across the creature as the
	/// agenda is made, its plan and agenda already taken up (CreatureMindLearning.cpp)
	void OtherComesAcrossAsker(entt::entity creature, const components::CreatureMindState& mind);
	/// Seeing again a thing it has a belief about stirs some of its desire sources (creature_object_beliefs)
	void StirSourcesOnSighting(entt::entity creature, components::CreatureMindState& mind,
	                           creature_object_beliefs::Belief& belief);
	/// Seeing again a villager it has a belief about, a creature free to learn from it watches the villager's skill
	/// (WatchVillagerSkill), then a villager of another player's town or of a neutral one makes it wish more to impress
	/// the villagers
	void WatchVillager(entt::entity creature, components::CreatureMindState& mind, entt::entity villager);
	/// Seeing again another creature it has a belief about, when it is not under a script's control, it finds it has
	/// that creature's attention while the other's plan or the steps it has left use what the other believes about it
	/// (creature_mind::AgendaUseOf, creature_friendship::AgendaUses): the attention it reckons it gets from the other goes
	/// up by creature_friendship::k_AttentionStep, to at most 1
	void NoteAttentionOf(entt::entity creature, components::CreatureMindState& mind, entt::entity other);
	/// It watches the skill the villager's state shows, when it doesn't know it yet, and goes to follow the villager to
	/// look at it
	void WatchVillagerSkill(entt::entity creature, const components::CreatureMindState& mind, entt::entity villager);
	/// It forgets the things gone from the world, and the friends a script made for it that are gone
	void ForgetGoneThings(entt::entity creature, components::CreatureMindState& mind);
	/// How useful a thing the creature has come across is to do an action to for a desire: by what it has learnt of
	/// acting on such things, by how far it is from the creature (or from where its player's leash is tied) and by the
	/// desire's factor for what the thing is; nothing when it is kept within an area and the thing lies outside it (but a
	/// part of a temple, for hanging around at home), or it has not come across the thing
	/// (CreatureMindLearning.cpp)
	[[nodiscard]] float ObjectToActOnUsefulness(entt::entity creature, const components::CreatureMindState& mind,
	                                            creature_desires::Desire desire, entt::entity object);
	/// The same, whether or not it holds a belief about the thing: a thing its player sends it to act on is weighed as
	/// the game weighs it, having filed a belief about it as it is sent (CreatureMindLearning.cpp)
	[[nodiscard]] float ActOnUsefulnessOnceBelieved(entt::entity creature, const components::CreatureMindState& mind,
	                                                creature_desires::Desire desire, entt::entity object);
	/// Whether a thing is one a desire of the creature may act on, as the game asks it of what it knows: not a thing it
	/// found no way to lately, nor one outside the area its leash keeps it within, nor one that is going; for hunger, not
	/// a town's thing when compassion for the town (which it comes across first) is worth enough to it
	/// (creature_planner::HungerSparesTownThing); one a creature may act on at all, and one the desire's own test lets
	/// (mind_detail::DesireMayActOn) (CreatureMindLearning.cpp)
	[[nodiscard]] bool SuitableForDesire(entt::entity creature, components::CreatureMindState& mind, entt::entity thing,
	                                     creature_desires::Desire desire);
	/// How useful a thing the creature has come across is to a desire as what a plan for it is for (its goal), as
	/// mind_detail::GoalUsefulnessOf weighs it; nothing when it has not come across the thing (CreatureMindLearning.cpp)
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
	/// Drawing a miracle's gesture begins, and each turn it goes on with StepSubMove
	void StartGesture(entt::entity creature, uint32_t gesture);
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
	/// Carries out what a stroke, a slap or its leash left it to do, if anything: true when it did, and it chooses nothing
	/// else this turn
	bool DoAfterFeedback(entt::entity creature, components::CreatureMindState& mind);
	/// Stroked for what it did to a thing other than itself, and not while it copies the player, it does it again to the
	/// thing of that kind most useful to act on, as an order is carried out
	void RepeatLastAction(entt::entity creature, components::CreatureMindState& mind, const creature_learning::Context& lesson);
	/// The thing the creature knows of best to use for an action of a desire, other than the thing it does it to: none
	/// when it knows of nothing worth using
	std::optional<entt::entity> BestObjectToUse(entt::entity creature, components::CreatureMindState& mind,
	                                            creature_desires::Desire desire, size_t action, entt::entity actOn);
	/// The thing a planned action that uses a thing takes up: the thing the creature knows of best to use
	/// (BestObjectToUse); none when there is none
	std::optional<uint32_t> InstrumentFor(entt::entity creature, components::CreatureMindState& mind,
	                                      creature_desires::Desire desire, size_t action, std::optional<uint32_t> goal);
	/// Keeps what a stroke or slap leaves it to do next, in place of a fizzle to show or a gift to decide about
	/// What its leash's needs sign left it to do, once it is free to choose (KeepToHelpWorshippers)
	void HelpWorshippers(entt::entity creature, components::CreatureMindState& mind);
	static void KeepAfterFeedback(entt::entity creature, components::CreatureMindState& mind,
	                              components::CreatureMindState::AfterFeedback after);
	/// Shut in, once it has done what it was given to clear its way: it walks on to the kept walk point, to within its
	/// height of it, as its player's order
	void WalkToKeptPoint(entt::entity creature, components::CreatureMindState& mind);
	/// A thing that its plan or its agenda uses is gone before the work is done: it stops and decides again
	void LoseGoneThings(entt::entity creature, components::CreatureMindState& mind);

	/// The game's tables for the minds, taken once the game's data is loaded
	std::optional<creature_mind_tables::Tables> _tables;
	/// What every creature's mind reads of the turn as it starts, and the creatures passing out, to be knocked out
	struct MindTurn
	{
		bool night {false};
		bool barsIn {false};
		uint32_t barsOwner {0};
		const creature_mind_tables::Tables* lookTables {nullptr};
		std::optional<uint32_t> cutSceneLook;
		std::vector<entt::entity> passingOut;
	};
	MindTurn _turn;
	/// The desires each creature plans a turn, counted from the creatures as the creatures' turn starts and kept until
	/// the next start (two before the first)
	size_t _goalsPerTurn {creature_planner::k_GoalsPerTurn};
	/// For the debug tools: each script native's last call, outside the registry
	std::array<std::optional<CreatureNativeCall>, k_CreatureNativeCount> _lastNatives {};
	/// The thing PlanCreature is asked about by DesireFoundOn, and what it found
	std::optional<entt::entity> _desireQuery;
	std::optional<creature_desires::Desire> _desireFound;
	/// (readout, OPENBLACK_CREATURE_STEP_TRACE) where each creature's mind turn last stopped, or that it thought
	std::vector<std::pair<entt::entity, std::string_view>> _stepGates;
	/// The last line the step readout gave each creature (OPENBLACK_CREATURE_STEP_TRACE)
	std::vector<std::pair<entt::entity, std::string>> _stepLines;
	/// Says where the creature's mind turn stopped this turn, when it is not where it stopped the turn before
	void TraceStepGate(entt::entity creature, std::string_view gate);
};

} // namespace openblack::ecs::systems
