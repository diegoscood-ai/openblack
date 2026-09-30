/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The trees' own turn: growth (Tree::Process 0x74A290, Tree::Grow 0x74A3F0) and the forest ids the hand plants into.

#include "Trees.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Audio/AnimationSounds.h"
#include "Camera/Camera.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Archetypes/Utils.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "3D/L3DMesh.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// Tree::PreDraw's global 0xC22FA0, recomputed once a frame.
uint8_t g_brightness = 255;

/// The original's per-town forest list (Town +0x608): every tree replanted in the same town joins the same forest.
std::unordered_map<uint32_t, uint32_t> g_townForests;
} // namespace

bool openblack::ecs::IsInForest(uint32_t forestId)
{
	// The scripts give scenic trees forest 0 or -1; CREATE_NEW_TREE passes NULL when the id is not a real forest.
	return forestId != 0 && forestId != std::numeric_limits<uint32_t>::max();
}

void openblack::ecs::ClearForests()
{
	g_townForests.clear();
}

uint32_t openblack::ecs::NewForestId()
{
	uint32_t highest = 0;
	Locator::entitiesRegistry::value().Each<const Tree>([&highest](const Tree& tree) {
		if (IsInForest(tree.forestId))
		{
			highest = std::max(highest, tree.forestId);
		}
	});
	for (const auto& [town, forest] : g_townForests)
	{
		highest = std::max(highest, forest);
	}
	return highest + 1;
}

uint32_t openblack::ecs::TownForestId(uint32_t townId)
{
	const auto it = g_townForests.find(townId);
	if (it != g_townForests.end())
	{
		return it->second;
	}
	const auto forest = NewForestId();
	g_townForests.emplace(townId, forest);
	return forest;
}

float openblack::ecs::GrowTree(entt::entity entity, float amount, bool raiseMax)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* tree = registry.TryGet<Tree>(entity);
	auto* transform = registry.TryGet<Transform>(entity);
	if (tree == nullptr || transform == nullptr)
	{
		return 0.0f;
	}
	const float size = transform->scale.x;
	if (raiseMax)
	{
		tree->maxSize = std::max(tree->maxSize, size + amount);
	}
	if (size >= tree->maxSize)
	{
		return 0.0f;
	}
	const float grown = std::min(size + amount, tree->maxSize);
	transform->scale = glm::vec3(grown);
	// SetScale is virtual in the original and rebuilds the collide data: the obstacle circle follows the new size.
	if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
	{
		const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(tree->type));
		const auto [point, radius] = archetypes::GetFixedObstacleBoundingCircle(info.normal, *transform);
		fixed->boundingCenter = point;
		fixed->boundingRadius = radius;
	}
	registry.SetDirty();
	return grown - size;
}

uint8_t openblack::ecs::TreeBrightness()
{
	return g_brightness;
}

void openblack::ecs::UpdateTrees(float seconds)
{
	if (!Locator::camera::has_value())
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto cameraPosition = camera.GetOrigin();
	// Tree::PreDraw 0x74A883: b = 200 + 55 x (horizontal view direction . normalize(camera focus - light position)),
	// floored at 200, and Tree::Draw multiplies the tree's colour by b/256. LH3DTech keeps one point light, and by day
	// its only setter is dead code, so the light stays at the map origin (0, 0, 0): trees are darkest when the camera
	// looks back towards that corner. TODO: at dawn and dusk the original moves the light to a camera-relative spot.
	const glm::vec3 light(0.0f, 0.0f, 0.0f);
	const auto toFocus = camera.GetFocus() - light;
	const auto forward = camera.GetForward();
	const glm::vec2 heading(forward.x, forward.z);
	g_brightness = 255;
	if (glm::length(toFocus) > 1e-4f && glm::length(heading) > 1e-4f)
	{
		const auto d = glm::normalize(toFocus);
		const auto v = glm::normalize(heading);
		const float dot = v.x * d.x + v.y * d.z;
		g_brightness = static_cast<uint8_t>(dot < 0.0f ? 200 : std::min(255, static_cast<int>(200.0f + 55.0f * dot)));
	}
	if (std::getenv("OPENBLACK_TREE_TRACE") != nullptr)
	{
		static float since = 0.0f;
		since += seconds;
		if (since > 1.0f)
		{
			since = 0.0f;
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree trace: brightness {} ({:.3f}) heading ({:.2f},{:.2f})", g_brightness,
			                   static_cast<float>(g_brightness) / 256.0f, heading.x, heading.y);
		}
	}

	// Tree::Draw 0x74B111: a tree over 10 tall whose position is within 10 of the camera in x and z (and 18 in y)
	// rustles about once a second (LocalRand(1000 / frame ms) == 1, so the chance per frame is the frame's seconds):
	// one of the editor.sad ambient samples of the tree group (G_TreeRustle / G_TreeCreak).
	if (!Locator::resources::has_value() || seconds <= 0.0f)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	auto& rng = Locator::rng::value();
	registry.Each<const Tree, const Transform, const Mesh>(
	    [&](entt::entity entity, const Tree&, const Transform& transform, const Mesh& mesh) {
		    const auto& at = transform.position;
		    if (std::abs(at.x - cameraPosition.x) > 10.0f || std::abs(at.z - cameraPosition.z) > 10.0f ||
		        std::abs(cameraPosition.y - at.y) >= 18.0f)
		    {
			    return;
		    }
		    if (!meshes.Contains(mesh.id) || meshes.Handle(mesh.id)->GetBoundingBox().Size().y * transform.scale.y <= 10.0f)
		    {
			    return;
		    }
		    if (rng.NextValue(0.0f, 1.0f) >= seconds)
		    {
			    return;
		    }
		    // key {voice, 2, group 20 = tree, surface, soundId 70 = ambient}: the row is {*, *, 20, *, 70}
		    audio::AnimationSounds::PlayFromTable(entity, at, {0, 2, 20, 0, 70});
	    });
}

void openblack::ecs::ProcessTreesTurn(uint32_t /*turn*/)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& constants = Locator::infoConstants::value();
	std::vector<std::pair<entt::entity, float>> growing;
	registry.Each<Tree, const Transform>([&constants, &growing](entt::entity entity, Tree& tree, const Transform& transform) {
		// Only a tree in a forest is processed at all: the forest walks its own list (Forest::Process 0x539DA0).
		if (!IsInForest(tree.forestId))
		{
			return;
		}
		const auto& info = constants.tree.at(static_cast<size_t>(tree.type));
		// Tree::Process: the counter ticks every turn and only the turn it reaches 0 does anything (growTurns = 10 for
		// every type, so once a second).
		if (tree.growCounter != 0 && --tree.growCounter != 0)
		{
			return;
		}
		tree.growCounter = static_cast<uint16_t>(std::max(1u, info.growsAfterNumGameTurns));
		if (!tree.growing)
		{
			return;
		}
		if (transform.scale.x >= tree.maxSize)
		{
			tree.growing = false;
			return;
		}
		// amount = growthAmount x (1 + 0.01 x rainMultiplier x GClimate::GetMaxRainingOrSnowing) x
		//          (1 + 0.5 x MapCoords::GetAlignment). No weather and no land alignment yet: dry, alignment 0.
		constexpr float k_Rain = 0.0f;
		constexpr float k_LandAlignment = 0.0f;
		const float amount = info.growthAmount * (1.0f + 0.01f * info.rainingAcceleratorMultiplier * k_Rain) *
		                     (1.0f + 0.5f * k_LandAlignment);
		growing.emplace_back(entity, amount);
	});
	const bool trace = std::getenv("OPENBLACK_TREE_TRACE") != nullptr;
	for (const auto& [entity, amount] : growing)
	{
		const float grown = GrowTree(entity, amount, false);
		auto* tree = registry.TryGet<Tree>(entity);
		if (tree == nullptr)
		{
			continue;
		}
		const auto& transform = registry.Get<const Transform>(entity);
		tree->growing = transform.scale.x < tree->maxSize;
		if (trace)
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree trace: {} forest {} grew {:.4f} to {:.3f} of {:.3f}{}",
			                   static_cast<uint32_t>(entity), tree->forestId, grown, transform.scale.x, tree->maxSize,
			                   tree->growing ? "" : " (full grown)");
		}
	}
}
