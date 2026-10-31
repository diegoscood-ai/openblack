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

/// How many bone matrices an object's vertex shader declares. A draw uploads the whole array the shader declares, not
/// only the bones its mesh has, and Vulkan holds every draw's uniforms of a frame in one buffer of fixed size: drawing
/// every mesh with the largest array overflows it on a land with many villagers, so each mesh is drawn with the
/// smallest array that holds its bones.
namespace openblack::graphics::bone_budget
{

/// The bones of the shaders for meshes without bones, which use only their model matrix
constexpr std::size_t k_Single = 1;
/// The bones of the shaders for meshes with a skeleton of their own, such as the villagers' 22
constexpr std::size_t k_Few = 32;
/// The bones of the shaders that hold the largest skeleton, a creature's
constexpr std::size_t k_All = 128;

enum class Budget : uint8_t
{
	Single,
	Few,
	All,
};

/// The smallest budget that holds a mesh's bones
[[nodiscard]] constexpr Budget For(std::size_t boneCount) noexcept
{
	if (boneCount <= k_Single)
	{
		return Budget::Single;
	}
	if (boneCount <= k_Few)
	{
		return Budget::Few;
	}
	return Budget::All;
}

/// The budget of one sub-mesh's draw. The original skins by each sub-mesh's own bones flag, not by its mesh's: a skinned
/// sub-mesh takes the bone matrix of each of its vertex groups, and a rigid one only the object's matrix, even inside a
/// boned mesh. A skinned sub-mesh never gets Single, even with one bone, as that bone still moves it out of its file's
/// space (docs/wiki/rendering/meshes-and-lod.md#skinned-and-rigid-sub-meshes).
[[nodiscard]] constexpr Budget ForSubMesh(bool skinned, std::size_t meshBones) noexcept
{
	if (!skinned)
	{
		return Budget::Single;
	}
	return meshBones <= k_Few ? Budget::Few : Budget::All;
}

/// How many bones a budget's shaders declare
[[nodiscard]] constexpr std::size_t Bones(Budget budget) noexcept
{
	switch (budget)
	{
	case Budget::Single:
		return k_Single;
	case Budget::Few:
		return k_Few;
	case Budget::All:
		return k_All;
	}
	return k_All;
}

} // namespace openblack::graphics::bone_budget
