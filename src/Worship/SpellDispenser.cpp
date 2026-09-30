/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellDispenser.h"

#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/MagicTables.h"
#include "PSys/PSysManager.h"
#include "PSys/ParticleTypes.h"
#include "Resources/ResourcesInterface.h"
#include "SpecialPoints.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// CallVirtualFunctionsForCreation 0x7227D0: PSysInterface::Create(NULL, 0x90, ...)
constexpr auto k_DispenserParticle = static_cast<ParticleType>(0x90);
/// fn_00722B30: the orb at the dispenser's height x 1.2 (0x8C6C98)
constexpr float k_OrbHeight = 1.2f;
/// GParticleContainer::CreateSpotVisual(pos, 9, 1, 0) for a new orb; 0x1B for a seed given to a dispenser
constexpr int k_OrbSpotVisual = 9;
constexpr int k_SeedSpotVisual = 0x1B;

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

const GAbodeInfo* AbodeInfoOf(entt::entity dispenser)
{
	const auto* abode = Registry().TryGet<const ecs::components::Abode>(dispenser);
	if (abode == nullptr)
	{
		return nullptr;
	}
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber == abode->type)
		{
			return &info;
		}
	}
	return nullptr;
}

/// Object::GetHeight 0x638120: 2 x the mesh's half height x scale
float HeightOf(entt::entity entity)
{
	const auto* mesh = Registry().TryGet<const Mesh>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || !meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Size().y * Registry().Get<const Transform>(entity).scale.y;
}

/// fn_00722B30
glm::vec3 OrbPosition(entt::entity dispenser)
{
	return Registry().Get<const Transform>(dispenser).position + glm::vec3(0.0f, HeightOf(dispenser) * k_OrbHeight, 0.0f);
}

/// Object::IsTouching 0x637E00 (orb, 0.001): (inf) the orb still stands where the dispenser made it
bool OrbStillThere(entt::entity dispenser, entt::entity orb)
{
	auto& registry = Registry();
	if (orb == entt::null || !registry.Valid(orb) || !registry.AllOf<OneOffSpellSeed, Transform>(orb))
	{
		return false;
	}
	const auto& at = registry.Get<const Transform>(orb).position;
	const auto spawn = OrbPosition(dispenser);
	return glm::distance(glm::vec2(at.x, at.z), glm::vec2(spawn.x, spawn.z)) <= 0.001f + 0.5f;
}

/// fn_00723010: no town -> GetPlayer(0)'s town list head; openblack: the player's first town, else AbodeArchetype's
/// nearest town
uint32_t TownIdFor(int townId)
{
	if (townId >= 0)
	{
		return static_cast<uint32_t>(townId);
	}
	uint32_t found = 0xFFFFFFFFu;
	Registry().Each<const Town>([&](const Town& town) {
		if (found == 0xFFFFFFFFu && town.owner == PlayerNames::PLAYER_ONE)
		{
			found = town.id;
		}
	});
	return found;
}
} // namespace

entt::entity dispenser::Create(const glm::vec3& position, AbodeInfo type, int townId, float yAngle, float scale)
{
	auto& registry = Registry();
	const auto entity = ecs::archetypes::AbodeArchetype::Create(TownIdFor(townId), position, type, yAngle, scale, 0, 0);
	if (entity == entt::null)
	{
		return entt::null;
	}
	auto& component = registry.Assign<SpellDispenser>(entity); // fn_007227B0: all 0
	const auto* info = AbodeInfoOf(entity);
	component.period = info != nullptr ? static_cast<uint32_t>(info->timeEachMobileObjectTakesToProduce) : 0;
	component.magicType = MagicType::None;
	if (component.period == 0)
	{
		SetActive(entity, false);
	}
	// CallVirtualFunctionsForCreation 0x7227D0: its effect at the land under it
	const auto file = psys::ParticleTypeFile(k_DispenserParticle);
	if (!file.empty())
	{
		auto ground = registry.Get<const Transform>(entity).position;
		ground.y = GroundAt(ground);
		registry.Get<SpellDispenser>(entity).psys = psys::manager::Start(std::string(file), ground, 1.0f);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship: spell dispenser {} ({}) at ({:.1f}, {:.1f})",
	                   static_cast<uint32_t>(entity), static_cast<int>(type), position.x, position.z);
	return entity;
}

bool dispenser::IsDispenser(entt::entity entity)
{
	return entity != entt::null && Registry().Valid(entity) && Registry().AllOf<SpellDispenser>(entity);
}

void dispenser::SetActive(entt::entity dispenser, bool active)
{
	if (!IsDispenser(dispenser))
	{
		return;
	}
	Registry().Get<SpellDispenser>(dispenser).active = active;
	if (active)
	{
		CreateOneOffSpellSeed(dispenser);
	}
}

void dispenser::SetMagicProperties(entt::entity dispenser, MagicType magic, float seconds)
{
	if (!IsDispenser(dispenser))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SET_MAGIC_PROPERTIES: Thing must be a dispenser");
		return;
	}
	auto& component = Registry().Get<SpellDispenser>(dispenser);
	component.magicType = magic;
	if (seconds > 0.0f)
	{
		constexpr float k_TurnsPerSecond = 1000.0f / static_cast<float>(magic::k_TurnMs);
		component.period = static_cast<uint32_t>(k_TurnsPerSecond * seconds);
	}
	else
	{
		const auto* info = AbodeInfoOf(dispenser);
		component.period = info != nullptr ? static_cast<uint32_t>(info->timeEachMobileObjectTakesToProduce) : 0;
	}
	if (component.period == 0)
	{
		SetActive(dispenser, false);
	}
}

void dispenser::SetTimerTime(entt::entity dispenser, float seconds)
{
	if (!IsDispenser(dispenser))
	{
		return;
	}
	constexpr float k_TurnsPerSecond = 1000.0f / static_cast<float>(magic::k_TurnMs);
	const auto period = static_cast<uint32_t>(k_TurnsPerSecond * seconds);
	if (period > 0)
	{
		Registry().Get<SpellDispenser>(dispenser).period = period;
	}
}

void dispenser::SetMagicAndPeriod(entt::entity dispenser, MagicType magic, uint32_t periodTurns)
{
	if (!IsDispenser(dispenser))
	{
		return;
	}
	Registry().Get<SpellDispenser>(dispenser).magicType = magic;
	SetActive(dispenser, true);
	Registry().Get<SpellDispenser>(dispenser).period = periodTurns;
	if (periodTurns == 0)
	{
		SetActive(dispenser, false);
	}
}

void dispenser::ProcessTurn()
{
	auto& registry = Registry();
	std::vector<entt::entity> dispensers;
	registry.Each<const SpellDispenser>([&](entt::entity entity, const SpellDispenser&) { dispensers.push_back(entity); });
	for (const auto entity : dispensers)
	{
		auto& component = registry.Get<SpellDispenser>(entity);
		if (component.oneShot != entt::null)
		{
			if (OrbStillThere(entity, component.oneShot))
			{
				continue;
			}
			component.oneShot = entt::null;
			component.tick = 0;
			continue;
		}
		// vt 0x40C IsActive, the magic, vt 0x890 IsBuilt and vt 0x88C IsRepaired (openblack's abodes are both)
		if (!component.active || component.magicType == MagicType::None)
		{
			continue;
		}
		if (++component.tick >= component.period)
		{
			CreateOneOffSpellSeed(entity);
		}
	}
}

entt::entity dispenser::CreateOneOffSpellSeed(entt::entity dispenser)
{
	auto& registry = Registry();
	auto& component = registry.Get<SpellDispenser>(dispenser);
	const auto& tables = Locator::infoConstants::value();
	// fn_00722B20 the magic's GMagicInfo, fn_005FB400 its seed, fn_0072B010 the level
	const auto seed = magic::GetFirstSpellSeedForMagicType(tables, component.magicType);
	if (static_cast<int>(seed) < 0)
	{
		return entt::null;
	}
	int powerUp = -1;
	(void)magic::GetPowerUpGesture(magic::GetSpellSeedInfo(tables, seed), component.magicType, &powerUp);
	const auto position = OrbPosition(dispenser);
	const auto orb = magic::one_off::Create(position, seed, powerUp, 1.0f);
	registry.Get<SpellDispenser>(dispenser).oneShot = orb;
	if (orb == entt::null)
	{
		return entt::null;
	}
	psys::manager::CreateSpotVisual(k_OrbSpotVisual, position, 0.0f, entt::null);
	registry.Get<SpellDispenser>(dispenser).tick = 0;
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Worship trace: dispenser {} makes orb {} (seed {}, pu {})",
		                   static_cast<uint32_t>(dispenser), static_cast<uint32_t>(orb), static_cast<int>(seed), powerUp);
	}
	return orb;
}

bool dispenser::ApplySeed(entt::entity dispenser, entt::entity seedEntity)
{
	auto& registry = Registry();
	if (!IsDispenser(dispenser) || !registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity))
	{
		return false;
	}
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	// fn_00728C50: only a seed that has not cast (+0x98 == 0)
	if (seed.lastMagic != MagicType::None)
	{
		return false;
	}
	const auto position = OrbPosition(dispenser);
	magic::one_off::Create(position, seed.seedType, seed.powerUp, 1.0f);
	magic::seed::SetChantStore(seed, 0.0f);
	magic::seed::ToBeDeleted(seedEntity);
	psys::manager::CreateSpotVisual(k_SeedSpotVisual, position, 0.0f, entt::null);
	return true;
}
