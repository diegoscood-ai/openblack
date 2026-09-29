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
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
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
	// Tree::EndPhysics: planted again only on dry land (and not burning, TODO: fire); otherwise a DeadTree.
	if (IsLand(transform.position))
	{
		Replant(tree);
	}
	else
	{
		const float angle = Locator::rng::value().NextValue(0.0f, glm::two_pi<float>());
		MakeDeadTree(tree, glm::vec3(std::sin(angle), 0.0f, std::cos(angle)));
	}
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

	// Within 25 m of a town building the tree becomes scenic (foresters leave it alone) and joins no forest.
	constexpr float k_TownRadius = 25.0f;
	bool nearTown = false;
	registry.Each<const Abode, const Transform>([&](entt::entity, const Abode&, const Transform& abode) {
		nearTown = nearTown || glm::distance(at, glm::vec2(abode.position.x, abode.position.z)) < k_TownRadius;
	});
	// Otherwise it joins the forest of the nearest tree within 25 + 10 m, or starts a new forest.
	constexpr float k_ForestRadius = 35.0f;
	std::optional<uint32_t> forest;
	uint32_t maxForest = 0;
	float nearest = k_ForestRadius;
	registry.Each<const Tree, const Transform>([&](entt::entity other, const Tree& t, const Transform& position) {
		// Scripts create scenic trees with forest -1 (and 0): no forest.
		const bool inForest = t.forestId != 0 && t.forestId != std::numeric_limits<uint32_t>::max();
		if (!inForest)
		{
			return;
		}
		maxForest = std::max(maxForest, t.forestId);
		const float distance = glm::distance(at, glm::vec2(position.position.x, position.position.z));
		if (other != tree && distance < nearest)
		{
			nearest = distance;
			forest = t.forestId;
		}
	});
	component.isNonScenic = !nearTown;
	if (nearTown)
	{
		component.forestId = 0;
	}
	else
	{
		// TODO: SPOT_VISUAL_FOREST_CREATED when a new forest is started.
		component.forestId = forest.value_or(maxForest + 1);
	}
	// Tree::DropSfx: LH_SAMPLE_G_PLANTTREE_01 + GetTickCount() % 3.
	// TODO: SmokyStuff, alignment (+treePullPutAlignmentChange).
	static constexpr auto k_PlantTree = std::array<audio::SoundId, 3> {
	    audio::SoundId::G_PlantTree_01, audio::SoundId::G_PlantTree_02, audio::SoundId::G_PlantTree_03};
	PlaySample(Locator::rng::value().Choose(k_PlantTree));
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree replanted at ({:.1f}, {:.1f}), {} forest {}", at.x, at.y,
	                   nearTown ? "scenic (town)," : (forest ? "joined" : "new"), component.forestId);
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
	// HandStateTug::Enter (0x5B7DF0)
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(tree);
	_tug = tree;
	ComputeHoldParameters(tree);
	_tugPoint = transform.position;
	_tugRotation = transform.rotation;
	_tugPlantedRotation = transform.rotation;
	_tugNormal = physics::LandscapeNormal(_tugPoint);
	// the drag plane: through the anchor, at the hand's height seen at the anchor's horizontal distance from the camera
	const auto hand = _interactionPoint.value_or(_tugPoint);
	const auto camera = Locator::camera::has_value() ? Locator::camera::value().GetOrigin() : hand + glm::vec3(0.0f, 10.0f, 0.0f);
	const float dHand = glm::length(glm::vec2(hand.x - camera.x, hand.z - camera.z));
	const float dGrip = glm::length(glm::vec2(_tugPoint.x - camera.x, _tugPoint.z - camera.z));
	const float y = dHand > 1e-4f ? camera.y - (camera.y - hand.y) * dGrip / dHand : hand.y;
	_tugPlanePoint = glm::vec3(_tugPoint.x, y, _tugPoint.z);
	// the grip height: GetHoldLoweringMultiplier x GetHeight, at least 3.2 x hand scale x 0.3
	_tugLowering = std::max(_loweringMultiplier * _heldHeight, 3.2f * _handScale * 0.3f);
	_tugOmega = glm::vec3(0.0f);
	_tugTime = 0.0f;
	_tugStretch.SetPosition(1.0f);
	_hovered.reset();
}

void HandSystem::UpdateTug(float seconds, bool actionHeld) noexcept
{
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
		// let go before it came out: the tug matrix was only its drawing, the tree stands as it was
		transform.rotation = _tugPlantedRotation;
		_tug.reset();
		registry.SetDirty();
		return;
	}
	// HandStateTug::Update (0x5B8070): no forces while the state blend runs (CHand+0x49B0 < 0.13)
	_tugTime += seconds;
	if (_tugTime < 0.13f || seconds <= 0.0f)
	{
		return;
	}
	// 1. the hand follows the mouse ray on the drag plane
	const float along = glm::dot(_mouseRayDirection, _tugNormal);
	if (std::abs(along) < 1e-6f)
	{
		return;
	}
	const float t = glm::dot(_tugPlanePoint - _mouseRayOrigin, _tugNormal) / along;
	const auto handPosition = _mouseRayOrigin + _mouseRayDirection * t;
	// 2. the grip on the trunk: base + up x lowering
	glm::mat3 axes = _tugRotation;
	for (int k = 0; k < 3; ++k)
	{
		axes[k] = glm::normalize(axes[k]);
	}
	const auto grip = _tugPoint + axes[1] * _tugLowering;
	// 3. a spring from the grip to the hand, at most GetMaxForce ((1 + CHand+0xAC) x 600000; +0xAC taken as 0)
	const auto d = handPosition - grip;
	const float distance = glm::length(d);
	constexpr float k_MaxForce = 600000.0f;
	const auto force = k_MaxForce > 1000.0f * distance ? d * 1000.0f : d * (k_MaxForce / distance);
	const float weight = physics::PhysicsObjects::Weight(*_tug); // GetWeight: scale^3 x info weight
	// 4. the trunk stretches along its up axis (at most 1.3, in 0.3 s)
	const float stretch = std::min((distance + _tugLowering) / _tugLowering, 1.3f);
	_tugStretch.SetDestinationWithSpeedAndTime(stretch, 0.0f, 0.3f);
	_tugStretch.Update(seconds);
	// 5. it comes out once |F| > weight (GetMaxForce >= weight x g always holds for trees)
	if (k_MaxForce >= weight * 9.81f && glm::length(force) > weight)
	{
		transform.rotation = _tugPlantedRotation;
		const auto tree = *_tug;
		_tug.reset();
		Uproot(tree);
		return;
	}
	// 6. it turns about its base: torque (F x r) / 1000 with quadratic drag 4; the original's F x r and its rotation of
	// the matrix rows flip the sign twice, so here r x F and a plain rotation give the same turn
	const auto torque = glm::cross(grip - _tugPoint, force) / 1000.0f;
	const float drag = glm::length(_tugOmega) * 4000.0f * seconds / 1000.0f;
	_tugOmega += torque * seconds - _tugOmega * drag;
	const auto step = _tugOmega * seconds;
	const float angle = glm::length(step);
	if (angle > 0.0f)
	{
		axes = glm::mat3(glm::rotate(glm::mat4(1.0f), angle, step / angle)) * axes;
	}
	_tugRotation = axes;
	// the drawn matrix: up stretched (the hand sits on base + stretched up x lowering, HandPlacement)
	transform.rotation = glm::mat3(axes[0], axes[1] * _tugStretch.value, axes[2]);
	// 7. from here the grip height is GetHoldLoweringMultiplier x GetHeight, not clamped again
	_tugLowering = _loweringMultiplier * _heldHeight;
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Tug trace: hand ({:.2f},{:.2f},{:.2f}) grip ({:.2f},{:.2f},{:.2f}) |F| {:.0f} / {:.0f} tilt {:.3f}",
		                   handPosition.x, handPosition.y, handPosition.z, grip.x, grip.y, grip.z, glm::length(force), weight,
		                   std::acos(std::clamp(axes[1].y, -1.0f, 1.0f)));
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
	const float forestRadius = [&]() {
		const auto* mesh = registry.TryGet<const Mesh>(forestEntity);
		auto& meshes = Locator::resources::value().GetMeshes();
		if (mesh == nullptr || !meshes.Contains(mesh->id))
		{
			return 5.0f;
		}
		const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size() * forestTransform.scale;
		return 0.5f * std::max(size.x, size.z);
	}();
	// RemoveResource 0x4390D0 (life 1): all that is left when it has no more, and the forest goes; otherwise it
	// shrinks to wood / woodValue and a sapling grows at its edge (AddTreeAround 0x439220)
	if (forest->wood <= amount)
	{
		registry.Destroy(forestEntity);
	}
	else
	{
		forest->wood -= amount;
		forestTransform.scale = glm::vec3(forest->wood / forest->woodValue);
		// AddTreeAround: up to 10 random angles at the forest's radius; on land and with no object whose distance plus
		// radius is under 4, a Pine (scale 0.05, random angle, max size 0.5 + FloatRand(0.5)) of the forest
		auto& rng = Locator::rng::value();
		const auto& island = Locator::terrainSystem::value();
		for (int attempt = 0; attempt < 10; ++attempt)
		{
			const float angle = rng.NextValue(0.0f, glm::two_pi<float>());
			const glm::vec3 point(forestPosition.x + std::cos(angle) * forestRadius, 0.0f,
			                      forestPosition.z + std::sin(angle) * forestRadius);
			if (!IsLand(point))
			{
				continue;
			}
			bool blocked = false;
			registry.Each<const Transform, const Mesh>([&](entt::entity other, const Transform& t, const Mesh&) {
				if (blocked || other == forestEntity)
				{
					return;
				}
				const float d = glm::distance(glm::vec2(t.position.x, t.position.z), glm::vec2(point.x, point.z));
				blocked = d < 4.0f;
			});
			if (blocked)
			{
				continue;
			}
			const float ground = island.GetHeightAt(glm::vec2(point.x, point.z));
			archetypes::TreeArchetype::Create(0, glm::vec3(point.x, ground, point.z), TreeInfo::Pine, false,
			                                  rng.NextValue(0.0f, glm::two_pi<float>()), 0.5f + rng.NextValue(0.0f, 0.5f), 0.05f);
			break;
		}
	}
	registry.SetDirty();
	const auto point = _interactionPoint.value_or(forestPosition);
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(point.x, point.z));
	const auto tree =
	    archetypes::TreeArchetype::Create(0, glm::vec3(point.x, ground, point.z), TreeInfo::Conifer, false, 0.0f, 1.0f, 1.0f);
	if (tree == entt::null)
	{
		return false;
	}
	PickUp(tree);
	return _held.has_value();
}
