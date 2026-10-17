/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/TempleDestructionWorld.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

/// A temple's destruction in the game, through the game's services
class GameTempleDestructionWorld final: public temple_world::World
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] uint32_t MillisecondsPerTurn() const override;
	[[nodiscard]] uint32_t Turn() const override;
	[[nodiscard]] std::optional<PlayerNames> LocalPlayer() const override;
	[[nodiscard]] bool PlaygroundGame() const override;
	[[nodiscard]] entt::entity CitadelHeartOf(PlayerNames player) const override;
	[[nodiscard]] std::vector<entt::entity> PartsOf(entt::entity heart) const override;
	[[nodiscard]] std::vector<openblack::physics::temple_heart::Town> HeartTowns(PlayerNames owner) const override;
	[[nodiscard]] bool Available(entt::entity thing) const override;
	[[nodiscard]] float LifeOf(entt::entity thing) const override;
	[[nodiscard]] glm::vec3 MapPointOf(entt::entity thing) const override;
	[[nodiscard]] float HeightOf(entt::entity thing) const override;
	void TagSound(entt::entity thing, int sample, bool loops) override;
	void RemoveSoundTags(entt::entity thing, int sample) override;
	void PlaySpark(entt::entity heart, bool hurt) override;
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, int32_t turns) override;
	[[nodiscard]] bool SpotVisualGoing(entt::entity visual) const override;
	void SetSpotVisualPlayer(entt::entity visual, PlayerNames player) override;
	void SetSpotVisualScale(entt::entity visual, float scale) override;
	void AddSpotVisualTarget(entt::entity visual, entt::entity thing) override;
	void PowerGlowByHeartbeat(entt::entity visual) override;
	void AddPlasma(entt::entity visual, const particles::PlasmaCommand& command) override;
	[[nodiscard]] std::vector<model_surface::Triangle> DrawnTrianglesOf(entt::entity object) const override;
	[[nodiscard]] glm::mat4 PlacementOf(entt::entity object) const override;
	[[nodiscard]] GameRandomInterface* LocalRandom() override;
	[[nodiscard]] float GameFloatRand(float range) override;
	void HoldHeartbeat(float pace) override;
	void FadeHeart(entt::entity heart, uint8_t alpha) override;
	void ShowShell(entt::entity heart, bool shown) override;
	void BreakUpModel(entt::entity heart) override;
	void ToBeDeleted(entt::entity object) override;
	void StartScript(std::string_view name) override;
};

} // namespace openblack::ecs::systems
