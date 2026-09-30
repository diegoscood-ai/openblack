/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Alignment.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "EffectValues.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

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
bool Trace()
{
	return std::getenv("OPENBLACK_ALIGNMENT_TRACE") != nullptr;
}
} // namespace

float alignment::ScaleChange(const ecs::components::PlayerAlignment& alignment, float change)
{
	// the same sign as the alignment (0 counts as positive) is damped, the opposite sign boosted
	const float a = std::abs(alignment.value * 0.5f);
	const bool sameSign = (alignment.value < 0.0f) == (change < 0.0f);
	return sameSign ? (1.0f - a) * change : (a + 1.0f) * change;
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

ecs::components::PlayerAlignment& alignment::Of(PlayerNames player)
{
	return magic::players::AlignmentOf(player);
}

float alignment::Get(PlayerNames player)
{
	return Of(player).value;
}

void alignment::CrudeSet(PlayerNames player, float value)
{
	Of(player).value = std::clamp(value, -1.0f, 1.0f);
}

void alignment::CrudeUpdate(PlayerNames player, float change)
{
	auto& alignment = Of(player);
	alignment.value = std::clamp(alignment.value + change, -1.0f, 1.0f);
}

void alignment::UpdateForTree(PlayerNames player, bool good)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const float change = Locator::infoConstants::value().player.treePullPutAlignmentChange;
	auto& alignment = Of(player);
	const float weighed = ScaleChange(alignment, good ? change : -change);
	alignment.pending += weighed;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} tree {} {:+.5f} (pending {:+.5f}, alignment {:+.4f})",
		                   static_cast<int>(player), good ? "planted" : "uprooted", weighed, alignment.pending,
		                   alignment.value);
	}
}

void alignment::ProcessForPlayer(PlayerNames player)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	auto& alignment = Of(player);
	if (alignment.pending == 0.0f)
	{
		return;
	}
	// TODO: GGuidance::HelpSpritesAlignmentProcess for the local player (the good and evil advisors)
	const float cap = Locator::infoConstants::value().player.maxAlignmentChangePerGameTurn;
	const float change = cap * std::clamp(alignment.pending, -1.0f, 1.0f);
	CrudeUpdate(player, change);
	alignment.pending = 0.0f;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} {:+.5f} -> {:+.4f}", static_cast<int>(player), change,
		                   alignment.value);
	}
}

void alignment::ProcessPlayers()
{
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		ProcessForPlayer(static_cast<PlayerNames>(i));
	}
}
