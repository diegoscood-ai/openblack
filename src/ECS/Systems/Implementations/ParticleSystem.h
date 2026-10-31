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
#include "Particles/ParticleManager.h"
#include "Particles/ParticleManagerState.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Holds the particle system's state; it starts empty, with the first effect id 1. The effects are run by the particle
/// engine (particles::manager and particles::utility), which works on the state of the particle system in the locator
class ParticleSystem final: public ParticleSystemInterface
{
public:
	EffectId Start(std::string_view file, glm::vec3 origin, float magnitude, bool synced) override;
	EffectId Start(ParticleType type, glm::vec3 origin, float magnitude, bool synced) override;
	EffectId StartForSpell(std::string_view file, glm::vec3 origin, glm::vec3 direction, float magnitude,
	                       particles::SpellSink* sink, bool synced) override;
	bool ProcessForSpell(EffectId id, const particles::ProcessInfo& info, float seconds) override;
	bool ProcessByFrame(EffectId id, float seconds) override { return particles::manager::ProcessByFrame(id, seconds); }
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 position, std::optional<int> turns, entt::entity owner,
	                             float magnitude) override;
	void CloseDownSpotVisual(entt::entity object) override { particles::manager::CloseDownSpotVisual(object); }
	void DeleteSpotVisual(entt::entity object) override { particles::manager::DeleteSpotVisual(object); }

	void SetOrigin(EffectId id, glm::vec3 origin) override { particles::manager::SetOrigin(id, origin); }
	void SetPlayer(EffectId id, int player) override;
	void SetDrawPath(EffectId id, particles::DrawPath path) override { particles::manager::SetDrawPath(id, path); }
	void SetDrawOffset(EffectId id, glm::vec3 offset) override { particles::manager::SetDrawOffset(id, offset); }
	[[nodiscard]] const particles::ShieldSphere* FindShield(glm::vec3 point, float margin) const override;
	[[nodiscard]] size_t GetSoundCount() const override;
	void SetDrawn(EffectId id, bool drawn) override { particles::manager::SetDrawn(id, drawn); }
	void SetInTemple(EffectId id) override { particles::manager::SetInTemple(id); }
	void AddTarget(EffectId id, entt::entity target) override;
	void AddTargetPosition(EffectId id, glm::vec3 position) override;
	void AddSpotVisualTarget(entt::entity visual, glm::vec3 position) override;
	void AddSpotVisualTargetObject(entt::entity visual, entt::entity target) override;
	void AddSpotVisualPlasma(entt::entity visual, const particles::PlasmaCommand& command) override;
	void SetSpotVisualPlayer(entt::entity visual, int player) override;
	void SetSpotVisualStrength(entt::entity visual, float strength) override;
	[[nodiscard]] std::optional<float> GetSpotVisualStrength(entt::entity visual) const override;
	bool SetSpotVisualMagnitude(entt::entity visual, float magnitude) override;
	void AddBeliefSprite(const particles::BeliefSprite& sprite) override;

	void AddGestureTrail(std::shared_ptr<particles::GestureTrail> trail) override;
	void UpdateFrame(float gameSeconds, const HandFrame& hand) override;
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override { _state.lightSheets.push_back(sheet); }
	/// The gone ones are forgotten first
	void AddOwnedLightSheet(const std::shared_ptr<particles::LightSheet>& sheet) override;
	[[nodiscard]] std::vector<std::shared_ptr<particles::LightSheet>> LightSheets() override;
	void CloseDown(EffectId id) override { particles::manager::CloseDown(id); }
	void Delete(EffectId id) override { particles::manager::Delete(id); }
	[[nodiscard]] bool IsRunning(EffectId id) const override { return particles::manager::Find(id) != nullptr; }
	[[nodiscard]] particles::Effect* Find(EffectId id) override { return particles::manager::Find(id); }

	void ProcessTurn() override { particles::manager::ProcessTurn(game_clock::k_TurnSeconds); }
	void Reset() override
	{
		particles::manager::Clear();
		_drawStats = {};
	}
	void CollectDrawFrame(float turnFraction, particles::draw::Frame& frame, bool inTemple) const override;
	[[nodiscard]] DrawStats GetDrawStats() const override { return _drawStats; }

	[[nodiscard]] std::vector<EffectInfo> GetEffects() const override;
	[[nodiscard]] std::vector<std::string> GetFileNames() const override;
	void SetPaused(bool paused) override { _state.paused = paused; }
	[[nodiscard]] bool IsPaused() const override { return _state.paused; }

	[[nodiscard]] particles::manager::State& GetState() noexcept override { return _state; }

protected:
	[[nodiscard]] entt::registry& ModuleStore() noexcept override { return _modules; }

private:
	/// The sheets of light still standing, oldest first, as LightSheets gives them but forgetting none: one gone is
	/// left out (UpdateFrame forgets it). Then the owned ones still held, oldest first (the gone ones are forgotten here)
	[[nodiscard]] std::vector<std::shared_ptr<particles::LightSheet>> StandingSheets() const;

	/// The files' own state, declared first so that it goes after the effects that point into it
	entt::registry _modules;
	particles::manager::State _state;
	/// What the last collected frame drew
	mutable DrawStats _drawStats {};
};
} // namespace openblack::ecs::systems
