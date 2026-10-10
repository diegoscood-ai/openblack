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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/ParticleSystemInterface.h"
#include "Enums.h"
#include "Magic/Gestures/GestureShapes.h"
#include "Particles/PSysManagerState.h"

namespace openblack::test
{
/// The running particle effects as a test that does not look at them sees them: nothing ever starts, runs or is found,
/// and every step does nothing; a gesture's trail is kept in the engine state, as the real service keeps it. Its engine
/// state and the files' own state are real and empty, so that the engine's functions that reach them through the
/// locator find them. A test's own fake of the particles derives from it and overrides only what it records, so that a
/// method the service gains reaches every fake from here. Inject a fake with Locator::particleSystem::emplace<Fake>()
class InertParticleSystem: public ecs::systems::ParticleSystemInterface
{
public:
	EffectId Start(std::string_view /*file*/, glm::vec3 /*origin*/, float /*magnitude*/, bool /*synced*/) override
	{
		return k_NoEffect;
	}
	EffectId Start(ParticleType /*type*/, glm::vec3 /*origin*/, float /*magnitude*/, bool /*synced*/) override
	{
		return k_NoEffect;
	}
	EffectId StartForSpell(std::string_view /*file*/, glm::vec3 /*origin*/, glm::vec3 /*direction*/, float /*magnitude*/,
	                       psys::SpellSink* /*sink*/, bool /*synced*/) override
	{
		return k_NoEffect;
	}
	bool ProcessForSpell(EffectId /*id*/, const psys::ProcessInfo& /*info*/, float /*seconds*/) override { return false; }
	entt::entity StartSpotVisual(SpotVisualType /*type*/, glm::vec3 /*position*/, std::optional<int> /*turns*/,
	                             entt::entity /*owner*/, float /*magnitude*/) override
	{
		return entt::null;
	}

	void SetOrigin(EffectId /*id*/, glm::vec3 /*origin*/) override {}
	void SetPlayer(EffectId /*id*/, int /*player*/) override {}
	void SetDrawPath(EffectId /*id*/, psys::DrawPath /*path*/) override {}
	void SetDrawOffset(EffectId /*id*/, glm::vec3 /*offset*/) override {}
	[[nodiscard]] const particles::ShieldSphere* FindShield(glm::vec3 /*point*/, float /*margin*/) const override
	{
		return nullptr;
	}
	[[nodiscard]] size_t GetSoundCount() const override { return 0; }
	void SetDrawn(EffectId /*id*/, bool /*drawn*/) override {}
	void AddTarget(EffectId /*id*/, entt::entity /*target*/) override {}

	/// Kept in its engine state as the real service keeps it, for a rule a test runs to take
	void AddGestureTrail(std::shared_ptr<particles::GestureTrail> trail) override
	{
		if (trail != nullptr)
		{
			_state.pendingGestures.push_back(trail.use_count() == 1 ? std::move(*trail) : *trail);
		}
	}
	void UpdateFrame(float /*gameSeconds*/, const HandFrame& /*hand*/) override {}
	void AddLightSheet(const std::shared_ptr<particles::LightSheet>& /*sheet*/) override {}
	[[nodiscard]] std::vector<std::shared_ptr<particles::LightSheet>> LightSheets() override { return {}; }
	void CloseDown(EffectId /*id*/) override {}
	void Delete(EffectId /*id*/) override {}
	[[nodiscard]] bool IsRunning(EffectId /*id*/) const override { return false; }
	[[nodiscard]] psys::Effect* Find(EffectId /*id*/) override { return nullptr; }

	void ProcessTurn() override {}
	void Reset() override {}

	[[nodiscard]] std::vector<EffectInfo> GetEffects() const override { return {}; }
	[[nodiscard]] std::vector<std::string> GetFileNames() const override { return {}; }
	void SetPaused(bool /*paused*/) override {}
	[[nodiscard]] bool IsPaused() const override { return false; }

	[[nodiscard]] psys::manager::State& GetState() noexcept override { return _state; }

protected:
	[[nodiscard]] entt::registry& ModuleStore() noexcept override { return _store; }

private:
	/// The files' own state, declared first so that it goes after the effects that point into it
	entt::registry _store;
	psys::manager::State _state;
};
} // namespace openblack::test
