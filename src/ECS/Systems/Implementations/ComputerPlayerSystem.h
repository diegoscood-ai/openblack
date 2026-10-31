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
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ECS/Components/ComputerHand.h"
#include "ECS/Components/ComputerMind.h"
#include "ECS/ComputerPlayerMind.h"
#include "ECS/ComputerPlayerMindTree.h"
#include "ECS/ComputerPlayerStart.h"
#include "ECS/Systems/ComputerPlayerSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class ComputerPlayerSystem final: public ComputerPlayerSystemInterface
{
public:
	/// In the game's own world
	ComputerPlayerSystem();
	/// In a world of its own, as tests give it
	explicit ComputerPlayerSystem(std::unique_ptr<computer_player::World> world);
	~ComputerPlayerSystem() override;

	void SwitchOn(PlayerNames player, bool on) override;
	void Pause(PlayerNames player, bool paused) override;
	void SetCreatureLike(PlayerNames player, PlayerNames like) override;
	void ProcessTurn() override;
	void MoveHand(PlayerNames player, const glm::vec3& point, float speed, bool overGround) override;
	void PutHand(PlayerNames player, const glm::vec3& point, bool overGround) override;
	[[nodiscard]] std::optional<glm::vec3> HandOnGround(PlayerNames player) const override;
	[[nodiscard]] bool IsHandReady(PlayerNames player) const override;
	void SetHandSpeed(PlayerNames player, float speed) override;
	void Release(PlayerNames player) override;
	void ClearActions(PlayerNames player) override;
	bool ForceAction(PlayerNames player, std::string_view name, entt::entity first, entt::entity second) override;
	void QueueAction(PlayerNames player, std::string_view name, entt::entity first, entt::entity second) override;
	void SetPersonality(PlayerNames player, std::string_view name, float value) override;
	void SetSuppression(PlayerNames player, std::string_view name, float value) override;
	void SetAttitude(PlayerNames player, PlayerNames toward, float value) override;
	[[nodiscard]] float Attitude(PlayerNames player, PlayerNames toward) const override;
	void LoadPersonality(PlayerNames player, std::string_view path) override;
	void SavePersonality(PlayerNames player, std::string_view path) override;
	void KeepForNextLand() override;
	void SetTraced(PlayerNames player, bool on) override;
	[[nodiscard]] const components::ComputerMind* SavedMind(PlayerNames player) const override;
	void Restore(PlayerNames player, const components::ComputerPlayer& state, const components::ComputerMind& mind) override;

private:
	/// The player's entity, made with the player when the land has none
	entt::entity PlayerEntity(PlayerNames player);
	/// The computer mind of the player, made with the player when the land has none
	components::ComputerPlayer& MindOf(PlayerNames player);
	/// The hand of the player, made the first time it is asked for; none when the land has no such player
	components::ComputerHand* HandOf(PlayerNames player);
	[[nodiscard]] const components::ComputerHand* ReadHandOf(PlayerNames player) const;
	/// A script takes the player's hand for the step: what the hand did is given up (what it held let go of for the
	/// player), and the step goes at the speed
	void TakeHand(PlayerNames player, components::ComputerHand& hand, components::HandStep step, float speed);
	/// The action is forced on the hand through the player's mind, with what the game knows of the two things
	bool Force(PlayerNames player, components::ComputerHand& hand, std::string_view name, entt::entity first,
	           entt::entity second);
	/// The player's mind: the one its entity has, else the one kept from the land before, else a new one
	components::ComputerMind& ComputerMindOf(PlayerNames player);
	/// A chosen node whose action openblack lacks is told in the log (openblack's, while the actions are being ported)
	void NoteMissingAction(PlayerNames player, uint16_t node) const;
	/// The debug trace of a mind's turn: one line in the log, with the hand's first step, its stage and what it holds
	void TraceTurn(PlayerNames player, const components::ComputerMind& mind, const components::ComputerHand& hand,
	               computer_player::TurnDone done, const std::optional<std::pair<std::string, bool>>& forced,
	               uint32_t turn) const;
	/// The player's mind as it is, without making one: none when the player has none on this land or kept
	[[nodiscard]] const components::ComputerMind* ReadComputerMindOf(PlayerNames player) const;

	std::unique_ptr<computer_player::World> _world;
	/// The minds kept as the last land was cleared, by the player's number, until the player's entity takes them up
	std::array<std::optional<components::ComputerMind>, static_cast<size_t>(PlayerNames::_COUNT)> _keptMinds;
	/// The names of the tree's rows as every mind looks them up: a personality file read writes over them, and they
	/// stay so for the rest of the game
	computer_player::MindRowNames _rowNames;
};

} // namespace openblack::ecs::systems
