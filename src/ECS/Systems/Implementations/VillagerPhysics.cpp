/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerPhysics.h"

#include <cmath>
#include <cstdlib>

#include <functional>
#include <optional>
#include <utility>

#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/HeldApply.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Villager/VillagerBreeding.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerDrowning.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "Physics/LivingRules.h"
#include "VillagerMate.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;
using openblack::ecs::life::LifeOf;
namespace living_rules = openblack::physics::living;

namespace
{

/// The info.dat effect a physics impact applies (crush 1, alignment modification 1, radius 1)
constexpr size_t k_PhysicsImpactEffect = 3;

/// The impact's effect: the physics impact effect's numbers times the knock's share (living_rules::LivingCrush).
/// Without the info (tests) a crush of 1
effects::EffectValues ImpactEffect(float share)
{
	effects::EffectValues values;
	if (Locator::infoConstants::has_value())
	{
		values = effects::EffectValues::FromEffectInfo(Locator::infoConstants::value().effect.at(k_PhysicsImpactEffect));
	}
	else
	{
		values.numbers.at(static_cast<size_t>(effects::EffectValues::Number::Crush)) = 1.0f;
	}
	values.Scale(share);
	return values;
}

/// Whether the Living takes the effect: a villager, with a heal in it, while it is not dead, otherwise while it can be
/// reached; anything else always
bool TakesImpactEffect(entt::entity entity, const effects::EffectValues& values)
{
	if (!Locator::entitiesRegistry::value().AllOf<Villager>(entity))
	{
		return true;
	}
	if (values.numbers.at(static_cast<size_t>(effects::EffectValues::Number::Heal)) > 0.0f)
	{
		return !villager::IsDead(entity);
	}
	return villager::IsReachable(entity);
}

/// The effect's damage: its positive crush and hit, each times the Living's defence multiplier for it
float EffectDamage(entt::entity entity, const effects::EffectValues& values)
{
	const auto multipliers = effects::GetDefenseMultiplier(entity);
	float damage = 0.0f;
	for (const auto number : {effects::EffectValues::Number::Crush, effects::EffectValues::Number::Hit})
	{
		const auto i = static_cast<size_t>(number);
		const float amount = values.numbers.at(i) * multipliers.at(i);
		if (amount > 0.0f)
		{
			damage += amount;
		}
	}
	return damage;
}

/// A Living's reaction to a physics impact: above 2 G the physics impact effect (villager_physics::ImpactEffectOf), when
/// the Living takes it, goes through effects::ApplyEffect as any effect does: the life, a kill, the crushed reaction,
/// the attack on its town and the alignment. Returns false when the entity went away.
bool LivingReactToPhysicsImpact(entt::entity entity, const ImpactInfo& impact)
{
	auto values = villager_physics::ImpactEffectOf(entity, impact);
	if (!values.has_value())
	{
		return true;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	// for the log line, as the effect will take it
	[[maybe_unused]] const bool villager = registry.AllOf<Villager>(entity);
	[[maybe_unused]] const float damage = EffectDamage(entity, *values);
	// through the miracles' service as a blow: the values already say who applied it, so the source only describes it
	if (Locator::magicSystem::has_value())
	{
		const auto& thrower = impact.throwerPlayer;
		std::optional<glm::vec3> from;
		if (values->appliedBy != entt::null && Locator::dynamicsSystem::has_value())
		{
			if (const auto* body = Locator::dynamicsSystem::value().Find(values->appliedBy); body != nullptr)
			{
				from = body->body.Centre();
			}
		}
		const magic::EffectSource source {.player = thrower.value_or(PlayerNames::NEUTRAL),
		                                  .appliedBy = values->appliedBy,
		                                  .playerless = !thrower.has_value(),
		                                  .blow = true,
		                                  .point = from};
		static_cast<void>(Locator::magicSystem::value().ApplyEffectToObject(entity, *values, source));
	}
	else
	{
		effects::ApplyEffect(entity, *values);
	}
	if (registry.Valid(entity))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} hurt {:.3f}, life {:.2f}", villager ? "villager" : "animal",
		                   damage, LifeOf(entity));
	}
	return ecs::IsAvailable(entity);
}

/// The animal's reaction to a physics impact, then the Living's.
bool AnimalReactToPhysicsImpact(entt::entity entity, [[maybe_unused]] PhysicsEntry& po, const ImpactInfo& impact)
{
	auto& registry = Locator::entitiesRegistry::value();
	// hitting an available store of food, the animal available too: the store takes it with the thrower's interface
	// (its food, maybe none), and the animal goes; then nothing more
	const auto hit = impact.hitBy;
	auto& stores = ecs::resource_stores::Service();
	if (registry.AllOf<Animal>(entity) &&
	    ecs::held_apply::AnimalImpactTakes(ecs::IsAvailable(hit), ecs::IsAvailable(entity),
	                                       stores.IsStore(hit, ResourceType::Food)) &&
	    stores.TakeObject(hit, entity, impact.throwerPlayer))
	{
		return false;
	}
	return LivingReactToPhysicsImpact(entity, impact);
}

/// The animal's landing from where the bodiless path joins it (landType 3, no SetYAngle): back on the map, the
/// landType into its status, altitude 0, then the life / state work (LANDED, or dying / dead). False when the animal
/// is gone. Both paths share it
bool AnimalSharedLanding(entt::entity entity, uint16_t landType)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(entity))
	{
		return false;
	}
	// the landing's SetYAngle (or the bodiless path's redraw) has used the held y angle
	registry.Remove<villager_physics::HeldYAngle>(entity);
	auto& transform = registry.Get<Transform>(entity);
	if (Locator::terrainSystem::has_value())
	{
		transform.position.y =
		    Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	}
	// back on the map after SetYAngle, before the landType / life / state work
	if (!Locator::dynamicsSystem::value().EndPhysicsAsObject(entity))
	{
		return false; // (approximate) the original goes on with an object marked for deletion; openblack destroys it at once
	}
	// the landType, altitude and the life / state work
	ecs::animal_ai::EndPhysics(entity, landType);
	registry.SetDirty();
	return registry.Valid(entity);
}

/// The animal's landing, the class's part.
entt::entity AnimalEndPhysics(entt::entity entity, PhysicsEntry& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the landType from the turn-start matrix, the heading from the current one, back on the land (altitude 0) and out
	// of the physics; LANDED, or dying / dead. There is no drowning for animals (only a sunk corpse goes, HasSunk). The
	// rows are the original's (physics::living::BodyRows: as they are, from the hand and from the world path's
	// WorldMatrixRows(ObjectYAngle, S))
	const auto current = living_rules::BodyRows(po.body.Axes(), (po.flags & PhysicsEntry::k_FromHand) != 0);
	const auto turnStart = living_rules::BodyRows(po.turnStartRotation, (po.flags & PhysicsEntry::k_FromHand) != 0);
	// the DecomposeYXZ yaw of the current matrix: the physics sets the angles from it again after EndPhysics
	const float drawnYaw = living_rules::YawFromRotation(current);
	// the landType from the turn-start right.y
	const uint16_t landType = living_rules::AnimalLandType(turnStart[0].y);
	// SetYAngle of Wrap(GetYAngle(current fwd) + pi): the y angle and the game angle
	const float yaw = living_rules::AnimalLandingYaw(current[2]);
	ecs::animal_ai::SetYAngle(entity, yaw);
	// back on the map, the landType, the life / state work (the bodiless path's too)
	if (!AnimalSharedLanding(entity, landType))
	{
		return entt::null;
	}
	// the caller's angles from DecomposeYXZ(M): drawn at the current matrix's yaw (the game angle is kept)
	registry.Get<Transform>(entity).rotation = living_rules::DrawnRotation(drawnYaw);
	if (debug_env::TraceLogs(debug_env::AnimalTrace(), "game"))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "landing: animal {} landType {} (original right.y {:.2f}, openblack column 0 y {:.2f}) yaw {:.4f} "
		                   "drawn {:.4f} landed {}",
		                   static_cast<uint32_t>(entity), landType, turnStart[0].y, po.turnStartRotation[0].y, yaw, drawnYaw,
		                   (po.flags & PhysicsEntry::k_Landed) != 0);
	}
	registry.SetDirty();
	return entt::null;
}

entt::entity VillagerSharedLanding(entt::entity entity, uint16_t landType, std::optional<PlayerNames> throwerPlayer);

/// The landType into the villager's status bits 4-5
void SetLandType(entt::entity entity, uint16_t landType)
{
	if (auto* v = Locator::entitiesRegistry::value().TryGet<Villager>(entity))
	{
		const auto bits = static_cast<uint16_t>(landType << Villager::k_LandTypeShift);
		v->status = static_cast<uint16_t>((v->status & ~Villager::k_StatusLandTypeMask) | bits);
	}
}

/// The villager's landing, the class's part.
entt::entity VillagerEndPhysics(entt::entity entity, PhysicsEntry& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the object's landing clears its in-physics flag before the dead branches below (VillagerDead's first test)
	const ecs::villager::EndingPhysicsScope ending(entity);
	// The rows are the original's (physics::living::BodyRows: as they are, from the hand and from the world path's
	// WorldMatrixRows(ObjectYAngle, S))
	const auto current = living_rules::BodyRows(po.body.Axes(), (po.flags & PhysicsEntry::k_FromHand) != 0);
	// the DecomposeYXZ yaw of the current matrix (only y is read): the y angle before EndPhysics, and again after it (below)
	const float drawnYaw = living_rules::YawFromRotation(current);
	// the pose from the turn-start matrix (SyncTurnStart keeps it the ground-aligned one for a put-down)
	const bool fromHand = (po.flags & PhysicsEntry::k_FromHand) != 0;
	const auto pose = living_rules::VillagerLandingPoseOf(living_rules::BodyRows(po.turnStartRotation, fromHand));
	// the player who threw it makes the creature empathise (desire, amount, position). TODO(creature): po.throwerPlayer is
	// that player
	// SetYAngle: the y angle and the game angle
	ecs::villager::SetYAngle(entity, pose.yaw);
	// not put down gently (not LANDED) -> no longer marked as held: it lands as a plain villager
	if (auto* v = registry.TryGet<Villager>(entity); v != nullptr && (po.flags & PhysicsEntry::k_Landed) == 0)
	{
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagInHand);
	}
	// the caller's angles from DecomposeYXZ(M) after EndPhysics (nothing in between reads the y angle): drawn at the current
	// matrix's yaw, walking off on the game angle
	auto& transform = registry.Get<Transform>(entity);
	transform.rotation = living_rules::DrawnRotation(drawnYaw);
	if (debug_env::TraceLogs(ecs::villager::TraceOn(entity), "game"))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "landing: villager {} landType {} (original right.y {:.2f}, openblack column 0 y {:.2f}) yaw {:.4f} "
		                   "game {} "
		                   "drawn {:.4f} landed {}",
		                   static_cast<uint32_t>(entity), pose.landType,
		                   living_rules::BodyRows(po.turnStartRotation, fromHand)[0].y, po.turnStartRotation[0].y, pose.yaw,
		                   ecs::villager::GetGameAngle(entity), drawnYaw, (po.flags & PhysicsEntry::k_Landed) != 0);
	}
	// back on the map, the landType, the water / death branches, LANDED (the bodiless path's too)
	return VillagerSharedLanding(entity, pose.landType, po.throwerPlayer);
}

/// The villager's landing from where the bodiless path joins it (landType 3, no pose, no empathy, no SetYAngle, still
/// marked as held). throwerPlayer = the player who threw it; on the bodiless path the player is never asked: none. Both
/// paths share it
entt::entity VillagerSharedLanding(entt::entity entity, uint16_t landType, std::optional<PlayerNames> throwerPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the landing's SetYAngle (or the bodiless path's redraw) has used the held y angle
	registry.Remove<villager_physics::HeldYAngle>(entity);
	auto& transform = registry.Get<Transform>(entity);
	if (Locator::terrainSystem::has_value())
	{
		transform.position.y =
		    Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	}
	// back on the map after SetYAngle, before the landType, the water test and the state / death work
	if (!Locator::dynamicsSystem::value().EndPhysicsAsObject(entity))
	{
		return entt::null; // (approximate) the original goes on with an object marked for deletion; openblack destroys it at
		                   // once
	}
	// the landType into the status bits 4-5, after the object's landing; altitude 0: the position is on the ground
	// (above)
	SetLandType(entity, landType);
	// the water test of the position (the cell's water bit, the shallow shore too; not the body's inWater)
	if (ecs::sea_cells::IsWater(transform.position))
	{
		// alive: the last player to interact is the thrower, none without a body; dead: VillagerDead(6, the thrower or,
		// without a body, the neutral player, so NEUTRAL in VillagerDeadDrowned), 0.01, 1)
		ecs::RememberLastPlayerToInteract(entity, throwerPlayer);
		ecs::VillagerEndPhysicsInWater(entity);
		return entt::null;
	}
	// on land with life <= 0: player = the thrower (as VillagerDrowning's last player), none without a body; dead already
	// -> SetTopState(DEAD) with its counter kept, else VillagerDead(5 PLAYER_INTERACTION, player, 0.0, 1); then the
	// landType again (SetDying's change undone)
	if (LifeOf(entity) <= 0.0f)
	{
		const auto player = throwerPlayer;
		const auto* v = registry.TryGet<const Villager>(entity);
		if (v != nullptr && (v->status & Villager::k_StatusDead) != 0)
		{
			ecs::villager::SetTopState(entity, VillagerStates::Dead);
		}
		else
		{
			ecs::villager::VillagerDead(entity, DeathReason::PlayerInteraction, player, 0.0f, 1);
		}
		SetLandType(entity, landType);
		registry.SetDirty();
		return entt::null;
	}
	// LANDED (its landing clip by the landType, then the villager's Landed), or back to what it was doing
	villager_physics::SetLandedState(entity);
	registry.SetDirty();
	return entity;
}

/// The class's InitialisePhysics for a knocked resting proxy (PhysicsClassHooks::InitialisePhysicsKnocked), on top of
/// its without-body part: the same Villager / Living InitialisePhysics runs, only the body is the proxy's own
bool KnockedInitialisePhysics(entt::entity entity, const std::function<bool(entt::entity)>& classPart)
{
	// the physics asks CanBecomeAPhysicsObject and the carried flag before the call
	if (!Locator::dynamicsSystem::value().CanWakeKnockedProxy(entity))
	{
		return false;
	}
	// the villager's InitialisePhysics (CreateDroppedResource with v = w = 0) / the Living's: refused when
	// SetTopState(FLYING) does not take
	if (!classPart || !classPart(entity))
	{
		return false;
	}
	// the object's InitialisePhysics, last: started
	return Locator::dynamicsSystem::value().CanWakeKnockedProxy(entity);
}

} // namespace

namespace openblack::ecs::villager_physics
{

// The Villager class's hooks (ecs::PhysicsGameHooks)

namespace
{
/// The Living's start of physics: FLYING already -> taken, nothing changed; else StorePreviousState unless IN_HAND, and
/// SetTopState(FLYING) must return 1, else refused
bool LivingStartPhysics(entt::entity entity)
{
	const auto* action = Locator::entitiesRegistry::value().TryGet<const components::LivingAction>(entity);
	if (action == nullptr)
	{
		return false;
	}
	const auto top = static_cast<VillagerStates>(action->states[static_cast<size_t>(components::LivingAction::Index::Top)]);
	if (top == VillagerStates::Flying)
	{
		return true;
	}
	if (top != VillagerStates::InHand)
	{
		villager::StorePreviousState(entity);
	}
	return villager::SetTopState(entity, VillagerStates::Flying) == 1;
}
} // namespace

bool VillagerStartPhysicsInWorld(entt::entity entity, glm::vec3 velocity, glm::vec3 spin)
{
	// The villager's InitialisePhysics in the world, before the body: CreateDroppedResource(v, w, none), the log
	// thrown with the villager's own velocity and spin, then the Living's start, which may refuse (then no physics)
	villager::CreateDroppedResource(entity, velocity, spin, std::nullopt);
	return LivingStartPhysics(entity);
}

void VillagerBeforeStartFromHand(entt::entity entity)
{
	// a dancer leaves its dance first
	dances::RemoveDancer(Locator::entitiesRegistry::value(), entity);
	const auto* action = Locator::entitiesRegistry::value().TryGet<const components::LivingAction>(entity);
	if (action != nullptr &&
	    static_cast<VillagerStates>(action->states[static_cast<size_t>(components::LivingAction::Index::Top)]) !=
	        VillagerStates::InHand)
	{
		villager::StorePreviousState(entity);
	}
}

void VillagerStartPhysics(entt::entity entity, [[maybe_unused]] PhysicsEntry& po, bool fromHand)
{
	// In the world the villager's own start ran before the body was made (VillagerStartPhysicsInWorld): it is already
	// FLYING, so here only its walk is dropped and it is redrawn.
	// The Living's InitialisePhysics: the villager flies (THROWN clips, ECS/VillagerAnimations).
	// From the hand (fromHand) the physics calls it only when the villager did not land (a body for this object, not
	// LANDED -> SetTopState(FLYING); DynamicsSystem::InitialisePhysicsOfClass): a villager put down gently goes IN_HAND
	// -> LANDED through EndPhysics and stays marked as held. From the hand it flies: back to the disciple it had as it
	// was picked up (villager::DiscipleFlownFromHand; with no body ECS/Physics/FromHand does it); from the hand the
	// physics calls CreateDroppedResource itself (ECS/Physics/FromHand), and its StorePreviousState came before the
	// object's start (VillagerBeforeStartFromHand).
	// FLYING through the old state's exit, so a reaction ends; already FLYING, nothing
	if (fromHand)
	{
		villager::DiscipleFlownFromHand(entity);
	}
	ecs::SetVillagerStateWithExit(entity, VillagerStates::Flying);
}

bool VillagerReactToImpact(entt::entity entity, const ImpactInfo& impact)
{
	// the Living's reaction to an impact (the villager shares it)
	return LivingReactToPhysicsImpact(entity, impact);
}

entt::entity VillagerEndPhysicsOfClass(entt::entity entity, PhysicsEntry& po)
{
	return VillagerEndPhysics(entity, po);
}

bool VillagerInitialisePhysicsWithoutBody(entt::entity entity)
{
	// The villager's InitialisePhysics without a body (a particle system takes it): CreateDroppedResource with the
	// take's own velocity and spin (k_ParticleTakeVelocity, k_ParticleTakeSpin), then the Living's: FLYING already ->
	// taken; else StorePreviousState unless IN_HAND, and SetTopState(FLYING) must return 1, else refused
	villager::CreateDroppedResource(entity, k_ParticleTakeVelocity, k_ParticleTakeSpin, std::nullopt);
	return LivingStartPhysics(entity);
}

bool VillagerInitialisePhysicsKnocked(entt::entity entity)
{
	// the villager's InitialisePhysics for a knocked proxy: CreateDroppedResource with no velocity and no spin, the
	// Living's start, then the object's
	return KnockedInitialisePhysics(entity, [](entt::entity knocked) {
		villager::CreateDroppedResource(knocked, glm::vec3(0.0f), glm::vec3(0.0f), std::nullopt);
		return LivingStartPhysics(knocked);
	});
}

// The Animal class's hooks (ecs::PhysicsGameHooks); from the hand: FLYING, the species' THROWN clip
// (animal_ai::InitialisePhysics), and the Living's InitialisePhysics without a body
// (animal_ai::InitialisePhysicsWithoutBody)

bool AnimalReactToImpact(entt::entity entity, PhysicsEntry& po, const ImpactInfo& impact)
{
	return AnimalReactToPhysicsImpact(entity, po, impact);
}

entt::entity AnimalEndPhysicsOfClass(entt::entity entity, PhysicsEntry& po)
{
	return AnimalEndPhysics(entity, po);
}

bool AnimalInitialisePhysicsKnocked(entt::entity entity)
{
	// the Living's InitialisePhysics for a knocked proxy (the animal has no override)
	return KnockedInitialisePhysics(entity, ecs::animal_ai::InitialisePhysicsWithoutBody);
}

void InterfaceSetInMagicHand(entt::entity entity, std::optional<PlayerNames> handPlayer)
{
	// marked as held: read when the villager lands and cleared by EndPhysics (a thrown landing) or by Landed
	if (auto* v = Locator::entitiesRegistry::value().TryGet<Villager>(entity); v != nullptr)
	{
		v->flags = static_cast<uint16_t>(v->flags | Villager::k_FlagInHand);
		// a villager taken mid-kiss lets its partner go, who decides afresh
		ecs::villager::LetPartnerGo(entity);
		// its own player's villager of breeding age then calls those of the other sex to it
		ecs::villager_mate::HeldByHand(entity, handPlayer);
	}
	// the y angle while in the hand: nothing of the hand writes it (HandStateHolding builds only the drawn matrix, no
	// hold rotation), so it keeps the value from before the pick-up; openblack keeps it in HeldYAngle until the landing's
	// SetYAngle (EndPhysics / EndPhysicsWithoutBody)
	if (auto& registry = Locator::entitiesRegistry::value(); registry.AnyOf<Villager, Animal>(entity))
	{
		registry.AssignOrReplace<HeldYAngle>(entity, ObjectYAngle(entity));
	}
	// the Living: its state kept as its previous one, then IN_HAND (its clip SCARED_STIFF, ECS/VillagerAnimations),
	// through the old state's exit, so a reaction ends
	if (Locator::entitiesRegistry::value().AllOf<Villager>(entity))
	{
		villager::StorePreviousState(entity);
	}
	ecs::SetVillagerStateWithExit(entity, VillagerStates::InHand);
	// the animal: off its flock, IN_HAND
	if (Locator::entitiesRegistry::value().AllOf<Animal>(entity))
	{
		ecs::animal_ai::PlaceInHand(entity);
	}
}

void InterfaceSetOutMagicHand(entt::entity entity)
{
	// what every object does: its fire effect, if any, is told it left the hand (fire::SetOutMagicHand: nothing without a
	// fire)
	fire::SetOutMagicHand(entity);
}

void EndPhysicsWithoutBody(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(entity) || !registry.AllOf<Transform>(entity))
	{
		return;
	}
	const bool villager = registry.AllOf<Villager>(entity);
	if (!villager && !registry.AllOf<Animal>(entity))
	{
		return;
	}
	// from the hand with no body (or flying) the villager's previous disciple has come back (ECS/Physics/FromHand,
	// VillagerStartPhysics). The object's landing with no body too: the object's flying-object reaction (9) goes (as the
	// physics' EndPhysicsOfClass does for a body) and ConsiderCreatureMimickingWhenObjectLands (TODO(creature))
	ecs::animal_ai::EndReactionsOf(entity);
	// (openblack) the drawn rotation without the hand's tilt: the original draws a Living from its y angle alone and
	// the bodiless landing does not touch it (no SetYAngle, no caller's angles)
	registry.Get<Transform>(entity).rotation = living_rules::DrawnRotation(ObjectYAngle(entity));
	if (villager)
	{
		// the object's landing clears its in-physics flag with no body too (VillagerDead's first test)
		const ecs::villager::EndingPhysicsScope ending(entity);
		// landType 3, straight to the shared part
		VillagerSharedLanding(entity, living_rules::k_LandTypeWithoutBody, std::nullopt);
		return;
	}
	// landType 3, straight to the shared part
	AnimalSharedLanding(entity, living_rules::k_LandTypeWithoutBody);
}

float ObjectYAngle(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return 0.0f;
	}
	const auto& drawn = transform->rotation;
	// held (or flying after the hand): the y angle kept from before the pick-up (InterfaceSetInMagicHand)
	if (const auto* held = registry.TryGet<const HeldYAngle>(entity); held != nullptr)
	{
		return held->value;
	}
	// a walking object's game angle sets its y angle through ConvertGameAngleTo3D, so the y angle is that float.
	// openblack keeps it in WallHug::yAngle (villager::SetGameAngle) and the animal's game angle in
	// AnimalBrain::angle (FaceAngle draws ConvertGameAngleTo3D(angle) + pi / 2, bit for bit DrawnRotation's): exact when
	// the Transform is still the one drawn from it
	if (const auto* wallHug = registry.TryGet<const WallHug>(entity);
	    wallHug != nullptr && living_rules::DrawnRotation(wallHug->yAngle) == drawn)
	{
		return wallHug->yAngle;
	}
	if (const auto* brain = registry.TryGet<const AnimalBrain>(entity); brain != nullptr)
	{
		const float angle = gutils::ConvertGameAngleTo3D(brain->angle);
		if (living_rules::DrawnRotation(angle) == drawn)
		{
			return angle;
		}
	}
	// any other writer (EndPhysics' SetYAngle then the caller's angles from DecomposeYXZ(M)): openblack keeps only the drawn
	// matrix, so its DecomposeYXZ yaw through the original's rows. (approximate) within the rounding of the matrix's
	// floats (cos / sin of the y angle + pi / 2), in DecomposeYXZ's range (-pi, pi]
	return living_rules::YawFromRotation(living_rules::OriginalRows(drawn));
}

void SetLandedState(entt::entity villager)
{
	// TODO(villager-jobs): first, a villager marked as special leaves its town, or its script lets it go when it has no
	// town, and loses the mark. Nothing marks a villager so yet
	// the state it was in when it began to fly, read before LANDED replaces the top state
	const auto previous = villager::GetState(villager, villager::Index::Previous);
	ecs::SetVillagerState(villager, VillagerStates::Landed);
	if (villager::IsScriptControlled(villager) ||
	    villager::state_info::GoesBackAfterLanding(villager::state_info::StateInfo(previous)))
	{
		ecs::SetVillagerState(villager, previous);
	}
}

std::optional<float> ImpactDamage(entt::entity entity, float g)
{
	const auto share = living_rules::LivingCrush(g);
	if (!share.has_value())
	{
		return std::nullopt;
	}
	const auto values = ImpactEffect(*share);
	if (!TakesImpactEffect(entity, values))
	{
		return std::nullopt;
	}
	return EffectDamage(entity, values);
}

std::optional<effects::EffectValues> ImpactEffectOf(entt::entity entity, const ImpactInfo& impact)
{
	const auto share = living_rules::LivingCrush(impact.g);
	if (!share.has_value())
	{
		return std::nullopt;
	}
	auto values = ImpactEffect(*share);
	if (!TakesImpactEffect(entity, values))
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	// what struck it applies the effect: none for a fall onto the land, or for a hitter already gone
	const bool struck = impact.hitBy != entt::null && registry.Valid(impact.hitBy);
	values.appliedBy = struck ? impact.hitBy : entt::entity {entt::null};
	values.appliedByCreature = struck && registry.AllOf<Creature>(impact.hitBy);
	// the player who threw it, which the town's attack takes first
	values.causedPlayer = impact.throwerPlayer;
	// whose alignment moves: what struck it answers for itself, even with no player; with nothing, the thrower's, else
	// the neutral player's
	values.player = effects::EffectPlayer(struck, town_emergency::HitterPlayer(values.appliedBy), values.causedPlayer);
	return values;
}

} // namespace openblack::ecs::villager_physics
