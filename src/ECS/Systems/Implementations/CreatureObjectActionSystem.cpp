/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureObjectActionSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <limits>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandAvoid.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Common/GameDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureCatch.h"
#include "Creature/CreatureEating.h"
#include "Creature/CreatureGiving.h"
#include "Creature/CreatureHold.h"
#include "Creature/CreatureKiss.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureObjectActions.h"
#include "Creature/CreaturePickUpRules.h"
#include "Creature/CreatureReach.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureThrow.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/CreaturePose.h"
#include "ECS/CreatureStruck.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/HoldLowering.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectFlags.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PhysicsGround.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/RewardRules.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "EngineConfig.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "InfoConstants.h"
#include "Input/LocalPlayer.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Physics/LivingRules.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;
using openblack::creature_object_actions::Kind;
using openblack::creature_object_actions::Status;
using Phase = openblack::ecs::components::CreatureObjectAction::Phase;

namespace
{
/// Whether a held thing is drawn with its blob shadow on the ground (a villager's or an animal's), and setting it
bool IsHumanShadowed(const ecs::Registry& registry, entt::entity object)
{
	if (const auto* villager = registry.TryGet<const Villager>(object))
	{
		return villager->humanShadowed;
	}
	const auto* animal = registry.TryGet<const Animal>(object);
	return animal != nullptr && animal->humanShadowed;
}

void SetHumanShadowed(ecs::Registry& registry, entt::entity object, bool shadowed)
{
	if (auto* villager = registry.TryGet<Villager>(object))
	{
		villager->humanShadowed = shadowed;
	}
	if (auto* animal = registry.TryGet<Animal>(object))
	{
		animal->humanShadowed = shadowed;
	}
}

/// Taking hold of a thing drawn with a blob shadow turns the shadow off; whether it did
bool TakeShadow(ecs::Registry& registry, entt::entity object)
{
	if (!IsHumanShadowed(std::as_const(registry), object))
	{
		return false;
	}
	SetHumanShadowed(registry, object, false);
	return true;
}
} // namespace

namespace
{
/// A turn's milliseconds, as the game runs
float TurnMs()
{
	return static_cast<float>(game_clock::MsPerTurn());
}
/// The meshes look back along their z axis: ahead is -z
constexpr glm::vec3 k_MeshBack {0.0f, 0.0f, 1.0f};
/// Turning to face what it throws at or points at, it tries this many times
constexpr uint32_t k_TurnAttempts = 2;
/// Nothing smaller than this counts as a distance
constexpr float k_Tiny = 1e-4f;

const CreatureRig* RigOf(const Creature& creature)
{
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(creature.species);
	return rigs.Contains(id) ? &*rigs.Handle(id) : nullptr;
}

const CreatureRig::ActionPoints* PointsOf(const Creature& creature)
{
	const auto* rig = RigOf(creature);
	return rig != nullptr && rig->actionPoints.has_value() ? &*rig->actionPoints : nullptr;
}

/// The moment of the kissing animations the species' file measures its kissing points at
int32_t KissTimeOf(const Creature& creature)
{
	const auto* rig = RigOf(creature);
	return rig != nullptr ? rig->kissTimeMs : 0;
}

/// How far up its kiss a creature kisses another, at the two creatures' drawn sizes; none when they can't kiss or
/// either has no kissing points
std::optional<float> KissShareWith(entt::entity creature, entt::entity other)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto own = animations.KissPointsOf(creature);
	const auto theirs = animations.KissPointsOf(other);
	if (!own.has_value() || !theirs.has_value())
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return creature_kiss::KissShare(*own, creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, creature)),
	                                *theirs, creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, other)));
}

glm::mat4 PlacementOf(const Transform& transform)
{
	return creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
}

/// A point of the world in the creature's own space, kept at the world's scale
glm::vec3 ToLocal(const Transform& transform, const glm::vec3& world)
{
	return glm::transpose(transform.rotation) * (world - transform.position);
}

/// How far round from ahead a point is, along the ground
float BearingTo(const Transform& transform, const glm::vec3& target)
{
	const auto local = ToLocal(transform, target);
	const glm::vec2 ahead {0.0f, -1.0f};
	const glm::vec2 towards {local.x, local.z};
	if (glm::length(towards) < k_Tiny)
	{
		return 0.0f;
	}
	return std::acos(std::clamp(glm::dot(ahead, glm::normalize(towards)), -1.0f, 1.0f));
}

/// The hand a creature acts with: the right, or its mirror
uint32_t HandBone(const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, bool mirrored)
{
	return mirrored && points.rightHand < animation.mirror.size() ? animation.mirror[points.rightHand] : points.rightHand;
}

/// Where the palm is: among the hand bone and the fingers that hang from it, posed by `bones`
glm::vec3 PalmOf(uint32_t hand, const CreatureAnimation& animation, std::span<const glm::mat4> bones,
                 const glm::mat4& placement)
{
	auto sum = glm::vec3(creature::PosedBone(hand, bones, placement)[3]);
	float count = 1.0f;
	const auto& parents = animation.skeleton.parents;
	for (uint32_t bone = 0; bone < parents.size(); ++bone)
	{
		if (parents[bone] == hand)
		{
			sum += glm::vec3(creature::PosedBone(bone, bones, placement)[3]);
			count += 1.0f;
		}
	}
	return sum / count;
}

/// Where the palm is as the body is posed this frame
glm::vec3 PalmOf(uint32_t hand, const CreatureAnimation& animation, const glm::mat4& placement)
{
	return PalmOf(hand, animation, animation.boneMatrices, placement);
}

/// How what a hand holds sits in it, from the bones the body is posed with under a placement: turned by the hand's bone,
/// hung below its grasp point by the thing's height times its hold lowering, at the thing's scale
creature_hold::HeldPlacement HeldPlacementOf(const ecs::Registry& registry, const CreatureHeldObject& held,
                                             const CreatureAnimation& animation, std::span<const glm::mat4> bones,
                                             const glm::mat4& placement)
{
	const auto hand = glm::mat3(creature::PosedBone(held.bone, bones, placement));
	return creature_hold::Place(hand, PalmOf(held.bone, animation, bones, placement), held.mirrored,
	                            ecs::object::GetScale(held.object), ecs::hold_lowering::LoweringOf(registry, held.object));
}

/// What a hand puts down, tosses away or throws takes the angles of its hand's bone as the body was posed for the last
/// turn (the frame's pose under its Transform when it has none), at its scale. A creature keeps its own
void TurnAsTheHandLetsGo(ecs::Registry& registry, entt::entity creature, const CreatureHeldObject& held)
{
	const auto& lookup = std::as_const(registry);
	const auto* animation = lookup.TryGet<const CreatureAnimation>(creature);
	const auto* transform = lookup.TryGet<const Transform>(creature);
	if (animation == nullptr || transform == nullptr || !lookup.Valid(held.object) || lookup.AllOf<Creature>(held.object))
	{
		return;
	}
	const auto* pose = lookup.TryGet<const CreatureTurnPose>(creature);
	const bool turnPosed = pose != nullptr && pose->current.size() == animation->boneMatrices.size();
	const auto hand = glm::mat3(turnPosed ? creature::PosedBone(held.bone, pose->current, glm::mat4(1.0f))
	                                      : creature::PosedBone(held.bone, animation->boneMatrices, PlacementOf(*transform)));
	static_cast<void>(ecs::placed_turn::SetAngles(
	    registry, held.object, creature_hold::AnglesAtRelease(hand, held.mirrored, ecs::object::GetScale(held.object))));
}

/// Where the middle of something is from where it stands, by its mesh's bounds, in its own space at the world's scale
glm::vec3 MiddleOf(const ecs::Registry& registry, entt::entity entity)
{
	const auto* mesh = registry.TryGet<const Mesh>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
	{
		return glm::vec3(0.0f);
	}
	return meshes.Handle(mesh->id)->GetBoundingBox().Center() * transform->scale;
}

/// Where a bone is in an animation at a time, in the creature's own space at the world's scale
std::optional<glm::vec3> BoneAt(entt::entity creature, const Transform& transform, uint32_t bone, size_t animation,
                                float timeMs)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	const auto at = Locator::creatureAnimationSystem::value().BoneInAnimation(creature, animation, timeMs, bone, false);
	return at.has_value() ? std::optional(*at * transform.scale) : std::nullopt;
}

/// Where the right hand is in an animation at a time, in the creature's own space at the world's scale
std::optional<glm::vec3> HandAt(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                size_t animation, float timeMs)
{
	return BoneAt(creature, transform, points.rightHand, animation, timeMs);
}

/// Where the palm is at a moment of the action's animations, as the body plays them blended, in the world: the hand
/// bone and the fingers that hang from it, in each animation at that time, weighed as the animations are. Nothing when
/// the creature hasn't been posed.
std::optional<glm::vec3> PalmAtMoment(entt::entity creature, const CreatureObjectAction& action,
                                      const CreatureRig::ActionPoints& points, const CreatureAnimation& animation,
                                      const Transform& transform)
{
	if (!Locator::creatureAnimationSystem::has_value() || action.animationCount == 0)
	{
		return std::nullopt;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const auto hand = HandBone(points, animation, action.mirrored);
	std::vector<uint32_t> bones {hand};
	for (uint32_t bone = 0; bone < animation.skeleton.parents.size(); ++bone)
	{
		if (animation.skeleton.parents[bone] == hand)
		{
			bones.push_back(bone);
		}
	}
	glm::vec3 sum {0.0f};
	float weights = 0.0f;
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		glm::vec3 palm {0.0f};
		float count = 0.0f;
		for (const auto bone : bones)
		{
			if (const auto at =
			        animations.BoneInAnimation(creature, action.animations.at(i), action.eventMs, bone, action.mirrored))
			{
				palm += *at;
				count += 1.0f;
			}
		}
		if (count > 0.0f)
		{
			sum += (palm / count) * action.weights.at(i);
			weights += action.weights.at(i);
		}
	}
	if (weights <= 0.0f)
	{
		return std::nullopt;
	}
	return glm::vec3(PlacementOf(transform) * glm::vec4(sum / weights, 1.0f));
}

/// Where the hand grasps in an animation at a time, in the creature's own space at the world's scale
std::optional<glm::vec3> GraspAt(entt::entity creature, const Transform& transform, uint32_t hand, size_t animation,
                                 float timeMs)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	const auto at = Locator::creatureAnimationSystem::value().GraspInAnimation(creature, animation, timeMs, hand);
	// a body that knows no grasp, as the tests' fakes, reaches from the hand bone itself
	return at.has_value() ? std::optional(*at * transform.scale) : BoneAt(creature, transform, hand, animation, timeMs);
}

/// Where it acts from in each of four reaching animations: where the right hand grasps reaching to pick something up
/// (the mean of the ends under the hand bone, as the game's pick-up places are), the right foot itself striking a blow
std::optional<creature_reach::Points> MeasureReach(entt::entity creature, const Transform& transform, uint32_t bone,
                                                   const std::array<size_t, creature_reach::k_CornerCount>& animations,
                                                   float timeMs, bool grasp)
{
	creature_reach::Points reach {};
	for (size_t i = 0; i < animations.size(); ++i)
	{
		const auto hand = grasp ? GraspAt(creature, transform, bone, animations.at(i), timeMs)
		                        : BoneAt(creature, transform, bone, animations.at(i), timeMs);
		if (!hand.has_value())
		{
			return std::nullopt;
		}
		reach.at(i) = *hand;
	}
	return reach;
}

float DurationOf(entt::entity creature, size_t animation)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return 0.0f;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation).value_or(0.0f);
}

/// How long each of the animations an action plays lasts
std::array<float, 4> DurationsOf(entt::entity creature, const CreatureObjectAction& action)
{
	std::array<float, 4> durations {};
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		durations.at(i) = DurationOf(creature, action.animations.at(i));
	}
	return durations;
}

/// Whether the action points still: its time rises to the point moment and holds there
bool IsHoldingPoint(const CreatureObjectAction& action)
{
	return action.kind == Kind::Point && action.holdMs > 0.0f;
}

/// How heavy something carried is: a creature's mass, a reward chest's fixed weight whatever its size, and anything
/// else its own weight as an object, its scale cubed times the weight of its row in the game's tables
std::optional<float> WeightOf(const ecs::Registry& registry, entt::entity entity)
{
	if (registry.AllOf<Creature>(entity))
	{
		return ecs::creature_physics::Weight(registry, entity);
	}
	if (registry.AllOf<Reward>(entity))
	{
		return ecs::reward::k_Weight;
	}
	if (!Locator::dynamicsSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::dynamicsSystem::value().ObjectWeight(entity);
}

/// Something eaten is gone: a person first dies to the creature's player, as any death goes (its town counts it, its
/// owner's alignment moves, it lets go of what it carries); then a villager or an animal dies, which takes it out of
/// its home, its town and its flock; anything else is deleted
void Consume(const ecs::Registry& registry, entt::entity food, entt::entity creature)
{
	if (const auto* person = registry.TryGet<const Villager>(food); person != nullptr)
	{
		const auto* eater = registry.TryGet<const Creature>(creature);
		const auto death =
		    creature_eating::EatenPersonDeathOf(eater != nullptr ? eater->owner : PlayerNames::NEUTRAL, person->life);
		ecs::villager::VillagerDead(food, death.reason, death.killer, death.amount, death.drop);
	}
	if (registry.AnyOf<Villager, Animal>(food))
	{
		ecs::life::Kill(food, "eaten by a creature");
	}
	else
	{
		ecs::ToBeDeleted(food);
	}
}

/// The class of a mobile object as its row of the game's table makes it, none without the table
std::optional<MobileObjectInfo> MobileClassOf(const ecs::Registry& registry, entt::entity object)
{
	const auto* mobile = registry.TryGet<const MobileObject>(object);
	if (mobile == nullptr || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& rows = Locator::infoConstants::value().mobileObject;
	const auto row = static_cast<size_t>(mobile->type);
	return row < rows.size() ? std::optional(rows.at(row).mobileType) : std::nullopt;
}

/// A mushroom, which a creature always eats: a mobile object of the magic mushroom or toadstool class, or the magic
/// mushroom feature
bool IsMushroom(const ecs::Registry& registry, entt::entity object)
{
	if (const auto* feature = registry.TryGet<const Feature>(object))
	{
		return feature->type == FeatureInfo::MagicMushroom;
	}
	const auto mobileClass = MobileClassOf(registry, object);
	return mobileClass == MobileObjectInfo::MagicMushroom || mobileClass == MobileObjectInfo::Toadstool;
}

/// Something poisoned, which a creature spits out: a poisoned pot or pile, a toadstool, a poisoned villager or animal
bool IsPoisonedFood(const ecs::Registry& registry, entt::entity object)
{
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return pot->poisoned;
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		return mobile->type == MobileObjectInfo::Toadstool;
	}
	return registry.AnyOf<Villager, Animal>(object) && ecs::life::IsPoisoned(object);
}

/// The sound of a creature's knock-down on a home, from the editor bank's table
constexpr std::array<int32_t, 5> k_KnockDownHomeSound {2, 1, 1, 9, 75};
/// The knock-down's blow on a home strikes straight down
constexpr glm::vec3 k_KnockDownHomeBlow {0.0f, -1.0f, 0.0f};

/// A creature's knock-down on a home: a sound at the home, then one blow straight down on it where it stands, as wide as
/// the kick's reach for the creature's size, that breaks pieces off it; when something broke off it takes the damage for
/// the creature's player. Unlike the kick, a home not yet built or on fire is struck too
void StrikeHome(entt::entity creature, entt::entity home)
{
	if (!Locator::buildingDamageSystem::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto at = registry.Get<const Transform>(home).position;
	Locator::dynamicsSystem::value().PlayAnimEffect(k_KnockDownHomeSound, home, at, false);
	const float size = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, creature));
	const std::array strikes {BuildingDamageSystemInterface::BodilessBlow {.point = at, .velocity = k_KnockDownHomeBlow}};
	const auto owner = registry.Get<const Creature>(creature).owner;
	static_cast<void>(Locator::buildingDamageSystem::value().StrikeWithoutBody(
	    home, creature, owner != PlayerNames::NEUTRAL ? std::optional(owner) : std::nullopt,
	    size * creature_object_actions::k_HomeKickReachPerSize, strikes));
}

/// A standing tree kicked creaks as it does when the hand pulls one up, from the second set of those sounds
constexpr int k_KickedTreeCreak = 136;
constexpr int k_TreeCreakCount = 3;
/// The sound of a kicked home, villager or fence: the editor bank's row for the kick on the thing's surface
constexpr int32_t k_KickSoundEvent = 150;
constexpr int32_t k_KickedHomeSurface = 19;
constexpr int32_t k_KickedVillagerSurface = 22;
constexpr int32_t k_KickedFenceSurface = 25;
/// The effect info's row of the crush a kicked creature takes
constexpr size_t k_KickCrushEffect = ecs::creature_struck::k_CrushEffect;

/// Where the foot meets what it kicks, in the world: where the right foot grasps in the kicking animation, measured at
/// the moment the reaching hand takes what the player's hand gives it (the moment the game measures it at, not the
/// kick's own); where the creature stands when it has no kicking animation. The body is placed leaned onto the slope it
/// stands on, as it is posed for the turn
glm::vec3 KickPoint(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points)
{
	std::optional<glm::vec3> grasp;
	if (Locator::creatureAnimationSystem::has_value())
	{
		grasp = Locator::creatureAnimationSystem::value().GraspInAnimation(creature, creature_object_actions::k_KickAnimation,
		                                                                   points.takeFromHandMs, points.rightFoot);
	}
	const auto placement = ecs::creature_pose::LeanedModel(std::as_const(Locator::entitiesRegistry::value()), creature,
	                                                       transform.position, transform.rotation, transform.scale);
	return glm::vec3(placement * glm::vec4(grasp.value_or(glm::vec3(0.0f)), 1.0f));
}

/// The way a creature kicks: ahead as it faces, leaning with the land under it, and its size
struct KickWay
{
	glm::vec2 forward;
	glm::vec2 lean;
	float size;
};
KickWay KickWayOf(entt::entity creature, const Transform& transform)
{
	const glm::vec2 at {transform.position.x, transform.position.z};
	const auto radius = ecs::object::Get2DRadius(creature);
	const auto height = [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	};
	const auto lean =
	    creature_object_actions::LeanOf(height(at + glm::vec2(radius, 0.0f)), height(at - glm::vec2(radius, 0.0f)),
	                                    height(at + glm::vec2(0.0f, radius)), height(at - glm::vec2(0.0f, radius)), radius);
	const auto ahead = transform.rotation * -k_MeshBack;
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return {.forward = glm::normalize(glm::xz(ahead)),
	        .lean = lean,
	        .size = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, creature))};
}

/// The kick of a home: a built home that is not burning has pieces broken off it by two blows of the foot, low on it,
/// half way from where the creature stands to where the foot meets it (three of the game's random numbers drawn), and
/// takes the damage for the creature's player when something broke off
void KickHome(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points, entt::entity home)
{
	if (!Locator::buildingDamageSystem::has_value() ||
	    !creature_object_actions::CanHomeBeKicked(ecs::fire::IsOnFire(home), ecs::abodes::IsBuilt(home)))
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto way = KickWayOf(creature, transform);
	const auto centre = creature_object_actions::HomeKickCentre(transform.position, KickPoint(creature, transform, points));
	const auto turnDraw = game_random::GameRand(creature_object_actions::k_HomeKickTurns);
	const auto firstHeightDraw = game_random::GameRand(creature_object_actions::k_HomeKickHeights);
	const auto secondHeightDraw = game_random::GameRand(creature_object_actions::k_HomeKickHeights);
	// The land under that point, at the point the game's land lookup rounds it to. (inferred) the land's height stands
	// for the game's altitude there, as for the hand
	const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
	const float altitude = Locator::terrainSystem::has_value()
	                           ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(lookup(centre.x), lookup(centre.z)))
	                           : 0.0f;
	// The game's heading turns the other way round from ours: it faces (sin, -cos) of it, ours (-sin, -cos)
	const auto* motion = registry.TryGet<const CreatureLocomotion>(creature);
	const float heading =
	    motion != nullptr && motion->started ? motion->heading : ecs::creature_pose::ReadHeading(transform.rotation);
	const auto blows = creature_object_actions::HomeKickBlows(
	    centre, altitude, creature_object_actions::KickDirection(way.forward, way.lean), -heading, way.size,
	    ecs::object::Get2DRadius(home), turnDraw, firstHeightDraw, secondHeightDraw);
	const std::array strikes {
	    BuildingDamageSystemInterface::BodilessBlow {.point = blows.firstPoint, .velocity = blows.firstVelocity},
	    BuildingDamageSystemInterface::BodilessBlow {.point = blows.secondPoint, .velocity = blows.secondVelocity},
	};
	const auto owner = registry.Get<const Creature>(creature).owner;
	static_cast<void>(Locator::buildingDamageSystem::value().StrikeWithoutBody(
	    home, creature, owner != PlayerNames::NEUTRAL ? std::optional(owner) : std::nullopt, blows.radius, strikes));
}

/// A creature kicked by another takes the effect info's crush, applied by the kicker, scaled by their heights
/// (creature_object_actions::KickCrushShare)
void KickCrush(entt::entity creature, entt::entity target)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	auto values = ecs::effects::EffectValues::FromEffectInfo(Locator::infoConstants::value().effect.at(k_KickCrushEffect));
	values.appliedBy = creature;
	values.appliedByCreature = true;
	values.player = ecs::effects::EffectPlayer(true, registry.Get<const Creature>(creature).owner, std::nullopt);
	values.Scale(creature_object_actions::KickCrushShare(ecs::object::GetHeight(creature), ecs::object::GetHeight(target)));
	static_cast<void>(ecs::effects::ApplyEffect(target, values));
}

/// The foot meets what the creature kicks: a standing tree not already flying creaks where the foot meets it; then a
/// home is kicked (KickHome); a thing that can fly, a standing tree too, flies off ahead of the creature, already
/// flying or set flying from where it stands, thrown by the creature for its player (none for a creature of no
/// player's). A home, a villager or a fence sounds the kick on its surface first; the kick of any other thing makes no
/// sound: the game's sound for it is keyed so that it rarely matches anything
/// (docs/wiki/project/progress/vanilla_bugs/creature.md). A creature that can't fly is crushed (KickCrush)
void KickAway(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points, entt::entity target)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(target))
	{
		return;
	}
	const auto surface = registry.AllOf<Abode>(target)              ? std::optional(k_KickedHomeSurface)
	                     : registry.AllOf<Villager>(target)         ? std::optional(k_KickedVillagerSurface)
	                     : ecs::physics::from_hand::IsFence(target) ? std::optional(k_KickedFenceSurface)
	                                                                : std::nullopt;
	if (surface.has_value() && Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().PlayAnimEffect({0, 0, *surface, 0, k_KickSoundEvent}, target,
		                                                KickPoint(creature, transform, points), false);
	}
	else if (registry.AllOf<Tree>(target) && !registry.AllOf<InPhysics>(target))
	{
		audio::tags::Create(KickPoint(creature, transform, points),
		                    audio::tags::RandomSample(k_KickedTreeCreak, k_TreeCreakCount), false, 3, 0, false, true,
		                    audio::SfxBank::InGame, 0);
	}
	if (registry.AllOf<Abode>(target))
	{
		KickHome(creature, transform, points, target);
		return;
	}
	if (!Locator::dynamicsSystem::has_value() || ecs::object_flags::IsImmovable(target) ||
	    !Locator::dynamicsSystem::value().CanBecomeAPhysicsObject(target))
	{
		if (registry.AllOf<Creature>(target))
		{
			KickCrush(creature, target);
		}
		return;
	}
	auto& dynamics = Locator::dynamicsSystem::value();
	// It kicks the way it faces, leaning with the land under it
	const auto way = KickWayOf(creature, transform);
	const auto velocity = creature_object_actions::KickVelocity(way.forward, way.lean, way.size);
	if (auto* entry = dynamics.Find(target); entry != nullptr)
	{
		entry->body.velocity = velocity;
		return;
	}
	// the creature's throw for its player, from the start
	const auto player = creature_object_actions::ThrowerPlayerOf(registry.Get<const Creature>(creature).owner);
	auto* entry = dynamics.AddObject(target, velocity, DynamicsSystemInterface::BodySpin {}, creature, false, player);
	if (entry == nullptr)
	{
		return;
	}
	entry->body.SettleOnLand(ecs::GameGround(), false, true);
	dynamics.SyncTurnStart(*entry);
	dynamics.RaiseClearOfWhatIsUnder(*entry);
}

/// The blend of the four reaching animations for a target now, mirrored when it lies to the creature's left
struct Reach
{
	bool mirrored;
	creature_reach::Blend blend;
};
Reach ReachFor(const creature_reach::Points& points, const glm::vec3& local)
{
	const bool mirrored = creature_reach::ReachesMirrored(points, local);
	return {.mirrored = mirrored, .blend = creature_reach::Solve(mirrored ? creature_reach::Mirrored(points) : points, local)};
}

void SetSlots(CreatureObjectAction& action, std::span<const size_t> animations, std::span<const float> weights)
{
	action.animationCount = static_cast<uint8_t>(std::min(animations.size(), action.animations.size()));
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.animations.at(i) = animations[i];
		action.weights.at(i) = weights[i];
	}
}

/// The creature counts a thing as the last it let go of (GET_OBJECT_DROPPED)
void RememberLetGo(entt::entity creature, entt::entity object)
{
	Locator::entitiesRegistry::value().AssignOrReplaceState<CreatureDroppedObject>(creature,
	                                                                               CreatureDroppedObject {.object = object});
}

/// The body stops whatever else it plays and walking, and the animations start from their beginning
void BeginPlaying(entt::entity creature, CreatureObjectAction& action, CreatureAnimation& animation)
{
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
	animation.body = {};
	action.phase = Phase::Playing;
	// Setting about putting down or tossing away what it holds, or throwing it at a thing, it counts that as the last
	// thing it let go of from now
	const auto* held = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureHeldObject>(creature);
	if (held != nullptr &&
	    (action.kind == Kind::PutDown || action.kind == Kind::Discard || (action.kind == Kind::Throw && action.atThing)))
	{
		RememberLetGo(creature, held->object);
	}
	action.timeMs = 0.0f;
	action.durationMs = 0.0f;
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		action.durationMs = std::max(action.durationMs, DurationOf(creature, action.animations.at(i)));
	}
	// The two kisses are played together through the high kiss, and end with it
	if (action.kind == Kind::Kiss)
	{
		action.durationMs = DurationOf(creature, creature_kiss::k_KissHigh);
	}
	// Whatever it does at a moment happens by the end at the latest
	action.eventMs = std::min(action.eventMs, std::max(action.durationMs - 1.0f, 0.0f));
}

/// The body is busy walking or turning, or playing an action of its own: the hands start nothing until it is free
bool BodyIsBusy(entt::entity creature, const CreatureAnimation& animation)
{
	return (Locator::creatureLocomotionSystem::has_value() && Locator::creatureLocomotionSystem::value().IsMoving(creature)) ||
	       creature_layers::IsPlaying(animation.body);
}

/// The hands still play the animations of an action, up to their end
bool HandsArePlaying(const ecs::Registry& registry, entt::entity creature)
{
	const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	return action != nullptr && action->phase == Phase::Playing &&
	       (action->status == Status::Running || action->status == Status::Contact);
}

/// The actions on what it holds, or on the thing in front of it, that start without walking or turning to anything
bool StartsAtOnce(Kind kind)
{
	return kind != Kind::PickUp && kind != Kind::Destroy && kind != Kind::Throw && kind != Kind::Point;
}

void Fail(CreatureObjectAction& action, std::string reason,
          creature_object_actions::Failure kind = creature_object_actions::Failure::Other)
{
	action.status = Status::Failed;
	action.failure = std::move(reason);
	action.failureKind = kind;
}

/// The flat and high throws blended by how high the target is against where the hand lets go in each
std::array<float, 2> ThrowWeights(entt::entity creature, const Transform& transform, const CreatureRig::ActionPoints& points,
                                  const glm::vec3& target)
{
	const auto slope = [](const glm::vec3& local) { return local.y / std::max(-local.z, k_Tiny); };
	const auto flat = HandAt(creature, transform, points, creature_throw::k_HurlFlat, points.throwMs);
	const auto high = HandAt(creature, transform, points, creature_throw::k_HurlHigh, points.throwMs);
	if (!flat.has_value() || !high.has_value())
	{
		return {1.0f, 0.0f};
	}
	const auto weight = creature_throw::HighThrowWeight(slope(ToLocal(transform, target)), slope(*flat), slope(*high));
	return {1.0f - weight, weight};
}

/// Where a point is for the pointing animations, from the creature's eye: its standing height (its size as it is drawn)
/// above where it stands, the way it faces its body's
creature_object_actions::PointAim PointAimOf(const ecs::Registry& registry, entt::entity creature,
                                             const CreatureObjectAction& action, const Transform& transform)
{
	const float standing = creature_object_actions::k_PointEyeHeightOfSizeOne *
	                       creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, creature));
	const glm::vec3 eye {transform.position.x, transform.position.y + standing, transform.position.z};
	return creature_object_actions::AimOfPoint(action.point - eye,
	                                           ecs::creature_pose::BodyYaw(registry, creature, transform.rotation));
}

/// The four pointing animations blended for where the point is now, played mirrored as the side chosen as the pointing
/// started (ChoosePointingSide)
void SetPointing(const ecs::Registry& registry, entt::entity creature, CreatureObjectAction& action, const Transform& transform)
{
	const auto weights =
	    creature_object_actions::PointWeights(PointAimOf(registry, creature, action, transform), action.mirrored);
	SetSlots(action, creature_object_actions::k_PointAnimations, weights);
}

/// Where the creature's head is as the turn posed its body (its species' head bone); where it stands without a pose
glm::vec3 PointingHeadOf(const ecs::Registry& registry, entt::entity creature, const Creature& body, const Transform& transform)
{
	const auto* points = PointsOf(body);
	const auto* pose = registry.TryGet<const CreatureTurnPose>(creature);
	if (points == nullptr || pose == nullptr)
	{
		return transform.position;
	}
	const auto bones = creature_turn_pose::Safe(*pose);
	return points->head < bones.size() ? glm::vec3(bones[points->head][3]) : transform.position;
}

/// The side it points with, chosen once as the pointing starts and kept while the point moves. Holding something, it
/// points with the other hand (mirrored when it holds it in its right). Else, having just turned to face the point,
/// mirrored when the way it faces less the way to the point from its eye is above 0; pointing without a turn, mirrored
/// when the way from its head to the point less the way it faces is above 0
void ChoosePointingSide(const ecs::Registry& registry, entt::entity creature, CreatureObjectAction& action,
                        const Creature& body, const Transform& transform, const CreatureHeldObject* held, bool turned)
{
	if (held != nullptr)
	{
		action.mirrored = !held->mirrored;
		return;
	}
	if (turned)
	{
		action.mirrored = PointAimOf(registry, creature, action, transform).across > 0.0f;
		return;
	}
	action.mirrored =
	    creature_object_actions::PointsMirroredFacing(action.point - PointingHeadOf(registry, creature, body, transform),
	                                                  ecs::creature_pose::BodyYaw(registry, creature, transform.rotation));
}

/// A vector along the ground turned by an angle
glm::vec2 Turned(glm::vec2 v, float angle)
{
	const auto c = std::cos(angle);
	const auto s = std::sin(angle);
	return {(v.x * c) - (v.y * s), (v.x * s) + (v.y * c)};
}

/// The point to turn to face so that a target lies in the direction of the middle of the reach on the side it reaches
/// with: the other side's when mirrored
glm::vec2 FacingToReach(const Transform& transform, const creature_reach::Points& reach, const glm::vec3& target, bool mirrored)
{
	const auto from = glm::xz(transform.position);
	const auto towards = glm::xz(target) - from;
	const auto centre = creature_reach::Centre(reach);
	const glm::vec2 middle {mirrored ? -centre.x : centre.x, centre.z};
	if (glm::length(middle) < k_Tiny)
	{
		return from + towards;
	}
	// How far round from ahead the middle of the reach is, and so which way to face for it to lie on the target
	const glm::vec2 ahead {0.0f, -1.0f};
	const auto offset = std::atan2((ahead.x * middle.y) - (ahead.y * middle.x), glm::dot(ahead, middle));
	return from + Turned(towards, -offset);
}

/// Where the hand (or the foot, for a blow) gets to in the reaching animations, and how far it can reach, measured once
/// for the action: whether they are known
bool MeasureActionReach(entt::entity creature, CreatureObjectAction& action, const CreatureRig::ActionPoints& points,
                        const Transform& transform)
{
	if (action.reach.has_value())
	{
		return true;
	}
	const bool pickingUp = action.kind == Kind::PickUp;
	action.eventMs = pickingUp ? points.pickUpMs : points.destroyMs;
	// A blow is measured at the right foot, a pick-up at the right hand
	action.reach = MeasureReach(creature, transform, pickingUp ? points.rightHand : points.rightFoot,
	                            pickingUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations,
	                            action.eventMs, pickingUp);
	if (!action.reach.has_value())
	{
		return false;
	}
	// As far as the stretched reach goes, height and all
	action.maxReach = creature_reach::MaxReach3D(*action.reach, 1.0f);
	return true;
}

/// Whether a thing in reach is too high for the creature to pick up, by the heights of the two above the ground and the
/// game's distance in metres between their map positions
bool TooHighToPickUp(const Transform& creature, const Transform& thing)
{
	const auto self = map_coords::FromWorld(creature.position);
	const auto other = map_coords::FromWorld(thing.position);
	return creature_object_actions::TooHighToPickUp(std::abs(other.altitude - self.altitude),
	                                                game_math::GetDistanceInMetres(other, self));
}

/// A walk up to a thing goes on as it was while the thing is less than a tenth from where it was aimed
constexpr float k_SameWalkPoint = 0.1f;

/// The walk up to a thing, while it walks: aimed afresh at where the thing is, to within its furthest reach, unless the
/// thing is already nearer than its average reach. A walk to where it already goes goes on as it was
void ReaimWalk(entt::entity creature, CreatureObjectAction& action, const Creature& body, const Transform& transform)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* points = PointsOf(body);
	const auto* at = action.target.has_value() && ecs::IsAvailable(*action.target)
	                     ? registry.TryGet<const Transform>(*action.target)
	                     : nullptr;
	if (points == nullptr || at == nullptr || !MeasureActionReach(creature, action, *points, transform))
	{
		return;
	}
	const auto distance = glm::distance(glm::xz(at->position), glm::xz(transform.position));
	if (distance < creature_reach::AverageReach(*action.reach, 1.0f))
	{
		// In reach as it walks, a thing too high stops the pick-up
		if (action.kind == Kind::PickUp && TooHighToPickUp(transform, *at))
		{
			Fail(action, "it is too high");
		}
		return;
	}
	const auto point = glm::xz(at->position);
	if (action.walkPoint.has_value())
	{
		const auto moved = point - *action.walkPoint;
		if (glm::dot(moved, moved) < k_SameWalkPoint * k_SameWalkPoint)
		{
			return;
		}
	}
	const auto aimedAt = std::exchange(action.walkPoint, point);
	using MoveResult = CreatureLocomotionSystemInterface::MoveResult;
	const auto result =
	    Locator::creatureLocomotionSystem::value().MoveTo(creature, point, CreatureLocomotionSystemInterface::Pace::Walk, 0.0f,
	                                                      creature_object_actions::WalkUpToWithin(action.maxReach));
	switch (creature_cast_moves::WalkStartOf(result == MoveResult::Started, result != MoveResult::InvalidDestination))
	{
	case creature_cast_moves::WalkStart::Walking:
		break;
	case creature_cast_moves::WalkStart::TryAgain:
		// The body can't walk now: the walk goes on where it was aimed, and it aims again on the next turn
		action.walkPoint = aimedAt;
		break;
	case creature_cast_moves::WalkStart::NoWay:
		Fail(action, "navigation failed", creature_object_actions::Failure::NoWay);
		break;
	}
}

/// One game turn of walking or turning up to what the creature acts on
void Approach(entt::entity creature, CreatureObjectAction& action, const Creature& body, const Transform& transform,
              CreatureAnimation& animation, const CreatureHeldObject* held)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	// On every turn of a pick-up, the walk to it included, a thing still there that it can no longer pick up stops the
	// pick-up and its walk: it found no way to the thing
	if (action.kind == Kind::PickUp && action.target.has_value() && ecs::IsAvailable(*action.target) &&
	    !CreatureObjectActionSystem::PickUpTest(creature, *action.target, false))
	{
		locomotion.BrakeToStop(creature);
		Fail(action, "it can't be picked up", creature_object_actions::Failure::NoWay);
		return;
	}
	// So on every turn of a stomp, the walk to it included, a thing still there that is no longer in the map stops the
	// stomp and its walk: it found no way to the thing
	if (action.kind == Kind::Destroy && action.target.has_value() && ecs::IsAvailable(*action.target) &&
	    !ecs::map_cells::IsObjectInMap(*action.target))
	{
		locomotion.BrakeToStop(creature);
		Fail(action, "it is not in the map", creature_object_actions::Failure::NoWay);
		return;
	}
	// A walk ends as the creatures move, after this: the hands start the turn after (ProcessApproach). Its own turn to
	// face the thing is part of acting on it, and the hands go straight on from it (ProcessTurn)
	if (locomotion.IsMoving(creature) || creature_layers::IsPlaying(animation.body))
	{
		// Walking up to a thing, each turn it aims its walk afresh at where the thing is, unless it is already nearer
		// than its average reach
		if (!action.turningToIt && (action.kind == Kind::PickUp || action.kind == Kind::Destroy) &&
		    !creature_layers::IsPlaying(animation.body))
		{
			ReaimWalk(creature, action, body, transform);
		}
		return;
	}
	const bool turned = action.turningToIt;
	action.turningToIt = false;
	const auto* points = PointsOf(body);
	if (points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
		return;
	}

	if (action.kind == Kind::Throw || action.kind == Kind::Point)
	{
		// Pointing, it turns first unless the point is near enough ahead as seen from its head, and once turned it points
		const bool turns = action.kind == Kind::Throw
		                       ? BearingTo(transform, action.point) > creature_object_actions::k_ThrowTurnRadians
		                       : !turned && creature_object_actions::TurnsToPoint(creature_object_actions::PointTurnAngle(
		                                        action.point - PointingHeadOf(registry, creature, body, transform),
		                                        ecs::creature_pose::BodyYaw(registry, creature, transform.rotation)));
		if (turns && action.attempts < k_TurnAttempts)
		{
			++action.attempts;
			action.turningToIt = true;
			locomotion.TurnToFace(creature, glm::xz(action.point));
			return;
		}
		if (action.kind == Kind::Throw)
		{
			const auto weights = ThrowWeights(creature, transform, *points, action.point);
			const std::array<size_t, 2> animations {creature_throw::k_HurlFlat, creature_throw::k_HurlHigh};
			SetSlots(action, animations, weights);
			action.mirrored = held != nullptr && held->mirrored;
			action.eventMs = points->throwMs;
		}
		else
		{
			ChoosePointingSide(registry, creature, action, body, transform, held, turned);
			SetPointing(registry, creature, action, transform);
			action.holdMs = action.pointSeconds.value_or(creature_object_actions::k_PointSeconds) * 1000.0f;
			action.pointMs = points->pointMs;
		}
		BeginPlaying(creature, action, animation);
		return;
	}

	// Walking up to something and reaching for it
	const auto* at = action.target.has_value() && ecs::IsAvailable(*action.target)
	                     ? registry.TryGet<const Transform>(*action.target)
	                     : nullptr;
	if (at == nullptr)
	{
		Fail(action, "it has gone");
		return;
	}
	if (!MeasureActionReach(creature, action, *points, transform))
	{
		// Not posed yet: it tries again next turn
		return;
	}
	const auto local = ToLocal(transform, at->position);
	const auto& animations =
	    action.kind == Kind::PickUp ? creature_reach::k_PickUpAnimations : creature_reach::k_DestroyAnimations;
	if (turned)
	{
		// Turned to it as it set about the action, it reaches with the side it chose then, or the action fails
		const auto blend =
		    creature_reach::Solve(action.mirrored ? creature_reach::Mirrored(*action.reach) : *action.reach, local);
		if (!blend.inRange)
		{
			Fail(action, "it couldn't get in reach");
			return;
		}
		SetSlots(action, animations, blend.weights);
		BeginPlaying(creature, action, animation);
		return;
	}
	const auto reach = ReachFor(*action.reach, local);
	const auto distance = glm::length(glm::vec2(local.x, local.z));
	// It goes on until the thing is in reach or the action fails, with no limit to its walks
	const auto step = creature_object_actions::NextApproachStep(reach.blend.inRange, distance < action.maxReach);
	// In reach, a thing too high stops the pick-up before it turns or reaches
	if (step != creature_object_actions::ApproachStep::Walk && action.kind == Kind::PickUp && TooHighToPickUp(transform, *at))
	{
		Fail(action, "it is too high");
		return;
	}
	switch (step)
	{
	case creature_object_actions::ApproachStep::Begin:
	{
		SetSlots(action, animations, reach.blend.weights);
		action.mirrored = reach.mirrored;
		BeginPlaying(creature, action, animation);
		return;
	}
	case creature_object_actions::ApproachStep::TurnThenBegin:
		// It turns on the spot so that the middle of its reach on the side it reaches with faces the thing, and the
		// action goes straight on from the turn
		action.mirrored = reach.mirrored;
		action.turningToIt = true;
		static_cast<void>(
		    locomotion.TurnToFace(creature, FacingToReach(transform, *action.reach, at->position, reach.mirrored)));
		return;
	case creature_object_actions::ApproachStep::Walk:
		break;
	}
	++action.attempts;
	// It walks until it is within its furthest reach of the thing, with no nearest distance to stop at
	action.walkPoint = glm::xz(at->position);
	using MoveResult = CreatureLocomotionSystemInterface::MoveResult;
	const auto result = locomotion.MoveTo(creature, *action.walkPoint, CreatureLocomotionSystemInterface::Pace::Walk, 0.0f,
	                                      creature_object_actions::WalkUpToWithin(action.maxReach));
	// A body that can't walk now asks again on the next turn; only a thing where it can't stand fails the action
	if (creature_cast_moves::WalkStartOf(result == MoveResult::Started, result != MoveResult::InvalidDestination) ==
	    creature_cast_moves::WalkStart::NoWay)
	{
		Fail(action, "navigation failed", creature_object_actions::Failure::NoWay);
	}
}

/// What is held by a creature that is gone, or no longer holds it, falls where it is, and a hand whose thing is gone
/// is empty
void LetGoOfWhatIsLoose(ecs::Registry& registry)
{
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> loose;
	lookup.Each<const HeldByCreature>([&](entt::entity entity, const HeldByCreature& by) {
		const auto* held = lookup.Valid(by.creature) ? lookup.TryGet<const CreatureHeldObject>(by.creature) : nullptr;
		if (held == nullptr || held->object != entity)
		{
			loose.push_back(entity);
		}
	});
	for (const auto entity : loose)
	{
		const auto* at = lookup.TryGet<const Transform>(entity);
		CreatureObjectActionSystem::ReleaseHeld(entity, at != nullptr ? at->position : glm::vec3(0.0f), glm::vec3(0.0f));
	}
	std::vector<entt::entity> emptyHanded;
	lookup.Each<const CreatureHeldObject>([&](entt::entity entity, const CreatureHeldObject& held) {
		if (!ecs::IsAvailable(held.object))
		{
			emptyHanded.push_back(entity);
		}
	});
	for (const auto entity : emptyHanded)
	{
		registry.RemoveState<CreatureHeldObject>(entity);
	}
}

/// A living thing: a villager, an animal or a creature
bool IsLivingThing(const ecs::Registry& registry, entt::entity object)
{
	return registry.AnyOf<Villager, Animal, Creature>(object);
}

/// Whether the creature's agenda is at its last step
bool OnLastStep(const ecs::Registry& registry, entt::entity creature)
{
	const auto* mind = registry.TryGet<const CreatureMindState>(creature);
	return mind != nullptr && mind->idle.step + 1 == mind->idle.agenda.size();
}

/// The crush a knock-down gives a living thing it knocked down: a single crush, applied by the creature
void CrushKnockedDown(const ecs::Registry& registry, entt::entity creature, entt::entity target)
{
	auto values = ecs::effects::EffectValues {};
	values.numbers.at(static_cast<size_t>(ecs::effects::EffectValues::Number::Crush)) =
	    creature_object_actions::k_KnockDownCrush;
	values.radius = creature_object_actions::k_KnockDownCrushRadius;
	values.appliedBy = creature;
	values.appliedByCreature = true;
	values.player = ecs::effects::EffectPlayer(true, registry.Get<const Creature>(creature).owner, std::nullopt);
	static_cast<void>(ecs::effects::ApplyEffect(target, values));
}

/// The creatures acting on something, in the registry's order
std::vector<entt::entity> Acting(const ecs::Registry& lookup)
{
	std::vector<entt::entity> acting;
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureObjectAction>(
	    [&acting](entt::entity entity, const auto&...) { acting.push_back(entity); });
	return acting;
}

/// Whether a stomp's test of its class may ever take the thing
bool HasStompTest(entt::entity object)
{
	const auto kind = CreatureObjectActionSystem::KindFactsOf(object);
	return creature_object_actions::StompRuleOf(kind.thing, kind.stompGround) != creature_object_actions::StompRule::Never;
}
} // namespace

bool CreatureObjectActionSystem::Start(entt::entity creature, CreatureObjectAction action)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creature))
	{
		return false;
	}
	// While the hands still play an action or catch, what is asked waits for that to end, as every start of the
	// original refuses while the body is busy: what the playing one does at its moment still happens
	if (HandsArePlaying(registry, creature) || IsCatching(creature))
	{
		registry.AssignOrReplaceState<CreatureObjectActionWaiting>(creature,
		                                                           CreatureObjectActionWaiting {.action = std::move(action)});
		return true;
	}
	if (std::as_const(registry).AllOf<CreatureObjectActionWaiting>(creature))
	{
		registry.RemoveState<CreatureObjectActionWaiting>(creature);
	}
	if (std::as_const(registry).AllOf<CreatureCatching>(creature))
	{
		registry.RemoveState<CreatureCatching>(creature);
	}
	const auto& body = registry.Get<const Creature>(creature);
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	action.status = Status::Running;
	action.phase = Phase::Approach;
	action.turningToIt = false;
	switch (creature_object_actions::HandsFor(action.kind))
	{
	case creature_object_actions::Hands::Holding:
		if (held == nullptr)
		{
			Fail(action, "it has nothing in its hand");
		}
		break;
	case creature_object_actions::Hands::Empty:
		if (held != nullptr)
		{
			Fail(action, "its hands are full");
		}
		break;
	case creature_object_actions::Hands::Either:
		break;
	}
	const auto* points = PointsOf(body);
	if (action.status == Status::Running && points == nullptr)
	{
		Fail(action, "its species has nothing to act with");
	}
	if (action.status == Status::Running && action.target.has_value())
	{
		if (!registry.Valid(*action.target) || !registry.AllOf<Transform>(*action.target))
		{
			Fail(action, "there is nothing there");
		}
		else if (action.kind == Kind::PickUp &&
		         (!CanPickUp(creature, *action.target) || registry.AllOf<HeldByCreature>(*action.target)))
		{
			// A thing that is no longer available fails it with no more said; any other found no way to it
			Fail(action, "it can't be picked up",
			     ecs::IsAvailable(*action.target) ? creature_object_actions::Failure::NoWay
			                                      : creature_object_actions::Failure::Other);
		}
		else if (action.kind == Kind::Destroy && ecs::IsAvailable(*action.target) &&
		         !ecs::map_cells::IsObjectInMap(*action.target))
		{
			// A stomp needs the thing in the map: off it, the stomp found no way to it
			Fail(action, "it is not in the map", creature_object_actions::Failure::NoWay);
		}
		else if (action.kind == Kind::Destroy && !CanDestroy(*action.target) && !IsLivingThing(registry, *action.target) &&
		         !HasStompTest(*action.target))
		{
			// What a stomp's test of its class may take is knocked down: a thing with no such test is refused
			Fail(action, "it can't be knocked down");
		}
		else if (action.kind == Kind::Kick &&
		         ((held != nullptr && held->object == *action.target) ||
		          (Locator::handSystem::has_value() && Locator::handSystem::value().GetHeldObject() == action.target)))
		{
			Fail(action, "it is in a hand");
		}
		else if (action.kind == Kind::Kick && DurationOf(creature, creature_object_actions::k_KickAnimation) <= 0.0f)
		{
			Fail(action, "its species can't kick");
		}
	}
	// How far up its kiss it kisses: all the way down kissing the air, else so that its lips meet the other's
	float kissShare = 0.0f;
	if (action.status == Status::Running && action.kind == Kind::Kiss)
	{
		if (DurationOf(creature, creature_kiss::k_KissHigh) <= 0.0f || DurationOf(creature, creature_kiss::k_KissLow) <= 0.0f)
		{
			Fail(action, "its species can't kiss");
		}
		else if (action.target.has_value())
		{
			const auto share = KissShareWith(creature, *action.target);
			if (share.has_value())
			{
				kissShare = *share;
			}
			else
			{
				Fail(action, "the two can't kiss");
			}
		}
	}
	if (action.status == Status::Running && action.kind == Kind::Throw)
	{
		const auto& transform = registry.Get<const Transform>(creature);
		const auto ground = creature_throw::GroundDistance(transform.position, action.point);
		if (!creature_throw::FarEnoughToThrow(ground, body.size))
		{
			Fail(action, "too close to throw at");
		}
		// Timed from where it stands as it sets about the throw, before any turn, by the distance along the ground
		action.flightSeconds = action.givenFlightSeconds.value_or(creature_throw::ThrowFlightTime(ground));
	}

	if (action.status == Status::Running && points != nullptr)
	{
		// Acting on what it holds starts at once, with the hand it holds it in
		const auto single = [&](size_t animation, float eventMs) {
			const std::array<size_t, 1> animations {animation};
			const std::array<float, 1> weights {1.0f};
			SetSlots(action, animations, weights);
			action.mirrored = held != nullptr && held->mirrored;
			action.eventMs = eventMs;
		};
		bool now = true;
		switch (action.kind)
		{
		case Kind::PutDown:
			single(creature_throw::k_PutDown, points->putDownMs);
			break;
		case Kind::Discard:
			single(creature_throw::k_Discard, points->discardMs);
			break;
		case Kind::Lob:
		{
			// The game times the gentle lob by nothing of its own: it lets go as far through as tossing away does
			const auto discard = DurationOf(creature, creature_throw::k_Discard);
			const auto lob = DurationOf(creature, creature_throw::k_GentleLob);
			single(creature_throw::k_GentleLob, discard > 0.0f ? points->discardMs * lob / discard : points->discardMs);
			break;
		}
		case Kind::Eat:
			single(creature_throw::k_Eat, points->eatMs);
			break;
		case Kind::Keep:
			// The animation was chosen as the action was asked for
			single(action.animations.front(), 0.0f);
			action.eventDone = true;
			break;
		case Kind::TakeFromHand:
		{
			// It reaches with the hand it was asked to (holding something, it takes nothing: BeginAtOnce)
			const bool mirrored = action.mirrored;
			single(creature_giving::k_TakeAnimation, points->takeFromHandMs);
			action.mirrored = mirrored;
			break;
		}
		case Kind::Kick:
			// It kicks with its right foot, whichever hand holds what it holds
			single(creature_object_actions::k_KickAnimation, points->kickMs);
			action.mirrored = false;
			break;
		case Kind::Kiss:
		{
			const std::array<size_t, 2> animations {creature_kiss::k_KissHigh, creature_kiss::k_KissLow};
			const std::array<float, 2> weights {kissShare, 1.0f - kissShare};
			SetSlots(action, animations, weights);
			action.mirrored = false;
			action.eventMs = static_cast<float>(KissTimeOf(body));
			break;
		}
		case Kind::PickUp:
		case Kind::Destroy:
		case Kind::Throw:
		case Kind::Point:
			now = false;
			break;
		}
		// While the body is busy it waits, set up, until the body is free (ProcessApproach)
		if (now && !BodyIsBusy(creature, registry.Get<const CreatureAnimation>(creature)))
		{
			BeginAtOnce(creature, action);
		}
	}
	// A take from the hand with something already held is done at once, nothing taken
	const bool started = action.status != Status::Failed;
	registry.AssignOrReplaceState<CreatureObjectAction>(creature, std::move(action));
	return started;
}

bool CreatureObjectActionSystem::TakeFromHand(entt::entity creature, entt::entity object, bool mirrored)
{
	return Start(creature, {.kind = Kind::TakeFromHand, .target = object, .mirrored = mirrored});
}

std::optional<CreatureObjectActionSystemInterface::TakingFromHand>
CreatureObjectActionSystem::GetTakingFromHand(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->kind != Kind::TakeFromHand || action->phase != Phase::Playing ||
	    (action->status != Status::Running && action->status != Status::Contact) || !action->target.has_value())
	{
		return std::nullopt;
	}
	return TakingFromHand {.object = *action->target, .msFromTake = action->timeMs - action->eventMs};
}

void CreatureObjectActionSystem::CallOffTake(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	// Only before its moment: having taken the thing, it plays on as it was
	if (action == nullptr || action->kind != Kind::TakeFromHand || action->phase != Phase::Playing ||
	    action->status != Status::Running)
	{
		return;
	}
	// Its hand goes on reaching, for nothing now
	action->target.reset();
}

bool CreatureObjectActionSystem::IsSetOnTakingFromHand(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature))
	{
		return false;
	}
	if (const auto* next = registry.TryGet<const CreatureObjectActionWaiting>(creature);
	    next != nullptr && next->action.kind == Kind::TakeFromHand)
	{
		return true;
	}
	const auto* action = registry.TryGet<const CreatureObjectAction>(creature);
	return action != nullptr && action->kind == Kind::TakeFromHand &&
	       (action->status == Status::Running || action->status == Status::Contact);
}

std::optional<glm::vec3> CreatureObjectActionSystem::TakePoint(entt::entity creature, glm::vec3 facing) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return std::nullopt;
	}
	const auto* points = PointsOf(registry.Get<const Creature>(creature));
	const auto& transform = registry.Get<const Transform>(creature);
	if (points == nullptr)
	{
		return std::nullopt;
	}
	// The right hand at the moment of taking, with the creature turned to face the point
	const auto hand = HandAt(creature, transform, *points, creature_giving::k_TakeAnimation, points->takeFromHandMs);
	if (!hand.has_value())
	{
		return std::nullopt;
	}
	const glm::vec2 towards {facing.x - transform.position.x, facing.z - transform.position.z};
	const auto heading = glm::length(towards) > 0.0f ? std::atan2(-towards.x, -towards.y) : 0.0f;
	return transform.position + glm::vec3(glm::eulerAngleY(heading) * glm::vec4(*hand, 0.0f));
}

bool CreatureObjectActionSystem::PickUp(entt::entity creature, entt::entity object)
{
	return Start(creature, {.kind = Kind::PickUp, .target = object});
}

bool CreatureObjectActionSystem::PickUpPortion(entt::entity creature, entt::entity source, PotInfo kind)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature) || !registry.Valid(source) ||
	    !registry.AllOf<Transform>(source))
	{
		// as picking up a thing that is not there
		return Start(creature, {.kind = Kind::PickUp, .target = source});
	}
	// A portion of its kind, at most of its drawn size's worth, made towards the source at nearly the creature's reach,
	// and as big as its drawn size makes it
	const float drawnSize = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(std::as_const(registry), creature));
	const auto& from = registry.Get<const Transform>(creature).position;
	const auto& to = registry.Get<const Transform>(source).position;
	const auto at = creature_eating::PortionPoint(glm::xz(from), glm::xz(to), PickUpReach(creature).value_or(0.0f));
	const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(at) : from.y;
	// It holds as much of its own kind's resource as the source has of it
	const auto row = static_cast<size_t>(kind);
	const auto* pots = Locator::infoConstants::has_value() ? &Locator::infoConstants::value().pot : nullptr;
	const auto resource = creature_eating::PortionResource(
	    pots != nullptr && row < pots->size() ? std::optional(pots->at(row).resourceType) : std::nullopt);
	const auto amount = creature_eating::PortionAmount(drawnSize, ecs::object_resources::GetResource(source, resource));
	const auto portion =
	    ecs::archetypes::PotArchetype::Create(glm::vec3(at.x, ground, at.y), 0.0f, kind, static_cast<int32_t>(amount), true);
	if (portion == entt::null)
	{
		return Start(creature, {.kind = Kind::PickUp, .target = entt::null});
	}
	// the portion is the town's nearest to it, however far
	registry.Get<Pot>(portion).town =
	    ecs::map_cells::GetNearestTown(map_coords::FromMetres(at), std::numeric_limits<float>::max());
	registry.Get<Transform>(portion).scale = glm::vec3(creature_eating::PortionScale(drawnSize));
	registry.SetDirty();
	return Start(creature, {.kind = Kind::PickUp, .target = portion, .source = source});
}

bool CreatureObjectActionSystem::PickUpFishFromSea(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return Start(creature, {.kind = Kind::PickUp, .target = entt::null});
	}
	// The handful is made where the creature stands, of wheat, a thousand for each unit of the size its body is drawn at,
	// and half as big again as that size
	constexpr float k_ScalePerSize = 1.5f;
	const float drawnSize = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(std::as_const(registry), creature));
	const auto& at = registry.Get<const Transform>(creature).position;
	const float ground = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(at)) : at.y;
	const auto amount = static_cast<int32_t>(map_coords::FtoL(creature_eating::k_PortionAmountPerSize * drawnSize));
	const auto food =
	    ecs::archetypes::PotArchetype::Create(glm::vec3(at.x, ground, at.z), 0.0f, PotInfo::WheatInHand, amount, true);
	if (food == entt::null)
	{
		return Start(creature, {.kind = Kind::PickUp, .target = entt::null});
	}
	// the handful is the town's nearest to it, however far
	registry.Get<Pot>(food).town =
	    ecs::map_cells::GetNearestTown(map_coords::FromMetres(glm::xz(at)), std::numeric_limits<float>::max());
	registry.Get<Transform>(food).scale = glm::vec3(drawnSize * k_ScalePerSize);
	registry.SetDirty();
	return Start(creature, {.kind = Kind::PickUp, .target = food});
}

bool CreatureObjectActionSystem::PutDown(entt::entity creature)
{
	return Start(creature, {.kind = Kind::PutDown});
}

bool CreatureObjectActionSystem::Discard(entt::entity creature)
{
	return Start(creature, {.kind = Kind::Discard});
}

bool CreatureObjectActionSystem::Lob(entt::entity creature)
{
	return Start(creature, {.kind = Kind::Lob});
}

bool CreatureObjectActionSystem::EatHeld(entt::entity creature)
{
	// Whatever it holds is eaten, food or not: what is not food it finds out about when the meal ends
	return Start(creature, {.kind = Kind::Eat, .target = GetHeld(creature)});
}

bool CreatureObjectActionSystem::Keep(entt::entity creature, size_t animation)
{
	CreatureObjectAction action {.kind = Kind::Keep};
	action.animations.front() = animation;
	return Start(creature, std::move(action));
}

bool CreatureObjectActionSystem::Throw(entt::entity creature, const glm::vec3& target)
{
	return Start(creature, {.kind = Kind::Throw, .point = target});
}

bool CreatureObjectActionSystem::ThrowTaking(entt::entity creature, const glm::vec3& target, float flightSeconds)
{
	CreatureObjectAction action {.kind = Kind::Throw, .point = target};
	action.givenFlightSeconds = flightSeconds;
	return Start(creature, std::move(action));
}

bool CreatureObjectActionSystem::ThrowAtThing(entt::entity creature, const glm::vec3& target, float flightSeconds)
{
	CreatureObjectAction action {.kind = Kind::Throw, .point = target};
	action.givenFlightSeconds = flightSeconds;
	action.atThing = true;
	return Start(creature, std::move(action));
}

bool CreatureObjectActionSystem::Destroy(entt::entity creature, entt::entity target)
{
	return Start(creature, {.kind = Kind::Destroy, .target = target});
}

bool CreatureObjectActionSystem::Kick(entt::entity creature, entt::entity target)
{
	return Start(creature, {.kind = Kind::Kick, .target = target});
}

bool CreatureObjectActionSystem::Kiss(entt::entity creature, std::optional<entt::entity> other)
{
	return Start(creature, {.kind = Kind::Kiss, .target = other});
}

bool CreatureObjectActionSystem::PointAt(entt::entity creature, const glm::vec3& point)
{
	return Start(creature, {.kind = Kind::Point, .point = point});
}

bool CreatureObjectActionSystem::PointAtFor(entt::entity creature, const glm::vec3& point, float seconds,
                                            std::optional<entt::entity> thing)
{
	return Start(creature, {.kind = Kind::Point, .target = thing, .point = point, .pointSeconds = seconds});
}

bool CreatureObjectActionSystem::PointUntilStopped(entt::entity creature, const glm::vec3& point)
{
	// The hold never runs out by itself
	return Start(creature, {.kind = Kind::Point, .point = point, .pointSeconds = std::numeric_limits<float>::infinity()});
}

void CreatureObjectActionSystem::UpdatePointPoint(entt::entity creature, const glm::vec3& point)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->kind != Kind::Point || action->status != Status::Running)
	{
		return;
	}
	action->point = point;
	// An arm that points already aims at it at once
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	if (action->phase == Phase::Playing && transform != nullptr && body != nullptr)
	{
		SetPointing(std::as_const(registry), creature, *action, *transform);
	}
}

bool CreatureObjectActionSystem::IsPointing(entt::entity creature) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	return action != nullptr && action->kind == Kind::Point && action->phase == Phase::Playing &&
	       action->status == Status::Running && action->holdMs > 0.0f;
}

std::optional<glm::vec3> CreatureObjectActionSystem::GetPointingHand(entt::entity creature) const
{
	if (!IsPointing(creature))
	{
		return std::nullopt;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& action = registry.Get<const CreatureObjectAction>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* points = body != nullptr ? PointsOf(*body) : nullptr;
	if (animation == nullptr || transform == nullptr || points == nullptr || animation->boneMatrices.empty())
	{
		return std::nullopt;
	}
	const auto hand = HandBone(*points, *animation, action.mirrored);
	if (hand >= animation->boneMatrices.size())
	{
		return std::nullopt;
	}
	// The turn's pose when it has one for every bone, else the last drawn
	const auto* pose = registry.TryGet<const CreatureTurnPose>(creature);
	const bool turnPosed = pose != nullptr && pose->current.size() == animation->boneMatrices.size();
	return glm::vec3(turnPosed ? creature::PosedBone(hand, pose->current, glm::mat4(1.0f))[3]
	                           : creature::PosedBone(hand, animation->boneMatrices, PlacementOf(*transform))[3]);
}

void CreatureObjectActionSystem::StopPointing(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action != nullptr && action->kind == Kind::Point && action->phase == Phase::Playing &&
	    action->status == Status::Running && action->holdMs > 0.0f)
	{
		action->holdMs = 0.0f;
		action->status = Status::Done;
	}
}

void CreatureObjectActionSystem::Cancel(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(creature) && std::as_const(registry).AllOf<CreatureObjectActionWaiting>(creature))
	{
		registry.RemoveState<CreatureObjectActionWaiting>(creature);
	}
	if (registry.Valid(creature) && std::as_const(registry).AllOf<CreatureObjectAction>(creature))
	{
		// A take whose giving was called off plays on to its moment, where its hand closes on nothing: nothing else stops it
		if (IsCalledOffTake(registry.Get<const CreatureObjectAction>(creature)))
		{
			return;
		}
		registry.RemoveState<CreatureObjectAction>(creature);
		if (auto* animation = registry.TryGet<CreatureAnimation>(creature))
		{
			animation->slots.clear();
		}
	}
}

void CreatureObjectActionSystem::StopForNewAction(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	// Its animations on a thing play on; pointing ends there and then, and a walk or turn up to a thing stops
	if (action == nullptr || action->phase != Phase::Playing || action->kind == Kind::Point ||
	    (action->status != Status::Running && action->status != Status::Contact))
	{
		if (action != nullptr && action->kind == Kind::Point && action->phase == Phase::Playing)
		{
			StopPointing(creature);
		}
		Cancel(creature);
		return;
	}
	if (std::as_const(registry).AllOf<CreatureObjectActionWaiting>(creature))
	{
		registry.RemoveState<CreatureObjectActionWaiting>(creature);
	}
	action->playsOn = true;
	// A readout at run time (the game channel at trace level), so a player's log shows the action playing on
	if (const auto logger = spdlog::get("game"))
	{
		logger->trace("Creature {} stopped: its {} plays on to its end", entt::to_integral(creature),
		              creature_object_actions::Name(action->kind));
	}
}

void CreatureObjectActionSystem::Hurry(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.Valid(creature) ? registry.TryGet<CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->phase != Phase::Playing)
	{
		return;
	}
	action->hurried = creature_object_actions::HurriedByForcedFinish(action->kind);
	if (const auto logger = spdlog::get("game"); logger && action->hurried)
	{
		logger->trace("Creature {} forced to finish: its {} plays at twice the pace", entt::to_integral(creature),
		              creature_object_actions::Name(action->kind));
	}
}

void CreatureObjectActionSystem::Drop(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto held = GetHeld(creature);
	if (!held.has_value())
	{
		return;
	}
	const auto* at = registry.TryGet<const Transform>(*held);
	Release(creature, at != nullptr ? at->position : registry.Get<const Transform>(creature).position, glm::vec3(0.0f));
}

void CreatureObjectActionSystem::Release(entt::entity creature, const glm::vec3& position, const glm::vec3& velocity,
                                         bool atTarget, bool turnedByHand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	if (held == nullptr)
	{
		return;
	}
	const auto object = held->object;
	// at the point it is let go of, then turned by the hand
	if (auto* at = turnedByHand && registry.Valid(object) ? registry.TryGet<Transform>(object) : nullptr; at != nullptr)
	{
		at->position = position;
		TurnAsTheHandLetsGo(registry, creature, *held);
	}
	// Let go of, it has its blob shadow again if holding it took it
	if (held->shadowTaken && registry.Valid(object))
	{
		SetHumanShadowed(registry, object, true);
	}
	registry.RemoveState<CreatureHeldObject>(creature);
	ReleaseHeld(object, position, velocity, creature, atTarget);
}

void CreatureObjectActionSystem::ReleaseHeld(entt::entity object, const glm::vec3& position, const glm::vec3& velocity,
                                             entt::entity creature, bool atTarget)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	const auto& lookup = std::as_const(registry);
	if (lookup.AllOf<HeldByCreature>(object))
	{
		registry.RemoveState<HeldByCreature>(object);
	}
	if (lookup.AllOf<PhysicsDrawPose>(object))
	{
		registry.Remove<PhysicsDrawPose>(object);
	}
	auto* transform = registry.TryGet<Transform>(object);
	if (transform != nullptr)
	{
		transform->position = position;
	}
	ecs::NotifyTeleported(object);
	// It flies from where it was let go of; the physics puts it back on the map as it comes to rest
	bool placed = false;
	if (transform != nullptr)
	{
		const auto release = ecs::creature_physics::ReleaseOf(atTarget, creature);
		// what a creature sets flying is its player's throw (none for a creature of no player's); a thing let go of by
		// nobody is credited to the local player
		const auto* body = lookup.Valid(creature) ? lookup.TryGet<const Creature>(creature) : nullptr;
		const auto player = body != nullptr ? creature_object_actions::ThrowerPlayerOf(body->owner)
		                                    : std::optional(openblack::creature::LocalPlayer());
		switch (release)
		{
		case ecs::creature_physics::Release::AtTarget:
			placed = Locator::dynamicsSystem::value().AddObject(object, velocity, glm::vec3(0.0f), creature, false, player) !=
			         nullptr;
			break;
		case ecs::creature_physics::Release::LetGo:
		{
			// the creature is the thrower of what it lets go of, which leaves its hand turning about the vertical
			const auto spin =
			    ecs::creature_physics::LetGoSpin(!Locator::config::has_value() || Locator::config::value().creatureLetGoSpins);
			placed = ecs::physics::from_hand::InitialisePhysicsFromHand(object, velocity, false, player, spin, true, creature)
			             .has_value();
			break;
		}
		}
		if (!placed)
		{
			// No body was made for it. Let go, a thing a script made immovable hangs where it was let go, as from the
			// player's hand; anything else is put down where it is
			if (release == ecs::creature_physics::Release::LetGo)
			{
				ecs::physics::from_hand::LetGoWithoutBody(object);
			}
			else
			{
				ecs::physics::from_hand::PlaceWithoutBody(object);
			}
			placed = true;
		}
	}
	if (!placed)
	{
		ecs::map_cells::InsertMapObject(object);
	}
	// Out of the hand
	ecs::villager_physics::InterfaceSetOutMagicHand(object);
}

CreatureObjectActionSystem::State CreatureObjectActionSystem::GetState(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	// One asked while the hands played another is under way from the start
	if (registry.Valid(creature) && registry.AllOf<CreatureObjectActionWaiting>(creature))
	{
		return State::Busy;
	}
	if (action == nullptr)
	{
		// A catch's hands' part is over as soon as it starts: its body carries it on by itself
		return registry.Valid(creature) && registry.AllOf<CreatureCatching>(creature) ? State::Done : State::Idle;
	}
	switch (action->status)
	{
	case Status::Running:
	case Status::Contact:
		return State::Busy;
	case Status::Done:
		return State::Done;
	case Status::Failed:
		break;
	}
	return State::Failed;
}

std::optional<float> CreatureObjectActionSystem::GetProgress(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->phase != Phase::Playing ||
	    (action->status != Status::Running && action->status != Status::Contact))
	{
		return std::nullopt;
	}
	if (action->holdMs > 0.0f || action->durationMs <= 0.0f)
	{
		return 0.0f;
	}
	return std::clamp(action->timeMs / action->durationMs, 0.0f, 1.0f);
}

std::optional<entt::entity> CreatureObjectActionSystem::GetHeld(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* held = registry.Valid(creature) ? registry.TryGet<const CreatureHeldObject>(creature) : nullptr;
	return held != nullptr && registry.Valid(held->object) ? std::optional(held->object) : std::nullopt;
}

std::optional<entt::entity> CreatureObjectActionSystem::GetEating(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	if (action == nullptr || action->kind != Kind::Eat || !action->target.has_value() || !registry.Valid(*action->target))
	{
		return std::nullopt;
	}
	return action->target;
}

namespace
{
/// The food type of a thing's kind's row, for a villager, an animal or a mobile object; none for anything else (a pot
/// is worth the food in it whatever its kind)
std::optional<FoodType> RowFoodType(const ecs::Registry& registry, entt::entity object)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	if (registry.AllOf<Villager>(object))
	{
		const auto* villager = ecs::VillagerInfoOf(object);
		return villager != nullptr ? std::optional(villager->foodType) : std::nullopt;
	}
	if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		const auto kind = static_cast<size_t>(animal->type);
		return kind < info.animal.size() ? std::optional(info.animal.at(kind).foodType) : std::nullopt;
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		const auto kind = static_cast<size_t>(mobile->type);
		return kind < info.mobileObject.size() ? std::optional(info.mobileObject.at(kind).foodType) : std::nullopt;
	}
	return std::nullopt;
}

/// What a thing is for the stomp's test of its class beyond its kind: a town's building, a field, a fish farm, a thing
/// set into the land, or a static lying about
creature_object_actions::StompGround StompGroundOf(const ecs::Registry& registry, entt::entity object)
{
	using creature_object_actions::StompGround;
	if (registry.AllOf<Abode>(object))
	{
		return StompGround::Building;
	}
	if (registry.AllOf<Field>(object))
	{
		return StompGround::Field;
	}
	if (registry.AllOf<FishFarm>(object))
	{
		return StompGround::FishFarm;
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(object))
	{
		// The singing stones' base is made as a thing of its own set into the land, every other row as a static
		return still->type == MobileStaticInfo::SingingStoneBase ? StompGround::SetIntoLand : StompGround::LyingAbout;
	}
	if (registry.AnyOf<Temple, WorshipSite, VillageTotem, Feature, AnimatedStatic, Flowers, MagicShield>(object))
	{
		return StompGround::SetIntoLand;
	}
	return StompGround::None;
}
} // namespace

float CreatureObjectActionSystem::MealValueOf(entt::entity object) const
{
	// The meal reads the row's value only for meat or plant food, as the game asks for the value of food of those types
	const float value = FoodValueOf(object).value_or(0.0f);
	const auto type = ecs::IsAvailable(object) ? RowFoodType(Locator::entitiesRegistry::value(), object) : std::nullopt;
	return type.has_value() ? creature_eating::MealValue(value, *type) : value;
}

std::optional<float> CreatureObjectActionSystem::FoodValueOf(entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::infoConstants::has_value() || !ecs::IsAvailable(object))
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	float value = 0.0f;
	if (registry.AllOf<Villager>(object))
	{
		const auto* villager = ecs::VillagerInfoOf(object);
		value = villager != nullptr ? villager->foodValue : 0.0f;
	}
	else if (const auto* animal = registry.TryGet<const Animal>(object))
	{
		const auto kind = static_cast<size_t>(animal->type);
		value = kind < info.animal.size() ? info.animal.at(kind).foodValue : 0.0f;
	}
	else if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		const auto kind = static_cast<size_t>(mobile->type);
		value = kind < info.mobileObject.size() ? info.mobileObject.at(kind).foodValue : 0.0f;
	}
	else if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		// A pot or pile of food is worth as much as the food in it. A storage pit's pile is eaten from where it lies,
		// which isn't done here.
		const auto kind = static_cast<size_t>(pot->type);
		const bool food = kind < info.pot.size() && info.pot.at(kind).resourceType == ResourceType::Food &&
		                  pot->type != PotInfo::StoragePitFoodPile;
		value = food ? static_cast<float>(pot->amount) : 0.0f;
	}
	return value > 0.0f ? std::optional(value) : std::nullopt;
}

creature_object_actions::Failure CreatureObjectActionSystem::GetFailure(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.Valid(creature) ? registry.TryGet<const CreatureObjectAction>(creature) : nullptr;
	return action != nullptr && action->status == Status::Failed ? action->failureKind
	                                                             : creature_object_actions::Failure::Other;
}

bool CreatureObjectActionSystem::CanPickUp(entt::entity creature, entt::entity object) const
{
	return PickUpTest(creature, object, false);
}

bool CreatureObjectActionSystem::CanPickUpIgnoringMap(entt::entity creature, entt::entity object) const
{
	return PickUpTest(creature, object, true);
}

creature_object_actions::KindFacts CreatureObjectActionSystem::KindFactsOf(entt::entity object)
{
	using creature_object_actions::Thing;
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || !registry.AllOf<Transform>(object))
	{
		return {};
	}
	const auto* info = Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
	creature_object_actions::KindFacts facts;
	const auto thing = [&] {
		if (registry.AllOf<Creature>(object))
		{
			return Thing::Creature;
		}
		if (registry.AllOf<Villager>(object))
		{
			return Thing::Villager;
		}
		if (const auto* animal = registry.TryGet<const Animal>(object))
		{
			// A bird goes by its height over the land
			if (ecs::animal_ai::IsDoveClass(animal->type))
			{
				facts.low =
				    Locator::animalSystem::has_value() && Locator::animalSystem::value().CanBePickedUpByCreature(object);
				return Thing::Bird;
			}
			return Thing::Animal;
		}
		if (registry.AllOf<Ball>(object))
		{
			return Thing::Ball;
		}
		if (const auto* pot = registry.TryGet<const Pot>(object))
		{
			// A pile of food or wood goes by whether its kind may fly about
			const auto kind = static_cast<size_t>(pot->type);
			if (info != nullptr && kind < info->pot.size() && info->pot.at(kind).potType != PotType::Pot)
			{
				facts.mayFly = info->pot.at(kind).canBecomeAPhysicsObject != 0;
				return Thing::Pile;
			}
			return Thing::Pot;
		}
		if (const auto* mobile = registry.TryGet<const MobileObject>(object))
		{
			// A creed (the creed and the eyes) by the class the game makes it as
			const auto kind = static_cast<size_t>(mobile->type);
			const bool creed = info != nullptr && kind < info->mobileObject.size() &&
			                   info->mobileObject.at(kind).mobileType == MobileObjectInfo::Creed;
			return creed ? Thing::Creed : Thing::MobileObject;
		}
		// A one-shot miracle's globe lies about as any other thing does
		if (registry.AllOf<OneOffSpellSeed>(object))
		{
			return Thing::MobileObject;
		}
		if (const auto* still = registry.TryGet<const MobileStatic>(object))
		{
			if (ecs::object_physics::IsRockObject(object))
			{
				return Thing::Rock;
			}
			switch (still->type)
			{
			case MobileStaticInfo::Bonfire:
				return Thing::Bonfire;
			case MobileStaticInfo::StreetLantern:
			case MobileStaticInfo::CountryLantern:
				return Thing::Lantern;
			case MobileStaticInfo::PhysicalShield:
				return Thing::Shield;
			case MobileStaticInfo::Vortex:
				return Thing::Vortex;
			case MobileStaticInfo::Teleport:
				return Thing::TeleportStone;
			default:
				break;
			}
			// The toys are known by their models: a ball, a bowling ball, a cuddly toy, a die and a skittle
			const auto kind = static_cast<size_t>(still->type);
			facts.toy = info != nullptr && kind < info->mobileStatic.size() &&
			            creature_struck::IsToyModel(info->mobileStatic.at(kind).meshId);
			return Thing::Static;
		}
		if (registry.AllOf<DeadTree>(object))
		{
			return Thing::DeadTree;
		}
		if (registry.AllOf<Tree>(object))
		{
			return Thing::Tree;
		}
		if (const auto* feature = registry.TryGet<const Feature>(object);
		    feature != nullptr && feature->type == FeatureInfo::MagicMushroom)
		{
			return Thing::MagicMushroom;
		}
		// The street lanterns and the shields have entities of their own
		if (registry.AllOf<StreetLantern>(object))
		{
			return Thing::Lantern;
		}
		if (registry.AllOf<MagicShield>(object))
		{
			return Thing::Shield;
		}
		// What is built or set into the land: buildings, the temple, totems, worship sites, features, animated statics and
		// flowers
		if (registry.AnyOf<Abode, Temple, WorshipSite, VillageTotem, Feature, AnimatedStatic, Flowers>(object))
		{
			return Thing::Fixture;
		}
		return Thing::Other;
	}();
	facts.thing = thing;
	facts.stompGround = StompGroundOf(registry, object);
	facts.rule = creature_object_actions::RuleOf(thing);
	facts.holdable = creature_object_actions::Holdable(thing);
	return facts;
}

bool CreatureObjectActionSystem::PickUpTest(entt::entity creature, entt::entity object, bool ignoringMap)
{
	using creature_object_actions::Thing;
	const auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object) || !registry.AllOf<Transform>(object))
	{
		return false;
	}
	// The thing's kind decides first: the football and creeds always, toys, birds low over the land, mushrooms and piles
	// by their own tests, and what no hand holds never
	const auto kind = KindFactsOf(object);
	// The cannot-be-picked-up flag alone: a beach temple puzzle piece's own refusal is the hand's, not the creature's
	const bool cannot = ecs::object_flags::HasCannotBePickedUpFlag(object);
	if (const auto decided = creature_object_actions::DecidedByKind(kind, !cannot); decided.has_value())
	{
		return *decided;
	}
	// The rest by the test of things in general, by their kind's weight
	if (!Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value();
	switch (kind.thing)
	{
	case Thing::Animal:
	{
		const auto type = static_cast<size_t>(registry.Get<const Animal>(object).type);
		return PassesPickUpTest(creature, object, type < info.animal.size() ? info.animal.at(type).weight : 0.0f, false,
		                        !cannot, ignoringMap);
	}
	case Thing::Villager:
	{
		// Not one worshipping at its own player's site, nor one inside its home
		const auto* villagerInfo = ecs::VillagerInfoOf(object);
		const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
		const bool own = body != nullptr && creature_pick_up::IsOwnWorshipper(ecs::villager::GetFinalState(object) ==
		                                                                          VillagerStates::WorshippingAtWorshipSite,
		                                                                      ecs::villager::GetPlayerOf(object), body->owner);
		return PassesPickUpTest(creature, object, villagerInfo != nullptr ? villagerInfo->weight : 0.0f, own,
		                        creature_pick_up::VillagerAllows(registry.AllOf<AtHome>(object), cannot), ignoringMap);
	}
	case Thing::MobileObject:
	{
		// A scaffold only while its building may still be moved, and by the scaffolds' weight. A one-shot miracle's
		// globe goes by its mobile object row, as the map cells read it
		const auto* mobile = registry.TryGet<const MobileObject>(object);
		const auto type =
		    mobile != nullptr ? static_cast<size_t>(mobile->type) : static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed);
		const auto* row = type < info.mobileObject.size() ? &info.mobileObject.at(type) : nullptr;
		if (registry.AllOf<Scaffold>(object))
		{
			return ecs::scaffolds::ValidForPlaceInHand(object) &&
			       PassesPickUpTest(creature, object, info.scaffold.weight, false, !cannot, ignoringMap);
		}
		return PassesPickUpTest(creature, object, row != nullptr ? row->weight : 0.0f, false, !cannot, ignoringMap);
	}
	case Thing::Pot:
	{
		const auto type = static_cast<size_t>(registry.Get<const Pot>(object).type);
		if (type >= info.pot.size())
		{
			return false;
		}
		return PassesPickUpTest(creature, object, info.pot.at(type).weight, false, !cannot, ignoringMap);
	}
	case Thing::Rock:
	case Thing::Bonfire:
	{
		// A rock or a bonfire by its static's row
		const auto type = static_cast<size_t>(registry.Get<const MobileStatic>(object).type);
		return PassesPickUpTest(creature, object, type < info.mobileStatic.size() ? info.mobileStatic.at(type).weight : 0.0f,
		                        false, !cannot, ignoringMap);
	}
	case Thing::DeadTree:
	case Thing::Tree:
	{
		// A tree, standing or dead, by its tree's row
		const auto type = kind.thing == Thing::Tree ? static_cast<size_t>(registry.Get<const Tree>(object).type)
		                                            : static_cast<size_t>(registry.Get<const DeadTree>(object).type);
		return PassesPickUpTest(creature, object, type < info.tree.size() ? info.tree.at(type).weight : 0.0f, false, !cannot,
		                        ignoringMap);
	}
	default:
		return false;
	}
}

std::optional<float> CreatureObjectActionSystem::PickUpReach(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature, CreatureAnimation, Transform>(creature) ||
	    !Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	// The hand's points in the four pick-up animations, at size one, measured once for the body as it is built: the
	// body posed first when it has not been yet, as the game poses it when its test asks
	auto& animations = Locator::creatureAnimationSystem::value();
	if (!animations.EnsurePosed(creature))
	{
		return std::nullopt;
	}
	const auto revision = registry.Get<const CreatureAnimation>(creature).builtRevision;
	const auto* kept = registry.TryGet<const CreaturePickUpReach>(creature);
	if (kept == nullptr || kept->bodyRevision != revision)
	{
		const auto* points = PointsOf(registry.Get<const Creature>(creature));
		if (points == nullptr)
		{
			return std::nullopt;
		}
		auto unit = registry.Get<const Transform>(creature);
		unit.scale = glm::vec3(1.0f);
		const auto measured =
		    MeasureReach(creature, unit, points->rightHand, creature_reach::k_PickUpAnimations, points->pickUpMs, true);
		if (!measured.has_value())
		{
			return std::nullopt;
		}
		kept = &registry.AssignOrReplace<CreaturePickUpReach>(
		    creature, CreaturePickUpReach {.points = *measured, .bodyRevision = revision});
	}
	// scaled to the size the body is drawn at
	return creature_reach::MaxReach3D(kept->points, ecs::creature_pose::DrawnScale(std::as_const(registry), creature).x);
}

float CreatureObjectActionSystem::CreatureWeight(entt::entity creature)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	if (body == nullptr)
	{
		return 0.0f;
	}
	const auto* morph = registry.TryGet<const CreatureMorph>(creature);
	// the weight the creature's body is to have: its size drawn, and the thin to fat and weak to strong it is becoming
	const auto target = morph != nullptr
	                        ? creature_morph::FromAttributes(body->alignment, morph->shownFatness, body->strength,
	                                                         archetypes::CreatureArchetype::SpeciesStrength(body->species))
	                        : creature_morph::Morph {};
	const float drawnSize = creature_morph::ClampScale(ecs::creature_pose::DrawnSize(registry, creature));
	return openblack::physics::living::CreatureMass(drawnSize, target.thinFat, target.weakStrong);
}

bool CreatureObjectActionSystem::CanThrowToImpress(entt::entity creature, entt::entity object) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature) || !registry.Valid(object))
	{
		return false;
	}
	return creature_object_actions::CanBeThrownToImpress(KindFactsOf(object).thing, ecs::object::GetHeight(object),
	                                                     CreatureWeight(creature));
}

bool CreatureObjectActionSystem::PassesPickUpTest(entt::entity creature, entt::entity object, float kindWeight,
                                                  bool ownWorshipper, bool thingAllows, bool ignoringMap)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.Valid(creature) ? registry.TryGet<const Creature>(creature) : nullptr;
	const auto reach = body != nullptr ? PickUpReach(creature) : std::nullopt;
	if (body == nullptr || !reach.has_value())
	{
		// (openblack) without the species' meshes, as in tests, the creature has no reach to measure
		return false;
	}
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	const auto* leash = registry.TryGet<const CreatureLeash>(creature);
	const creature_pick_up::Check check {
	    .computerPlayer = magic::players::ReadMagicOf(body->owner).playerType == PlayerMagic::k_ComputerPlayerType,
	    .creatureScripted = ecs::script_held::IsControlledByScript(creature),
	    .thingInScript = ecs::script_held::IsInScript(object),
	    .ownWorshipper = ownWorshipper,
	    .inMap = ignoringMap || ecs::map_cells::IsObjectInMap(object),
	    .inThisHand = held != nullptr && held->object == object,
	    .leashTiedTo = leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo == object,
	    .thingRadius = ecs::object::Get2DRadius(object),
	    .creatureRadius = ecs::object::Get2DRadius(creature),
	    .reach = *reach,
	    .thingWeight = creature_pick_up::ThingWeight(ecs::object::GetScale(object), kindWeight),
	    .creatureWeight = CreatureWeight(creature),
	    .thingAllows = thingAllows,
	};
	return creature_pick_up::CreatureMayPickUp(check);
}

bool CreatureObjectActionSystem::CanDestroy(entt::entity target) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	// Statics too, rocks among them
	return ecs::IsAvailable(target) && registry.AllOf<Transform>(target) &&
	       registry.AnyOf<Tree, Abode, MobileObject, MobileStatic>(target);
}

namespace
{
/// The model of a thing's row in the game's tables, for the kinds whose rows name one: statics, mobile objects, pots
/// and piles, and the football
std::optional<MeshId> RowModelOf(const ecs::Registry& registry, entt::entity object)
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	if (const auto* still = registry.TryGet<const MobileStatic>(object))
	{
		const auto kind = static_cast<size_t>(still->type);
		return kind < info.mobileStatic.size() ? std::optional(info.mobileStatic.at(kind).meshId) : std::nullopt;
	}
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		const auto kind = static_cast<size_t>(pot->type);
		return kind < info.pot.size() ? std::optional(info.pot.at(kind).meshId) : std::nullopt;
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		const auto kind = static_cast<size_t>(mobile->type);
		return kind < info.mobileObject.size() ? std::optional(info.mobileObject.at(kind).meshId) : std::nullopt;
	}
	if (registry.AllOf<Ball>(object))
	{
		return info.ball.meshId;
	}
	return std::nullopt;
}
} // namespace

bool CreatureObjectActionSystem::CanKick(entt::entity creature, entt::entity object) const
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature) || !registry.Valid(object))
	{
		return false;
	}
	// A home has a test of its own: built and not on fire. (pending) fields, the football pitch and the totems are
	// homes to this test too, and are not asked here
	if (registry.AllOf<Abode>(object))
	{
		return creature_object_actions::CanHomeBeKicked(ecs::fire::IsOnFire(object), ecs::abodes::IsBuilt(object));
	}
	const auto* held = registry.TryGet<const CreatureHeldObject>(creature);
	const auto* leash = registry.TryGet<const CreatureLeash>(creature);
	const bool isCreature = registry.AllOf<Creature>(object);
	// The weights are only read of a thing that is no creature and can fly, as the game reads them
	const bool canFly =
	    !isCreature && Locator::dynamicsSystem::has_value() && Locator::dynamicsSystem::value().CanBecomeAPhysicsObject(object);
	return creature_object_actions::CanBeKicked({
	    .leashTiedTo = leash != nullptr && leash->worn.has_value() && leash->worn->tiedTo == object,
	    .bowlingBall = RowModelOf(registry, object) == MeshId::ObjectToyBowlingBall,
	    .creature = isCreature,
	    .canBecomePhysicsObject = canFly,
	    .immovable = ecs::object_flags::IsImmovable(object),
	    .thingWeight = canFly ? WeightOf(registry, object).value_or(0.0f) : 0.0f,
	    .creatureWeight = canFly ? CreatureWeight(creature) : 0.0f,
	    .indestructible = registry.AllOf<Indestructible>(object),
	    .carriedByIt = held != nullptr && held->object == object,
	    .inHand = Locator::handSystem::has_value() && Locator::handSystem::value().IsInMagicHand(object),
	});
}

void CreatureObjectActionSystem::ProcessApproach()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}
	LetGoOfWhatIsLoose(registry);

	// What was asked while the hands played another action or caught is set about once that has ended
	std::vector<entt::entity> waiting;
	lookup.Each<const CreatureObjectActionWaiting>(
	    [&waiting](entt::entity entity, const CreatureObjectActionWaiting&) { waiting.push_back(entity); });
	for (const auto entity : waiting)
	{
		if (HandsArePlaying(lookup, entity) || IsCatching(entity))
		{
			continue;
		}
		auto next = std::move(registry.Get<CreatureObjectActionWaiting>(entity).action);
		registry.RemoveState<CreatureObjectActionWaiting>(entity);
		static_cast<void>(Start(entity, std::move(next)));
	}

	// Walking or turning up to what they act on, or setting about it, before the creatures move this turn
	for (const auto entity : Acting(lookup))
	{
		auto& action = registry.Get<CreatureObjectAction>(entity);
		if (action.status != Status::Running || action.phase != Phase::Approach)
		{
			continue;
		}
		if (StartsAtOnce(action.kind))
		{
			// What it was to act on in its hand may have gone while it waited
			if (creature_object_actions::HandsFor(action.kind) == creature_object_actions::Hands::Holding &&
			    !lookup.AllOf<CreatureHeldObject>(entity))
			{
				Fail(action, "it has nothing in its hand");
			}
			else if (!BodyIsBusy(entity, lookup.Get<const CreatureAnimation>(entity)))
			{
				BeginAtOnce(entity, action);
			}
			continue;
		}
		Approach(entity, action, lookup.Get<Creature>(entity), lookup.Get<Transform>(entity),
		         registry.Get<CreatureAnimation>(entity), lookup.TryGet<const CreatureHeldObject>(entity));
	}
}

void CreatureObjectActionSystem::BeginAtOnce(entt::entity creature, CreatureObjectAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	// A take from the hand reaches for what the player's hand holds as it sets about it, and fails with nothing there
	if (action.kind == Kind::TakeFromHand)
	{
		const auto given = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHeldObject() : std::nullopt;
		if (!given.has_value())
		{
			Fail(action, "the hand holds nothing");
			return;
		}
		action.target = *given;
	}
	// Holding something already, it takes nothing from the hand, and the take is done all the same
	if (action.kind == Kind::TakeFromHand && std::as_const(registry).AllOf<CreatureHeldObject>(creature))
	{
		action.status = Status::Done;
		return;
	}
	BeginPlaying(creature, action, registry.Get<CreatureAnimation>(creature));
	// The meal's energy comes as the eating starts, by the food value of what it holds (none: nothing)
	const auto* held = std::as_const(registry).TryGet<const CreatureHeldObject>(creature);
	if (action.kind == Kind::Eat && held != nullptr && Locator::creaturePhysiologySystem::has_value())
	{
		Locator::creaturePhysiologySystem::value().Eat(creature, MealValueOf(held->object));
	}
}

void CreatureObjectActionSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!Locator::creatureLocomotionSystem::has_value() || !Locator::creatureAnimationSystem::has_value())
	{
		return;
	}

	LetGoOfWhatIsLoose(registry);

	const auto acting = Acting(lookup);

	// Its own turn to face what it acts on, ended this turn, goes straight on into acting on it
	const auto& locomotion = Locator::creatureLocomotionSystem::value();
	for (const auto entity : acting)
	{
		auto& action = registry.Get<CreatureObjectAction>(entity);
		if (action.status == Status::Running && action.phase == Phase::Approach && action.turningToIt &&
		    !locomotion.IsMoving(entity))
		{
			Approach(entity, action, lookup.Get<Creature>(entity), lookup.Get<Transform>(entity),
			         registry.Get<CreatureAnimation>(entity), lookup.TryGet<const CreatureHeldObject>(entity));
		}
	}

	// A knock-down that has landed on a living thing crushes it every turn its animation plays on, before it plays on
	for (const auto entity : acting)
	{
		const auto* action = lookup.Valid(entity) ? lookup.TryGet<const CreatureObjectAction>(entity) : nullptr;
		if (action == nullptr || action->phase != Phase::Playing || !action->target.has_value())
		{
			continue;
		}
		const auto target = *action->target;
		const bool living = ecs::IsAvailable(target) && IsLivingThing(lookup, target);
		if (creature_object_actions::KnockDownCrushes(action->kind, action->status, living,
		                                              living && ecs::map_cells::IsObjectInMap(target)))
		{
			CrushKnockedDown(lookup, entity, target);
		}
		// Then, as the agenda's last step, a thing in the map left with no life counts among the creature's kills by its
		// type, a person, an animal or a creature, and the stomp achieved the plan's aim: the end of the plan may then
		// lessen the desire of its action
		if (lookup.Valid(target) &&
		    creature_object_actions::KnockDownKills(action->kind, action->status, ecs::map_cells::IsObjectInMap(target),
		                                            OnLastStep(lookup, entity), ecs::life::LifeOf(target)))
		{
			ecs::player_statistics::CountCreatureMeal(registry, entity, static_cast<uint32_t>(ecs::map_cells::TypeOf(target)));
			if (auto* mind = registry.TryGet<CreatureMindState>(entity); mind != nullptr && mind->desires.has_value())
			{
				mind->desires->achieved = true;
			}
		}
	}

	// The actions play on by a turn
	for (const auto entity : acting)
	{
		if (!lookup.AllOf<CreatureObjectAction>(entity))
		{
			continue;
		}
		auto& action = registry.Get<CreatureObjectAction>(entity);
		if (action.phase != Phase::Playing || (action.status != Status::Running && action.status != Status::Contact))
		{
			continue;
		}
		const auto& transform = lookup.Get<Transform>(entity);
		// at the animation rate of the size it is drawn at (the pen's in its pen)
		const auto* body = lookup.TryGet<const CreatureAnimation>(entity);
		const auto stepMs = TurnMs() *
		                    creature_layers::BodyRate(ecs::creature_pose::DrawnSize(lookup, entity),
		                                              (body != nullptr && body->realSpeed) ||
		                                                  creature_object_actions::PlaysAtRealSpeed(action.kind)) *
		                    (action.hurried ? creature_object_actions::k_HurriedPace : 1.0f);
		// Pointing, the arm rises to the species' point moment and stays there, round the low left animation
		action.timeMs =
		    IsHoldingPoint(action)
		        ? creature_object_actions::PointClipTime(action.timeMs, stepMs, action.pointMs,
		                                                 DurationOf(entity, creature_object_actions::k_PointAnimations[0]))
		        : action.timeMs + stepMs;

		// Until it has hold, the reach follows what it reaches for, and fails when it gets out of reach
		if ((action.kind == Kind::PickUp || action.kind == Kind::Destroy) && !action.eventDone && action.reach.has_value())
		{
			const auto* at = action.target.has_value() && ecs::IsAvailable(*action.target)
			                     ? lookup.TryGet<const Transform>(*action.target)
			                     : nullptr;
			if (at == nullptr)
			{
				Fail(action, "it has gone");
				continue;
			}
			const auto local = ToLocal(transform, at->position);
			const auto reach =
			    creature_reach::Solve(action.mirrored ? creature_reach::Mirrored(*action.reach) : *action.reach, local);
			if (!reach.inRange)
			{
				Fail(action, "it got out of reach");
				continue;
			}
			action.weights = reach.weights;
		}

		if (action.holdMs > 0.0f)
		{
			// Pointing at a thing, it points wherever the thing has got to
			if (action.kind == Kind::Point && action.target.has_value() && lookup.Valid(*action.target))
			{
				if (const auto* at = lookup.TryGet<const Transform>(*action.target); at != nullptr)
				{
					action.point = at->position;
					SetPointing(lookup, entity, action, transform);
				}
			}
			action.holdMs -= TurnMs();
			if (action.holdMs <= 0.0f)
			{
				action.status = Status::Done;
			}
		}
		else if (action.timeMs >= action.durationMs && (action.eventDone || action.eventMs > action.durationMs))
		{
			action.status = Status::Done;
		}
	}

	// The moments things are taken hold of, knocked down, let go of and eaten
	for (const auto entity : acting)
	{
		const auto* action = lookup.TryGet<const CreatureObjectAction>(entity);
		if (action != nullptr && action->phase == Phase::Playing && action->status == Status::Running && !action->eventDone &&
		    action->timeMs >= action->eventMs)
		{
			Moment(entity);
		}
	}

	// The meals whose eating is over
	for (const auto entity : acting)
	{
		auto* action = registry.TryGet<CreatureObjectAction>(entity);
		if (action != nullptr && action->kind == Kind::Eat && action->status == Status::Done && action->meal.has_value())
		{
			const auto meal = *std::exchange(action->meal, std::nullopt);
			EndMeal(entity, meal);
		}
	}

	// An action that played on after what it was for stopped goes once its animations end, and the body stands again
	for (const auto entity : acting)
	{
		const auto* action = registry.TryGet<const CreatureObjectAction>(entity);
		if (action != nullptr && action->playsOn && (action->status == Status::Done || action->status == Status::Failed))
		{
			registry.RemoveState<CreatureObjectAction>(entity);
			if (auto* animation = registry.TryGet<CreatureAnimation>(entity))
			{
				animation->slots.clear();
			}
		}
	}

	ProcessCatches();

	// What is held rides in the hand, turned with it; out of the map cells while it is held
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureHeldObject>(
	    [&](entt::entity entity, const Creature&, const Transform& transform, const CreatureAnimation& animation,
	        const CreatureHeldObject& held) {
		    if (!ecs::IsAvailable(held.object) || !lookup.AllOf<Transform>(held.object) || animation.boneMatrices.empty())
		    {
			    return;
		    }
		    auto& at = registry.Get<Transform>(held.object);
		    // In the hand of the body as posed for the last turn, in the world, where what it holds is placed as it lets go
		    // of it; a body not posed for a turn yet, or posed with other bones, as the frame poses it under its Transform.
		    // A thing is turned by the hand's bone and hung below the palm; a creature is turned with the body and held by
		    // its middle
		    const auto* pose = lookup.TryGet<const CreatureTurnPose>(entity);
		    const bool turnPosed = pose != nullptr && pose->current.size() == animation.boneMatrices.size();
		    const std::span<const glm::mat4> bones = turnPosed ? pose->current : animation.boneMatrices;
		    const auto placement = turnPosed ? glm::mat4(1.0f) : PlacementOf(transform);
		    if (lookup.AllOf<Creature>(held.object))
		    {
			    at.rotation = transform.rotation * held.rotation;
			    at.position = PalmOf(held.bone, animation, bones, placement) - (at.rotation * held.middle);
		    }
		    else
		    {
			    const auto placed = HeldPlacementOf(lookup, held, animation, bones, placement);
			    at.rotation = placed.turn;
			    at.position = placed.position;
		    }
	    });

	// Carrying something heavy makes it stronger as it walks about
	std::vector<entt::entity> bodies;
	lookup.Each<const Creature, const CreatureNeeds>(
	    [&bodies](entt::entity entity, const auto&...) { bodies.push_back(entity); });
	for (const auto entity : bodies)
	{
		auto& needs = registry.Get<CreatureNeeds>(entity);
		needs.carriedWeight.reset();
		const auto held = GetHeld(entity);
		if (!held.has_value())
		{
			continue;
		}
		if (const auto weight = WeightOf(lookup, *held))
		{
			needs.carriedWeight =
			    openblack::physics::living::CarriedShare(*weight, ecs::creature_physics::Weight(lookup, entity));
		}
	}
}

namespace
{
/// The creaks of a tree a creature pulls from the ground, as the original plays them: one of the three tree-break
/// samples kept for a creature (the in-game bank's 136 to 138), drawn from the local random numbers, as a point sound
/// at the creature (docs/wiki/creature/plans-and-needs.md#the-kind-actions-in-openblack)
constexpr int k_TreeCreakFirstSample = 136;
constexpr int k_TreeCreakSamples = 3;
constexpr int k_TreeCreakMode = 3;

/// A living thing a creature takes hold of (picked up, handed over or caught) goes into the held state, its state kept
/// as its previous one, as the creature's take-hold puts it there, and keeps its y angle to its landing as anything held does.
/// Nothing else of the hand's take is done: the hand's own in-hand mark, a kissing partner let go and an animal taken off its
/// flock are the hand's, and an artefact caught keeps its owner
void HoldCaught(ecs::Registry& registry, entt::entity object)
{
	if (registry.AllOf<Villager>(object))
	{
		ecs::villager::StorePreviousState(object);
		ecs::SetVillagerStateWithExit(object, VillagerStates::InHand);
	}
	else if (registry.AllOf<Animal>(object))
	{
		ecs::animal_ai::PlaceInCreaturesHand(object);
	}
	if (registry.AnyOf<Villager, Animal>(object))
	{
		registry.AssignOrReplace<ecs::villager_physics::HeldYAngle>(object, ecs::villager_physics::ObjectYAngle(object));
	}
}

/// The creature's hand takes hold of a thing at the palm: taken out of the physics, into the hand and off the map, as the
/// player's hand takes things, and drawn in the palm by its middle at once. A villager that may not leave its state for
/// the held one is not taken: false
bool TakeIntoHand(ecs::Registry& registry, entt::entity creature, entt::entity object, bool mirrored, glm::vec3 handPosition,
                  const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, const Transform& transform)
{
	const auto& lookup = std::as_const(registry);
	const auto bone = HandBone(points, animation, mirrored);
	const auto middle = MiddleOf(registry, object);
	const auto rotation = glm::transpose(transform.rotation) * lookup.Get<Transform>(object).rotation;
	// A living thing goes into the held state first, as the creature's take-hold puts it there before anything else of
	// the hold, with none of the player's hand's take; a villager its state will not let go is let go of at once
	HoldCaught(registry, object);
	if (lookup.AllOf<Villager, LivingAction>(object) &&
	    ecs::villager::GetState(object, LivingAction::Index::Top) != VillagerStates::InHand)
	{
		return false;
	}
	// A standing tree pulled from the ground creaks, at the creature: one of the three tree-break sounds
	if (lookup.AllOf<Tree>(object) && !Locator::dynamicsSystem::value().IsFlying(object))
	{
		audio::tags::Create(transform.position, audio::tags::RandomSample(k_TreeCreakFirstSample, k_TreeCreakSamples), false,
		                    k_TreeCreakMode, 0, false, true, audio::SfxBank::InGame, 0);
	}
	// Taken out of the physics and off the map
	Locator::dynamicsSystem::value().RemoveObject(object);
	// an artefact leaves its town, and the creature's player owns it
	ecs::object_physics::ArtefactTaken(object, lookup.Get<Creature>(creature).owner);
	if (lookup.AllOf<Pot>(object))
	{
		ecs::animal_ai::RemovePotReaction(object);
	}
	if (ecs::map_cells::IsObjectInMap(object))
	{
		ecs::map_cells::RemoveMapObject(object);
	}
	if (lookup.AllOf<MorphWithTerrain>(object))
	{
		registry.Remove<MorphWithTerrain>(object);
	}
	registry.AssignOrReplaceState<CreatureHeldObject>(creature,
	                                                  CreatureHeldObject {.object = object,
	                                                                      .mirrored = mirrored,
	                                                                      .bone = bone,
	                                                                      .rotation = rotation,
	                                                                      .middle = middle,
	                                                                      .shadowTaken = TakeShadow(registry, object)});
	registry.AssignOrReplaceState<HeldByCreature>(object, HeldByCreature {.creature = creature});
	// Into the palm, drawn there at once: a thing turned by the hand's bone as the frame poses it and hung below the
	// palm, a creature turned with the body and held by its middle
	auto& at = registry.Get<Transform>(object);
	if (lookup.AllOf<Creature>(object))
	{
		at.rotation = transform.rotation * rotation;
		at.position = handPosition - (at.rotation * middle);
	}
	else
	{
		const auto hand = glm::mat3(creature::PosedBone(bone, animation.boneMatrices, PlacementOf(transform)));
		const auto placed = creature_hold::Place(hand, handPosition, mirrored, ecs::object::GetScale(object),
		                                         ecs::hold_lowering::LoweringOf(lookup, object));
		at.rotation = placed.turn;
		at.position = placed.position;
	}
	ecs::NotifyTeleported(object);
	return true;
}
} // namespace

void CreatureObjectActionSystem::Moment(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	auto& action = registry.Get<CreatureObjectAction>(creature);
	const auto& body = lookup.Get<Creature>(creature);
	const auto& transform = lookup.Get<Transform>(creature);
	const auto& animation = lookup.Get<CreatureAnimation>(creature);
	const auto* points = PointsOf(body);
	action.eventDone = true;
	if (points == nullptr)
	{
		return;
	}
	// The palm as the animations have it at the moment, else as the body was last drawn
	const auto handPosition =
	    PalmAtMoment(creature, action, *points, animation, transform)
	        .value_or(animation.boneMatrices.empty()
	                      ? transform.position
	                      : PalmOf(HandBone(*points, animation, action.mirrored), animation, PlacementOf(transform)));
	switch (action.kind)
	{
	case Kind::PickUp:
	{
		const auto object = *action.target;
		if (!ecs::IsAvailable(object) || lookup.AllOf<HeldByCreature>(object) || !lookup.AllOf<Transform>(object))
		{
			Fail(action, "it has gone");
			break;
		}
		if (!TakeIntoHand(registry, creature, object, action.mirrored, handPosition, *points, animation, transform))
		{
			Fail(action, "it will not be held");
			break;
		}
		action.status = Status::Contact;
		// A portion's source loses what the portion holds, when it is a pot or a pile
		if (action.source.has_value() && ecs::IsAvailable(*action.source) && lookup.AllOf<Pot>(*action.source) &&
		    lookup.AllOf<Pot>(object))
		{
			const auto type = ecs::object_resources::GetResourceType(object);
			static_cast<void>(
			    ecs::object_resources::RemoveResource(*action.source, type, ecs::object_resources::GetResource(object, type)));
		}
		break;
	}
	case Kind::Destroy:
	{
		const auto target = *action.target;
		action.status = Status::Contact;
		if (!ecs::IsAvailable(target))
		{
			break;
		}
		// Homes take a blow; a standing tree takes nothing (it is kicked over, not knocked down); a rock breaks; a mobile
		// object takes nothing, neither at the blow nor as the knock-down ends (only a living thing would)
		if (lookup.AllOf<Abode>(target))
		{
			StrikeHome(creature, target);
		}
		else if (lookup.AllOf<Tree>(target))
		{
			// A tree takes nothing from the blow: it stays standing
		}
		else if (ecs::object_physics::IsRock(target))
		{
			// A thing that answers it is a rock (a rock, the bonfire, a piece knocked off a building) breaks in two with
			// a smashing sound; the blow does nothing to any other static
			static_cast<void>(ecs::object_physics::SmashRock(target, creature));
		}
		break;
	}
	case Kind::PutDown:
	{
		// What it puts down, tosses away or throws, it counts as the last thing it let go of; a creature put down or
		// tossed away only from the start
		const auto letGo = GetHeld(creature);
		const bool remembered = letGo.has_value() && !lookup.AllOf<Creature>(*letGo);
		Release(creature, handPosition, glm::vec3(0.0f), false, true);
		if (remembered)
		{
			RememberLetGo(creature, *letGo);
		}
		break;
	}
	case Kind::Discard:
	case Kind::Lob:
	{
		const auto letGo = GetHeld(creature);
		const bool remembered = action.kind == Kind::Discard && letGo.has_value() && !lookup.AllOf<Creature>(*letGo);
		const auto animationIndex = action.animations.front();
		// The hand's speed is taken at size one: the toss makes it the creature's size after its share, as the game does.
		// It is the speed of where the right hand grasps, the middle of the ends of the bones under it
		const auto unscaled = [&](float timeMs) -> std::optional<glm::vec3> {
			if (!Locator::creatureAnimationSystem::has_value())
			{
				return std::nullopt;
			}
			return Locator::creatureAnimationSystem::value().GraspInAnimation(creature, animationIndex, timeMs,
			                                                                  points->rightHand);
		};
		const auto now = unscaled(action.eventMs);
		const auto before = unscaled(action.eventMs - creature_throw::k_HandSpeedSpanMs);
		const auto* walking = lookup.TryGet<const CreatureLocomotion>(creature);
		const float heading =
		    walking != nullptr && walking->started ? walking->heading : ecs::creature_pose::ReadHeading(transform.rotation);
		const auto velocity =
		    now.has_value() && before.has_value()
		        ? creature_throw::TossVelocity(creature_throw::HandVelocity(*now, *before, creature_throw::k_HandSpeedSpanMs),
		                                       action.mirrored, heading, transform.scale.x, creature_throw::k_DiscardSpeedShare)
		        : glm::vec3(0.0f);
		Release(creature, handPosition, velocity, false, action.kind == Kind::Discard);
		if (remembered)
		{
			RememberLetGo(creature, *letGo);
		}
		break;
	}
	case Kind::Throw:
	{
		const auto letGo = GetHeld(creature);
		Release(creature, handPosition,
		        creature_throw::ThrowVelocity(action.point, handPosition, std::max(action.flightSeconds, k_Tiny)), true, true);
		if (letGo.has_value())
		{
			RememberLetGo(creature, *letGo);
		}
		break;
	}
	case Kind::Eat:
	{
		const auto held = GetHeld(creature);
		if (!held.has_value())
		{
			break;
		}
		// The hand lets go of it as it is eaten: neither in the hand nor on the land, and not drawn, until the eating
		// ends and it is used up
		registry.RemoveState<CreatureHeldObject>(creature);
		if (lookup.AllOf<HeldByCreature>(*held))
		{
			registry.RemoveState<HeldByCreature>(*held);
		}
		registry.AssignOrReplace<NotDrawn>(*held);
		action.meal = *held;
		break;
	}
	case Kind::TakeFromHand:
	{
		// The giving called off while it reached, its hand closes on nothing and it stands there
		if (!action.target.has_value())
		{
			Fail(action, "the giving was called off");
			break;
		}
		// It takes the thing only if the player's hand still gives it; otherwise its hand closes on nothing
		const auto object = *action.target;
		if (!ecs::IsAvailable(object) || lookup.AllOf<HeldByCreature>(object) || !lookup.AllOf<Transform>(object) ||
		    !Locator::handGrabSystem::has_value() || !Locator::handGrabSystem::value().HandOver(object))
		{
			Fail(action, "the hand no longer gives it");
			break;
		}
		if (!TakeIntoHand(registry, creature, object, action.mirrored, handPosition, *points, animation, transform))
		{
			Fail(action, "it will not be held");
			break;
		}
		action.status = Status::Contact;
		break;
	}
	case Kind::Kick:
		if (action.target.has_value())
		{
			KickAway(creature, transform, *points, *action.target);
		}
		action.status = Status::Contact;
		break;
	case Kind::Keep:
	case Kind::Point:
	case Kind::Kiss:
		break;
	}
}

void CreatureObjectActionSystem::EndMeal(entt::entity creature, entt::entity meal) const
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	if (!lookup.Valid(meal))
	{
		return;
	}
	// A meal that may not be eaten is spat out whole before anything else: the creature is sick and holds back its
	// hunger, and the thing is let go of where it is, with no speed, thrown by the creature. Nothing is counted
	const auto* helpSystem = openblack::help::Get();
	if (creature_eating::RefusesMeal(lookup.AllOf<Indestructible>(meal), ecs::script_held::IsInScript(meal),
	                                 helpSystem != nullptr && helpSystem->IsScriptWideScreen()))
	{
		const auto type = static_cast<uint32_t>(ecs::map_cells::TypeOf(meal));
		const float value = MealValueOf(meal);
		if (Locator::creatureMindSystem::has_value())
		{
			Locator::creatureMindSystem::value().EndMeal(creature, type, value, creature_eating::MealEnd::Refused);
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (lookup.AllOf<NotDrawn>(meal))
		{
			registry.Remove<NotDrawn>(meal);
		}
		const auto* at = lookup.TryGet<const Transform>(meal);
		ReleaseHeld(meal, at != nullptr ? at->position : glm::vec3(0.0f), glm::vec3(0.0f), creature, true);
		return;
	}
	// What it was, read before it is used up
	const float value = MealValueOf(meal);
	const bool mushroom = IsMushroom(lookup, meal);
	const bool poisoned = IsPoisonedFood(lookup, meal);
	const bool pileFood = ecs::object::IsPileFood(meal);
	const auto type = static_cast<uint32_t>(ecs::map_cells::TypeOf(meal));
	// Only while it is still there: the creature's tallies (a person, an animal or a creature eaten counts as killed, a
	// mushroom eaten among its mushrooms), then it is used up, food or not
	auto& registry = Locator::entitiesRegistry::value();
	const bool available = ecs::IsAvailable(meal);
	ecs::player_statistics::CountCreatureMealEnd(registry, creature, available, type, mushroom);
	if (available)
	{
		Consume(lookup, meal, creature);
	}
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().EndMeal(creature, type, value,
		                                             creature_eating::JudgeMeal(value, mushroom, pileFood, poisoned));
		// A magic mushroom makes it high once it is done with what it is doing
		if (mushroom)
		{
			Locator::creatureMindSystem::value().AteMagicMushroom(creature);
		}
	}
}

void CreatureObjectActionSystem::UpdateDraw(float turnFraction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	const auto f = std::clamp(turnFraction, 0.0f, 1.0f);

	// The actions' animations, a share of the way through the turn on
	std::vector<entt::entity> acting;
	lookup.Each<const Creature, const CreatureAnimation, const CreatureObjectAction>(
	    [&acting](entt::entity entity, const auto&...) { acting.push_back(entity); });
	for (const auto entity : acting)
	{
		const auto& action = lookup.Get<CreatureObjectAction>(entity);
		auto& animation = registry.Get<CreatureAnimation>(entity);
		if (!IsDrawn(action))
		{
			continue;
		}
		animation.slots =
		    SlotsAt(action, ecs::creature_pose::DrawnSize(lookup, entity), DurationsOf(entity, action), f, animation.realSpeed);
	}
	std::vector<entt::entity> catching;
	lookup.Each<const Creature, const CreatureAnimation, const CreatureCatching>(
	    [&catching](entt::entity entity, const auto&...) { catching.push_back(entity); });
	for (const auto entity : catching)
	{
		const auto& catch_ = lookup.Get<CreatureCatching>(entity);
		if (catch_.stage != CreatureCatching::Stage::Over)
		{
			registry.Get<CreatureAnimation>(entity).slots = CatchSlotsAt(catch_, f);
		}
	}
}

void CreatureObjectActionSystem::AnimationAt(entt::entity creature, float turnFraction, CreatureAnimationInputs& inputs) const
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	if (const auto* catching = lookup.TryGet<const CreatureCatching>(creature);
	    catching != nullptr && catching->stage != CreatureCatching::Stage::Over)
	{
		inputs.slots = CatchSlotsAt(*catching, std::clamp(turnFraction, 0.0f, 1.0f));
		return;
	}
	const auto* action = lookup.TryGet<const CreatureObjectAction>(creature);
	if (action == nullptr || !lookup.AllOf<Creature, CreatureAnimation>(creature) || !IsDrawn(*action))
	{
		return;
	}
	inputs.slots = SlotsAt(*action, ecs::creature_pose::DrawnSize(lookup, creature), DurationsOf(creature, *action),
	                       std::clamp(turnFraction, 0.0f, 1.0f), lookup.Get<const CreatureAnimation>(creature).realSpeed);
}

bool CreatureObjectActionSystem::IsDrawn(const CreatureObjectAction& action)
{
	return action.phase == Phase::Playing && (action.status == Status::Running || action.status == Status::Contact);
}

bool CreatureObjectActionSystem::KeepsFromCatching(const CreatureObjectAction* action)
{
	return action != nullptr && (action->status == Status::Running || action->status == Status::Contact);
}

bool CreatureObjectActionSystem::IsCalledOffTake(const CreatureObjectAction& action)
{
	return action.kind == Kind::TakeFromHand && action.phase == Phase::Playing && action.status == Status::Running &&
	       !action.target.has_value();
}

std::vector<CreatureAnimation::Slot> CreatureObjectActionSystem::SlotsAt(const CreatureObjectAction& action, float drawnSize,
                                                                         const std::array<float, 4>& durationsMs, float share,
                                                                         bool realSpeed)
{
	std::vector<CreatureAnimation::Slot> slots;
	// A pointing arm that has risen to its point moment stays there between turns too
	const bool held = IsHoldingPoint(action) && !(action.timeMs < action.pointMs);
	const auto drawnMs =
	    held ? action.timeMs
	         : action.timeMs +
	               (share * TurnMs() *
	                creature_layers::BodyRate(drawnSize, realSpeed || creature_object_actions::PlaysAtRealSpeed(action.kind)));
	for (size_t i = 0; i < action.animationCount; ++i)
	{
		const auto duration = durationsMs.at(i);
		// Pointing goes round its animations (up to its point moment); everything else holds its last frame
		const auto time = action.holdMs > 0.0f && duration > 0.0f ? std::fmod(drawnMs, duration)
		                                                          : std::clamp(drawnMs, 0.0f, std::max(duration - 1.0f, 0.0f));
		slots.push_back({.animation = action.animations.at(i),
		                 .timeMs = time,
		                 .weight = action.weights.at(i),
		                 .mirrored = action.mirrored});
	}
	return slots;
}

void CreatureObjectActionSystem::UpdateHeldDraw()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);

	// What is held is drawn in the hand as the body is drawn this frame
	bool moved = false;
	std::vector<entt::entity> holding;
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureHeldObject>(
	    [&holding](entt::entity entity, const auto&...) { holding.push_back(entity); });
	for (const auto entity : holding)
	{
		const auto& held = lookup.Get<CreatureHeldObject>(entity);
		const auto& animation = lookup.Get<CreatureAnimation>(entity);
		if (!ecs::IsAvailable(held.object) || !lookup.AllOf<Transform>(held.object) || animation.boneMatrices.empty())
		{
			continue;
		}
		// in the hand as the body is drawn this frame (its own drawn matrix and the bones it is drawn with: between its
		// turns); the object's Transform stays where the turn put it. A thing is turned by the hand's bone and hung below
		// its grasp point; a creature it holds is still turned with the body and held by its middle
		const auto& bones = creature_turn_pose::BonesDrawn(animation);
		const auto placement = ecs::DrawnBodyModel(lookup, entity);
		glm::mat3 rotation {1.0f};
		glm::vec3 position {0.0f};
		if (lookup.AllOf<Creature>(held.object))
		{
			rotation = ecs::creature_pose::DrawnPlacementOf(lookup, entity).rotation * held.rotation;
			position = PalmOf(held.bone, animation, bones, placement) - (rotation * held.middle);
		}
		else
		{
			const auto placed = HeldPlacementOf(lookup, held, animation, bones, placement);
			rotation = placed.turn;
			position = placed.position;
		}
		auto& pose = lookup.AllOf<PhysicsDrawPose>(held.object) ? registry.Get<PhysicsDrawPose>(held.object)
		                                                        : registry.Assign<PhysicsDrawPose>(held.object);
		if (pose.position != position || pose.rotation != rotation)
		{
			pose.position = position;
			pose.rotation = rotation;
			moved = true;
		}
	}
	if (moved)
	{
		registry.SetDirty();
	}
}

namespace
{
using Catching = CreatureCatching;

/// The physics entry of the thing a creature catches, while the thing is in the physics
ecs::PhysicsEntry* FlightOf(entt::entity object)
{
	if (!Locator::dynamicsSystem::has_value() || object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		return nullptr;
	}
	return Locator::dynamicsSystem::value().Find(object);
}

/// Where the creature stands and the way it faces as the game holds it: minus its walk's heading, or that of the way its
/// body faces before it has walked
creature_catch::Stance StanceOf(const ecs::Registry& registry, entt::entity creature, const Transform& transform)
{
	const auto* locomotion = registry.TryGet<const CreatureLocomotion>(creature);
	if (locomotion != nullptr && locomotion->started)
	{
		return {.position = transform.position, .heading = -locomotion->heading};
	}
	const auto ahead = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
	return {.position = transform.position, .heading = -creature_locomotion::HeadingOf({ahead.x, ahead.z})};
}

/// How long one of the creature's animations lasts, in whole milliseconds
int32_t WholeDurationOf(entt::entity creature, size_t animation)
{
	return static_cast<int32_t>(DurationOf(creature, animation));
}

/// The catch is over: the body stands again
void EndCatch(Catching& catching, CreatureAnimation& animation)
{
	catching.stage = Catching::Stage::Over;
	catching.stepped = false;
	animation.slots.clear();
}

/// A turn of the step across: the creature is carried by the turn's share of the step's travel, sized and turned as it
/// is, and put at the nearest place it may stand when the step took it too near ground it may not; the step plays on
void StepTurn(const ecs::Registry& registry, entt::entity creature, Catching& catching, const Transform& transform)
{
	const auto travel = Locator::creatureAnimationSystem::value().AnimationTravel(creature, creature_catch::k_CatchStep);
	if (travel.has_value() && catching.durationMs > 0 && Locator::creatureLocomotionSystem::has_value())
	{
		auto position =
		    transform.position + creature_catch::StepMove(*travel, transform.rotation,
		                                                  ecs::creature_pose::DrawnScale(registry, creature).x,
		                                                  static_cast<float>(catching.stepMs),
		                                                  static_cast<float>(catching.durationMs), catching.mirrored);
		if (!land_avoid::IsPosValid(position, creature_catch::k_StepClearance))
		{
			if (const auto valid = land_avoid::NearestValid(glm::xz(position), creature_catch::k_StepSearchClearance,
			                                                creature_catch::k_StepSearchDistance))
			{
				position.x = valid->x;
				position.z = valid->y;
			}
		}
		Locator::creatureLocomotionSystem::value().Place(creature, position);
	}
	catching.timeMs = creature_catch::AdvanceCyclic(catching.timeMs, catching.stepMs, catching.durationMs);
}

/// Where the side step would end: the step's whole travel, its sideways part alone turned round for the left hand,
/// through the body's placement leaned onto the land at the size it is drawn at. None without the step
std::optional<glm::vec3> StepEnd(const ecs::Registry& registry, entt::entity creature, const Transform& transform,
                                 bool mirrored)
{
	auto travel = Locator::creatureAnimationSystem::value().AnimationTravel(creature, creature_catch::k_CatchStep);
	if (!travel.has_value())
	{
		return std::nullopt;
	}
	if (mirrored)
	{
		travel->x = -travel->x;
	}
	const auto placement = ecs::creature_pose::LeanedModel(registry, creature, transform.position, transform.rotation,
	                                                       ecs::creature_pose::DrawnScale(registry, creature));
	return glm::vec3(placement * glm::vec4(*travel, 1.0f));
}

/// The caught thing is taken out of the physics into the hand it was caught with
void TakeCaught(ecs::Registry& registry, entt::entity creature, entt::entity object, const Catching& catching,
                const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, const Transform& transform)
{
	const auto& lookup = std::as_const(registry);
	const auto bone = HandBone(points, animation, catching.mirrored);
	const auto middle = MiddleOf(registry, object);
	const auto rotation = glm::transpose(transform.rotation) * lookup.Get<Transform>(object).rotation;
	// out of the physics without its end: its flying-object reactions end when its entry goes
	ecs::physics::from_hand::LeavePhysicsWithoutItsEnd(object);
	HoldCaught(registry, object);
	if (ecs::map_cells::IsObjectInMap(object))
	{
		ecs::map_cells::RemoveMapObject(object);
	}
	if (lookup.AllOf<MorphWithTerrain>(object))
	{
		registry.Remove<MorphWithTerrain>(object);
	}
	registry.AssignOrReplaceState<CreatureHeldObject>(creature,
	                                                  CreatureHeldObject {.object = object,
	                                                                      .mirrored = catching.mirrored,
	                                                                      .bone = bone,
	                                                                      .rotation = rotation,
	                                                                      .middle = middle,
	                                                                      .shadowTaken = TakeShadow(registry, object)});
	registry.AssignOrReplaceState<HeldByCreature>(object, HeldByCreature {.creature = creature});
	// Into the palm, drawn there at once: a thing turned by the hand's bone and hung below the palm, a creature turned
	// with the body and held by its middle
	auto& at = registry.Get<Transform>(object);
	if (lookup.AllOf<Creature>(object))
	{
		at.rotation = transform.rotation * rotation;
		if (!animation.boneMatrices.empty())
		{
			at.position = PalmOf(bone, animation, PlacementOf(transform)) - (at.rotation * middle);
		}
	}
	else if (!animation.boneMatrices.empty())
	{
		const auto placed = HeldPlacementOf(lookup, lookup.Get<CreatureHeldObject>(creature), animation, animation.boneMatrices,
		                                    PlacementOf(transform));
		at.rotation = placed.turn;
		at.position = placed.position;
	}
	else
	{
		at.rotation = creature_hold::HeldTurn(glm::mat3(PlacementOf(transform)), catching.mirrored);
	}
	ecs::NotifyTeleported(object);
}

/// A turn of the reach: the four catching animations blended by where the thing is against where the hand closes in
/// each, the catch played on, or back once the thing is out of the physics; at its moment the hand closes on the thing,
/// unless the blend had to lean too far. Whether the hand closed in the turn
bool ReachTurn(ecs::Registry& registry, entt::entity creature, Catching& catching, const Transform& transform,
               const CreatureRig::ActionPoints& points, const CreatureAnimation& animation, const CatchHands& hands)
{
	const auto& lookup = std::as_const(registry);
	const auto* flight = FlightOf(catching.object);
	creature_catch::Blend blend {};
	if (flight != nullptr)
	{
		blend = creature_catch::Weigh(flight->body.Centre(), StanceOf(lookup, creature, transform), hands.hands,
		                              ecs::creature_pose::DrawnScale(lookup, creature).x, catching.mirrored);
		catching.height = blend.height;
	}
	else
	{
		blend = creature_catch::WeighWithout(catching.height);
		catching.hand = Catching::Hand::Missed;
	}
	catching.side = blend.side;
	catching.weights = blend.weights;
	catching.timeMs = creature_catch::CatchClock(catching.hand == Catching::Hand::Missed, catching.timeMs, catching.stepMs,
	                                             catching.durationMs);
	if (catching.hand != Catching::Hand::Reaching || static_cast<float>(catching.timeMs) < points.catchMs)
	{
		return false;
	}
	// The hand closes, on the thing or on nothing: what it looked at is let go
	catching.lookPoint.reset();
	if (blend.clamped || !ecs::IsAvailable(catching.object) || lookup.AllOf<HeldByCreature>(catching.object) ||
	    !lookup.AllOf<Transform>(catching.object))
	{
		catching.hand = Catching::Hand::Missed;
		return true;
	}
	catching.hand = Catching::Hand::Caught;
	TakeCaught(registry, creature, catching.object, catching, points, animation, transform);
	return true;
}

/// Where the creature looks after a turn of its catch, over what its mind chose: while it turns, waits or steps, at the
/// thing's place on the map; while it reaches, the head back ahead and the eyes on the point kept. In the turn the catch
/// ends, standing again, the head and the eyes are on the point kept, if any; from the next the mind's look is left
void LookTurn(ecs::Registry& registry, entt::entity creature, Catching& catching, CreatureAnimation& animation, bool handClosed)
{
	// While the player's hand is on the creature its look is the hand's: the catch sets no point, turns the head back
	// ahead as it reaches, and its hand's closing lets go of the point the player's hand set, until that hand's next frame
	if (Locator::creatureHandSystem::has_value() && Locator::creatureHandSystem::value().GetCreature() == creature)
	{
		auto* eyes = registry.TryGet<CreatureEyes>(creature);
		const auto look =
		    creature_catch::LookUnderTheHand(catching.stage == Catching::Stage::Reaching, handClosed,
		                                     {.head = animation.lookAt, .eyes = eyes != nullptr ? eyes->lookAt : std::nullopt});
		animation.lookAt = look.head;
		if (eyes != nullptr)
		{
			eyes->lookAt = look.eyes;
		}
		return;
	}
	const auto& lookup = std::as_const(registry);
	const bool over = catching.stage == Catching::Stage::Over;
	const bool reaching = catching.stage == Catching::Stage::Reaching;
	if (!over && !reaching && lookup.Valid(catching.object))
	{
		if (const auto* at = lookup.TryGet<const Transform>(catching.object); at != nullptr)
		{
			catching.lookPoint = map_coords::ToWorld(map_coords::FromWorld(at->position));
		}
	}
	const auto look = creature_catch::LookWhileCatching(reaching, catching.lookPoint);
	animation.lookAt = look.head;
	if (auto* eyes = registry.TryGet<CreatureEyes>(creature))
	{
		eyes->lookAt = look.eyes;
	}
	if (over)
	{
		catching.lookPoint.reset();
	}
}

/// A turn of waiting for the thing, once turned to face it: it steps across when the thing will pass out of reach,
/// reaches for it when it will arrive as the hand closes, and gives up when the thing is behind it, out of the physics
/// or too late. Whether the hand closed in the turn
bool ReadyTurn(ecs::Registry& registry, entt::entity creature, Catching& catching, const Transform& transform,
               const CreatureRig::ActionPoints& points, CreatureAnimation& animation, const CatchHands& hands)
{
	auto& locomotion = Locator::creatureLocomotionSystem::value();
	if (!catching.turned)
	{
		if (locomotion.IsMoving(creature))
		{
			return false;
		}
		catching.turned = true;
		catching.timeMs = 0;
	}
	const auto* flight = FlightOf(catching.object);
	if (flight == nullptr)
	{
		EndCatch(catching, animation);
		return false;
	}
	// Turned to it, it looks where the thing flies
	catching.lookPoint = flight->body.Centre();
	const auto& lookup = std::as_const(registry);
	const auto lead = creature_catch::LeadSeconds(points.catchMs, ecs::creature_pose::DrawnSize(lookup, creature));
	const auto stance = StanceOf(lookup, creature, transform);
	catching.heading = stance.heading;
	catching.ahead = creature_catch::AheadOf(flight->body.Centre(), stance);
	catching.right = creature_catch::RightOf(flight->body.Centre(), stance);
	const auto ready = creature_catch::ReadyToCatch(flight->body.Centre(), flight->body.velocity, stance, hands.hands,
	                                                ecs::creature_pose::DrawnScale(lookup, creature).x, lead);
	switch (ready.readiness)
	{
	case creature_catch::Readiness::Wait:
		break;
	case creature_catch::Readiness::Step:
	{
		// Not while it moves, nor without the step, nor when the step would end inside a circle its routes keep clear of
		const auto end = StepEnd(lookup, creature, transform, ready.mirrored);
		catching.stepEnd = end;
		catching.stepClear = !locomotion.IsMoving(creature) && end.has_value() &&
		                     ecs::route_plan_world::IsStepEndClear(creature, {.x = end->x, .z = end->z}, end->y);
		if (!catching.stepClear)
		{
			EndCatch(catching, animation);
			break;
		}
		catching.stage = Catching::Stage::Stepping;
		catching.stepped = true;
		catching.mirrored = ready.mirrored;
		catching.timeMs = 0;
		catching.durationMs = WholeDurationOf(creature, creature_catch::k_CatchStep);
		StepTurn(lookup, creature, catching, transform);
		break;
	}
	case creature_catch::Readiness::Catch:
		catching.stage = Catching::Stage::Reaching;
		catching.hand = Catching::Hand::Reaching;
		catching.mirrored = ready.mirrored;
		catching.timeMs = 0;
		catching.durationMs = WholeDurationOf(creature, creature_catch::k_CatchAnimations.front());
		return ReachTurn(registry, creature, catching, transform, points, animation, hands);
	case creature_catch::Readiness::GiveUp:
		EndCatch(catching, animation);
		break;
	}
	return false;
}
} // namespace

bool CreatureObjectActionSystem::Catch(entt::entity creature, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!registry.Valid(creature) || !lookup.AllOf<Creature, CreatureAnimation, Transform>(creature) ||
	    !Locator::creatureAnimationSystem::has_value() || !Locator::creatureLocomotionSystem::has_value())
	{
		return false;
	}
	auto& animations = Locator::creatureAnimationSystem::value();
	const bool hasAnimations = std::ranges::all_of(creature_catch::k_CatchAnimations, [&](size_t clip) {
		return animations.AnimationDuration(creature, clip).has_value();
	});
	const auto* points = PointsOf(lookup.Get<Creature>(creature));
	// Not without the four catching animations, nor while its body acts something else out. An action only asked for,
	// waiting for the hands, is no act of the body: the catch takes its place
	if (!hasAnimations || points == nullptr || IsCatching(creature) ||
	    KeepsFromCatching(lookup.TryGet<const CreatureObjectAction>(creature)))
	{
		return false;
	}
	// The thing must be flying across and up or down fast enough
	const auto* flight = FlightOf(object);
	if (flight == nullptr || !creature_catch::FastEnoughToStart(flight->body.velocity))
	{
		return false;
	}
	// Where the hand closes in each catching animation, measured at its first catch and kept
	if (!lookup.AllOf<CatchHands>(creature))
	{
		CatchHands measured;
		for (size_t i = 0; i < creature_catch::k_CatchAnimations.size(); ++i)
		{
			const auto grasp = animations.GraspInAnimation(creature, creature_catch::k_CatchAnimations.at(i), points->catchMs,
			                                               points->rightHand);
			if (!grasp.has_value())
			{
				return false;
			}
			measured.hands.at(i) = *grasp;
		}
		registry.Assign<CatchHands>(creature, measured);
	}
	if (lookup.AllOf<CreatureObjectAction>(creature))
	{
		registry.RemoveState<CreatureObjectAction>(creature);
	}
	if (lookup.AllOf<CreatureObjectActionWaiting>(creature))
	{
		registry.RemoveState<CreatureObjectActionWaiting>(creature);
	}
	// It faces back along the thing's flight, turning first when that is too far round
	const auto& transform = lookup.Get<Transform>(creature);
	const auto& velocity = flight->body.velocity;
	const auto back = transform.position - glm::vec3(velocity.x, 0.0f, velocity.z);
	const bool turns = BearingTo(transform, back) > creature_catch::k_TurnAngle;
	if (turns)
	{
		Locator::creatureLocomotionSystem::value().TurnToFace(creature, glm::xz(back));
	}
	registry.AssignOrReplaceState<CreatureCatching>(creature, CreatureCatching {.object = object, .turned = !turns});
	return true;
}

bool CreatureObjectActionSystem::IsCatching(entt::entity creature) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* catching = registry.Valid(creature) ? registry.TryGet<const CreatureCatching>(creature) : nullptr;
	return catching != nullptr && catching->stage != CreatureCatching::Stage::Over;
}

void CreatureObjectActionSystem::ProcessCatches()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> catchers;
	lookup.Each<const Creature, const Transform, const CreatureAnimation, const CreatureCatching>(
	    [&catchers](entt::entity entity, const auto&...) { catchers.push_back(entity); });
	for (const auto entity : catchers)
	{
		auto& catching = registry.Get<CreatureCatching>(entity);
		if (catching.stage == Catching::Stage::Over)
		{
			continue;
		}
		// Started in the turn's mind, the body takes it up from the next turn
		if (catching.fresh)
		{
			catching.fresh = false;
			continue;
		}
		auto& animation = registry.Get<CreatureAnimation>(entity);
		const auto* points = PointsOf(lookup.Get<Creature>(entity));
		const auto* hands = lookup.TryGet<const CatchHands>(entity);
		if (points == nullptr || hands == nullptr)
		{
			EndCatch(catching, animation);
			continue;
		}
		const auto& transform = lookup.Get<Transform>(entity);
		// The turn's step of its animations, as the game takes it for the size it is drawn at
		catching.stepMs = creature_turn_pose::Step(static_cast<int32_t>(game_clock::MsPerTurn()),
		                                           ecs::creature_pose::DrawnSize(lookup, entity), false, false);
		// An animation the turn's step would take past its end ends first: the step back to waiting, the catch over
		if (catching.stage == Catching::Stage::Stepping &&
		    creature_catch::StepEnds(catching.timeMs, catching.stepMs, catching.durationMs))
		{
			catching.stage = Catching::Stage::Ready;
			catching.timeMs = 0;
		}
		if (catching.stage == Catching::Stage::Reaching &&
		    creature_catch::CatchEnds(catching.hand == Catching::Hand::Missed, catching.timeMs, catching.stepMs,
		                              catching.durationMs))
		{
			EndCatch(catching, animation);
			LookTurn(registry, entity, catching, animation, false);
			continue;
		}
		bool handClosed = false;
		switch (catching.stage)
		{
		case Catching::Stage::Ready:
			handClosed = ReadyTurn(registry, entity, catching, transform, *points, animation, *hands);
			break;
		case Catching::Stage::Stepping:
			StepTurn(lookup, entity, catching, transform);
			break;
		case Catching::Stage::Reaching:
			handClosed = ReachTurn(registry, entity, catching, transform, *points, animation, *hands);
			break;
		case Catching::Stage::Over:
			break;
		}
		LookTurn(registry, entity, catching, animation, handClosed);
	}
}

std::vector<CreatureAnimation::Slot> CreatureObjectActionSystem::CatchSlotsAt(const CreatureCatching& catching, float share)
{
	std::vector<CreatureAnimation::Slot> slots;
	const bool back = catching.stage == Catching::Stage::Reaching && catching.hand == Catching::Hand::Missed;
	const auto ahead = share * static_cast<float>(catching.stepMs);
	const auto time = std::clamp(static_cast<float>(catching.timeMs) + (back ? -ahead : ahead), 0.0f,
	                             static_cast<float>(std::max(catching.durationMs - 1, 0)));
	switch (catching.stage)
	{
	case Catching::Stage::Stepping:
		slots.push_back(
		    {.animation = creature_catch::k_CatchStep, .timeMs = time, .weight = 1.0f, .mirrored = catching.mirrored});
		break;
	case Catching::Stage::Reaching:
		for (size_t i = 0; i < creature_catch::k_CatchAnimations.size(); ++i)
		{
			slots.push_back({.animation = creature_catch::k_CatchAnimations.at(i),
			                 .timeMs = time,
			                 .weight = catching.weights.at(i),
			                 .mirrored = catching.mirrored});
		}
		break;
	case Catching::Stage::Ready:
	case Catching::Stage::Over:
		break;
	}
	return slots;
}
