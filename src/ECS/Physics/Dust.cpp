/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Dust.h"

#include <algorithm>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Lh3dColour.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

namespace
{
constexpr size_t k_MaxPuffs = 1024;
constexpr float k_Life = 1.0f;

struct Puff
{
	entt::entity entity;
	glm::vec3 velocity;
	float size;
	float age;
	uint32_t seed; ///< rand % 16 of the cell
};
std::vector<Puff> g_Puffs;

/// The shape of the puffs: blobsa.raw, the alpha of data\blobs.raw (white RGB).
std::optional<graphics::TextureHandle> Texture()
{
	auto& textures = Locator::resources::value().GetTextures();
	const auto id = entt::hashed_string("raw/blobsa").value();
	if (!textures.Contains(id))
	{
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			textures.Load(id, resources::Texture2DLoader::FromDiskTag {}, fileSystem.FindPath(std::filesystem::path("Data") / "blobsa.raw"));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "Dust: cannot load Data/blobsa.raw: {}", e.what());
			return std::nullopt;
		}
	}
	return textures.Handle(id)->GetNativeHandle();
}

glm::vec2 CellUv(uint32_t cell)
{
	return graphics::frame_anim::SpriteCellUv(static_cast<int>(cell), 8)[0];
}
} // namespace

namespace
{
/// (r - 100) x [0x8CF178] (0.02): fild of the int, fmul
float Axis(uint32_t r)
{
	const auto centred = static_cast<int32_t>(r) - 100;
	return static_cast<float>(centred) * 0.02f;
}
} // namespace

glm::vec3 Dust::RandomVelocity()
{
	// 0x6467D1, 0x6467ED, 0x646809: z ([ebp-0x38]), y ([ebp-0x3C]), x ([ebp-0x40]) in that order
	const float z = Axis(game_random::LocalRand(201));
	const float y = Axis(game_random::LocalRand(201));
	const float x = Axis(game_random::LocalRand(201));
	return {x, y, z};
}

glm::vec3 Dust::SyncedRandomVelocity()
{
	// 0x76EEC5, 0x76EEEE, 0x76EF1A (ViscousLiquid.cpp line 0x27D): z (stored 0x76EF4A), y (0x76EF35), x (0x76EF53)
	const float z = Axis(game_random::GameRand(201));
	const float y = Axis(game_random::GameRand(201));
	const float x = Axis(game_random::GameRand(201));
	return {x, y, z};
}

void Dust::Emit(glm::vec3 at, glm::vec3 velocity, uint32_t argb, float size)
{
	if (g_Puffs.size() >= k_MaxPuffs)
	{
		return;
	}
	// fn_00845FA0 0x845FDE: a kind other than 0 (all of openblack's are kind 4) takes CRT rand() % 16 (signed; rand is
	// never negative), after the 0x400 test of fn_00845D30 (0x845D42)
	const auto seed = static_cast<uint32_t>(game_random::crt::Rand() % 16);
	const auto texture = Texture();
	if (!texture)
	{
		return;
	}
	const glm::vec4 colour = lh3d_colour::ToVec4(argb);
	const float a = colour.a;
	const glm::vec3 rgb(colour);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// normal blending with the tint premultiplied by its alpha
	registry.Assign<Sprite>(entity, *texture, CellUv(16 + seed), glm::vec2(1.0f / 8.0f), glm::vec4(rgb * a, a), false);
	registry.Assign<Transform>(entity, at, glm::mat3(1.0f), glm::vec3(0.0f));
	g_Puffs.push_back({entity, velocity, size, 0.0f, seed});
	registry.SetDirty();
}

void Dust::Update(float seconds)
{
	if (g_Puffs.empty() || seconds <= 0.0f)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& puff : g_Puffs)
	{
		puff.age += seconds;
		// fn_00846010: age += dt, gone once past the kind's life (kind 4: 1 s; 0: 3 s; others 2 s), before moving
		if (puff.age > k_Life || !registry.Valid(puff.entity))
		{
			if (registry.Valid(puff.entity))
			{
				registry.Destroy(puff.entity);
			}
			puff.entity = entt::null;
			continue;
		}
		auto& transform = registry.Get<Transform>(puff.entity);
		transform.position += puff.velocity * seconds;
		// half size = size x (1 - age) x min(1, age / 0.125)
		const float half = puff.size * (1.0f - puff.age) * std::min(1.0f, puff.age / 0.125f);
		transform.scale = glm::vec3(half);
		// cell 16 + ((rand % 16 + (int)(2 age)) & 15) (frame_anim::DustCell)
		registry.Get<Sprite>(puff.entity).uvMin = CellUv(graphics::frame_anim::DustCell(puff.seed, puff.age));
	}
	std::erase_if(g_Puffs, [](const Puff& p) { return p.entity == entt::null; });
	registry.SetDirty();
}

void Dust::Clear()
{
	g_Puffs.clear();
}
