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

#include <array>
#include <string_view>
#include <type_traits>

#include "Enums.h"
#include "InfoConstants.h"

// The miracles' info.dat tables: GMagicInfo* per MAGIC_TYPE (0xD37D10), GMagicEffectInfo[42] (0xCC6630),
// GSpellSeedInfo[30] (0xD9D678) and their non-virtual helpers. Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic
{
constexpr size_t k_MagicTypeCount = 42;
constexpr size_t k_SpellSeedCount = 30;
constexpr int k_MagicTypeNotFound = 42;   ///< GetInfoFromText's "none"
constexpr int k_SpellSeedNotFound = 30;   ///< the "none" of the seed lookups by name and by magic type

/// The info.dat section, i.e. the GMagicInfo class, of a MAGIC_TYPE. load_variables creates the objects in MAGIC_TYPE
/// order, one class per section, so the sections in file order give MAGIC_TYPE 0..41.
enum class MagicInfoSection : uint8_t
{
	General,         ///< 0-9 GMagicInfo
	Heal,            ///< 10-11 GMagicHealInfo
	Teleport,        ///< 12 GMagicTeleportInfo
	Forest,          ///< 13 GMagicForestInfo
	Food,            ///< 14-15 GMagicResourceInfo
	StormAndTornado, ///< 16-18 GMagicStormAndTornadoInfo
	Shield,          ///< 19-20 GMagicShieldInfo
	Wood,            ///< 21 GMagicResourceInfo
	Water,           ///< 22-23 GMagicWaterInfo
	FlockFlying,     ///< 24 GMagicFlockFlyingInfo
	FlockGround,     ///< 25 GMagicFlockGroundInfo
	CreatureSpell,   ///< 26-41 GMagicCreatureSpellInfo

	_COUNT
};

struct MagicInfoSlot
{
	MagicInfoSection section;
	uint8_t index; ///< in the section's array
};

/// MAGIC_TYPE -> its section and index (valid for 0..41)
[[nodiscard]] MagicInfoSlot SlotOf(MagicType type);

// ---- GMagicInfo (the 0xD37D10 table) ----

/// The 0xD37D10 entry: the section record, as its GMagicInfo base
[[nodiscard]] const GMagicInfo& GetMagicInfo(const InfoConstants& info, MagicType type);

/// The record as its class; nullptr when the magic type is of another section
template <class T>
[[nodiscard]] const T* GetMagicInfoAs(const InfoConstants& info, MagicType type);

/// GMagicInfo::GetMagicEffectInfo 0x5FB680
[[nodiscard]] const GMagicEffectInfo& GetMagicEffectInfo(const InfoConstants& info, MagicType type);

/// GMagicInfo::GetInfoFromText 0x5FB3B0: stricmp on the effect's debugString; k_MagicTypeNotFound when none
[[nodiscard]] int GetInfoFromText(const InfoConstants& info, std::string_view text);

/// GMagicInfo::IsMaintainedSpell 0x5FB810: FOREST, SHIELD, PHYSICAL_SHIELD
[[nodiscard]] bool IsMaintainedSpell(MagicType type);

/// GetChantsRequiredToCreate 0x5FB830 (= GScript::GetManaForSpell 0x70CD40 via 0x5FB800): costToCreate
[[nodiscard]] float GetChantsRequiredToCreate(const InfoConstants& info, MagicType type);

// The timer getters, seconds (-1 = no limit)
[[nodiscard]] float GetTimerWhenOneShot(const InfoConstants& info, MagicType type);               ///< 0x5FB7C0
[[nodiscard]] float GetTimerWhenPlayerCasting(const InfoConstants& info, MagicType type);         ///< 0x5FB7A0
[[nodiscard]] float GetTimerWhenCreatureCasting(const InfoConstants& info, MagicType type);       ///< 0x5FB7B0
[[nodiscard]] float GetTimerWhenComputerPlayerCasting(const InfoConstants& info, MagicType type); ///< 0x5FB7D0

/// 0x5FB7E0: isCreatureCastFromAbove == 1
[[nodiscard]] bool IsCreatureCastFromAbove(const InfoConstants& info, MagicType type);

/// 0x5FB840: 1 when agressiveRangeMin <= distance <= agressiveRangeMax, else 0
[[nodiscard]] float IsInAggressiveRange(const InfoConstants& info, MagicType type, float distance);

/// GMagicEffectInfo::GetTribalPower 0x5FB6A0: the product of the player's TribalPower[t] (GPlayer +0x68, 1.0 in
/// vanilla) over the tribes the effect flags, clamped to [0.5, 100]; 1 without a player (tribalPower nullptr)
[[nodiscard]] float GetTribalPower(const GMagicEffectInfo& effect, const std::array<float, 9>* tribalPower);

/// GMagicEffectInfo::GetTribalPowerTribe 0x5FB710: the first flagged tribe whose power is over 1, else -1
[[nodiscard]] int GetTribalPowerTribe(const GMagicEffectInfo& effect, const std::array<float, 9>* tribalPower);

// ---- GSpellSeedInfo ----

/// the 0xD9D678 entry
[[nodiscard]] const GSpellSeedInfo& GetSpellSeedInfo(const InfoConstants& info, SpellSeedType seed);

/// GSpellSeedInfo::GetPowerUpFromMagicType 0x72AF70: -1 for magicTypes[0], 0/1/2 for magicTypes[1..3], -1 if none
/// (a seed whose slot 3 is 0 gives 2 for MAGIC_TYPE NONE, as the original)
[[nodiscard]] int GetPowerUpFromMagicType(const GSpellSeedInfo& seed, MagicType type);

/// fn_0072AFA0: the number of power-up levels, 1 + the non-zero powerUpGestures
[[nodiscard]] int GetNumPowerUpLevels(const GSpellSeedInfo& seed);

/// GSpellSeedInfo::GetMagicTypeFromPULevel 0x72AFC0: -1 -> magicTypes[0], pu -> magicTypes[pu + 1]
[[nodiscard]] MagicType GetMagicTypeFromPULevel(const GSpellSeedInfo& seed, int powerUp);

/// GetMagicInfoFromPULevel 0x72AFE0: that level's GMagicInfo, the base one when the level has no magic type (0)
[[nodiscard]] const GMagicInfo& GetMagicInfoFromPULevel(const InfoConstants& info, const GSpellSeedInfo& seed, int powerUp);

/// fn_0072B010: the gesture that powers the seed up to that magic type's level, and the level (-1 for the base
/// type, which has no gesture: 0)
[[nodiscard]] GestureType GetPowerUpGesture(const GSpellSeedInfo& seed, MagicType type, int* powerUp);

/// GSpellSeedInfo::SpellSeedIsOfMagicType 0x72B060: any of magicTypes[0..3]
[[nodiscard]] bool SpellSeedIsOfMagicType(const GSpellSeedInfo& seed, MagicType type);

/// GetFirstSpellSeedForMagicType 0x72B090: SpellSeedType::None (-1) when no seed has it
[[nodiscard]] SpellSeedType GetFirstSpellSeedForMagicType(const InfoConstants& info, MagicType type);

/// fn_0072B100: GetPowerUpGesture on the first seed of that magic type (0 and -1 without one)
[[nodiscard]] GestureType GetPowerUpGestureForMagicType(const InfoConstants& info, MagicType type, int* powerUp);

/// fn_0072B0D0: the first existing seed whose iconIndex is that one, else -1
[[nodiscard]] int GetSpellSeedFromIconIndex(const InfoConstants& info, uint32_t iconIndex);

/// fn_0072B170: stricmp on the seed's debugString; k_SpellSeedNotFound when none
[[nodiscard]] int GetSpellSeedFromText(const InfoConstants& info, std::string_view text);

/// fn_0072B1C0: the first seed with that magic type; k_SpellSeedNotFound when none
[[nodiscard]] int GetSpellSeedForMagicType(const InfoConstants& info, MagicType type);

/// The power-up level a spell of this magic type runs at (PSys GetPowerUpLevel 0x673510, the storm, the fireball
/// rows). GMagicInfo::powerupType is -1 in every row and nothing writes it, so this is the seed-derived level of
/// GetPowerUpFromMagicType on the first seed of the type (-1 base, 0 PU one, 1 PU two). UNVERIFIED (PLAN R3).
[[nodiscard]] int GetPowerUpLevel(const InfoConstants& info, MagicType type);

// ---- template definition ----

namespace detail
{
[[nodiscard]] const GMagicInfo* SectionRecord(const InfoConstants& info, MagicInfoSlot slot);

/// Whether a record of that section is a T (the resource and radius bases cover two sections each)
template <class T>
constexpr bool IsOfSection(MagicInfoSection section)
{
	using S = MagicInfoSection;
	if constexpr (std::is_same_v<T, GMagicGeneralInfo>)
	{
		return section == S::General;
	}
	else if constexpr (std::is_same_v<T, GMagicHealInfo>)
	{
		return section == S::Heal;
	}
	else if constexpr (std::is_same_v<T, GMagicTeleportInfo>)
	{
		return section == S::Teleport;
	}
	else if constexpr (std::is_same_v<T, GMagicForestInfo>)
	{
		return section == S::Forest;
	}
	else if constexpr (std::is_same_v<T, GMagicFoodInfo>)
	{
		return section == S::Food;
	}
	else if constexpr (std::is_same_v<T, GMagicWoodInfo>)
	{
		return section == S::Wood;
	}
	else if constexpr (std::is_same_v<T, GMagicResourceInfo>)
	{
		return section == S::Food || section == S::Wood;
	}
	else if constexpr (std::is_same_v<T, GMagicStormAndTornadoInfo>)
	{
		return section == S::StormAndTornado;
	}
	else if constexpr (std::is_same_v<T, GMagicShieldInfo>)
	{
		return section == S::Shield;
	}
	else if constexpr (std::is_same_v<T, GMagicRadiusSpellInfo>)
	{
		return section == S::StormAndTornado || section == S::Shield;
	}
	else if constexpr (std::is_same_v<T, GMagicWaterInfo>)
	{
		return section == S::Water;
	}
	else if constexpr (std::is_same_v<T, GMagicFlockFlyingInfo>)
	{
		return section == S::FlockFlying;
	}
	else if constexpr (std::is_same_v<T, GMagicFlockGroundInfo>)
	{
		return section == S::FlockGround;
	}
	else if constexpr (std::is_same_v<T, GMagicCreatureSpellInfo>)
	{
		return section == S::CreatureSpell;
	}
	else
	{
		static_assert(sizeof(T) == 0, "not a GMagicInfo section class");
	}
}
} // namespace detail

template <class T>
const T* GetMagicInfoAs(const InfoConstants& info, MagicType type)
{
	const auto slot = SlotOf(type);
	if (!detail::IsOfSection<T>(slot.section))
	{
		return nullptr;
	}
	return static_cast<const T*>(detail::SectionRecord(info, slot));
}
} // namespace openblack::magic
