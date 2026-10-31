/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MiracleDeeds.h"

#include "Creature/CreatureDeeds.h"

using namespace openblack;

namespace
{
// The deeds are the rows of our table of what creatures copy
constexpr uint32_t Row(creature_watching::Deed deed)
{
	return static_cast<uint32_t>(deed);
}
static_assert(magic::k_DeedCastFoodInWorshipSite == Row(creature_watching::Deed::CastMagicFoodInWorshipSite));
static_assert(magic::k_DeedCastFoodInStoragePit == Row(creature_watching::Deed::CastMagicFoodInStoragePit));
static_assert(magic::k_DeedCastWoodInStoragePit == Row(creature_watching::Deed::CastMagicWoodInStoragePit));
static_assert(magic::k_DeedCastWoodByBuildingSite == Row(creature_watching::Deed::CastMagicWoodByBuildingSite));
static_assert(magic::k_DeedDamageWithFire == Row(creature_watching::Deed::DamageWithFire));
static_assert(magic::k_DeedDamageWithMagic == Row(creature_watching::Deed::DamageWithMagic));
static_assert(magic::k_DeedImpressWithMagic == Row(creature_watching::Deed::ImpressWithMagic));
static_assert(magic::k_DeedCastWaterOnCrops == Row(creature_watching::Deed::CastWaterOnCrops));
static_assert(magic::k_DeedCastWaterToPutOutFire == Row(creature_watching::Deed::CastWaterToPutOutFire));
static_assert(magic::k_DeedHeal == Row(creature_watching::Deed::Heal));
static_assert(magic::k_NoDeed == creature_watching::k_DeedCount);
} // namespace

uint32_t magic::MiracleDeed(MagicType type, bool meantToAnger, bool anotherPlayersTownNear, const DeedTarget& target)
{
	if (!meantToAnger && anotherPlayersTownNear)
	{
		return k_DeedImpressWithMagic;
	}
	switch (type)
	{
	case MagicType::Fireball:
	case MagicType::FireballPowerUpOne:
	case MagicType::FireballPowerUpTwo:
		return k_DeedDamageWithFire;
	case MagicType::LightningBolt:
	case MagicType::LightningBoltPowerUpOne:
	case MagicType::LightningBoltPowerUpTwo:
	case MagicType::ExplosionOne:
	case MagicType::ExplosionOnePuOne:
		return k_DeedDamageWithMagic;
	case MagicType::Heal:
	case MagicType::HealPowerUpOne:
		return k_DeedHeal;
	case MagicType::Food:
	case MagicType::FoodPowerUpOne:
		if (target.worshipSite)
		{
			return k_DeedCastFoodInWorshipSite;
		}
		if (target.storagePit)
		{
			return k_DeedCastFoodInStoragePit;
		}
		break;
	case MagicType::Wood:
		if (target.storagePit)
		{
			return k_DeedCastWoodInStoragePit;
		}
		if (target.buildingBeingBuilt)
		{
			return k_DeedCastWoodByBuildingSite;
		}
		break;
	case MagicType::Water:
	case MagicType::WaterPowerUpOne:
		if (target.field)
		{
			return k_DeedCastWaterOnCrops;
		}
		return target.onFire ? k_DeedCastWaterToPutOutFire : k_DeedCastWaterOnCrops;
	case MagicType::FlockFlying:
	case MagicType::FlockGround:
		return k_DeedImpressWithMagic;
	default:
		break;
	}
	return k_NoDeed;
}
