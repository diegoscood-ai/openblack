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

#include "Common/RandomNumberManager.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "FileSystem/FileSystemInterface.h"
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
	return glm::vec2(static_cast<float>(cell % 8), static_cast<float>(cell / 8)) / 8.0f;
}
} // namespace

glm::vec3 Dust::RandomVelocity()
{
	auto& rng = Locator::rng::value();
	const auto r = [&rng]() { return static_cast<float>(rng.NextValue(0, 200) - 100) * 0.02f; };
	return {r(), r(), r()};
}

void Dust::Emit(glm::vec3 at, glm::vec3 velocity, uint32_t argb, float size)
{
	if (g_Puffs.size() >= k_MaxPuffs)
	{
		return;
	}
	const auto texture = Texture();
	if (!texture)
	{
		return;
	}
	const float a = static_cast<float>((argb >> 24) & 0xFF) / 255.0f;
	const glm::vec3 rgb(static_cast<float>((argb >> 16) & 0xFF) / 255.0f, static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
	                    static_cast<float>(argb & 0xFF) / 255.0f);
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	const auto seed = static_cast<uint32_t>(Locator::rng::value().NextValue(0, 15));
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
		// cell 16 + ((rand % 16 + (int)(2 age)) & 15)
		registry.Get<Sprite>(puff.entity).uvMin = CellUv(16 + ((puff.seed + static_cast<uint32_t>(2.0f * puff.age)) & 15u));
	}
	std::erase_if(g_Puffs, [](const Puff& p) { return p.entity == entt::null; });
	registry.SetDirty();
}

void Dust::Clear()
{
	g_Puffs.clear();
}
