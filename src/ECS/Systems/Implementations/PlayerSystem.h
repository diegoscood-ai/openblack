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
#include <unordered_map>

#include "ECS/Components/VirtualInfluence.h"
#include "ECS/Systems/PlayerSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{
class PlayerSystem final: public PlayerSystemInterface
{
public:
	static constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);

	void RegisterPlayers() override;
	void AddPlayer(entt::entity playerEntity) override;
	[[nodiscard]] entt::entity GetPlayer(PlayerNames playerName) const override;
	void AddCreature(entt::entity creature) override;
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames name) const override;
	void SetPrimaryCreature(PlayerNames name, std::optional<entt::entity> creature) override;
	void KeepForNextLand() override;
	void TakeUpKept(entt::entity playerEntity) override;
	void ClearPlayers() override;

	[[nodiscard]] components::Alignment& Alignment(PlayerNames name) override;
	[[nodiscard]] components::PlayerMagic& MagicWithoutEntity(PlayerNames name) override;
	void ClearMagicWithoutEntity() override;
	[[nodiscard]] components::InterfaceAlignment& InterfaceAlignmentWithoutEntity() override
	{
		return _interfaceAlignmentWithoutEntity;
	}
	[[nodiscard]] components::InfluenceCrossing& InfluenceCrossingWithoutEntity() override
	{
		return _influenceCrossingWithoutEntity;
	}

private:
	/// The player's entity on this land, if one is listed and still there
	[[nodiscard]] std::optional<entt::entity> EntityOf(PlayerNames name) const;
	/// A player entity just listed takes up the creature the service kept for them without one
	void TakeUpCreature(PlayerNames name, entt::entity playerEntity);

	std::unordered_map<PlayerNames, entt::entity> _players;
	std::array<components::Alignment, k_Players> _alignment {};
	std::array<components::PlayerMagic, k_Players> _magicWithoutEntity {};
	std::array<std::optional<entt::entity>, k_Players> _creatureWithoutEntity {};
	components::InterfaceAlignment _interfaceAlignmentWithoutEntity {};
	components::InfluenceCrossing _influenceCrossingWithoutEntity {};
	/// What the local player's hand kept of their influence past the border, from the last land's KeepForNextLand to the
	/// next land's first local player entity
	std::optional<components::VirtualInfluence> _keptVirtualInfluence;
};
} // namespace openblack::ecs::systems
