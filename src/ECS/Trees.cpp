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
#include <array>
#include <map>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Audio/AnimationSounds.h"
#include "Camera/Camera.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/Utils.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// Tree::PreDraw's global 0xC22FA0, recomputed once a frame.
uint8_t g_brightness = 255;

/// A Forest (0x58 bytes, ctor 0x539BD0): its centre, the empty timer (+0x34) and the planting attempts (+0x36). Its
/// trees are the ones whose Tree::forestId is its id (the original keeps two lists sorted by distance to the centre).
struct ForestData
{
	glm::vec3 centre;
	uint16_t emptyTimer {0};
	uint16_t attempts {0};
};
std::map<uint32_t, ForestData> g_forests;
/// 0xBEA238: the next free forest id
uint32_t g_nextForestId = 1;
/// 0xCD04C8: the turn the last tree of the world was planted by a forest (the planting chance and the water miracle's
/// 40-turn cooldown both use it)
uint32_t g_lastTreeCreatedTurn = 0;
uint32_t g_currentTurn = 0;

/// The original's per-town forest list (Town +0x608): every tree replanted in the same town joins the same forest.
std::unordered_map<uint32_t, uint32_t> g_townForests;

/// GUtils::SigmoidThreshold 0x74F170's table (0xC23284, 41 steps of a logistic curve, 0 to 1)
constexpr std::array<float, 41> k_Sigmoid = {
    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,    0.0f,
    0.0001f, 0.0003f, 0.0008f, 0.0022f, 0.006f,  0.0164f, 0.0444f, 0.1144f, 0.2644f, 0.5f,    0.7356f,
    0.8856f, 0.9556f, 0.9836f, 0.994f,  0.9978f, 0.9992f, 0.9997f, 0.9999f, 1.0f,    1.0f,    1.0f,
    1.0f,    1.0f,    1.0f,    1.0f,    1.0f,    1.0f,    1.0f,    1.0f};

/// GUtils::SigmoidThreshold(t, x) 0x74F170: 0 when t is 1; else the table at (clamp(clamp(x, -1, 1) - t, -1, 1) + 1) x 20.5
float SigmoidThreshold(float threshold, float x)
{
	if (threshold == 1.0f)
	{
		return 0.0f;
	}
	const float v = std::clamp(std::clamp(x, -1.0f, 1.0f) - threshold, -1.0f, 1.0f);
	return k_Sigmoid.at(std::min<size_t>(40, static_cast<size_t>((v + 1.0f) * 20.5f)));
}

/// fn_0074C180: nothing fixed in the way (a 0.5 circle against the fixed objects' circles) and on land. The original
/// reads `(collide & 8) == 0 || IsWater(p)`; the water half looks inverted and is taken as "not in water" [guess].
bool IsFreeForTree(glm::vec3 point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	const glm::vec2 at(point.x, point.z);
	if (Locator::terrainSystem::value().GetHeightAt(at) <= 0.0f)
	{
		return false;
	}
	bool blocked = false;
	Locator::entitiesRegistry::value().Each<const Fixed>([&](const Fixed& fixed) {
		blocked = blocked || glm::distance(at, fixed.boundingCenter) < fixed.boundingRadius + 0.5f;
	});
	return !blocked;
}
} // namespace

uint32_t openblack::ecs::CreateForest(uint32_t id, glm::vec3 centre)
{
	if (id == 0)
	{
		while (g_forests.contains(g_nextForestId))
		{
			++g_nextForestId;
		}
		id = g_nextForestId++;
	}
	else
	{
		g_nextForestId = std::max(g_nextForestId, id + 1);
	}
	g_forests.insert_or_assign(id, ForestData {centre});
	return id;
}

bool openblack::ecs::IsInForest(uint32_t forestId)
{
	return forestId != 0 && g_forests.contains(forestId);
}

uint32_t openblack::ecs::ResolveForestId(int32_t scriptForestId)
{
	const auto id = static_cast<uint32_t>(scriptForestId);
	return IsInForest(id) ? id : 0u;
}

void openblack::ecs::ClearForests()
{
	g_forests.clear();
	g_townForests.clear();
	g_nextForestId = 1;
	g_lastTreeCreatedTurn = 0;
}

uint32_t openblack::ecs::TownForestId(uint32_t townId, glm::vec3 at)
{
	if (const auto it = g_townForests.find(townId); it != g_townForests.end() && IsInForest(it->second))
	{
		return it->second;
	}
	const auto forest = CreateForest(0, at);
	g_townForests.insert_or_assign(townId, forest);
	return forest;
}

std::optional<uint32_t> openblack::ecs::NearestForest(glm::vec3 at, float radius)
{
	std::optional<uint32_t> best;
	float nearest = radius;
	for (const auto& [id, forest] : g_forests)
	{
		const float d = glm::distance(glm::vec2(at.x, at.z), glm::vec2(forest.centre.x, forest.centre.z));
		if (d <= nearest)
		{
			nearest = d;
			best = id;
		}
	}
	return best;
}

void openblack::ecs::SetTreeForest(entt::entity entity, uint32_t forestId)
{
	if (auto* tree = Locator::entitiesRegistry::value().TryGet<Tree>(entity); tree != nullptr)
	{
		tree->forestId = IsInForest(forestId) ? forestId : 0u;
	}
}

entt::entity openblack::ecs::PlantTreeNear(uint32_t forestId, entt::entity parent)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* parentTree = registry.TryGet<const Tree>(parent);
	const auto* parentTransform = registry.TryGet<const Transform>(parent);
	if (parentTree == nullptr || parentTransform == nullptr || !Locator::terrainSystem::has_value())
	{
		return entt::null;
	}
	const auto type = parentTree->type;
	const auto origin = parentTransform->position;
	auto& rng = Locator::rng::value();
	float angle = rng.NextValue(0.0f, glm::two_pi<float>());
	for (int ring = 0; ring < 32; ++ring)
	{
		int radius = static_cast<int>(rng.NextValue<uint32_t>(0, 4)) + 5;
		for (int attempt = 0; attempt < 5; ++attempt)
		{
			// GUtils::GetPosFromAngle 0x74D580: x + cos(a) r, z + sin(a) r
			const glm::vec2 at(origin.x + std::cos(angle) * static_cast<float>(radius),
			                   origin.z + std::sin(angle) * static_cast<float>(radius));
			const glm::vec3 point(at.x, Locator::terrainSystem::value().GetHeightAt(at), at.y);
			if (IsFreeForTree(point))
			{
				g_lastTreeCreatedTurn = g_currentTurn;
				const auto tree = archetypes::TreeArchetype::Create(forestId, point, type, true,
				                                                     rng.NextValue(0.0f, glm::two_pi<float>()),
				                                                     0.8f + rng.NextValue(0.0f, 0.4f), 0.1f);
				registry.SetDirty();
				if (std::getenv("OPENBLACK_TREE_TRACE") != nullptr)
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree trace: forest {} planted {} at ({:.1f}, {:.1f})", forestId,
					                   static_cast<uint32_t>(tree), at.x, at.y);
				}
				return tree;
			}
			radius = (radius + 2) % 10;
		}
		angle += 0.19635f;
	}
	return entt::null;
}

entt::entity openblack::ecs::ApplyWaterSpell(entt::entity entity, bool raiseMaximum)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* tree = registry.TryGet<Tree>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (tree == nullptr || transform == nullptr)
	{
		return entt::null;
	}
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(tree->type));
	const bool growing = tree->growing && transform->scale.x < tree->maxSize;
	if (growing || raiseMaximum)
	{
		float amount = info.waterSpellAcceleratorMultiplier * info.growthAmount;
		if (!growing)
		{
			// GUtils::GetDistanceModifier(size, 3) 0x74F290 = SigmoidThreshold(0.5, 1 - min(size, 3) / 3)
			amount *= 0.5f * SigmoidThreshold(0.5f, 1.0f - std::min(transform->scale.x, 3.0f) / 3.0f);
		}
		if (GrowTree(entity, amount, raiseMaximum) != 0.0f)
		{
			tree->growing = true;
			// TODO: sample 0x78 + GetTickCount() % 9 at the tree
		}
	}
	if (!growing && IsInForest(tree->forestId) && !raiseMaximum && g_currentTurn - g_lastTreeCreatedTurn > 40)
	{
		return PlantTreeNear(tree->forestId, entity);
	}
	return entt::null;
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

namespace
{
/// fn_005DF1B0: an entry of the bend table (0xD19A48): a position and a radius
struct BendSource
{
	glm::vec3 position;
	float radius;
};

/// Object::GetHeight x 0.5 on each axis: LH3DMesh +0x30, the length of the half extents, times the scale
float MeshHalfDiagonal(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	return transform->scale.x * glm::length(0.5f * meshes.Handle(mesh->id)->GetBoundingBox().Size());
}

/// GLandscape::Draw 0x5E4960 / 0x5E49DC and fn_00646FE0 feed the bend table each frame (fn_005DF1B0): the object in the
/// hand (slot 1, every held object is "drawn in hand"), the physics objects in flight (slots 3-13 in turn) and the
/// player's creature (slot 2, no creature yet). Each marks the trees of the 3 x 3 map cells (10 x 10) around it, the
/// last one wins. Tree::Draw 0x74AB8B then bends a marked tree when the crown is not below the source
/// (base y + height >= source y) and the source is closer than its radius r horizontally: by
/// 0.471239 x (1 - ((r - 0.75) d / r + 0.75) / r) rad, d the horizontal distance, about the horizontal axis across the
/// source-to-tree direction, the crown leaning away from the source. The rubbing sound (editor.sad key {c, 0, 0, 10, 75}
/// with c = 3 below 0.3 of the bend, 2 below 0.67, else 1; 3 has no samples) plays when a bend starts.
void UpdateTreeBends()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<std::pair<entt::entity, BendSource>> sources;
	if (Locator::handSystem::has_value())
	{
		if (const auto held = Locator::handSystem::value().GetHeldObject(); held && registry.Valid(*held) &&
		                                                                       registry.AllOf<Transform>(*held))
		{
			sources.emplace_back(*held, BendSource {registry.Get<const Transform>(*held).position, MeshHalfDiagonal(*held)});
		}
	}
	ecs::physics::PhysicsObjects::ForEach([&](const ecs::physics::PhysicsObject& po) {
		if (!registry.Valid(po.entity) || !registry.AllOf<Transform>(po.entity) ||
		    (po.flags & ecs::physics::PhysicsObject::Awake) == 0)
		{
			return;
		}
		sources.emplace_back(po.entity, BendSource {registry.Get<const Transform>(po.entity).position, MeshHalfDiagonal(po.entity)});
	});

	// Tree +0x5C bits 6-9: which source marked each tree this frame (the last one wins)
	std::unordered_map<entt::entity, size_t> marked;
	if (!sources.empty())
	{
		registry.Each<const Tree, const Transform>([&](entt::entity entity, const Tree&, const Transform& transform) {
			const auto cell = glm::floor(glm::vec2(transform.position.x, transform.position.z) * 0.1f);
			for (size_t i = 0; i < sources.size(); ++i)
			{
				if (sources[i].first == entity)
				{
					continue;
				}
				const auto sourceCell = glm::floor(glm::vec2(sources[i].second.position.x, sources[i].second.position.z) * 0.1f);
				if (std::abs(cell.x - sourceCell.x) <= 1.0f && std::abs(cell.y - sourceCell.y) <= 1.0f)
				{
					marked.insert_or_assign(entity, i);
				}
			}
		});
	}

	auto& meshes = Locator::resources::value().GetMeshes();
	const bool trace = std::getenv("OPENBLACK_TREE_TRACE") != nullptr;
	registry.Each<Tree, const Transform, const Mesh>([&](entt::entity entity, Tree& tree, const Transform& transform,
	                                                     const Mesh& mesh) {
		float angle = 0.0f;
		const auto it = marked.find(entity);
		if (it != marked.end() && meshes.Contains(mesh.id))
		{
			const auto& source = sources[it->second].second;
			const float crown = transform.position.y + meshes.Handle(mesh.id)->GetBoundingBox().Size().y * transform.scale.y;
			const glm::vec2 away(transform.position.x - source.position.x, transform.position.z - source.position.z);
			const float d = glm::length(away);
			const float r = source.radius;
			if (crown >= source.position.y && d < r && r > 0.0f)
			{
				const float bend = 1.0f - ((r - 0.75f) * d / r + 0.75f) / r;
				angle = 0.471239f * bend;
				tree.bendDirection = d > 1e-4f ? away / d : glm::vec2(0.0f, 1.0f);
				if (!tree.wasBent)
				{
					const int strength = bend < 0.3f ? 3 : bend < 0.67f ? 2 : 1;
					audio::AnimationSounds::PlayFromTable(entity, transform.position, {strength, 2, 0, 10, 75});
					if (trace)
					{
						SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tree trace: {} bends {:.3f} rad (d {:.2f}, r {:.2f}), sound {}",
						                   static_cast<uint32_t>(entity), angle, d, r, strength);
					}
				}
			}
		}
		tree.bendAngle = angle;
		tree.wasBent = angle != 0.0f;
	});
}
} // namespace

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

	UpdateTreeBends();

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

namespace
{
/// Forest::ProcessForests 0x539D70 -> Forest::Process 0x539DA0, once per turn. An empty forest waits 2000 turns and is
/// deleted. Otherwise, besides growing its trees (ProcessTreesTurn), a forest may plant a new tree: with
/// r = 2000 + rand(1000), f = min(1, 0.05 x its full grown trees), T = turns since the last tree the forests planted
/// anywhere and c = its attempts so far (+1 each turn), when c f T / 300 > r it plants one (Forest::CreateNewTree
/// 0x539FD0: next to one of the rand(n/2 + 1) full grown trees nearest its centre) and c goes back to 0.
void ProcessForests(uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& rng = Locator::rng::value();
	std::vector<uint32_t> empty;
	std::vector<std::pair<uint32_t, entt::entity>> plant;
	for (auto& [id, forest] : g_forests)
	{
		std::vector<std::pair<float, entt::entity>> grown;
		size_t count = 0;
		const auto forestId = id;
		const auto centre = forest.centre;
		registry.Each<const Tree, const Transform>([&](entt::entity entity, const Tree& tree, const Transform& transform) {
			if (tree.forestId != forestId)
			{
				return;
			}
			++count;
			if (!tree.growing || transform.scale.x >= tree.maxSize)
			{
				grown.emplace_back(glm::distance(transform.position, centre), entity);
			}
		});
		if (count == 0)
		{
			if (forest.emptyTimer == 0)
			{
				forest.emptyTimer = 2000;
			}
			else if (--forest.emptyTimer < 2)
			{
				empty.push_back(id);
			}
			continue;
		}
		forest.emptyTimer = 0;
		const float r = 2000.0f + rng.NextValue(0.0f, 1000.0f);
		const float f = std::min(1.0f, 0.05f * static_cast<float>(grown.size()));
		const float t = static_cast<float>(turn - g_lastTreeCreatedTurn);
		const float c = static_cast<float>(++forest.attempts);
		if (c * f * t / 300.0f > r && !grown.empty())
		{
			forest.attempts = 0;
			std::ranges::sort(grown, [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
			const auto pick = std::min(rng.NextValue<size_t>(0, grown.size() / 2), grown.size() - 1);
			plant.emplace_back(id, grown.at(pick).second);
		}
	}
	for (const auto& [id, parent] : plant)
	{
		openblack::ecs::PlantTreeNear(id, parent);
	}
	for (const auto id : empty)
	{
		g_forests.erase(id);
	}
}
} // namespace

void openblack::ecs::ProcessTreesTurn(uint32_t turn)
{
	g_currentTurn = turn;
	ProcessForests(turn);
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
