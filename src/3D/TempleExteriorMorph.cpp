/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleExteriorMorph.h"

#include <cassert>
#include <cmath>

#include <algorithm>

#include <L3DFile.h>
#include <fmt/format.h>

namespace openblack::TempleExteriorMorph
{

namespace
{
/// The size's index stops short of the largest, and the alignment's target short of 1
constexpr float k_JustShortOfOne = 0.9999f;
/// From neutral the texture goes halfway either way: twice 255 a whole
constexpr float k_TextureWeight = 510.0f;
/// The share of the influence the game gives a player on the first land, and when nobody has any
constexpr float k_SmallShare = 0.01f;

/// Either value more than k_Changed from the one last blended
bool MovedSinceBlend(const State& state)
{
	return std::abs(state.blendedAlignment - state.alignment) > k_Changed ||
	       std::abs(state.blendedSize - state.size) > k_Changed;
}
} // namespace

float Step(float current, float target)
{
	if (std::abs(current - target) <= k_Near)
	{
		return target;
	}
	return current < target ? std::min(current + k_Step, target) : std::max(current - k_Step, target);
}

float AlignmentTarget(float playerAlignment)
{
	const float target = (playerAlignment + 1.0f) * 0.5f;
	// Below 0 it is 0, and from 1 up just short of 1
	if (target < 0.0f)
	{
		return 0.0f;
	}
	return target < 1.0f ? target : k_JustShortOfOne;
}

float SizeTarget(bool firstLand, float ownPower, float allPowers)
{
	const float share = firstLand || allPowers == 0.0f ? k_SmallShare : ownPower / allPowers;
	const float target = 2.0f * share;
	if (target < 0.0f)
	{
		return 0.0f;
	}
	return target > 1.0f ? 1.0f : target;
}

bool Turn(State& state, float alignmentTarget, float sizeTarget)
{
	state.alignmentTarget = alignmentTarget;
	state.sizeTarget = sizeTarget;
	state.alignment = Step(state.alignment, state.alignmentTarget);
	state.size = Step(state.size, state.sizeTarget);
	return MovedSinceBlend(state);
}

bool NeedsBlend(const State& state)
{
	return state.neverBlended || MovedSinceBlend(state);
}

void Blended(State& state)
{
	state.blendedAlignment = state.alignment;
	state.blendedSize = state.size;
	state.neverBlended = false;
}

Place PlaceOf(float size, float alignment)
{
	// Each the index below and the one after, as far as there are, and how much of the one below
	const auto sizeBelow = static_cast<int32_t>(2.0f * std::min(size, k_JustShortOfOne));
	const auto sizeLow = static_cast<uint32_t>(std::clamp(sizeBelow, 0, static_cast<int32_t>(k_Sizes) - 1));
	const auto sizeHigh = static_cast<uint32_t>(std::clamp(sizeBelow + 1, 0, static_cast<int32_t>(k_Sizes) - 1));
	const auto stageBelow = static_cast<int32_t>(4.0f * alignment);
	const auto stageLow = static_cast<uint32_t>(std::clamp(stageBelow, 0, static_cast<int32_t>(k_Stages) - 1));
	const auto stageHigh = static_cast<uint32_t>(std::clamp(stageBelow + 1, 0, static_cast<int32_t>(k_Stages) - 1));
	return {
	    .sizeLow = sizeLow,
	    .sizeHigh = sizeHigh,
	    .sizeWeight = std::clamp(static_cast<float>(sizeHigh) - (2.0f * size), 0.0f, 1.0f),
	    .stageLow = stageLow,
	    .stageHigh = stageHigh,
	    .stageWeight = static_cast<float>(stageHigh) - (4.0f * alignment),
	};
}

float Mix(float first, float second, float towardSecond)
{
	// one step a statement, so that no multiply and add are fused: each rounds to a float as the game's do
	const float keep = 1.0f - towardSecond;
	const float rest = 1.0f - keep;
	const float fromFirst = keep * first;
	const float fromSecond = rest * second;
	return fromFirst + fromSecond;
}

l3d::L3DVertex Mix(const l3d::L3DVertex& first, const l3d::L3DVertex& second, float towardSecond)
{
	const auto mix = [towardSecond](float a, float b) { return Mix(a, b, towardSecond); };
	return {
	    .position = {mix(first.position.x, second.position.x), mix(first.position.y, second.position.y),
	                 mix(first.position.z, second.position.z)},
	    .texCoord = {mix(first.texCoord.x, second.texCoord.x), mix(first.texCoord.y, second.texCoord.y)},
	    .normal = {mix(first.normal.x, second.normal.x), mix(first.normal.y, second.normal.y),
	               mix(first.normal.z, second.normal.z)},
	};
}

std::string MeshName(uint32_t size, uint32_t stage)
{
	return fmt::format("b_temple{}{}_l3d", size, stage);
}

bool BlendVertices(const Place& place, const VerticesOf& verticesOf, std::span<l3d::L3DVertex> blended)
{
	const auto shaped = [&verticesOf, &blended](uint32_t size, uint32_t stage) -> const std::vector<l3d::L3DVertex>* {
		const auto* vertices = verticesOf(size, stage);
		return vertices != nullptr && vertices->size() == blended.size() ? vertices : nullptr;
	};
	const bool twoStages = place.stageLow != place.stageHigh;
	const auto* lowerSmall = shaped(place.sizeLow, place.stageLow);
	const auto* lowerLarge = shaped(place.sizeHigh, place.stageLow);
	const auto* upperSmall = twoStages ? shaped(place.sizeLow, place.stageHigh) : nullptr;
	const auto* upperLarge = twoStages ? shaped(place.sizeHigh, place.stageHigh) : nullptr;
	if (lowerSmall == nullptr || lowerLarge == nullptr || (twoStages && (upperSmall == nullptr || upperLarge == nullptr)))
	{
		return false;
	}
	// the share of the larger size and of the upper stage, each rounded to a float before the mix
	const float towardLarger = 1.0f - place.sizeWeight;
	const float towardUpper = 1.0f - place.stageWeight;
	for (size_t i = 0; i < blended.size(); ++i)
	{
		auto vertex = Mix((*lowerSmall)[i], (*lowerLarge)[i], towardLarger);
		if (twoStages)
		{
			vertex = Mix(vertex, Mix((*upperSmall)[i], (*upperLarge)[i], towardLarger), towardUpper);
		}
		blended[i] = vertex;
	}
	return true;
}

void BakeToLand(const land_morph::Ground& ground, const glm::mat4& object, std::span<l3d::L3DVertex> vertices)
{
	std::vector<glm::vec3> positions(vertices.size());
	std::ranges::transform(vertices, positions.begin(),
	                       [](const l3d::L3DVertex& v) { return glm::vec3(v.position.x, v.position.y, v.position.z); });
	land_morph::BakeAgainstY(ground, object, positions);
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		vertices[i].position.y = positions[i].y;
	}
}

TextureBlend TextureOf(float alignment)
{
	const bool good = alignment > 0.5f;
	const float along = good ? alignment - 0.5f : alignment;
	const auto weight = std::clamp(static_cast<int32_t>(along * k_TextureWeight), 0, 255);
	return {
	    .from = good ? Look::Neutral : Look::Evil,
	    .to = good ? Look::Good : Look::Neutral,
	    .weight = static_cast<uint8_t>(weight),
	};
}

std::string ImageName(Look look, uint32_t set)
{
	constexpr std::array<std::string_view, 3> k_Looks {"evil", "neutral", "good"};
	return fmt::format("{}{}", k_Looks.at(static_cast<size_t>(look)), set);
}

void BlendTexels(std::span<const uint16_t> from, std::span<const uint16_t> to, uint8_t weight, std::span<uint16_t> blended)
{
	assert(from.size() == to.size() && to.size() == blended.size());
	const uint32_t toWeight = weight;
	const uint32_t fromWeight = 255 - toWeight;
	// Each channel in its place, the two by their weights over 255, rounded down
	const auto channel = [toWeight, fromWeight](uint32_t a, uint32_t b, uint32_t mask) {
		return ((((b & mask) * toWeight) + ((a & mask) * fromWeight)) / 255) & mask;
	};
	for (size_t i = 0; i < blended.size(); ++i)
	{
		const uint32_t a = from[i];
		const uint32_t b = to[i];
		blended[i] = static_cast<uint16_t>(channel(a, b, 0x00F) | channel(a, b, 0x0F0) | channel(a, b, 0xF00) | (a & 0xF000));
	}
}

} // namespace openblack::TempleExteriorMorph
