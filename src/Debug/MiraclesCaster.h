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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "MiraclesModel.h"

namespace openblack::debug::miracles
{

/// A spiritual shield breaks, as when a blow its caster can't pay for empties it: its people take up its fall
/// (REACT_TO_MAGIC_SHIELD_DESTROYED) and it closes down. Whether it was a live shield spell. The Magic window's and the
/// testbed's
bool BreakShield(entt::entity spell);

/// What a strike did to a shield
struct ShieldStrike
{
	/// The shield's strength after it paid for the blow
	float strength {0.0f};
	/// The blow emptied it: it fell, and its people take up its fall
	bool destroyed {false};
	/// Its struck reaction was made by this blow, rather than renewed
	bool reactionMade {false};
};

/// The available shield spell (spiritual or physical) nearest a world point (x, z), or none
[[nodiscard]] entt::entity NearestShield(glm::vec2 point);

/// A shield struck by another spell, as when a miracle meets it, through the game's own spell-to-spell hit: the striker,
/// the live fireball nearest the shield or else one cast as the player's for the blow and closed down at once, pays an
/// event and the shield pays the blow. A shield left with strength takes up its struck reaction (made the first time,
/// renewed after), an emptied one its fall. None when it is no live shield. The Magic window's and the testbed's
[[nodiscard]] std::optional<ShieldStrike> StrikeShield(entt::entity spell);

/// The Magic window's miracle caster over the game's spells: a cast as the player's, as the hand's cast is. Its points are
/// world points; it turns them into map positions for the spells
class GameSpellCaster final: public CasterInterface
{
public:
	entt::entity CastAtPoint(const CastPlan& plan, PlayerNames player, glm::vec3 point) override;
	entt::entity CastOnObject(const CastPlan& plan, PlayerNames player, entt::entity target) override;
	[[nodiscard]] bool CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) const override;
};

/// The Magic window's miracle dispensers over the game's own: a one-shot orb as the map script's one-shot with a power-up
/// makes it, the seed into the hand as the script's one-shot in the hand does, and a dispenser as the map script's
/// spell dispenser command makes it (the nearest town, angle 0, scale 1)
class GameDispenserCreator final: public DispenserCreatorInterface
{
public:
	entt::entity CreateOneShot(SpellSeedType seed, int powerUpLevel, glm::vec3 point) override;
	entt::entity CreateOneShotInHand(SpellSeedType seed, int powerUpLevel, PlayerNames player) override;
	entt::entity CreatePermanent(AbodeInfo abode, MagicType magic, uint32_t periodTurns, glm::vec3 point) override;
};

} // namespace openblack::debug::miracles
