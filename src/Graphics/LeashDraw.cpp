/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashDraw.h"

#include <cmath>

#include <algorithm>

#include "3D/LandMorph.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::graphics;

// The rope's shadow lies on the land as the game's leash does
static_assert(leash_rope::k_ShadowLift == land_morph::k_LeashRibbonLift);

namespace
{
constexpr uint32_t k_Rgb = 0x00FFFFFFu;

/// An opacity, 0 to 1, as the alpha byte of a colour
uint32_t AlphaByte(float alpha)
{
	return static_cast<uint32_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
}
} // namespace

leash_draw::Draw leash_draw::Build(const leash_rope::Rope& rope, const glm::vec3& eye, const leash_rope::GroundHeight& ground,
                                   const LightAt& light)
{
	const auto ribbon = leash_rope::BuildRibbon(rope, eye, ground);
	Draw draw {};
	for (size_t point = 0; point < leash_rope::k_PointCount; ++point)
	{
		// Both corners of a point take the light at the point itself, its alpha included, but the two end points are
		// fully transparent
		const bool atEnd = point == 0 || point == leash_rope::k_PointCount - 1;
		const uint32_t lit = light(leash_rope::Point(rope, point));
		const uint32_t argb = atEnd ? lit & k_Rgb : lit;
		for (const size_t i : {point * 2, (point * 2) + 1})
		{
			const auto& corner = ribbon.rope.at(i);
			draw.rope.at(i) = {.position = corner.position, .uv = corner.uv, .abgr = world_triangles::ToAbgr(argb)};
		}
	}
	for (size_t i = 0; i < ribbon.shadow.size(); ++i)
	{
		const auto& corner = ribbon.shadow.at(i);
		const uint32_t argb = AlphaByte(corner.alpha) << 24u;
		draw.shadow.at(i) = {.position = corner.position, .uv = corner.uv, .abgr = world_triangles::ToAbgr(argb)};
	}
	return draw;
}

leash_draw::Strip leash_draw::StripOf(const Draw& draw)
{
	Strip strip {};
	for (size_t point = 0; point < leash_rope::k_PointCount; ++point)
	{
		strip.at(point * 4) = draw.rope.at(point * 2);
		strip.at((point * 4) + 1) = draw.rope.at((point * 2) + 1);
		strip.at((point * 4) + 2) = draw.shadow.at(point * 2);
		strip.at((point * 4) + 3) = draw.shadow.at((point * 2) + 1);
	}
	return strip;
}

std::array<uint16_t, leash_draw::k_StripIndexCount> leash_draw::StripIndices()
{
	// One segment's triangles from its first point's corners: the rope's quad, then the shadow's
	constexpr std::array<uint16_t, 12> k_Segment {5, 4, 0, 0, 1, 5, 7, 6, 2, 2, 3, 7};
	std::array<uint16_t, k_StripIndexCount> indices {};
	for (size_t segment = 0; segment + 1 < leash_rope::k_PointCount; ++segment)
	{
		for (size_t i = 0; i < k_Segment.size(); ++i)
		{
			indices.at((segment * k_Segment.size()) + i) = static_cast<uint16_t>(k_Segment.at(i) + (segment * 4));
		}
	}
	return indices;
}

bool leash_draw::IsDrawn(const ecs::components::CreatureLeash& leash)
{
	return leash.worn.has_value() && leash.worn->ropeStarted;
}

std::vector<const leash_rope::Rope*> leash_draw::Ropes(const ecs::Registry& registry, bool shown)
{
	std::vector<const leash_rope::Rope*> ropes;
	if (!shown)
	{
		return ropes;
	}
	registry.Each<const ecs::components::CreatureLeash>([&ropes](const ecs::components::CreatureLeash& leash) {
		if (IsDrawn(leash))
		{
			ropes.push_back(&leash.worn->rope);
		}
	});
	return ropes;
}
