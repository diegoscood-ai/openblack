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
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Audio/AnimationSounds.h"
#include "Audio/AudioManagerInterface.h"
#include "Camera/Camera.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/Utils.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/DayNightClock.h"
#include "Game.h"
#include "InfoConstants.h"
#include "LandBalance.h"
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
	uint32_t created {0}; ///< creation order (the original's list is newest first)
	entt::entity bigForest {entt::null}; ///< +0x38
	bool scenic {false};                 ///< +0x3C: the town's scenic forest (Town::MakeScenicForest)
};
/// Town +0x608: each town's forests, head first
std::unordered_map<uint32_t, std::vector<uint32_t>> g_townForestLists;
uint32_t g_mapInsertions = 0;
uint32_t g_forestsCreated = 0;
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
/// reads `(collide & 8) == 0 || IsWater(p)`; the water half looks inverted and is taken as "not in water" (inferido).
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
	g_forests.insert_or_assign(id, ForestData {centre, 0, 0, ++g_forestsCreated});
	return id;
}

std::vector<uint32_t> openblack::ecs::ForestsNewestFirst()
{
	std::vector<std::pair<uint32_t, uint32_t>> order;
	for (const auto& [id, forest] : g_forests)
	{
		order.emplace_back(forest.created, id);
	}
	std::ranges::sort(order, [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });
	std::vector<uint32_t> ids;
	for (const auto& [created, id] : order)
	{
		ids.push_back(id);
	}
	return ids;
}

glm::vec3 openblack::ecs::ForestCentre(uint32_t forestId)
{
	const auto it = g_forests.find(forestId);
	return it != g_forests.end() ? it->second.centre : glm::vec3(0.0f);
}

size_t openblack::ecs::ForestTreeCount(uint32_t forestId)
{
	size_t count = 0;
	Locator::entitiesRegistry::value().Each<const Tree>([&](const Tree& tree) {
		count += forestId != 0 && tree.forestId == forestId ? 1 : 0;
	});
	return count;
}

std::vector<entt::entity> openblack::ecs::GrownTreesByDistance(uint32_t forestId)
{
	std::vector<std::pair<float, entt::entity>> grown;
	const auto centre = ForestCentre(forestId);
	Locator::entitiesRegistry::value().Each<const Tree, const Transform>(
	    [&](entt::entity entity, const Tree& tree, const Transform& transform) {
		    if (forestId != 0 && tree.forestId == forestId && (!tree.growing || transform.scale.x >= tree.maxSize))
		    {
			    grown.emplace_back(glm::distance(transform.position, centre), entity);
		    }
	    });
	std::ranges::stable_sort(grown, [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
	std::vector<entt::entity> trees;
	for (const auto& [distance, entity] : grown)
	{
		trees.push_back(entity);
	}
	return trees;
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
	g_townForestLists.clear();
	g_forests.clear();
	g_townForests.clear();
	g_nextForestId = 1;
	g_lastTreeCreatedTurn = 0;
}

uint32_t openblack::ecs::TownForestId(uint32_t townId, glm::vec3 /*at*/)
{
	// Tree::EndPhysics 0x74BA2B: over the town's list (Town +0x608), every forest with +0x3C != 0 (the scenic one)
	// becomes the best at distance 0, so the last of them wins; with none the tree stays without a forest
	uint32_t best = 0;
	if (const auto it = g_townForestLists.find(townId); it != g_townForestLists.end())
	{
		for (const auto id : it->second)
		{
			if (IsScenicForest(id))
			{
				best = id;
			}
		}
	}
	return best;
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

namespace
{
/// GAudio::PlaySoundEffect at a position: a one-shot 3D emitter, within the sample's max distance of the camera
void PlayAt(const std::string& name, glm::vec3 position)
{
	if (!Locator::audio::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto id = entt::hashed_string(name.c_str()).value();
	if (!Locator::resources::value().GetSounds().Contains(id))
	{
		return;
	}
	auto& audio = Locator::audio::value();
	const auto& sound = audio.GetSound(id);
	if (glm::distance(position, Locator::camera::value().GetOrigin()) > sound.maxDistance)
	{
		return;
	}
	const auto emitter = audio.CreateEmitter(id, audio::PlayType::Once, position, glm::vec3(0.0f), glm::vec2(0.0f),
	                                         sound.volume, audio::AudioStatus::Playing, false);
	Locator::entitiesRegistry::value().Get<Transform>(emitter).position = position;
	audio.PlayEmitter(emitter);
}

/// The trees of a forest (both of the original's lists)
std::vector<entt::entity> ForestTrees(uint32_t forestId)
{
	std::vector<entt::entity> trees;
	Locator::entitiesRegistry::value().Each<const Tree>([&](entt::entity entity, const Tree& tree) {
		if (forestId != 0 && tree.forestId == forestId)
		{
			trees.push_back(entity);
		}
	});
	return trees;
}
} // namespace

float openblack::ecs::GrowAllTrees(uint32_t forestId, float amount)
{
	float total = 0.0f;
	for (const auto tree : ForestTrees(forestId))
	{
		total += std::max(0.0f, GrowTree(tree, amount, false));
	}
	return total;
}

float openblack::ecs::ShrinkAllTrees(uint32_t forestId, float amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	float total = 0.0f;
	for (const auto tree : ForestTrees(forestId))
	{
		auto& transform = registry.Get<Transform>(tree);
		const float size = transform.scale.x - amount;
		if (size <= 0.0f)
		{
			// fn_0074A3A0: a tree that would reach 0 is ToBeDeleted and adds nothing
			DeleteTree(tree);
			continue;
		}
		transform.scale = glm::vec3(size);
		if (auto* fixed = registry.TryGet<Fixed>(tree); fixed != nullptr)
		{
			const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(registry.Get<const Tree>(tree).type));
			const auto [point, radius] = archetypes::GetFixedObstacleBoundingCircle(info.normal, transform);
			fixed->boundingCenter = point;
			fixed->boundingRadius = radius;
		}
		total += amount;
	}
	registry.SetDirty();
	return total;
}

float openblack::ecs::TallestTreeHeight(uint32_t forestId)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& meshes = Locator::resources::value().GetMeshes();
	float tallest = 0.0f;
	for (const auto tree : ForestTrees(forestId))
	{
		const auto* mesh = registry.TryGet<const Mesh>(tree);
		if (mesh == nullptr || !meshes.Contains(mesh->id))
		{
			continue;
		}
		tallest = std::max(tallest, meshes.Handle(mesh->id)->GetBoundingBox().Size().y * registry.Get<const Transform>(tree).scale.y);
	}
	return tallest;
}

namespace
{
std::vector<openblack::ecs::TreeDeletedListener> g_treeDeletedListeners;
} // namespace

void openblack::ecs::AddTreeDeletedListener(TreeDeletedListener listener)
{
	g_treeDeletedListeners.push_back(std::move(listener));
}

void openblack::ecs::DeleteTree(entt::entity tree)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(tree))
	{
		return;
	}
	for (const auto& listener : g_treeDeletedListeners)
	{
		listener(tree);
	}
	if (ecs::physics::PhysicsObjects::Find(tree) != nullptr)
	{
		ecs::physics::PhysicsObjects::RemoveObject(tree);
	}
	registry.Destroy(tree);
	registry.SetDirty();
}

void openblack::ecs::DeleteForest(uint32_t forestId)
{
	for (const auto tree : ForestTrees(forestId))
	{
		DeleteTree(tree);
	}
	g_forests.erase(forestId);
	// fn_0053AE10: every town (g_game +0x205C84) drops the forest from its list Town +0x608 (fn_00741A70)
	std::erase_if(g_townForests, [forestId](const auto& pair) { return pair.second == forestId; });
	for (auto& [town, list] : g_townForestLists)
	{
		std::erase(list, forestId);
	}
}

float openblack::ecs::TreeWoodValue(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return 0.0f;
	}
	const auto* life = registry.TryGet<const Life>(entity);
	const float lifeValue = life != nullptr ? life->value : 1.0f;
	const float scale = transform->scale.x;
	if (const auto* tree = registry.TryGet<const Tree>(entity); tree != nullptr)
	{
		const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(tree->type));
		return lifeValue * 1.0f * static_cast<float>(info.woodValue) * scale * openblack::land_balance::Get(5);
	}
	if (const auto* dead = registry.TryGet<const DeadTree>(entity); dead != nullptr)
	{
		const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type));
		return lifeValue * static_cast<float>(info.woodValue) * scale * scale * scale;
	}
	return 0.0f;
}

uint32_t openblack::ecs::TreeWood(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Tree>(entity))
	{
		return static_cast<uint32_t>(TreeWoodValue(entity));
	}
	const auto* dead = registry.TryGet<const DeadTree>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (dead == nullptr || transform == nullptr)
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type));
	// DeadTree +0x9C: Tree::GetWoodValueMultiplier 0x74B810 of the tree it was, 1.0
	constexpr float k_WoodMultiplier = 1.0f;
	return static_cast<uint32_t>(static_cast<float>(info.woodValue) * k_WoodMultiplier * transform->scale.x);
}

uint32_t openblack::ecs::RemoveWood(entt::entity entity, uint32_t amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* dead = registry.TryGet<const DeadTree>(entity);
	if (dead == nullptr)
	{
		return 0;
	}
	// Object::GetResource 0x639520: GetDefaultResource() when the type is the object's GetResourceType() (WOOD for a
	// dead tree), 0 otherwise
	const uint32_t have = TreeWood(entity);
	if (have <= amount)
	{
		DeleteTree(entity);
		return have;
	}
	const auto& info = Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type));
	constexpr float k_WoodMultiplier = 1.0f;
	auto& transform = registry.Get<Transform>(entity);
	transform.scale = glm::vec3(static_cast<float>(have - amount) / (static_cast<float>(info.woodValue) * k_WoodMultiplier));
	registry.SetDirty();
	return amount;
}

CarriedTreeType openblack::ecs::TreeCarriedType(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto* tree = registry.TryGet<const Tree>(entity); tree != nullptr)
	{
		return Locator::infoConstants::value().tree.at(static_cast<size_t>(tree->type)).carriedType;
	}
	const auto* dead = registry.TryGet<const DeadTree>(entity);
	if (dead == nullptr)
	{
		return CarriedTreeType::None;
	}
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr)
	{
		// CarriedObject::Init 0x462600: the four carried logs, MeshPack 0x196 / 0x15B / 0x15C / 0x15D
		constexpr std::array<uint32_t, 4> k_Logs = {0x196, 0x15B, 0x15C, 0x15D};
		for (size_t i = 0; i < k_Logs.size(); ++i)
		{
			if (mesh->id == resources::HashIdentifier(static_cast<MeshId>(k_Logs[i])))
			{
				return static_cast<CarriedTreeType>(i);
			}
		}
	}
	return Locator::infoConstants::value().tree.at(static_cast<size_t>(dead->type)).carriedType;
}

entt::entity openblack::ecs::FellTree(entt::entity tree, entt::entity chopper)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* treeComponent = registry.TryGet<const Tree>(tree);
	const auto* mesh = registry.TryGet<const Mesh>(tree);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (treeComponent == nullptr || mesh == nullptr || !meshes.Contains(mesh->id) || !registry.Valid(chopper) ||
	    !registry.AllOf<Transform>(chopper))
	{
		return entt::null;
	}
	const auto& transform = registry.Get<const Transform>(tree);
	const auto from = registry.Get<const Transform>(chopper).position;
	// k = 0.4 (0x8C7A44) x GetHeight() x 0.5 (0x8AA3B4); Object::GetHeight 0x638120 = 2 x LH3DMesh +0x28 (half height)
	// x scale, here the mesh box height x scale (the same, the repo's convention)
	const float height = meshes.Handle(mesh->id)->GetBoundingBox().Size().y * transform.scale.y;
	const float k = 0.4f * height * 0.5f;
	// the direction from the forester to the tree (y 0) and its angle a = fn_007FAA50 = atan2(x, -z) (0 when shorter than
	// sqrt(0.001)): velocity (sin a, 0, -cos a) k = k along that direction; spin (cos a, 0, sin a) x 0.4 rad/s
	const glm::vec3 d(transform.position.x - from.x, 0.0f, transform.position.z - from.z);
	// 0x51179E: |d|^2 <= 0.001 (0x8AA3B0) gives a = 0
	const float a = glm::dot(d, d) <= 0.001f ? 0.0f : std::atan2(d.x, -d.z);
	const glm::vec3 velocity(std::sin(a) * k, 0.0f, -std::cos(a) * k);
	// The spin is a BODY-space angular velocity: PhysicsObject::AddObject 0x6443A0 (0x6445DB-0x644691) builds the angular
	// momentum as (w I) summed over the body matrix rows, the tree's matrix with its yaw (SetUpPos 0x63A603), so in world
	// it is R x (cos a, 0, sin a) x 0.4. And PhysOb::Integrate 0x7FE260 turns the rows by R(w, angle), which is a turn by
	// -angle in openblack's right-handed PhysOb (tmp_dis/physics/physob.md, "Sign convention"), so the axis is negated.
	const glm::vec3 spin = -(transform.rotation * (0.4f * glm::vec3(std::cos(a), 0.0f, std::sin(a))));

	// DeadTree::DeadTree 0x510880 takes over the tree's 3D object (mesh, matrix, scale) and info; the roots do not break
	// off (only Tree::EndPhysics sets that flag)
	const auto type = treeComponent->type;
	// Villager::ForesterChopsTree 0x75FAE7 then calls tree->ToBeDeleted(0): the tree leaves its forest and whoever
	// listens (hand, reactions) lets go of it. Its fire (+0x44) moves to the DeadTree (fn_00730960): kept here because it
	// is the same entity.
	for (const auto& listener : g_treeDeletedListeners)
	{
		listener(tree);
	}
	registry.Remove<Tree>(tree);
	registry.Assign<DeadTree>(tree, type);
	registry.AssignOrReplace<FelledTree>(tree, chopper);
	registry.SetDirty();
	auto* po = ecs::physics::PhysicsObjects::AddObject(tree, velocity, spin, chopper, false);
	if (po != nullptr)
	{
		// PhysOb::AdjustToGroundLevel(false, true) 0x7FCB80
		po->body.AdjustToGroundLevel(false, true);
		// TODO: po->flags |= 2, PhysicsObject::RaiseUntilNotIntersecting 0x644800, po +0x1A4 = 2 (flag 2 and +0x1A4
		// unidentified), and the two REACTION 0x0C "wood here" (one from the DeadTree ctor 0x510957, one from
		// FelledTree::Create 0x511889): reactions not ported for trees
	}
	return tree;
}

float openblack::ecs::Object2DRadius(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
	{
		return 0.0f;
	}
	const auto half = 0.5f * meshes.Handle(mesh->id)->GetBoundingBox().Size();
	return transform->scale.x * std::max(half.x, half.z);
}

uint32_t openblack::ecs::NextMapInsertion()
{
	return ++g_mapInsertions;
}

std::vector<entt::entity> openblack::ecs::TreesInCell(glm::ivec2 cell)
{
	std::vector<std::pair<uint32_t, entt::entity>> found;
	Locator::entitiesRegistry::value().Each<const Tree, const Transform>(
	    [&](entt::entity entity, const Tree& tree, const Transform& transform) {
		    const glm::ivec2 at(static_cast<int>(std::floor(transform.position.x * 0.1f)),
		                        static_cast<int>(std::floor(transform.position.z * 0.1f)));
		    if (at == cell)
		    {
			    found.emplace_back(tree.mapInsertion, entity);
		    }
	    });
	// the list is filled at its head: the tree inserted last is first
	std::ranges::sort(found, [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });
	std::vector<entt::entity> trees;
	for (const auto& [stamp, entity] : found)
	{
		trees.push_back(entity);
	}
	return trees;
}

glm::vec3 openblack::ecs::TreeWorkingPos(entt::entity tree, entt::entity who)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& at = registry.Get<const Transform>(tree).position;
	const auto& from = registry.Get<const Transform>(who).position;
	const glm::vec2 towards(from.x - at.x, from.z - at.z);
	const float length = glm::length(towards);
	const glm::vec2 direction = length > 1e-6f ? towards / length : glm::vec2(1.0f, 0.0f);
	// 0.9: 0x8C5844
	const float reach = Object2DRadius(who) + 0.9f;
	const glm::vec2 point = glm::vec2(at.x, at.z) + direction * reach;
	const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : at.y;
	return {point.x, ground, point.y};
}

entt::entity openblack::ecs::FindTreeNearVillager(entt::entity who)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(who) || !registry.AllOf<Transform>(who))
	{
		return entt::null;
	}
	const auto& from = registry.Get<const Transform>(who).position;
	glm::ivec2 cell(static_cast<int>(std::floor(from.x * 0.1f)), static_cast<int>(std::floor(from.z * 0.1f)));
	// GUtils::Spiral 0x74D7E0 with the step table 0xDA59FC {(1,0), (0,1), (-1,0), (0,-1)}: the centre, then 1 step,
	// 1 step, 2, 2 over the 3 x 3 cells
	static constexpr std::array<glm::ivec2, 4> k_Steps = {glm::ivec2(1, 0), glm::ivec2(0, 1), glm::ivec2(-1, 0),
	                                                      glm::ivec2(0, -1)};
	std::vector<glm::ivec2> cells {cell};
	int run = 1;
	size_t step = 0;
	while (cells.size() < 9)
	{
		for (int twice = 0; twice < 2 && cells.size() < 9; ++twice)
		{
			for (int i = 0; i < run && cells.size() < 9; ++i)
			{
				cell += k_Steps.at(step % 4);
				cells.push_back(cell);
			}
			++step;
		}
		++run;
	}
	entt::entity best = entt::null;
	float nearest = 99999.0f; // 0x47C34F80
	for (const auto& c : cells)
	{
		const auto trees = TreesInCell(c);
		if (trees.empty())
		{
			continue;
		}
		// only the first of the cell (INDESTRUCTIBLE 0x4000 never set on a tree outside puzzles)
		const auto tree = trees.front();
		const auto working = TreeWorkingPos(tree, who);
		const float d = glm::distance(glm::vec2(from.x, from.z), glm::vec2(working.x, working.z));
		if (d < nearest)
		{
			nearest = d;
			best = tree;
		}
	}
	return best;
}

entt::entity openblack::ecs::ForestBigForest(uint32_t forestId)
{
	const auto it = g_forests.find(forestId);
	return it != g_forests.end() ? it->second.bigForest : entt::entity {entt::null};
}

void openblack::ecs::SetForestBigForest(uint32_t forestId, entt::entity bigForest)
{
	if (const auto it = g_forests.find(forestId); it != g_forests.end())
	{
		it->second.bigForest = bigForest;
	}
}

bool openblack::ecs::IsScenicForest(uint32_t forestId)
{
	const auto it = g_forests.find(forestId);
	return it != g_forests.end() && it->second.scenic;
}

float openblack::ecs::ForestWood(uint32_t forestId)
{
	auto& registry = Locator::entitiesRegistry::value();
	float wood = 0.0f;
	if (const auto bigForest = ForestBigForest(forestId); bigForest != entt::null && registry.Valid(bigForest))
	{
		// BigForest::GetWoodValue 0x4390B0 = life x +0x84
		if (const auto* forest = registry.TryGet<const BigForest>(bigForest); forest != nullptr)
		{
			const auto* life = registry.TryGet<const Life>(bigForest);
			wood += (life != nullptr ? life->value : 1.0f) * forest->wood;
		}
	}
	for (const auto tree : ForestTrees(forestId))
	{
		wood += TreeWoodValue(tree);
	}
	return wood;
}

entt::entity openblack::ecs::ForestCentreTree(uint32_t forestId)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto centre = ForestCentre(forestId);
	std::optional<std::pair<float, entt::entity>> grown;
	std::optional<std::pair<float, entt::entity>> growing;
	for (const auto tree : ForestTrees(forestId))
	{
		const auto& t = registry.Get<const Tree>(tree);
		const auto& transform = registry.Get<const Transform>(tree);
		const float d = glm::distance(transform.position, centre);
		auto& head = (t.growing && transform.scale.x < t.maxSize) ? growing : grown;
		if (!head || d < head->first)
		{
			head = std::make_pair(d, tree);
		}
	}
	if (grown && (!growing || grown->first <= growing->first))
	{
		return grown->second;
	}
	return growing ? growing->second : entt::entity {entt::null};
}

std::optional<uint32_t> openblack::ecs::FindForest(glm::vec3 at, float max, bool onlyEmpty)
{
	std::optional<uint32_t> best;
	for (const auto id : ForestsNewestFirst())
	{
		const auto& forest = g_forests.at(id);
		const float d = glm::distance(glm::vec2(at.x, at.z), glm::vec2(forest.centre.x, forest.centre.z));
		if (d >= max)
		{
			continue;
		}
		const size_t trees = ForestTreeCount(id);
		const bool fits = onlyEmpty ? (trees == 0 && forest.bigForest == entt::null) : trees > 0;
		if (fits)
		{
			max = d;
			best = id;
		}
	}
	return best;
}

namespace
{
/// fn_0053ADB0 and Town::FindNearestForestToPos: the forest's point nearest `at`: a BigForest's nearest edge (or `at`
/// itself when inside its 2D radius, FindNearestForestToPos only), else its centre. BigForest::GetNearestEdgeToPos
/// (vt+0x83C) is taken as the point of its 2D-radius circle towards `at` (inferido).
glm::vec2 ForestNearestPoint(uint32_t forestId, glm::vec3 at, bool insideIsZero)
{
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec2 target(at.x, at.z);
	const auto bigForest = openblack::ecs::ForestBigForest(forestId);
	if (bigForest != entt::null && registry.Valid(bigForest) && registry.AllOf<Transform>(bigForest))
	{
		const auto& p = registry.Get<const Transform>(bigForest).position;
		const glm::vec2 centre(p.x, p.z);
		const float radius = openblack::ecs::Object2DRadius(bigForest);
		const float d = glm::distance(centre, target);
		if (insideIsZero && d <= radius)
		{
			return target;
		}
		return d > 1e-6f ? centre + (target - centre) / d * radius : centre;
	}
	const auto c = openblack::ecs::ForestCentre(forestId);
	return {c.x, c.z};
}
} // namespace

void openblack::ecs::MakeScenicForest(uint32_t townId, glm::vec3 townCentre)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& list = g_townForestLists[townId];
	std::optional<uint32_t> scenic;
	for (const auto id : list)
	{
		if (IsScenicForest(id))
		{
			scenic = id;
			break;
		}
	}
	// R = 250 (GTownInfo +0x164) + 10 (0x8AB414)
	const float radius = Locator::infoConstants::value().town.maxDistanceForTownForest + 10.0f;
	std::vector<entt::entity> taken;
	registry.Each<const Tree, const Transform>([&](entt::entity entity, const Tree& tree, const Transform& transform) {
		// the spiral stops at the first CELL farther than R: taken as every tree whose cell centre is within R
		// (aproximado: the spiral's corners)
		const glm::vec2 cellCentre = (glm::floor(glm::vec2(transform.position.x, transform.position.z) * 0.1f) +
		                              glm::vec2(0.5f)) * 10.0f;
		if (glm::distance(cellCentre, glm::vec2(townCentre.x, townCentre.z)) > radius)
		{
			return;
		}
		if (!IsInForest(tree.forestId))
		{
			taken.push_back(entity);
			return;
		}
		if (IsScenicForest(tree.forestId))
		{
			const auto centre = ForestCentre(tree.forestId);
			if (glm::distance(transform.position, townCentre) < glm::distance(transform.position, centre))
			{
				taken.push_back(entity);
			}
		}
	});
	if (!scenic)
	{
		scenic = CreateForest(0, townCentre);
		g_forests.at(*scenic).scenic = true;
	}
	for (const auto tree : taken)
	{
		SetTreeForest(tree, *scenic);
	}
}

void openblack::ecs::AssignForestsToTown(uint32_t townId, glm::vec3 reference)
{
	auto& list = g_townForestLists[townId];
	list.clear();
	const float maxDistance = Locator::infoConstants::value().town.maxDistanceForTownForest;
	// the global list, newest first; fn_00741AF0 inserts at the head (no duplicates)
	for (const auto id : ForestsNewestFirst())
	{
		const auto point = ForestNearestPoint(id, reference, false);
		if (glm::distance(point, glm::vec2(reference.x, reference.z)) < maxDistance && ForestWood(id) != 0.0f)
		{
			list.insert(list.begin(), id);
		}
	}
}

std::vector<uint32_t> openblack::ecs::TownForests(uint32_t townId)
{
	const auto it = g_townForestLists.find(townId);
	return it != g_townForestLists.end() ? it->second : std::vector<uint32_t> {};
}

std::optional<uint32_t> openblack::ecs::FindNearestForestToPos(uint32_t townId, glm::vec3 at)
{
	const float maxDistance = Locator::infoConstants::value().town.maxDistanceForTownForest;
	float bestNormal = maxDistance;
	float bestScenic = maxDistance;
	std::optional<uint32_t> normal;
	std::optional<uint32_t> scenic;
	for (const auto id : TownForests(townId))
	{
		const float d = glm::distance(ForestNearestPoint(id, at, true), glm::vec2(at.x, at.z));
		if (IsScenicForest(id))
		{
			if (d < bestScenic)
			{
				bestScenic = d;
				scenic = id;
			}
		}
		else if (d < bestNormal)
		{
			bestNormal = d;
			normal = id;
		}
	}
	return normal ? normal : scenic;
}

glm::vec3 openblack::ecs::BigForestArrivePos(entt::entity bigForest, entt::entity who)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& at = registry.Get<const Transform>(bigForest).position;
	const auto& from = registry.Get<const Transform>(who).position;
	const glm::vec2 towards(from.x - at.x, from.z - at.z);
	const float length = glm::length(towards);
	const glm::vec2 direction = length > 1e-6f ? towards / length : glm::vec2(1.0f, 0.0f);
	const glm::vec2 point = glm::vec2(at.x, at.z) + direction * (0.5f * Object2DRadius(bigForest));
	const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : at.y;
	return {point.x, ground, point.y};
}

uint32_t openblack::ecs::BigForestRemoveWood(entt::entity bigForest, uint32_t amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* forest = registry.TryGet<BigForest>(bigForest);
	if (forest == nullptr)
	{
		return 0;
	}
	const auto* life = registry.TryGet<const Life>(bigForest);
	const float lifeValue = life != nullptr ? life->value : 1.0f;
	const float wanted = static_cast<float>(amount) / lifeValue;
	const float woodValue = lifeValue * forest->wood; // GetWoodValue 0x4390B0
	if (woodValue <= wanted)
	{
		const auto had = static_cast<uint32_t>(woodValue);
		forest->wood = 0.0f;
		registry.Destroy(bigForest);
		registry.SetDirty();
		return had;
	}
	forest->wood -= wanted;
	auto& transform = registry.Get<Transform>(bigForest);
	const float drawn = lifeValue * transform.scale.x * forest->woodValue;
	if (std::abs(lifeValue * forest->wood - drawn) > 250.0f)
	{
		transform.scale = glm::vec3(lifeValue * forest->wood / (lifeValue * forest->woodValue));
		// AddTreeAround 0x439220
		const auto position = transform.position;
		const float radius = Object2DRadius(bigForest);
		auto& rng = Locator::rng::value();
		for (int attempt = 0; attempt < 10 && Locator::terrainSystem::has_value(); ++attempt)
		{
			const float angle = rng.NextValue(0.0f, glm::two_pi<float>());
			const glm::vec2 point(position.x + std::cos(angle) * radius, position.z + std::sin(angle) * radius);
			const float ground = Locator::terrainSystem::value().GetHeightAt(point);
			if (ground <= 0.0f)
			{
				continue;
			}
			bool blocked = false;
			registry.Each<const Transform, const Mesh>([&](entt::entity other, const Transform& t, const Mesh&) {
				blocked = blocked || (other != bigForest &&
				                      glm::distance(glm::vec2(t.position.x, t.position.z), point) < 4.0f);
			});
			if (blocked)
			{
				continue;
			}
			// Tree::Create(pos, GTreeInfo 0xDA49D8 = Pine, the BigForest's forest, maxSize 0.75 (0x8AC3F8) +
			// GameFloatRand(0.5), yAngle GameFloatRand(2pi), size 0.05)
			const float maxSize = 0.75f + rng.NextValue(0.0f, 0.5f);
			archetypes::TreeArchetype::Create(forest->forestId, glm::vec3(point.x, ground, point.y), TreeInfo::Pine, false,
			                                  rng.NextValue(0.0f, glm::two_pi<float>()), maxSize, 0.05f);
			break;
		}
	}
	registry.SetDirty();
	return amount;
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
			// sample 0x78 + GetTickCount() % 9 (InGame.sad 120-128, G_TreeGrow) at the tree
			PlayAt(fmt::format("InGame.sad/{}", 120 + Locator::rng::value().NextValue<uint32_t>(0, 8)), transform->position);
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
	// floored at 200, and Tree::Draw multiplies the tree's colour by b/256. LH3DTech keeps one point light, placed by
	// fn_005E5830 each frame (after Tree::PreDraw, so the trees use the last frame's). By day (LH3DSky::Time2SkyType of
	// the visual time 0) it is the global 0xEA1C88, whose only setter is dead code: the map origin (0, 0, 0), so trees
	// are darkest when the camera looks back towards that corner. At dusk and night (sky type > 0) it goes 3 units from
	// the hand towards the camera, the hand raised to at least 10 above the land under it.
	glm::vec3 light(0.0f, 0.0f, 0.0f);
	if (Game::Instance() != nullptr && Game::Instance()->GetDayNightClock().GetSkyType() > 0.0f &&
	    Locator::handSystem::has_value())
	{
		if (const auto& hands = Locator::handSystem::value().GetPlayerHandPositions(); hands[0].has_value())
		{
			auto hand = *hands[0];
			if (Locator::terrainSystem::has_value())
			{
				hand.y = std::max(hand.y, Locator::terrainSystem::value().GetHeightAt(glm::vec2(hand.x, hand.z)) + 10.0f);
			}
			const auto toCamera = cameraPosition - hand;
			light = hand + (glm::length(toCamera) > 1e-4f ? glm::normalize(toCamera) : glm::vec3(0.0f)) * 3.0f;
		}
	}
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
		// empty: no BigForest (+0x38) and no trees in either list
		if (count == 0 && forest.bigForest == entt::null)
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
		// +0x3C == 1: a town's scenic forest is not processed (its trees do not grow, it plants none)
		if (forest.scenic)
		{
			continue;
		}
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
		openblack::ecs::DeleteForest(id);
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
		if (!IsInForest(tree.forestId) || IsScenicForest(tree.forestId))
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
