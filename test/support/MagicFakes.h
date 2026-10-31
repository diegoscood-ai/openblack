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
#include <cstdint>

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
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
#include "Magic/MagicWorldInterface.h"
#include "Particles/SpellLink.h"

namespace openblack::test
{
/// The world as the miracles see it, for a test that looks at what they do to it, as his tests' FakeWorld: flat land
/// at height 0 over x and z of 0 and more, wet west of wetBelowX and with water south of waterBelowZ, every point in
/// the influence while `influence`, the objects it knows where they stand; it records every effect, drop and
/// resource, and heals the first `maximum` of its objects
class FakeMagicWorld final: public magic::MagicWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return 0.0f; }
	[[nodiscard]] bool InBounds(glm::vec3 point) const override { return point.x >= 0.0f && point.z >= 0.0f; }
	[[nodiscard]] bool IsDryLand(glm::vec3 point) const override { return InBounds(point) && !(point.x < wetBelowX); }
	[[nodiscard]] bool IsLand(glm::vec3 point) const override
	{
		return InBounds(point) && !(point.x < wetBelowX) && !(point.z < waterBelowZ);
	}
	[[nodiscard]] bool InInfluence(PlayerNames /*player*/, glm::vec3 /*point*/) const override { return influence; }
	[[nodiscard]] std::optional<glm::vec3> PositionOf(entt::entity object) const override
	{
		const auto found = objects.find(object);
		return found != objects.end() ? std::optional(found->second) : std::nullopt;
	}
	bool ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source) override
	{
		lastSource = source;
		applied.emplace_back(object, values);
		return true;
	}
	std::vector<entt::entity> ApplyEffectAt(glm::vec3 point, const magic::EffectValues& values,
	                                        const magic::EffectSource& source) override
	{
		lastSource = source;
		appliedAt.emplace_back(point, values);
		return {};
	}
	void Water(const magic::WaterDrop& drop) override
	{
		drops.push_back(drop.position);
		rings.push_back(drop.ringGrowth);
		extremeDrops.push_back(drop.extreme);
	}
	[[nodiscard]] std::vector<entt::entity> HealTargets(glm::vec3 /*point*/, float radius, size_t maximum) const override
	{
		lastHealRadius = radius;
		std::vector<entt::entity> targets;
		for (const auto& [entity, position] : objects)
		{
			if (targets.size() < maximum)
			{
				targets.push_back(entity);
			}
		}
		return targets;
	}
	bool AddResource(ResourceType type, glm::vec3 point, uint32_t amount, bool sparkles, PlayerNames /*player*/) override
	{
		resources.push_back({type, point, amount, sparkles});
		return true;
	}
	[[nodiscard]] bool CanBeDestroyedBySpell(entt::entity object, entt::entity /*spell*/) const override
	{
		return destroyable.contains(object);
	}

	struct ResourceRecord
	{
		ResourceType type;
		glm::vec3 point;
		uint32_t amount;
		bool sparkles;
	};
	bool influence {true};
	float wetBelowX {-1.0f};
	/// Cells with water in them, though their land stands above the sea, south of this
	float waterBelowZ {-1.0f};
	std::map<entt::entity, glm::vec3> objects;
	std::vector<std::pair<entt::entity, magic::EffectValues>> applied;
	std::vector<std::pair<glm::vec3, magic::EffectValues>> appliedAt;
	magic::EffectSource lastSource;
	std::vector<glm::vec3> drops;
	std::vector<std::optional<float>> rings;
	std::vector<bool> extremeDrops;
	std::vector<ResourceRecord> resources;
	/// The objects a miracle may destroy; no other
	std::set<entt::entity> destroyable;
	mutable float lastHealRadius {0.0f};
};

/// The miracles as a test that does not look at them sees them: nothing is ever cast, paid, made or found, the hand
/// holds nothing, and every step does nothing. Its stores are real and empty, as the game's service owns them, so that
/// a game made after the fake was injected finds them; its world is a FakeMagicWorld. A test's own fake of the miracles
/// derives from it and overrides only what it records, so that a method the service gains reaches every fake from
/// here. Inject a fake with Locator::magicSystem::emplace<Fake>()
class InertMagicSystem: public ecs::systems::MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType /*type*/, PlayerNames /*player*/, glm::vec3 /*point*/,
	                         const magic::SpellCastData& /*cast*/, const particles::ProcessInfo& /*info*/) override
	{
		return entt::null;
	}
	entt::entity CastOnObject(MagicType /*type*/, PlayerNames /*player*/, entt::entity /*target*/,
	                          const magic::SpellCastData& /*cast*/, const particles::ProcessInfo& /*info*/) override
	{
		return entt::null;
	}
	void CloseDown(entt::entity /*spell*/) override {}
	[[nodiscard]] bool CanCastAt(MagicType /*type*/, PlayerNames /*player*/, glm::vec3 /*point*/) override { return false; }
	bool SpellEvent(entt::entity /*spell*/, const particles::SpellEventInfo& /*event*/) override { return false; }
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
	entt::entity SummonSeedAtSite(entt::entity /*site*/, SpellSeedType /*seed*/, int /*powerUp*/) override
	{
		return entt::null;
	}
	void DiscardHeldSeed() override {}

	bool TapAction() override { return false; }
	bool PressAction() override { return false; }
	void ReleaseAction() override {}
	[[nodiscard]] bool IsHandBusy() const override { return false; }
	[[nodiscard]] magic::PourPose GetHandPour(float /*fraction*/) const override { return {}; }
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override { return std::nullopt; }
	[[nodiscard]] HandResult GetLastHandResult() const override { return HandResult::None; }

	bool SendSpellEvent(entt::entity /*spell*/, const particles::SpellEventInfo& /*event*/) override { return false; }
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
	[[nodiscard]] magic::MagicWorldInterface& World() override { return _world; }
	/// The fake world it hands out, for a test that sets it up and reads what the miracles did to it
	[[nodiscard]] FakeMagicWorld& FakeWorld() { return _world; }

private:
	ecs::systems::FallingSpellSystem _fallingSpell;
	ecs::systems::HandMagicState _handMagic;
	ecs::systems::MagicObjectsSystem _magicObjects;
	ecs::systems::SpellSystem _spells;
	FakeMagicWorld _world;
};
} // namespace openblack::test
