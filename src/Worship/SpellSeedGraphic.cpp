/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSeedGraphic.h"

#include <string>

#include <spdlog/spdlog.h>

#include "ECS/Components/Alpha.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "PSys/ParticleTypes.h"
#include "PSys/PSysManager.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// CreatePUBand 0x727080: GJUtils::GetSharedMesh(".\data\spells\meshes\Power_Up_Band.L3d") (loaded here on first use:
/// Game.cpp loads only O_Bibble_up of that folder)
constexpr auto k_BandMesh = entt::hashed_string("Power_Up_Band");

float g_Phase = 0.0f;

bool LoadBandMesh()
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return false;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(k_BandMesh))
	{
		return true;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		meshes.Load(k_BandMesh, resources::L3DLoader::FromDiskTag {},
		            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "Spells" / "Meshes" /
		                                "Power_Up_Band.L3d"));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Worship: cannot load Power_Up_Band.L3d: {}", e.what());
		return false;
	}
	return true;
}

void CreateBand(entt::entity graphicEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& graphic = registry.Get<SpellSeedGraphic>(graphicEntity);
	if (graphic.band != entt::null || !LoadBandMesh())
	{
		return;
	}
	const auto& transform = registry.Get<const Transform>(graphicEntity);
	graphic.band = registry.Create();
	registry.Assign<Transform>(graphic.band, transform);
	registry.Assign<Mesh>(graphic.band, k_BandMesh.value(), static_cast<int8_t>(0), static_cast<int8_t>(0));
}
} // namespace

entt::entity seed_graphic::Create(const glm::vec3& worldPosition, SpellSeedType seed, PlayerNames player, float scale,
                                  int powerUp)
{
	const auto index = static_cast<int>(seed);
	if (index < 0 || index >= static_cast<int>(magic::k_SpellSeedCount))
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seed);
	// fn_00726E70 (ctor, not an Object: no creation index), then fn_00727190
	const auto entity = registry.Create();
	auto& graphic = registry.Assign<SpellSeedGraphic>(entity);
	graphic.seedType = seed;
	graphic.player = player;
	graphic.scale = scale;
	graphic.powerUp = powerUp;
	// fn_007270E0: the mesh at the MapCoords + unknown0x150 x scale, the effect at the point + unknown0x154 x scale
	const glm::vec3 meshPosition = worldPosition + glm::vec3(0.0f, info.unknown0x150 * scale, 0.0f);
	const glm::vec3 effectPosition = worldPosition + glm::vec3(0.0f, info.unknown0x154 * scale, 0.0f);
	// Game3DObject::Create(pos, Get3DType(mesh), mesh, 0, 1) with the matrix's scale and no angle; ReplaceMeshGivenSeedType
	// 0x728450 swaps the mesh for some players (not ported)
	registry.Assign<Transform>(entity, meshPosition, glm::mat3(1.0f), glm::vec3(scale));
	registry.Assign<Mesh>(entity, resources::HashIdentifier(info.mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	// fn_007276E0: the holder particle type; PSysInterface::Create(NULL, pt, pos, 0, scale), AddTarget(this), SetPlayer
	if (info.holderParticle != ParticleType::None)
	{
		const auto file = psys::ParticleTypeFile(info.holderParticle);
		if (!file.empty())
		{
			registry.Get<SpellSeedGraphic>(entity).psys = psys::manager::Start(std::string(file), effectPosition, scale);
		}
	}
	if (powerUp != -1)
	{
		CreateBand(entity);
	}
	return entity;
}

void seed_graphic::Delete(entt::entity graphic)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic))
	{
		return;
	}
	if (auto* component = registry.TryGet<SpellSeedGraphic>(graphic); component != nullptr)
	{
		if (component->psys != 0)
		{
			psys::manager::Delete(component->psys);
		}
		if (component->band != entt::null && registry.Valid(component->band))
		{
			registry.Destroy(component->band);
		}
	}
	registry.Destroy(graphic);
	registry.SetDirty();
}

void seed_graphic::SetPowerUpType(entt::entity graphic, int powerUp)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic))
	{
		return;
	}
	registry.Get<SpellSeedGraphic>(graphic).powerUp = powerUp;
	if (powerUp != -1)
	{
		CreateBand(graphic);
	}
}

void seed_graphic::SetAutoUpdate(entt::entity graphic, bool autoUpdate)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic != entt::null && registry.Valid(graphic))
	{
		registry.Get<SpellSeedGraphic>(graphic).autoUpdate = autoUpdate;
	}
}

void seed_graphic::SetAlpha(entt::entity graphic, float alpha)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (graphic == entt::null || !registry.Valid(graphic))
	{
		return;
	}
	registry.Get<SpellSeedGraphic>(graphic).alpha = alpha;
	// (inf) +0x58 is the alpha DrawSpellGraphic 0x519AD0 draws the seed with (research R11)
	if (alpha < 1.0f)
	{
		registry.AssignOrReplace<Alpha>(graphic, alpha);
	}
	else
	{
		registry.Remove<Alpha>(graphic);
	}
	registry.SetDirty();
}

void seed_graphic::ProcessTurn()
{
	// fn_007273A0 steps the holder PSys of auto-updated graphics; psys::manager::ProcessTurn already steps every effect
	// started with manager::Start (holders are not spell-owned). fn_00727440 (every 30 turns) refreshes the player's
	// mesh swap: not ported.
}

void seed_graphic::UpdatePhase(float milliseconds)
{
	g_Phase += milliseconds * 0.001f / 3.33f;
	while (g_Phase > 1.0f)
	{
		g_Phase -= 1.0f;
	}
}

float seed_graphic::Phase()
{
	return g_Phase;
}
