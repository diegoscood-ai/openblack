/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a released object does when the hand lets it go: the throw and the physics it starts with

#include "FromHand.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Creature/CreatureDeeds.h"
#include "Creature/LocalPlayer.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishShoals.h"
#include "ECS/GamePhysicsHooksWorld.h"
#include "ECS/LastDropped.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectFlags.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/PhysicsGround.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/WaterRings.h"
#include "Hand/HandGrabRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace openblack::ecs::physics::from_hand
{
namespace
{
/// The physics objects' state (Locator::dynamicsSystem)
openblack::ecs::physics::State& PhysicsState()
{
	return openblack::Locator::dynamicsSystem::value().GetState();
}

bool IsPot(entt::entity entity)
{
	return PhysicsState().handHooks.isPot && PhysicsState().handHooks.isPot(entity);
}

/// A creed (a mobile object made from a row of the creed's kind), which the physics never takes
bool IsCreed(entt::entity entity)
{
	const auto* mobile = Locator::entitiesRegistry::value().TryGet<const MobileObject>(entity);
	if (mobile == nullptr || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& rows = Locator::infoConstants::value().mobileObject;
	const auto row = static_cast<size_t>(mobile->type);
	return row < rows.size() && rows.at(row).mobileType == MobileObjectInfo::Creed;
}

} // namespace

void SetHandHooks(HandHooks hooks)
{
	PhysicsState().handHooks = std::move(hooks);
}

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

void PlaceWithoutBody(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A villager or animal the physics won't take, as one a script made immovable, ends its physics at once with no
	// body, as the game does: on the land under where it was let go, upright, back in the map, landing as if it had
	// come down there
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		villager_physics::EndPhysicsWithoutBody(entity);
		registry.SetDirty();
		return;
	}
	// (approximate) openblack only: anything else is put where it is, on the ground and back in the map. The game
	// leaves it where the hand let it go, counted in the physics and out of the map
	auto& transform = registry.Get<Transform>(entity);
	const float ground =
	    Locator::terrainSystem::has_value()
	        ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z))
	        : 0.0f;
	transform.position.y = ground;
	if (auto* fixed = registry.TryGet<Fixed>(entity); fixed != nullptr)
	{
		fixed->boundingCenter = glm::vec2(transform.position.x, transform.position.z);
	}
	map_cells::InsertMapObject(entity);
	registry.SetDirty();
}

bool MayLand(bool thrown, bool raised, bool computersVillager)
{
	return !thrown && (!raised || computersVillager);
}

bool IsMyLetGo(std::optional<PlayerNames> throwerPlayer, PlayerNames local)
{
	return throwerPlayer == local;
}

std::optional<PlayerNames> ToyPlayer(std::optional<PlayerNames> throwerPlayer, bool byCreature)
{
	return byCreature ? std::nullopt : throwerPlayer;
}

bool LandedTreeStaysInPhysics(bool byThrower, bool dontReplant, glm::vec2 tilt)
{
	// a thrower (a creature) replants it whatever its tilt
	return !byThrower && (dontReplant || std::abs(tilt.x) > 0.2f || std::abs(tilt.y) > 0.2f);
}

bool HangsWhereLetGo(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && !registry.AnyOf<Villager, Animal>(entity) &&
	       (object_flags::IsImmovable(entity) || IsCreed(entity));
}

void LetGoWithoutBody(entt::entity entity)
{
	if (!HangsWhereLetGo(entity))
	{
		PlaceWithoutBody(entity);
		return;
	}
	// in the physics with no body and out of the map, where the hand let it go: nothing puts it down, and nothing
	// takes it out of the physics while it has no entry there
	auto& registry = Locator::entitiesRegistry::value();
	registry.AssignOrReplaceState<InPhysics>(entity);
	if (map_cells::IsObjectInMap(entity))
	{
		map_cells::RemoveMapObject(entity);
	}
	registry.SetDirty();
}

bool TakeOutOfPhysicsForHand(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<InPhysics>(entity) ||
	    Locator::dynamicsSystem::value().Find(entity) != nullptr)
	{
		return true;
	}
	// nothing goes into the hand: the object is filed back in the map while it is available, still in the physics
	if (ecs::IsAvailable(entity))
	{
		map_cells::InsertMapObject(entity);
	}
	return false;
}

std::optional<bool> InitialisePhysicsFromHand(entt::entity entity, glm::vec3 velocity, bool dontReplant,
                                              std::optional<PlayerNames> throwerPlayer,
                                              std::optional<glm::vec3> angularMomentum, bool byCreature, entt::entity thrower)
{
	// From the hand (no thrower) or a creature (the thrower). Living things and villagers add FLYING and the disciple around
	// it: FLYING (the class's StartPhysics) at the end, only when the object does not land.
	auto& registry = Locator::entitiesRegistry::value();
	auto& hooks = Locator::dynamicsSystem::value().ClassHooks();
	// the class's part before the shared one (a scaffold destroys the things in its way)
	hooks.BeforeInitialisePhysicsFromHand(Locator::dynamicsSystem::value(), entity);
	// thrown by a player's hand (not a creature): that player's interface keeps it as the last thing it let go of
	if (thrower == entt::null && throwerPlayer.has_value())
	{
		last_dropped::MarkThrown(registry, *throwerPlayer, entity);
	}
	const bool tree = registry.AnyOf<Tree, DeadTree>(entity);
	// AddObject's own fromHand path would spread the flying-object reaction at once, but here it is only spread when
	// the object does not land (below); the from-hand flag and the throwing player are set by
	// InitialisePhysics
	// the prediction of this object ends (before AddObject)
	Locator::dynamicsSystem::value().EndPredictionOf(entity);
	auto* po = Locator::dynamicsSystem::value().InitialisePhysics(entity, velocity, glm::vec3(0.0f), throwerPlayer, thrower);
	if (po == nullptr)
	{
		return std::nullopt;
	}
	// the angular momentum L = the throw's, after AddObject(zero) and the paper / disappear block, before
	// SettleOnLand
	if (angularMomentum)
	{
		po->body.angularMomentum = *angularMomentum;
	}
	// thrown = |v.xz|^2 > 4 from the hand (> 1 from a creature); either way SettleOnLand(thrown, !tree) (thrown:
	// only raised out of the ground), ClearForces, RaiseClearOfWhatIsUnder and the from-hand flag
	const bool thrown = hand_grab::IsThrow(velocity, byCreature);
	po->body.SettleOnLand(ecs::GameGround(), thrown, !tree);
	// SettleOnLand ends copying the matrix into the turn-start one: a put-down villager's landType
	// comes from the ground-aligned rows, not the hand's (RaiseClearOfWhatIsUnder does the same when it raises it)
	Locator::dynamicsSystem::value().SyncTurnStart(*po);
	const float oldAltitude = po->body.Centre().y;
	po->body.ClearForces();
	Locator::dynamicsSystem::value().RaiseClearOfWhatIsUnder(*po);
	const auto at = po->body.Centre();
	// not thrown and not raised over something, unless it is a villager let go of for a computer player (by its hand
	// or its creature): landed on dry land, or on a cell (rounded) of altitude > 1
	const bool computersVillager =
	    registry.AllOf<Villager>(entity) && throwerPlayer.has_value() &&
	    openblack::magic::players::ReadMagicOf(*throwerPlayer).playerType == PlayerMagic::k_ComputerPlayerType;
	bool landed = false;
	if (MayLand(thrown, oldAltitude != at.y, computersVillager) && Locator::terrainSystem::has_value())
	{
		const auto& island = Locator::terrainSystem::value();
		landed = sea_cells::IsDryLand(island, sea_cells::CellOf(at)) ||
		         sea_cells::AltitudeAt(island, sea_cells::RoundedCellOf(at)) > 1;
	}
	const bool living = registry.AnyOf<Villager, Animal>(entity);
	const bool fence = IsFence(entity);
	if (landed && (living || fence) && ecs::GameGround().NormalAt(glm::vec2(at.x, at.z)).y < 0.7f)
	{
		landed = false; // too steep to stand on
	}
	// the yaw, pitch and roll of the object's matrix, as floats
	float tiltY = 0.0f;
	glm::vec2 tilt(0.0f);
	affine::DecomposeYXZ(registry.Get<const Transform>(entity).rotation, tiltY, tilt.x, tilt.y);
	const char* outcome = "in physics";
	if (landed)
	{
		// a villager whose disciple changed in the hand: the guidance's disciple remark, only for a let-go of the local
		// player's
		if (registry.AllOf<Villager>(entity) && IsMyLetGo(throwerPlayer, openblack::creature::LocalPlayer()))
		{
			villager::DisciplePutDownByHand(entity);
		}
		// the creature of the player whose interface let go of it last may copy that player for where it was put down
		ecs::creature_mimic::ConsiderObjectLands(entity);
		po->flags |= PhysicsEntry::k_Landed;
		// Living, Fence, or a Tree with no FireEffect (ECS/Fire) on land: out of physics at once, where it is,
		// through its EndPhysics; a tree held tilted (DecomposeYXZ) or dontReplant stays in physics, not LANDED, unless a
		// creature let go of it (LandedTreeStaysInPhysics). A hot or burning tree stays in physics, LANDED, and ends a
		// DeadTree.
		const bool treeOnLand = registry.AllOf<Tree>(entity) && fire::Find(entity) == nullptr && sea_cells::IsLand(at);
		if (living || fence || treeOnLand)
		{
			if (treeOnLand && LandedTreeStaysInPhysics(thrower != entt::null, dontReplant, tilt))
			{
				po->flags &= ~PhysicsEntry::k_Landed;
				landed = false;
				outcome = "tilted tree: in physics";
			}
			else
			{
				Locator::dynamicsSystem::value().RemoveObjectWithEndPhysics(entity);
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
			// the villager drops what it carries (ECS/Villager), with the throw's velocity; the hand's angular
			// velocity is 0
			ecs::villager::CreateDroppedResource(entity, velocity, std::nullopt, glm::vec3(0.0f));
		}
		// the flying-object reaction: spread now to the living things near it, for the thrower's player (the neutral
		// player for no one's)
		animal_ai::SpreadFlyingObjectReaction(entity, throwerPlayer.value_or(PlayerNames::NEUTRAL));
		Locator::dynamicsSystem::value().CheckAllCreaturesForCatching(entity, *po);
	}
	// a toy let go by a player's hand: that player's creature may copy the player playing with it
	if (const auto player = ToyPlayer(throwerPlayer, byCreature); player && ecs::GamePhysicsHooksWorld().IsToy(entity))
	{
		ecs::creature_mimic::Consider(*player, creature_watching::Deed::PlayWithToy, entity);
	}
	// A living thing, once the shared part has returned: a body, not LANDED and still this object -> FLYING.
	// Find(entity) is the "still this one" (the ones taken out of the physics are gone), so a put-down
	// villager or animal goes to LANDED through its EndPhysics without passing through FLYING.
	if (auto* now = Locator::dynamicsSystem::value().Find(entity); now != nullptr && (now->flags & PhysicsEntry::k_Landed) == 0)
	{
		Locator::dynamicsSystem::value().InitialisePhysicsOfClass(*now, true);
	}
	// the class's part after the shared one returned the body (a scaffold reads its LANDED flag and
	// dontReplant); a body already taken out of the physics (Living, Fence, replanted Tree) has no class part here
	if (const auto* now = Locator::dynamicsSystem::value().Find(entity); now != nullptr)
	{
		hooks.InitialisedPhysicsFromHand(Locator::dynamicsSystem::value(), entity, *now, dontReplant);
	}
	SPDLOG_LOGGER_INFO(
	    spdlog::get("game"),
	    "Hand: released {} at ({:.1f}, {:.1f}, {:.1f}) v ({:.1f}, {:.1f}, {:.1f}) thrown {} tilt ({:.2f}, {:.2f}): {}",
	    static_cast<uint32_t>(entity), at.x, at.y, at.z, velocity.x, velocity.y, velocity.z, thrown, tilt.x, tilt.y, outcome);
	registry.SetDirty();
	return landed;
}

bool Throw(entt::entity entity, glm::vec3 velocity, bool dontReplant, std::optional<PlayerNames> throwerPlayer,
           std::optional<glm::vec3> angularMomentum)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return false;
	}
	// a pot of any class with |v|^2 <= 5 (all three axes) puts its resource down at once (the put-down effect, the
	// resource onto the ground, the pot deleted with its ghost); faster it flies as an object
	if (hand_grab::PotDissolvesOnRelease(IsPot(entity), velocity))
	{
		if (PhysicsState().handHooks.putDownHandPot)
		{
			PhysicsState().handHooks.putDownHandPot(entity);
		}
		return true;
	}
	// the class's throw before the shared one (a villager's disciple)
	Locator::dynamicsSystem::value().ThrowFromHandOfClass(entity, dontReplant);
	if (const auto landed = InitialisePhysicsFromHand(entity, velocity, dontReplant, throwerPlayer, angularMomentum))
	{
		return *landed;
	}
	// no body: an immovable thing hangs where it was let go; anything else had no mesh to build one from and is put down.
	// A villager goes back to the disciple it had as it was picked up
	if (registry.AllOf<Villager>(entity))
	{
		villager::DiscipleFlownFromHand(entity);
	}
	const bool hangs = HangsWhereLetGo(entity);
	LetGoWithoutBody(entity);
	return !hangs;
}

CappedReleaseVelocity CapReleaseVelocity(glm::vec3 handVelocity)
{
	// l2 = (z z + y y) + x x; l2 > 124 x 124 (NaN fails) and not all three 0: k = 124 / sqrt(l2),
	// v = (k x, k y, k z); the square root and the division round to float. fast = the UNCAPPED l2 > 1
	CappedReleaseVelocity out;
	glm::vec3 v = handVelocity;
	const float l2 = (v.z * v.z + v.y * v.y) + v.x * v.x;
	if (l2 > openblack::physics::k_MaxSpeed * openblack::physics::k_MaxSpeed && !(v.x == 0.0f && v.y == 0.0f && v.z == 0.0f))
	{
		const float k = openblack::physics::k_MaxSpeed / std::sqrt(l2);
		v = glm::vec3(k * v.x, k * v.y, k * v.z);
	}
	out.velocity = v;
	out.fast = l2 > 1.0f;
	return out;
}

std::optional<ReleasePrediction> PredictRelease(entt::entity entity, glm::vec3 handVelocity, glm::vec3 handAngularVelocity,
                                                uint32_t turns, const glm::vec3& drawnPosition, const glm::mat3& drawnRotation)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the old prediction ends and the new one is active with the held object
	// before the body is built
	Locator::dynamicsSystem::value().BeginPrediction(entity);
	// the body from the held object's mesh and scale, set up by it: dynamic when CanBecomeAPhysicsObject and not
	// IMMOVABLE, placed where the hand drew it (the turn's place is put back once the body is built: the release does
	// not move the object in the game's logic)
	openblack::physics::Body body;
	const bool dynamic = registry.Valid(entity) && Locator::dynamicsSystem::value().CanBecomeAPhysicsObject(entity) &&
	                     !object_flags::IsImmovable(entity);
	bool built = false;
	if (registry.Valid(entity))
	{
		auto& transform = registry.Get<Transform>(entity);
		const auto turnPosition = transform.position;
		const auto turnRotation = transform.rotation;
		transform.position = drawnPosition;
		transform.rotation = drawnRotation;
		built = Locator::dynamicsSystem::value().SetUpBody(entity, body, dynamic);
		auto& restored = registry.Get<Transform>(entity);
		restored.position = turnPosition;
		restored.rotation = turnRotation;
	}
	if (!built)
	{
		// (openblack guard) the setup cannot fail in the original; here an invalid object or one with no mesh ends it
		Locator::dynamicsSystem::value().EndPredictionOf(entity);
		return std::nullopt;
	}
	// the angular momentum from the hand's angular velocity
	body.SetAngularMomentumFromHand(handAngularVelocity);
	// Velocity = the hand's, capped at 124
	const auto capped = CapReleaseVelocity(handVelocity);
	body.velocity = capped.velocity;
	// SettleOnLand(1, (tree || fast) ? 0 : 1)
	const bool tree = registry.AnyOf<Tree, DeadTree>(entity);
	body.SettleOnLand(ecs::GameGround(), true, !(tree || capped.fast));
	// n = min(n, 15); history[i] = the pose, then 20 x ClearForces, TouchGround,
	// ApplyContacts, Integrate (its result ignored); history[n] = the final pose
	const uint32_t n = std::min(turns, 15u);
	ReleasePrediction prediction;
	prediction.history.reserve(n + 1);
	for (uint32_t i = 0; i < n; ++i)
	{
		prediction.history.emplace_back(body.Axes(), body.Centre());
		for (int s = 0; s < openblack::physics::k_StepsPerTurn; ++s)
		{
			body.ClearForces();
			body.TouchGround(ecs::GameGround());
			body.ApplyContacts();
			static_cast<void>(body.Integrate());
		}
	}
	prediction.history.emplace_back(body.Axes(), body.Centre());
	// the object's translation, Velocity, the angular momentum and the rotation rows for DecomposeYXZ
	prediction.velocity = body.velocity;
	prediction.angularMomentum = body.angularMomentum;
	prediction.origin = body.ObjectOrigin();
	prediction.rotation = body.Axes();
	// the prediction's body and history are kept for the drawing until the throw
	Locator::dynamicsSystem::value().SetPrediction(entity, std::move(body), prediction.history);
	return prediction;
}

void ApplyReleaseSpin(entt::entity entity, glm::vec3 handDelta)
{
	auto* po = Locator::dynamicsSystem::value().Find(entity);
	if (po == nullptr)
	{
		return;
	}
	const auto& v = po->body.velocity;
	const float speed = std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
	const float m = po->body.Mass();
	const float tx = -(((m * handDelta.z) * 1.6f) * speed);
	const float tz = ((m * handDelta.x) * 1.6f) * speed;
	// += into the external torque, with this port's torque sign
	po->body.externalTorque += -glm::vec3(tx, 0.0f, tz);
}

bool ForceDrop(entt::entity entity, std::optional<PlayerNames> throwerPlayer)
{
	return Throw(entity, glm::vec3(0.0f), true, throwerPlayer);
}

} // namespace openblack::ecs::physics::from_hand
