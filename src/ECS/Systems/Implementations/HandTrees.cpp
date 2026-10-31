/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <fstream>
#include <tuple>

#include <L3DFile.h>
#include <LNDFile.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Common/GUtilsDistance.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TreeRoots.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureMimic.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/GroundMarks.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/TreeReplant.h"
#include "ECS/Trees.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/Texture2D.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "InfoConstants.h"
#include "Input/HandDemo.h"
#include "Locator.h"
#include "Particles/ParticleManager.h"
#include "Physics/Body.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

bool HandSystem::IsHoldingTree() const noexcept
{
	// Trees and dead trees are held as HOLD_TYPE_TREE.
	return _held && ecs::IsAvailable(*_held) && Locator::entitiesRegistry::value().AnyOf<Tree, DeadTree>(*_held);
}

void HandSystem::Replant(entt::entity tree, std::optional<PlayerNames> player) noexcept
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
	// The steps in the original's order (tree_replant::k_Order). First the brown DisappearSmoke puff on the ground under
	// the tree
	ecs::disappear_smoke::Create(
	    glm::vec3(transform.position.x,
	              Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z)),
	              transform.position.z),
	    1, 1.0f, 0xFFFFFFFFu);
	// Then the plant-object feel on the dropper's mouse (tree_replant::k_Immersion): openblack drives no force feedback

	// Then the tree looks for its forest: a spiral over a copy of the tree's map coordinates, at most 1000 cells,
	// that stops at the first cell farther than 25 + 10 m. It walks only the fixed list of each cell. For each object
	// d = its distance to the tree minus its 2D radius:
	// - d < 25 and a town (the object's town) or a citadel part: in a town; with a town every scenic forest of its list
	//   becomes the best at distance 0, so the last one wins (ecs::TownForestId); then the next CELL;
	// - else a Tree with a forest nearer than the best (from 99999) lends its forest.
	// The later cells go on with the same best, after a town too.
	namespace map_cells = ecs::map_cells;
	namespace map_coords = openblack::map_coords;
	constexpr int32_t k_SearchCells = 1000;
	constexpr float k_TownRadius = 25.0f;
	constexpr float k_SearchRadius = k_TownRadius + 10.0f;
	// The town of the fixed list's classes: an abode's (storage pits and town centres are abodes here), a field's, a
	// fish farm's, a totem statue's (its town centre's), a store pile's (the store's), a town desire flag's; any other
	// object: none
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
			if (ecs::IsAvailable(farm->town) && registry.AllOf<components::Town>(farm->town))
			{
				id = registry.Get<const components::Town>(farm->town).id;
			}
		}
		else if (const auto* totem = registry.TryGet<const components::VillageTotem>(object); totem != nullptr)
		{
			if (ecs::IsAvailable(totem->townCentre))
			{
				id = abodeTown(totem->townCentre);
			}
		}
		else if (registry.AllOf<components::Pot>(object))
		{
			if (const auto store = ecs::StoragePitStore::OwnerOf(object); ecs::IsAvailable(store))
			{
				id = abodeTown(store);
			}
		}
		else if (const auto* flag = registry.TryGet<const components::TownDesireFlag>(object); flag != nullptr)
		{
			if (ecs::IsAvailable(flag->town) && registry.AllOf<components::Town>(flag->town))
			{
				id = registry.Get<const components::Town>(flag->town).id;
			}
		}
		if (id && !registry.Context().towns.contains(*id))
		{
			id.reset();
		}
		return id;
	};
	// The tree is out of the map while it is held or flying, so the search does not see it. (until the hand and physics
	// hooks are in, it can still be listed where it was picked up)
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
	// The tree's non-scenic flag takes the "in a town" answer; the forest found, else outside a town a new forest at the
	// tree; in a town without a forest the tree stays without one
	component.isNonScenic = inTown;
	component.forestId = forest != 0 ? forest : inTown ? 0u : ecs::CreateForest(0, transform.position);
	// Outside a town, the SPOT_VISUAL_FOREST_CREATED spot visual at magnitude 0.3 for 50 turns, given the body's player as
	// the original gives it (its spell file colours nothing by the player); without one it keeps none
	if (tree_replant::Runs(tree_replant::Step::SpotVisual, {.inTown = inTown}))
	{
		auto& particles = Locator::particleSystem::value();
		const auto visual = particles.StartSpotVisual(SpotVisualType::ForestCreated, transform.position, 50, entt::null, 0.3f);
		if (player.has_value())
		{
			particles.SetSpotVisualPlayer(visual, static_cast<int>(*player));
		}
	}
	// After the spot visual, with a player, that player's creature may learn to copy planting a tree, and only then that
	// player's alignment for it: planting is good
	if (tree_replant::Runs(tree_replant::Step::Alignment, {.player = player.has_value()}))
	{
		ecs::creature_mimic::Consider(*player, creature_watching::Deed::PlantTree, tree);
		ecs::effects::alignment::UpdateForTree(*player, true);
	}
	// The landing puts it back in its cell, at the head of the fixed list, after the search
	ecs::map_cells::InsertMapObject(tree);
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree replanted at ({:.1f}, {:.1f}), {} forest {}", at.x, at.y,
	                   inTown ? "town" : (forest ? "joined" : "new"), component.forestId);
}

void HandSystem::MakeDeadTree(entt::entity tree, glm::vec3 direction, bool placeLying, PlayerNames player) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(tree);
	ecs::TreeBecomesDeadTree(tree, player);
	if (!placeLying)
	{
		registry.SetDirty();
		UpdateRoots(tree, true);
		DropRoots(tree, true);
		// The dead tree is made and put in the map
		ecs::map_cells::InsertMapObject(tree);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: tree became a dead tree at ({:.1f}, {:.1f})", transform.position.x,
		                   transform.position.z);
		return;
	}
	// It keeps the tree mesh and comes to rest lying down, the crown towards where it was going.
	direction.y = 0.0f;
	direction = glm::length(direction) > 1e-4f ? glm::normalize(direction) : glm::vec3(0.0f, 0.0f, 1.0f);
	const auto axis = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), direction));
	// (inferred) no source for the lying-down turn: the crown goes towards `direction`, not checked against the original
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
	// The roots break off and fall to the ground.
	UpdateRoots(tree, true);
	DropRoots(tree, true);
	// The dead tree goes into the map once it lies where it rests
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
	_tugFreed.reset();
	ComputeHoldParameters(tree);
	// The pull's plane, set once: through the tree's base along the land there, at the height the line from the eye
	// through the hand (where the hand was before the press) has once it is as far across the ground as the base. The
	// hand was on the pointer's line, so the plane is where that line crosses the tree. (approximate) taken at the press,
	// with the eye of the press's frame and the hand as the frame before placed it; the original takes them as its
	// pulling state starts, the next frame, with the hand as the press's frame placed it
	const auto hand = registry.Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]).position;
	const glm::vec2 at(transform.position.x, transform.position.z);
	auto& pull = _tugPull.emplace();
	pull.plane =
	    hand_grab::TugPlaneAt(_mouseRayOrigin, hand, transform.position, Locator::terrainSystem::value().GetNormalAt(at));
	// The tree's matrix as it stands, its axes scaled, and the first grip: its own hang, no closer than a share of the
	// hand's height (the pulling state's first frame grips it at its own hang again, before any pull)
	pull.tug = hand_grab::TugOf(transform.rotation, transform.scale, transform.position);
	pull.holdDistance = hand_grab::HoldDistance(_loweringMultiplier, ecs::object::GetHeight(tree), _handScale);
	pull.stretch.SetPosition(1.0f);
	_tugHandPoint.reset();
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
	// The tug writes nothing to the tree: its lean is the tug matrix re-applied to the drawn object after the tree is
	// drawn, a HandDrawPose here; leaving the state, nothing is restored: the tree is drawn upright again from its logic
	const auto endTug = [this, &registry]() {
		if (registry.Valid(*_tug) && registry.AllOf<HandDrawPose>(*_tug))
		{
			registry.Remove<HandDrawPose>(*_tug);
			registry.SetDirty();
		}
		_tug.reset();
		_tugPull.reset();
		_tugHandPoint.reset();
	};
	if (!ecs::IsAvailable(*_tug) || !registry.AllOf<Tree>(*_tug) || !_tugPull)
	{
		endTug();
		return;
	}
	if (!actionHeld)
	{
		endTug(); // let go before it came out: it stays planted as it stood
		return;
	}
	// The hand's pulling state starts the frame after the press, as the hand changes state before it sees the press;
	// until then the tree is drawn as it stands
	if (_renderHandState != 3)
	{
		return;
	}
	auto& pull = *_tugPull;
	// The frame's time as the hand's states count it: the camera's time step, the real frame time inside the citadel
	const auto ms =
	    game_clock::IsInsideCitadel() ? game_clock::FrameRealMs() : game_clock::CameraFrameMs(hand_demo::IsPlaying());
	const float dt = static_cast<float>(ms) * 0.001f;
	const float height = ecs::object::GetHeight(*_tug);
	if (hand_grab::StepTugFade(pull.fade, dt))
	{
		// The hand is where the pointer's line meets the plane; a spring from the grip to it pulls the tree, which comes
		// out once the hand is strong enough to lift it and pulls harder than it weighs (scale^3 x info weight), and
		// otherwise leans about its base towards the hand and stretches towards it, at most 1.3 times, over 0.3 s
		const auto hand = hand_grab::TugHandOnPlane(pull.plane, _mouseRayOrigin, _mouseRayDirection);
		const float weight = Locator::dynamicsSystem::value().WeightOf(*_tug);
		const auto result = hand_grab::PullAt(pull.tug, {.hand = hand,
		                                                 .holdDistance = pull.holdDistance,
		                                                 .maxForce = hand_grab::MaxForce(0.0f),
		                                                 .weight = weight,
		                                                 .height = height,
		                                                 .tree = true,
		                                                 .leans = true,
		                                                 .seconds = dt});
		pull.stretch.SetDestinationWithSpeedAndTime(result.stretchTarget, 0.0f, hand_grab::k_TugStretchSeconds);
		pull.stretch.UpdateInlineValue(dt);
		if (debug_env::TraceLogs(debug_env::HandTrace(), "game"))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Tug trace: hand ({:.2f},{:.2f},{:.2f}) pull {:.1f} / {:.1f} stretch {:.3f}", hand.x, hand.y,
			                   hand.z, glm::length(result.force), weight, pull.stretch.GetValue());
		}
		// From the first pull on the tree is drawn as it leans and stretches, in place of its own sway; during the fade
		// it is drawn as it stands. The frame it comes free it still leans by this frame's pull
		auto& pose = registry.AssignOrReplace<HandDrawPose>(*_tug);
		pose.position = pull.tug.base;
		pose.rotation = hand_grab::TugRotation(pull.tug);
		pose.upStretch = pull.stretch.GetValue();
		if (result.comesFree)
		{
			const auto tree = *_tug;
			// pulled free, the tree is held: the grab's wait is over, and letting go of it is no tap now
			if (_pendingPick && *_pendingPick == tree)
			{
				_pendingPick.reset();
				_pendingTree = false;
			}
			// The tug is over, so the next frame's state is the holding one. Until then the frame is still the pulling
			// state's: the tree drawn as it leans and the hand at its grip, as on every pull
			pull.holdDistance = height * _loweringMultiplier;
			_tugHandPoint = hand_grab::TugHandPoint(pull.tug, pull.holdDistance, pull.stretch.GetValue());
			_tugFreed = tree;
			_tug.reset();
			_tugPull.reset();
			registry.SetDirty();
			Uproot(tree);
			return;
		}
	}
	// Every frame of the state, the fade's too: the grip back at the tree's own hang (no least hang now), and the hand
	// drawn at the grip of the tree as it leans and stretches
	pull.holdDistance = height * _loweringMultiplier;
	_tugHandPoint = hand_grab::TugHandPoint(pull.tug, pull.holdDistance, pull.stretch.GetValue());
	registry.SetDirty();
}

void HandSystem::Uproot(entt::entity tree) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(tree);
	// A roots pile (MeshPack TreeRootsPile) where the tree stood, scaled (mesh extent x + extent z) * scale * 0.3,
	// lasting 15 s; making it throws up the uproot's own white dust (the ground mark's, disappear_smoke mode 1).
	auto& meshes = Locator::resources::value().GetMeshes();
	float extentX = 1.0f;
	float extentZ = 1.0f;
	if (const auto* mesh = registry.TryGet<const Mesh>(tree); mesh != nullptr && meshes.Contains(mesh->id))
	{
		// The mesh extents, taken as the half extents of the bounding box.
		const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size();
		extentX = 0.5f * size.x;
		extentZ = 0.5f * size.z;
	}
	// A ground mark (ecs/GroundMarks.h) that melts into the land and fades after 15 s
	ecs::ground_marks::Create(transform.position, transform.rotation, (extentX + extentZ) * transform.scale.x * 0.3f);
	// The tug ends with only the mark and the hand's tug cleared; the next state then picks up the tree: packet 0x13
	// with the tree, state 7. The tree leaves the ground in the handler (ApplyPlaceInHand). (approximate) during that
	// wait of up to one turn the original's hand already draws the object held (every pick-up but the forest's, whose
	// tree is made in the handler); here it stays where it was. (pending) a draw-only pose in the hand while the 0x13
	// waits
	SendPlaceInHand(tree);
}

void HandSystem::UpdateRoots(entt::entity tree, bool dying) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	// Only rooted objects (living trees) have roots; a dying tree hands them over to the fall.
	if (!ecs::IsAvailable(tree) || (!dying && !registry.AllOf<Tree>(tree)))
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
		registry.Assign<TreeRoots>(roots, tree); // drawn from the tree's drawn matrix
		_roots.emplace_back(tree, roots);
		it = std::prev(_roots.end());
	}
	// The tree matrix with its rotation scaled by 0.15 * mesh extent.
	const auto& transform = registry.Get<Transform>(tree);
	const float extent = 0.5f * meshes.Handle(treeMesh->id)->GetBoundingBox().Size().x;
	auto& roots = registry.Get<Transform>(it->second);
	roots.position = transform.position;
	roots.rotation = transform.rotation;
	roots.scale = transform.scale * (0.15f * extent);
	registry.Get<TreeRoots>(it->second).factor = 0.15f * extent; // the same float, for the drawing
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
	if (registry.Valid(it->second) && registry.AllOf<TreeRoots>(it->second))
	{
		registry.Remove<TreeRoots>(it->second); // from here on they fall by themselves
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
	// y = y0 - 20 t^2 down to altitude + 0.1 * extent * scale, gone after 20 s.
	auto& transform = registry.Get<Transform>(roots);
	// They start from the dead tree's own matrix: copied at its first draw, with its rotation scaled by 0.15 x extent;
	// that matrix is the one the dead tree is drawn with, rebuilt when its scale is set, never the physics' in-between
	// pose. Here: the tree's Transform after MakeDeadTree (UpdateRoots just copied it), which RenderingSystem draws the
	// dead tree with. (pending) the dead tree's own angles (a scale of 1 keeps the yaw)
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
		// alpha = 255 * (1 - (t - 18) / 2) from 18 s.
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
		if (ecs::IsAvailable(pair.first))
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

entt::entity HandSystem::CreateTreeFromForest(entt::entity forestEntity) noexcept
{
	// A big forest put in the hand: it gives up a Conifer's wood value (350), then a Conifer at the forest's position, of
	// its forest, size 1.0, angle 0.0, maximum 1.0; the hand takes it in the caller (ApplyPlaceInHand); the forest keeps
	// no tug
	auto& registry = Locator::entitiesRegistry::value();
	auto* forest = registry.TryGet<BigForest>(forestEntity);
	if (forest == nullptr || !Locator::terrainSystem::has_value())
	{
		return entt::null;
	}
	const auto& trees = Locator::infoConstants::value().tree;
	const float amount = static_cast<float>(trees.at(static_cast<size_t>(TreeInfo::Conifer)).woodValue);
	auto& forestTransform = registry.Get<Transform>(forestEntity);
	const auto forestPosition = forestTransform.position;
	// ecs::BigForestRemoveWood: the forest shrinks, a Pine sapling at its edge, or it goes
	const auto forestId = forest->forestId;
	ecs::BigForestRemoveWood(forestEntity, static_cast<uint32_t>(amount));
	registry.SetDirty();
	// At the forest's map coordinates, not the hand's point
	const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(forestPosition.x, forestPosition.z));
	return archetypes::TreeArchetype::Create(forestId, glm::vec3(forestPosition.x, ground, forestPosition.z), TreeInfo::Conifer,
	                                         false, 0.0f, 1.0f, 1.0f);
}
