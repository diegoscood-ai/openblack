/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <memory>
#include <string>
#include <vector>

#include "ECS/Systems/ParticleSystemInterface.h"
#include "GameClock.h"
#include "Particles/LightSheet.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysManagerState.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Holds the particle system's state; it starts empty, with the first effect id 1. The effects are run by the particle
/// engine (psys::manager and psys::utility), which works on the state of the particle system in the locator
class ParticleSystem final: public ParticleSystemInterface
{
public:
	EffectId Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced) override;
	EffectId Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced) override;
	EffectId StartForSpell(std::string_view file, glm::vec3 origin, glm::vec3 direction, float magnitude, psys::SpellSink* sink,
	                       bool synced) override;
	bool ProcessForSpell(EffectId id, const psys::ProcessInfo& info, float seconds) override;
	bool ProcessByFrame(EffectId id, float seconds) override { return psys::manager::ProcessByFrame(id, seconds); }
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                             float magnitude) override;
	void CloseDownSpotVisual(entt::entity object) override { psys::manager::CloseDownSpotVisual(object); }

	void SetOrigin(EffectId id, glm::vec3 origin) override { psys::manager::SetOrigin(id, origin); }
	void SetPlayer(EffectId id, int player) override;
	void SetDrawPath(EffectId id, psys::DrawPath path) override { psys::manager::SetDrawPath(id, path); }
	void SetDrawOffset(EffectId id, glm::vec3 offset) override { psys::manager::SetDrawOffset(id, offset); }
	[[nodiscard]] const particles::ShieldSphere* FindShield(glm::vec3 point, float margin) const override;
	[[nodiscard]] size_t GetSoundCount() const override;
	void SetDrawn(EffectId id, bool drawn) override { psys::manager::SetDrawn(id, drawn); }
	void SetInTemple(EffectId id) override { psys::manager::SetInTemple(id); }
	void AddTarget(EffectId id, entt::entity target) override;
	void AddTargetPosition(EffectId id, glm::vec3 position) override;
	void AddSpotVisualTarget(entt::entity visual, glm::vec3 position) override;
	void SetSpotVisualPlayer(entt::entity visual, int player) override;
	void SetSpotVisualMagnitude(entt::entity visual, float magnitude) override;
	void AddBeliefSprite(const particles::BeliefSprite& sprite) override;

	void AddGestureTrail(std::shared_ptr<particles::GestureTrail> trail) override;
	void UpdateFrame(float gameSeconds, const HandFrame& hand) override;
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override { _state.lightSheets.push_back(sheet); }
	[[nodiscard]] std::vector<std::shared_ptr<particles::LightSheet>> LightSheets() override;
	void CloseDown(EffectId id) override { psys::manager::CloseDown(id); }
	void Delete(EffectId id) override { psys::manager::Delete(id); }
	[[nodiscard]] bool IsRunning(EffectId id) const override { return psys::manager::Find(id) != nullptr; }
	[[nodiscard]] psys::Effect* Find(EffectId id) override { return psys::manager::Find(id); }

	void ProcessTurn() override { psys::manager::ProcessTurn(game_clock::k_TurnSeconds); }
	void Reset() override
	{
		psys::manager::Clear();
		_drawStats = {};
	}
	void CollectDrawFrame(float turnFraction, particles::draw::Frame& frame, bool inTemple) const override;
	[[nodiscard]] DrawStats GetDrawStats() const override { return _drawStats; }

	[[nodiscard]] std::vector<EffectInfo> GetEffects() const override;
	[[nodiscard]] std::vector<std::string> GetFileNames() const override;
	void SetPaused(bool paused) override { _state.paused = paused; }
	[[nodiscard]] bool IsPaused() const override { return _state.paused; }

	[[nodiscard]] psys::manager::State& GetState() noexcept override { return _state; }

protected:
	[[nodiscard]] entt::registry& ModuleStore() noexcept override { return _modules; }

private:
	/// The sheets of light still standing, oldest first, as LightSheets gives them but forgetting none: one gone is
	/// left out (UpdateFrame forgets it)
	[[nodiscard]] std::vector<std::shared_ptr<particles::LightSheet>> StandingSheets() const;

	/// The files' own state, declared first so that it goes after the effects that point into it
	entt::registry _modules;
	psys::manager::State _state;
	/// What the last collected frame drew
	mutable DrawStats _drawStats {};
};
} // namespace openblack::ecs::systems
