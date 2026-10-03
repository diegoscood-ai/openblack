/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <tuple>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <L3DFile.h>
#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "Camera/Camera.h"
#include "Windowing/WindowingInterface.h"
#include "Camera/CameraModel.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Physics/PhysOb.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/GroundMarks.h"
#include "ECS/MapCells.h"
#include "ECS/Trees.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PSys/PSysManager.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

bool HandSystem::IsHoldingTree() const noexcept
{
	// Tree::GetHoldType and DeadTree::GetHoldType return HOLD_TYPE_TREE.
	return _held && Locator::entitiesRegistry::value().Valid(*_held) &&
	       Locator::entitiesRegistry::value().AnyOf<Tree, DeadTree>(*_held);
}

void HandSystem::Replant(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(tree);
	auto& component = registry.Get<Tree>(tree);
	DropRoots(tree, false);
	transform.position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	if (auto* fixed = registry.TryGet<Fixed>(tree); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	const glm::vec2 at(transform.position.x, transform.position.z);

	// Tree::EndPhysics 0x74B982..0x74BADB: a spiral (GUtils::Spiral 0x74BAC9, MapCoords += JustMapXZ 0x605470) over a
	// copy of the tree's MapCoords, at most 1000 cells (0x3E8, 0x74B982), that stops at the first cell farther than
	// 25 [0xC22FA4] + 10 [0x8AB414] m ("fcompp; test ah, 1" 0x74B99B..0x74B9B6). It walks only the fixed list of each
	// cell (GetFirstObjectFixed 0x74B9C0, GetMapChild 0x74BA66). For each object d = fn_00605CD0(object, tree) minus
	// its Get2DRadius (vt +0x64, 0x74B9D6..0x74B9E6):
	// - d < 25 (0x74B9EE) and a town (GetTown vt +0x48) or a citadel part (IsCitadelPart 0x6380C0: info type 8): in a
	//   town (0x74BA7B); with a town every scenic forest of its list (Town +0x608) becomes the best at distance 0, so
	//   the last one wins (0x74BA9C..0x74BAB4, ecs::TownForestId); then the next CELL (0x74BAB6);
	// - else a Tree (RTTI 0x74BA20) with a forest (GetForest vt +0x86C) nearer than the best (from 99999 =
	//   0x47C34F80, 0x74B902; "test ah, 1" 0x74BA46) lends its forest.
	// The later cells go on with the same best, after a town too.
	namespace map_cells = ecs::map_cells;
	namespace map_coords = ecs::map_coords;
	constexpr int32_t k_SearchCells = 1000;
	constexpr float k_TownRadius = 25.0f;
	constexpr float k_SearchRadius = k_TownRadius + 10.0f;
	// Object::GetTown vt +0x48 of the fixed list's classes: Abode 0x401730 (+0x98; storage pits and town centres are
	// abodes here), Field 0x528960 (+0x118), FishFarm 0x52C450 (+0x8C), TotemStatue 0x738480 (its town centre's),
	// PotStructure 0x66EF60 (a store's pile: the store's, vt +0x860); MultiMapFixed 0x4220A0 / Object 0x419950: none
	const auto abodeTown = [&registry](entt::entity object) -> std::optional<uint32_t> {
		if (const auto* abode = registry.TryGet<const Abode>(object); abode != nullptr)
		{
			return abode->townId;
		}
		return std::nullopt;
	};
	const auto townOf = [&registry, &abodeTown](entt::entity object) -> std::optional<uint32_t> {
		std::optional<uint32_t> id;
		if (const auto* field = registry.TryGet<const components::Field>(object); field != nullptr)
		{
			id = static_cast<uint32_t>(field->town);
		}
		else if (registry.AllOf<Abode>(object))
		{
			id = abodeTown(object);
		}
		else if (const auto* farm = registry.TryGet<const components::FishFarm>(object); farm != nullptr)
		{
			if (registry.Valid(farm->town) && registry.AllOf<components::Town>(farm->town))
			{
				id = registry.Get<const components::Town>(farm->town).id;
			}
		}
		else if (const auto* totem = registry.TryGet<const components::TotemStatue>(object); totem != nullptr)
		{
			if (registry.Valid(totem->townCentre))
			{
				id = abodeTown(totem->townCentre);
			}
		}
		else if (registry.AllOf<components::Pot>(object))
		{
			if (const auto store = ecs::StoragePitStore::OwnerOf(object); store != entt::null && registry.Valid(store))
			{
				id = abodeTown(store);
			}
		}
		if (id && !registry.Context().towns.contains(*id))
		{
			id.reset();
		}
		return id;
	};
	// The tree is out of the map while it is held or flying (fn_005DC330 0x5DC385, Object::InitialisePhysics
	// 0x6374BA: RemoveMapObject vt +0x548), so the search does not see it. (until the hand and physics hooks are in, it
	// can still be listed where it was picked up)
	map_cells::RemoveMapObject(tree);
	const auto treeCoords = map_coords::FromMetres(at);
	auto coords = treeCoords;
	map_coords::Spiral spiral;
	bool inTown = false;
	uint32_t forest = 0;
	float nearest = 99999.0f;
	for (int32_t left = k_SearchCells; left != 0; --left)
	{
		if (gutils::GetDistanceInMetres(treeCoords, coords) > k_SearchRadius)
		{
			break;
		}
		map_cells::ForEachFixed(map_coords::Cell(coords), [&](entt::entity other) {
			const auto& position = registry.Get<const Transform>(other).position;
			const float d =
			    gutils::GetDistanceInMetres(glm::vec2(position.x, position.z), at) - ecs::object::Get2DRadius(other);
			if (d < k_TownRadius)
			{
				const auto town = townOf(other);
				if (town || map_cells::TypeOf(other) == ObjectType::Citadel)
				{
					inTown = true;
					if (town)
					{
						if (const auto scenic = ecs::TownForestId(*town, transform.position); scenic != 0)
						{
							nearest = 0.0f;
							forest = scenic;
						}
					}
					return false;
				}
			}
			if (const auto* other_tree = registry.TryGet<const Tree>(other);
			    other_tree != nullptr && ecs::IsInForest(other_tree->forestId) && d < nearest)
			{
				nearest = d;
				forest = other_tree->forestId;
			}
			return true;
		});
		map_coords::AddCells(coords, spiral.Next());
	}
	// 0x74BB10..0x74BB62: Tree +0x5E bit 1 takes the "in a town" answer; the forest found, else outside a town a new
	// Forest at the tree (0x74BB53); in a town without a forest the tree stays without one
	component.isNonScenic = inTown;
	component.forestId = forest != 0 ? forest : inTown ? 0u : ecs::CreateForest(0, transform.position);
	// Tree::EndPhysics: a white SmokyStuff puff on the ground (the grip dust stands in for it) and, outside a town, the
	// SPOT_VISUAL_FOREST_CREATED effect (0x2C; the original also passes 0.3 and 50, whose meaning is not pinned down,
	// so the effect runs for its own life from the data).
	// GAlignment::Update(the dropper's player, tree, true) 0x74BBB6: planting is good. TODO: StartImmersion(0x2E) and
	// ConsiderMakingCreatureMimicPlayer.
	ecs::effects::alignment::UpdateForTree(PlayerNames::PLAYER_ONE, true);
	EmitGripDust(transform.position);
	if (!inTown)
	{
		psys::manager::CreateSpotVisual(static_cast<int>(SpotVisualType::ForestCreated), transform.position, 0.0f,
		                                entt::null);
	}
	// Fixed::EndPhysics 0x74BBCA -> Object::EndPhysics (0x52E054 / 0x52E0CB) -> InsertMapObject vt +0x544 (0x63762C):
	// back in its cell, at the head of the fixed list, after the search
	ecs::map_cells::InsertMapObject(tree);
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree replanted at ({:.1f}, {:.1f}), {} forest {}", at.x, at.y,
	                   inTown ? "town" : (forest ? "joined" : "new"), component.forestId);
}

void HandSystem::MakeDeadTree(entt::entity tree, glm::vec3 direction, bool placeLying) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(tree);
	// Tree::EndPhysics 0x74BBF0: a DeadTree takes over the tree's 3D object (and fire, fn_00730960) and the tree is
	// ToBeDeleted; DeadTree +0x9C keeps its GetWoodValueMultiplier
	const auto type = registry.Get<Tree>(tree).type;
	const float multiplier = registry.Get<Tree>(tree).woodValueMultiplier;
	ecs::NotifyTreeDeleted(tree, ecs::TreeDeletion::BecameDeadTree);
	// the tree's ToBeDeleted (0x74BC37) -> CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548
	ecs::map_cells::RemoveMapObject(tree);
	registry.Remove<Tree>(tree);
	registry.Assign<DeadTree>(tree, type, multiplier);
	if (!placeLying)
	{
		registry.SetDirty();
		UpdateRoots(tree, true);
		DropRoots(tree, true);
		// Tree::EndPhysics 0x74BBD9: fn_00510B70 makes the DeadTree and InsertMapObject vt +0x544 (0x74BC0A) puts it
		// in the map
		ecs::map_cells::InsertMapObject(tree);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree became a dead tree at ({:.1f}, {:.1f})", transform.position.x,
		                   transform.position.z);
		return;
	}
	// It keeps the tree mesh and comes to rest lying down, the crown towards where it was going.
	direction.y = 0.0f;
	direction = glm::length(direction) > 1e-4f ? glm::normalize(direction) : glm::vec3(0.0f, 0.0f, 1.0f);
	const auto axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), direction));
	// (inferido) no source for the lying-down turn: the crown goes towards `direction`, not checked against the original
	transform.rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), axis)) * transform.rotation;
	float trunk = 0.3f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(tree); mesh != nullptr && meshes.Contains(mesh->id))
	{
		const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size() * transform.scale;
		trunk = 0.2f * 0.5f * std::max(size.x, size.z);
	}
	transform.position.y =
	    Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z)) + trunk;
	if (auto* fixed = registry.TryGet<Fixed>(tree); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	registry.SetDirty();
	// DeadTree::Draw: the roots break off and fall to the ground (fn_00826280).
	UpdateRoots(tree, true);
	DropRoots(tree, true);
	// Tree::EndPhysics: InsertMapObject vt +0x544 of the DeadTree (0x74BC0A), once it lies where it rests
	ecs::map_cells::InsertMapObject(tree);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree became a dead tree at ({:.1f}, {:.1f})", transform.position.x,
	                   transform.position.z);
}

glm::vec3 HandSystem::GripCentre() const noexcept
{
	if (!_animator || _vertices.empty())
	{
		return _palmCenter;
	}
	const auto& bones = _animator->GetBoneMatrices();
	glm::vec3 palm(0.0f);
	glm::vec3 tips(0.0f);
	int palmCount = 0;
	int tipCount = 0;
	for (size_t v = 0; v < _vertices.size(); ++v)
	{
		const auto bone = _vertexBones[v];
		if (bone == 0)
		{
			palm += ModelPosition(v, bones);
			++palmCount;
		}
		else if (bone == 10 || bone == 13 || bone == 16 || bone == 19 || bone == 21)
		{
			tips += ModelPosition(v, bones);
			++tipCount;
		}
	}
	if (palmCount == 0 || tipCount == 0)
	{
		return _palmCenter;
	}
	return 0.5f * (palm / static_cast<float>(palmCount) + tips / static_cast<float>(tipCount));
}

void HandSystem::BeginTug(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(tree);
	_tug = tree;
	ComputeHoldParameters(tree);
	_tugPoint = transform.position;
	_tugRotation = transform.rotation;
	// where the hand took hold of it, and how far along the mouse ray it was
	_tugGrab = _interactionPoint.value_or(_tugPoint);
	_tugDepth = glm::distance(_mouseRayOrigin, _tugGrab);
	_hovered.reset();
}

void HandSystem::UpdateTug(float seconds, bool actionHeld) noexcept
{
	static_cast<void>(seconds);
	if (!_tug)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(*_tug) || !registry.AllOf<Tree>(*_tug))
	{
		_tug.reset();
		return;
	}
	auto& transform = registry.Get<Transform>(*_tug);
	if (!actionHeld)
	{
		// let go before it came out: it stays planted as it stood
		transform.rotation = _tugRotation;
		_tug.reset();
		registry.SetDirty();
		return;
	}
	// The tug as it was before the physics port (user, 2026-09-30: grabbed anywhere, the tree only leans until it is
	// pulled away): the pull is how far the hand has moved sideways since it took hold (the mouse ray at the depth of
	// the grab), and the tree comes out once it is over weight / 1000 (HandStateTug::Update: F = 1000 x distance against
	// GetWeight = scale^3 x info weight). The literal port of the spring (drag plane at the cursor's height, grip at
	// 0.1 x height) made a tree grabbed higher up come out at once; see the wiki.
	const float weight = physics::PhysicsObjects::Weight(*_tug);
	const auto hand = _mouseRayOrigin + _mouseRayDirection * _tugDepth;
	auto pull = glm::vec3(hand.x - _tugGrab.x, 0.0f, hand.z - _tugGrab.z);
	const float distance = glm::length(pull);
	const float threshold = std::max(0.01f, weight / 1000.0f);
	if (distance > threshold)
	{
		transform.rotation = _tugRotation;
		const auto tree = *_tug;
		_tug.reset();
		Uproot(tree);
		return;
	}
	// it leans towards the hand, up to 0.25 rad at the threshold
	transform.rotation = _tugRotation;
	if (distance > 1e-3f)
	{
		pull /= distance;
		const float lean = 0.25f * distance / threshold;
		const auto axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), pull));
		// (inferido) HandStateTug::Update turns with fn_007FB180 (lh_matrix::AxisAngle, 0x5B8700); its angle and axis are
		// not checked, so the lean keeps glm's +angle about up x pull
		transform.rotation = glm::mat3(glm::rotate(glm::mat4(1.0f), lean, axis)) * _tugRotation;
	}
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tug trace: hand ({:.2f},{:.2f}) pull {:.2f} / {:.2f}", hand.x, hand.z,
		                   distance, threshold);
	}
	registry.SetDirty();
}

void HandSystem::Uproot(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(tree);
	// fn_0074BD20: a roots pile (MeshPack TreeRootsPile) where the tree stood, scaled
	// (mesh extent x + extent z) * scale * 0.3, lasting 15 s, plus SmokyStuff (the grip dust stands in for it).
	auto& meshes = Locator::resources::value().GetMeshes();
	float extentX = 1.0f;
	float extentZ = 1.0f;
	if (const auto* mesh = registry.TryGet<const Mesh>(tree); mesh != nullptr && meshes.Contains(mesh->id))
	{
		// LH3DMesh +0x24 / +0x2C, taken as the half extents of the bounding box.
		const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size();
		extentX = 0.5f * size.x;
		extentZ = 0.5f * size.z;
	}
	// fn_008251F0 -> fn_00825240: a ground mark (ecs/GroundMarks.h) that melts into the land and fades after 15 s
	ecs::ground_marks::Create(transform.position, transform.rotation, (extentX + extentZ) * transform.scale.x * 0.3f);
	EmitGripDust(transform.position);
	PickUp(tree);
	UpdateRoots(tree);
}

void HandSystem::UpdateRoots(entt::entity tree, bool dying) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	// Only rooted objects (Object::IsARootedObject: living trees) have roots; a dying tree hands them over to the fall.
	if (!registry.Valid(tree) || (!dying && !registry.AllOf<Tree>(tree)))
	{
		return;
	}
	const auto rootsMesh = resources::HashIdentifier(MeshId::TreeRoots);
	auto& meshes = Locator::resources::value().GetMeshes();
	const auto* treeMesh = registry.TryGet<const Mesh>(tree);
	if (!meshes.Contains(rootsMesh) || treeMesh == nullptr || !meshes.Contains(treeMesh->id))
	{
		return;
	}
	auto it = std::find_if(_roots.begin(), _roots.end(), [tree](const auto& pair) { return pair.first == tree; });
	if (it == _roots.end())
	{
		const auto roots = registry.Create();
		registry.Assign<Transform>(roots, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mesh>(roots, rootsMesh, static_cast<int8_t>(0), static_cast<int8_t>(-1));
		_roots.emplace_back(tree, roots);
		it = std::prev(_roots.end());
	}
	// fn_00511270: the tree matrix with its rotation scaled by 0.15 * mesh extent (LH3DMesh +0x24).
	const auto& transform = registry.Get<Transform>(tree);
	const float extent = 0.5f * meshes.Handle(treeMesh->id)->GetBoundingBox().Size().x;
	auto& roots = registry.Get<Transform>(it->second);
	roots.position = transform.position;
	roots.rotation = transform.rotation;
	roots.scale = transform.scale * (0.15f * extent);
	registry.SetDirty();
}

void HandSystem::DropRoots(entt::entity tree, bool fall) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto it = std::find_if(_roots.begin(), _roots.end(), [tree](const auto& pair) { return pair.first == tree; });
	if (it == _roots.end())
	{
		return;
	}
	const auto roots = it->second;
	_roots.erase(it);
	if (!registry.Valid(roots))
	{
		return;
	}
	if (!fall)
	{
		registry.Destroy(roots);
		registry.SetDirty();
		return;
	}
	// fn_008263C0 / fn_008261D0: y = y0 - 20 t^2 down to altitude + 0.1 * extent * scale, gone after 20 s.
	auto& transform = registry.Get<Transform>(roots);
	float extent = 1.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(tree); mesh != nullptr && meshes.Contains(mesh->id))
	{
		extent = 0.5f * meshes.Handle(mesh->id)->GetBoundingBox().Size().x;
	}
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	_fallingRoots.push_back({roots, 0.0f, transform.position.y, ground + 0.1f * extent * transform.scale.x});
}

void HandSystem::UpdateRootsAndPiles(float seconds) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	bool dirty = false;
	for (auto& roots : _fallingRoots)
	{
		if (!registry.Valid(roots.entity))
		{
			roots.entity = entt::null;
			continue;
		}
		roots.age += seconds;
		auto& transform = registry.Get<Transform>(roots.entity);
		transform.position.y = std::max(roots.groundY, roots.startY - 20.0f * roots.age * roots.age);
		// fn_008261D0: alpha = 255 * (1 - (t - 18) / 2) from 18 s.
		if (roots.age > 18.0f)
		{
			registry.AssignOrReplace<Alpha>(roots.entity, std::max(0.0f, 1.0f - (roots.age - 18.0f) / 2.0f));
		}
		dirty = true;
		if (roots.age >= 20.0f)
		{
			registry.Destroy(roots.entity);
			roots.entity = entt::null;
		}
	}
	std::erase_if(_fallingRoots, [](const FallingRoots& roots) { return roots.entity == entt::null; });
	std::erase_if(_roots, [&registry](const auto& pair) {
		if (registry.Valid(pair.first))
		{
			return false;
		}
		if (registry.Valid(pair.second))
		{
			registry.Destroy(pair.second);
		}
		return true;
	});
	if (dirty)
	{
		registry.SetDirty();
	}
}

bool HandSystem::TakeTreeFromForest(entt::entity forestEntity) noexcept
{
	// BigForest::InterfaceSetInMagicHand 0x4393C0: RemoveResource(WOOD, Conifer woodValue 350), then a Conifer
	// (scale 1, angle 0, max size 1) is created at the hand and placed in it; the forest keeps no tug
	auto& registry = Locator::entitiesRegistry::value();
	auto* forest = registry.TryGet<BigForest>(forestEntity);
	if (forest == nullptr || !Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto& trees = Locator::infoConstants::value().tree;
	const float amount = static_cast<float>(trees.at(static_cast<size_t>(TreeInfo::Conifer)).woodValue);
	auto& forestTransform = registry.Get<Transform>(forestEntity);
	const auto forestPosition = forestTransform.position;
	// RemoveResource 0x4390D0 (ecs::BigForestRemoveWood: the forest shrinks, a Pine sapling at its edge, or it goes)
	const auto forestId = forest->forestId;
	ecs::BigForestRemoveWood(forestEntity, static_cast<uint32_t>(amount));
	registry.SetDirty();
	const auto point = _interactionPoint.value_or(forestPosition);
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z));
	const auto tree =
	    archetypes::TreeArchetype::Create(forestId, glm::vec3(point.x, ground, point.z), TreeInfo::Conifer, false, 0.0f, 1.0f,
	                                      1.0f);
	if (tree == entt::null)
	{
		return false;
	}
	PickUp(tree);
	return _held.has_value();
}
