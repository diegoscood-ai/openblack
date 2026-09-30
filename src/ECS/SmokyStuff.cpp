/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SmokyStuff.h"

#include <algorithm>
#include <optional>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
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
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t k_MaxSprites = 512;
/// SmokyStuff type 0: life 1 falling by dt / 3 (three seconds)
constexpr float k_Life = 3.0f;
/// frames 0..15: LH3DSprite::SetToZero leaves 8 cells per row (+0x30) and SmokyStuff::Create 0x823C90 does not change
/// it, so they are rows 0-1 of smoke.raw's 8 x 8 sheet (LH3DSprite::Draw 0x840530: u = (c & 7) / 8, v = (c >> 3) / 8)
constexpr uint32_t k_Frames = 16;

struct Sprite2
{
	entt::entity entity;
	glm::vec3 velocity;
	float size;
	float life;
};
std::vector<Sprite2> g_Sprites;

/// the alpha of Data\Textures\smoke.raw (as the night lights' glow)
std::optional<graphics::TextureHandle> Texture()
{
	auto& textures = Locator::resources::value().GetTextures();
	const auto id = entt::hashed_string("raw/smokea").value();
	if (!textures.Contains(id))
	{
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			textures.Load(id, resources::Texture2DLoader::FromDiskTag {},
			              fileSystem.FindPath(std::filesystem::path("Data") / "Textures" / "smokea.raw"));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "SmokyStuff: cannot load Data/Textures/smokea.raw: {}", e.what());
			return std::nullopt;
		}
	}
	return textures.Handle(id)->GetNativeHandle();
}

glm::vec2 FrameUv(uint32_t frame)
{
	const uint32_t f = std::min(frame, k_Frames - 1);
	return glm::vec2(static_cast<float>(f & 7), static_cast<float>(f >> 3)) * 0.125f;
}

/// grey 0x808080 with alpha = trunc(life x 100) of 255
glm::vec4 Colour(float life)
{
	const float a = std::clamp(std::trunc(life * 100.0f) / 255.0f, 0.0f, 1.0f);
	const glm::vec3 rgb(128.0f / 255.0f);
	return glm::vec4(rgb * a, a);
}
} // namespace

void SmokyStuff::Create(glm::vec3 at, float size)
{
	const auto texture = Texture();
	if (!texture || size <= 0.0f)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& rng = Locator::rng::value();
	const auto random = [&rng, size]() { return rng.NextValue(-size, size); };
	for (int i = 0; i < 15 && g_Sprites.size() < k_MaxSprites; ++i)
	{
		const auto entity = registry.Create();
		const glm::vec3 offset(random(), random(), random());
		glm::vec3 direction(random(), random(), random());
		direction = glm::length(direction) > 0.0f ? glm::normalize(direction) : glm::vec3(0.0f, 1.0f, 0.0f);
		const glm::vec3 velocity = direction * rng.NextValue(0.3f, 1.0f) * size;
		registry.Assign<Sprite>(entity, *texture, FrameUv(k_Frames - 1), glm::vec2(0.125f), Colour(1.0f), false);
		registry.Assign<Transform>(entity, at + offset, glm::mat3(1.0f), glm::vec3(size * 0.5f));
		g_Sprites.push_back({entity, velocity, size, 1.0f});
	}
	registry.SetDirty();
}

void SmokyStuff::Update(float seconds)
{
	if (g_Sprites.empty() || seconds <= 0.0f)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& sprite : g_Sprites)
	{
		sprite.life -= seconds / k_Life;
		if (sprite.life <= 0.0f || !registry.Valid(sprite.entity))
		{
			if (registry.Valid(sprite.entity))
			{
				registry.Destroy(sprite.entity);
			}
			sprite.entity = entt::null;
			continue;
		}
		auto& transform = registry.Get<Transform>(sprite.entity);
		transform.position += sprite.velocity * seconds;
		// 0.5 x size at the start, 1.5 x size at the end
		transform.scale = glm::vec3(std::max(0.0001f, (1.0f + 2.0f * (1.0f - sprite.life)) * sprite.size * 0.5f));
		auto& drawn = registry.Get<Sprite>(sprite.entity);
		drawn.uvMin = FrameUv(static_cast<uint32_t>(sprite.life * static_cast<float>(k_Frames) * 0.9375f));
		drawn.tint = Colour(sprite.life);
	}
	std::erase_if(g_Sprites, [](const Sprite2& s) { return s.entity == entt::null; });
	registry.SetDirty();
}

void SmokyStuff::Clear()
{
	g_Sprites.clear();
}
