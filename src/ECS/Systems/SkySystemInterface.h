/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "3D/DayNightClock.h"

namespace openblack::graphics
{
class L3DMesh;
class Texture2D;
} // namespace openblack::graphics

namespace openblack::ecs::systems
{

/// The world's sky (Locator::skySystem): the dome (components::SkyDome), the sun (components::Sun) and the moon
/// (components::Moon) as entities; the dome's mesh and its three blended textures, the sun's, the moon's and the
/// clouds' meshes, the hour they are drawn at, and the clock of day and night the sky follows
class SkySystemInterface
{
public:
	virtual ~SkySystemInterface() = default;

	/// Makes the sky's entities, with the sky kept from the last land when there is one
	virtual void Initialize() = 0;
	/// Keeps the sky as it is while the last land's entities go, for Initialize to make it again
	virtual void KeepForNextLand() = 0;

	/// One turn of game time
	virtual void ProcessTurn() = 0;
	/// Once a frame, before it is drawn: the sky type of the visual time is sampled (sky_type::SampleFrame), and while
	/// the sky is drawn the dome moves on towards it (sky_type::Dome().Advance) and the rows it asks for are blended
	/// into the dome textures
	virtual void UpdateFrame(bool skyDrawn) = 0;

	/// Keeps the hour the sun and the moon are placed at, between 0 and 24
	virtual void SetTime(float time) = 0;
	/// The hour last given to SetTime, between 0 and 24
	[[nodiscard]] virtual float GetTime() const = 0;
	/// The hours of the visual time the sky turns at
	[[nodiscard]] virtual DayNightTimes GetDayNightTimes() const = 0;
	/// The clock of day and night the sky follows
	[[nodiscard]] virtual DayNightClock& GetClock() = 0;
	[[nodiscard]] virtual const DayNightClock& GetClock() const = 0;

	[[nodiscard]] virtual graphics::L3DMesh& GetMesh() const noexcept = 0;
	/// The three dome textures (sky_type::DomeBlend), one layer per alignment: 0 evil, 1 neutral, 2 good
	[[nodiscard]] virtual graphics::Texture2D& GetTexture() const noexcept = 0;
	/// sun.l3d (a 9928 x 9928 quad using a quarter of Data\Textures\sun.raw)
	[[nodiscard]] virtual graphics::L3DMesh& GetSunMesh() const noexcept = 0;
	/// moon.l3d (a hemisphere of radius ~104 textured from Data\Textures\weather.raw)
	[[nodiscard]] virtual graphics::L3DMesh& GetMoonMesh() const noexcept = 0;
	/// Data\Landscape\mist.l3d, the sky clouds' mesh
	[[nodiscard]] virtual graphics::L3DMesh& GetCloudMesh() const noexcept = 0;

	/// The sky's entities: the dome, the sun and the moon, which keeps the phase it last showed with
	[[nodiscard]] virtual entt::entity GetDome() const = 0;
	[[nodiscard]] virtual entt::entity GetSun() const = 0;
	[[nodiscard]] virtual entt::entity GetMoon() const = 0;
};

} // namespace openblack::ecs::systems
