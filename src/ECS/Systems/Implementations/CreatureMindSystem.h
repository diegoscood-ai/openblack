/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

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
	void SetScriptPlay(entt::entity creature, uint32_t animation, uint32_t plays) override;
	bool PlayForScript(entt::entity creature) override;
	bool HasPlayed(entt::entity creature) override;
	bool MoveForScript(entt::entity creature, const glm::vec3& point, float distance) override;
	bool ScriptDoAction(entt::entity creature, uint32_t action, entt::entity target, std::optional<entt::entity> with) override;
	void AbandonAction(entt::entity creature) override;
	void SetIdlePolicy(bool chooses) override { _idlePolicy = chooses; }
	[[nodiscard]] bool IdlePolicy() const override { return _idlePolicy; }
	void GiveUpPlan(entt::entity creature) override;
	void SetAgendaPriority(entt::entity creature, float priority) override;
	void SeeSkill(const glm::vec3& point, size_t skill) override;
	void SeeMiracle(const glm::vec3& point, size_t miracle) override;
	void PlayerDid(PlayerNames player, size_t deed, const glm::vec3& point, std::optional<entt::entity> object) override;
	void EmpathiseWithPlayer(PlayerNames player, CreatureDesires desire, float weight, const glm::vec3& point) override;
	void EmpathiseWithTownDesire(PlayerNames player, TownDesireInfo desire, float weight, const glm::vec3& point) override;
	bool SetKnowsAction(entt::entity creature, uint32_t kind, uint32_t action, bool knows) override;
	bool TryMiracle(entt::entity creature, MagicType type, entt::entity target) override;
	bool ForceActivity(entt::entity creature, const ForcedActivity& forced) override;
	bool TakeFromHand(entt::entity creature, entt::entity object, bool leftHand, glm::vec2 camera) override;
	[[nodiscard]] bool IsTakingFromHand(entt::entity creature) const override;
	[[nodiscard]] bool PlansAction(entt::entity creature, std::string_view action) override;
	[[nodiscard]] bool IsAvailableForReaction(entt::entity creature, uint8_t reaction) override;
	void KnowMiracle(entt::entity creature, size_t miracle) override;
	bool TellCast(entt::entity creature, MagicType type, entt::entity target) override;
	void ReactToNastyMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn) override;
	void ReactToNiceMagic(entt::entity creature, const glm::vec3& point, std::optional<size_t> learn) override;
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override;
	[[nodiscard]] std::optional<uint32_t> CurrentActionOf(entt::entity creature) override;

	bool PlayAction(entt::entity creature, size_t animation, std::optional<bool> mirrored = std::nullopt) override;
	bool PlayGesture(entt::entity creature, size_t animation) override;
	void PullFace(entt::entity creature, size_t animation) override;
	std::optional<creature_face::Request> ShowFeeling(entt::entity creature, creature_face::Cue cue) override;
	bool SitDown(entt::entity creature) override;
	void StandUp(entt::entity creature) override;
	void ReceiveFeedback(entt::entity creature, float feedback) override;
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
	bool Faint(entt::entity creature) override;
	void Wake(entt::entity creature) override;
	void FoughtFight(entt::entity creature, bool won) override;

private:
	/// Sets up what a creature has learnt the first time its mind thinks, from its mind file when it has one
	void SetUpLearning(entt::entity creature, components::CreatureMindState& mind);
	/// Gives a creature's mind its desires and what it has learnt if it has not got them yet, as its first turn does, so
	/// that a creature can be told to do something in the turn it was made
	void SetUpMind(entt::entity creature, CreatureType species, components::CreatureMindState& mind);
	/// Takes up a mind file waiting to be loaded
	void TakeUpFile(entt::entity creature, components::CreatureMindState& mind);
	/// What a script taught a creature or took away, in order, on what it has learnt (nothing without the tables)
	static void TakeUpScriptKnows(std::span<const components::CreatureMindState::ScriptKnows> edits,
	                              const creature_mind_tables::Tables* tables, float speciesMultiplier,
	                              creature_mind_model::Learnt& learnt);
	/// Remembers what the idle mind has started, for feedback to be credited to, and finishes plans that are done
	void FollowAgenda(entt::entity creature, components::CreatureMindState& mind);
	/// Carries out a plan in place of what the creature was doing; returns whether it could
	bool Adopt(entt::entity creature, components::CreatureMindState& mind, const creature_planner::Plan& plan,
	           const creature_plan_actions::Situation& situation);
	/// Gives up the plan carried out, if any
	static void Abandon(components::CreatureMindState& mind);
	/// Whether the player's camera looks at the creature, for whether it saw an action finish
	[[nodiscard]] static bool PlayerLooksAt(entt::entity creature);
	/// Learns what feedback teaches, from what the creature did lately
	void LearnFromFeedback(entt::entity creature, components::CreatureMindState& mind, float feedback);
	/// The creature has taken a thing the hand gave it: its desires choose what it does with it
	void UseGift(entt::entity creature, entt::entity object);
	/// Plans the desires due this turn for one creature, or all of them, at most once a turn
	void PlanCreature(entt::entity creature, components::CreatureMindState& mind, bool everyDesire = false);
	/// The game's synchronised random numbers: from 0 to n - 1, and from 0 to 1
	static uint32_t Random(uint32_t range);
	static float Chance();

	/// One creature sees a miracle (by its magic type) and may learn it from the sightings, unless its mind is stilled
	void WatchMiracle(entt::entity creature, size_t miracle);
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
	/// A try at a miracle that fizzled is shown once it is free to choose what to do next
	void ShowFizzle(entt::entity creature, components::CreatureMindState& mind);
	/// What a stroke or slap left it to do next, done once it is free to choose
	void DoAfterFeedback(entt::entity creature, components::CreatureMindState& mind);

	/// The game's tables for the minds, taken once the game's data is loaded
	std::optional<creature_mind_tables::Tables> _tables;
	/// For the debug tools: openblack's former idle policy, off as in the game
	bool _idlePolicy {false};
};

} // namespace openblack::ecs::systems
