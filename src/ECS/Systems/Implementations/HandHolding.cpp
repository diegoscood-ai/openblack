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
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/WaterRings.h"
#include "ECS/Registry.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/StoragePitStore.h"
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

void HandSystem::PickUp(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	_pickSource.reset();
	_pickFish = false;
	_pickField = false;
	_pickTime = 0.0f;
	_pickTurnAccumulator = 0.0f;
	_lastHeldPosition.reset();
	_handVelocity = glm::vec3(0.0f);
	// GInterface::PlaceObjectInMagicHand: an object in physics leaves it (RemoveObject)
	const bool caught = physics::PhysicsObjects::Find(entity) != nullptr;
	physics::PhysicsObjects::RemoveObject(entity);
	// Food / wood: the hand grabs a HandFood / HandWood pile and keeps pulling from the source while held over it
	// (GPotInfo.amountPickedUpInitially / PerTurn / PerTurnEnd / multiPickUpRampTime from info.dat).
	if (auto* pot = registry.TryGet<Pot>(entity); pot != nullptr)
	{
		const auto& pots = Locator::infoConstants::value().pot;
		const auto& sourceType = registry.Get<Mesh>(entity);
		PotInfo sourceInfo = PotInfo::FoodPot;
		for (size_t i = 0; i < pots.size(); ++i)
		{
			if (resources::HashIdentifier(pots[i].meshId) == sourceType.id)
			{
				sourceInfo = static_cast<PotInfo>(i);
				break;
			}
		}
		const bool isHandPile = sourceInfo == PotInfo::HandWood || sourceInfo == PotInfo::HandFood;
		if (!isHandPile)
		{
			const auto handType =
			    pots[static_cast<size_t>(sourceInfo)].resourceType == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood;
			const auto& handInfo = pots[static_cast<size_t>(handType)];
			// PotStructure::GetResource: a store pile offers the store's total.
			const auto store = StoragePitStore::OwnerOf(entity);
			const auto resource = pots[static_cast<size_t>(sourceInfo)].resourceType;
			const uint32_t available = store != entt::null ? StoragePitStore::GetResource(store, resource) : pot->amount;
			const auto take = std::min<uint32_t>(available, handInfo.amountPickedUpInitially);
			if (take == 0)
			{
				return;
			}
			const auto position = registry.Get<Transform>(entity).position;
			const auto pile = archetypes::PotArchetype::Create(position, 0.0f, handType, static_cast<int32_t>(take));
			if (pile == entt::null)
			{
				return;
			}
			if (store != entt::null)
			{
				StoragePitStore::RemoveResource(store, resource, take);
			}
			else
			{
				pot->amount = static_cast<uint16_t>(pot->amount - take);
				SinkPile(entity);
			}
			_pickSource = entity;
			_pickTurns = 0;
			_pickLock = _interactionPoint.value_or(position);
			entity = pile;
		}
	}
	// Carried objects must not be glued to the landscape by the height-map shader.
	if (registry.AllOf<MorphWithTerrain>(entity))
	{
		registry.Remove<MorphWithTerrain>(entity);
	}
	auto& transform = registry.Get<Transform>(entity);
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
	                         : 0.0f;
	_heldAltitude = transform.position.y - ground;
	_heldRotation = transform.rotation;
	_heldTop = 1.0f;
	_heldHeight = 0.0f;
	_holdRadius = 0.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr && meshes.Contains(mesh->id))
	{
		const auto& box = meshes.Handle(mesh->id)->GetBoundingBox();
		_heldTop = box.maxima.y * transform.scale.y;
		// Tree::GetHoldRadius = 0.2 * Get2DRadius.
		_heldHeight = _heldTop;
		_holdRadius = 0.2f * 0.5f * std::max(box.Size().x * transform.scale.x, box.Size().z * transform.scale.z);
	}
	if (registry.AllOf<Tree>(entity) && caught)
	{
		// a thrown tree caught again: already out of the ground, no uprooting
		PlaySample(audio::SoundId::G_PickUpObject);
	}
	else if (registry.AllOf<Tree>(entity))
	{
		// Tree::InterfaceSetInMagicHand: uprooting cracks (LH_SAMPLE_G_TREEBREAK_01 + rand % 3). The player also
		// loses alignment (GPlayerInfo.treePullPutAlignmentChange). TODO: alignment once players track it.
		static constexpr auto k_TreeBreak = std::array<audio::SoundId, 3> {
		    audio::SoundId::G_TreeBreak_01_1, audio::SoundId::G_TreeBreak_02_1, audio::SoundId::G_TreeBreak_03_1};
		PlaySample(Locator::rng::value().Choose(k_TreeBreak));
		_heldAltitude = 0.0f;
	}
	else if (!registry.AllOf<DeadTree>(entity))
	{
		PlaySample(audio::SoundId::G_PickUpObject);
	}
	ComputeHoldParameters(entity);
	_held = entity;
	_hovered.reset();
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Hand: picked up entity {}", static_cast<uint32_t>(entity));
}

void HandSystem::Drop() noexcept
{
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(*_held) && registry.AnyOf<Tree, DeadTree>(*_held))
	{
		const auto entity = *_held;
		_held.reset();
		_pickSource.reset();
		// Tree::ApplyThisToObject: dropped on a wood store it becomes its wood.
		if (const auto store = _interactionPoint ? FindWoodStore(*_interactionPoint) : std::nullopt; store)
		{
			DepositInStore(entity, *store);
		}
		else if (registry.AllOf<Tree>(entity))
		{
			ReleaseTree(entity);
		}
		else
		{
			auto& transform = registry.Get<Transform>(entity);
			transform.position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
			if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
			{
				fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
			}
			registry.SetDirty();
		}
		return;
	}
	if (registry.Valid(*_held) && (PotInfoOf(*_held) == PotInfo::HandWood || PotInfoOf(*_held) == PotInfo::HandFood))
	{
		const auto pot = *_held;
		_held.reset();
		_pickSource.reset();
		PutDownHandPot(pot);
		return;
	}
	if (registry.Valid(*_held))
	{
		auto& transform = registry.Get<Transform>(*_held);
		const float ground = Locator::terrainSystem::has_value()
		                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
		                         : 0.0f;
		transform.position.y = ground + _heldAltitude;
		if (auto* fixed = registry.TryGet<Fixed>(*_held); fixed != nullptr)
		{
			fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
		}
		registry.SetDirty();
	}
	_held.reset();
	_pickSource.reset();
}

void HandSystem::UpdateHeldObject() noexcept
{
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(*_held))
	{
		_held.reset();
		return;
	}
	const auto& hand = registry.Get<Transform>(_hands[static_cast<size_t>(Side::Left)]);
	auto& transform = registry.Get<Transform>(*_held);
	if (_holdType != HoldType::None)
	{
		// The grip point is the hand position itself (the model origin): obj.pos = hand->pos - lowering * up'.
		// The object matrix (fn_0046E2F0) is rebuilt from the hand: X = up' x side, Y = up', Z = side (the original is
		// mirrored, det -1; here a proper rotation), so the held object turns with the view.
		const auto centre = hand.position;
		const auto up = glm::normalize(-hand.rotation[2]);
		const auto side = glm::normalize(hand.rotation[0]);
		transform.rotation = glm::mat3(glm::cross(up, side), up, side);
		transform.position = centre - up * (_loweringMultiplier * _heldHeight);
		UpdateRoots(*_held);
		if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
		{
			static int frame = 0;
			if (++frame % 300 == 0)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"),
				                   "Tree hold: hand ({:.1f},{:.1f},{:.1f}) grip ({:.1f},{:.1f},{:.1f}) tree ({:.1f},{:.1f},{:.1f}) h={:.1f} r={:.2f} point ({:.1f},{:.1f})",
				                   hand.position.x, hand.position.y, hand.position.z, centre.x, centre.y, centre.z, transform.position.x,
				                   transform.position.y, transform.position.z, _heldHeight, _holdRadius,
				                   _interactionPoint ? _interactionPoint->x : 0.0f, _interactionPoint ? _interactionPoint->z : 0.0f);
			}
		}
	}
	if (_lastHeldPosition && _lastDt > 0.0f && !_springActive)
	{
		const auto velocity = (transform.position - *_lastHeldPosition) / _lastDt;
		_handVelocity += (velocity - _handVelocity) * 0.35f;
	}
	_lastHeldPosition = transform.position;
}

void HandSystem::Throw(glm::vec3 velocity) noexcept
{
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(*_held))
	{
		// ThrowObjectFromHand -> Object::InitialisePhysicsFromHand -> PhysicsObject::AddObject with the spring's velocity
		// TODO(physics): the angular velocity InitialisePhysicsFromHand gives
		if (physics::PhysicsObjects::AddObject(*_held, velocity, glm::vec3(0.0f), entt::null, true) == nullptr)
		{
			// no mesh to build a body from: the old ballistic flight
			_thrown.push_back({*_held, velocity, _heldAltitude});
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: thrown at ({:.1f}, {:.1f}, {:.1f}) u/s", velocity.x, velocity.y, velocity.z);
	}
	_held.reset();
	_pickSource.reset();
}

void HandSystem::UpdateThrown(float seconds) noexcept
{
	if (_thrown.empty() || seconds <= 0.0f)
	{
		return;
	}
	constexpr float k_Gravity = 30.0f;
	constexpr float k_AirDrag = 0.4f;
	auto& registry = Locator::entitiesRegistry::value();
	const auto* terrain = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
	for (auto& thrown : _thrown)
	{
		if (!registry.Valid(thrown.entity))
		{
			thrown.entity = entt::null;
			continue;
		}
		auto& transform = registry.Get<Transform>(thrown.entity);
		UpdateRoots(thrown.entity);
		thrown.velocity.y -= k_Gravity * seconds;
		thrown.velocity *= std::exp(-k_AirDrag * seconds);
		transform.position += thrown.velocity * seconds;
		const float ground = terrain != nullptr ? terrain->GetHeightAt(glm::vec2(transform.position.x, transform.position.z)) : 0.0f;
		if (transform.position.y <= ground + thrown.altitude && thrown.velocity.y < 0.0f)
		{
			transform.position.y = ground + thrown.altitude;
			// PhysicsObject::AttemptToAddSoundEvent 0x646683 -> fn_0074F2D0: landing in the water scares the fish, and
			// leaves a white ring at y 0.1 that grows 2 x the object's radius, aging at 1 / radius (cell 0x3F)
			if (!IsLand(transform.position))
			{
				ecs::SplashWater(transform.position);
				float radius = 1.0f;
				if (const auto* mesh = registry.TryGet<const Mesh>(thrown.entity);
				    mesh != nullptr && Locator::resources::value().GetMeshes().Contains(mesh->id))
				{
					radius = 0.5f * glm::length(Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox().Size()) *
					         transform.scale.x;
				}
				radius = std::max(radius, 0.01f);
				ecs::WaterRing ring;
				ring.position = glm::vec3(transform.position.x, 0.1f, transform.position.z);
				ring.growth = 2.0f * radius;
				ring.rate = 1.0f / radius;
				ring.cell = 0x3F;
				ecs::AddWaterRing(ring);
			}
			if (auto* fixed = registry.TryGet<Fixed>(thrown.entity); fixed != nullptr)
			{
				fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
			}
			if (const auto type = PotInfoOf(thrown.entity); (type == PotInfo::HandWood || type == PotInfo::HandFood) && IsLand(transform.position))
			{
				PutDownHandPot(thrown.entity);
			}
			else if (registry.AnyOf<Tree, DeadTree>(thrown.entity))
			{
				// Tree::ReactToPhysicsImpact: absorbed by a wood store it hits. Anything else: a thrown tree never
				// lands as planted (PHYSICS_OBJECT_FLAG_LANDED is only set by a gentle release) and becomes a DeadTree.
				if (const auto store = FindWoodStore(transform.position); store)
				{
					DepositInStore(thrown.entity, *store);
				}
				else if (registry.AllOf<Tree>(thrown.entity))
				{
					MakeDeadTree(thrown.entity, thrown.velocity);
				}
			}
			thrown.entity = entt::null;
		}
	}
	std::erase_if(_thrown, [](const Thrown& thrown) { return thrown.entity == entt::null; });
	registry.SetDirty();
}

glm::mat3 HandSystem::HeldSway(glm::vec3 at) const noexcept
{
	// HandStateHolding::Update: up to 0.3 rad about the view direction ((smooth - mouse).x, clamped to 80 px) and
	// about the side axis ((mouse - smooth).y), pivoting on the grip.
	glm::mat3 sway(1.0f);
	if (!Locator::camera::has_value())
	{
		return sway;
	}
	auto toCamera = Locator::camera::value().GetOrigin() - at;
	if (glm::length(toCamera) <= 1e-4f)
	{
		return sway;
	}
	toCamera = glm::normalize(toCamera);
	const float tiltX = glm::clamp(_smoothMouse.x - _mouse.x, -80.0f, 80.0f) * (0.3f / 80.0f);
	const float tiltY = glm::clamp(_mouse.y - _smoothMouse.y, -80.0f, 80.0f) * (0.3f / 80.0f);
	const auto side = glm::vec3(-toCamera.z, 0.0f, toCamera.x);
	sway = glm::mat3(glm::rotate(glm::mat4(1.0f), tiltX, toCamera));
	if (glm::length(side) > 1e-4f)
	{
		sway = sway * glm::mat3(glm::rotate(glm::mat4(1.0f), tiltY, glm::normalize(side)));
	}
	return sway;
}

void HandSystem::ComputeHoldParameters(entt::entity entity) noexcept
{
	// objects.cpp / NOTES_objects.md (class table):
	//   Object (default)          ABOVE     R = 0.75 * height   lowering 0
	//   Tree / DeadTree           TREE      R = 0.2 * R2D       lowering 0.1   (Tree is rooted)
	//   MobileObject, Pot         SIDE      R = R2D             lowering 0.7
	//   MobileStatic gate totems, weeping stones: SIDE 0.7; singing stone 1: SIDE 0.4; others ABOVE
	//   Villager                  VILLAGER  R = R2D             lowering 0.65
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<Transform>(entity);
	float radius2D = 0.5f;
	_heldHeight = 1.0f;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (const auto* mesh = registry.TryGet<const Mesh>(entity); mesh != nullptr && meshes.Contains(mesh->id))
	{
		const auto size = meshes.Handle(mesh->id)->GetBoundingBox().Size() * transform.scale;
		radius2D = 0.5f * std::max(size.x, size.z);
		_heldHeight = size.y; // Object::GetHeight = 2 * half extent y * scale
	}
	_holdType = HoldType::Above;
	_holdRadius = 0.75f * _heldHeight;
	_loweringMultiplier = 0.0f;
	_rooted = registry.AllOf<Tree>(entity);
	if (registry.AnyOf<Tree, DeadTree>(entity))
	{
		_holdType = HoldType::Tree;
		_holdRadius = 0.2f * radius2D;
		_loweringMultiplier = 0.1f;
	}
	else if (registry.AnyOf<MobileObject, Pot>(entity))
	{
		_holdType = HoldType::Side;
		_holdRadius = radius2D;
		_loweringMultiplier = 0.7f;
	}
	else if (registry.AllOf<Villager>(entity))
	{
		_holdType = HoldType::Villager;
		_holdRadius = radius2D;
		_loweringMultiplier = 0.65f;
	}
	else if (const auto* statics = registry.TryGet<const MobileStatic>(entity); statics != nullptr)
	{
		switch (statics->type)
		{
		case MobileStaticInfo::GateTotemApe:
		case MobileStaticInfo::GateTotemBlank:
		case MobileStaticInfo::GateTotemCow:
		case MobileStaticInfo::GateTotemTiger:
		case MobileStaticInfo::WeepingStone:
		case MobileStaticInfo::WeepingStoneReward:
			_holdType = HoldType::Side;
			_holdRadius = radius2D;
			_loweringMultiplier = 0.7f;
			break;
		case MobileStaticInfo::SingingStone_1:
			_holdType = HoldType::Side;
			_holdRadius = radius2D;
			_loweringMultiplier = 0.4f;
			break;
		default:
			break;
		}
	}
}
