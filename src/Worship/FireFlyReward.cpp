/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireFlyReward.h"

#include <array>
#include <cmath>

#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Components/Transform.h"
#include "ECS/FireFlies.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::worship;

namespace
{
constexpr size_t k_Magic = 42;
std::array<float, k_Magic> g_Probabilities {}; ///< 0xCCFBAC
std::array<float, k_Magic> g_Sums {};          ///< 0xCCFB04..0xCCFBA8
} // namespace

void fire_fly::SetRewardProbability(MagicType magic, float probability)
{
	const auto index = static_cast<size_t>(magic);
	if (static_cast<int>(magic) < 0 || index >= k_Magic)
	{
		return;
	}
	g_Probabilities.at(index) = probability;
	float sum = 0.0f;
	for (size_t i = 0; i < k_Magic; ++i)
	{
		sum += g_Probabilities.at(i);
		g_Sums.at(i) = sum;
	}
}

void fire_fly::OnPlacedInMagicHand(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object) || !registry.AllOf<ecs::components::Transform>(object))
	{
		return;
	}
	const auto position = registry.Get<const ecs::components::Transform>(object).position;
	if (ecs::TakeFireFlyAt(position))
	{
		Reward(position);
	}
}

entt::entity fire_fly::Reward(const glm::vec3& position)
{
	// fn_0052B6F0: GRand::GameFloatRand 0x6DE530 of the sum
	const float r = game_random::GameFloatRand(Total());
	if (r == 0.0f)
	{
		return entt::null;
	}
	size_t magic = k_Magic;
	for (size_t i = 0; i < k_Magic; ++i)
	{
		if (r <= g_Sums.at(i))
		{
			magic = i;
			break;
		}
	}
	if (magic == k_Magic || magic == 0)
	{
		return entt::null;
	}
	const auto& tables = Locator::infoConstants::value();
	const auto type = static_cast<MagicType>(magic);
	// fn_005FB400 the magic's seed info, fn_0072B200 its index, GetPowerUpFromMagicType the level
	const auto seed = magic::GetFirstSpellSeedForMagicType(tables, type);
	if (static_cast<int>(seed) < 0)
	{
		return entt::null;
	}
	const auto& seedInfo = magic::GetSpellSeedInfo(tables, seed);
	if (seedInfo.exists == 0)
	{
		return entt::null;
	}
	const auto orb = magic::one_off::Create(position, seed, magic::GetPowerUpFromMagicType(seedInfo, type), 1.0f);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: a firefly leaves a one-shot {} at ({:.1f}, {:.1f})",
	                   seedInfo.debugString.data(), position.x, position.z);
	return orb;
}

void fire_fly::Reset()
{
	// FireFly::OnClearMap 0x52A1E0 clears the probabilities only: the running sums (what fn_0052B6F0 reads) keep the
	// last land's until the next FIRE_FLY_SPELL_REWARD_PROB (kept)
	g_Probabilities.fill(0.0f);
}

float fire_fly::Total()
{
	return g_Sums.back();
}
