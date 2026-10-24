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
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <MindFile.h>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"

/// Recording fakes of the creature systems and the hand, for the tests of the systems that call them. The tests put
/// them in the locator; they are never read back through it.
namespace openblack::test::creature_fakes
{
using namespace openblack::ecs::systems;

/// The player's hands: the left one's entity, where it is and the matrix it is drawn with, what it holds and what it is
/// over, how many times it was made to drop it, and how many times the action was pressed and released with a seed and
/// what each gave
class FakeHand final: public HandSystemInterface
{
public:
	using HandResult = MagicSystemInterface::HandResult;

	entt::entity left {entt::null};
	std::optional<glm::vec3> position;
	glm::mat4 matrix {1.0f};
	std::optional<entt::entity> held;
	/// The land under the cursor this frame
	std::optional<glm::vec3> hoverLandPoint;
	std::optional<entt::entity> cursorObject;
	int forcedDrops {0};
	int seedPresses {0};
	int seedReleases {0};
	HandResult pressResult {HandResult::None};
	HandResult releaseResult {HandResult::None};
	uint32_t stillTurns {0};
	std::optional<glm::vec3> actionPoint;
	map_coords::MapCoords syncedPlace {};
	/// The thing in the hand's locked select, if any
	std::optional<entt::entity> lockedSelect;

	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override
	{
		return {left, entt::null};
	}
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override
	{
		return {position, std::nullopt};
	}
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords>, bool) override {}
	void Place(std::optional<glm::vec3>, glm::vec3, bool, std::chrono::microseconds) noexcept override {}
	[[nodiscard]] std::optional<glm::vec3> ResolveCursorPoint(const glm::vec3&, const glm::vec3&, CursorLand, bool,
	                                                          std::chrono::microseconds) noexcept override
	{
		return std::nullopt;
	}
	void PrepareState() noexcept override {}
	void Update(std::chrono::microseconds, glm::vec2, bool, bool) noexcept override {}
	void ActionDoubleClick() noexcept override {}
	[[nodiscard]] HandResult PressSeedAction() noexcept override
	{
		++seedPresses;
		return pressResult;
	}
	[[nodiscard]] HandResult ReleaseSeedAction() noexcept override
	{
		++seedReleases;
		return releaseResult;
	}
	[[nodiscard]] std::optional<entt::entity> GetHeldObject() const noexcept override { return held; }
	[[nodiscard]] std::optional<entt::entity> GetCursorObject() const noexcept override { return cursorObject; }
	[[nodiscard]] bool IsInLockedSelect(entt::entity object) const noexcept override { return lockedSelect == object; }
	void PlaceObjectInMagicHand(entt::entity) noexcept override {}
	[[nodiscard]] std::optional<PlayerNames> GetSourceOwner() const noexcept override { return std::nullopt; }
	[[nodiscard]] bool IsHandReadyForObject() const noexcept override { return false; }
	void ForceDropHeld() noexcept override { ++forcedDrops; }
	void EndAction() noexcept override {}
	// the physics of trees and pots: nothing happens to them here
	bool TreeReactToImpact(entt::entity /*entity*/, ecs::PhysicsEntry& /*po*/) override { return true; }
	entt::entity TreeEndPhysics(entt::entity entity, ecs::PhysicsEntry& /*po*/) override { return entity; }
	void TreeMoved(entt::entity /*entity*/) override {}
	void TreeDropSound(entt::entity /*entity*/) override {}
	bool PotReactToImpact(entt::entity /*entity*/, ecs::PhysicsEntry& /*po*/, const ecs::ImpactInfo& /*impact*/) override
	{
		return true;
	}
	/// Each PotEndPhysics asked: the pot and the player whose interface puts it down
	std::vector<std::pair<entt::entity, std::optional<PlayerNames>>> potEnds;
	/// PotEndPhysics puts the pot down as a pile (it is destroyed and none stays), as a hand pot on land
	bool potBecomesPile {false};
	entt::entity PotEndPhysics(entt::entity entity, std::optional<PlayerNames> throwerPlayer) override
	{
		potEnds.emplace_back(entity, throwerPlayer);
		return potBecomesPile ? entt::entity {entt::null} : entity;
	}
	void BoxTakesInput() noexcept override {}
	void GetSpellInfo(glm::vec3&, glm::vec3&, glm::vec3&, glm::vec3&) const noexcept override {}
	[[nodiscard]] float GetHandScale() const noexcept override { return 1.0f; }
	[[nodiscard]] glm::mat4 GetHandMatrix() const noexcept override { return matrix; }
	void SetHandReach(float) noexcept override {}
	[[nodiscard]] float GetHandReach() const noexcept override { return 0.0f; }
	[[nodiscard]] uint32_t GetStillTurns() const noexcept override { return stillTurns; }
	[[nodiscard]] entt::entity GetClickedObject() const noexcept override { return entt::null; }
	[[nodiscard]] std::optional<glm::vec3> GetActionPoint() const noexcept override { return actionPoint; }
	[[nodiscard]] map_coords::MapCoords GetSyncedPlace() const noexcept override { return syncedPlace; }
	void ProcessTurn() noexcept override {}
	void GameTurnUpdate() noexcept override {}
	[[nodiscard]] std::optional<entt::entity> GetRenderHandObject() const noexcept override { return std::nullopt; }
	void SetThrowBlock(const std::array<float, 12>&) noexcept override {}
	void ResetTurnState(bool /*newGame*/) noexcept override {}
	[[nodiscard]] glm::vec3 GetTurnHandVelocity() const noexcept override { return glm::vec3(0.0f); }
	[[nodiscard]] int32_t GetInterfaceHandState() const noexcept override { return 0; }
	void UpdateInterfaceHandState() noexcept override {}
	[[nodiscard]] bool OfferScreenObject(uint32_t, float) noexcept override { return false; }
	[[nodiscard]] std::optional<uint32_t> ScreenObjectUnderHand() const noexcept override { return std::nullopt; }
	[[nodiscard]] std::optional<uint32_t> DraggedScreenObject() const noexcept override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> GetHoverLandPoint() const noexcept override { return hoverLandPoint; }
	[[nodiscard]] std::vector<entt::entity> GetThrownObjects() const noexcept override { return {}; }
	[[nodiscard]] const std::vector<glm::mat4>* GetBoneMatrices() const noexcept override { return nullptr; }
	[[nodiscard]] std::vector<std::string> GetAnimationNames() const noexcept override { return {}; }
	[[nodiscard]] const std::string& GetCurrentAnimation() const noexcept override { return _animation; }
	void SetAnimationOverride(const std::string&) noexcept override {}
	void StartFixedPosAnimation(const std::string&, glm::vec3) noexcept override {}

private:
	std::string _animation;
};

/// Every animation lasts as long as set (nothing when unset), a bone is where it is set to be, and so is a grasp, each
/// grasp asked for kept
class FakeAnimation final: public CreatureAnimationSystemInterface
{
public:
	struct GraspAsked
	{
		size_t animation;
		float timeMs;
		uint32_t bone;
	};
	std::map<size_t, float> durations;
	std::optional<glm::vec3> bone;
	std::optional<glm::vec3> grasp;
	std::vector<GraspAsked> graspsAsked;

	void ProcessTurn() override {}
	void Update(float, std::chrono::duration<float, std::milli>) override {}
	TurnMoments AdvanceTurn() override { return {}; }
	void PoseTurn(const TurnInputs&) override {}
	[[nodiscard]] std::optional<glm::vec3> BoneInAnimation(entt::entity, size_t, float, uint32_t, bool) override
	{
		return bone;
	}
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity, size_t animation) override
	{
		const auto found = durations.find(animation);
		return found != durations.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> GraspInAnimation(entt::entity, size_t animation, float timeMs,
	                                                        uint32_t hand) override
	{
		graspsAsked.push_back({.animation = animation, .timeMs = timeMs, .bone = hand});
		return grasp;
	}
	bool EnsurePosed(entt::entity) override { return false; }
	void UpdateCopies(uint32_t) override {}
	[[nodiscard]] bool AreSparklesShown() const override { return true; }
	void SetSparklesShown(bool) override {}
};

/// The moves asked for
class FakeLocomotion final: public CreatureLocomotionSystemInterface
{
public:
	struct Asked
	{
		entt::entity creature;
		glm::vec2 point;
		float value;
		/// A walk back into the area it is kept within: how much faster than walking, and how near counts as back
		bool walkBack {false};
		float hurry {0.0f};
		/// Asked to run there rather than walk
		bool run {false};
	};
	std::vector<Asked> moves;
	std::vector<entt::entity> stopped;
	bool moving {false};
	/// What a walk back answers: refused where the creature can't stand
	MoveResult walkBackResult {MoveResult::Started};

	void ProcessTurn() override {}
	void Update(float) override {}
	void AnimationAt(entt::entity, float, ecs::components::CreatureAnimationInputs&) const override {}
	MoveResult MoveTo(entt::entity creature, glm::vec2 point, Pace pace, float, float maxDistance) override
	{
		moves.push_back({.creature = creature, .point = point, .value = maxDistance, .run = pace == Pace::Run});
		return MoveResult::Started;
	}
	MoveResult WalkBack(entt::entity creature, glm::vec2 point, float hurry, float maxDistance) override
	{
		moves.push_back({.creature = creature, .point = point, .value = maxDistance, .walkBack = true, .hurry = hurry});
		return walkBackResult;
	}
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Started; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Started; }
	MoveResult FleeFrom(entt::entity, glm::vec2) override { return MoveResult::Started; }
	bool TurnToFace(entt::entity, glm::vec2) override { return true; }
	void Place(entt::entity, glm::vec3) override {}
	void Stop(entt::entity creature) override { stopped.push_back(creature); }
	[[nodiscard]] bool IsMoving(entt::entity) const override { return moving; }
	[[nodiscard]] bool IsValidPosition(glm::vec2, float) const override { return true; }
};

/// The marks put on the skins
class FakeSkin final: public CreatureSkinSystemInterface
{
public:
	std::vector<creature_marks::Mark> wounds;
	std::vector<creature_marks::Mark> blood;

	void Update() override {}
	void ProcessTurn() override {}
	void SetTattoo(entt::entity, size_t, const creature_tattoo::Slot&) override {}
	void AddWound(entt::entity, const creature_marks::Mark& wound) override { wounds.push_back(wound); }
	void AddBlood(entt::entity, const creature_marks::Mark& drop) override { blood.push_back(drop); }
	void Heal(entt::entity, uint32_t) override {}
};

/// What the minds were told
class FakeMind final: public CreatureMindSystemInterface
{
public:
	std::vector<std::pair<entt::entity, float>> feedback;
	std::vector<size_t> forced;
	std::vector<std::pair<entt::entity, bool>> fought;
	/// The meals ended: the creature, the object's type, its food value and how it ended
	std::vector<std::tuple<entt::entity, uint32_t, float, creature_eating::MealEnd>> meals;
	/// The creatures that gave up what they were doing
	std::vector<entt::entity> abandoned;
	/// The things the player's hand gave: the creature, the thing, its left hand, and the camera's point on the ground
	std::vector<std::tuple<entt::entity, entt::entity, bool, glm::vec2>> given;
	bool forceResult {true};
	/// The action every creature's mind carries out
	std::optional<uint32_t> action;
	/// Whether every creature's take from the hand is still ahead of it or under way
	bool takingFromHand {false};
	/// How strongly each creature was last rewarded or punished (none listed: 0)
	std::map<entt::entity, float> interactionMagnitudes;

	void ProcessTurn() override {}
	void PlanTurn() override {}
	void LearnTurn() override {}
	void LoadMind(entt::entity, std::shared_ptr<const creaturemind::MindFileData>) override {}
	[[nodiscard]] std::optional<creaturemind::MindFileData> SaveMind(entt::entity) const override { return std::nullopt; }
	void ClearLearning(entt::entity) override {}
	void FocusByScript(entt::entity, const glm::vec3&) override {}
	void ReleasedByScript(entt::entity) override {}
	void AbandonAction(entt::entity creature) override { abandoned.push_back(creature); }
	void GiveUpPlan(entt::entity) override {}
	void SetAgendaPriority(entt::entity, float) override {}
	void SeeSkill(const glm::vec3&, size_t) override {}
	void SeeMiracle(const glm::vec3&, size_t) override {}
	void PlayerDid(PlayerNames, size_t, const glm::vec3&, std::optional<entt::entity>) override {}
	void EmpathiseWithPlayer(PlayerNames, CreatureDesires, float, const glm::vec3&) override {}
	void EmpathiseWithTownDesire(PlayerNames, TownDesireInfo, float, const glm::vec3&) override {}
	bool SetKnowsAction(entt::entity, uint32_t, uint32_t, bool) override { return true; }
	[[nodiscard]] const creature_mind_tables::Tables* GetTables() override { return nullptr; }
	[[nodiscard]] std::optional<uint32_t> CurrentActionOf(entt::entity) override { return action; }
	bool PlayAction(entt::entity, size_t, std::optional<bool>) override { return true; }
	bool PlayGesture(entt::entity, size_t) override { return true; }
	void PullFace(entt::entity, size_t) override {}
	std::optional<creature_face::Request> ShowFeeling(entt::entity, creature_face::Cue) override { return std::nullopt; }
	bool SitDown(entt::entity) override { return true; }
	void StandUp(entt::entity) override {}
	void ReceiveFeedback(entt::entity creature, float value) override { feedback.emplace_back(creature, value); }
	bool ForceAction(entt::entity, size_t animation, bool, std::optional<creature_face::Request>, double) override
	{
		forced.push_back(animation);
		return forceResult;
	}
	bool Sleep(entt::entity) override { return true; }
	bool Eat(entt::entity, std::optional<entt::entity>) override { return true; }
	bool Drink(entt::entity) override { return true; }
	bool Poo(entt::entity) override { return true; }
	bool Puke(entt::entity) override { return true; }
	void EndMeal(entt::entity creature, uint32_t objectType, float foodValue, creature_eating::MealEnd end) override
	{
		meals.emplace_back(creature, objectType, foodValue, end);
	}
	bool Faint(entt::entity) override { return true; }
	void Wake(entt::entity) override {}
	void FoughtFight(entt::entity creature, bool won) override { fought.emplace_back(creature, won); }
	bool TakeFromHand(entt::entity creature, entt::entity object, bool leftHand, glm::vec2 camera) override
	{
		given.emplace_back(creature, object, leftHand, camera);
		return true;
	}
	[[nodiscard]] bool IsTakingFromHand(entt::entity) const override { return takingFromHand; }
	[[nodiscard]] float GetInteractionMagnitude(entt::entity creature) const override
	{
		const auto found = interactionMagnitudes.find(creature);
		return found != interactionMagnitudes.end() ? found->second : 0.0f;
	}
};

/// Hands that do nothing
class FakeObjectAction final: public CreatureObjectActionSystemInterface
{
public:
	std::vector<entt::entity> cancelled;
	/// The creatures whose take from the hand was called off
	std::vector<entt::entity> calledOff;
	/// What every creature reaches for from the player's hand, if anything
	std::optional<TakingFromHand> taking;
	/// Whether every creature's hands are set on taking from the player's hand
	bool setOnTaking {false};
	/// What every creature holds in its hand, if anything
	std::optional<entt::entity> held;

	void ProcessApproach() override {}
	void ProcessTurn() override {}
	void UpdateDraw(float) override {}
	void AnimationAt(entt::entity, float, ecs::components::CreatureAnimationInputs&) const override {}
	void UpdateHeldDraw() override {}
	bool PickUp(entt::entity, entt::entity) override { return false; }
	bool PickUpPortion(entt::entity, entt::entity, PotInfo) override { return false; }
	bool PutDown(entt::entity) override { return false; }
	bool Discard(entt::entity) override { return false; }
	bool Lob(entt::entity) override { return false; }
	bool EatHeld(entt::entity) override { return false; }
	bool Keep(entt::entity, size_t) override { return false; }
	bool Throw(entt::entity, const glm::vec3&) override { return false; }
	bool Destroy(entt::entity, entt::entity) override { return false; }
	bool PointAt(entt::entity, const glm::vec3&) override { return false; }
	void Cancel(entt::entity creature) override { cancelled.push_back(creature); }
	void Drop(entt::entity) override {}
	[[nodiscard]] State GetState(entt::entity) const override { return State::Idle; }
	[[nodiscard]] std::optional<float> GetProgress(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetHeld(entt::entity) const override { return held; }
	[[nodiscard]] std::optional<float> FoodValueOf(entt::entity) const override { return std::nullopt; }
	[[nodiscard]] bool CanPickUp(entt::entity, entt::entity) const override { return false; }
	[[nodiscard]] bool CanDestroy(entt::entity) const override { return false; }
	[[nodiscard]] std::optional<TakingFromHand> GetTakingFromHand(entt::entity) const override { return taking; }
	void CallOffTake(entt::entity creature) override { calledOff.push_back(creature); }
	[[nodiscard]] bool IsSetOnTakingFromHand(entt::entity) const override { return setOnTaking; }
};
} // namespace openblack::test::creature_fakes
