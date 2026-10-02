/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Haze.h"

#include <cmath>

#include <algorithm>

#include <LNDFile.h>

#include "3D/LandBlock.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Locator.h"

namespace openblack::graphics
{

haze::Params haze::FromTable(const LandLightTable::Haze& haze, bool on) noexcept
{
	Params params;
	params.on = on;
	// fn_007FEAA0 0x7FEAB0..0x7FEABB
	params.nearDistance = haze.nearDistance;
	params.farDistance = haze.farDistance;
	params.range = haze.farDistance - haze.nearDistance;
	// fn_007FEAD0 0x7FEAE0..0x7FEAF5: k is an int argument (fn_00869850 passes its integer k)
	params.k = static_cast<int>(haze.k);
	params.colour = haze.colour;
	// 0x7FEAFA..0x7FEB26: __ftol of each float, b in byte 0, g in byte 1, r in byte 2, alpha 0
	params.packed = static_cast<uint32_t>(static_cast<int32_t>(haze.colour.b)) & 0xFFu;
	params.packed |= (static_cast<uint32_t>(static_cast<int32_t>(haze.colour.g)) & 0xFFu) << 8;
	params.packed |= (static_cast<uint32_t>(static_cast<int32_t>(haze.colour.r)) & 0xFFu) << 16;
	return params;
}

haze::Params haze::Frame() noexcept
{
	const bool on = Locator::config::has_value() && GetDetailLevel(Locator::config::value().detailLevel).fog;
	return FromTable(LandLightTable::Current().GetHaze(), on);
}

float haze::Depth(const glm::mat4& view, const glm::vec3& point) noexcept
{
	return (view * glm::vec4(point, 1.0f)).z;
}

int haze::RoundHalfEven(float value) noexcept
{
	// std::nearbyint rounds in the current mode, to nearest with halves to even by default: fistp's
	return static_cast<int>(std::nearbyint(value));
}

float haze::T(const Params& params, float depth) noexcept
{
	const float z = std::min(std::max(depth, params.nearDistance), params.farDistance);
	return (z - params.nearDistance) / params.range;
}

int haze::Factor(const Params& params, float t) noexcept
{
	return 256 - static_cast<int>(static_cast<float>(256 - params.k) * t);
}

uint32_t haze::ScaleDiffuse(uint32_t argb, int f) noexcept
{
	if (static_cast<uint32_t>(f) >= 256u)
	{
		return argb;
	}
	const auto factor = static_cast<uint32_t>(f);
	uint32_t out = argb & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		out |= ((((argb >> shift) & 0xFFu) * factor) >> 8u) << shift;
	}
	return out;
}

uint32_t haze::Colour(const Params& params, float t) noexcept
{
	const auto channel = [t](float c) {
		return static_cast<uint32_t>(RoundHalfEven(c * t)) & 0xFFu;
	};
	return channel(params.colour.r) << 16 | channel(params.colour.g) << 8 | channel(params.colour.b);
}

uint32_t haze::AddSaturated(uint32_t a, uint32_t b) noexcept
{
	uint32_t out = 0;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		out |= std::min(((a >> shift) & 0xFFu) + ((b >> shift) & 0xFFu), 0xFFu) << shift;
	}
	return out;
}

uint32_t haze::ApplyObject(const Params& params, float depth, uint32_t specular, uint32_t* diffuse) noexcept
{
	// 0x7FEB36..0x7FEB49: the "Fog" key off -> the specular as it came
	if (!params.on)
	{
		return specular;
	}
	// 0x7FEB7D..0x7FEB94: fcomp, test ah, 1: strictly closer than near -> untouched
	if (depth < params.nearDistance)
	{
		return specular;
	}
	const float t = T(params, depth);
	if (diffuse != nullptr)
	{
		*diffuse = ScaleDiffuse(*diffuse, Factor(params, t));
	}
	// 0x7FEC80..0x7FEC99: or 0xFFFFFF00 then the shifts: alpha 0xFF
	const uint32_t fog = 0xFF000000u | Colour(params, t);
	// 0x7FEC9B: a zero specular takes the haze colour as it is
	if (specular == 0)
	{
		return fog;
	}
	// 0x7FECA2..0x7FED0D: saturated per channel, alpha 0xFF
	return 0xFF000000u | AddSaturated(specular, fog);
}

int haze::BlockClass(const Params& params, const std::array<float, 8>& cornerDepths) noexcept
{
	uint32_t bits = 0;
	for (const float z : cornerDepths)
	{
		// 0x87743D..0x87745F: fcom near, test ah, 0x41 (z > near), then fcomp far (z > far)
		if (z > params.nearDistance)
		{
			bits |= z > params.farDistance ? 2u : 1u;
		}
	}
	// 0x87746D..0x87749B
	if (!params.on || bits == 0)
	{
		return 0;
	}
	return (bits & 1u) != 0 ? 1 : 2;
}

std::array<glm::vec3, 8> haze::BlockCorners(glm::vec2 mapPosition, float highestAltitude, bool landRef) noexcept
{
	constexpr float k_Half = 80.0f; // [0x8D060C]
	// 0x877232..0x877292: the centre +0x90C / +0x910 + 80; y: h = +0x924 x 0.67 ([0xC3720C]), centre h / 2 and half h / 2
	// (0x877272..0x877282), or centre 0 and half h with LandRef (0x87724E..0x87726A)
	const float height = highestAltitude * 0.67f;
	const float centreY = landRef ? 0.0f : height * 0.5f;
	const float halfY = landRef ? height : height * 0.5f;
	const glm::vec2 centre = mapPosition + glm::vec2(k_Half);
	std::array<glm::vec3, 8> corners {};
	for (size_t corner = 0; corner < corners.size(); ++corner)
	{
		// 0x8772BC..0x87736A: every sign of the three half sizes once
		corners.at(corner) = glm::vec3(centre.x + ((corner & 1u) != 0 ? k_Half : -k_Half),
		                               centreY + ((corner & 2u) != 0 ? halfY : -halfY),
		                               centre.y + ((corner & 4u) != 0 ? k_Half : -k_Half));
	}
	return corners;
}

int haze::BlockClassOf(const Params& params, const glm::mat4& view, const LandBlock& block, bool landRef) noexcept
{
	const auto& lnd = block.GetLndBlock();
	const float height = lnd ? static_cast<float>(static_cast<int32_t>(lnd->highestAltitude)) : 0.0f; // fild +0x924
	const auto corners = BlockCorners(block.GetMapPosition(), height, landRef);
	std::array<float, 8> depths {};
	for (size_t corner = 0; corner < depths.size(); ++corner)
	{
		depths.at(corner) = Depth(view, corners.at(corner)); // 0x877370..0x8773BC
	}
	return BlockClass(params, depths);
}

void haze::ApplyVertex(const Params& params, int blockClass, float depth, uint32_t& diffuse, uint32_t& specular) noexcept
{
	if (blockClass == 0)
	{
		return; // 0x874B5E -> 0x875024: the loop without haze
	}
	int f = params.k;
	uint32_t fog = params.packed; // 0x874C48: class 2, no t
	if (blockClass != 2)
	{
		const float t = T(params, depth);
		f = Factor(params, t);
		fog = Colour(params, t); // alpha 0
	}
	// 0x874D21..0x874D9E: the specular first, its own alpha kept (mov ah, [esi+0x17], 0x874D83)
	specular = specular == 0 ? fog : (specular & 0xFF000000u) | AddSaturated(specular, fog);
	// 0x874DA1..0x874DF0
	diffuse = ScaleDiffuse(diffuse, f);
}

std::array<glm::vec4, 2> haze::Uniforms(const Params& params) noexcept
{
	return {glm::vec4(params.nearDistance, params.farDistance, static_cast<float>(params.k), params.on ? 1.0f : 0.0f),
	        glm::vec4(params.colour, 0.0f)};
}

} // namespace openblack::graphics
