/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureEffectCut.h"

#include "3D/MapCoords.h"

using namespace openblack;

namespace
{
/// The squared length of a line: z first, then y, then x
float Length2(const glm::vec3& line)
{
	const float z = line.z * line.z;
	const float zy = z + (line.y * line.y);
	return zy + (line.x * line.x);
}
} // namespace

float creature_effect_cut::KeptDamage(float damage)
{
	if (!(damage >= 0.0f))
	{
		return 0.0f;
	}
	return damage > 1.0f ? 1.0f : damage;
}

uint8_t creature_effect_cut::Column(uint32_t draw, float damage)
{
	const float depth = (damage * k_ColumnPerDamage) - k_ColumnPerDamage;
	const auto column = static_cast<int32_t>(draw + static_cast<uint32_t>(-2 - map_coords::FtoL(depth)));
	if (column < 1)
	{
		return 0;
	}
	return column < k_LastColumn ? static_cast<uint8_t>(column) : k_LastColumn;
}

std::optional<creature_effect_cut::Cut> creature_effect_cut::Plan(const Inputs& inputs, const Random& random)
{
	const float height = inputs.size * k_HeightPerSize;
	const float halfHeight = height * k_UpShare;
	const float across = height * k_AcrossShare;
	const float acrossBoth = across + across;
	Cut cut;
	if (inputs.burn < inputs.crush)
	{
		// From where the effect came, towards the groin
		cut.type = inputs.damage < k_HighDamage ? k_LowDamageKind : k_HighDamageKind;
		cut.from = inputs.from;
	}
	else
	{
		// About the groin, now and then
		cut.type = random.integer(k_RareKindOneIn) != 0 ? k_GroinKind : k_RareGroinKind;
		const float odds = (inputs.markedShare * k_MarkedShareWeight) + k_MarkedShareBase;
		if (random.integer(static_cast<uint32_t>(map_coords::FtoL(odds))) != 0)
		{
			return std::nullopt;
		}
		cut.from.x = (random.real(acrossBoth) + inputs.groin.x) - across;
		cut.from.y = (random.real(height) + inputs.groin.y) - halfHeight;
		cut.from.z = (random.real(acrossBoth) + inputs.groin.z) - across;
	}
	cut.direction = inputs.groin - cut.from;
	if (!(Length2(cut.direction) > k_LeastLength2))
	{
		return std::nullopt;
	}
	cut.column = Column(random.integer(k_ColumnDraws), inputs.damage);
	return cut;
}

std::vector<creature_effect_cut::Cut> creature_effect_cut::CaughtFire(glm::vec3 groin, float size, const Random& random)
{
	const float height = size * k_HeightPerSize;
	const float halfHeight = height * k_UpShare;
	const float across = height * k_AcrossShare;
	const float acrossBoth = across + across;
	std::vector<Cut> cuts;
	for (uint32_t i = 0; i < k_CaughtFireCuts; ++i)
	{
		Cut cut;
		cut.from.x = (random.real(acrossBoth) + groin.x) - across;
		cut.from.y = (random.real(height) + groin.y) - halfHeight;
		cut.from.z = (random.real(acrossBoth) + groin.z) - across;
		cut.direction = groin - cut.from;
		if (!(Length2(cut.direction) > k_LeastLength2))
		{
			continue;
		}
		cut.type = random.integer(k_RareKindOneIn) != 0 ? k_GroinKind : k_RareGroinKind;
		cut.column = static_cast<uint8_t>(std::min(random.integer(k_CaughtFireColumns), uint32_t {k_LastColumn}));
		cuts.push_back(cut);
	}
	return cuts;
}

uint8_t creature_effect_cut::Texel(float first, float second, float third, float s, float t)
{
	constexpr float k_SkinTexels = 256.0f;
	constexpr uint32_t k_LastTexel = 255;
	const float along = ((second - first) * s) + ((third - first) * t);
	const auto texel = static_cast<uint32_t>(map_coords::FtoL((along + first) * k_SkinTexels));
	if (texel == 0)
	{
		return 0;
	}
	return texel < k_LastTexel ? static_cast<uint8_t>(texel) : static_cast<uint8_t>(k_LastTexel);
}
