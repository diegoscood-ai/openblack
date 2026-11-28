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

#include <entt/entity/entity.hpp>

#include "ECS/Systems/VillageTotemSystemInterface.h"
#include "Enums.h"

namespace openblack::test
{
/// The village totems as a test sees them: every player whose creature is set and every totem asked to wear its
/// creature's icon is kept, in order, and nothing else is done. Inject it with
/// Locator::villageTotemSystem::emplace<RecordingVillageTotems>()
class RecordingVillageTotems final: public ecs::systems::VillageTotemSystemInterface
{
public:
	std::vector<PlayerNames> creaturesSet;
	std::vector<entt::entity> iconsSet;
	int worldsLoaded {0};

	void AddToPlayer(entt::entity /*totem*/) override {}
	void SetPlayersCreature(entt::entity totem) override { iconsSet.push_back(totem); }
	void TownChangedPlayer(entt::entity /*town*/) override {}
	void PlayersCreatureSet(PlayerNames player) override { creaturesSet.push_back(player); }
	void WorldLoaded() override { ++worldsLoaded; }
	void SetTownShare(entt::entity /*town*/, float /*share*/) override {}
	void Update(float /*gameMilliseconds*/) override {}
	[[nodiscard]] std::optional<entt::entity> TotemOf(entt::entity /*picked*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> TotemOfTownCentre(entt::entity /*centre*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<float> GetShare(entt::entity /*object*/) const override { return std::nullopt; }
	bool SetShareFromScript(entt::entity /*object*/, float /*share*/) override { return false; }
	[[nodiscard]] std::optional<float> GetBuiltPercentage(entt::entity /*object*/) const override { return std::nullopt; }
	bool SetBuiltPercentageFromScript(entt::entity /*object*/, float /*percent*/) override { return false; }
	bool SetLifeFromScript(entt::entity /*object*/, float /*life*/) override { return false; }
	[[nodiscard]] bool ValidForLockedSelect(entt::entity /*totem*/, PlayerNames /*player*/) const override { return false; }
	[[nodiscard]] bool IsGripped(entt::entity /*totem*/) const override { return false; }
	void StartLockedSelect(entt::entity /*totem*/) override {}
	void FriendlyStartLockedSelect(entt::entity /*totem*/, PlayerNames /*player*/) override {}
	void SetGripped(entt::entity /*totem*/, bool /*gripped*/) override {}
	void Slide(entt::entity /*totem*/, float /*upPixels*/, float /*screenHeight*/) override {}
	[[nodiscard]] std::optional<HandHold> GetHandHold(entt::entity /*totem*/) const override { return std::nullopt; }
	void EndLockedSelect(entt::entity /*totem*/) override {}
	void LetGo(entt::entity /*totem*/, float /*share*/) override {}
};
} // namespace openblack::test
