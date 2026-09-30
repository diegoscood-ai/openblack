/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MagicTables.h"

#include <cctype>
#include <cstring>

#include <algorithm>

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// The sections in file (= MAGIC_TYPE) order and their counts (InfoConstants v120, core.md section 2)
constexpr std::array<uint8_t, static_cast<size_t>(MagicInfoSection::_COUNT)> k_SectionSizes = {
    10, 2, 1, 1, 2, 3, 2, 1, 2, 1, 1, 16};
static_assert([] {
	int total = 0;
	for (const auto n : k_SectionSizes)
	{
		total += n;
	}
	return total;
}() == static_cast<int>(k_MagicTypeCount));

constexpr std::array<MagicInfoSlot, k_MagicTypeCount> k_Slots = [] {
	std::array<MagicInfoSlot, k_MagicTypeCount> slots {};
	size_t type = 0;
	for (size_t section = 0; section < k_SectionSizes.size(); ++section)
	{
		for (uint8_t i = 0; i < k_SectionSizes[section]; ++i)
		{
			slots[type++] = {static_cast<MagicInfoSection>(section), i};
		}
	}
	return slots;
}();

/// __stricmp against a fixed-size name
bool EqualsNoCase(std::string_view text, const std::array<char, 0x30>& name)
{
	const size_t length = strnlen(name.data(), name.size());
	if (text.size() != length)
	{
		return false;
	}
	return std::equal(text.begin(), text.end(), name.begin(), [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	});
}

template <class Array>
const GMagicInfo* At(const Array& records, uint8_t index)
{
	return &records.at(index);
}
} // namespace

MagicInfoSlot magic::SlotOf(MagicType type)
{
	return k_Slots.at(static_cast<size_t>(type));
}

const GMagicInfo* magic::detail::SectionRecord(const InfoConstants& info, MagicInfoSlot slot)
{
	switch (slot.section)
	{
	case MagicInfoSection::General:
		return At(info.magicGeneral, slot.index);
	case MagicInfoSection::Heal:
		return At(info.magicHeal, slot.index);
	case MagicInfoSection::Teleport:
		return At(info.magicTeleport, slot.index);
	case MagicInfoSection::Forest:
		return At(info.magicForest, slot.index);
	case MagicInfoSection::Food:
		return At(info.magicFood, slot.index);
	case MagicInfoSection::StormAndTornado:
		return At(info.magicStormAndTornado, slot.index);
	case MagicInfoSection::Shield:
		return At(info.magicShield, slot.index);
	case MagicInfoSection::Wood:
		return At(info.magicWood, slot.index);
	case MagicInfoSection::Water:
		return At(info.magicWater, slot.index);
	case MagicInfoSection::FlockFlying:
		return At(info.magicFlockFlying, slot.index);
	case MagicInfoSection::FlockGround:
		return At(info.magicFlockGround, slot.index);
	case MagicInfoSection::CreatureSpell:
	default:
		return At(info.magicCreatureSpell, slot.index);
	}
}

const GMagicInfo& magic::GetMagicInfo(const InfoConstants& info, MagicType type)
{
	return *detail::SectionRecord(info, SlotOf(type));
}

const GMagicEffectInfo& magic::GetMagicEffectInfo(const InfoConstants& info, MagicType type)
{
	return info.magicEffect.at(static_cast<size_t>(type));
}

int magic::GetInfoFromText(const InfoConstants& info, std::string_view text)
{
	// GetMagicInfoText 0x5FB3F0 = the effect info of table[i]'s own magicType, + 0x34 (debugString)
	for (size_t i = 0; i < k_MagicTypeCount; ++i)
	{
		const auto& record = GetMagicInfo(info, static_cast<MagicType>(i));
		if (EqualsNoCase(text, GetMagicEffectInfo(info, record.magicType).debugString))
		{
			return static_cast<int>(i);
		}
	}
	return k_MagicTypeNotFound;
}

bool magic::IsMaintainedSpell(MagicType type)
{
	const auto t = static_cast<uint32_t>(type);
	return t == 13 || (t > 18 && t <= 20);
}

float magic::GetChantsRequiredToCreate(const InfoConstants& info, MagicType type)
{
	return GetMagicEffectInfo(info, type).costToCreate;
}

float magic::GetTimerWhenOneShot(const InfoConstants& info, MagicType type)
{
	return GetMagicEffectInfo(info, type).timerWhenOneShot;
}

float magic::GetTimerWhenPlayerCasting(const InfoConstants& info, MagicType type)
{
	return GetMagicEffectInfo(info, type).timerWhenPlayerCasting;
}

float magic::GetTimerWhenCreatureCasting(const InfoConstants& info, MagicType type)
{
	return GetMagicEffectInfo(info, type).timerWhenCreatureCasting;
}

float magic::GetTimerWhenComputerPlayerCasting(const InfoConstants& info, MagicType type)
{
	return GetMagicEffectInfo(info, type).timerWhenComputerPlayerCasting;
}

bool magic::IsCreatureCastFromAbove(const InfoConstants& info, MagicType type)
{
	return GetMagicInfo(info, type).isCreatureCastFromAbove == 1;
}

float magic::IsInAggressiveRange(const InfoConstants& info, MagicType type, float distance)
{
	const auto& effect = GetMagicEffectInfo(info, type);
	return distance >= effect.agressiveRangeMin && distance <= effect.agressiveRangeMax ? 1.0f : 0.0f;
}

float magic::GetTribalPower(const GMagicEffectInfo& effect, const std::array<float, 9>* tribalPower)
{
	if (tribalPower == nullptr)
	{
		return 1.0f;
	}
	float power = 1.0f;
	for (size_t t = 0; t < tribalPower->size(); ++t)
	{
		if (effect.useTribalPowerMultiplier.at(t) != 0)
		{
			power *= (*tribalPower)[t];
		}
	}
	if (power < 0.0f)
	{
		return 0.5f;
	}
	if (power > 100.0f)
	{
		return 100.0f;
	}
	return power > 0.5f ? power : 0.5f;
}

int magic::GetTribalPowerTribe(const GMagicEffectInfo& effect, const std::array<float, 9>* tribalPower)
{
	if (tribalPower == nullptr)
	{
		return -1;
	}
	for (size_t t = 0; t < tribalPower->size(); ++t)
	{
		if (effect.useTribalPowerMultiplier.at(t) != 0 && (*tribalPower)[t] > 1.0f)
		{
			return static_cast<int>(t);
		}
	}
	return -1;
}

const GSpellSeedInfo& magic::GetSpellSeedInfo(const InfoConstants& info, SpellSeedType seed)
{
	return info.spellSeed.at(static_cast<size_t>(seed));
}

int magic::GetPowerUpFromMagicType(const GSpellSeedInfo& seed, MagicType type)
{
	if (seed.magicTypes[0] == type)
	{
		return -1;
	}
	for (int i = 0; i < 3; ++i)
	{
		if (seed.magicTypes.at(static_cast<size_t>(i + 1)) == type)
		{
			return i;
		}
	}
	return -1;
}

int magic::GetNumPowerUpLevels(const GSpellSeedInfo& seed)
{
	return 1 + static_cast<int>(std::count_if(seed.powerUpGestures.begin(), seed.powerUpGestures.end(),
	                                          [](GestureType g) { return g != GestureType::None; }));
}

MagicType magic::GetMagicTypeFromPULevel(const GSpellSeedInfo& seed, int powerUp)
{
	// no bounds check in the original: the level is -1..2
	return powerUp == -1 ? seed.magicTypes[0] : seed.magicTypes.at(static_cast<size_t>(powerUp + 1));
}

const GMagicInfo& magic::GetMagicInfoFromPULevel(const InfoConstants& info, const GSpellSeedInfo& seed, int powerUp)
{
	const auto type = GetMagicTypeFromPULevel(seed, powerUp);
	return GetMagicInfo(info, type != MagicType::None ? type : seed.magicTypes[0]);
}

GestureType magic::GetPowerUpGesture(const GSpellSeedInfo& seed, MagicType type, int* powerUp)
{
	if (powerUp != nullptr)
	{
		*powerUp = -1;
	}
	if (seed.magicTypes[0] == type)
	{
		return GestureType::None;
	}
	for (int i = 0; i < 3; ++i)
	{
		if (seed.magicTypes.at(static_cast<size_t>(i + 1)) == type)
		{
			if (powerUp != nullptr)
			{
				*powerUp = i;
			}
			return seed.powerUpGestures.at(static_cast<size_t>(i));
		}
	}
	return GestureType::None;
}

bool magic::SpellSeedIsOfMagicType(const GSpellSeedInfo& seed, MagicType type)
{
	return std::find(seed.magicTypes.begin(), seed.magicTypes.end(), type) != seed.magicTypes.end();
}

SpellSeedType magic::GetFirstSpellSeedForMagicType(const InfoConstants& info, MagicType type)
{
	for (size_t i = 0; i < k_SpellSeedCount; ++i)
	{
		if (SpellSeedIsOfMagicType(info.spellSeed.at(i), type))
		{
			return static_cast<SpellSeedType>(i);
		}
	}
	return SpellSeedType::None;
}

GestureType magic::GetPowerUpGestureForMagicType(const InfoConstants& info, MagicType type, int* powerUp)
{
	if (powerUp != nullptr)
	{
		*powerUp = -1;
	}
	const auto seed = GetFirstSpellSeedForMagicType(info, type);
	if (seed == SpellSeedType::None)
	{
		return GestureType::None;
	}
	return GetPowerUpGesture(GetSpellSeedInfo(info, seed), type, powerUp);
}

int magic::GetSpellSeedFromIconIndex(const InfoConstants& info, uint32_t iconIndex)
{
	for (size_t i = 0; i < k_SpellSeedCount; ++i)
	{
		const auto& seed = info.spellSeed.at(i);
		if (seed.exists != 0 && seed.iconIndex == iconIndex)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

int magic::GetSpellSeedFromText(const InfoConstants& info, std::string_view text)
{
	for (size_t i = 0; i < k_SpellSeedCount; ++i)
	{
		if (EqualsNoCase(text, info.spellSeed.at(i).debugString))
		{
			return static_cast<int>(i);
		}
	}
	return k_SpellSeedNotFound;
}

int magic::GetSpellSeedForMagicType(const InfoConstants& info, MagicType type)
{
	const auto seed = GetFirstSpellSeedForMagicType(info, type);
	return seed == SpellSeedType::None ? k_SpellSeedNotFound : static_cast<int>(seed);
}

int magic::GetPowerUpLevel(const InfoConstants& info, MagicType type)
{
	const auto seed = GetFirstSpellSeedForMagicType(info, type);
	return seed == SpellSeedType::None ? -1 : GetPowerUpFromMagicType(GetSpellSeedInfo(info, seed), type);
}
