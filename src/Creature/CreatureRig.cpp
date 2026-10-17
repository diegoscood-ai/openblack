/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureRig.h"

#include <glm/gtx/transform.hpp>

using namespace openblack;
using namespace openblack::creature;

const skeletal_animation::Animation* CreatureRig::GetAnimation(Mesh mesh, size_t index) const
{
	const auto slot = static_cast<size_t>(mesh);
	if (slot < animations.size() && index < animations.at(slot).size() && animations.at(slot)[index].has_value())
	{
		return &*animations.at(slot)[index];
	}
	const auto& base = animations.front();
	return index < base.size() && base[index].has_value() ? &*base[index] : nullptr;
}

std::optional<std::vector<uint32_t>> CreatureRig::MirrorBones(size_t boneCount) const
{
	// A version 20 file holds one number fewer when the first bone has a mirror, which can't be told from the end of
	// the file alone; none of the game's files is of that version
	constexpr uint32_t k_ShiftedVersion = 20;
	if (boneCount == 0 || fileTail.size() < boneCount || creatureVersion == k_ShiftedVersion)
	{
		return std::nullopt;
	}
	std::vector<uint32_t> mirror(boneCount);
	const auto first = fileTail.size() - boneCount;
	for (size_t i = 0; i < boneCount; ++i)
	{
		const auto value = fileTail[first + i];
		if (value < 0 || static_cast<size_t>(value) >= boneCount)
		{
			return std::nullopt;
		}
		mirror[i] = static_cast<uint32_t>(value);
	}
	return mirror;
}

glm::mat4 creature::PlacementMatrix(const glm::vec3& position, const glm::mat3& rotation, const glm::vec3& scale)
{
	auto model = glm::mat4(rotation);
	model = glm::translate(model, position * rotation);
	return glm::scale(model, scale);
}

glm::mat4 creature::PosedBone(uint32_t bone, std::span<const glm::mat4> boneMatrices, const glm::mat4& placement)
{
	return bone < boneMatrices.size() ? placement * boneMatrices[bone] : placement;
}

std::array<glm::vec3, 3> creature::PosedTriangle(const std::array<std::array<glm::vec3, 3>, CreatureRig::k_MeshCount>& vertices,
                                                 const std::array<uint32_t, 3>& bones, const creature_morph::Morph& morph,
                                                 std::span<const glm::mat4> boneMatrices, const glm::mat4& placement)
{
	const auto evilGood = morph.evilGood < 0.0f ? CreatureRig::Mesh::Evil : CreatureRig::Mesh::Good;
	const auto thinFat = morph.thinFat < 0.0f ? CreatureRig::Mesh::Thin : CreatureRig::Mesh::Fat;
	const auto weakStrong = morph.weakStrong < 0.0f ? CreatureRig::Mesh::Weak : CreatureRig::Mesh::Strong;
	std::array<glm::vec3, 3> world {};
	for (size_t i = 0; i < world.size(); ++i)
	{
		const auto vertexOf = [&vertices, i](CreatureRig::Mesh mesh) { return vertices.at(static_cast<size_t>(mesh)).at(i); };
		const auto local = creature_morph::Blend(vertexOf(CreatureRig::Mesh::Base), vertexOf(evilGood), vertexOf(thinFat),
		                                         vertexOf(weakStrong), morph);
		world.at(i) = glm::vec3(PosedBone(bones.at(i), boneMatrices, placement) * glm::vec4(local, 1.0f));
	}
	return world;
}

std::array<std::optional<CreatureRig::TattooPoint>, creature_tattoo::k_SlotCount>
creature::FindTattooPoints(const creature_tattoo::Sites& sites, std::span<const creature_tattoo_editor::TexturedPrimitive> base,
                           const std::array<PrimitivePositions, CreatureRig::k_MeshCount>& positions)
{
	std::array<std::optional<CreatureRig::TattooPoint>, creature_tattoo::k_SlotCount> points {};
	for (size_t i = 0; i < sites.size(); ++i)
	{
		if (!sites.at(i).enabled)
		{
			continue;
		}
		const auto triangle = creature_tattoo_editor::FindSiteTriangle(base, sites.at(i));
		if (!triangle.has_value())
		{
			continue;
		}
		const auto positionIn = [&positions, &triangle](size_t mesh, uint16_t vertex) -> std::optional<glm::vec3> {
			const auto& primitives = positions.at(mesh);
			if (triangle->primitive < primitives.size() && vertex < primitives[triangle->primitive].size())
			{
				return primitives[triangle->primitive][vertex];
			}
			return std::nullopt;
		};
		CreatureRig::TattooPoint point {.vertices = {}, .bones = triangle->bones, .u = triangle->u, .v = triangle->v};
		bool complete = true;
		for (size_t m = 0; m < CreatureRig::k_MeshCount && complete; ++m)
		{
			for (size_t c = 0; c < 3; ++c)
			{
				const auto vertex = triangle->vertices.at(c);
				auto position = positionIn(m, vertex);
				if (!position.has_value())
				{
					position = positionIn(0, vertex);
				}
				if (!position.has_value())
				{
					complete = false;
					break;
				}
				point.vertices.at(m).at(c) = *position;
			}
		}
		if (complete)
		{
			points.at(i) = point;
		}
	}
	return points;
}

std::vector<creature_tattoo_editor::SitePoint>
creature::PosedTattooPoints(std::span<const std::optional<CreatureRig::TattooPoint>> points, const creature_morph::Morph& morph,
                            std::span<const glm::mat4> boneMatrices, const glm::mat4& placement)
{
	std::vector<creature_tattoo_editor::SitePoint> posed;
	for (size_t i = 0; i < points.size(); ++i)
	{
		if (!points[i].has_value())
		{
			continue;
		}
		const auto& point = *points[i];
		const auto world = PosedTriangle(point.vertices, point.bones, morph, boneMatrices, placement);
		const auto surface = creature_eyes::PointOnTriangle(world[0], world[1], world[2], point.u, point.v);
		posed.push_back({.site = static_cast<uint8_t>(i), .position = surface.position, .inwardNormal = surface.normal});
	}
	return posed;
}
