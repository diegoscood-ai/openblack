/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ParticleDrawFrame.h"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <string>

#include "Graphics/ArgbColour.h"

using namespace openblack;
using namespace openblack::particles::draw;
using openblack::graphics::render_modes::Mode;

namespace
{
/// The corners a surface's 16-bit indices reach
constexpr size_t k_SixteenBitCorners = 0x10000;
} // namespace

Material particles::draw::SpriteMaterial(std::pair<entt::id_type, entt::id_type> textures, bool additive, bool writeDepth)
{
	// Two-sided with the depth test on: the additive mode or not, each with or without depth writes
	const auto mode = graphics::render_modes::ModeFromProperties(Mode::AlphaTexturedAlphaNoZWrite,
	                                                             {.additive = additive, .zWrite = writeDepth, .alpha = true});
	return {.texture = textures.first, .alphaTexture = textures.second, .mode = mode};
}

void Frame::Clear()
{
	sorted = {};
	queued.clear();
	hand.clear();
	surfaces.clear();
	groundSurfaces.clear();
	sortedPieces.Clear();
	orderedPieces.Clear();
	lightSheets.clear();
}

void particles::draw::AddLightSheet(Frame& frame, std::span<const LightSheet::Vertex> vertices,
                                    std::span<const uint32_t> triangles, const glm::vec3& sortPoint)
{
	if (vertices.empty() || triangles.empty() || vertices.size() > k_SixteenBitCorners)
	{
		return;
	}
	// The sheet's material: the stars with their alpha, alpha blended onto what is behind (mode 13), no depth write,
	// two-sided; the texture wraps as every texture here does
	psys::surf_revol::Surface surface {
	    .texture = std::string(k_LightSheetTexture),
	    .additive = true,
	    .writeDepth = false,
	    .doubleSided = true,
	    .useTextureAlpha = true,
	    .specularInPass = true,
	    .origin = sortPoint,
	    .path = psys::DrawPath::Sorted,
	};
	surface.vertices.reserve(vertices.size());
	for (const auto& vertex : vertices)
	{
		// bgfx takes the colours red first; the specular keeps the surfaces' convention of the diffuse alpha, which the
		// one-pass draw does not read
		surface.vertices.push_back({.position = vertex.position,
		                            .uv = vertex.uv,
		                            .abgr = argb_colour::ToAbgr(vertex.argb),
		                            .specular = argb_colour::ToAbgr(vertex.specularArgb, argb_colour::Alpha(vertex.argb))});
	}
	surface.indices.reserve(triangles.size());
	std::ranges::transform(triangles, std::back_inserter(surface.indices),
	                       [](uint32_t index) { return static_cast<uint16_t>(index); });
	frame.lightSheets.push_back(std::move(surface));
}

void particles::draw::AddLightSheets(Frame& frame, std::span<const std::shared_ptr<LightSheet>> oldestFirst)
{
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	for (const auto& sheet : oldestFirst | std::views::reverse)
	{
		if (sheet == nullptr)
		{
			continue;
		}
		// its place is read before the build moves its middle
		const auto sortPoint = sheet->Middle();
		sheet->Build(vertices, triangles);
		AddLightSheet(frame, vertices, triangles, sortPoint);
	}
}
