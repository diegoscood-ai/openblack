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
#include "Audio/AudioManagerInterface.h"
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
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Physics/PhysOb.h"
#include "ECS/Registry.h"
#include "ECS/Alignment.h"
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

void HandSystem::ReleaseTree(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(tree);
	// Object::InitialisePhysicsFromHand 0x636F00: a released tree only counts as "gently put down" (the physics object's
	// flag 8, the one Tree::EndPhysics asks for) when the landscape normal under it points up (y >= 0.7, a slope under
	// about 45 degrees) and the tree comes down almost upright: the x and z angles of its YXZ matrix within 0.2 rad
	// (about 11.5 degrees). A held tree takes the hand's up axis, which follows the surface, so it leans on a slope.
	// Anything else falls with physics and ends up a DeadTree (the handler in HandPhysics.cpp).
	float yAngle = 0.0f;
	float xAngle = 0.0f;
	float zAngle = 0.0f;
	glm::extractEulerAngleYXZ(glm::mat4(transform.rotation), yAngle, xAngle, zAngle);
	constexpr float k_UprightAngle = 0.2f;
	constexpr float k_FlatNormal = 0.7f;
	if (physics::LandscapeNormal(transform.position).y < k_FlatNormal || std::abs(xAngle) > k_UprightAngle ||
	    std::abs(zAngle) > k_UprightAngle)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Hand: tree dropped on a slope or leaning (x {:.2f} z {:.2f}): it falls",
		                    xAngle, zAngle);
		physics::PhysicsObjects::AddObject(tree, glm::vec3(0.0f), glm::vec3(0.0f), entt::null, true);
		return;
	}
	// Tree::EndPhysics: planted again only on dry land (and not burning, TODO: fire); otherwise a DeadTree.
	const bool land = IsLand(transform.position);
	if (land)
	{
		Replant(tree);
	}
	else
	{
		const float angle = Locator::rng::value().NextValue(0.0f, glm::two_pi<float>());
		MakeDeadTree(tree, glm::vec3(std::sin(angle), 0.0f, std::cos(angle)));
	}
	// PhysicsObject::RemoveObject 0x646B44 calls Tree::DropSfx 0x74BC60 for every gentle release that ends on land,
	// replanted or not (the original picks the sample by GetTickCount() % 3).
	if (land)
	{
		static constexpr auto k_PlantTree = std::array<audio::SoundId, 3> {
		    audio::SoundId::G_PlantTree_01, audio::SoundId::G_PlantTree_02, audio::SoundId::G_PlantTree_03};
		PlaySample(Locator::rng::value().Choose(k_PlantTree));
	}
}

void HandSystem::Replant(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(tree);
	auto& component = registry.Get<Tree>(tree);
	DropRoots(tree, false);
	// Fixed::EndPhysics puts it back in its map cell (InsertMapObject: at the head of the cell's list)
	component.mapInsertion = ecs::NextMapInsertion();
	transform.position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	if (auto* fixed = registry.TryGet<Fixed>(tree); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	const glm::vec2 at(transform.position.x, transform.position.z);

	// Tree::EndPhysics 0x74B8BF: a spiral over the map cells, stopping at 25 + 10 m. For every fixed object in those
	// cells d = its distance minus its own 2D radius; an object that belongs to a town (or a citadel part) within 25 m
	// means the tree was planted "in a town" and it joins that town's forest, which beats any other. Otherwise the
	// nearest tree that has a forest lends its forest (with no distance limit of its own, only the 35 m of the search),
	// and a tree with neither, outside a town, starts a new forest.
	// Deviation: the original takes the town's forest from a list the town keeps (Town +0x608); openblack does not
	// model that list, so the first tree planted in a town starts the town's forest (ecs::TownForestId).
	constexpr float k_SearchRadius = 35.0f;
	constexpr float k_TownRadius = 25.0f;
	bool inTown = false;
	uint32_t townId = 0;
	std::optional<uint32_t> forest;
	float nearest = std::numeric_limits<float>::max();
	registry.Each<const Transform>([&](entt::entity other, const Transform& position) {
		if (other == tree)
		{
			return;
		}
		const float cellDistance = glm::distance(at, glm::vec2(position.position.x, position.position.z));
		if (cellDistance > k_SearchRadius)
		{
			return;
		}
		const auto* fixed = registry.TryGet<const Fixed>(other);
		const float d = cellDistance - (fixed != nullptr ? fixed->boundingRadius : 0.0f);
		if (d < k_TownRadius)
		{
			// Object::GetTown: every town building is an Abode here (storage pits and town centres included).
			if (const auto* abode = registry.TryGet<const Abode>(other); abode != nullptr)
			{
				inTown = true;
				townId = abode->townId;
				return;
			}
		}
		const auto* other_tree = registry.TryGet<const Tree>(other);
		if (other_tree != nullptr && ecs::IsInForest(other_tree->forestId) && d < nearest)
		{
			nearest = d;
			forest = other_tree->forestId;
		}
	});
	// Tree +0x5E bit 1 (0x74BB5A) takes the "in a town" answer.
	component.isNonScenic = inTown;
	component.forestId = inTown ? ecs::TownForestId(townId, transform.position)
	                             : forest.value_or(0u) != 0 ? *forest : ecs::CreateForest(0, transform.position);
	// Tree::EndPhysics: a white SmokyStuff puff on the ground (the grip dust stands in for it) and, outside a town, the
	// SPOT_VISUAL_FOREST_CREATED effect (0x2C; the original also passes 0.3 and 50, whose meaning is not pinned down,
	// so the effect runs for its own life from the data).
	// GAlignment::Update(the dropper's player, tree, true) 0x74BBB6: planting is good. TODO: StartImmersion(0x2E) and
	// ConsiderMakingCreatureMimicPlayer.
	ecs::alignment::UpdateForTree(PlayerNames::PLAYER_ONE, true);
	EmitGripDust(transform.position);
	if (!inTown)
	{
		psys::manager::CreateSpotVisual(static_cast<int>(SpotVisualType::ForestCreated), transform.position, 0.0f,
		                                entt::null);
	}
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree replanted at ({:.1f}, {:.1f}), {} forest {}", at.x, at.y,
	                   inTown ? "town" : (forest ? "joined" : "new"), component.forestId);
}

void HandSystem::MakeDeadTree(entt::entity tree, glm::vec3 direction, bool placeLying) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(tree);
	const auto type = registry.Get<Tree>(tree).type;
	registry.Remove<Tree>(tree);
	registry.Assign<DeadTree>(tree, type);
	if (!placeLying)
	{
		registry.SetDirty();
		UpdateRoots(tree, true);
		DropRoots(tree, true);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree became a dead tree at ({:.1f}, {:.1f})", transform.position.x,
		                   transform.position.z);
		return;
	}
	// It keeps the tree mesh and comes to rest lying down, the crown towards where it was going.
	direction.y = 0.0f;
	direction = glm::length(direction) > 1e-4f ? glm::normalize(direction) : glm::vec3(0.0f, 0.0f, 1.0f);
	const auto axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), direction));
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
	const auto pileMesh = resources::HashIdentifier(MeshId::TreeRootsPile);
	if (meshes.Contains(pileMesh))
	{
		const auto pile = registry.Create();
		const float scale = (extentX + extentZ) * transform.scale.x * 0.3f;
		registry.Assign<Transform>(pile, transform.position, transform.rotation, glm::vec3(scale));
		registry.Assign<Mesh>(pile, pileMesh, static_cast<int8_t>(0), static_cast<int8_t>(-1));
		// fn_00825240: LH3DObject::Create(1), a morphable object whose deltas UpdateMelting takes once (vt+0x1E8), so
		// the crater follows the land under it
		registry.Assign<MorphWithTerrain>(pile);
		_rootsPiles.emplace_back(pile, 15.0f);
	}
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
	// fn_00825350: the pile fades out over its last second (alpha = life * 0.255 of 255).
	for (auto& [pile, life] : _rootsPiles)
	{
		life -= seconds;
		if (life < 1.0f && life > 0.0f && registry.Valid(pile))
		{
			registry.AssignOrReplace<Alpha>(pile, life);
			dirty = true;
		}
		if (life <= 0.0f && registry.Valid(pile))
		{
			registry.Destroy(pile);
			pile = entt::null;
			dirty = true;
		}
	}
	std::erase_if(_rootsPiles, [](const auto& pair) { return pair.first == entt::null; });
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
