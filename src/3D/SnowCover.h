/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Snow lying on the island: storms that snow pile it up on a grid of 40 unit cells, and it melts away slowly
/// everywhere. The land turns white where it is deep enough, raggedly by a noise image, and objects standing in it
/// show a snow texture over themselves, more of it the deeper it lies.
namespace openblack::snow_cover
{

/// The texture objects show where snow lies on them, with its alpha
inline constexpr entt::hashed_string k_TextureId = entt::hashed_string("raw/SNOW");
inline constexpr entt::hashed_string k_AlphaTextureId = entt::hashed_string("raw/SNOWA");

/// The grid's cells along each side, and their size
inline constexpr int32_t k_GridSize = 128;
inline constexpr size_t k_Cells = static_cast<size_t>(k_GridSize) * k_GridSize;
inline constexpr float k_CellSize = 40.0f;
/// The deepest the snow lies
inline constexpr float k_MaxDepth = 255.0f;

/// The snow a storm lays on each cell in a turn of 0.1 seconds: the whole part of its snow in percent by its snow cover
/// rate and its fade, 0 to its strength
[[nodiscard]] float StormSnowPerTurn(int8_t snow, float rate, float fade);

/// How the snow has changed over each land block since the land's textures last took it in, and the blocks waiting to
/// be painted again for it: the 16 that changed most, by how much, the most first
struct BlockChanges
{
	static constexpr int32_t k_BlocksPerSide = 32;
	static constexpr size_t k_Listed = 16;
	/// The snow added (less the snow melted) over each block, [z * 32 + x]
	std::array<float, static_cast<size_t>(k_BlocksPerSide) * k_BlocksPerSide> change {};
	std::array<float, k_Listed> listed {};
	std::array<glm::ivec2, k_Listed> places {};
	int32_t count {0};
};

/// A storm snowing on the cells within its outer radius of its centre: as much as `amount` within its inner radius,
/// less beyond, and towards its edge it takes snow away. The depth stays between 0 and k_MaxDepth. With `changes`,
/// what each cell gained or lost counts for its block, and on a block's lower x edge for the block before it too (and,
/// on its lower z edge as well, the one before both), else on its lower z edge for the block before it in z
void AddStorm(std::span<float> grid, glm::vec2 centre, float innerRadius, float outerRadius, float amount,
              BlockChanges* changes = nullptr);

/// The snow melts by 2 in one of eight bands of the grid's rows every 0.3 seconds, so all of it in 2.4 seconds
struct Melting
{
	float clock {0.0f};
	int32_t band {0};
};
/// The melting clock after `seconds` more, melting each band whose turn has come. Whether any snow melted. With
/// `changes`, the snow melted counts for its block, and after each band its blocks are listed (MarkChangedBlocks): the
/// blocks to be painted again go into `marked`
bool Melt(std::span<float> grid, Melting& melting, float seconds, BlockChanges* changes = nullptr,
          std::vector<glm::ivec2>* marked = nullptr);

/// The blocks of rows [firstRow, firstRow + rows) whose snow changed by more than 96 go into the list, each into the
/// first place it beats: when the list is full the block there is marked to be painted again and replaced, else the
/// ones from there move down; its change starts again from 0. A block that beats none keeps its change. At the first
/// row (0) every listed block is marked and the list emptied first. The marked blocks' places (x, z) go into `marked`
void MarkChangedBlocks(BlockChanges& changes, int32_t firstRow, int32_t rows, std::vector<glm::ivec2>& marked);

/// How deep the snow lies at a point in the world, blended between the four cells around it; none off the grid
[[nodiscard]] float DepthAt(std::span<const float> grid, glm::vec2 xz);

/// The snow lying at a point as the weather grid keeps it in a cell's byte: half its depth, towards zero, as a byte
[[nodiscard]] int8_t GridByte(float depth);

/// How much snow shows on an object standing where the snow is `depth` deep, 0 to `cap`: none below 20, then as deep
/// as it lies beyond that, scaled by `rate` of 256
inline constexpr int32_t k_ObjectRate = 255;
[[nodiscard]] int32_t ObjectLevel(float depth, int32_t rate, int32_t cap);
/// Objects show snow from this much of it
inline constexpr int32_t k_ObjectLevelShown = 5;

/// How much of the snow lying under it an object takes: its rate of 256 (ObjectLevel) and the most it shows
struct SnowHold
{
	int32_t rate {k_ObjectRate};
	int32_t cap {k_ObjectRate};
};
/// A field's crop takes a quarter of it, and shows a quarter at most
inline constexpr SnowHold k_FieldCropHold {.rate = 64, .cap = 64};
/// A town's desire flag and a building's needs sign take less still
inline constexpr SnowHold k_NeedsFlagHold {.rate = 45, .cap = 45};
/// A tree drawn out of the map once (in a hand, flying, carried) takes half of it, and a quarter at most, from then on
inline constexpr SnowHold k_TreeOutOfMapHold {.rate = 128, .cap = 64};

/// The most an object's snow moves in one draw: the frame's game time in whole ms, divided by 16. At 60 frames a second
/// it moves by 1, so it takes about 4 seconds to go from none to all of it; with the game paused, or at more than 64
/// frames a second, it does not move
[[nodiscard]] constexpr int32_t LevelStep(uint32_t frameGameMs) noexcept
{
	return static_cast<int32_t>(frameGameMs) >> 4;
}
/// An object's snow after one draw where it stands in the map: the snow lying under it now (`target`), but at most
/// `step` more or less than it had (`level`)
[[nodiscard]] constexpr int32_t EaseLevel(int32_t level, int32_t target, int32_t step) noexcept
{
	if (target > level + step)
	{
		return level + step;
	}
	if (target < level - step)
	{
		return level - step;
	}
	return target;
}

/// What a model is, for whether the snow lying under it shows on it
struct SnowTaker
{
	/// a creature's body (its hair and eyes are drawn from its row)
	bool creature {false};
	/// a bird: the flying animals and the vulture
	bool bird {false};
	/// a villager a script draws in high detail for a cut scene
	bool superVillager {false};
};
/// Whether the snow lying under a model shows on it. The buildings, trees, rocks and the other things of the map do, and
/// so do the villagers and the animals that walk, whose draw measures the snow under them every frame. The creature
/// never does: its body has a draw of its own that draws no snow. Nor do the birds, whose draw puts them in the frame
/// without measuring the snow under them, and marks them as never snowed on. Nor does a villager a script draws in high
/// detail for a cut scene: that draw has no snow
[[nodiscard]] constexpr bool TakesSnow(SnowTaker model) noexcept
{
	return !model.creature && !model.bird && !model.superVillager;
}
/// The snow texture's alpha, 0 to 255, at which an object shows snow at a level: the more snow, the more of it
[[nodiscard]] int32_t ObjectThreshold(int32_t level);

/// Where an object's vertex reads the snow texture (SnowUv in assets/shaders/snow.sh): across by how much its normal
/// faces up, down by how much it faces along x, each plus a fifth of the fraction left of a sum of where the vertex is in
/// the mesh, in the original's order of float steps. The position and the normal are the mesh's own
[[nodiscard]] glm::vec2 ObjectUv(glm::vec3 meshPosition, glm::vec3 meshNormal);
/// The normal a morphing object's snow is read with: the mesh's normal turned by the object's axes, each made unit
/// length (not the result)
[[nodiscard]] glm::vec3 MorphingObjectNormal(const glm::mat3& axes, glm::vec3 meshNormal);

/// How white the land is, 0 to 16, where the snow is `depth` deep over a texel of the noise image
[[nodiscard]] int32_t LandLevel(float depth, uint8_t noise);
/// The land turns no whiter where no corner of its cell has more snow than this
inline constexpr float k_LandLeast = 5.0f;
/// The grey the land turns to under snow, of 15, by the noise image's texel: 14 or 15
[[nodiscard]] uint8_t LandWhite(uint8_t noise);

} // namespace openblack::snow_cover
