/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/PhysicsHooksWorld.h"

namespace openblack::ecs
{

/// The game's kinds of thing in the physics, in the game: its entities, its hand system, its players, its effects and
/// its creatures' minds and bodies, through the locator
class GamePhysicsHooksWorld final: public physics_hooks::World
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] systems::HandSystemInterface& HandPart() override;
	[[nodiscard]] systems::BuildingDamageSystemInterface& BuildingDamage() override;
	[[nodiscard]] PlayerNames LocalPlayer() const override;
	[[nodiscard]] bool IsToy(entt::entity object) const override;
	[[nodiscard]] std::optional<effects::EffectValues> Crush() const override;
	void ApplyEffect(entt::entity object, const effects::EffectValues& values, const magic::EffectSource& source) override;
	void UpdateAttitudeFromFeedback(entt::entity creature, float feedback) override;
	void ChangeDesireSource(entt::entity creature, uint32_t source, float amount) override;
	void KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point) override;
	[[nodiscard]] entt::entity HeartPassesBlowTo(entt::entity heart, bool breaksBuildings) override;
	bool StrikeBuilding(entt::entity building, PhysicsEntry& entry) override;
	[[nodiscard]] std::vector<entt::entity> Creatures() override;
	[[nodiscard]] creature_catch::Eligibility CatchEligibility(entt::entity creature) override;
	[[nodiscard]] float CreatureWeight(entt::entity creature) override;
	[[nodiscard]] float ThingWeight(entt::entity object) override;
	[[nodiscard]] bool MayBeCaught(entt::entity object) override;
	[[nodiscard]] PlayerNames OwnerOf(entt::entity creature) override;
	[[nodiscard]] bool IsAllied(PlayerNames player, PlayerNames other) override;
	[[nodiscard]] uint32_t GameRand(uint32_t range) override;
	[[nodiscard]] std::optional<creature_catch::Approach> CatchApproach(entt::entity creature) override;
	void SetAboutCatching(entt::entity creature, entt::entity object) override;
};

} // namespace openblack::ecs
