/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureLayers.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CreatureFightSystem final: public CreatureFightSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(float turnFraction, float frameMs) override;
	void AnimationAt(entt::entity creature, float turnFraction, components::CreatureAnimationInputs& inputs) const override;

	/// The body a fighter is drawn playing a share of the way through the turn (0 to 1): its fight animation, of
	/// `durationMs`, at the time drawn between where it was as the turn started and where it is at the turn's end. The
	/// body plays it in place of any slots
	[[nodiscard]] static creature_layers::BodyAction BodyAt(const components::CreatureFighting& fighting, float durationMs,
	                                                        float share);

	StartResult StartFight(entt::entity creature, entt::entity opponent) override;
	void AbortFight(entt::entity creature) override;
	[[nodiscard]] bool IsFighting(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity creature) const override;
	[[nodiscard]] std::vector<ArenaView> GetArenas() const override;
	[[nodiscard]] std::optional<ArenaView> ArenaOf(entt::entity fighter) const override;

	bool QueueMove(entt::entity creature, const creature_fight::Move& move, bool replace) override;
	void ReleaseCharge(entt::entity creature, float heldMs) override;
	void SetAutoFighting(entt::entity creature, bool autoFight) override;
	[[nodiscard]] bool IsAutoFighting(entt::entity creature) const override;

	bool Press(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) override;
	void Release() override;
	[[nodiscard]] bool IsPressed() const override;

	[[nodiscard]] bool IsBlocking(entt::entity creature) const override;
	void Recoil(entt::entity creature) override;

	void KnockOut(entt::entity creature) override;
	void ForceFaint(entt::entity creature) override;
	void KillPermanently(entt::entity creature) override;
	void Resurrect(entt::entity creature) override;
	[[nodiscard]] bool IsKnockedOut(entt::entity creature) const override;

	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override;

	void SetAngerStartsFights(bool enabled) override { _angerStartsFights = enabled; }
	[[nodiscard]] bool GetAngerStartsFights() const override { return _angerStartsFights; }
	void SetCameraWatches(bool enabled) override { _cameraWatches = enabled; }
	[[nodiscard]] bool GetCameraWatches() const override { return _cameraWatches; }
	[[nodiscard]] bool IsCameraOnFight() const override;

private:
	/// The fighters' animations a number of milliseconds on: their clocks, the moves their animations carry them by,
	/// the blows that land, the turns to face their opponents and the animations that follow
	void StepFighters(float milliseconds);
	/// The turn's parts: fights picked by angry creatures and started by the leash, the stages before and after the
	/// duel, the duel's moves, and the creatures knocked out
	void StartFightsFromMinds();
	void ProcessStages();
	void ProcessDuels();
	void ProcessKnockedOut();
	/// Makes the move at the front of a fighter's queue, if it can
	void CheckQueue(entt::entity creature);
	/// A blow at a band: struck, or a step taken towards where it would land
	void AttemptBlow(entt::entity creature, creature_fight::Band band, float speed);
	/// A fighter's action landing on its opponent this turn, if it does
	void TestHit(entt::entity creature);
	/// One creature beat the other: the loser faints and the winner shows off
	void Win(entt::entity winner, entt::entity loser);
	/// The fight ends for a creature: its life pays for it and it learns from it
	void EndFightFor(entt::entity creature, bool won);
	void BeginDuel(entt::entity creature);
	void MeasureBlows(entt::entity creature);
	/// Faints and lies out cold, to be taken home later, or back to where it started fighting
	void Faint(entt::entity creature, std::optional<glm::vec3> start);
	/// What the player's camera does as a fight starts: it goes to watch it, or the spirits remark on it
	void WatchFightStart(entt::entity fighterA, entt::entity fighterB, const creature_fight::Arena& arena) const;
	/// Leaves the fight for good, its mind taking over again
	void Leave(entt::entity creature);

	bool _angerStartsFights {true};
	bool _cameraWatches {true};
	/// How many arenas have been made, which numbers the next
	uint32_t _arenasMade {0};
};

} // namespace openblack::ecs::systems
