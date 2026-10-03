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
#include "Camera/Camera.h"
#include "Windowing/WindowingInterface.h"
#include "Camera/CameraModel.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishShoals.h"
#include "ECS/WaterRings.h"
#include "ECS/Registry.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/SeaCells.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Fire/FireEffect.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/Worship.h"

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
	_pickTurn = game_clock::Turn();
	_lastHeldPosition.reset();
	_handVelocity = glm::vec3(0.0f);
	// GInterface::PlaceObjectInMagicHand: an object in physics leaves it (RemoveObject)
	const bool caught = physics::PhysicsObjects::Find(entity) != nullptr;
	physics::PhysicsObjects::RemoveObject(entity);
	// Living::PlaceInHand: IN_HAND (its clip SCARED_STIFF, ECS/VillagerAnimations)
	ecs::SetVillagerState(entity, VillagerStates::InHand);
	// Animal::InterfaceSetInMagicHand: off its flock, IN_HAND
	if (registry.AllOf<Animal>(entity))
	{
		ecs::animal_ai::PlaceInHand(entity);
	}
	// Pot / PileResource::InterfaceSetInMagicHand: Pot::RemoveReaction
	if (registry.AllOf<Pot>(entity))
	{
		ecs::animal_ai::RemovePotReaction(entity);
	}
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
	// fn_005DC330, the object entering the hand: out of the map cells (IsObjectInMap vt +0x178 at 0x5DC377,
	// RemoveMapObject vt +0x548 at 0x5DC385). (inferido) The early returns above put nothing in the hand and take
	// nothing out, the same end as PlaceObjectInMagicHand 0x5DA6F0's failure path (fn_005DC330 != 1 at 0x5DA7C9 ->
	// InsertMapObject 0x5DA849)
	if (ecs::map_cells::IsObjectInMap(entity))
	{
		ecs::map_cells::RemoveMapObject(entity);
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
	// GInterface::GenericPickup 0x5D2800 (0x5D2881..0x5D28B7): every object but a rooted tree or forest (IsTree /
	// IsForest without the uprooted bit +0x24 & 0x40) gets SoundTag::Create(its MapCoords +0x14, 10 G_PickUpObject,
	// track 0, mode 3, loops 0, +0x40 0, is3D 1, InGame, delay 0) 0x71EB60, a point tag at the object's point (x, altitude
	// + height, z) that plays at once (fn_0071EA40). A DeadTree is not IsTree (it has no override of
	// GameThingWithPos::IsTree 0x402320, which returns 0), so it sounds too.
	const auto pickupPoint = transform.position;
	const auto pickupTag = [&pickupPoint](int sample) {
		audio::tags::Create(pickupPoint, sample, false, 3, 0, false, true, audio::SfxBank::InGame, 0);
	};
	if (registry.AllOf<Tree>(entity) && caught)
	{
		// a thrown tree caught again: already out of the ground (+0x24 & 0x40), no uprooting
		pickupTag(10);
	}
	else if (registry.AllOf<Tree>(entity))
	{
		// Tree::InterfaceSetInMagicHand 0x74B730 (rooted, +0x24 & 0x40 clear): SoundTag::Create(the tree's MapCoords,
		// GetRandomSample(32 G_TreeBreak_01, 3) 0x71ED40, track 0, mode 3, loops 0, 0, is3D 1, InGame, delay 0)
		// (0x74B739..0x74B758). Uprooting is evil: GAlignment::Update(the hand's player, tree, false),
		// -treePullPutAlignmentChange weighed by the alignment.
		ecs::effects::alignment::UpdateForTree(PlayerNames::PLAYER_ONE, false);
		audio::tags::Create(pickupPoint, audio::tags::RandomSample(32, 3), false, 3, 0, false, true, audio::SfxBank::InGame,
		                    0);
		_heldAltitude = 0.0f;
	}
	else
	{
		pickupTag(10);
	}
	// 0x5D28C5..0x5D295D: a villager (IsVillager vt +0x2C8) that is alive (Object::IsAlive 0x402610: GetLife() > 0 and
	// available) screams with a second point tag of the same form: a child (IsChild vt +0xAF8) 180 + GetRandomSample(7)
	// G_PickUpChild_01.., else a woman (Villager::IsWoman 0x752620) 194 G_PickUpWoman_01.., else 187 G_PickUpMan_01..
	if (registry.AllOf<Villager>(entity) && ecs::life::LifeOf(entity) > 0.0f)
	{
		const int first = ecs::villager::IsChild(entity) ? 180 : ecs::villager::IsWoman(entity) ? 194 : 187;
		pickupTag(audio::tags::RandomSample(first, 7));
	}
	ComputeHoldParameters(entity);
	_held = entity;
	_hovered.reset();
	// GInterface::PlaceObjectInMagicHand 0x5DA6F0: a burning object leaves its group; held (not a villager) it is
	// REACT_TO_BURNING_OBJECT_IN_HAND (FireEffect::StartedMoving, ECS/Fire)
	fire::StartedMoving(entity, !registry.AllOf<Villager>(entity));
	// the same tail, fn_0052B600: a firefly sleeping on the object is freed and may leave a one-shot miracle
	// (Worship/FireFlyReward.cpp, Land 1's FIRE_FLY_SPELL_REWARD_PROB)
	worship::OnPlacedInMagicHand(entity);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Hand: picked up entity {}", static_cast<uint32_t>(entity));
}

namespace
{
/// GameThingWithPos::IsFence (vt +0x3CC) = MobileStatic::IsFence 0x609110: its GMobileStaticInfo's mesh is MESH_LIST
/// 0x38 (BuildingAmericanFence) or 0x51..0x52 (the Celtic fences).
bool IsFence(entt::entity entity)
{
	const auto* statics = Locator::entitiesRegistry::value().TryGet<const MobileStatic>(entity);
	if (statics == nullptr || !Locator::infoConstants::has_value() || statics->type == MobileStaticInfo::None)
	{
		return false;
	}
	const auto mesh = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics->type)).meshId;
	return mesh == MeshId::BuildingAmericanFence || mesh == MeshId::BuildingCelticFenceShort ||
	       mesh == MeshId::BuildingCelticFenceTall;
}

/// LHMatrix::GetYXZ 0x7FAB30 (confirmed by emulation, dev\tmp_dis\agua\re\emu_getyxz.py) with the rows = the body
/// axes (openblack's columns): x = asin(r2.y), z = atan2(-r0.y, r1.y); a uniform scale does not change them.
glm::vec2 TiltXZ(const glm::mat3& rotation)
{
	const auto& r0 = rotation[0];
	const auto& r1 = rotation[1];
	const auto& r2 = rotation[2];
	return glm::vec2(std::atan2(r2.y, std::sqrt(r2.x * r2.x + r2.z * r2.z)), std::atan2(-r0.y, r1.y));
}

/// Villager::CreateDroppedResource 0x750940, called for a villager released without landing: a villager whose
/// carrying type (+0xF1) is 2..15 and whose wood (+0xF6) is over GVillagerInfo::minWoodToShowGraphic (+0x26C) lets
/// his log fall: a DeadTree (fn_00510BB0, the mesh of table 0xC5E19C, scale 1, angle pi/2, woodValue = wood /
/// GetWoodValue) put into physics with the villager's velocity and angular velocity, flag 0x10,
/// AdjustToGroundLevel(false, true), RaiseUntilNotIntersecting, then DropWood(0).
/// TODO(villager-jobs): openblack's villagers carry no wood yet, so there is nothing to drop.
void CreateDroppedResource([[maybe_unused]] entt::entity villager, [[maybe_unused]] glm::vec3 velocity) {}
} // namespace

void HandSystem::PlaceWithoutBody(entt::entity entity) noexcept
{
	// openblack only: an object PhysicsObject::AddObject cannot build a body for (no mesh) is put where it is, on the
	// ground (Living::InitialisePhysicsFromHand 0x5EFDF8 ends the physics of a Living at once when AddObject fails)
	auto& registry = Locator::entitiesRegistry::value();
	auto& transform = registry.Get<Transform>(entity);
	const float ground = Locator::terrainSystem::has_value()
	                         ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
	                         : 0.0f;
	transform.position.y = ground;
	if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	// its EndPhysics at once (Living::InitialisePhysicsFromHand 0x5EFDF8): Object::EndPhysics 0x6375A0 puts it back in
	// the map cells (InsertMapObject vt +0x544 at 0x63762C)
	ecs::map_cells::InsertMapObject(entity);
	if (registry.AllOf<Villager>(entity) && ecs::sea_cells::IsWater(transform.position))
	{
		ecs::VillagerEndPhysicsInWater(entity);
	}
	else
	{
		ecs::SetVillagerState(entity, VillagerStates::Landed);
	}
	ecs::animal_ai::PutDown(entity);
	registry.SetDirty();
}

bool HandSystem::InitialisePhysicsFromHand(entt::entity entity, glm::vec3 velocity, bool dontReplant) noexcept
{
	// Object::InitialisePhysicsFromHand 0x636F00 (bw1-decomp src/Black/Object.cpp:447), from the hand (thrower NULL).
	// Living 0x5EFD80 / Villager 0x5EFE90 add FLYING and the disciple around it (AddObject sets FLYING in openblack).
	// TODO(physics): the angular velocity (GInterfaceStatus::ThrowAngularVelocity) is 0.
	auto& registry = Locator::entitiesRegistry::value();
	const bool tree = registry.AnyOf<Tree, DeadTree>(entity); // IsAnyKindOfTree (vt +0x478)
	// fromHand false: AddObject's own fromHand path spreads the flying-object reaction at once, but here it is only
	// spread when the object does not land (0x637412, below); PHYSICS_OBJECT_FLAG_FROM_HAND and the player are set here
	auto* po = physics::PhysicsObjects::AddObject(entity, velocity, glm::vec3(0.0f), entt::null, false);
	if (po == nullptr)
	{
		return false;
	}
	po->flags |= physics::PhysicsObject::FromHand;
	po->byPlayer = true;
	// thrown = |v.xz|^2 > 4 from the hand (> 1 from a creature); either way AdjustToGroundLevel(thrown, !tree) (thrown:
	// only raised out of the ground), ZeroForces, RaiseUntilNotIntersecting and PHYSICS_OBJECT_FLAG_FROM_HAND
	const bool thrown = velocity.x * velocity.x + velocity.z * velocity.z > 4.0f;
	po->body.AdjustToGroundLevel(thrown, !tree);
	const float oldAltitude = po->body.Centre().y;
	po->body.ZeroForces();
	physics::PhysicsObjects::RaiseUntilNotIntersecting(*po);
	const auto at = po->body.Centre();
	// 0x637128: not thrown and not raised over something (the exception of a computer player's villager has no hand
	// here): landed on dry land, or on a cell (rounded, fistp) of altitude > 1
	bool landed = false;
	if (!thrown && oldAltitude == at.y && Locator::terrainSystem::has_value())
	{
		namespace sea_cells = ecs::sea_cells;
		const auto& island = Locator::terrainSystem::value();
		landed = sea_cells::IsDryLand(island, sea_cells::CellOf(at)) ||
		         sea_cells::AltitudeAt(island, sea_cells::RoundedCellOf(at)) > 1; // 0x637231: cmp [cell + 4], 1; jbe
	}
	const bool living = registry.AnyOf<Villager, Animal>(entity); // IsLiving (vt +0x3C4)
	const bool fence = IsFence(entity);
	if (landed && (living || fence) && physics::LandscapeNormal(at).y < 0.7f)
	{
		landed = false; // 0x6372A6: too steep to stand on
	}
	const auto tilt = TiltXZ(registry.Get<const Transform>(entity).rotation);
	const char* outcome = "in physics";
	if (landed)
	{
		// TODO(villager-jobs): GGuidance::MakeDiscipleSFX when the villager's disciple changed (0x6372E8).
		// TODO(creature): ConsiderCreatureMimickingWhenObjectLands (0x637306).
		po->flags |= physics::PhysicsObject::Landed;
		// Living, Fence, or a Tree with no FireEffect (+0x44, ECS/Fire) on land: out of physics at once, where it is,
		// through its EndPhysics; a tree held tilted (GetYXZ: |x| or |z| > 0.2) or dont_replant stays in physics, not
		// LANDED. A hot or burning tree stays in physics, LANDED (bw1-decomp Object.cpp:567), and ends a DeadTree.
		const bool treeOnLand =
		    registry.AllOf<Tree>(entity) && fire::Find(entity) == nullptr && ecs::sea_cells::IsLand(at);
		if (living || fence || treeOnLand)
		{
			if (treeOnLand && (dontReplant || std::abs(tilt.x) > 0.2f || std::abs(tilt.y) > 0.2f))
			{
				po->flags &= ~physics::PhysicsObject::Landed;
				outcome = "tilted tree: in physics";
			}
			else
			{
				physics::PhysicsObjects::RemoveObjectWithEndPhysics(entity);
				outcome = "landed: out of physics";
			}
		}
		else
		{
			outcome = "landed: in physics";
		}
	}
	else
	{
		if (registry.AllOf<Villager>(entity))
		{
			CreateDroppedResource(entity, velocity);
		}
		// Reaction::CreateReaction(this, REACTION_REACT_TO_FLYING_OBJECT 9, player, 0) 0x637412: spread once
		// (SpreadReaction 0x6E3E10) to the Livings near it; the thrower is the hand's player (PLAYER_ONE's interface)
		ecs::animal_ai::SpreadFlyingObjectReaction(entity, PlayerNames::PLAYER_ONE);
		// TODO(creature): Creature::CheckAllCreaturesForCatching 0x47CBD0.
	}
	// TODO(creature): a toy -> ConsiderMakingCreatureMimicPlayer(DETECTED_PLAYER_ACTION_PLAY_WITH_TOY) (0x637457).
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Hand: released {} at ({:.1f}, {:.1f}, {:.1f}) v ({:.1f}, {:.1f}, {:.1f}) thrown {} tilt ({:.2f}, {:.2f}): {}",
	                   static_cast<uint32_t>(entity), at.x, at.y, at.z, velocity.x, velocity.y, velocity.z, thrown, tilt.x, tilt.y,
	                   outcome);
	registry.SetDirty();
	return true;
}

void HandSystem::Release(glm::vec3 velocity) noexcept
{
	// The hand opens (packet 0x12, GInterface 0x5DA400): held->ApplyThisToMapCoord(status, pos) whatever the speed, then
	// ThrowObjectFromHand(status, dont_replant 0) 0x6385E0 -> InitialisePhysicsFromHand(ThrowVelocity, ...)
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = *_held;
	if (registry.Valid(entity))
	{
		// Tree::ApplyThisToMapCoord 0x74BFD0 / DeadTree 0x511050: on a wood store (IsResourceStore) the store takes it
		// (DeleteObjectAndTakeResource, 3), thrown or not
		if (registry.AnyOf<Tree, DeadTree>(entity))
		{
			if (const auto store = _interactionPoint ? FindWoodStore(*_interactionPoint) : std::nullopt; store)
			{
				_held.reset();
				_pickSource.reset();
				fire::SetOutMagicHand(entity); // GMagicHand::RemoveFromHand 0x5FB0B0: FireEffect::SetOutMagicHand
				DepositInStore(entity, *store);
				return;
			}
		}
		// Pot::ApplyThisToMapCoord (0x66DED8): a pot put down offers its reaction again (the hand's HandWood / HandFood
		// become a pile in PutDownHandPot, which offers the pile's)
		if (const auto type = PotInfoOf(entity);
		    registry.AllOf<Pot>(entity) && type != PotInfo::HandWood && type != PotInfo::HandFood)
		{
			ecs::animal_ai::SetupPotReaction(entity);
		}
	}
	ThrowObjectFromHand(velocity, false);
}

void HandSystem::ThrowObjectFromHand(glm::vec3 velocity, bool dontReplant) noexcept
{
	// Object::ThrowObjectFromHand(status, dont_replant) 0x6385E0: out of the hand, then InitialisePhysicsFromHand
	if (!_held)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = *_held;
	_held.reset();
	_pickSource.reset();
	fire::SetOutMagicHand(entity); // GMagicHand::RemoveFromHand 0x5FB0B0: FireEffect::SetOutMagicHand
	if (!registry.Valid(entity))
	{
		return;
	}
	// Pot::InitialisePhysicsFromHand 0x66DF00: |v|^2 <= 5 (all three axes) puts the resource down at once
	// (PSysGlobal::StartMultiPutdown, Pot::AddResourceToPos, GoolooGooloo, ToBeDeleted); faster it flies as an Object
	if (const auto type = PotInfoOf(entity); type == PotInfo::HandWood || type == PotInfo::HandFood)
	{
		if (glm::dot(velocity, velocity) <= 5.0f)
		{
			PutDownHandPot(entity);
			return;
		}
	}
	// TODO(villager-jobs): Villager::ThrowObjectFromHand 0x756AE0 first clears the disciple (SetVillagerDisciple(0, 0, 0))
	// when the villager is the player's or nobody's.
	if (InitialisePhysicsFromHand(entity, velocity, dontReplant))
	{
		return;
	}
	// openblack only, no body could be built (no mesh): thrown, the old ballistic flight; put down, placed
	if (velocity.x * velocity.x + velocity.z * velocity.z > 4.0f)
	{
		_thrown.push_back({entity, velocity, _heldAltitude});
	}
	else
	{
		PlaceWithoutBody(entity);
	}
}

void HandSystem::Drop() noexcept
{
	// ForceDropHeld 0x5D4350 is packet 0x4D with zero velocity, then 0x1D -> ThrowObjectFromHand(status, 1); the test
	// hook OPENBLACK_HAND_TEST_DROP uses this as a gentle release instead (dont_replant 0)
	Release(glm::vec3(0.0f));
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
			// PhysicsObject::AttemptToAddSoundEvent 0x6465B7: off dry land a white ring at y 0.1 that grows 2 x the
			// object's radius, aging at 1 / radius (cell 0x3F); in deep water (no cell or altitude < 3 at the rounded
			// cell, 0x646683) fn_0074F2D0 scares the fish too
			if (!sea_cells::IsDryLand(transform.position))
			{
				const auto* island = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
				const auto* cell = island != nullptr ? sea_cells::CellAt(*island, sea_cells::RoundedCellOf(transform.position)) : nullptr;
				if (cell == nullptr || island->GetCellAltitude(*cell) < 3)
				{
					ecs::SplashWater(transform.position);
				}
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
	// (inferido) ObtainRequiredHandPosition builds this from two fn_007FB180 (lh_matrix::AxisAngle, glm's rotate(-a),
	// 0x5B49C8 and 0x5B4AD5) joined by fn_007FAFF0 0x5B4AE5; which angle and axis each one takes (0x5B49B6..0x5B4ACE) is
	// not read, so the +tilt and the order here are kept as they were
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
	// Object::GetHoldRadius 0x638C00 asks vt +0x64 (0x638C22..0x638C26), with its overrides: a food pile (PileFood,
	// MagicFood, PuzzleGrain and the hand's HandFood, 0x66F180) is x its GetProportionRaised, so the hand opens as the
	// food in it grows. GetHeight vt +0x42C (Object 0x638120). No mesh: 0 for both (0x6381E9, 0x638140)
	const float radius2D = ecs::object::Get2DRadius(entity);
	_heldHeight = ecs::object::GetHeight(entity);
	_holdType = HoldType::Above;
	_holdRadius = 0.75f * _heldHeight;
	_loweringMultiplier = 0.0f;
	_rooted = registry.AllOf<Tree>(entity);
	if (const auto* seed = registry.TryGet<const SpellSeed>(entity); seed != nullptr)
	{
		// SpellSeed 0x728640..0x728680: MAGIC until ready, then the seed info's hold type; R = holdRadius x scale. The
		// height is SpellSeed's vt +0x42C = Object::GetHeight 0x638120 of the seed info's mesh, even when the seed is not
		// drawn in the hand
		const auto& info = magic::seed::InfoOf(*seed);
		if (const auto half = ecs::object::MeshHalfExtents(resources::HashIdentifier(info.mesh)); half)
		{
			_heldHeight = ecs::object::Height(*half, ecs::object::GetScaleField(entity));
		}
		_holdType
 = seed->ready ? static_cast<HoldType>(info.holdType) : HoldType::Magic;
		_holdRadius = info.holdRadius * transform.scale.x;
		_loweringMultiplier = info.holdLoweringMultiplier;
		_rooted = false;
	}
	else if (registry.AnyOf<Tree, DeadTree>(entity))
	{
		_holdType = HoldType::Tree;
		_holdRadius = 0.2f * radius2D;
		_loweringMultiplier = 0.1f;
	}
	else if (registry.AnyOf<MobileObject, Pot, OneOffSpellSeed>(entity))
	{
		// a one-shot orb is a MobileObject: GetHoldType 0x607120 = 6, GetHoldLoweringMultiplier 0x607130 and
		// Object::GetHoldRadius 0x638C00 (Get2DRadius) in its vtable
		_holdType = HoldType::Side;
		_holdRadius = radius2D;
		_loweringMultiplier = 0.7f;
	}
	else if (registry.AnyOf<Villager, Animal>(entity))
	{
		// the Living hold class: villagers and animals alike
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
