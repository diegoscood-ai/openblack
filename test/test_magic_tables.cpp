/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "Magic/MagicTables.h"
#include "PSys/ParticleTypes.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// A zeroed info block with each magic record's magicType set to where it should be (as the file has it)
std::unique_ptr<InfoConstants> Synthetic()
{
	auto info = std::make_unique<InfoConstants>();
	uint32_t type = 0;
	const auto number = [&type](auto& records) {
		for (auto& record : records)
		{
			record.magicType = static_cast<MagicType>(type++);
		}
	};
	number(info->magicGeneral);
	number(info->magicHeal);
	number(info->magicTeleport);
	number(info->magicForest);
	number(info->magicFood);
	number(info->magicStormAndTornado);
	number(info->magicShield);
	number(info->magicWood);
	number(info->magicWater);
	number(info->magicFlockFlying);
	number(info->magicFlockGround);
	number(info->magicCreatureSpell);
	return info;
}

void SetName(std::array<char, 0x30>& name, const char* text)
{
	name.fill('\0');
	std::strncpy(name.data(), text, name.size() - 1);
}

/// the FIRE seed's shape: FIREBALL, its two power-ups, gestures 2/1/0
GSpellSeedInfo& MakeFireSeed(InfoConstants& info, size_t index)
{
	auto& seed = info.spellSeed.at(index);
	seed.magicTypes = {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::None};
	seed.powerUpGestures = {GestureType::InverseSpiral, GestureType::Spiral, GestureType::None};
	return seed;
}
} // namespace

TEST(MagicTables, sectionOfEachMagicType)
{
	EXPECT_EQ(SlotOf(MagicType::None).section, MagicInfoSection::General);
	EXPECT_EQ(SlotOf(MagicType::ExplosionOnePuTwo).index, 9);
	EXPECT_EQ(SlotOf(MagicType::HealPowerUpOne).section, MagicInfoSection::Heal);
	EXPECT_EQ(SlotOf(MagicType::HealPowerUpOne).index, 1);
	EXPECT_EQ(SlotOf(MagicType::Teleport).section, MagicInfoSection::Teleport);
	EXPECT_EQ(SlotOf(MagicType::Forest).section, MagicInfoSection::Forest);
	EXPECT_EQ(SlotOf(MagicType::FoodPowerUpOne).section, MagicInfoSection::Food);
	EXPECT_EQ(SlotOf(MagicType::Tornado).section, MagicInfoSection::StormAndTornado);
	EXPECT_EQ(SlotOf(MagicType::Tornado).index, 2);
	EXPECT_EQ(SlotOf(MagicType::PhysicalShield).section, MagicInfoSection::Shield);
	EXPECT_EQ(SlotOf(MagicType::Wood).section, MagicInfoSection::Wood);
	EXPECT_EQ(SlotOf(MagicType::WaterPowerUpOne).section, MagicInfoSection::Water);
	EXPECT_EQ(SlotOf(MagicType::FlockFlying).section, MagicInfoSection::FlockFlying);
	EXPECT_EQ(SlotOf(MagicType::FlockGround).section, MagicInfoSection::FlockGround);
	EXPECT_EQ(SlotOf(MagicType::CreatureSpellFreeze).index, 0);
	EXPECT_EQ(SlotOf(MagicType::CreatureSpellItchy).section, MagicInfoSection::CreatureSpell);
	EXPECT_EQ(SlotOf(MagicType::CreatureSpellItchy).index, 15);
}

TEST(MagicTables, recordsInMagicTypeOrder)
{
	const auto info = Synthetic();
	for (uint32_t t = 0; t < k_MagicTypeCount; ++t)
	{
		EXPECT_EQ(static_cast<uint32_t>(GetMagicInfo(*info, static_cast<MagicType>(t)).magicType), t);
	}
	EXPECT_EQ(GetMagicInfoAs<GMagicHealInfo>(*info, MagicType::Heal), &info->magicHeal[0]);
	EXPECT_EQ(GetMagicInfoAs<GMagicHealInfo>(*info, MagicType::Fireball), nullptr);
	EXPECT_EQ(GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::Wood), &info->magicWood[0]);
	EXPECT_EQ(GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::FoodPowerUpOne), &info->magicFood[1]);
	EXPECT_EQ(GetMagicInfoAs<GMagicRadiusSpellInfo>(*info, MagicType::Shield), &info->magicShield[0]);
	EXPECT_EQ(GetMagicInfoAs<GMagicCreatureSpellInfo>(*info, MagicType::CreatureSpellAngry), &info->magicCreatureSpell[9]);
}

TEST(MagicTables, infoFromText)
{
	auto info = Synthetic();
	SetName(info->magicEffect[16].debugString, "MAGIC_TYPE_STORM_WIND_RAIN");
	EXPECT_EQ(GetInfoFromText(*info, "magic_type_storm_wind_rain"), 16);
	EXPECT_EQ(GetInfoFromText(*info, "MAGIC_TYPE_STORM"), k_MagicTypeNotFound);
}

TEST(MagicTables, maintainedSpells)
{
	EXPECT_TRUE(IsMaintainedSpell(MagicType::Forest));
	EXPECT_TRUE(IsMaintainedSpell(MagicType::Shield));
	EXPECT_TRUE(IsMaintainedSpell(MagicType::PhysicalShield));
	EXPECT_FALSE(IsMaintainedSpell(MagicType::Teleport));
	EXPECT_FALSE(IsMaintainedSpell(MagicType::Tornado));
	EXPECT_FALSE(IsMaintainedSpell(MagicType::Wood));
}

TEST(MagicTables, effectGetters)
{
	auto info = Synthetic();
	auto& storm = info->magicEffect[16];
	storm.timerWhenPlayerCasting = 40.0f;
	storm.costToCreate = 3500.0f;
	storm.agressiveRangeMin = 10.0f;
	storm.agressiveRangeMax = 50.0f;
	info->magicStormAndTornado[0].isCreatureCastFromAbove = 1;
	EXPECT_FLOAT_EQ(GetTimerWhenPlayerCasting(*info, MagicType::StormWindRain), 40.0f);
	EXPECT_FLOAT_EQ(GetChantsRequiredToCreate(*info, MagicType::StormWindRain), 3500.0f);
	EXPECT_TRUE(IsCreatureCastFromAbove(*info, MagicType::StormWindRain));
	EXPECT_FLOAT_EQ(IsInAggressiveRange(*info, MagicType::StormWindRain, 10.0f), 1.0f);
	EXPECT_FLOAT_EQ(IsInAggressiveRange(*info, MagicType::StormWindRain, 50.5f), 0.0f);
}

TEST(MagicTables, tribalPower)
{
	GMagicEffectInfo effect {};
	effect.useTribalPowerMultiplier[0] = 1;
	effect.useTribalPowerMultiplier[2] = 1;
	std::array<float, 9> power {};
	power.fill(1.0f);
	EXPECT_FLOAT_EQ(GetTribalPower(effect, nullptr), 1.0f);
	EXPECT_FLOAT_EQ(GetTribalPower(effect, &power), 1.0f);
	EXPECT_EQ(GetTribalPowerTribe(effect, &power), -1);
	power[1] = 5.0f; // not flagged
	power[2] = 3.0f;
	power[0] = 2.0f;
	EXPECT_FLOAT_EQ(GetTribalPower(effect, &power), 6.0f);
	EXPECT_EQ(GetTribalPowerTribe(effect, &power), 0);
	power[0] = 100.0f;
	EXPECT_FLOAT_EQ(GetTribalPower(effect, &power), 100.0f);
	power[0] = 0.1f;
	EXPECT_FLOAT_EQ(GetTribalPower(effect, &power), 0.5f);
	EXPECT_EQ(GetTribalPowerTribe(effect, &power), 2);
}

TEST(MagicTables, powerUpHelpers)
{
	auto info = Synthetic();
	const auto& seed = MakeFireSeed(*info, 2);
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::Fireball), -1);
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::FireballPowerUpOne), 0);
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::FireballPowerUpTwo), 1);
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::None), 2); // the original's quirk: slot 3 is 0
	EXPECT_EQ(GetPowerUpFromMagicType(seed, MagicType::Heal), -1);
	EXPECT_EQ(GetNumPowerUpLevels(seed), 3);
	EXPECT_EQ(GetMagicTypeFromPULevel(seed, -1), MagicType::Fireball);
	EXPECT_EQ(GetMagicTypeFromPULevel(seed, 1), MagicType::FireballPowerUpTwo);
	EXPECT_EQ(&GetMagicInfoFromPULevel(*info, seed, 0), &info->magicGeneral[2]);
	EXPECT_EQ(&GetMagicInfoFromPULevel(*info, seed, 2), &info->magicGeneral[1]); // no magic: the base one
	int pu = 5;
	EXPECT_EQ(GetPowerUpGesture(seed, MagicType::FireballPowerUpTwo, &pu), GestureType::Spiral);
	EXPECT_EQ(pu, 1);
	EXPECT_EQ(GetPowerUpGesture(seed, MagicType::Fireball, &pu), GestureType::None);
	EXPECT_EQ(pu, -1);
}

TEST(MagicTables, seedLookups)
{
	auto info = Synthetic();
	MakeFireSeed(*info, 2);
	auto& heal = info->spellSeed[7];
	heal.magicTypes = {MagicType::Heal, MagicType::HealPowerUpOne, MagicType::None, MagicType::None};
	heal.exists = 1;
	heal.iconIndex = 6;
	SetName(heal.debugString, "SPELL_SEED_TYPE_HEAL");

	EXPECT_TRUE(SpellSeedIsOfMagicType(info->spellSeed[2], MagicType::FireballPowerUpOne));
	EXPECT_FALSE(SpellSeedIsOfMagicType(info->spellSeed[2], MagicType::Heal));
	EXPECT_EQ(GetFirstSpellSeedForMagicType(*info, MagicType::FireballPowerUpTwo), SpellSeedType::Fire);
	EXPECT_EQ(GetFirstSpellSeedForMagicType(*info, MagicType::HealPowerUpOne), SpellSeedType::Heal);
	EXPECT_EQ(GetFirstSpellSeedForMagicType(*info, MagicType::Tornado), SpellSeedType::None);
	EXPECT_EQ(GetSpellSeedForMagicType(*info, MagicType::Tornado), k_SpellSeedNotFound);
	EXPECT_EQ(GetSpellSeedForMagicType(*info, MagicType::Heal), 7);
	EXPECT_EQ(GetSpellSeedFromText(*info, "spell_seed_type_heal"), 7);
	EXPECT_EQ(GetSpellSeedFromText(*info, "SPELL_SEED_TYPE_FIRE"), k_SpellSeedNotFound);
	EXPECT_EQ(GetSpellSeedFromIconIndex(*info, 6), 7);
	EXPECT_EQ(GetSpellSeedFromIconIndex(*info, 9), -1);
	int pu = 5;
	EXPECT_EQ(GetPowerUpGestureForMagicType(*info, MagicType::FireballPowerUpOne, &pu), GestureType::InverseSpiral);
	EXPECT_EQ(pu, 0);
	EXPECT_EQ(GetPowerUpGestureForMagicType(*info, MagicType::Tornado, &pu), GestureType::None);
	EXPECT_EQ(pu, -1);
	EXPECT_EQ(GetPowerUpLevel(*info, MagicType::Fireball), -1);
	EXPECT_EQ(GetPowerUpLevel(*info, MagicType::FireballPowerUpOne), 0);
	EXPECT_EQ(GetPowerUpLevel(*info, MagicType::HealPowerUpOne), 0);
}

TEST(MagicTables, particleTypeFiles)
{
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::Leaves), "SF_Forest");
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::FoodPoisoned), "SF_Food");
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::Heal), "SF_HealChakra");
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::Bonfire), "SF_Bonfire");
	EXPECT_EQ(psys::ParticleTypeFile(ParticleType::SeeThisBeam), "SF_SeeThisBeam");
	EXPECT_TRUE(psys::ParticleTypeFile(ParticleType::None).empty());
	EXPECT_TRUE(psys::ParticleTypeFile(ParticleType::Tornado).empty());
	EXPECT_TRUE(psys::ParticleTypeFile(static_cast<ParticleType>(200)).empty());
}

/// With OPENBLACK_GAME_PATH set to the install: the real info.dat (core.md section 3)
TEST(MagicTables, realInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	// Scripts/info.dat: a 0x2C-byte pack header, then the InfoConstants block (what InfoFile reads, without the Locator)
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	for (uint32_t t = 0; t < k_MagicTypeCount; ++t)
	{
		EXPECT_EQ(static_cast<uint32_t>(GetMagicInfo(*info, static_cast<MagicType>(t)).magicType), t);
	}
	EXPECT_FLOAT_EQ(GetChantsRequiredToCreate(*info, MagicType::Fireball), 3500.0f);
	EXPECT_FLOAT_EQ(GetChantsRequiredToCreate(*info, MagicType::StormWindRain), 8000.0f);
	EXPECT_FLOAT_EQ(GetTimerWhenPlayerCasting(*info, MagicType::StormWindRain), 40.0f);
	EXPECT_EQ(GetInfoFromText(*info, "storm_pu2"), 18);
	const auto& food = GetSpellSeedInfo(*info, SpellSeedType::Food);
	EXPECT_EQ(food.castType, SpellCastType::SpellCastInHand);
	EXPECT_EQ(food.magicTypes[0], MagicType::Food);
	EXPECT_EQ(food.holdType, HoldType::Side);
	const auto& storm = GetSpellSeedInfo(*info, SpellSeedType::Storm);
	EXPECT_EQ(storm.sizingGesture, GestureType::Circle);
	EXPECT_EQ(storm.holderParticle, ParticleType::LightningStormOnHolder);
	EXPECT_FLOAT_EQ(storm.unknown0x15C, 0.1f);
	EXPECT_EQ(GetPowerUpLevel(*info, MagicType::Tornado), 1);
	EXPECT_EQ(GetSpellSeedFromText(*info, "Heal"), 7);
	EXPECT_EQ(psys::ParticleTypeFile(GetMagicInfo(*info, MagicType::Forest).particleType), "SF_Forest");
	// a villager's starting life (Living::Living 0x5EBEC0: SetLife(GLivingInfo::life))
	for (const auto& villager : info->villager)
	{
		EXPECT_FLOAT_EQ(villager.life, 1.0f) << villager.debugString.data();
	}
}
