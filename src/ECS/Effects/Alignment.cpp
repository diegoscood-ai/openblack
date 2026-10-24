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

#include <algorithm>

#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "Audio/Services/Guidance.h"
#include "Camera/Camera.h"
#include "Creature/CreatureDeedAlignment.h"
#include "Debug/TraceLog.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Villager.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "EffectValues.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"

using namespace openblack;
using namespace openblack::ecs::effects;

float alignment::AlignmentFactor(const InfoConstants& info, size_t effect, AlignmentType type)
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

namespace
{
/// The number of the kinds of thing in the alignment table
constexpr uint32_t k_AlignmentKinds = 14;

/// What the creature does on this turn, as the plan that it does tells
creature_deed_alignment::Deed DeedOf(const ecs::Registry& registry, entt::entity creature, const InfoConstants& info)
{
	creature_deed_alignment::Deed deed;
	const auto* mind = registry.TryGet<const ecs::components::CreatureMindState>(creature);
	if (mind == nullptr || !mind->planActive || !mind->planner.current.has_value())
	{
		return deed;
	}
	const auto& plan = *mind->planner.current;
	const auto desire = static_cast<size_t>(plan.desire);
	if (desire < info.creatureInitialDesire.size())
	{
		deed.desireChange = info.creatureInitialDesire.at(desire).alignmentChange;
	}
	// A plan to eat a villager uses the weight of the kind of the villager in the first row of the alignment table
	if (plan.desire != creature_desires::Desire::Hunger || !plan.object.has_value())
	{
		return deed;
	}
	const auto food = static_cast<entt::entity>(*plan.object);
	if (!registry.Valid(food) || !registry.AllOf<ecs::components::Villager>(food) || !Locator::dynamicsSystem::has_value())
	{
		return deed;
	}
	const auto* row = Locator::dynamicsSystem::value().ObjectInfo(food);
	if (row != nullptr && static_cast<uint32_t>(row->alignmentType) < k_AlignmentKinds && !info.alignment.empty())
	{
		deed.eatenVillagerWeight = alignment::AlignmentFactor(info, 0, row->alignmentType);
	}
	return deed;
}

bool Trace()
{
	return debug_env::TraceLogs(std::getenv("OPENBLACK_ALIGNMENT_TRACE") != nullptr, "game");
}
} // namespace

float alignment::ScaleChange(const ecs::components::Alignment& alignment, float change)
{
	// the same sign as the alignment (0 counts as positive) is damped, the opposite sign boosted
	const float a = std::abs(alignment.value * 0.5f);
	const bool sameSign = (alignment.value < 0.0f) == (change < 0.0f);
	return sameSign ? (1.0f - a) * change : (a + 1.0f) * change;
}

float alignment::StepPending(float value, float& pending, float changePerTurn)
{
	const float result = std::clamp(value + (changePerTurn * std::clamp(pending, -1.0f, 1.0f)), -1.0f, 1.0f);
	pending = 0.0f;
	return result;
}

alignment::EffectTarget alignment::TargetOf(entt::entity object, const EffectValues& values)
{
	constexpr auto k_Burn = static_cast<size_t>(EffectValues::Number::Burn);
	EffectTarget target {.life = life::LifeOf(object),
	                     .burnDamage = ConvertTemperatureToDamage(object, values.numbers[k_Burn])};
	if (!Locator::dynamicsSystem::has_value())
	{
		return target;
	}
	const GObjectInfo* objectInfo = Locator::dynamicsSystem::value().ObjectInfo(object);
	// an abode's (a field's) info is its GAbodeInfo (abodes::InfoOf), which DynamicsSystem does not keep
	if (objectInfo == nullptr)
	{
		objectInfo = abodes::InfoOf(object);
	}
	// a feature's is its feature row, and a patch of flowers' its flowers row
	auto& registry = Locator::entitiesRegistry::value();
	if (objectInfo == nullptr && Locator::infoConstants::has_value())
	{
		const auto& constants = Locator::infoConstants::value();
		if (const auto* feature = registry.TryGet<const ecs::components::Feature>(object))
		{
			objectInfo = &constants.feature.at(static_cast<size_t>(feature->type));
		}
		else if (const auto* flowers = registry.TryGet<const ecs::components::Flowers>(object))
		{
			objectInfo = &constants.flowers.at(static_cast<size_t>(flowers->type));
		}
	}
	// the info's alignmentType ((inferred) Unimportant without an info)
	if (objectInfo != nullptr)
	{
		target.type = objectInfo->alignmentType;
	}
	return target;
}

void alignment::Update(ecs::components::Alignment& alignment, entt::entity object, const EffectValues& values, float lifeBefore)
{
	if (!Locator::infoConstants::has_value() || lifeBefore - life::LifeOf(object) == 0.0f)
	{
		return;
	}
	Update(alignment, TargetOf(object, values), values, lifeBefore);
}

void alignment::Update(ecs::components::Alignment& alignment, const EffectTarget& target, const EffectValues& values,
                       float lifeBefore)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const float lifeChange = lifeBefore - target.life;
	if (lifeChange == 0.0f)
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	const float k = std::abs(lifeChange) + info.player.applyEffectAlignmentChangeAddition;
	constexpr auto k_First = static_cast<size_t>(EffectValues::Number::Crush);
	constexpr auto k_Last = static_cast<size_t>(EffectValues::Number::FlyAway);
	for (size_t i = k_First; i <= k_Last; ++i)
	{
		alignment.pending += ScaleChange(alignment, values.numbers[i] * AlignmentFactor(info, i, target.type) * k);
	}
	constexpr auto k_Burn = static_cast<size_t>(EffectValues::Number::Burn);
	alignment.pending += ScaleChange(alignment, target.burnDamage * AlignmentFactor(info, k_Burn, target.type) * k);
}

ecs::components::Alignment& alignment::Of(PlayerNames player)
{
	return magic::players::AlignmentOf(player);
}

float alignment::Get(PlayerNames player)
{
	return Of(player).value;
}

void alignment::SetClamped(PlayerNames player, float value)
{
	Of(player).value = std::clamp(value, -1.0f, 1.0f);
}

void alignment::AddClamped(PlayerNames player, float change)
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

void alignment::UpdateForResource(PlayerNames player, entt::entity abode, int32_t amount, float change)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	// the abode's town is not tested; there is a single town info
	static_cast<void>(abode);
	const auto& town = Locator::infoConstants::value().town;
	const float k = amount > 0 ? town.giveResourceAligmnetChangeMultiplier : town.takeResourceAligmnetChangeMultiplier;
	auto& alignment = Of(player);
	const float weighed = ScaleChange(alignment, change * k);
	alignment.pending += weighed;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} resource {:+d} {:+.5f} (pending {:+.5f})",
		                   static_cast<int>(player), amount, weighed, alignment.pending);
	}
}

void alignment::UpdateForImpression(PlayerNames player, float change)
{
	auto& alignment = Of(player);
	const float weighed = ScaleChange(alignment, change);
	alignment.pending += weighed;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} impressed {:+.5f} (pending {:+.5f})",
		                   static_cast<int>(player), weighed, alignment.pending);
	}
}

void alignment::UpdateForDisciple(PlayerNames player, float change)
{
	auto& alignment = Of(player);
	const float weighed = ScaleChange(alignment, change);
	alignment.pending += weighed;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} disciple {:+.5f} (pending {:+.5f})",
		                   static_cast<int>(player), weighed, alignment.pending);
	}
}

float alignment::DeathAlignmentChange(const GPlayerInfo& info, DeathReason reason, bool child, bool animal)
{
	// no player -> nothing (the caller's); the player info's dealthReason[r]
	const auto r = std::min<size_t>(static_cast<size_t>(reason), info.dealthReason.size() - 1);
	float change = info.dealthReason.at(r);
	// a child: doubled (v + v)
	if (child)
	{
		change = change + change;
	}
	// an animal: halved
	if (animal)
	{
		change *= 0.5f;
	}
	return change;
}

void alignment::UpdateForDeath(PlayerNames owner, DeathReason reason, bool child, bool animal)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	// pending += the change
	const float change = DeathAlignmentChange(Locator::infoConstants::value().player, reason, child, animal);
	auto& alignment = Of(owner);
	alignment.pending += change;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Alignment: player {} death {} {:+.5f} (pending {:+.5f}, alignment {:+.4f})",
		                   static_cast<int>(owner), static_cast<int>(reason), change, alignment.pending, alignment.value);
	}
}

void alignment::ProcessForPlayer(PlayerNames player)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	auto& alignment = Of(player);
	const float cap = Locator::infoConstants::value().player.maxAlignmentChangePerGameTurn;
	// for the local interface's player ((inferred) openblack's local player is PLAYER_ONE, as Game.cpp's
	// localPlayerNumber), every turn and before the fold, even with nothing pending (the advisors' running sum decays):
	// the guidance's alignment process with the maximum change per game turn x the pending change, not clamped yet;
	// the guidance reads the alignment before this turn's change and the same maximum
	if (player == PlayerNames::PLAYER_ONE && magic::players::EntityOf(player) != entt::null)
	{
		audio::guidance::UpdateAlignmentRemarks(cap * alignment.pending, alignment.value, cap);
	}
	// with nothing pending its AddClamped(0) changes nothing
	if (alignment.pending == 0.0f)
	{
		return;
	}
	const float change = cap * std::clamp(alignment.pending, -1.0f, 1.0f);
	AddClamped(player, change);
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

void alignment::ProcessCreatures()
{
	if (!Locator::infoConstants::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<ecs::components::Creature>([&info, &registry](entt::entity entity, ecs::components::Creature& creature) {
		const auto row = openblack::creature::InfoRow(creature.species);
		if (row >= info.creature.size() || row >= info.differentCreature.size())
		{
			return;
		}
		// The species scale is the balance row's: the game's load copies it over the creature row's own, which this
		// reads once that copy is ported
		const float scale = info.differentCreature.at(row).alignmentChangeScale;
		// The plan that the creature does adds its change before the turn moves the alignment
		creature.pendingAlignment +=
		    creature_deed_alignment::TurnChange(DeedOf(registry, entity, info), scale, creature.alignment);
		creature.alignment =
		    StepPending(creature.alignment, creature.pendingAlignment, info.creature.at(row).alignmentChangePerTurn);
	});
}

PlayerNames alignment::MostInfluentialPlayer(const glm::vec3& position)
{
	// the neutral player first, best 0; then every player from the first one on
	PlayerNames best = PlayerNames::NEUTRAL;
	float bestInfluence = 0.0f;
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		const auto player = static_cast<PlayerNames>(i);
		if (magic::players::EntityOf(player) == entt::null)
		{
			continue;
		}
		const float influence = influence::CalculatePlayerInfluence(player, position, influence::CalcType::Default, true);
		// only a strictly greater influence takes it
		if (influence > bestInfluence)
		{
			bestInfluence = influence;
			best = player;
		}
	}
	return best;
}

float alignment::LandAlignmentAt(const glm::vec3& position)
{
	// 0 to start with, then for every player add influence x the player's alignment value
	float sum = 0.0f;
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		const auto player = static_cast<PlayerNames>(i);
		// the player walk covers only the seven player slots: the neutral player is not one of them
		if (player == PlayerNames::NEUTRAL || magic::players::EntityOf(player) == entt::null)
		{
			continue;
		}
		sum += influence::CalculatePlayerInfluence(player, position, influence::CalcType::Default, true) * Get(player);
	}
	// below -1 -> -1; above 1 -> 1
	return std::clamp(sum, -1.0f, 1.0f);
}

float alignment::InterfaceAlignmentAt(const glm::vec3& position)
{
	// that player's alignment value, + 1, x 0.5; the sky clamps it
	const float x = (Get(MostInfluentialPlayer(position)) + 1.0f) * 0.5f;
	return std::clamp(x, 0.0f, 1.0f);
}

float alignment::GetInterfaceAlignment()
{
	return magic::players::LocalInterfaceAlignment().value;
}

void alignment::UpdateInterfaceAlignment()
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	magic::players::LocalInterfaceAlignment().value = InterfaceAlignmentAt(Locator::camera::value().GetOrigin());
}

void alignment::ResetInterfaceAlignment()
{
	magic::players::LocalInterfaceAlignment().value = 0.5f;
}
