/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/core/fwd.hpp>

namespace openblack::ecs::components
{

/// The sky's dome: a mesh about the world painted with the dome's pictures, blended for the time of day. The pictures
/// are the sky system's own (SkySystemInterface::GetTexture).
struct SkyDome
{
	entt::id_type meshId;
};

/// A body of the sky drawn as a mesh: its mesh, texture and the texture its alpha is taken from
struct CelestialBody
{
	entt::id_type meshId;
	entt::id_type textureId;
	entt::id_type alphaTextureId;
};

/// A body's glow, a square about it drawn from part of a texture
struct CelestialGlow
{
	entt::id_type textureId;
	entt::id_type alphaTextureId;
};

/// The sun. Where it stands is worked out from the hour as it is drawn.
struct Sun
{
	/// A script has hidden it (SET_SUN_DRAW): neither the sun nor its glare is drawn, and the glare's easing waits, until
	/// a script shows it again, the scripts restart or a frame is drawn inside the temple
	bool hiddenByScript {false};
};

/// The moon as it last showed, kept from land to land (SkySystemInterface::KeepForNextLand)
struct Moon
{
	/// The phase of the real moon as it last showed, 0 to 2 pi: a full turn at a new moon and half a turn at a full
	/// moon. It is taken from the date only while the moon shows, and stays as it was otherwise; 0 until it first shows.
	float phase {0.0f};
	/// The computer's date as last read for the phase, in seconds since 1970; 0 until it is first read
	int64_t date {0};
	/// The machine's clock, in milliseconds, when the date was last read: it is read again only after two seconds
	uint32_t dateReadAt {0};
	/// A date to take the phase from instead of the computer's, for trying the phases from the debug tools; none unless
	/// one is set there
	std::optional<int64_t> dateOverride;
};

} // namespace openblack::ecs::components
