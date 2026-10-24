/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CaveCopyPick.h"

#include <cstddef>

#include <glm/vec4.hpp>

using namespace openblack;

std::vector<glm::vec3> cave_copy_pick::PoseCorners(const glm::mat4& model, std::span<const glm::mat4> bones, bool skinned,
                                                   std::span<const uint16_t> skinBones, std::span<const glm::vec3> base,
                                                   const std::array<std::span<const glm::vec3>, 3>& shapes,
                                                   const creature_morph::Morph& morph)
{
	// A shape mesh whose corners don't match the base's stands in as the base, as the draw does
	std::array<std::span<const glm::vec3>, 3> towards {};
	for (size_t axis = 0; axis < shapes.size(); ++axis)
	{
		towards.at(axis) = shapes.at(axis).size() == base.size() ? shapes.at(axis) : base;
	}
	const bool posed = skinned && !bones.empty();
	std::vector<glm::vec3> corners;
	corners.reserve(base.size());
	for (size_t i = 0; i < base.size(); ++i)
	{
		const auto local = creature_morph::Blend(base[i], towards.at(0)[i], towards.at(1)[i], towards.at(2)[i], morph);
		const size_t bone = i < skinBones.size() && skinBones[i] < bones.size() ? size_t {skinBones[i]} : 0;
		const auto matrix = posed ? model * bones[bone] : model;
		corners.emplace_back(matrix * glm::vec4(local, 1.0f));
	}
	return corners;
}

bool cave_copy_pick::CursorOver(const screen_pick::View& view, const Sphere& sphere, std::span<const Part> parts)
{
	if (!screen_pick::CursorOverSphere(view, sphere.centre, sphere.radius, sphere.origin))
	{
		return false;
	}
	std::vector<screen_pick::ClipCorner> clip;
	screen_pick::PickScratch scratch;
	for (const auto& part : parts)
	{
		clip.clear();
		for (size_t c = 0; c < part.corners.size(); ++c)
		{
			clip.push_back(screen_pick::ToClip(view, part.corners[c], c < part.uvs.size() ? part.uvs[c] : glm::vec2(0.0f)));
		}
		const std::span<const uint16_t> indices = part.indices;
		for (const auto& primitive : part.primitives)
		{
			if (size_t {primitive.first} + primitive.count > indices.size())
			{
				continue;
			}
			const screen_pick::Primitive triangles {
			    .corners = clip,
			    .indices = indices.subspan(primitive.first, primitive.count),
			    .twoSided = primitive.twoSided,
			};
			if (screen_pick::FirstHit(view, triangles, scratch).has_value())
			{
				return true;
			}
		}
	}
	return false;
}
