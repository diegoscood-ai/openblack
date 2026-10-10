/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Define LOCATOR_IMPLEMENTATIONS before including this header: the fake owns the real stores of the miracles"
#endif

#include "ECS/Systems/Implementations/FallingSpellSystem.h"
#include "ECS/Systems/Implementations/HandMagicState.h"
#include "ECS/Systems/Implementations/MagicObjectsSystem.h"
#include "ECS/Systems/Implementations/SpellSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Enums.h"
#include "Magic/Core/SpellCastData.h"
#include "Magic/HandMotion.h"
#include "Particles/SpellLink.h"

namespace openblack::test
{
/// The miracles as a test that does not look at them sees them: nothing is ever cast, paid, made or found, the hand
/// holds nothing, and every step does nothing. Its stores are real and empty, as the game's service owns them, so that
/// a game made after the fake was injected finds them. A test's own fake of the miracles derives from it and overrides
/// only what it records, so that a method the service gains reaches every fake from here. Inject a fake with
/// Locator::magicSystem::emplace<Fake>()
class InertMagicSystem: public ecs::systems::MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType /*type*/, PlayerNames /*player*/, glm::vec3 /*point*/,
	                         const magic::SpellCastData& /*cast*/, const psys::ProcessInfo& /*info*/) override
	{
		return entt::null;
	}
	entt::entity CastOnObject(MagicType /*type*/, PlayerNames /*player*/, entt::entity /*target*/,
	                          const magic::SpellCastData& /*cast*/, const psys::ProcessInfo& /*info*/) override
	{
		return entt::null;
	}
	void CloseDown(entt::entity /*spell*/) override {}
	[[nodiscard]] bool CanCastAt(MagicType /*type*/, PlayerNames /*player*/, glm::vec3 /*point*/) override { return false; }
	bool SpellEvent(entt::entity /*spell*/, const psys::SpellEventInfo& /*event*/) override { return false; }
	void PayForSpell(entt::entity /*spell*/, float /*cost*/) override {}

	entt::entity CreateDispenser(glm::vec3 /*position*/, MagicType /*type*/, float /*yAngleRadians*/) override
	{
		return entt::null;
	}
	void SetDispenserPeriod(entt::entity /*dispenser*/, float /*seconds*/) override {}
	entt::entity CreateOneOffSeed(glm::vec3 /*position*/, SpellSeedType /*seed*/, int /*powerUp*/,
	                              float /*multiplier*/) override
	{
		return entt::null;
	}
	entt::entity CreateOneOffSeedFor(glm::vec3 /*position*/, MagicType /*type*/) override { return entt::null; }
	bool Remove(entt::entity /*entity*/) override { return false; }
	entt::entity GiveSeedToHand(PlayerNames /*player*/, SpellSeedType /*seed*/, int /*powerUp*/, float /*multiplier*/) override
	{
		return entt::null;
	}
	entt::entity SummonSeed(PlayerNames /*player*/, SpellSeedType /*seed*/, int /*powerUp*/) override { return entt::null; }
	void DiscardHeldSeed() override {}

	[[nodiscard]] bool IsHandBusy() const override { return false; }
	[[nodiscard]] magic::PourPose GetHandPour(float /*fraction*/) const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override { return std::nullopt; }

	bool SendSpellEvent(entt::entity /*spell*/, const psys::SpellEventInfo& /*event*/) override { return false; }
	float ForcePayForSpell(entt::entity /*spell*/, float /*cost*/) override { return 0.0f; }
	[[nodiscard]] float SpellStrength(entt::entity /*spell*/) override { return 0.0f; }

	void ProcessTurn() override {}
	void Update(float /*seconds*/) override {}
	void Reset() override {}

	void SetIgnoreInfluence(bool /*ignore*/) override {}
	[[nodiscard]] bool IsIgnoringInfluence() const override { return false; }
	void DriveHand(std::optional<HandFrame> /*frame*/) override {}
	[[nodiscard]] std::optional<HandFrame> GetDrivenHand() const override { return std::nullopt; }
	[[nodiscard]] std::vector<SpellInfo> GetSpells() const override { return {}; }
	void RainOnFire(const glm::vec3& /*point*/) override {}
	[[nodiscard]] std::vector<DispenserInfo> GetDispensers() const override { return {}; }

	void ProcessGameInputs() override {}
	void ProcessTurnStart(uint32_t /*turn*/) override {}
	void ProcessForests(uint32_t /*turn*/) override {}
	void RunDebugHooks() override {}
	void ProcessSpellParticlesEndOfLoop() override {}
	void ProcessHandTurn() override {}

	[[nodiscard]] ecs::systems::SpellSystemInterface& SpellStore() override { return _spells; }
	[[nodiscard]] ecs::systems::MagicObjectsSystemInterface& MagicObjects() override { return _magicObjects; }
	[[nodiscard]] ecs::systems::FallingSpellSystemInterface& FallingSpellStore() override { return _fallingSpell; }
	[[nodiscard]] ecs::systems::HandMagicStateInterface& HandMagic() override { return _handMagic; }

private:
	ecs::systems::FallingSpellSystem _fallingSpell;
	ecs::systems::HandMagicState _handMagic;
	ecs::systems::MagicObjectsSystem _magicObjects;
	ecs::systems::SpellSystem _spells;
};
} // namespace openblack::test
