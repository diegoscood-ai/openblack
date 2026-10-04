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

#include "Audio/Services/Guidance.h"
#include "ECS/Abodes.h"
#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "EffectValues.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Camera/Camera.h"
#include "ECS/Influence/Influence.h"
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
	const GObjectInfo* objectInfo = physics::PhysicsObjects::ObjectInfo(object);
	// an abode's (a field's) info is its GAbodeInfo (Abode +0x28, abodes::InfoOf), which PhysicsObjects does not keep
	if (objectInfo == nullptr)
	{
		objectInfo = abodes::InfoOf(object);
	}
	// the info's (+0x28) vt 0x34, GObjectInfo::GetAlignmentType 0x4012A0 (GAlignment::Update 0x41443E..0x414445):
	// alignmentType (inf: Unimportant without an info)
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

void alignment::UpdateForResource(PlayerNames player, entt::entity abode, int32_t amount, float change)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	// abode.GetTown() is not tested (0x414538); there is a single GTownInfo
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

float alignment::DeathAlignmentChange(const GPlayerInfo& info, DeathReason reason, bool child, bool animal)
{
	// 0x4143B9: no player -> nothing (the caller's); 0x4143BB..0x4143C2: fld [p +0x64 (its info) +0x20 + 4r]
	const auto r = std::min<size_t>(static_cast<size_t>(reason), info.dealthReason.size() - 1);
	float change = info.dealthReason.at(r);
	// 0x4143D3..0x4143E3: IsAChild (vt +0x458) -> fadd st0, st0
	if (child)
	{
		change = change + change;
	}
	// 0x4143EB..0x4143FA: IsAnimal (vt +0x454) -> fmul 0.5 (0x8AA3B4)
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
	// 0x414400..0x414403: this +0xC (pending) += the change (fadd, fstp)
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
	// 0x4141AB..0x4141D9: for the player of MyInterfaceStatus (IsMemberOfThisPlayer 0x64D750; (inferido) openblack's
	// local player is PLAYER_ONE, as Game.cpp's localPlayerNumber), every turn and before Process, even with nothing
	// pending (the advisors' running sum decays): GGuidance::HelpSpritesAlignmentProcess 0x71CEB0 with
	// GetMaxAlignmentChangePerGameTurn (vt +0x40) x the pending change +0xC, not clamped yet; the guidance reads the
	// alignment before this turn's change (GetAlignmentValue 0x64D6A0, 0x71CF05) and the same maximum (GPlayer+0x64
	// +0x10, 0x71CEDA)
	if (player == PlayerNames::PLAYER_ONE && magic::players::EntityOf(player) != entt::null)
	{
		audio::guidance::HelpSpritesAlignmentProcess(cap * alignment.pending, alignment.value, cap);
	}
	// GAlignment::Process 0x414140: with nothing pending its CrudeUpdate(0) changes nothing
	if (alignment.pending == 0.0f)
	{
		return;
	}
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

namespace
{
/// [0xBF337C] as its argument: fn_005E2240(x) stores (1 - x) x 2, and the sky starts at 1 (neutral) -> x = 0.5
float g_InterfaceAlignment = 0.5f;
} // namespace

PlayerNames alignment::MostInfluentialPlayer(const glm::vec3& position)
{
	// 0x5CD639: the neutral player first, best 0; GetNextPlayer from the first player on
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
		// 0x5CD678: fcom best; test ah, 0x41; jne -> only a strictly greater influence takes it
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
	// MapCoords::GetAlignment 0x6057B0: 0 to start with (0x6057BF), then for every player GGame::GetNextPlayer hands
	// back (0x6057C7 / 0x605800) add influence x GetAlignmentValue (0x6057DA .. 0x6057F8)
	float sum = 0.0f;
	for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
	{
		const auto player = static_cast<PlayerNames>(i);
		// GGame::GetNextPlayer 0x5508A0 walks only the seven-slot array g_game +0x18 .. +0x48B8 (stride 0xA60): the
		// neutral player (g_game +0x205A5B) is not one of them
		if (player == PlayerNames::NEUTRAL || magic::players::EntityOf(player) == entt::null)
		{
			continue;
		}
		sum += influence::CalculatePlayerInfluence(player, position, influence::CalcType::Default, true) * Get(player);
	}
	// 0x60580F: below -1 -> -1; 0x60582C: above 1 -> 1
	return std::clamp(sum, -1.0f, 1.0f);
}

float alignment::InterfaceAlignmentAt(const glm::vec3& position)
{
	// fn_0064AC30: GetAlignmentValue 0x64D6A0 of that player, + 1 (0x8AA390), x 0.5 (0x8AA3B4); fn_005E2240 clamps it
	const float x = (Get(MostInfluentialPlayer(position)) + 1.0f) * 0.5f;
	return std::clamp(x, 0.0f, 1.0f);
}

float alignment::GetInterfaceAlignment()
{
	return g_InterfaceAlignment;
}

void alignment::UpdateInterfaceAlignment()
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	g_InterfaceAlignment = InterfaceAlignmentAt(Locator::camera::value().GetOrigin());
}

void alignment::ResetInterfaceAlignment()
{
	g_InterfaceAlignment = 0.5f;
}
