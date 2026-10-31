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

#include <array>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "ECS/Systems/Implementations/FallingSpellSystem.h"
#include "ECS/Systems/Implementations/HandMagicState.h"
#include "ECS/Systems/Implementations/MagicObjectsSystem.h"
#include "ECS/Systems/Implementations/SpellSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Magic/FlockMiracle.h"
#include "Magic/MagicWorldInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// The miracles' world in the game, over our own code: the land and its cells, the influence, the effects, the water's
/// drops, the heal's search, the piles and what a miracle may destroy. It keeps no state. The hooks it does not
/// override are what the game does today: the hand's influence is the point's, the hand's use of the land and a
/// player's miracle on an object do nothing more. Its points are map positions (x, z and the altitude above the land),
/// but for ApplyEffectAt's, which is a point in the world
class GameMagicWorld final: public magic::MagicWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 xz) const override;
	[[nodiscard]] bool InBounds(glm::vec3 point) const override;
	[[nodiscard]] bool IsDryLand(glm::vec3 point) const override;
	[[nodiscard]] bool IsLand(glm::vec3 point) const override;
	[[nodiscard]] bool InInfluence(PlayerNames player, glm::vec3 point) const override;
	[[nodiscard]] std::optional<glm::vec3> PositionOf(entt::entity object) const override;
	bool ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source) override;
	std::vector<entt::entity> ApplyEffectAt(glm::vec3 point, const magic::EffectValues& values,
	                                        const magic::EffectSource& source) override;
	void Water(const magic::WaterDrop& drop) override;
	[[nodiscard]] std::vector<entt::entity> HealTargets(glm::vec3 point, float radius, size_t maximum) const override;
	void CurePoison(entt::entity object) override;
	bool AddResource(ResourceType type, glm::vec3 point, uint32_t amount, bool speedUp, PlayerNames player) override;
	[[nodiscard]] bool CanBeDestroyedBySpell(entt::entity object, entt::entity spell) const override;
	void PlayerAffected(entt::entity object, MagicType type, PlayerNames player) override;
};

/// The game's miracles behind the service: each method hands on to the miracles under src/Magic, which keep their own
/// state in the spell, magic object, falling spell and hand magic stores this service owns. The flock miracles it holds
/// keep none.
class MagicSystem final: public MagicSystemInterface
{
public:
	entt::entity CastAtPoint(MagicType type, PlayerNames player, glm::vec3 point, const magic::SpellCastData& cast,
	                         const particles::ProcessInfo& info) override;
	entt::entity CastOnObject(MagicType type, PlayerNames player, entt::entity target, const magic::SpellCastData& cast,
	                          const particles::ProcessInfo& info) override;
	void CloseDown(entt::entity spell) override;
	// Creatures casting miracles (MagicCreatureCasting.cpp)
	bool CastByCreature(entt::entity creature, MagicType type, entt::entity target) override;
	bool CastByCreatureAt(entt::entity creature, MagicType type, glm::vec3 place) override;
	void ReleaseCreatureCast(entt::entity creature) override;
	[[nodiscard]] bool CanCreatureCastAt(MagicType type, entt::entity target) override;
	[[nodiscard]] bool CanCreatureCastAtPlace(MagicType type, glm::vec3 place) override;
	void KeepCreatureBeams() override;
	/// The two beams of a creature's cast from above, at the hand's mirror and at the hand of its body as posed the turn
	/// before, both aimed at the point the miracle flows from; none without its species' rig or the particles
	std::array<entt::entity, 2> StartCreatureBeams(entt::entity creature, glm::vec3 aim);
	/// How far above what it is cast at a creature's miracle cast from above flows from, by that thing's height: ten,
	/// or 1.3 times the height when that is not below ten. A height that is no number gives ten
	[[nodiscard]] static float CastFromAboveRaise(float targetHeight);
	// An effect whose values name nobody who applied it takes who applied it from where it came from
	void ApplyEffectAt(glm::vec3 point, const magic::EffectValues& values, PlayerNames player) override;
	bool ApplyEffectToObject(entt::entity object, const magic::EffectValues& values, PlayerNames player) override;
	bool ApplyEffectToObject(entt::entity object, const magic::EffectValues& values,
	                         const magic::EffectSource& source) override;
	[[nodiscard]] bool CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) override;
	bool SpellEvent(entt::entity spell, const particles::SpellEventInfo& event) override;
	void PayForSpell(entt::entity spell, float cost) override;

	entt::entity CreateDispenser(glm::vec3 position, MagicType type, float yAngleRadians) override;
	void ChargeDispenser(entt::entity dispenser) override;
	void SetDispenserPeriod(entt::entity dispenser, float seconds) override;
	void SetDispenserActive(entt::entity dispenser, bool active) override;
	void SetDispenserTurns(entt::entity dispenser, uint32_t turns) override;
	entt::entity CreateOneOffSeed(glm::vec3 position, SpellSeedType seed, int powerUp, float multiplier) override;
	entt::entity CreateOneOffSeedFor(glm::vec3 position, MagicType type) override;
	bool Remove(entt::entity entity) override;
	entt::entity GiveSeedToHand(PlayerNames player, SpellSeedType seed, int powerUp, float multiplier) override;
	entt::entity SummonSeed(PlayerNames player, SpellSeedType seed, int powerUp) override;
	entt::entity SummonSeedAtSite(entt::entity site, SpellSeedType seed, int powerUp) override;
	void DiscardHeldSeed() override;

	bool TapAction() override;
	bool PressAction() override;
	void ReleaseAction() override;
	[[nodiscard]] bool IsHandBusy() const override;
	[[nodiscard]] magic::PourPose GetHandPour(float fraction) const override;
	[[nodiscard]] std::optional<entt::entity> GetHeldSeed() const override;
	[[nodiscard]] HandResult GetLastHandResult() const override { return _lastHandResult; }

	bool SendSpellEvent(entt::entity spell, const particles::SpellEventInfo& event) override;
	float ForcePayForSpell(entt::entity spell, float cost) override;
	[[nodiscard]] float SpellStrength(entt::entity spell) override;

	void ProcessTurn() override;
	void Update(float seconds) override;
	void Reset() override;

	[[nodiscard]] float GetTribalPower(PlayerNames player, Tribe tribe) const override;
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
	[[nodiscard]] magic::MagicWorldInterface& World() override { return _world; }

private:
	/// Keeps what the hand's press or release did, unless it did nothing
	void RecordHandResult(HandResult result);
	/// What a creature's cast at a thing and at a place of the world share, once its size is known: the body pays, the
	/// miracle starts there and flows from the hands, or from above where it is cast. Whether it could: not when the
	/// creature is too tired to pay for it, nor when the miracle can't go to the thing or the place.
	bool StartCreatureCast(entt::entity creature, MagicType type, std::optional<entt::entity> target, glm::vec3 at,
	                       float magnitude);

	// The stores, declared so that they go in the order the game has always released them in: the spells, the magic
	// objects, the hand magic state, then the falling spell
	FallingSpellSystem _fallingSpell;
	HandMagicState _handMagic;
	MagicObjectsSystem _magicObjects;
	SpellSystem _spells;
	magic::FlockMiracle _flocks;
	GameMagicWorld _world;
	/// The debug window's cheat and the testbed's hand, kept for the hand's casting, which does not read them yet
	bool _ignoreInfluence {false};
	std::optional<HandFrame> _drivenHand;
	/// What the last press, release or drop through the service did, for the debug window and the testbed
	HandResult _lastHandResult {HandResult::None};
	/// A press through the service went to the hand's seed press, and its release has not come yet
	bool _pressHeld {false};
};

} // namespace openblack::ecs::systems
