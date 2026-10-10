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

#include "ECS/Systems/Implementations/FallingSpellSystem.h"
#include "ECS/Systems/Implementations/HandMagicState.h"
#include "ECS/Systems/Implementations/MagicObjectsSystem.h"
#include "ECS/Systems/Implementations/SpellSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Magic/FlockMiracle.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The game's miracles behind the service: each method hands on to the miracles under src/Magic, which keep their own
/// state in the spell, magic object, falling spell and hand magic stores this service owns. The flock miracles it holds
/// keep none.
class MagicSystem final: public MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType type, PlayerNames player, glm::vec3 point, const magic::SpellCastData& cast,
	                         const psys::ProcessInfo& info) override;
	entt::entity CastOnObject(MagicType type, PlayerNames player, entt::entity target, const magic::SpellCastData& cast,
	                          const psys::ProcessInfo& info) override;
	void CloseDown(entt::entity spell) override;
	[[nodiscard]] bool CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) override;
	bool SpellEvent(entt::entity spell, const psys::SpellEventInfo& event) override;
	void PayForSpell(entt::entity spell, float cost) override;

	entt::entity CreateDispenser(glm::vec3 position, MagicType type, float yAngleRadians) override;
	void ChargeDispenser(entt::entity dispenser) override;
	void SetDispenserPeriod(entt::entity dispenser, float seconds) override;
	entt::entity CreateOneOffSeed(glm::vec3 position, SpellSeedType seed, int powerUp, float multiplier) override;
	entt::entity CreateOneOffSeedFor(glm::vec3 position, MagicType type) override;
	bool Remove(entt::entity entity) override;
	entt::entity GiveSeedToHand(PlayerNames player, SpellSeedType seed, int powerUp, float multiplier) override;
	entt::entity SummonSeed(PlayerNames player, SpellSeedType seed, int powerUp) override;
	void DiscardHeldSeed() override;

	[[nodiscard]] bool IsHandBusy() const override;
	[[nodiscard]] magic::PourPose GetHandPour(float fraction) const override;
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override;

	bool SendSpellEvent(entt::entity spell, const psys::SpellEventInfo& event) override;
	float ForcePayForSpell(entt::entity spell, float cost) override;
	[[nodiscard]] float SpellStrength(entt::entity spell) override;

	void ProcessTurn() override;
	void Update(float seconds) override;
	void Reset() override;

	void SetIgnoreInfluence(bool ignore) override { _ignoreInfluence = ignore; }
	[[nodiscard]] bool IsIgnoringInfluence() const override { return _ignoreInfluence; }
	void DriveHand(std::optional<HandFrame> frame) override { _drivenHand = frame; }
	[[nodiscard]] std::optional<HandFrame> GetDrivenHand() const override { return _drivenHand; }
	[[nodiscard]] std::vector<SpellInfo> GetSpells() const override;
	[[nodiscard]] std::optional<entt::entity> SpellAt(MagicType type, glm::vec3 point, float radius) const override;
	void RainOnFire(const glm::vec3& point) override;
	[[nodiscard]] std::vector<DispenserInfo> GetDispensers() const override;

	void ProcessGameInputs() override;
	void ProcessTurnStart(uint32_t turn) override;
	void ProcessForests(uint32_t turn) override;
	void RunDebugHooks() override;
	void ProcessSpellParticlesEndOfLoop() override;
	void ProcessHandTurn() override;

	[[nodiscard]] magic::FlockMiracleInterface* Flocks() override { return &_flocks; }

	[[nodiscard]] SpellSystemInterface& SpellStore() override { return _spells; }
	[[nodiscard]] MagicObjectsSystemInterface& MagicObjects() override { return _magicObjects; }
	[[nodiscard]] FallingSpellSystemInterface& FallingSpellStore() override { return _fallingSpell; }
	[[nodiscard]] HandMagicStateInterface& HandMagic() override { return _handMagic; }

private:
	// The stores, declared so that they go in the order the game has always released them in: the spells, the magic
	// objects, the hand magic state, then the falling spell
	FallingSpellSystem _fallingSpell;
	HandMagicState _handMagic;
	MagicObjectsSystem _magicObjects;
	SpellSystem _spells;
	magic::FlockMiracle _flocks;
	/// The debug window's cheat and the testbed's hand, kept for the hand's casting, which does not read them yet
	bool _ignoreInfluence {false};
	std::optional<HandFrame> _drivenHand;
};

} // namespace openblack::ecs::systems
