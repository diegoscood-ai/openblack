/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Sky.h"

#include <cassert>

#include <algorithm>
#include <span>

#include <bgfx/bgfx.h>
#include <glm/vec3.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Common/Bitmap16B.h"
#include "Common/StringUtils.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/GraphicsHandleBgfx.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"

using namespace openblack::filesystem;
using namespace openblack::graphics;

namespace openblack
{

Sky::Sky() noexcept
{
	auto& fileSystem = Locator::filesystem::value();

	// load in the mesh
	_mesh = std::make_unique<graphics::L3DMesh>("Sky");
	_mesh->LoadFromFilesystem(fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "sky.l3d");
	_sunMesh = std::make_unique<graphics::L3DMesh>("Sun");
	_sunMesh->LoadFromFilesystem(fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "sun.l3d");
	_moonMesh = std::make_unique<graphics::L3DMesh>("Moon");
	_moonMesh->LoadFromFilesystem(fileSystem.GetPath<filesystem::Path::WeatherSystem>() / "moon.l3d");
	_cloudMesh = std::make_unique<graphics::L3DMesh>("Mist");
	_cloudMesh->LoadFromFilesystem(fileSystem.GetPath<filesystem::Path::Landscape>() / "mist.l3d");

	// TODO (#749) Maybe use std::views::enumerate
	for (uint32_t idx = 0; const auto& alignment : k_Alignments)
	{
		for (const auto& timeView : k_Times)
		{
			auto time = std::string(timeView);
			auto prefix = std::string("sky");
			if (idx >= k_Times.size() && idx < 2 * k_Times.size())
			{
				time = string_utils::Capitalise(time);
				prefix = string_utils::Capitalise(prefix);
			}
			const auto filename = fmt::format("{}_{}_{}.555", prefix, alignment, time);
			const auto path = fileSystem.GetPath<filesystem::Path::WeatherSystem>() / filename;
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading sky texture: {}", path.generic_string());

			Bitmap16B* bitmap = Bitmap16B::LoadFromFile(path);
			assert(bitmap->Size() == k_LayerTexels * sizeof(uint16_t));
			memcpy(&_bitmaps.at(idx * k_LayerTexels), bitmap->Data(), bitmap->Size());
			delete bitmap;
			++idx;
		}
	}

	_timeOfDay = 1.0f;

	// fn_0086A3B0 0x86A530..0x86A564: three dynamic 256 x 256 textures (flags 0x104, fn_008379E0), one per alignment,
	// built whole at once (0x86A589) and then followed by fn_0086A330 / fn_0086A270 (sky_type::DomeBlend). The set-up
	// builds them with Time2SkyType of its hour on its own thresholds 4.5 / 7 / 7.5 / 8.25; here with the dome's current
	// sky type (inferido: in both games the jumps of GLandAlignement::Open rebuild them right after)
	_texture = std::make_unique<Texture2D>("Sky");
	_texture->Create(k_Size, k_Size, static_cast<uint16_t>(k_Alignments.size()), TextureFormat::BGR5A1,
	                 Wrapping::ClampEdge, Filter::Linear, nullptr);
	BlendDome({sky_type::Dome().Built(), 0, sky_type::DomeBlend::k_Rows});
}

Sky::~Sky() noexcept = default;

void Sky::SetTime(float time) noexcept
{
	assert(time <= 24.0f);
	_timeOfDay = time;
}

float Sky::GetCurrentSkyType() const noexcept
{
	// Deprecated forwarder in openblack's old convention (0 night .. 2 day) for the callers not moved to sky_type yet
	return 2.0f - sky_type::Frame();
}

void Sky::UpdateDome() noexcept
{
	const auto blocks = sky_type::Dome().Advance(sky_type::Frame());
	for (int i = 0; i < blocks.count; ++i)
	{
		BlendDome(blocks.blocks.at(i));
	}
}

void Sky::BlendDome(const sky_type::DomeBlock& block) noexcept
{
	// fn_0086B7F0 0x86B890..0x86B98B with fn_00869670 true ([0xC38200] = 1, [0xEDD470] = 0 in the file)
	const auto weight = sky_type::DomeWeightOf(block.skyType);
	const int lastRow = std::min(block.firstRow + block.rowCount, static_cast<int>(k_Size));
	if (lastRow <= block.firstRow)
	{
		return;
	}
	const auto first = static_cast<size_t>(block.firstRow) * k_Size;
	const auto count = static_cast<size_t>(lastRow - block.firstRow) * k_Size;
	for (size_t a = 0; a < k_Alignments.size(); ++a)
	{
		// the original's time of day 0 _day, 1 _dusk, 2 _night is k_Times index 2 - tod
		const auto lower = (a * k_Times.size() + (2 - weight.lower)) * k_LayerTexels;
		const auto upper = (a * k_Times.size() + (2 - weight.upper)) * k_LayerTexels;
		sky_type::BlendRows555(std::span(_dome).subspan(a * k_LayerTexels + first, count),
		                       std::span<const uint16_t>(_bitmaps).subspan(lower + first, count),
		                       std::span<const uint16_t>(_bitmaps).subspan(upper + first, count), weight.weight);
	}
	// 0x86B95B..0x86B977: the texture (format 4 of flags 0x104) gets its +0x138 flag only once first + rows reaches the
	// height; unlock fn_00838EB0 hands only formats 1, 2 and 0x20 to the surface itself. That the flag is what sends the
	// texels to the card is (inferido), so the GPU sees the dome change all at once when the last block is done.
	if (lastRow < static_cast<int>(k_Size))
	{
		return;
	}
	for (size_t a = 0; a < k_Alignments.size(); ++a)
	{
		bgfx::updateTexture2D(toBgfx(_texture->GetNativeHandle()), static_cast<uint16_t>(a), 0, 0, 0, k_Size, k_Size,
		                      bgfx::copy(&_dome.at(a * k_LayerTexels),
		                                 static_cast<uint32_t>(k_LayerTexels * sizeof(uint16_t))));
	}
}

} // namespace openblack
