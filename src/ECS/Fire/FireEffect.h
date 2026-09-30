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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::effects
{
struct EffectValues;
} // namespace openblack::ecs::effects

// The fire system (SpreadEffect.cpp, FireEffect 0x72E940..0x7310F0): every object that is hotter than the ambient air
// has a FireEffect with its temperature. Above its combustion temperature Tc it burns: it heats up to 2 Tc, loses life,
// chars, heats what is near (the same model for fireballs, lightning, held and thrown objects) and cools down again when
// nothing heats it. One game turn = 0.1 s. Report: tmp_dis\miracles\destructive.md §2; wiki docs/bw1-notes/magic.md.

namespace openblack::ecs::fire
{
/// FireEffect (GameThing, 0x50 bytes, vtable 0x9996D4, save type 0x29, "SpreadEffect:")
struct FireEffect
{
	enum Flags : uint8_t
	{
		JustIgnited = 0x01,   ///< Tprev < Tc <= T this turn
		VeryHot = 0x02,       ///< T > 3 Tc
		Cooling = 0x04,       ///< cooled (water, rain or T < Tprev) this turn
		JustExtinguished = 0x08,
		Deleted = 0x10,
		SoundPlaying = 0x20,
	};

	uint32_t id {0};                  ///< the port's handle (villagers keep it: Villager +0x114 is a FireEffect*)
	float temperature {0.0f};         ///< +0x14 T
	float previous {0.0f};            ///< +0x18 T at the end of the last Process
	entt::entity object {entt::null}; ///< +0x1C (its Object +0x44 points back)
	bool hasPlayer {false};           ///< +0x20 the player responsible (GetPlayer 0x72EAB0)
	PlayerNames player {PlayerNames::NEUTRAL};
	/// +0x24 the thing that heated it (a spell, an object); never gets heat back from this fire; cleared when gone
	entt::entity source {entt::null};
	uint32_t reaction {0};  ///< +0x28 REACT_TO_FIRE (10) or REACT_TO_BURNING_OBJECT_IN_HAND (33)
	uint8_t tag {0};        ///< +0x30 copied from 0xDA09E5 at creation; processed while == 0xDA09E4
	float charring {0.0f};  ///< +0x34 0..1
	uint8_t flags {0};      ///< +0x38
	FireEffect* root {this}; ///< +0x40 the first fire of its group (GetFirstCaused 0x732AE0)
	FireEffect* next {nullptr}; ///< +0x44 the next of the group (fn_00732AD0)
	/// +0x48 / +0x4C, the group root only: the villagers fighting it, newest first (AddFireman 0x7309A0)
	std::vector<entt::entity> firemen;

	/// fn_00730180: max(combustion temperature, 40); 0 without an object
	[[nodiscard]] float Tc() const;
	/// fn_007301B0: 2 Tc, the hottest a burning object gets
	[[nodiscard]] float Tmax() const { return 2.0f * Tc(); }
	/// fn_007301C0: MapCoords::GetTemperature 0x605CC0 of the object's position
	[[nodiscard]] float Ambient() const;
	/// fn_007301D0: max(heat capacity, 1)
	[[nodiscard]] float Capacity() const;
	/// IsOnFire 0x730360: T >= Tc
	[[nodiscard]] bool IsOnFire() const;
	/// IsAboveReactionTemperature 0x730380: T >= 100 or T >= Tc
	[[nodiscard]] bool IsAboveReactionTemperature() const;
	/// GetFireFraction 0x7303E0: (T - 0.8 Tc) / (2 Tc - 0.8 Tc), at most 2 x life, in 0..1
	[[nodiscard]] float FireFraction() const;
	/// GetFireRadius 0x72FF10: 1.25 x the object's fire radius x the fraction
	[[nodiscard]] float FireRadius() const;
	/// GetMaxFireRadius 0x730000: 1.25 x its fire radius
	[[nodiscard]] float MaxFireRadius() const;
	/// GetSafeFireRadius 0x730020: min(fire radius, max) + 1
	[[nodiscard]] float SafeFireRadius() const;
	/// fn_0072FF70: the flame height, 1.25 x height x (T - Tamb) / (2 Tc - Tamb) in 0..1
	[[nodiscard]] float FlameHeight() const;
	/// fn_00730600: (T - Tamb) x capacity
	[[nodiscard]] float HeatContent() const;
	/// fn_00730630: T += q / cap, but at most dTmax (by magnitude)
	void AddHeat(float heat, float maxChange);
	/// fn_00730290: the sum of the 2D radii of the group's burning objects
	[[nodiscard]] float GroupBurningRadius() const;
	/// fn_007302E0: the group's highest burningPriority (info +0xB8)
	[[nodiscard]] float GroupBurningPriority() const;
	/// fn_00730070: the group's fire nearest to `position` (within its safe radius or object radius) that is above the
	/// reaction temperature; nullptr if none
	[[nodiscard]] FireEffect* NearestFireToFight(const glm::vec3& position) const;
};

/// FireEffect::AddToMyFireGroup 0x72FBE0: `other` (with its group) joins `self`'s group, right after it
void AddToFireGroup(FireEffect& self, FireEffect& other);

/// Object +0x44: the object's fire, nullptr
[[nodiscard]] FireEffect* Find(entt::entity object);
/// A fire by its handle while it is in the list (Villager::IsValidFire 0x75AD90); nullptr once deleted
[[nodiscard]] FireEffect* Get(uint32_t id);

/// fn_0072ECF0, FireEffect::Create: none if the object refuses a burn (IsEffectReceiver of the static EffectValues
/// 0xDA0980 = BURN 100), can't be set on fire (+0x0A bit 3), has Tc 0 or is being deleted. Its T starts at the object's
/// temperature, it joins the head of the list (g_game +0x205C14) as the root of its own group, gets its graphic and
/// the object StartOnFire.
FireEffect* Create(entt::entity object, bool hasPlayer, PlayerNames player, entt::entity source);
/// FireEffect::ToBeDeleted 0x72EBE0
void ToBeDeleted(FireEffect& fire);

/// Object::GetTemperature 0x639A10: the fire's T, else the ambient temperature
[[nodiscard]] float GetTemperature(entt::entity object);
/// Object::IsOnFire 0x637CC0
[[nodiscard]] bool IsOnFire(entt::entity object);
/// MapCoords::GetTemperature 0x605CC0: 24.7 everywhere (fld 0x930080)
[[nodiscard]] float AmbientTemperature(const glm::vec3& position);

/// FireEffect::SetTemperature 0x72EF10 (Object::SetTemperature 0x639A60): a new fire when hotter than the object, then
/// T = t; an existing fire just takes t (also lower)
void SetTemperature(entt::entity object, float temperature, entt::entity source);
/// FireEffect::SetOnFire 0x72EF60 (Object::SetOnFire 0x639A40): T = 2 Tc x speed + Tc
void SetOnFire(entt::entity object, float speed);

/// FireEffect::ApplyEffectToFireEffectIfNecessary 0x730670 (the start of Object::GetDamageEffect 0x637D00): a burn
/// drives T towards ambient + burn, T += min(10 dT / cap, dT); a negative one (water, beating) cools it. A villager not
/// yet on fire runs (SetupOnFire).
void ApplyEffectToFireEffectIfNecessary(entt::entity object, const effects::EffectValues& values);

/// FireEffect::CheckToSeeIfObjectIsNearOnFireObject 0x730860 (Object::ProcessInHand 0x639AD0 inside the holder's
/// influence): every fire in the map cell of the object's fire centre heats it
void CheckToSeeIfObjectIsNearOnFireObject(entt::entity object);
/// fn_007308F0: the fire passes to a new object (Rock::SplitInTwo): same group, T = Tprev = max of both
void CopyFire(entt::entity from, entt::entity to);
/// fn_00730960: the fire moves to another object (Tree -> DeadTree), with its reaction
void MoveFire(entt::entity from, entt::entity to);
/// FireEffect::StartedMoving 0x730A60 (PlaceObjectInMagicHand, Object::InitialisePhysics): out of its group, no
/// REACT_TO_FIRE; in the hand it is REACT_TO_BURNING_OBJECT_IN_HAND
void StartedMoving(entt::entity object, bool inHand);
/// FireEffect::SetOutMagicHand 0x730AB0
void SetOutMagicHand(entt::entity object);

/// FireEffect::ProcessList 0x730760, once per game turn (GGame::ProcessTurn slot 6)
void ProcessList();
/// Every fire, newest first (the list order)
[[nodiscard]] const std::vector<FireEffect*>& All();
/// fn_0072ED80 and a land is loaded: no fires, no sound slots
void Clear();

/// OPENBLACK_FIRE_TRACE=1
[[nodiscard]] bool TraceEnabled();
} // namespace openblack::ecs::fire
