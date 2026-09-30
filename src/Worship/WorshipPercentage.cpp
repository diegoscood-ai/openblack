/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipPercentage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerWorship.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "TownMagic.h"
#include "WorshipSite.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

TownMagic* MagicOf(entt::entity town)
{
	if (town == entt::null || !Registry().Valid(town))
	{
		return nullptr;
	}
	return Registry().TryGet<TownMagic>(town);
}

/// Town::GetTotemStatue 0x73E1D0: the statue of the town's centre
entt::entity TotemOf(entt::entity town)
{
	const auto centre = town::TownCentreOf(town);
	if (centre == entt::null)
	{
		return entt::null;
	}
	entt::entity found = entt::null;
	Registry().Each<const TotemStatue>([&](entt::entity entity, const TotemStatue& statue) {
		if (statue.townCentre == centre)
		{
			found = entity;
		}
	});
	return found;
}

/// TotemStatue::SetWorshipPercentage 0x738270: a change of more than 0.001 ms moves the Zoomer in |change| x 5200 ms,
/// else it jumps
void SetTotemPercentage(entt::entity totem, float percentage)
{
	auto& registry = Registry();
	auto& worship = registry.TryGet<TotemWorship>(totem) != nullptr ? registry.Get<TotemWorship>(totem)
	                                                                 : registry.Assign<TotemWorship>(totem);
	const float milliseconds = std::abs(worship.percentage - percentage) * 5200.0f;
	worship.percentage = percentage;
	if (milliseconds < 0.001f)
	{
		worship.rise.SetPosition(percentage);
	}
	else
	{
		worship.rise.SetDestinationWithSpeedAndTime(percentage, 0.0f, milliseconds * 0.001f);
	}
}

std::vector<entt::entity> VillagersOf(entt::entity town)
{
	std::vector<entt::entity> villagers;
	Registry().Each<const Villager>([&](entt::entity entity, const Villager& villager) {
		if (villager.town == town)
		{
			villagers.push_back(entity);
		}
	});
	return villagers;
}
} // namespace

void percentage::SetWorshipPercentage(entt::entity town, float percentage)
{
	auto* magic = MagicOf(town);
	if (magic == nullptr)
	{
		return;
	}
	if (magic->worshipSite == entt::null)
	{
		magic->worshipPercentage = 0.0f;
		return;
	}
	magic->worshipPercentage = percentage;
	if (const auto totem = TotemOf(town); totem != entt::null)
	{
		SetTotemPercentage(totem, percentage);
	}
	const int need = GetWorshipersNeeded(town, true, true, nullptr);
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: town {} worship {:.2f}, {} villagers needed",
		                   Registry().Get<const Town>(town).id, percentage, need);
	}
	if (need != 0)
	{
		AdjustWorshipersWorshipping(town, need, true, false);
	}
}

float percentage::GetWorshipPercentage(entt::entity town)
{
	const auto* magic = MagicOf(town);
	return magic != nullptr ? magic->worshipPercentage : 0.0f;
}

int percentage::GetWorshipersNeeded(entt::entity town, bool countOnWay, bool countGoHome, bool* out)
{
	const auto* magic = MagicOf(town);
	if (magic == nullptr)
	{
		return 0;
	}
	const auto population = static_cast<float>(town::Population(town));
	const int current = magic->worshipping + (countOnWay ? magic->onWayToWorship : 0);
	int requests = 0;
	if (countGoHome && magic->worshipSite != entt::null && Registry().Valid(magic->worshipSite))
	{
		requests = site::VillagersRequestingToGoHome(Registry().Get<const WorshipSite>(magic->worshipSite));
	}
	int target = 0;
	if (magic->worshipPercentage > 0.0f)
	{
		target = std::max(1, static_cast<int>(population * magic->worshipPercentage + 0.5f));
	}
	const int result = target - current + requests;
	if (out != nullptr)
	{
		*out = result > 0 && current >= target;
	}
	return result;
}

void percentage::AdjustWorshipersWorshipping(entt::entity town, int count, bool skipLifeCheck, bool requireReachable)
{
	const auto& registry = Registry();
	for (int pass = 0; pass < 2 && count != 0; ++pass)
	{
		// the villagers of the town's abodes (+0x754), then the homeless (+0x768): openblack keeps them by town
		const auto villagers = VillagersOf(town);
		if (count > 0)
		{
			std::vector<std::pair<float, entt::entity>> candidates;
			for (const auto villager : villagers)
			{
				if (ecs::villager_worship::IsAvailableForWorshipSite(villager, pass != 0))
				{
					candidates.emplace_back(WorshipScore(villager), villager);
				}
			}
			if (trace::Enabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: pass {}, {} of {} villagers available", pass,
				                   candidates.size(), villagers.size());
			}
			// the highest score first (a new one goes before the first whose score is lower)
			std::stable_sort(candidates.begin(), candidates.end(),
			                 [](const auto& a, const auto& b) { return a.first > b.first; });
			const float threshold = Locator::infoConstants::value().villager.at(0).damageThresholdToGoHome;
			for (const auto& [distance, villager] : candidates)
			{
				if (count == 0)
				{
					break;
				}
				if (!registry.Valid(villager))
				{
					continue;
				}
				if (!skipLifeCheck && !(ecs::life::LifeOf(villager) > threshold))
				{
					continue;
				}
				if (ecs::villager_worship::CheckWorshipActivity(villager, requireReachable))
				{
					--count;
				}
			}
		}
		else
		{
			std::vector<std::pair<float, entt::entity>> candidates;
			for (const auto villager : villagers)
			{
				if (ecs::villager_worship::IsAtOrOnTheWayToWorshipSite(villager))
				{
					candidates.emplace_back(WorshipScore(villager), villager);
				}
			}
			// the lowest score first (a new one goes before the first whose score is higher)
			std::stable_sort(candidates.begin(), candidates.end(),
			                 [](const auto& a, const auto& b) { return a.first < b.first; });
			for (const auto& [distance, villager] : candidates)
			{
				if (count == 0)
				{
					break;
				}
				// vt 0x8E8 SetState(163), then vt 0x8C8 (1 when it left)
				ecs::villager_worship::SendBackToTown(villager);
				++count;
			}
		}
	}
}

void percentage::AddVillagerOnWay(entt::entity town, entt::entity villager)
{
	auto* magic = MagicOf(town);
	if (magic == nullptr || std::ranges::find(magic->onWayVillagers, villager) != magic->onWayVillagers.end())
	{
		return;
	}
	magic->onWayVillagers.insert(magic->onWayVillagers.begin(), villager);
	++magic->onWayToWorship;
}

void percentage::RemoveVillagerOnWay(entt::entity town, entt::entity villager)
{
	auto* magic = MagicOf(town);
	if (magic == nullptr)
	{
		return;
	}
	auto& list = magic->onWayVillagers;
	const auto it = std::ranges::find(list, villager);
	if (it == list.end())
	{
		return;
	}
	list.erase(it);
	--magic->onWayToWorship;
}

void percentage::AddWorshipper(entt::entity town)
{
	if (auto* magic = MagicOf(town); magic != nullptr)
	{
		++magic->worshipping;
	}
}

void percentage::RemoveWorshipper(entt::entity town)
{
	if (auto* magic = MagicOf(town); magic != nullptr)
	{
		--magic->worshipping;
	}
}

float percentage::WorshipScore(entt::entity villager)
{
	const auto& registry = Registry();
	const auto* component = registry.TryGet<const Villager>(villager);
	if (component == nullptr)
	{
		return 0.0f;
	}
	const auto* magic = MagicOf(component->town);
	if (magic == nullptr || magic->worshipSite == entt::null || !registry.Valid(magic->worshipSite))
	{
		return 0.0f;
	}
	// WorshipSite::CalculateCentrePos 0x77DD40
	const auto& site = registry.Get<const Transform>(magic->worshipSite);
	const auto centre = site.position + site.rotation * glm::vec3(12.55f, 0.0f, -26.1f);
	const auto flat = [](const glm::vec3& p) { return glm::vec2(p.x, p.z); };
	const float toVillager = glm::distance(flat(registry.Get<const Transform>(villager).position), flat(centre));
	const float toTown = glm::distance(flat(centre), flat(registry.Get<const Transform>(component->town).position)) + 100.0f;
	// GUtils::GetDistanceModifier 0x74F290: SigmoidThreshold(1 - min(a, b) / b, 0.5)
	const float x = 1.0f - std::min(toVillager, toTown) / toTown;
	const float life = ecs::life::LifeOf(villager);
	return SigmoidThreshold(x, 0.5f) * life * life;
}

float percentage::SigmoidThreshold(float x, float threshold)
{
	// GUtils::SigmoidThreshold 0x74F170 and its table 0xC23284 (41 steps of a logistic curve)
	static constexpr std::array<float, 41> k_Table = {
	    0.0f,     0.0f,     0.0f,     0.0f,     0.0f,     0.0f,     0.0f,     0.0f,     0.0f,     1e-05f,   4e-05f,
	    0.0001f,  0.00028f, 0.00078f, 0.00215f, 0.00597f, 0.01642f, 0.04439f, 0.11444f, 0.26442f, 0.5f,     0.73558f,
	    0.88556f, 0.95561f, 0.98358f, 0.99403f, 0.99785f, 0.99922f, 0.99972f, 0.9999f,  0.99996f, 0.99999f, 1.0f,
	    1.0f,     1.0f,     1.0f,     1.0f,     1.0f,     1.0f,     1.0f,     1.0f};
	if (x == 1.0f)
	{
		return 0.0f;
	}
	const float v = std::clamp(std::clamp(threshold, -1.0f, 1.0f) - x, -1.0f, 1.0f);
	auto index = static_cast<uint32_t>(static_cast<int>((v + 1.0f) * 20.5f));
	if (index > 40)
	{
		index = 40;
	}
	return k_Table.at(index);
}

void percentage::UpdateTotems(float seconds)
{
	auto& registry = Registry();
	bool changed = false;
	registry.Each<TotemStatue, TotemWorship, Transform>(
	    [&](entt::entity, TotemStatue& statue, TotemWorship& worship, Transform& plinth) {
		    worship.rise.Update(seconds);
		    const float rise = TotemStatue::k_WorshipRise * worship.rise.value;
		    if (rise == statue.rise)
		    {
			    return;
		    }
		    statue.rise = rise;
		    plinth.position.y = statue.baseY + rise;
		    if (statue.top != entt::null && registry.Valid(statue.top))
		    {
			    registry.Get<Transform>(statue.top).position.y = plinth.position.y + TotemStatue::k_PlinthTop;
		    }
		    changed = true;
	    });
	if (changed)
	{
		registry.SetDirty();
	}
}

entt::entity percentage::TotemTown(entt::entity statue)
{
	auto& registry = Registry();
	if (statue == entt::null || !registry.Valid(statue))
	{
		return entt::null;
	}
	const auto* totem = registry.TryGet<const TotemStatue>(statue);
	if (totem == nullptr)
	{
		// the icon on the plinth drags the same statue
		registry.Each<const TotemStatue>([&](entt::entity entity, const TotemStatue& s) {
			if (s.top == statue)
			{
				totem = &s;
				statue = entity;
			}
		});
	}
	if (totem == nullptr || totem->townCentre == entt::null)
	{
		return entt::null;
	}
	entt::entity found = entt::null;
	registry.Each<const TownMagic>([&](entt::entity town, const TownMagic& magic) {
		if (found == entt::null && town::TownCentreOf(town) == totem->townCentre && magic.worshipSite != entt::null)
		{
			found = town;
		}
	});
	return found;
}
