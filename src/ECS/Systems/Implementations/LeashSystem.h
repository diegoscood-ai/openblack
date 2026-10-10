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
#include <vector>

#include "ECS/Systems/LeashSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class LeashSystem final: public LeashSystemInterface
{
public:
	void ProcessTurn() override;
	void Update(float seconds) override;

	[[nodiscard]] bool Knows(entt::entity creature, LeashType type) const override;
	void SetKnown(entt::entity creature, LeashType type, bool known) override;
	[[nodiscard]] bool IsLeashable(entt::entity creature) const override;
	bool SetLeashable(entt::entity creature, bool leashable) override;
	void SetOwner(entt::entity creature, PlayerNames owner) override;
	void ClaimOnArrival(entt::entity creature) override;
	[[nodiscard]] creature_leash::Refusal WhyNot(PlayerNames player, entt::entity creature, LeashType type) const override;
	[[nodiscard]] std::optional<Refused> LastRefusal(PlayerNames player) const override;
	bool PutOn(entt::entity creature, LeashType type) override;
	void TakeOff(entt::entity creature) override;
	bool Toggle(entt::entity creature) override;
	bool ChangeType(entt::entity creature, LeashType type) override;
	bool TieTo(entt::entity creature, entt::entity object) override;
	void UntieToHand(entt::entity creature) override;
	void ReturnToHand(entt::entity creature) override;
	void SetWorks(entt::entity creature, bool works) override;
	[[nodiscard]] bool Works(entt::entity creature) const override;
	void PullAwayFromAction(entt::entity creature) override;
	void ActOn(entt::entity creature, entt::entity object) override;
	void ConfineToHome(entt::entity creature, float radius) override;
	void ClearConfinement(entt::entity creature) override;
	void SetHome(entt::entity creature, const glm::vec3& home) override;
	[[nodiscard]] bool FreeOfHome(entt::entity creature) const override;
	[[nodiscard]] creature_leash::HomeKeeping HomeKeepingOf(entt::entity creature) const override;
	[[nodiscard]] bool IsLeashed(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> TiedTo(entt::entity creature) const override;
	[[nodiscard]] LeashType TypeOf(entt::entity creature) const override;
	[[nodiscard]] LeashType Picked(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> PlayersCreature(PlayerNames player) const override;
	bool PressKey(PlayerNames player, creature_leash::LeashKey key) override;
	bool TapCreature(PlayerNames player, entt::entity creature) override;
	bool TakeOffHeldLeash(PlayerNames player) override;
	void Tug(entt::entity creature) override;

private:
	/// Puts the leash on for the player, unless the rules refuse it, which is logged and remembered
	bool PutOnFor(PlayerNames player, entt::entity creature, LeashType type);
	/// Puts the leash on in the player's hand with no rule asked, the creature taking the leash's moods
	void PutOnUnchecked(PlayerNames player, entt::entity creature, LeashType type);
	/// Carries out what a shortcut does to the player's creature
	bool Carry(PlayerNames player, entt::entity creature, const creature_leash::KeyCommand& command);
	/// Logs the refusal and keeps it on the player's entity, when the player has one
	void Refuse(PlayerNames player, entt::entity creature, creature_leash::Refusal why);
	/// The creatures as the one-each assignment sees them
	[[nodiscard]] std::vector<creature_leash::Claim> Claims() const;
	/// The two leash-tying sounds play in turn
	bool _secondAttachSound {false};
	/// The same two sounds play in turn as the hand unties a leash, kept apart from the tying's
	bool _secondUntieSound {false};
};

} // namespace openblack::ecs::systems
