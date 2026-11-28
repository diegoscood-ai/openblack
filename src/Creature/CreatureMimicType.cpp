/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMimicType.h"

using namespace openblack;
using creature_mimic_type::MimicType;

MimicType creature_mimic_type::Of(const Facts& facts)
{
	if (facts.creature)
	{
		return MimicType::Creature;
	}
	if (facts.villager)
	{
		if (facts.child)
		{
			return MimicType::Child;
		}
		return facts.female ? MimicType::FemaleVillager : MimicType::MaleVillager;
	}
	if (facts.animal)
	{
		return facts.nastyAnimal ? MimicType::NastyAnimal : MimicType::NiceAnimal;
	}
	if (facts.rock)
	{
		return MimicType::Rock;
	}
	if (facts.tree)
	{
		return MimicType::Tree;
	}
	if (facts.abode)
	{
		return MimicType::Abode;
	}
	return facts.mobile ? MimicType::MobileObject : MimicType::Other;
}
