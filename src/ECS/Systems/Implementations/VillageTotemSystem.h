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

#include "ECS/Systems/VillageTotemSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Ours behind his interface: the share, the rise and the totems' draw are served by worship::percentage, unchanged;
/// the hand's locked select is the totem's own
class VillageTotemSystem final: public VillageTotemSystemInterface
{
public:
	void AddToPlayer(entt::entity totem) override;
	/// The icon's mesh is the creature's species' totem icon in the game's creature info
	void SetPlayersCreature(entt::entity totem) override;
	void TownChangedPlayer(entt::entity town) override;
	void PlayersCreatureSet(PlayerNames player) override;
	/// The worship percentage's setter: also the villagers sent to worship or called back to match the share
	void SetTownShare(entt::entity town, float share) override;
	/// The totems' draw: the rise moves on by the engine clock since its last move, whatever the milliseconds given.
	/// The arrival and its sound are the town centre's process
	void Update(float gameMilliseconds) override;

	[[nodiscard]] std::optional<entt::entity> TotemOf(entt::entity picked) const override;
	[[nodiscard]] std::optional<entt::entity> TotemOfTownCentre(entt::entity centre) const override;

	[[nodiscard]] std::optional<float> GetShare(entt::entity object) const override;
	bool SetShareFromScript(entt::entity object, float share) override;
	[[nodiscard]] std::optional<float> GetBuiltPercentage(entt::entity object) const override;
	/// (pending) below the whole, the original also gives the totem a building site in its town when it has none
	bool SetBuiltPercentageFromScript(entt::entity object, float percent) override;
	/// (pending) the life the original refuses at or below 0.01 for an object marked indestructible, as life::SetLife
	bool SetLifeFromScript(entt::entity object, float life) override;

	[[nodiscard]] bool ValidForLockedSelect(entt::entity totem, PlayerNames player) const override;
	[[nodiscard]] bool IsGripped(entt::entity totem) const override;
	void StartLockedSelect(entt::entity totem) override;
	/// The reaction is the game's REACT_TO_HAND_USING_TOTEM, made by the totem for the player
	void FriendlyStartLockedSelect(entt::entity totem, PlayerNames player) override;
	void SetGripped(entt::entity totem, bool gripped) override;
	/// The hand stands at the totem, as high as it holds the icon: going down, it stops at the land there
	void Slide(entt::entity totem, float upPixels, float screenHeight) override;
	/// The hold radius is the icon's height x 0.75: the totem is held above, as most objects
	[[nodiscard]] std::optional<HandHold> GetHandHold(entt::entity totem) const override;
	void EndLockedSelect(entt::entity totem) override;
	void LetGo(entt::entity totem, float share) override;
};

} // namespace openblack::ecs::systems
