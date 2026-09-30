/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Alignment.h"

#include <cmath>

#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "EffectValues.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::effects;

namespace
{
/// GAlignmentInfo row `effect` (0 burn .. 4 fly away; runtime 0xC4CE30, stride 0x48), column `type`
float AlignmentFactor(const InfoConstants& info, size_t effect, AlignmentType type)
{
	if (effect >= info.alignment.size())
	{
		return 0.0f;
	}
	const auto& row = info.alignment[effect];
	switch (type)
	{
	case AlignmentType::AnimalNice:
		return row.animalNice;
	case AlignmentType::AnimalNasty:
		return row.animalNasty;
	case AlignmentType::Creature:
		return row.creature;
	case AlignmentType::Priest:
		return row.priest;
	case AlignmentType::Skeleton:
		return row.skeleton;
	case AlignmentType::Villager:
		return row.villager;
	case AlignmentType::Building:
		return row.building;
	case AlignmentType::Plant:
		return row.plant;
	case AlignmentType::Field:
		return row.field;
	case AlignmentType::Feature:
		return row.feature;
	case AlignmentType::MobileObject:
		return row.mobileObject;
	case AlignmentType::Land:
		return row.land;
	case AlignmentType::Script:
		return row.script;
	case AlignmentType::Unimportant:
		return row.unimportant;
	}
	return 0.0f;
}
} // namespace

float alignment::ScaleChange(const ecs::components::PlayerAlignment& alignment, float change)
{
	const float a = std::abs(alignment.value * 0.5f);
	return change >= 0.0f ? (a + 1.0f) * change : (1.0f - a) * change;
}

void alignment::Update(ecs::components::PlayerAlignment& alignment, entt::entity object, const EffectValues& values,
                       float lifeBefore)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const float lifeChange = lifeBefore - life::LifeOf(object);
	if (lifeChange == 0.0f)
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	const auto* objectInfo = physics::PhysicsObjects::ObjectInfo(object);
	// the info's vt 0x34: GObjectInfo.alignmentType (inf: Unimportant without an info)
	const auto type = objectInfo != nullptr ? objectInfo->alignmentType : AlignmentType::Unimportant;
	const float k = std::abs(lifeChange) + info.player.applyEffectAlignmentChangeAddition;
	for (size_t i = EffectValues::Crush; i <= EffectValues::FlyAway; ++i)
	{
		alignment.pending += ScaleChange(alignment, values.numbers[i] * AlignmentFactor(info, i, type) * k);
	}
	const float burn = ConvertTemperatureToDamage(object, values.numbers[EffectValues::Burn]);
	alignment.pending += ScaleChange(alignment, burn * AlignmentFactor(info, EffectValues::Burn, type) * k);
}
