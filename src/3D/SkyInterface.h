/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack
{

namespace graphics
{
class L3DMesh;
class Texture2D;
} // namespace graphics

class SkyInterface
{
public:
	/// Deprecated in favour of sky_type::Frame() (3D/SkyType.h): openblack's old convention, 0 night .. 2 day,
	/// = 2 - sky_type::Frame(). Kept only for Renderer::UpdateLandLight -> LandLightTable::Build until that takes the
	/// original's sky type.
	[[nodiscard]] virtual float GetCurrentSkyType() const noexcept = 0;
	/// fn_0086A330's dome part, once a frame after sky_type::SampleFrame: sky_type::Dome().Advance and the rows it asks
	/// for blended into the dome textures (fn_0086B7F0)
	virtual void UpdateDome() noexcept = 0;
	[[nodiscard]] virtual graphics::L3DMesh& GetMesh() const noexcept = 0;
	/// The three dome textures (sky_type::DomeBlend), one layer per alignment: 0 evil, 1 neutral, 2 good
	[[nodiscard]] virtual graphics::Texture2D& GetTexture() const noexcept = 0;
	virtual void SetTime(float time) noexcept = 0;
	/// Game hour 0..24
	[[nodiscard]] virtual float GetTime() const noexcept = 0;
	/// sun.l3d (a 9928 x 9928 quad using a quarter of Data\Textures\sun.raw)
	[[nodiscard]] virtual graphics::L3DMesh& GetSunMesh() const noexcept = 0;
	/// moon.l3d (a hemisphere of radius ~104 textured from Data\Textures\weather.raw)
	[[nodiscard]] virtual graphics::L3DMesh& GetMoonMesh() const noexcept = 0;
	/// Data\Landscape\mist.l3d, the sky clouds' mesh
	[[nodiscard]] virtual graphics::L3DMesh& GetCloudMesh() const noexcept = 0;
};

} // namespace openblack
