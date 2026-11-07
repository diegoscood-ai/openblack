/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>

namespace openblack::ecs::archetypes
{

/// The sky: its dome, the sun, and the moon with its glow
class SkyArchetype
{
public:
	/// The dome's mesh, in the mesh cache
	static constexpr entt::hashed_string k_DomeMeshId = entt::hashed_string("sky/sky.l3d");
	/// The sun: a square far out in the sky, and its texture
	static constexpr entt::hashed_string k_SunMeshId = entt::hashed_string("sky/sun.l3d");
	static constexpr entt::hashed_string k_SunTextureId = entt::hashed_string("raw/sun");
	/// The moon: a half sphere beside the camera, its texture and that texture's alpha
	static constexpr entt::hashed_string k_MoonMeshId = entt::hashed_string("sky/moon.l3d");
	static constexpr entt::hashed_string k_MoonTextureId = entt::hashed_string("raw/weather");
	static constexpr entt::hashed_string k_MoonAlphaTextureId = entt::hashed_string("raw/weathera");
	/// The moon's glow, from part of the atmosphere texture
	static constexpr entt::hashed_string k_GlowTextureId = entt::hashed_string("raw/ATMOS");
	static constexpr entt::hashed_string k_GlowAlphaTextureId = entt::hashed_string("raw/ATMOSA");

	struct Entities
	{
		entt::entity dome {entt::null};
		entt::entity sun {entt::null};
		entt::entity moon {entt::null};
	};

	/// The dome, the sun and the moon, in that order, with their components as the game starts
	static Entities Create();
	SkyArchetype() = delete;
};

} // namespace openblack::ecs::archetypes
