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

#include <array>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>

#include "3D/LandMorph.h"

namespace openblack::l3d
{
struct L3DVertex;
}

namespace openblack::TempleExteriorMorph
{

/// A temple's outside is three sizes, by its player's share of influence, of five stages each, from evil to good: the
/// meshes B_TEMPLE<size><stage>, all of the same shape. Its mesh is a blend of the four about where it is.
constexpr uint32_t k_Sizes = 3;
constexpr uint32_t k_Stages = 5;

/// The game moves each a step a turn toward where it is to be, and the rest of the way when that is near
constexpr float k_Step = 0.016f;
constexpr float k_Near = 0.001f;
[[nodiscard]] float Step(float current, float target);

/// Where the temple's alignment heads, from 0, evil, to just short of 1, good: its player's alignment, from -1 to 1,
/// taken to that range
[[nodiscard]] float AlignmentTarget(float playerAlignment);
/// Where the temple's size heads, from 0 to 1: twice its player's share of the influence. On the first land the share
/// is always a small one; elsewhere it is the player's influence power over the sum of every player's, that small
/// share again when nobody has any
[[nodiscard]] float SizeTarget(bool firstLand, float ownPower, float allPowers);

/// The game blends a temple's mesh again only once its look has moved more than this from the one last blended
constexpr float k_Changed = 0.03f;

/// Where a temple's outside is, where it heads, and what its mesh was last blended for. A new temple is neutral and
/// small, and its mesh is blended once as it is made, before it has moved at all
struct State
{
	float alignment {0.5f};
	float alignmentTarget {0.5f};
	float blendedAlignment {0.5f};
	float size {0.0f};
	float sizeTarget {0.0f};
	float blendedSize {0.0f};
	bool neverBlended {true};
};
/// One turn: the targets taken and each value a Step toward its own. True when either value is now more than
/// k_Changed from the one last blended, which is when the game blends the mesh again
[[nodiscard]] bool Turn(State& state, float alignmentTarget, float sizeTarget);
/// Whether a blend is due: the look moved far enough, or the mesh was never blended
[[nodiscard]] bool NeedsBlend(const State& state);
/// A blend done for the state's look
void Blended(State& state);

/// Where a look lies among the meshes: the size below and above with the lower one's share, and the stage below and
/// above with the lower one's share
struct Place
{
	uint32_t sizeLow;
	uint32_t sizeHigh;
	float sizeWeight;
	uint32_t stageLow;
	uint32_t stageHigh;
	float stageWeight;
};
/// The place of a size and an alignment, each from 0 to 1
[[nodiscard]] Place PlaceOf(float size, float alignment);
/// Two values mixed as two meshes are: the first keeps one less the share toward the second, rounded to a float, and
/// the second gets what that leaves of one; each product and the sum rounded to a float
[[nodiscard]] float Mix(float first, float second, float towardSecond);
/// Every value of a vertex (position, texture coordinates, normal) mixed so
[[nodiscard]] l3d::L3DVertex Mix(const l3d::L3DVertex& first, const l3d::L3DVertex& second, float towardSecond);
/// The name of a size's and stage's mesh
[[nodiscard]] std::string MeshName(uint32_t size, uint32_t stage);
/// A size's and stage's vertices, or null when that mesh is missing
using VerticesOf = std::function<const std::vector<l3d::L3DVertex>*(uint32_t size, uint32_t stage)>;
/// Each vertex blended two meshes at a time: the lower stage's two sizes, then, when the stages differ, the upper
/// stage's two sizes, and then the two results by the stage. The normal is not made unit again. False when any of the
/// place's meshes is missing or not of the blend's shape, whatever its share (each is read)
[[nodiscard]] bool BlendVertices(const Place& place, const VerticesOf& verticesOf, std::span<l3d::L3DVertex> blended);
/// Each blended vertex is laid on the land: lowered by how far the temple stands above the land under that vertex
/// (land_morph::BakeAgainstY with the temple's matrix)
void BakeToLand(const land_morph::Ground& ground, const glm::mat4& object, std::span<l3d::L3DVertex> vertices);

/// The two images of a temple's texture blended from evil to neutral, or neutral to good, and how far, from 0 to 255
enum class Look : uint8_t
{
	Evil,
	Neutral,
	Good,
};
struct TextureBlend
{
	Look from;
	Look to;
	uint8_t weight;
};
[[nodiscard]] TextureBlend TextureOf(float alignment);
/// The name of a look's image of one of the four sets of textures
[[nodiscard]] std::string ImageName(Look look, uint32_t set);
/// Each 16 bit texel, four bits a channel, blended from one image to another by weight from 0 to 255, keeping the first
/// image's alpha
void BlendTexels(std::span<const uint16_t> from, std::span<const uint16_t> to, uint8_t weight, std::span<uint16_t> blended);

} // namespace openblack::TempleExteriorMorph
