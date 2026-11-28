/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureMarks.h"

#include <cassert>

#include <algorithm>

using namespace openblack;
using namespace openblack::creature_marks;

namespace
{
constexpr uint32_t k_SkinSize = 256;
/// The skin's side at the detail levels 0 and 1
constexpr uint32_t k_HalfSkinSize = 128;
constexpr uint32_t k_Opaque = 0xF000;
/// The blood's colours by how old it is: up to 21 steps, up to 42, then older
constexpr std::array<glm::u8vec3, 3> k_BloodColours {glm::u8vec3(150, 0, 20), glm::u8vec3(120, 20, 20), glm::u8vec3(80, 40, 0)};
constexpr std::array<uint8_t, 2> k_BloodAges {22, 43};

uint8_t LifetimeOf(const Mark& wound)
{
	return k_WoundLifetimes.at(wound.type % k_WoundLifetimes.size());
}

/// Each mark of the list in turn takes its step; one that goes is replaced by the last mark, which takes its step there
template <typename Gone>
void StepEach(std::vector<Mark>& marks, Gone gone)
{
	for (size_t i = 0; i < marks.size();)
	{
		if (gone(marks[i]))
		{
			marks[i] = marks.back();
			marks.pop_back();
			continue;
		}
		++i;
	}
}
} // namespace

uint32_t creature_marks::ToFileWord(const Mark& mark, uint8_t flags)
{
	return static_cast<uint32_t>(mark.u) | (static_cast<uint32_t>(mark.v) << 8U) |
	       ((static_cast<uint32_t>(mark.age) & 0x3FU) << 16U) | ((static_cast<uint32_t>(mark.skin) & 0x3U) << 22U) |
	       ((static_cast<uint32_t>(mark.type) & 0x7U) << 24U) | ((static_cast<uint32_t>(mark.column) & 0x7U) << 27U) |
	       ((static_cast<uint32_t>(flags) & 0x3U) << 30U);
}

creature_marks::Mark creature_marks::FromFileWord(uint32_t word)
{
	return {
	    .u = static_cast<uint8_t>(word & 0xFFU),
	    .v = static_cast<uint8_t>((word >> 8U) & 0xFFU),
	    .skin = static_cast<uint8_t>((word >> 22U) & 0x3U),
	    .age = static_cast<uint8_t>((word >> 16U) & 0x3FU),
	    .type = static_cast<uint8_t>((word >> 24U) & 0x7U),
	    .column = static_cast<uint8_t>((word >> 27U) & 0x7U),
	};
}

void creature_marks::Add(std::vector<Mark>& marks, const Mark& mark)
{
	if (marks.size() < k_MaxMarks)
	{
		marks.push_back(mark);
		return;
	}
	*std::ranges::max_element(marks, {}, &Mark::age) = mark;
}

bool creature_marks::Heal(Marks& marks, uint32_t counts)
{
	marks.counts += counts;
	bool aged = false;
	while (marks.counts >= k_CountsPerStep)
	{
		marks.counts -= k_CountsPerStep;
		aged = true;
		// A drop of blood at its oldest goes; the others age
		StepEach(marks.blood, [](Mark& blood) {
			if (blood.age >= k_OldestAge)
			{
				return true;
			}
			++blood.age;
			return false;
		});
		// A wound ages until its oldest, and goes when it reaches the age its kind lasts
		StepEach(marks.wounds, [](Mark& wound) {
			if (wound.age >= k_OldestAge)
			{
				return false;
			}
			++wound.age;
			return wound.age == LifetimeOf(wound);
		});
	}
	return aged;
}

glm::u8vec4 creature_marks::WoundTexel(const DamageArt& art, const Mark& wound, uint32_t x, uint32_t y)
{
	const auto column = wound.column % k_CellsPerRow;
	const auto row = wound.type % k_CellsPerRow;
	const auto index = (static_cast<size_t>((row * k_CellSize) + y) * k_AtlasSize) + (column * k_CellSize) + x;
	if (index >= art.fresh.colours.size() || index >= art.old.colours.size() || index >= art.fresh.alpha.size() ||
	    index >= art.old.alpha.size())
	{
		return glm::u8vec4(0);
	}
	// How far towards old, in 256ths
	const auto t = std::min<int32_t>((static_cast<int32_t>(wound.age) * 256) / LifetimeOf(wound), 256);
	const auto mix = [t](int32_t fresh, int32_t old) { return static_cast<uint8_t>(fresh + (((old - fresh) * t) / 256)); };
	const auto& fresh = art.fresh.colours[index];
	const auto& old = art.old.colours[index];
	return {mix(fresh[0], old[0]), mix(fresh[1], old[1]), mix(fresh[2], old[2]),
	        mix(art.fresh.alpha[index], art.old.alpha[index])};
}

uint16_t creature_marks::BlendTexel(uint16_t skin, const glm::u8vec3& colour, uint8_t alpha)
{
	const uint32_t a = alpha;
	// The skin's 4 bits widened to 8 and blended with the colour's 8, then cut back to the top 4
	const auto channel = [&](uint32_t shift, uint8_t value) {
		const uint32_t from = ((skin >> shift) & 0xFu) << 4u;
		return ((((from * (255u - a)) + (static_cast<uint32_t>(value) * a)) / 255u) >> 4u) << shift;
	};
	return static_cast<uint16_t>(channel(8, colour.r) | channel(4, colour.g) | channel(0, colour.b) | k_Opaque);
}

void creature_marks::PaintWound(std::span<uint16_t> skin, uint32_t side, const DamageArt& art, const Mark& wound)
{
	if ((side != k_SkinSize && side != k_HalfSkinSize) || skin.size() != static_cast<size_t>(side) * side)
	{
		return;
	}
	// a skin of half size takes the picture's odd rows and columns, at half the place, centred on half the picture
	const bool large = side == k_SkinSize;
	const auto half = static_cast<int32_t>(large ? k_CellSize / 2 : k_CellSize / 4);
	const auto left = static_cast<int32_t>(large ? wound.u : wound.u >> 1u) - half;
	const auto top = static_cast<int32_t>(large ? wound.v : wound.v >> 1u) - half;
	for (uint32_t y = 0; y < k_CellSize; ++y)
	{
		if (!large && (y & 1u) == 0)
		{
			continue;
		}
		const auto skinY = top + static_cast<int32_t>(large ? y : y >> 1u);
		if (skinY < 0 || skinY >= static_cast<int32_t>(side))
		{
			continue;
		}
		for (uint32_t x = 0; x < k_CellSize; ++x)
		{
			if (!large && (x & 1u) == 0)
			{
				continue;
			}
			const auto skinX = left + static_cast<int32_t>(large ? x : x >> 1u);
			if (skinX < 0 || skinX >= static_cast<int32_t>(side))
			{
				continue;
			}
			const auto texel = WoundTexel(art, wound, x, y);
			if (texel.a == 0)
			{
				continue;
			}
			auto& target = skin[(static_cast<size_t>(skinY) * side) + static_cast<size_t>(skinX)];
			target = BlendTexel(target, glm::u8vec3(texel), texel.a);
		}
	}
}

glm::u8vec3 creature_marks::BloodColour(uint8_t age)
{
	if (age < k_BloodAges[0])
	{
		return k_BloodColours[0];
	}
	return age < k_BloodAges[1] ? k_BloodColours[1] : k_BloodColours[2];
}

uint16_t creature_marks::BloodTexel(uint16_t skin, const glm::u8vec3& colour)
{
	const auto channel = [skin](uint32_t shift, uint8_t value) {
		const uint32_t from = ((skin >> shift) & 0xFu) << 4u;
		return (((from + (3u * value)) / 4u) >> 4u) << shift;
	};
	return static_cast<uint16_t>(channel(8, colour.r) | channel(4, colour.g) | channel(0, colour.b) | k_Opaque);
}

void creature_marks::PaintBlood(std::span<uint16_t> skin, uint32_t side, const Mark& blood)
{
	if ((side != k_SkinSize && side != k_HalfSkinSize) || skin.size() != static_cast<size_t>(side) * side)
	{
		return;
	}
	const bool large = side == k_SkinSize;
	const auto u = static_cast<size_t>(large ? blood.u : blood.u >> 1u);
	const auto v = static_cast<size_t>(large ? blood.v : blood.v >> 1u);
	auto& target = skin[(v * side) + u];
	target = BloodTexel(target, BloodColour(blood.age));
}

void creature_marks::StartTrail(BloodTrails& trails, const MeshCut& cut)
{
	const auto free = std::ranges::find_if(trails, [](const BloodTrail& trail) { return trail.ms >= k_TrailRunsMs; });
	if (free != trails.end())
	{
		*free = BloodTrail {.cut = cut, .ms = 0};
	}
}

bool creature_marks::RunTrail(BloodTrail& trail, int32_t ms)
{
	if (trail.ms >= k_TrailRunsMs)
	{
		return false;
	}
	trail.ms += ms;
	return true;
}
