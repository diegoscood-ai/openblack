/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHands.h"

#include <cmath>

#include "Creature/CreatureRig.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Registry.h"

using namespace openblack;

namespace
{
/// The bones of the body as posed for the game turn, its parents and its hand bones, when it has all of them
struct Posed
{
	std::span<const glm::mat4> bones;
	std::span<const uint32_t> parents;
	std::pair<uint32_t, uint32_t> hands;
};

std::optional<Posed> PosedOf(const ecs::Registry& registry, entt::entity creature, const creature::CreatureRig& rig)
{
	if (!registry.Valid(creature))
	{
		return std::nullopt;
	}
	const auto* pose = registry.TryGet<const ecs::components::CreatureTurnPose>(creature);
	const auto* animation = registry.TryGet<const ecs::components::CreatureAnimation>(creature);
	if (pose == nullptr || animation == nullptr)
	{
		return std::nullopt;
	}
	const auto bones = creature_turn_pose::Safe(*pose);
	const auto& parents = animation->skeleton.parents;
	const auto hands = creature_hands::HandBones(rig, parents.size());
	if (bones.empty() || !hands.has_value() || bones.size() < parents.size())
	{
		return std::nullopt;
	}
	return Posed {.bones = bones, .parents = parents, .hands = *hands};
}
} // namespace

std::optional<glm::vec3> creature_hands::HandPoint(std::span<const uint32_t> parents, std::span<const glm::mat4> bones,
                                                   uint32_t hand, uint32_t mirror)
{
	const auto a = creature_turn_pose::LeafMean(parents, bones, hand);
	const auto b = creature_turn_pose::LeafMean(parents, bones, mirror);
	if (!a.has_value() || !b.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3((a->x + b->x) * 0.5f, (a->y + b->y) * 0.5f, (a->z + b->z) * 0.5f);
}

glm::vec3 creature_hands::Towards(glm::vec3 point, glm::vec3 hands)
{
	const glm::vec3 way(point.x - hands.x, point.y - hands.y, point.z - hands.z);
	const auto none = [](float part) { return !(part < 0.0f) && !(part > 0.0f); };
	if (none(way.x) && none(way.y) && none(way.z))
	{
		return way;
	}
	const float inverse = 1.0f / std::sqrt(((way.z * way.z) + (way.y * way.y)) + (way.x * way.x));
	return {way.x * inverse, way.y * inverse, way.z * inverse};
}

std::optional<std::pair<uint32_t, uint32_t>> creature_hands::HandBones(const creature::CreatureRig& rig, size_t boneCount)
{
	if (!rig.actionPoints.has_value())
	{
		return std::nullopt;
	}
	const auto hand = rig.actionPoints->rightHand;
	const auto mirror = creature_turn_pose::MirrorBone(rig.fileTail, boneCount, hand);
	if (!mirror.has_value())
	{
		return std::nullopt;
	}
	return std::pair {hand, *mirror};
}

std::optional<glm::vec3> creature_hands::HandsOf(const ecs::Registry& registry, entt::entity creature,
                                                 const creature::CreatureRig& rig)
{
	const auto posed = PosedOf(registry, creature, rig);
	if (!posed.has_value())
	{
		return std::nullopt;
	}
	return HandPoint(posed->parents, posed->bones, posed->hands.first, posed->hands.second);
}

std::optional<std::pair<glm::vec3, glm::vec3>>
creature_hands::BeamPointsOf(const ecs::Registry& registry, entt::entity creature, const creature::CreatureRig& rig)
{
	const auto posed = PosedOf(registry, creature, rig);
	if (!posed.has_value())
	{
		return std::nullopt;
	}
	return std::pair {glm::vec3(posed->bones[posed->hands.second][3]), glm::vec3(posed->bones[posed->hands.first][3])};
}
