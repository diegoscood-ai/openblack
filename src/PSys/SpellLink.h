/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// What a spell and its particle effect exchange (PSysInterface::Create 0x68E910 links the PSysManager to its Spell):
// the per-step input (PSysProcessInfo, the spell's +0x64), the events the rules send back (Spell::SpellEvent 0x720F40)
// and the power-up level. Wiki: docs/bw1-notes/magic.md.

namespace openblack::psys
{

/// PSysProcessInfo (0x3C bytes): copied into the spell at InitWithPos and refreshed every turn by the creator
/// (UpdateSpellInfo); every PSys step of the spell's effect reads it
struct ProcessInfo
{
	glm::vec3 interfacePos {0.0f}; ///< +0x00 (UNVERIFIED)
	/// +0x0C the current hand / gesture position, metres (PSysManager::GetCurrentGesturePosn 0x673600); the script
	/// cast puts its "from" point here
	glm::vec3 handPos {0.0f};
	glm::vec3 cameraForward {0.0f}; ///< +0x18 (hand: GInterfaceStatus +0x18; script: target - from)
	glm::vec3 direction {0.0f};     ///< +0x24 the hand velocity / cast direction (Spell +0xD8 at init)
	float power {1.0f};             ///< +0x30 the spell's strength: StrengthFloatProvider
	float curl {0.0f};              ///< +0x34 iface +0x54, the script's curl (UNVERIFIED use)
	bool enabled {true};            ///< +0x38 EventConditionTrueWhenEnabled: the creator still casts it
};

/// SpellEventInfo (0x28 bytes), built by the rules
struct SpellEventInfo
{
	enum Type : int
	{
		Started = 1,          ///< fn_00673070, when the effect starts
		Point = 2,            ///< EventAlways, UR_Explosion, the tornado base, SpellWater drops
		Landed = 3,           ///< LandscapeCollide SendEvent, lightning fork tips
		HitSpell = 4,         ///< a shield or another spell (target = that spell)
		Object = 5,           ///< UR_HealSpellChakra: the target object only
		CanDestroy = 7,       ///< a query: CanBeDestroyedBySpell
		InitWithoutPSys = 11, ///< Spell::InitWithPos of a spell with no particle type
	};
	int type {Point};
	glm::vec3 position {0.0f}; ///< +0x04 metres
	glm::vec3 velocity {0.0f}; ///< +0x10 the movement (Spell +0x2C after an applied event)
	float strength {1.0f};     ///< +0x1C multiplies the effect
	bool checkShields {false}; ///< +0x20
	entt::entity target {entt::null}; ///< +0x24
};

/// The spell behind an effect (PSysManager +0x30 -> Spell)
class SpellSink
{
public:
	SpellSink() = default;
	SpellSink(const SpellSink&) = default;
	SpellSink(SpellSink&&) = default;
	SpellSink& operator=(const SpellSink&) = default;
	SpellSink& operator=(SpellSink&&) = default;
	virtual ~SpellSink() = default;

	/// PSysManager::SpellEvent 0x6734C0 -> Spell::SpellEvent (vt): the rule's result (1 applied, 0 not)
	virtual int SpellEvent(const SpellEventInfo& event) = 0;
	/// PSysManager::GetPowerUpLevel 0x673510: -1 base, 0, 1
	[[nodiscard]] virtual int PowerUpLevel() const = 0;
	/// Spell::NetUnsafeIsMyInterfaceCasting 0x7201E0 (+0x44): this computer's interface casts it
	[[nodiscard]] virtual bool IsMyInterfaceCasting() const { return false; }
	/// Spell::IsHumanPlayerCasting 0x720230 (+0x4C)
	[[nodiscard]] virtual bool IsHumanPlayerCasting() const { return false; }
	/// Spell::IsScriptCasting 0x720270: its creator is the script's player (g_game +0x205A5B; the neutral one)
	[[nodiscard]] virtual bool IsScriptCasting() const { return false; }
	/// Spell::GetPlayer 0x55CDF0 (+0xA4); false without one
	[[nodiscard]] virtual bool Player([[maybe_unused]] int& player) const { return false; }
	/// PSysManager +0xA8: the Spell itself (DefensiveShield fn_006D0B10 gives it as a shield hit's target)
	[[nodiscard]] virtual entt::entity SpellEntity() const { return entt::null; }
};

} // namespace openblack::psys
